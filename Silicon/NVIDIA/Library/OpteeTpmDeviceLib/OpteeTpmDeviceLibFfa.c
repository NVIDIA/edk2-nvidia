/** @file
  EDK2 API for OpteeTpmInterfaceFfa

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

* */

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/TimerLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/ArmStdSmc.h>
#include "OpteeTpmDeviceLib.h"
#include "OpteeTpmDeviceLibFfa.h"

STATIC UINT16  OpteeVmId = 0xFFFF;

// Memory share handle tracking
STATIC PHYSICAL_ADDRESS  mRxBuffer;
STATIC PHYSICAL_ADDRESS  mTxBuffer;
STATIC PHYSICAL_ADDRESS  SharedMemAddr;
STATIC UINT64            mSharedMemoryHandle = 0;
STATIC VOID              *OpteeFtpmShmIpBuf  = NULL;
STATIC VOID              *OpteeFtpmShmOpBuf  = NULL;
STATIC UINTN             OpteeFtpmShmSize;

// Session ID
STATIC UINT32  SessionId;

/*
 * GetOpteeVmId
 * Get the Optee VM ID from the SPMC.
 */
STATIC
EFI_STATUS
GetOpteeVmId (
  VOID
  )
{
  ARM_SMC_ARGS  SmcArgs;
  EFI_STATUS    Status;
  EFI_GUID      OpteeGuid;
  UINT64        OpteeGuidData123;
  UINT64        OpteeGuidData23;
  UINT64        OpteeGuidData4Low32;
  UINT64        OpteeGuidData4Hi32;
  UINT64        OpteeGuidData4;

  // Setup UUID in accordance with the FF-A ABIs.
  CopyMem (&OpteeGuid, &gNVIDIAOpteeGuid, sizeof (EFI_GUID));
  OpteeGuidData23     = OpteeGuid.Data2 << 16 | OpteeGuid.Data3;
  OpteeGuidData123    = OpteeGuid.Data1 | OpteeGuidData23 << 32;
  OpteeGuidData4Hi32  = SwapBytes32 (*(UINT32 *)OpteeGuid.Data4);
  OpteeGuidData4Low32 = SwapBytes32 (*(UINT32 *)(OpteeGuid.Data4 + 4));
  OpteeGuidData4      = OpteeGuidData4Hi32 | OpteeGuidData4Low32 << 32;

  ZeroMem (&SmcArgs, sizeof (SmcArgs));

  SmcArgs.Arg0 = ARM_SMC_ID_FFA_PARTITION_INFO_GET_REG_64;
  SmcArgs.Arg1 = OpteeGuidData123;
  SmcArgs.Arg2 = OpteeGuidData4;
  SmcArgs.Arg3 = 0;

  CallFfaSmc (&SmcArgs);

  if ((SmcArgs.Arg0 != FFA_SUCCESS_AARCH64) && (SmcArgs.Arg0 != FFA_SUCCESS_AARCH32)) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "ARM_SMC_ID_FFA_PARTITION_INFO_GET_32 failed Arg0 0x%lx\n", SmcArgs.Arg0));
    goto ExitGetOpteeVmId;
  }

  OpteeVmId = (UINT16)(SmcArgs.Arg3 & 0xFFFF);
  Status    = EFI_SUCCESS;

ExitGetOpteeVmId:
  return Status;
}

/**
  Query the SPM version, check compatibility and return success if compatible.

  @retval EFI_SUCCESS       SPM versions compatible.
  @retval EFI_UNSUPPORTED   SPM versions not compatible.
**/
STATIC
EFI_STATUS
GetSpmVersion (
  VOID
  )
{
  EFI_STATUS    Status;
  UINT16        CalleeSpmMajorVer;
  UINT16        CallerSpmMajorVer;
  UINT16        CalleeSpmMinorVer;
  UINT16        CallerSpmMinorVer;
  UINT32        SpmVersion;
  ARM_SMC_ARGS  SpmVersionArgs;

  SpmVersionArgs.Arg0  = ARM_SMC_ID_FFA_VERSION_32;
  SpmVersionArgs.Arg1  = SPM_MAJOR_VERSION << SPM_MAJOR_VER_SHIFT;
  SpmVersionArgs.Arg1 |= (FeaturePcdGet (PcdFfaMinorV2Supported) ? 2 : 1);
  CallerSpmMajorVer    = SPM_MAJOR_VERSION;
  CallerSpmMinorVer    = (FeaturePcdGet (PcdFfaMinorV2Supported) ? 2 : 1);

  CallFfaSmc (&SpmVersionArgs);

  SpmVersion = SpmVersionArgs.Arg0;
  if (SpmVersion == FFA_NOT_SUPPORTED) {
    return EFI_UNSUPPORTED;
  }

  CalleeSpmMajorVer = ((SpmVersion & SPM_MAJOR_VER_MASK) >> SPM_MAJOR_VER_SHIFT);
  CalleeSpmMinorVer = ((SpmVersion & SPM_MINOR_VER_MASK) >> 0);

  // Different major revision values indicate possibly incompatible functions.
  // For two revisions, A and B, for which the major revision values are
  // identical, if the minor revision value of revision B is greater than
  // the minor revision value of revision A, then every function in
  // revision A must work in a compatible way with revision B.
  // However, it is possible for revision B to have a higher
  // function count than revision A.
  if ((CalleeSpmMajorVer == CallerSpmMajorVer) &&
      (CalleeSpmMinorVer >= CallerSpmMinorVer))
  {
    DEBUG ((
      DEBUG_INFO,
      "SPM Version: Major=0x%x, Minor=0x%x\n",
      CalleeSpmMajorVer,
      CalleeSpmMinorVer
      ));
    Status = EFI_SUCCESS;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "Incompatible SPM Versions.\n Callee Version: Major=0x%x, Minor=0x%x.\n Caller: Major=0x%x, Minor>=0x%x.\n",
      CalleeSpmMajorVer,
      CalleeSpmMinorVer,
      CallerSpmMajorVer,
      CallerSpmMinorVer
      ));
    Status = EFI_UNSUPPORTED;
  }

  return Status;
}

/**
  Send FFA direct message request and wait for response with retry mechanism

  This function sends an FFA (Firmware Framework for Armv8-A) direct message
  request to a secure partition and waits for the corresponding response.
  It implements a robust retry mechanism with exponential backoff to handle
  temporary communication failures.

  @param[in,out] Args    Pointer to ARM SMC arguments structure containing
                         the FFA direct message request parameters.
                         On return, contains the response from the target partition.

  @retval EFI_SUCCESS      FFA direct message completed successfully
  @retval EFI_DEVICE_ERROR All retry attempts failed or invalid response received
**/
STATIC
EFI_STATUS
SendFfaDirectReqAndResp (
  IN ARM_SMC_ARGS  *Args
  )
{
  EFI_STATUS    Status;
  UINTN         Idx;
  UINT8         MaxRetries      = MAX_RETRIES;       // Maximum number of retry attempts
  UINT64        BackOffTimeUsec = BACKOFF_TIME_USEC; // Initial backoff delay in microseconds
  ARM_SMC_ARGS  LocalArgs       = { 0 };

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Sending FFA direct request, Function ID: 0x%x\n",
    __FUNCTION__,
    Args->Arg0
    ));

  // Make a local copy of arguments to avoid modifying the original during retries
  CopyMem ((VOID *)&LocalArgs, (VOID *)Args, sizeof (ARM_SMC_ARGS));

  // Send the FFA direct message request via SMC call
  CallFfaSmc (&LocalArgs);

  // Check if we received the expected FFA direct response
  if (LocalArgs.Arg0 != ARM_SMC_ID_FFA_MSG_SEND_DIRECT_RESP_64) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid Response Arg0:0x%x, Arg1:0x%x, Arg2:0x%x, Arg3:0x%x\n",
      __FUNCTION__,
      LocalArgs.Arg0,
      LocalArgs.Arg1,
      LocalArgs.Arg2,
      LocalArgs.Arg3
      ));

    // Retry mechanism: attempt up to MaxRetries times with backoff delay
    for (Idx = 0; Idx < MaxRetries; Idx++) {
      // Wait before retrying (backoff delay to avoid overwhelming the target)
      MicroSecondDelay (BackOffTimeUsec);
      ZeroMem (&LocalArgs, sizeof (ARM_SMC_ARGS));
      CopyMem ((VOID *)&LocalArgs, (VOID *)Args, sizeof (ARM_SMC_ARGS));

      // Retry the FFA direct message request
      CallFfaSmc (&LocalArgs);

      if (LocalArgs.Arg0 != ARM_SMC_ID_FFA_MSG_SEND_DIRECT_RESP_64) {
        DEBUG ((
          DEBUG_ERROR,
          "%a:%d Invalid Response Arg0:0x%x, Arg1:0x%x, Arg2:0x%x, Arg3:0x%x\n",
          __FUNCTION__,
          __LINE__,
          LocalArgs.Arg0,
          LocalArgs.Arg1,
          LocalArgs.Arg2,
          LocalArgs.Arg3
          ));
        continue;
      } else {
        break;
      }
    }

    // Check if all retries failed
    if (Idx >= MaxRetries) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: All retries failed, last response: 0x%x\n",
        __FUNCTION__,
        LocalArgs.Arg0
        ));
      return EFI_DEVICE_ERROR;
    }
  }

  Status = EFI_SUCCESS;

  // Copy the successful response back to the caller's argument structure
  CopyMem ((VOID *)Args, (VOID *)&LocalArgs, sizeof (ARM_SMC_ARGS));

  DEBUG ((DEBUG_VERBOSE, "%a: FFA direct request completed successfully\n", __FUNCTION__));

  return Status;
}

STATIC
VOID
EfiGuidToRfc4122Uuid (
  OUT RFC4122_UUID  *Rfc4122Uuid,
  IN EFI_GUID       *Guid
  )
{
  Rfc4122Uuid->Data1 = SwapBytes32 (Guid->Data1);
  Rfc4122Uuid->Data2 = SwapBytes16 (Guid->Data2);
  Rfc4122Uuid->Data3 = SwapBytes16 (Guid->Data3);
  CopyMem (Rfc4122Uuid->Data4, Guid->Data4, sizeof (Rfc4122Uuid->Data4));
}

/**
  FFA implementation
  Open session with fTPM TA using direct message send

  @param[out] Session     Pointer to store the opened session ID

  @retval EFI_SUCCESS     The operation completed successfully.
  @retval EFI_DEVICE_ERROR Failed to communicate with OP-TEE
  @retval EFI_NOT_FOUND   TA not found
  @retval EFI_OUT_OF_RESOURCES Out of memory in OP-TEE

**/
STATIC
EFI_STATUS
EFIAPI
OpteeTpmOpenSession (
  OUT UINT32  *Session
  )
{
  EFI_STATUS         Status;
  ARM_SMC_ARGS       CommunicateSmcArgs;
  OPTEE_MESSAGE_ARG  *MessageArg;

  if (Session == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Get MessageArg from shared memory
  MessageArg = (OPTEE_MESSAGE_ARG *)SharedMemAddr;

  // Zero out the message structure
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));

  // Set up OPTEE message for opening session
  MessageArg->Command   = OPTEE_MESSAGE_COMMAND_OPEN_SESSION;
  MessageArg->NumParams = 2;  // UUID parameter + client login

  // Parameter 0: UUID of fTPM TA
  MessageArg->Params[0].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT | OPTEE_MESSAGE_ATTRIBUTE_META;
  EfiGuidToRfc4122Uuid ((RFC4122_UUID *)&MessageArg->Params[0].Union.Value, &gNVIDIAFtpmOpteeGuid);

  // Parameter 1: Client login information (set to public/non-secure)
  MessageArg->Params[1].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT | OPTEE_MESSAGE_ATTRIBUTE_META;
  ZeroMem (&MessageArg->Params[1].Union.Value, sizeof (EFI_GUID));
  MessageArg->Params[1].Union.Value.C = OPTEE_LOGIN_PUBLIC;

  // Prepare FFA direct message request
  ZeroMem (&CommunicateSmcArgs, sizeof (ARM_SMC_ARGS));
  CommunicateSmcArgs.Arg0 = ARM_SMC_ID_FFA_MSG_SEND_DIRECT_REQ_64;
  CommunicateSmcArgs.Arg1 = OpteeVmId;
  CommunicateSmcArgs.Arg2 = 0;  // Reserved

  // Implementation defined parameters for OP-TEE FFA
  CommunicateSmcArgs.Arg3 = OPTEE_FFA_YIELDING_CALL_WITH_ARG;
  CommunicateSmcArgs.Arg4 = (UINTN)LOWER_32_BITS (mSharedMemoryHandle);
  CommunicateSmcArgs.Arg5 = (UINTN)UPPER_32_BITS (mSharedMemoryHandle);
  CommunicateSmcArgs.Arg6 = 0;  // Offset in shared memory (0 for MessageArg)
  CommunicateSmcArgs.Arg7 = 0;  // Reserved

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Opening session with fTPM TA via FFA direct message\n",
    __FUNCTION__
    ));

  // Send the request
  Status = SendFfaDirectReqAndResp (&CommunicateSmcArgs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send FFA direct request: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Session open response - Return: 0x%x, Session: 0x%x\n",
    __FUNCTION__,
    MessageArg->Return,
    MessageArg->Session
    ));

  // Convert OP-TEE return code to EFI status
  switch (MessageArg->Return) {
    case OPTEE_SUCCESS:
      *Session = MessageArg->Session;
      Status   = EFI_SUCCESS;
      break;
    case OPTEE_ERROR_ITEM_NOT_FOUND:
      Status = EFI_NOT_FOUND;
      break;
    case OPTEE_ERROR_OUT_OF_MEMORY:
      Status = EFI_OUT_OF_RESOURCES;
      break;
    default:
      Status = EFI_DEVICE_ERROR;
      break;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to open session - OP-TEE return: 0x%x, EFI status: %r\n",
      __FUNCTION__,
      MessageArg->Return,
      Status
      ));
  } else {
    DEBUG ((
      DEBUG_INFO,
      "%a: Successfully opened session 0x%x with fTPM TA\n",
      __FUNCTION__,
      *Session
      ));
  }

  return Status;
}

/**
  FFA implementation
  Close session with fTPM TA using direct message send

  @param[in] Session      Session ID to close

  @retval EFI_SUCCESS     The operation completed successfully.
  @retval EFI_DEVICE_ERROR Failed to communicate with OP-TEE
  @retval EFI_INVALID_PARAMETER Invalid session ID

**/
STATIC
EFI_STATUS
EFIAPI
OpteeTpmCloseSession (
  IN UINT32  Session
  )
{
  EFI_STATUS         Status;
  ARM_SMC_ARGS       CommunicateSmcArgs;
  OPTEE_MESSAGE_ARG  *MessageArg;

  if (Session == 0) {
    DEBUG ((DEBUG_WARN, "%a: Invalid session ID (0)\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  // Get MessageArg from shared memory
  MessageArg = (OPTEE_MESSAGE_ARG *)SharedMemAddr;

  // Zero out the message structure
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));

  // Set up OPTEE message for closing session
  MessageArg->Command   = OPTEE_MESSAGE_COMMAND_CLOSE_SESSION;
  MessageArg->Session   = Session;
  MessageArg->NumParams = 0;  // No parameters needed for close session

  // Prepare FFA direct message request
  ZeroMem (&CommunicateSmcArgs, sizeof (ARM_SMC_ARGS));
  CommunicateSmcArgs.Arg0 = ARM_SMC_ID_FFA_MSG_SEND_DIRECT_REQ_64;
  CommunicateSmcArgs.Arg1 = OpteeVmId;
  CommunicateSmcArgs.Arg2 = 0;  // Reserved

  // Implementation defined parameters for OP-TEE FFA
  CommunicateSmcArgs.Arg3 = OPTEE_FFA_YIELDING_CALL_WITH_ARG;
  CommunicateSmcArgs.Arg4 = (UINTN)LOWER_32_BITS (mSharedMemoryHandle);
  CommunicateSmcArgs.Arg5 = (UINTN)UPPER_32_BITS (mSharedMemoryHandle);
  CommunicateSmcArgs.Arg6 = 0;  // Offset in shared memory (0 for MessageArg)
  CommunicateSmcArgs.Arg7 = 0;  // Reserved

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Closing session 0x%x with fTPM TA via FFA direct message\n",
    __FUNCTION__,
    Session
    ));

  // Send the request
  Status = SendFfaDirectReqAndResp (&CommunicateSmcArgs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send FFA direct request: %r\n", __FUNCTION__, Status));
    return Status;
  }

  return Status;
}

/**
  Convert OPTEE invoke function parameters to OPTEE message parameters for FFA

  This function converts input parameters from OPTEE_INVOKE_FUNCTION_ARG format
  to OPTEE_MESSAGE_PARAM format suitable for transmission via FFA shared memory.
  It handles different parameter types including values and memory buffers.

  For memory parameters, the function:
  - Copies buffer data to shared memory region
  - Sets up FMemory structure with shared memory handle and offsets
  - Manages memory layout to avoid overlaps

  @param[out] MessageParams  Array of OPTEE message parameters to fill
  @param[in]  NumParams      Number of parameters to process
  @param[in]  InParams       Array of input parameters from invoke function

  @retval EFI_SUCCESS           Parameters converted successfully
  @retval EFI_INVALID_PARAMETER Invalid parameter type encountered

**/
STATIC
EFI_STATUS
OpteeTpmToMessageParam (
  OUT OPTEE_MESSAGE_PARAM  *MessageParams,
  IN UINT32                NumParams,
  IN OPTEE_MESSAGE_PARAM   *InParams
  )
{
  UINT32  Idx;
  UINTN   ParamSharedMemoryAddress;
  UINTN   SharedMemorySize;
  UINTN   Size;

  // Calculate aligned size for OPTEE_MESSAGE_ARG (8-byte aligned)
  Size = (sizeof (OPTEE_MESSAGE_ARG) + sizeof (UINT64) - 1) &
         ~(sizeof (UINT64) - 1);

  // Start parameter shared memory after the MessageArg structure
  ParamSharedMemoryAddress = SharedMemAddr + Size;
  SharedMemorySize         = RXTX_BUFFER_SIZE - Size;

  // Process each parameter and convert to FFA message format
  for (Idx = 0; Idx < NumParams; Idx++) {
    CONST OPTEE_MESSAGE_PARAM  *InParam;
    OPTEE_MESSAGE_PARAM        *MessageParam;
    UINT32                     Attribute;

    InParam      = InParams + Idx;      // Input parameter from caller
    MessageParam = MessageParams + Idx; // Output message parameter for FFA
    Attribute    = InParam->Attribute & OPTEE_MESSAGE_ATTRIBUTE_TYPE_MASK;

    // Handle different parameter types based on attribute
    switch (Attribute) {
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_NONE:
        MessageParam->Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_NONE;
        ZeroMem (&MessageParam->Union, sizeof (MessageParam->Union));
        break;

      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT:
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_OUTPUT:
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INOUT:
        MessageParam->Attribute     = Attribute;
        MessageParam->Union.Value.A = InParam->Union.Value.A;
        MessageParam->Union.Value.B = InParam->Union.Value.B;
        MessageParam->Union.Value.C = InParam->Union.Value.C;
        break;

      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INPUT:
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_OUTPUT:
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT:
        // Check if we have enough shared memory space
        Size = (InParam->Union.Memory.Size + sizeof (UINT64) - 1) &
               ~(sizeof (UINT64) - 1); // 8-byte alignment
        if (SharedMemorySize < Size) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Insufficient shared memory. Need %u bytes, available %u bytes\n",
            __FUNCTION__,
            Size,
            SharedMemorySize
            ));
          return EFI_OUT_OF_RESOURCES;
        }

        MessageParam->Attribute = Attribute;

        // Copy buffer data from caller's memory to shared memory region
        CopyMem (
          (VOID *)ParamSharedMemoryAddress,
          (VOID *)(UINTN)InParam->Union.Memory.BufferAddress,
          InParam->Union.Memory.Size
          );

        // Set up FMemory structure for FFA shared memory access
        MessageParam->Union.FMemory.GlobalId = (UINT64)mSharedMemoryHandle;     // FFA shared memory handle
        MessageParam->Union.FMemory.Size     = InParam->Union.Memory.Size;      // Buffer size
        // Calculate offset properly to avoid casting issues
        UINT64  Offset = ParamSharedMemoryAddress - SharedMemAddr;
        MessageParam->Union.FMemory.OffsLow  = LOWER_32_BITS (Offset); // Lower 32 bits of offset
        MessageParam->Union.FMemory.OffsHigh = UPPER_32_BITS (Offset); // Upper 32 bits of offset

        // Calculate aligned size and advance to next parameter slot
        ParamSharedMemoryAddress += Size;
        SharedMemorySize         -= Size;
        break;

      case OPTEE_MESSAGE_ATTR_TYPE_TMEM_INPUT:
      case OPTEE_MESSAGE_ATTR_TYPE_TMEM_OUTPUT:
      case OPTEE_MESSAGE_ATTR_TYPE_TMEM_INOUT:
        DEBUG ((
          DEBUG_ERROR,
          "%a: TMEM parameter types not supported (Attribute: 0x%x)\n",
          __FUNCTION__,
          Attribute
          ));
        return EFI_UNSUPPORTED;

      default:
        DEBUG ((
          DEBUG_ERROR,
          "%a: Unknown parameter attribute: 0x%x\n",
          __FUNCTION__,
          Attribute
          ));
        return EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

/**
  Convert OPTEE message parameters back to OPTEE invoke function parameters

  This function converts response parameters from OPTEE_MESSAGE_PARAM format
  (received via FFA shared memory) back to OPTEE_INVOKE_FUNCTION_ARG format.
  It handles the reverse conversion of OpteeTpmToMessageParam.

  For memory parameters, the function:
  - Calculates buffer address from FMemory offsets and shared memory base
  - Copies data from shared memory back to original buffers
  - Preserves parameter attributes and sizes

  @param[out] OutParams     Array of output parameters for invoke function
  @param[in]  NumParams     Number of parameters to process
  @param[in]  MessageParams Array of message parameters from FFA response

  @retval EFI_SUCCESS           Parameters converted successfully
  @retval EFI_INVALID_PARAMETER Invalid parameter type encountered

**/
STATIC
EFI_STATUS
OpteeTpmFromMessageParam (
  OUT OPTEE_MESSAGE_PARAM  *OutParams,
  IN UINT32                NumParams,
  IN OPTEE_MESSAGE_PARAM   *MessageParams
  )
{
  UINT32  Idx;

  // Process each parameter and convert from FFA message format back to caller format
  for (Idx = 0; Idx < NumParams; Idx++) {
    OPTEE_MESSAGE_PARAM  *OutParam;
    OPTEE_MESSAGE_PARAM  *MessageParam;
    UINT32               Attribute;
    UINT64               BufferAddress;

    OutParam     = OutParams + Idx;     // Output parameter for caller
    MessageParam = MessageParams + Idx; // Input message parameter from FFA response
    Attribute    = MessageParam->Attribute & OPTEE_MESSAGE_ATTRIBUTE_TYPE_MASK;

    // Handle different parameter types based on attribute
    switch (Attribute) {
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_NONE:
        OutParam->Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_NONE;
        ZeroMem (&OutParam->Union, sizeof (OutParam->Union));
        break;

      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT:
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_OUTPUT:
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INOUT:
        OutParam->Attribute     = Attribute;
        OutParam->Union.Value.A = MessageParam->Union.Value.A;
        OutParam->Union.Value.B = MessageParam->Union.Value.B;
        OutParam->Union.Value.C = MessageParam->Union.Value.C;
        break;

      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INPUT:
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_OUTPUT:
      case OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT:
        OutParam->Attribute = Attribute;

        // Reconstruct buffer address from FMemory offset and shared memory base
        BufferAddress = ((UINT64)MessageParam->Union.FMemory.OffsLow |
                         (UINT64)MessageParam->Union.FMemory.OffsHigh << 32) + SharedMemAddr;

        // Copy data from shared memory back to caller's buffer
        CopyMem (
          (VOID *)(UINTN)OutParam->Union.Memory.BufferAddress,
          (VOID *)(UINTN)BufferAddress,
          MessageParam->Union.Memory.Size
          );

        // Update the output parameter size (may have been modified by OP-TEE)
        OutParam->Union.Memory.Size = MessageParam->Union.Memory.Size;
        break;

      case OPTEE_MESSAGE_ATTR_TYPE_TMEM_INPUT:
      case OPTEE_MESSAGE_ATTR_TYPE_TMEM_OUTPUT:
      case OPTEE_MESSAGE_ATTR_TYPE_TMEM_INOUT:
        // TMEM support needs to be fulfilled here
        break;
      default:
        return EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

/**
  FFA implementation
  Invoke an fTPM TA cmd request

  @param[inout] TpmTaArg  OPTEE_INVOKE_FUNCTION_ARG for fTPM TA cmd

  @retval EFI_SUCCESS     The operation completed successfully.

**/
STATIC
EFI_STATUS
OpteeTpmInvoke (
  IN OUT OPTEE_INVOKE_FUNCTION_ARG  *InvokeFunctionArg
  )
{
  ARM_SMC_ARGS       CommunicateSmcArgs = { 0 };
  EFI_STATUS         Status             = EFI_SUCCESS;
  OPTEE_MESSAGE_ARG  *MessageArg;

  MessageArg = (OPTEE_MESSAGE_ARG *)SharedMemAddr;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));

  MessageArg->Session = SessionId;

  MessageArg->Command   = OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION;
  MessageArg->Function  = InvokeFunctionArg->Function;
  MessageArg->NumParams = OPTEE_MAX_CALL_PARAMS;

  Status = OpteeTpmToMessageParam (
             MessageArg->Params,
             2,
             InvokeFunctionArg->Params
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set message param %r \n", __FUNCTION__, Status));
    return Status;
  }

  CommunicateSmcArgs.Arg0 = ARM_SMC_ID_FFA_MSG_SEND_DIRECT_REQ_64;
  CommunicateSmcArgs.Arg1 = OpteeVmId;
  CommunicateSmcArgs.Arg2 = 0;

  // Implementation defined parameters for OP-TEE FFA
  CommunicateSmcArgs.Arg3 = OPTEE_FFA_YIELDING_CALL_WITH_ARG;
  CommunicateSmcArgs.Arg4 = (UINTN)LOWER_32_BITS (mSharedMemoryHandle);
  CommunicateSmcArgs.Arg5 = (UINTN)UPPER_32_BITS (mSharedMemoryHandle);
  CommunicateSmcArgs.Arg6 = 0; // Second parameter (offset) for yielding_call_with_arg - set to 0 for no offset
  CommunicateSmcArgs.Arg7 = 0; // Reserved

  Status = SendFfaDirectReqAndResp (&CommunicateSmcArgs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FF-A direct Msg failed\n", __FUNCTION__));
    return Status;
  }

  Status = OpteeTpmFromMessageParam (
             InvokeFunctionArg->Params,
             2,
             MessageArg->Params
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get message param\n", __FUNCTION__));
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
    return Status;
  }

  InvokeFunctionArg->Return       = MessageArg->Return;
  InvokeFunctionArg->ReturnOrigin = MessageArg->ReturnOrigin;

  DEBUG ((
    DEBUG_ERROR,
    "%a: FF-A direct Msg completed, return = 0x%x, session = 0x%x\n",
    __FUNCTION__,
    InvokeFunctionArg->Return,
    InvokeFunctionArg->Session
    ));

  switch (InvokeFunctionArg->Return) {
    case OPTEE_SUCCESS:
      Status = EFI_SUCCESS;
      break;
    case OPTEE_ERROR_ITEM_NOT_FOUND:
      Status = EFI_NOT_FOUND;
      break;
    case OPTEE_ERROR_OUT_OF_MEMORY:
      Status = EFI_OUT_OF_RESOURCES;
      break;
    default:
      Status = EFI_NO_RESPONSE;
      break;
  }

  return Status;
}

/**
  Build a complete FFA Memory Transaction Descriptor (MTD) based on Linux kernel arm_ffa.h

  @param[out] Mtd                 Pointer to MTD structure to fill
  @param[in]  PhysicalAddress     Physical address of memory to share
  @param[in]  Pages               Number of pages to share
  @param[in]  ReceiverEndpointId  Endpoint ID of the receiver
  @param[in]  AccessPermissions   Access permissions for the receiver
  @param[in]  MemoryAttributes    Memory attributes (cacheability, shareability)
  @param[in]  Tag                 Optional tag for identification

  @retval EFI_SUCCESS           MTD built successfully
  @retval EFI_INVALID_PARAMETER Invalid parameters
**/
STATIC
EFI_STATUS
BuildFfaMtd (
  OUT FFA_COMPLETE_MTD  *Mtd,
  IN  PHYSICAL_ADDRESS  PhysicalAddress,
  IN  UINT64            Pages,
  IN  UINT16            ReceiverEndpointId,
  IN  UINT8             AccessPermissions,
  IN  UINT16            MemoryAttributes,
  IN  UINT64            Tag
  )
{
  UINT32  CompositeOffset;

  if (Mtd == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Mtd, sizeof (FFA_COMPLETE_MTD));

  // Calculate offset to composite memory region
  CompositeOffset = sizeof (FFA_MEM_REGION) + sizeof (FFA_MEM_REGION_ATTRIBUTES);

  // Fill FFA_MEM_REGION Header (based on Linux kernel struct ffa_mem_region)
  Mtd->Header.SenderId    = 0; // Sender ID - 0 indicates current endpoint as sender
  Mtd->Header.Attributes  = MemoryAttributes;
  Mtd->Header.Flags       = 0;
  Mtd->Header.Handle      = 0; // Will be filled by hypervisor
  Mtd->Header.Tag         = Tag;
  Mtd->Header.EpCount     = 1; // Single endpoint
  Mtd->Header.EpMemSize   = sizeof (FFA_MEM_REGION_ATTRIBUTES);
  Mtd->Header.EpMemOffset = sizeof (FFA_MEM_REGION);
  Mtd->Header.Reserved[0] = 0;
  Mtd->Header.Reserved[1] = 0;
  Mtd->Header.Reserved[2] = 0;

  // Fill FFA_MEM_REGION_ATTRIBUTES (based on Linux kernel struct ffa_mem_region_attributes)
  Mtd->EndpointAttributes.Receiver        = ReceiverEndpointId;
  Mtd->EndpointAttributes.Attrs           = AccessPermissions;
  Mtd->EndpointAttributes.Flag            = 0;
  Mtd->EndpointAttributes.CompositeOffset = CompositeOffset;
  Mtd->EndpointAttributes.Reserved        = 0;

  // Fill FFA_COMPOSITE_MEM_REGION (based on Linux kernel struct ffa_composite_mem_region)
  Mtd->CompositeRegion.TotalPageCount = (UINT32)Pages;
  Mtd->CompositeRegion.AddrRangeCount = 1;  // Single address range
  Mtd->CompositeRegion.Reserved       = 0;

  // Fill FFA_MEM_REGION_ADDR_RANGE (based on Linux kernel struct ffa_mem_region_addr_range)
  Mtd->AddressRange.Address   = PhysicalAddress;
  Mtd->AddressRange.PageCount = (UINT32)Pages;
  Mtd->AddressRange.Reserved  = 0;

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Linux kernel style MTD built - Addr: 0x%llx, Pages: %llu, Receiver: 0x%x, Attrs: 0x%x\n",
    __FUNCTION__,
    PhysicalAddress,
    Pages,
    ReceiverEndpointId,
    AccessPermissions
    ));

  return EFI_SUCCESS;
}

/**
  Enhanced MEM_SHARE implementation using Linux kernel style MTD descriptor

  @param[in]  PhysicalAddress     Physical address of memory to share
  @param[in]  Pages               Number of pages to share
  @param[in]  ReceiverEndpointId  Endpoint ID of the receiver
  @param[in]  AccessPermissions   Access permissions for the receiver
  @param[in]  MemoryAttributes    Memory attributes (cacheability, shareability)
  @param[out] Handle              Memory share handle returned by SPM

  @retval EFI_SUCCESS           Memory shared successfully
  @retval EFI_INVALID_PARAMETER Invalid parameters
  @retval EFI_OUT_OF_RESOURCES  Failed to share memory
**/
STATIC
EFI_STATUS
OpteeTpmMemShareWithMtd (
  IN  PHYSICAL_ADDRESS  PhysicalAddress,
  IN  UINT64            Pages,
  IN  UINT16            ReceiverEndpointId,
  IN  UINT8             AccessPermissions,
  IN  UINT16            MemoryAttributes,
  OUT UINT64            *Handle
  )
{
  ARM_SMC_ARGS      ArmSmcArgs = { 0 };
  EFI_STATUS        Status     = EFI_SUCCESS;
  FFA_COMPLETE_MTD  *Mtd;

  if ((PhysicalAddress == 0) || (Pages == 0) || (Handle == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  // Use TX buffer to store MTD
  Mtd = (FFA_COMPLETE_MTD *)mTxBuffer;

  // Build Linux kernel style MTD descriptor
  Status = BuildFfaMtd (
             Mtd,
             PhysicalAddress,
             Pages,
             ReceiverEndpointId,
             AccessPermissions,
             MemoryAttributes,
             0 // Tag
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to build MTD: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Prepare FFA MEM_SHARE call with MTD in TX buffer
  ArmSmcArgs.Arg0 = ARM_SMC_ID_FFA_MEM_SHARE_32;
  ArmSmcArgs.Arg1 = sizeof (FFA_COMPLETE_MTD); // MTD length
  ArmSmcArgs.Arg2 = sizeof (FFA_COMPLETE_MTD); // Fragment length - fit in one go
  ArmSmcArgs.Arg3 = 0;                         // Reserved
  ArmSmcArgs.Arg4 = 0;                         // Reserved
  ArmSmcArgs.Arg5 = 0;                         // Reserved
  ArmSmcArgs.Arg6 = 0;                         // Reserved
  ArmSmcArgs.Arg7 = 0;                         // Reserved

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: MEM_SHARE with MTD - Length: %d, Addr: 0x%llx, Pages: %llu\n",
    __FUNCTION__,
    sizeof (FFA_COMPLETE_MTD),
    PhysicalAddress,
    Pages
    ));

  CallFfaSmc (&ArmSmcArgs);

  // Check FFA call result
  if (ArmSmcArgs.Arg0 != FFA_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: FFA MEM_SHARE with MTD failed: Arg0=0x%llx, Arg2=0x%llx\n",
      __FUNCTION__,
      ArmSmcArgs.Arg0,
      ArmSmcArgs.Arg2
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto out;
  }

  // Extract handle from response (typically in Arg2 for MTD-based sharing)
  *Handle = ((UINT64)ArmSmcArgs.Arg3 << 32) | (UINT64)ArmSmcArgs.Arg2;

out:
  return Status;
}

/**
  Example usage of MTD-based memory sharing for OPTEE communication

  @param[in]  Pages               Number of pages to allocate and share
  @param[out] PhysicalAddress     Physical address of allocated memory
  @param[out] Handle              Memory share handle

  @retval EFI_SUCCESS           Memory allocated and shared successfully
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate or share memory
**/
STATIC
EFI_STATUS
TpmFfaAllocateAndShareMemWithMtd (
  IN  UINT64            Pages,
  OUT PHYSICAL_ADDRESS  *PhysicalAddress,
  OUT UINT64            *Handle
  )
{
  EFI_STATUS        Status;
  PHYSICAL_ADDRESS  AllocatedAddress;

  if ((Pages == 0) || (PhysicalAddress == NULL) || (Handle == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  // Allocate memory pages
  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiBootServicesData,
                  Pages,
                  &AllocatedAddress
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate %llu pages\n", __FUNCTION__, Pages));
    return Status;
  }

  // Share the allocated memory using MTD
  Status = OpteeTpmMemShareWithMtd (
             AllocatedAddress,
             Pages,
             OpteeVmId,                                                     // Receiver endpoint ID
             FFA_MEM_RW,                                                    // Access permissions
             FFA_MEM_NORMAL | FFA_MEM_WRITE_BACK | FFA_MEM_INNER_SHAREABLE, // Memory attributes
             Handle
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to share memory with MTD\n", __FUNCTION__));
    // Free the allocated memory on failure
    gBS->FreePages (AllocatedAddress, Pages);
    return Status;
  }

  *PhysicalAddress = AllocatedAddress;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
TpmFfaAllocateAndMapRxTxBuffers (
  VOID
  )
{
  EFI_STATUS    Status     = EFI_SUCCESS;
  ARM_SMC_ARGS  ArmSmcArgs = { 0 };

  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData, RXTX_PAGE_COUNT, &mRxBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RX buffer allocation failed\n", __FUNCTION__));
    goto ExitMapRxTxBuffers;
  }

  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData, RXTX_PAGE_COUNT, &mTxBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TX buffer allocation failed\n", __FUNCTION__));
    goto ExitMapRxTxBuffers;
  }

  ArmSmcArgs.Arg0 = ARM_SMC_ID_FFA_RXTX_MAP_64;
  ArmSmcArgs.Arg1 = (UINTN)mTxBuffer;
  ArmSmcArgs.Arg2 = (UINTN)mRxBuffer;
  ArmSmcArgs.Arg3 = RXTX_PAGE_COUNT;

  ArmCallSmc (&ArmSmcArgs);

  // Check FFA call result
  if (ArmSmcArgs.Arg0 != FFA_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SMC_ID_FFA_RXTX_MAP_64 failed: Arg0=0x%llx, Arg2=0x%llx\n",
      __FUNCTION__,
      ArmSmcArgs.Arg0,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
    goto ExitMapRxTxBuffers;
  }

  return Status;

ExitMapRxTxBuffers:
  if (mTxBuffer != 0) {
    gBS->FreePages (mTxBuffer, RXTX_PAGE_COUNT);
    mTxBuffer = 0;
  }

  if (mRxBuffer != 0) {
    gBS->FreePages (mRxBuffer, RXTX_PAGE_COUNT);
    mRxBuffer = 0;
  }

  return Status;
}

STATIC
EFI_STATUS
TpmFfaUnmapRxTxBuffers (
  VOID
  )
{
  EFI_STATUS    Status     = EFI_SUCCESS;
  ARM_SMC_ARGS  ArmSmcArgs = { 0 };

  ArmSmcArgs.Arg0 = ARM_SMC_ID_FFA_RXTX_UNMAP_32;
  ArmSmcArgs.Arg1 = 0;

  ArmCallSmc (&ArmSmcArgs);

  // Check FFA call result
  if ((ArmSmcArgs.Arg0 != FFA_SUCCESS) ||
      (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SMC_ID_FFA_RXTX_UNMAP_32 failed: Arg0=0x%llx, Arg2=0x%llx\n",
      __FUNCTION__,
      ArmSmcArgs.Arg0,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
  }

  return Status;
}

/**
  Init the optee interface for fTPM

  @retval EFI_SUCCESS  Init Optee interface successfully.
**/
STATIC
EFI_STATUS
OpteeTpmInterfaceInit (
  VOID
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  Status = GetOpteeVmId ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get VmId\n", __FUNCTION__, Status));
    return Status;
  }

  Status = GetSpmVersion ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get SPM version \n", __FUNCTION__, Status));
    return Status;
  }

  // Init RXTX buf for fTPM TA FF-A interface
  Status = TpmFfaAllocateAndMapRxTxBuffers ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate and map RX/TX buffers: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Initialize MEM_SHARE functionality by allocating and sharing a small buffer using MTD
  Status = TpmFfaAllocateAndShareMemWithMtd (
             EFI_SIZE_TO_PAGES (OpteeFtpmShmSize),
             &SharedMemAddr,
             &mSharedMemoryHandle
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to initialize MEM_SHARE with MTD: %r\n", __FUNCTION__, Status));
    goto ExitOpteeTpmInterfaceInit;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: MEM_SHARE with MTD initialized successfully, handle: 0x%llx\n",
    __FUNCTION__,
    mSharedMemoryHandle
    ));

  return EFI_SUCCESS;

ExitOpteeTpmInterfaceInit:

  TpmFfaUnmapRxTxBuffers ();

  if (mTxBuffer != 0) {
    gBS->FreePages (mTxBuffer, RXTX_PAGE_COUNT);
    mTxBuffer = 0;
  }

  if (mRxBuffer != 0) {
    gBS->FreePages (mRxBuffer, RXTX_PAGE_COUNT);
    mRxBuffer = 0;
  }

  return Status;
}

/**
  This service enables the sending of commands to the TPM2.

  @param[in]      InputParameterBlockSize  Size of the TPM2 input parameter block.
  @param[in]      InputParameterBlock      Pointer to the TPM2 input parameter block.
  @param[in,out]  OutputParameterBlockSize Size of the TPM2 output parameter block.
  @param[in]      OutputParameterBlock     Pointer to the TPM2 output parameter block.

  @retval EFI_SUCCESS            The command byte stream was successfully sent to the device and a response was successfully received.
  @retval EFI_DEVICE_ERROR       The command was not successfully sent to the device or a response was not successfully received from the device.
  @retval EFI_BUFFER_TOO_SMALL   The output parameter block is too small.
**/
EFI_STATUS
EFIAPI
Tpm2SubmitCommand (
  IN UINT32      InputParameterBlockSize,
  IN UINT8       *InputParameterBlock,
  IN OUT UINT32  *OutputParameterBlockSize,
  IN UINT8       *OutputParameterBlock
  )
{
  OPTEE_INVOKE_FUNCTION_ARG  InvokeFunctionArg;
  EFI_STATUS                 Status = EFI_SUCCESS;

  // Input validation
  if ((InputParameterBlock == NULL) ||
      (OutputParameterBlock == NULL) ||
      (OutputParameterBlockSize == NULL))
  {
    DEBUG ((DEBUG_ERROR, "%a: Invalid NULL parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (InputParameterBlockSize == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid input size (0)\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (*OutputParameterBlockSize == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid output size (0)\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (InputParameterBlockSize > OpteeFtpmShmSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: InSufficient ShmSize. Requested %u Avail %u\n",
      __FUNCTION__,
      InputParameterBlockSize,
      OpteeFtpmShmSize
      ));
    Status = EFI_OUT_OF_RESOURCES;
    ASSERT (FALSE);
    goto ExitTpm2SubmitCommand;
  }

  InvokeFunctionArg.Function = FTPM_SUBMIT_COMMAND;
  ZeroMem ((UINT8 *)OpteeFtpmShmIpBuf, OpteeFtpmShmSize);
  ZeroMem ((UINT8 *)OpteeFtpmShmOpBuf, OpteeFtpmShmSize);

  CopyMem (OpteeFtpmShmIpBuf, InputParameterBlock, InputParameterBlockSize);

  InvokeFunctionArg.Params[0].Attribute                  =  OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INPUT;
  InvokeFunctionArg.Params[0].Union.Memory.Size          =  InputParameterBlockSize;
  InvokeFunctionArg.Params[0].Union.Memory.BufferAddress =  (UINT64)OpteeFtpmShmIpBuf;

  InvokeFunctionArg.Params[1].Attribute                  =  OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT;
  InvokeFunctionArg.Params[1].Union.Memory.Size          =  *OutputParameterBlockSize;
  InvokeFunctionArg.Params[1].Union.Memory.BufferAddress =  (UINT64)OpteeFtpmShmOpBuf;
  Status                                                 = OpteeTpmInvoke (&InvokeFunctionArg);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to invoke command to optee tpm %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitTpm2SubmitCommand;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Return %u Origin %u \n",
    __FUNCTION__,
    InvokeFunctionArg.Return,
    InvokeFunctionArg.ReturnOrigin
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: Copying %u bytes\n",
    __FUNCTION__,
    *OutputParameterBlockSize
    ));

  if (InvokeFunctionArg.Params[1].Union.Memory.Size > *OutputParameterBlockSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Provided buffer too small %u expected %u\n",
      __FUNCTION__,
      *OutputParameterBlockSize,
      InvokeFunctionArg.Params[1].Union.Memory.Size
      ));
    Status                    = EFI_BUFFER_TOO_SMALL;
    *OutputParameterBlockSize =  InvokeFunctionArg.Params[1].Union.Memory.Size;
    goto ExitTpm2SubmitCommand;
  } else {
    *OutputParameterBlockSize =  InvokeFunctionArg.Params[1].Union.Memory.Size;
    CopyMem (OutputParameterBlock, OpteeFtpmShmOpBuf, *OutputParameterBlockSize);
  }

ExitTpm2SubmitCommand:
  return Status;
}

/**
 * FreeBuffers
   Free all the buffers allocated.
 */
STATIC
VOID
FreeBuffers (
  VOID
  )
{
  if ( mRxBuffer != 0) {
    gBS->FreePages (mRxBuffer, RXTX_PAGE_COUNT);
    mRxBuffer = 0;
  }

  if ( mTxBuffer != 0) {
    gBS->FreePages (mTxBuffer, RXTX_PAGE_COUNT);
    mTxBuffer = 0;
  }

  if (OpteeFtpmShmIpBuf != NULL) {
    FreeAlignedPages (OpteeFtpmShmIpBuf, EFI_SIZE_TO_PAGES (OpteeFtpmShmSize));
    OpteeFtpmShmIpBuf = NULL;
  }

  if (OpteeFtpmShmOpBuf != NULL) {
    FreeAlignedPages (OpteeFtpmShmOpBuf, EFI_SIZE_TO_PAGES (OpteeFtpmShmSize));
    OpteeFtpmShmOpBuf = NULL;
  }

  if ( SharedMemAddr != 0) {
    gBS->FreePages (SharedMemAddr, EFI_SIZE_TO_PAGES (OpteeFtpmShmSize));
    SharedMemAddr = 0;
  }

  mSharedMemoryHandle = 0;
}

/**
 * ExitBootServicesCallback
   Close the fTPM session and unregister Shared memory buffers.

   @param[in] Event    Event identifier of the callback.
   @param[in] Context  Context data of the callback
 **/
VOID
EFIAPI
ExitBootServicesCallBack (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if (SessionId != 0) {
    OpteeTpmCloseSession (SessionId);
    SessionId = 0;
  }

  TpmFfaUnmapRxTxBuffers ();

  FreeBuffers ();
}

/**
  This service requests use TPM2. Use this function to setup the
  Optee session and shared buffers


  @retval EFI_SUCCESS      Get the control of TPM2 chip.
  @retval Other            Failed to initialize communication to fTPM.
**/
EFI_STATUS
EFIAPI
Tpm2RequestUseTpm (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle = NULL;

  OpteeFtpmShmSize = PcdGet64 (PcdFtpmShmSize);

  Status = OpteeTpmInterfaceInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to init tpm ffa: %r\n", __FUNCTION__, Status));
    return Status;
  }

  OpteeFtpmShmIpBuf = AllocateAlignedPages (
                        EFI_SIZE_TO_PAGES (OpteeFtpmShmSize),
                        OPTEE_MSG_PAGE_SIZE
                        );
  if (OpteeFtpmShmIpBuf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate Input Buffer\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitTpm2RequestUseTpm;
  }

  OpteeFtpmShmOpBuf = AllocateAlignedPages (
                        EFI_SIZE_TO_PAGES (OpteeFtpmShmSize),
                        OPTEE_MSG_PAGE_SIZE
                        );
  if (OpteeFtpmShmOpBuf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate Output Buffer\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitTpm2RequestUseTpm;
  }

  // Open session with fTPM TA
  Status = OpteeTpmOpenSession (&SessionId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to open session: %r\n", __FUNCTION__, Status));
    goto ExitTpm2RequestUseTpm;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gNVIDIAFtpmPresentProtocolGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install fTPM present Protocol %r \n",
      __FUNCTION__,
      Status
      ));
    goto ExitTpm2RequestUseTpm;
  }

  return Status;

ExitTpm2RequestUseTpm:

  OpteeTpmCloseSession (SessionId);
  SessionId = 0;
  TpmFfaUnmapRxTxBuffers ();
  FreeBuffers ();

  return Status;
}
