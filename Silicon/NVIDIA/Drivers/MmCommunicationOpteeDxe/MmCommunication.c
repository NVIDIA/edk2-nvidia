/** @file

  Copyright (c) 2022-2023, NVIDIA Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/ArmLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/PrintLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Guid/RtPropertiesTable.h>

#include <Protocol/MmCommunication2.h>

#include <IndustryStandard/ArmStdSmc.h>

#include "MmCommunicate.h"
typedef struct {
  UINTN                  TotalSize;
  UINTN                  MmCommBufSize;
  VOID                   *OpteeMsgArgPa;
  VOID                   *OpteeMsgArgVa;
  VOID                   *MmCommBufPa;
  VOID                   *MmCommBufVa;
  OPTEE_SHM_COOKIE       *MmMsgCookiePa;
  OPTEE_SHM_COOKIE       *MmMsgCookieVa;
  OPTEE_SHM_PAGE_LIST    *ShmListPa;
  OPTEE_SHM_PAGE_LIST    *ShmListVa;
} OPTEE_MM_SESSION;

STATIC OPTEE_MM_SESSION  OpteeMmSession;
STATIC EFI_STATUS
OpteeMmCommunicate (
  IN OUT VOID  *CommBuf,
  IN UINTN     CommSize
  );

STATIC BOOLEAN  OpteePresent = FALSE;
STATIC BOOLEAN  RpmbPresent  = FALSE;

//
// Address, Length of the pre-allocated buffer for communication with the secure
// world.
//
STATIC ARM_MEMORY_REGION_DESCRIPTOR  mNsCommBuffMemRegion;

// Notification event when virtual address map is set.
STATIC EFI_EVENT  mSetVirtualAddressMapEvent;

//
// Handle to install the MM Communication Protocol
//
STATIC EFI_HANDLE  mMmCommunicateHandle;

STATIC
BOOLEAN
EFIAPI
IsRpmbPresent (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT32      NumberOfPlatformNodes;

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,p2972-0000", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,galen", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,e3360_1099", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  return FALSE;
}

/**
  Communicates with a registered handler.

  This function provides a service to send and receive messages from a registered UEFI service.

  @param[in] This                     The EFI_MM_COMMUNICATION_PROTOCOL instance.
  @param[in, out] CommBufferPhysical  Physical address of the MM communication buffer
  @param[in, out] CommBufferVirtual   Virtual address of the MM communication buffer
  @param[in, out] CommSize            The size of the data buffer being passed in. On input,
                                      when not omitted, the buffer should cover EFI_MM_COMMUNICATE_HEADER
                                      and the value of MessageLength field. On exit, the size
                                      of data being returned. Zero if the handler does not
                                      wish to reply with any data. This parameter is optional
                                      and may be NULL.

  @retval EFI_SUCCESS            The message was successfully posted.
  @retval EFI_INVALID_PARAMETER  CommBufferPhysical or CommBufferVirtual was NULL, or
                                 integer value pointed by CommSize does not cover
                                 EFI_MM_COMMUNICATE_HEADER and the value of MessageLength
                                 field.
  @retval EFI_BAD_BUFFER_SIZE    The buffer is too large for the MM implementation.
                                 If this error is returned, the MessageLength field
                                 in the CommBuffer header or the integer pointed by
                                 CommSize, are updated to reflect the maximum payload
                                 size the implementation can accommodate.
  @retval EFI_ACCESS_DENIED      The CommunicateBuffer parameter or CommSize parameter,
                                 if not omitted, are in address range that cannot be
                                 accessed by the MM environment.

**/
EFI_STATUS
EFIAPI
MmCommunication2Communicate (
  IN CONST EFI_MM_COMMUNICATION2_PROTOCOL  *This,
  IN OUT VOID                              *CommBufferPhysical,
  IN OUT VOID                              *CommBufferVirtual,
  IN OUT UINTN                             *CommSize OPTIONAL
  )
{
  EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader;
  ARM_SMC_ARGS               CommunicateSmcArgs;
  EFI_STATUS                 Status;
  UINTN                      BufferSize;

  Status     = EFI_ACCESS_DENIED;
  BufferSize = 0;

  ZeroMem (&CommunicateSmcArgs, sizeof (ARM_SMC_ARGS));

  //
  // Check parameters
  //
  if ((CommBufferVirtual == NULL) || (CommBufferPhysical == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status            = EFI_SUCCESS;
  CommunicateHeader = CommBufferVirtual;
  // CommBuffer is a mandatory parameter. Hence, Rely on
  // MessageLength + Header to ascertain the
  // total size of the communication payload rather than
  // rely on optional CommSize parameter
  BufferSize = CommunicateHeader->MessageLength +
               sizeof (CommunicateHeader->HeaderGuid) +
               sizeof (CommunicateHeader->MessageLength);

  // If CommSize is not omitted, perform size inspection before proceeding.
  if (CommSize != NULL) {
    // This case can be used by the consumer of this driver to find out the
    // max size that can be used for allocating CommBuffer.
    if ((*CommSize == 0) ||
        (*CommSize > mNsCommBuffMemRegion.Length))
    {
      *CommSize = mNsCommBuffMemRegion.Length;
      Status    = EFI_BAD_BUFFER_SIZE;
    }

    //
    // CommSize should cover at least MessageLength + sizeof (EFI_MM_COMMUNICATE_HEADER);
    //
    if (*CommSize < BufferSize) {
      Status = EFI_INVALID_PARAMETER;
    }
  }

  //
  // If the message length is 0 or greater than what can be tolerated by the MM
  // environment then return the expected size.
  //
  if ((CommunicateHeader->MessageLength == 0) ||
      (BufferSize > mNsCommBuffMemRegion.Length))
  {
    CommunicateHeader->MessageLength = mNsCommBuffMemRegion.Length -
                                       sizeof (CommunicateHeader->HeaderGuid) -
                                       sizeof (CommunicateHeader->MessageLength);
    Status = EFI_BAD_BUFFER_SIZE;
  }

  // MessageLength or CommSize check has failed, return here.
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (OpteePresent) {
    Status = OpteeMmCommunicate (CommBufferVirtual, BufferSize);
  } else {
    // SMC Function ID
    CommunicateSmcArgs.Arg0 = ARM_SMC_ID_MM_COMMUNICATE_AARCH64;

    // Cookie
    CommunicateSmcArgs.Arg1 = 0;

    // Copy Communication Payload
    CopyMem ((VOID *)mNsCommBuffMemRegion.VirtualBase, CommBufferVirtual, BufferSize);

    // comm_buffer_address (64-bit physical address)
    CommunicateSmcArgs.Arg2 = (UINTN)mNsCommBuffMemRegion.PhysicalBase;

    // comm_size_address (not used, indicated by setting to zero)
    CommunicateSmcArgs.Arg3 = 0;

    // Call the Standalone MM environment.
    ArmCallSmc (&CommunicateSmcArgs);

    switch (CommunicateSmcArgs.Arg0) {
      case ARM_SMC_MM_RET_SUCCESS:
        ZeroMem (CommBufferVirtual, BufferSize);
        // On successful return, the size of data being returned is inferred from
        // MessageLength + Header.
        CommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)mNsCommBuffMemRegion.VirtualBase;
        BufferSize        = CommunicateHeader->MessageLength +
                            sizeof (CommunicateHeader->HeaderGuid) +
                            sizeof (CommunicateHeader->MessageLength);

        CopyMem (
          CommBufferVirtual,
          (VOID *)mNsCommBuffMemRegion.VirtualBase,
          BufferSize
          );
        Status = EFI_SUCCESS;
        break;

      case ARM_SMC_MM_RET_INVALID_PARAMS:
        Status = EFI_INVALID_PARAMETER;
        break;

      case ARM_SMC_MM_RET_DENIED:
        Status = EFI_ACCESS_DENIED;
        break;

      case ARM_SMC_MM_RET_NO_MEMORY:
        // Unexpected error since the CommSize was checked for zero length
        // prior to issuing the SMC
        Status = EFI_OUT_OF_RESOURCES;
        ASSERT (0);
        break;

      default:
        Status = EFI_ACCESS_DENIED;
        ASSERT (0);
    }
  }

  return Status;
}

//
// MM Communication Protocol instance
//
STATIC EFI_MM_COMMUNICATION2_PROTOCOL  mMmCommunication2 = {
  MmCommunication2Communicate
};

/**
 * OP-TEE specific changes for MmCommunicate
 */
STATIC
EFI_STATUS
OpteeStmmInit (
  VOID
  )
{
  EFI_STATUS              Status = EFI_SUCCESS;
  OPTEE_OPEN_SESSION_ARG  OpenSessionArg;
  VOID                    *OpteeBuf;
  UINTN                   TotalOpteeBufSize = 0;
  UINT64                  Capabilities      = 0;
  UINTN                   MmMsgCookieSizePg = EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE));
  UINTN                   OpteeMsgBufSizePg = EFI_SIZE_TO_PAGES (sizeof (OPTEE_MESSAGE_ARG));
  UINTN                   ShmPageListSizePg = EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_PAGE_LIST));
  UINTN                   MmCommBufSizePg   = 0;

  if (!OpteePresent) {
    DEBUG ((DEBUG_ERROR, "OP-Tee is not present\n"));
    return EFI_UNSUPPORTED;
  }

  if (RpmbPresent) {
    DEBUG ((DEBUG_INFO, "OP-Tee MM is not supported on RPMB platforms.\n"));
    return EFI_UNSUPPORTED;
  }

  if (PcdGetBool (PcdTegraStmmEnabled) == FALSE) {
    DEBUG ((DEBUG_INFO, "PCD to Enable MM set to False\n"));
    return EFI_UNSUPPORTED;
  }

  if (!OpteeExchangeCapabilities (&Capabilities)) {
    DEBUG ((
      DEBUG_ERROR,
      "Failed to exchange capabilities with OP-TEE(%r)\n",
      Status
      ));
    return Status;
  }

  if (Capabilities & OPTEE_SMC_SEC_CAP_DYNAMIC_SHM) {
    MmCommBufSizePg = EFI_SIZE_TO_PAGES (PcdGet64 (PcdMmBufferSize));
    if (MmCommBufSizePg == 0) {
      DEBUG ((DEBUG_ERROR, "Mm Comm Buf size is not provided"));
      return EFI_UNSUPPORTED;
    }

    OpteeMmSession.MmCommBufSize = EFI_PAGES_TO_SIZE (MmCommBufSizePg);
    // Allocate one contiguous buffer for all the OP-TEE and MM Buffers.
    TotalOpteeBufSize = OpteeMsgBufSizePg + MmMsgCookieSizePg + MmCommBufSizePg
                        + ShmPageListSizePg;
    OpteeBuf = AllocateAlignedRuntimePages (
                 TotalOpteeBufSize,
                 OPTEE_MSG_PAGE_SIZE
                 );
    if (OpteeBuf == NULL) {
      DEBUG ((DEBUG_ERROR, "Failed to allocate Comm Buf"));
      Status = EFI_OUT_OF_RESOURCES;
      return Status;
    }

    OpteeMmSession.OpteeMsgArgPa = OpteeBuf;
    OpteeMmSession.OpteeMsgArgVa = OpteeMmSession.OpteeMsgArgPa;
    OpteeMmSession.TotalSize     = EFI_PAGES_TO_SIZE (TotalOpteeBufSize);
    OpteeMmSession.MmCommBufPa   = OpteeBuf + EFI_PAGES_TO_SIZE (OpteeMsgBufSizePg);

    OpteeMmSession.MmCommBufVa   = OpteeMmSession.MmCommBufPa;
    OpteeMmSession.MmMsgCookiePa = OpteeBuf + EFI_PAGES_TO_SIZE (OpteeMsgBufSizePg) +
                                   EFI_PAGES_TO_SIZE (MmCommBufSizePg);
    OpteeMmSession.MmMsgCookieVa = OpteeMmSession.MmMsgCookiePa;
    OpteeMmSession.ShmListPa     = OpteeBuf +  EFI_PAGES_TO_SIZE (OpteeMsgBufSizePg) +
                                   EFI_PAGES_TO_SIZE (MmCommBufSizePg) +
                                   EFI_PAGES_TO_SIZE (ShmPageListSizePg);
    OpteeMmSession.ShmListVa           = OpteeMmSession.ShmListPa;
    OpteeMmSession.MmMsgCookiePa->Addr = OpteeMmSession.MmMsgCookieVa->Addr = OpteeMmSession.MmCommBufPa;
    OpteeMmSession.MmMsgCookiePa->Size = OpteeMmSession.MmMsgCookieVa->Size = EFI_PAGES_TO_SIZE (MmCommBufSizePg);

    OpteeSetProperties (
      (UINT64)OpteeMmSession.OpteeMsgArgPa,
      (UINT64)OpteeMmSession.OpteeMsgArgVa,
      OpteeMmSession.TotalSize
      );

    ZeroMem (&OpenSessionArg, sizeof (OPTEE_OPEN_SESSION_ARG));
    CopyMem (&OpenSessionArg.Uuid, &gEfiSmmVariableProtocolGuid, sizeof (EFI_GUID));
    Status = OpteeOpenSession (&OpenSessionArg);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Open Optee Session %r\n", Status));
      return Status;
    } else {
      if (OpenSessionArg.Return != OPTEE_SUCCESS) {
        DEBUG ((
          DEBUG_ERROR,
          "Failed to Open Session to OPTEE STMM %u\n",
          OpenSessionArg.Return
          ));
        Status = EFI_UNSUPPORTED;
        goto Error;
      }
    }

    Status = OpteeRegisterShm (
               OpteeMmSession.MmCommBufPa,
               (UINT64)OpteeMmSession.MmMsgCookiePa,
               OpteeMmSession.MmCommBufSize,
               OpteeMmSession.ShmListPa
               );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to register Mmsg %r\n", Status));
      goto Error;
    }
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "Unsupported OP-TEE Communication Method.(%x)\n",
      Capabilities
      ));
    return EFI_UNSUPPORTED;
  }

  OpteeCloseSession (OpenSessionArg.Session);
  return Status;
Error:
  FreeAlignedPages (OpteeBuf, TotalOpteeBufSize);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
OpteeMmConvertPointers (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = gRT->ConvertPointer (
                  EFI_OPTIONAL_PTR,
                  (VOID **)&OpteeMmSession.OpteeMsgArgVa
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, " Error converting main %r\n", Status));
  }

  Status = gRT->ConvertPointer (
                  EFI_OPTIONAL_PTR,
                  (VOID **)&OpteeMmSession.MmCommBufVa
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, " Error converting mmmsg %r\n", Status));
  }

  Status = gRT->ConvertPointer (
                  EFI_OPTIONAL_PTR,
                  (VOID **)&OpteeMmSession.MmMsgCookieVa
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, " Error converting mmmsgcookie %r\n", Status));
  }

  Status = gRT->ConvertPointer (
                  EFI_OPTIONAL_PTR,
                  (VOID **)&OpteeMmSession.ShmListVa
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, " Error converting shm %r\n", Status));
  }

  Status = gRT->ConvertPointer (
                  EFI_OPTIONAL_PTR,
                  (VOID **)&mMmCommunication2.Communicate
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, " Error converting Proto Fn %r\n", Status));
  }

  OpteeSetProperties (
    (UINT64)OpteeMmSession.OpteeMsgArgPa,
    (UINT64)OpteeMmSession.OpteeMsgArgVa,
    OpteeMmSession.TotalSize
    );

  return Status;
}

STATIC
EFI_STATUS
OpteeMmCommunicate (
  IN OUT VOID  *CommBuf,
  IN UINTN     CommSize
  )
{
  EFI_STATUS                 Status;
  EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader;
  UINTN                      BufferSize;
  OPTEE_MESSAGE_ARG          *MessageArg = NULL;
  OPTEE_OPEN_SESSION_ARG     OpenSessionArg;

  ZeroMem (&OpenSessionArg, sizeof (OPTEE_OPEN_SESSION_ARG));
  CopyMem (&OpenSessionArg.Uuid, &gEfiSmmVariableProtocolGuid, sizeof (EFI_GUID));
  Status = OpteeOpenSession (&OpenSessionArg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Open Optee Session %r\n", Status));
    return Status;
  }

  if (OpteeMmSession.OpteeMsgArgPa == 0) {
    DEBUG ((DEBUG_WARN, "OP-TEE not initialized\n"));
    Status = EFI_NOT_STARTED;
    goto Exit;
  }

  /* Not verifying the CommBufSize since the part that checks the buffer
     Size is common. */
  ZeroMem (OpteeMmSession.MmCommBufVa, OpteeMmSession.MmCommBufSize);
  CopyMem (
    (VOID *)OpteeMmSession.MmCommBufVa,
    (VOID *)CommBuf,
    CommSize
    );

  MessageArg = OpteeMmSession.OpteeMsgArgVa;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));
  MessageArg->Command                                      = OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION;
  MessageArg->Function                                     = OPTEE_MESSAGE_FUNCTION_STMM_COMMUNICATE;
  MessageArg->Session                                      = OpenSessionArg.Session;
  MessageArg->Params[0].Attribute                          = OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT;
  MessageArg->Params[0].Union.Memory.Size                  = OpteeMmSession.MmCommBufSize;
  MessageArg->Params[0].Union.Memory.SharedMemoryReference =
    (UINT64)OpteeMmSession.MmMsgCookiePa;

  MessageArg->Params[1].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_OUTPUT;
  MessageArg->NumParams           = 2;

  if (OpteeCallWithArg ((UINTN)OpteeMmSession.OpteeMsgArgPa) != 0) {
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
    Status                   = EFI_ACCESS_DENIED;
    DEBUG ((DEBUG_ERROR, "Optee call failed %r\n", Status));
  } else {
    switch (MessageArg->Params[1].Union.Value.A) {
      case ARM_SMC_MM_RET_SUCCESS:
        ZeroMem (CommBuf, CommSize);
        // On successful return, the size of data being returned is inferred from
        // MessageLength + Header.
        CommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)OpteeMmSession.MmCommBufVa;
        BufferSize        = CommunicateHeader->MessageLength +
                            sizeof (CommunicateHeader->HeaderGuid) +
                            sizeof (CommunicateHeader->MessageLength);

        CopyMem (
          CommBuf,
          OpteeMmSession.MmCommBufVa,
          BufferSize
          );
        Status = EFI_SUCCESS;
        break;
      case ARM_SMC_MM_RET_INVALID_PARAMS:
        Status = EFI_INVALID_PARAMETER;
        break;
      case ARM_SMC_MM_RET_DENIED:
        Status = EFI_ACCESS_DENIED;
        break;
      case ARM_SMC_MM_RET_NO_MEMORY:
        Status = EFI_OUT_OF_RESOURCES;
        break;
      default:
        DEBUG ((DEBUG_ERROR, "Unknown Return %d\n", MessageArg->Params[1].Union.Value.A));
        Status = EFI_ACCESS_DENIED;
        break;
    }
  }

Exit:
  OpteeCloseSession (OpenSessionArg.Session);
  return Status;
}

/**
  Notification callback on SetVirtualAddressMap event.

  This function notifies the MM communication protocol interface on
  SetVirtualAddressMap event and converts pointers used in this driver
  from physical to virtual address.

  @param  Event          SetVirtualAddressMap event.
  @param  Context        A context when the SetVirtualAddressMap triggered.

  @retval EFI_SUCCESS    The function executed successfully.
  @retval Other          Some error occurred when executing this function.

**/
STATIC
VOID
EFIAPI
NotifySetVirtualAddressMap (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  if (!OpteePresent) {
    Status = gRT->ConvertPointer (
                    EFI_OPTIONAL_PTR,
                    (VOID **)&mNsCommBuffMemRegion.VirtualBase
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "NotifySetVirtualAddressMap():"
        " Unable to convert MM runtime pointer. Status:0x%r\n",
        Status
        ));
    }
  } else {
    OpteeMmConvertPointers ();
  }
}

STATIC
EFI_STATUS
GetMmCompatibility (
  )
{
  EFI_STATUS    Status;
  UINT32        MmVersion;
  ARM_SMC_ARGS  MmVersionArgs;

  if (OpteePresent) {
    Status = OpteeStmmInit ();
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "Failed to open Session to StMM/OPTEE %r.\n", Status));
    } else {
      DEBUG ((DEBUG_INFO, "Found StMM PTA managed by OPTEE.\n"));
    }
  } else {
    // MM_VERSION uses SMC32 calling conventions
    MmVersionArgs.Arg0 = ARM_SMC_ID_MM_VERSION_AARCH32;

    ArmCallSmc (&MmVersionArgs);

    MmVersion = MmVersionArgs.Arg0;

    if ((MM_MAJOR_VER (MmVersion) == MM_CALLER_MAJOR_VER) &&
        (MM_MINOR_VER (MmVersion) >= MM_CALLER_MINOR_VER))
    {
      DEBUG ((
        DEBUG_INFO,
        "MM Version: Major=0x%x, Minor=0x%x\n",
        MM_MAJOR_VER (MmVersion),
        MM_MINOR_VER (MmVersion)
        ));
      Status = EFI_SUCCESS;
    } else {
      DEBUG ((
        DEBUG_ERROR,
        "Incompatible MM Versions.\n Current Version: Major=0x%x, Minor=0x%x.\n Expected: Major=0x%x, Minor>=0x%x.\n",
        MM_MAJOR_VER (MmVersion),
        MM_MINOR_VER (MmVersion),
        MM_CALLER_MAJOR_VER,
        MM_CALLER_MINOR_VER
        ));
      Status = EFI_UNSUPPORTED;
    }
  }

  return Status;
}

STATIC EFI_GUID *CONST  mGuidedEventGuid[] = {
  &gEfiEndOfDxeEventGroupGuid,
  &gEfiEventExitBootServicesGuid,
  &gEfiEventReadyToBootGuid,
};

STATIC EFI_EVENT  mGuidedEvent[ARRAY_SIZE (mGuidedEventGuid)];

/**
  Event notification that is fired when GUIDed Event Group is signaled.

  @param  Event                 The Event that is being processed, not used.
  @param  Context               Event Context, not used.

**/
STATIC
VOID
EFIAPI
MmGuidedEventNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_MM_COMMUNICATE_HEADER  Header;
  UINTN                      Size;

  //
  // Use Guid to initialize EFI_SMM_COMMUNICATE_HEADER structure
  //
  CopyGuid (&Header.HeaderGuid, Context);
  Header.MessageLength = 1;
  Header.Data[0]       = 0;

  Size = sizeof (Header);
  MmCommunication2Communicate (&mMmCommunication2, &Header, &Header, &Size);
  if (CompareGuid (Context, &gEfiEventExitBootServicesGuid)) {
    OpteeLibNotifyRuntime (TRUE);
  }
}

/**
  The Entry Point for MM Communication

  This function installs the MM communication protocol interface and finds out
  what type of buffer management will be required prior to invoking the
  communication SMC.

  @param  ImageHandle    The firmware allocated handle for the EFI image.
  @param  SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS    The entry point is executed successfully.
  @retval Other          Some error occurred when executing this entry point.

**/
EFI_STATUS
EFIAPI
MmCommunication2Initialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_RT_PROPERTIES_TABLE  *RtProperties;
  EFI_STATUS               Status;
  UINTN                    Index;

  OpteePresent = IsOpteePresent ();
  RpmbPresent  = IsRpmbPresent ();

  // Check if we can make the MM call
  Status = GetMmCompatibility ();
  if (EFI_ERROR (Status)) {
    goto ReturnErrorStatus;
  }

  if (!OpteePresent) {
    mNsCommBuffMemRegion.PhysicalBase = PcdGet64 (PcdMmBufferBase);
    // During boot , Virtual and Physical are same
    mNsCommBuffMemRegion.VirtualBase = mNsCommBuffMemRegion.PhysicalBase;
    mNsCommBuffMemRegion.Length      = PcdGet64 (PcdMmBufferSize);

    Status = gDS->AddMemorySpace (
                    EfiGcdMemoryTypeReserved,
                    mNsCommBuffMemRegion.PhysicalBase,
                    mNsCommBuffMemRegion.Length,
                    EFI_MEMORY_WB |
                    EFI_MEMORY_XP |
                    EFI_MEMORY_RUNTIME
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "MmCommunicateInitialize: "
        "Failed to add MM-NS Buffer Memory Space\n"
        ));
      goto ReturnErrorStatus;
    }
  } else {
    mNsCommBuffMemRegion.PhysicalBase = (EFI_PHYSICAL_ADDRESS)OpteeMmSession.MmCommBufPa;
    mNsCommBuffMemRegion.Length       = OpteeMmSession.MmCommBufSize;
    mNsCommBuffMemRegion.VirtualBase  = mNsCommBuffMemRegion.PhysicalBase;
  }

  ASSERT (mNsCommBuffMemRegion.PhysicalBase != 0);
  ASSERT (mNsCommBuffMemRegion.Length != 0);
  Status = gDS->SetMemorySpaceAttributes (
                  mNsCommBuffMemRegion.PhysicalBase,
                  mNsCommBuffMemRegion.Length,
                  EFI_MEMORY_WB | EFI_MEMORY_XP | EFI_MEMORY_RUNTIME
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "MmCommunicateInitialize: "
      "Failed to set MM-NS Buffer Memory attributes\n"
      ));
    goto CleanAddedMemorySpace;
  }

  // Install the communication protocol
  Status = gBS->InstallProtocolInterface (
                  &mMmCommunicateHandle,
                  &gEfiMmCommunication2ProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mMmCommunication2
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "MmCommunicationInitialize: "
      "Failed to install MM communication protocol\n"
      ));
    goto CleanAddedMemorySpace;
  }

  // Register notification callback when virtual address is associated
  // with the physical address.
  // Create a Set Virtual Address Map event.
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                  TPL_NOTIFY,
                  NotifySetVirtualAddressMap,
                  NULL,
                  &mSetVirtualAddressMapEvent
                  );
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < ARRAY_SIZE (mGuidedEventGuid); Index++) {
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    MmGuidedEventNotify,
                    mGuidedEventGuid[Index],
                    mGuidedEventGuid[Index],
                    &mGuidedEvent[Index]
                    );
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      while (Index-- > 0) {
        gBS->CloseEvent (mGuidedEvent[Index]);
      }

      goto UninstallProtocol;
    }
  }

  RtProperties = (EFI_RT_PROPERTIES_TABLE *)AllocatePool (sizeof (EFI_RT_PROPERTIES_TABLE));
  if (RtProperties == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate RT table\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto UninstallProtocol;
  }

  RtProperties->Version                  = EFI_RT_PROPERTIES_TABLE_VERSION;
  RtProperties->Length                   = sizeof (EFI_RT_PROPERTIES_TABLE);
  RtProperties->RuntimeServicesSupported = PcdGet32 (PcdVariableRtProperties);

  Status = gBS->InstallConfigurationTable (&gEfiRtPropertiesTableGuid, RtProperties);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing RT table: %r\n", __FUNCTION__, Status));
    goto UninstallProtocol;
  }

  return EFI_SUCCESS;

UninstallProtocol:
  gBS->UninstallProtocolInterface (
         mMmCommunicateHandle,
         &gEfiMmCommunication2ProtocolGuid,
         &mMmCommunication2
         );

CleanAddedMemorySpace:
  if (!OpteePresent) {
    gDS->RemoveMemorySpace (
           mNsCommBuffMemRegion.PhysicalBase,
           mNsCommBuffMemRegion.Length
           );
  }

ReturnErrorStatus:
  return EFI_INVALID_PARAMETER;
}
