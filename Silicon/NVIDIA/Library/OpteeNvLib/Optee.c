/** @file
  Api's to communicate with OP-TEE OS (Trusted OS based on ARM TrustZone) via
  secure monitor calls.

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <IndustryStandard/ArmStdSmc.h>

#include <Library/ArmMmuLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#include <OpteeSmc.h>

STATIC OPTEE_SHARED_MEMORY_INFORMATION  OpteeSharedMemoryInformation = { 0 };
STATIC BOOLEAN                          IsRpmbPresent                = FALSE;
STATIC BOOLEAN                          InRuntime;

/**
  Check for OP-TEE presence.
**/
BOOLEAN
EFIAPI
IsOpteePresent (
  VOID
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));
  // Send a Trusted OS Calls UID command
  ArmSmcArgs.Arg0 = ARM_SMC_ID_TOS_UID;
  ArmCallSmc (&ArmSmcArgs);

  if ((ArmSmcArgs.Arg0 == OPTEE_OS_UID0) &&
      (ArmSmcArgs.Arg1 == OPTEE_OS_UID1) &&
      (ArmSmcArgs.Arg2 == OPTEE_OS_UID2) &&
      (ArmSmcArgs.Arg3 == OPTEE_OS_UID3))
  {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*
 * OpteeExchangeCapabilities
 * Get the capabilities of the OP-TEE Trusted OS
 *
 * @param[in]   Cap       Output Buffer that contains a bitmask of
                          the capabilities of the OP-TEE image.
 * @return      TRUE      On successful call to OP-TEE secure OS.
 *              FALSE     If OP-TEE returns a failure.
 *
 */
BOOLEAN
EFIAPI
OpteeExchangeCapabilities (
  OUT UINT64  *Cap
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));
  ArmSmcArgs.Arg0 = ARM_SMC_ID_TOS_CAPABILITIES;
  ArmSmcArgs.Arg1 = OPTEE_SMC_NSEC_CAP_UNIPROCESSOR;
  ArmCallSmc (&ArmSmcArgs);

  if ((ArmSmcArgs.Arg0 == OPTEE_SMC_RETURN_OK)) {
    *Cap = ArmSmcArgs.Arg1;
    return TRUE;
  } else {
    return FALSE;
  }
}

/*
 * OpteeRegisterShm
 * Register a Buffer with OP-TEE if it supports Dynamic Shared Memory
 *
 * @param[in]   PageList         Optional PageList to be sent to OP-TEE, this
                                 should be aligned to a 4k boundary.
 * @param[in]   UserBuf          User buffer to be shared with OP-TEE, this
 *                               should also be aliged to a 4k boundary.
 * @param[in]   BufSize          Size of the buffer
 * @param[out]  RetPtr           The return pointer contains a pointer to a
 *                               Page list that is passed to OP-TEE
 *
 * @return      TRUE             On successful call to OP-TEE secure OS.
 *              FALSE            If OP-TEE returns a failure.
 *
 */
STATIC
EFI_STATUS
OpteeSetupPageList (
  IN  OPTEE_SHM_PAGE_LIST  *PageList OPTIONAL,
  IN  VOID                 *UserBuf,
  IN  UINTN                BufSize,
  OUT UINT64               *RetPtr
  )
{
  EFI_STATUS           Status   = EFI_SUCCESS;
  UINTN                NumPages = EFI_SIZE_TO_PAGES (BufSize);
  UINTN                Pages    = NumPages;
  UINTN                n        = 0;
  VOID                 *BufBase = UserBuf;
  OPTEE_SHM_PAGE_LIST  *ShmList;
  UINTN                NumPgLists = 0;

  if (!(UserBuf && IS_ALIGNED (UserBuf, OPTEE_MSG_PAGE_SIZE))) {
    DEBUG ((DEBUG_ERROR, "UserBuf %p is not valid\n", UserBuf));
    return EFI_INVALID_PARAMETER;
  }

  ShmList = PageList;
  if (ShmList != NULL) {
    if (!IS_ALIGNED (ShmList, OPTEE_MSG_PAGE_SIZE)) {
      DEBUG ((DEBUG_ERROR, "Invalid Shm List Buffer\n"));
      return EFI_INVALID_PARAMETER;
    }
  } else {
    NumPgLists = (NumPages / MAX_PAGELIST_ENTRIES) + 1;
    ShmList    =
      AllocateAlignedRuntimePages (EFI_SIZE_TO_PAGES (NumPages * sizeof (OPTEE_SHM_PAGE_LIST)), OPTEE_MSG_PAGE_SIZE);
    if (ShmList == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  while (Pages) {
    ShmList->PagesArray[n] = (UINT64)BufBase;
    n++;
    Pages--;
    BufBase += OPTEE_MSG_PAGE_SIZE;
    if (n == MAX_PAGELIST_ENTRIES) {
      ShmList->NextPage = (UINT64)(ShmList + 1);
      ShmList++;
      n = 0;
    }
  }

  *RetPtr = (UINT64)ShmList;
  return Status;
}

/*
 * OpteeRegisterShm
 * Register a Buffer with OP-TEE if it supports Dynamic Shared Memory
 *
 * @param[in]   Buf              Input Buffer[Should be aligned to 4k Boundary]
 * @param[in]   SharedMemCookie  Cookie to refer to this shared memory segment
 *                               during future transactions
 * @param[in]   Size             Size of the buffer
 * @param[in]   Shm              Optional buffer[Should be aligned to 4k
 *                               Boundary to be used when sending the message
 *                               to OP-TEE].
 *
 */
EFI_STATUS
EFIAPI
OpteeRegisterShm (
  IN VOID                 *Buf,
  IN UINT64               SharedMemCookie,
  IN UINTN                Size,
  IN OPTEE_SHM_PAGE_LIST  *Shm OPTIONAL
  )
{
  EFI_STATUS         Status = EFI_SUCCESS;
  OPTEE_MESSAGE_ARG  *MessageArg;
  UINT64             PageList;
  UINT32             RetCode;

  if (OpteeSharedMemoryInformation.PBase == 0) {
    DEBUG ((DEBUG_WARN, "OP-TEE not initialized\n"));
    return EFI_NOT_STARTED;
  }

  MessageArg = (OPTEE_MESSAGE_ARG *)OpteeSharedMemoryInformation.VBase;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));
  MessageArg->Command = OPTEE_MESSAGE_COMMAND_REGISTER_SHM;

  Status = OpteeSetupPageList (Shm, Buf, Size, &PageList);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Setup Page list for OPTEE:%r\n", Status));
    goto Exit;
  }

  MessageArg->Params[0].Attribute = OPTEE_MESSAGE_ATTR_TYPE_TMEM_OUTPUT |
                                    OPTEE_MESSAGE_ATTR_NONCONTIG;
  MessageArg->Params[0].Union.Memory.BufferAddress         = PageList;
  MessageArg->Params[0].Union.Memory.Size                  = Size;
  MessageArg->Params[0].Union.Memory.SharedMemoryReference = SharedMemCookie;
  MessageArg->NumParams                                    = 1;

  RetCode = OpteeCallWithArg (OpteeSharedMemoryInformation.PBase);
  if (RetCode != 0) {
    DEBUG ((DEBUG_ERROR, "Error(%u) from OP-TEE REGISTER_SHM\n", RetCode));
    Status = EFI_ACCESS_DENIED;
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
OpteeUnRegisterShm (
  IN UINT64  SharedMemCookie
  )
{
  EFI_STATUS         Status = EFI_SUCCESS;
  OPTEE_MESSAGE_ARG  *MessageArg;
  UINT32             RetCode;

  if (OpteeSharedMemoryInformation.PBase == 0) {
    DEBUG ((DEBUG_WARN, "OP-TEE not initialized\n"));
    return EFI_NOT_STARTED;
  }

  MessageArg = (OPTEE_MESSAGE_ARG *)OpteeSharedMemoryInformation.VBase;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));
  MessageArg->Command = OPTEE_MESSAGE_COMMAND_UNREGISTER_SHM;

  MessageArg->Params[0].Attribute                          = OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INPUT;
  MessageArg->Params[0].Union.Memory.SharedMemoryReference = SharedMemCookie;
  MessageArg->NumParams                                    = 1;

  RetCode = OpteeCallWithArg (OpteeSharedMemoryInformation.PBase);
  if (RetCode != 0) {
    DEBUG ((DEBUG_ERROR, "Error(%u) from OP-TEE UNREGISTER_SHM\n", RetCode));
    Status = EFI_ACCESS_DENIED;
  }

  return Status;
}

STATIC
EFI_STATUS
OpteeSharedMemoryRemap (
  VOID
  )
{
  ARM_SMC_ARGS          ArmSmcArgs;
  EFI_PHYSICAL_ADDRESS  PhysicalAddress;
  EFI_PHYSICAL_ADDRESS  Start;
  EFI_PHYSICAL_ADDRESS  End;
  EFI_STATUS            Status;
  UINTN                 Size;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));
  ArmSmcArgs.Arg0 = OPTEE_SMC_GET_SHARED_MEMORY_CONFIG;

  ArmCallSmc (&ArmSmcArgs);
  if (ArmSmcArgs.Arg0 != OPTEE_SMC_RETURN_OK) {
    DEBUG ((DEBUG_WARN, "OP-TEE shared memory not supported\n"));
    return EFI_UNSUPPORTED;
  }

  if (ArmSmcArgs.Arg3 != OPTEE_SMC_SHARED_MEMORY_CACHED) {
    DEBUG ((DEBUG_WARN, "OP-TEE: Only normal cached shared memory supported\n"));
    return EFI_UNSUPPORTED;
  }

  Start           = (ArmSmcArgs.Arg1 + SIZE_4KB - 1) & ~(SIZE_4KB - 1);
  End             = (ArmSmcArgs.Arg1 + ArmSmcArgs.Arg2) & ~(SIZE_4KB - 1);
  PhysicalAddress = Start;
  Size            = End - Start;

  if (Size < SIZE_4KB) {
    DEBUG ((DEBUG_WARN, "OP-TEE shared memory too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  Status = ArmSetMemoryAttributes (PhysicalAddress, Size, EFI_MEMORY_WB);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  OpteeSharedMemoryInformation.PBase = (UINT64)PhysicalAddress;
  OpteeSharedMemoryInformation.VBase = (UINT64)PhysicalAddress;
  OpteeSharedMemoryInformation.Size  = Size;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
OpteeInit (
  VOID
  )
{
  EFI_STATUS  Status;

  if (!IsOpteePresent ()) {
    DEBUG ((DEBUG_WARN, "OP-TEE not present\n"));
    return EFI_UNSUPPORTED;
  }

  Status = OpteeSharedMemoryRemap ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "OP-TEE shared memory remap failed\n"));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
HandleCmdFree (
  OPTEE_MESSAGE_ARG  *Msg
  )
{
  BOOLEAN           Ret     = TRUE;
  OPTEE_SHM_COOKIE  *Cookie = NULL;

  if ((Msg->NumParams != 1) || (Msg->Params[0].Attribute != OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT)) {
    DEBUG ((DEBUG_WARN, "Bad Params Num %d, Attr %d \n", Msg->NumParams, Msg->Params[0].Attribute));
    Msg->Return = OPTEE_ERROR_BAD_PARAMS;
    goto Error;
  }

  Cookie = (OPTEE_SHM_COOKIE *)((UINT64)Msg->Params[0].Union.Value.B);
  FreeAlignedPages (Cookie->Addr, EFI_SIZE_TO_PAGES (Cookie->Size));
  Msg->Return = OPTEE_SUCCESS;
Error:
  return Ret;
}

STATIC
BOOLEAN
HandleCmdAlloc (
  OPTEE_MESSAGE_ARG  *Msg
  )
{
  BOOLEAN           Ret = TRUE;
  UINTN             Size;
  VOID              *Buf    = NULL;
  OPTEE_SHM_COOKIE  *Cookie = NULL;
  UINT64            PageList;
  EFI_STATUS        Status;

  if ((Msg->NumParams != 1) || (Msg->Params[0].Attribute != OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT)) {
    DEBUG ((DEBUG_WARN, "Bad Params Num %d, Attr %d \n", Msg->NumParams, Msg->Params[0].Attribute));
    Msg->Return = OPTEE_ERROR_BAD_PARAMS;
    goto Error;
  }

  Size = Msg->Params[0].Union.Value.B;

  Buf = AllocateAlignedRuntimePages (EFI_SIZE_TO_PAGES (Size), OPTEE_MSG_PAGE_SIZE);
  if (Buf == NULL) {
    DEBUG ((DEBUG_WARN, "Failed to alloc buf"));
    goto Error;
  }

  Cookie = AllocateAlignedRuntimePages (EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE)), OPTEE_MSG_PAGE_SIZE);
  if (Cookie == NULL) {
    DEBUG ((DEBUG_WARN, "Failed to alloc cookie"));
    goto Error;
  }

  Cookie->Addr = Buf;
  Cookie->Size = Size;
  Status       = OpteeSetupPageList (NULL, Buf, Size, &PageList);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Failed to register %r\n", Status));
    goto Error;
  }

  Msg->Params[0].Attribute = OPTEE_MESSAGE_ATTR_TYPE_TMEM_OUTPUT | OPTEE_MESSAGE_ATTR_NONCONTIG;
  // Need to free this Pagelist
  Msg->Params[0].Union.Memory.BufferAddress         = (UINT64)PageList;
  Msg->Params[0].Union.Memory.Size                  = Size;
  Msg->Params[0].Union.Memory.SharedMemoryReference = (UINT64)Cookie;

  Msg->Return = OPTEE_SUCCESS;
Error:
  return Ret;
}

STATIC
VOID
HandleCmdNotification (
  OPTEE_MESSAGE_ARG  *Msg
  )
{
  if (Msg->NumParams != 1) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid NumParams %u \n", __FUNCTION__, Msg->NumParams));
    goto err;
  }

  switch (Msg->Params[0].Union.Value.A) {
    case NOTIFICATION_MSG_WAIT:
      DEBUG ((DEBUG_INFO, "SecureWorld is busy, do an unconditional 500ms Stall"));
      gBS->Stall (500000);
      break;
    case NOTIFICATION_MSG_WAKE:
      DEBUG ((DEBUG_INFO, "SecureWorld is ready\n"));
      break;
    default:
      DEBUG ((
        DEBUG_INFO,
        "Unknown Notification %d\n",
        Msg->Params[0].Union.Value.A
        ));
      break;
  }

  Msg->Return = OPTEE_SUCCESS;
  return;
err:
  Msg->Return = OPTEE_ERROR_BAD_PARAMS;
}

STATIC
VOID
HandleRpcCmd (
  ARM_SMC_ARGS  *Regs
  )
{
  OPTEE_SHM_COOKIE   *Cookie = (VOID *)(UINT64)(((UINT64)Regs->Arg1 << 32) | Regs->Arg2);
  OPTEE_MESSAGE_ARG  *Msg    = Cookie->Addr;

  switch (Msg->Command) {
    case OPTEE_MSG_RPC_CMD_SHM_ALLOC:
      if (!InRuntime) {
        HandleCmdAlloc (Msg);
      }

      break;
    case OPTEE_MSG_RPC_CMD_SHM_FREE:
      if (!InRuntime) {
        HandleCmdFree (Msg);
      }

      break;
    case OPTEE_MSG_RPC_CMD_RPMB:
      if (IsRpmbPresent) {
        HandleCmdRpmb (Msg);
      }

      break;
    case OPTEE_MSG_RPC_CMD_NOTIFICATION:
      HandleCmdNotification (Msg);
      break;
    default:
      DEBUG ((DEBUG_WARN, "Unhandled command %d \n", Msg->Command));
      break;
  }

  Msg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
}

STATIC
OPTEE_SHM_COOKIE *
HandleRpcAlloc (
  UINT32  Size
  )
{
  VOID              *Buf    = NULL;
  OPTEE_SHM_COOKIE  *Cookie = NULL;

  Buf = AllocateAlignedRuntimePages (EFI_SIZE_TO_PAGES (Size), OPTEE_MSG_PAGE_SIZE);
  if (Buf == NULL) {
    DEBUG ((DEBUG_WARN, "Failed to alloc buf"));
    goto Error;
  }

  Cookie = AllocateAlignedRuntimePages (EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE)), OPTEE_MSG_PAGE_SIZE);
  if (Cookie == NULL) {
    DEBUG ((DEBUG_WARN, "Failed to alloc cookie"));
    goto Error;
  }

  Cookie->Addr = Buf;
  Cookie->Size = Size;
  return Cookie;
Error:
  if (Buf != NULL) {
    FreePool (Buf);
  }

  if (Cookie != NULL) {
    FreePool (Cookie);
  }

  return NULL;
}

STATIC
BOOLEAN
IsOpteeSmcReturnRpc (
  UINT32  Return
  )
{
  return ((Return != OPTEE_SMC_RETURN_UNKNOWN_FUNCTION) &&
          ((Return & OPTEE_SMC_RETURN_RPC_PREFIX_MASK) ==
           OPTEE_SMC_RETURN_RPC_PREFIX));
}

/**
  Does Standard SMC to OP-TEE in secure world.

  @param[in]  PhysicalArg   Physical address of message to pass to secure world

  @return                   0 on success, secure world return code otherwise

**/
UINT32
EFIAPI
OpteeCallWithArg (
  IN UINT64  PhysicalArg
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));
  ArmSmcArgs.Arg0 = OPTEE_SMC_CALL_WITH_ARG;
  ArmSmcArgs.Arg1 = (UINT32)(PhysicalArg >> 32);
  ArmSmcArgs.Arg2 = (UINT32)PhysicalArg;

  while (TRUE) {
    ArmCallSmc (&ArmSmcArgs);

    if (IsOpteeSmcReturnRpc (ArmSmcArgs.Arg0)) {
      OPTEE_SHM_COOKIE  *Cookie = 0;

      switch (ArmSmcArgs.Arg0) {
        case OPTEE_SMC_RETURN_RPC_FOREIGN_INTERRUPT:
          //
          // A foreign interrupt was raised while secure world was
          // executing, since they are handled in UEFI a dummy RPC is
          // performed to let UEFI take the interrupt through the normal
          // vector.
          //
          break;
        case OPTEE_SMC_RETURN_RPC_FUNC_ALLOC:
          if (!InRuntime) {
            Cookie = HandleRpcAlloc (ArmSmcArgs.Arg1);
            if (Cookie != NULL) {
              ArmSmcArgs.Arg1 = (UINT32)((UINT64)Cookie->Addr >> 32);
              ArmSmcArgs.Arg2 = (UINT32)((UINT64)Cookie->Addr);
              ArmSmcArgs.Arg4 = (UINT32)((UINT64)Cookie >> 32);
              ArmSmcArgs.Arg5 = (UINT32)((UINT64)Cookie);
            }
          }

          break;
        case OPTEE_SMC_RETURN_RPC_FUNC_CMD:
          HandleRpcCmd (&ArmSmcArgs);
          break;
        case OPTEE_SMC_RETURN_RPC_FUNC_FREE:
        default:
          DEBUG ((
            DEBUG_WARN,
            "%a: Function 0x%x not implemented.\n",
            __FUNCTION__,
            ArmSmcArgs.Arg0
            ));
          // Do nothing in case RPC is not implemented.
          break;
      }

      ArmSmcArgs.Arg0 = OPTEE_SMC_RETURN_FROM_RPC;
    } else {
      break;
    }
  }

  return ArmSmcArgs.Arg0;
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

EFI_STATUS
EFIAPI
OpteeOpenSession (
  IN OUT OPTEE_OPEN_SESSION_ARG  *OpenSessionArg
  )
{
  OPTEE_MESSAGE_ARG  *MessageArg;

  MessageArg = NULL;

  if (OpteeSharedMemoryInformation.PBase == 0) {
    DEBUG ((DEBUG_WARN, "OP-TEE not initialized\n"));
    return EFI_NOT_STARTED;
  }

  MessageArg = (OPTEE_MESSAGE_ARG *)OpteeSharedMemoryInformation.VBase;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));

  MessageArg->Command = OPTEE_MESSAGE_COMMAND_OPEN_SESSION;

  //
  // Initialize and add the meta parameters needed when opening a
  // session.
  //
  MessageArg->Params[0].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT |
                                    OPTEE_MESSAGE_ATTRIBUTE_META;
  MessageArg->Params[1].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT |
                                    OPTEE_MESSAGE_ATTRIBUTE_META;
  EfiGuidToRfc4122Uuid (
    (RFC4122_UUID *)&MessageArg->Params[0].Union.Value,
    &OpenSessionArg->Uuid
    );
  ZeroMem (&MessageArg->Params[1].Union.Value, sizeof (EFI_GUID));
  MessageArg->Params[1].Union.Value.C = OPTEE_LOGIN_PUBLIC;

  MessageArg->NumParams = 2;

  if (OpteeCallWithArg (OpteeSharedMemoryInformation.PBase) != 0) {
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
  }

  OpenSessionArg->Session      = MessageArg->Session;
  OpenSessionArg->Return       = MessageArg->Return;
  OpenSessionArg->ReturnOrigin = MessageArg->ReturnOrigin;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
OpteeCloseSession (
  IN UINT32  Session
  )
{
  OPTEE_MESSAGE_ARG  *MessageArg;

  MessageArg = NULL;

  if (OpteeSharedMemoryInformation.PBase == 0) {
    DEBUG ((DEBUG_WARN, "OP-TEE not initialized\n"));
    return EFI_NOT_STARTED;
  }

  MessageArg = (OPTEE_MESSAGE_ARG *)OpteeSharedMemoryInformation.VBase;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));

  MessageArg->Command = OPTEE_MESSAGE_COMMAND_CLOSE_SESSION;
  MessageArg->Session = Session;

  OpteeCallWithArg (OpteeSharedMemoryInformation.PBase);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
OpteeToMessageParam (
  OUT OPTEE_MESSAGE_PARAM  *MessageParams,
  IN UINT32                NumParams,
  IN OPTEE_MESSAGE_PARAM   *InParams
  )
{
  UINT32  Idx;
  UINTN   ParamSharedMemoryAddress;
  UINTN   SharedMemorySize;
  UINTN   Size;

  Size = (sizeof (OPTEE_MESSAGE_ARG) + sizeof (UINT64) - 1) &
         ~(sizeof (UINT64) - 1);
  ParamSharedMemoryAddress = OpteeSharedMemoryInformation.VBase + Size;
  SharedMemorySize         = OpteeSharedMemoryInformation.Size - Size;

  for (Idx = 0; Idx < NumParams; Idx++) {
    CONST OPTEE_MESSAGE_PARAM  *InParam;
    OPTEE_MESSAGE_PARAM        *MessageParam;
    UINT32                     Attribute;

    InParam      = InParams + Idx;
    MessageParam = MessageParams + Idx;
    Attribute    = InParam->Attribute & OPTEE_MESSAGE_ATTRIBUTE_TYPE_MASK;

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
        MessageParam->Attribute = Attribute;

        if (InParam->Union.Memory.Size > SharedMemorySize) {
          return EFI_OUT_OF_RESOURCES;
        }

        CopyMem (
          (VOID *)ParamSharedMemoryAddress,
          (VOID *)(UINTN)InParam->Union.Memory.BufferAddress,
          InParam->Union.Memory.Size
          );
        MessageParam->Union.Memory.BufferAddress = (UINT64)ParamSharedMemoryAddress;
        MessageParam->Union.Memory.Size          = InParam->Union.Memory.Size;

        Size = (InParam->Union.Memory.Size + sizeof (UINT64) - 1) &
               ~(sizeof (UINT64) - 1);
        ParamSharedMemoryAddress += Size;
        SharedMemorySize         -= Size;
        break;

      default:
        return EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
OpteeFromMessageParam (
  OUT OPTEE_MESSAGE_PARAM  *OutParams,
  IN UINT32                NumParams,
  IN OPTEE_MESSAGE_PARAM   *MessageParams
  )
{
  UINT32  Idx;

  for (Idx = 0; Idx < NumParams; Idx++) {
    OPTEE_MESSAGE_PARAM        *OutParam;
    CONST OPTEE_MESSAGE_PARAM  *MessageParam;
    UINT32                     Attribute;

    OutParam     = OutParams + Idx;
    MessageParam = MessageParams + Idx;
    Attribute    = MessageParam->Attribute & OPTEE_MESSAGE_ATTRIBUTE_TYPE_MASK;

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

        if (MessageParam->Union.Memory.Size > OutParam->Union.Memory.Size) {
          return EFI_BAD_BUFFER_SIZE;
        }

        CopyMem (
          (VOID *)(UINTN)OutParam->Union.Memory.BufferAddress,
          (VOID *)(UINTN)MessageParam->Union.Memory.BufferAddress,
          MessageParam->Union.Memory.Size
          );
        OutParam->Union.Memory.Size = MessageParam->Union.Memory.Size;
        break;

      default:
        return EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
OpteeInvokeFunction (
  IN OUT OPTEE_INVOKE_FUNCTION_ARG  *InvokeFunctionArg
  )
{
  EFI_STATUS         Status;
  OPTEE_MESSAGE_ARG  *MessageArg;

  MessageArg = NULL;

  if (OpteeSharedMemoryInformation.PBase == 0) {
    DEBUG ((DEBUG_WARN, "OP-TEE not initialized\n"));
    return EFI_NOT_STARTED;
  }

  MessageArg = (OPTEE_MESSAGE_ARG *)OpteeSharedMemoryInformation.VBase;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));

  MessageArg->Command  = OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION;
  MessageArg->Function = InvokeFunctionArg->Function;
  MessageArg->Session  = InvokeFunctionArg->Session;

  Status = OpteeToMessageParam (
             MessageArg->Params,
             OPTEE_MAX_CALL_PARAMS,
             InvokeFunctionArg->Params
             );
  if (Status) {
    return Status;
  }

  MessageArg->NumParams = OPTEE_MAX_CALL_PARAMS;

  if (OpteeCallWithArg (OpteeSharedMemoryInformation.PBase) != 0) {
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
  }

  if (OpteeFromMessageParam (
        InvokeFunctionArg->Params,
        OPTEE_MAX_CALL_PARAMS,
        MessageArg->Params
        ) != 0)
  {
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
  }

  InvokeFunctionArg->Return       = MessageArg->Return;
  InvokeFunctionArg->ReturnOrigin = MessageArg->ReturnOrigin;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
OpteeSetProperties (
  UINT64   PBuf,
  UINT64   VBuf,
  UINT64   Size,
  BOOLEAN  RpmbPresent
  )
{
  OpteeSharedMemoryInformation.PBase = PBuf;
  OpteeSharedMemoryInformation.VBase = VBuf;
  OpteeSharedMemoryInformation.Size  = Size;
  IsRpmbPresent                      = RpmbPresent;
  return EFI_SUCCESS;
}

VOID
EFIAPI
OpteeLibNotifyRuntime (
  BOOLEAN  Runtime
  )
{
  InRuntime = Runtime;
}
