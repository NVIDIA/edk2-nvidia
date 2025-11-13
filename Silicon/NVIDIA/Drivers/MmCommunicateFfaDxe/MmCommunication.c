/** @file

  SPDX-FileCopyrightText: Copyright (c) 2022-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2016-2021, Arm Limited. All rights reserved.<BR>

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
#include <Library/TimerLib.h>
#include <Guid/RtPropertiesTable.h>

#include <Protocol/MmCommunication2.h>

#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/ArmStdSmc.h>

#include "MmCommunicateFfa.h"

#define STMM_UUID_0                        0x8afb129b
#define STMM_UUID_1                        0x64ca4760
#define STMM_UUID_2                        0x8618c888
#define STMM_UUID_3                        0x4caa6c4a
#define ARM_SVC_ID_FFA_PARTITION_INFO_GET  0x84000068
#define ARM_SVC_ID_FFA_RXTX_MAP            0xC4000066
#define ARM_SVC_ID_FFA_RXTX_UNMAP          0x84000067
#define ARM_SVC_ID_FFA_SUCCESS_AARCH64     0xC4000061
#define ARM_SVC_ID_FFA_SUCCESS_AARCH32     0x84000060
#define STMM_GET_NS_BUFFER                 0xC0270001
#define STMM_GET_ERST_UNCACHED_BUFFER      0xC0270002
#define STMM_GET_PRM0_BUFFER               0xC0270004
#define ARM_SVC_ID_FFA_RX_RELEASE          0x84000065

STATIC UINT16  StmmVmId = 0xFFFF;
STATIC EFI_STATUS
GetStmmVmId (
  VOID
  );

STATIC EFI_STATUS
GetNsBufferAddr (
  VOID
  );

STATIC
EFI_STATUS
GetErstBufferAddr (
  VOID
  );

STATIC EFI_STATUS
GetPrmBufferAddr (
  VOID
  );

//
// Address, Length of the pre-allocated buffer for communication with the secure
// world.
//
STATIC ARM_MEMORY_REGION_DESCRIPTOR  mNsCommBuffMemRegion;
STATIC ARM_MEMORY_REGION_DESCRIPTOR  mPrmCommBuffMemRegion;

// Notification event when virtual address map is set.
STATIC EFI_EVENT  mSetVirtualAddressMapEvent;

//
// Handle to install the MM Communication Protocol
//
STATIC EFI_HANDLE  mMmCommunicateHandle;
STATIC EFI_HANDLE  mMmPrmCommunicateHandle;

/**
 * Wrapper function to send an FF-A Direct Message, this
 * includes retry mechanism.

  @param[in, out] Args ARM SMC Args that caller has setup.

  @return EFI_SUCCESS        On Success.
          EFI_ACCESS_DENIED  Secure World didn't respond favorably.
 **/
STATIC
EFI_STATUS
SendFfaDirectReqStMm (
  ARM_SMC_ARGS  *Args
  )
{
  UINT8         MaxRetries;
  UINT64        BackOffTimeUsec;
  UINTN         Index;
  EFI_STATUS    Status;
  ARM_SMC_ARGS  LocalArgs;

  ZeroMem (&LocalArgs, sizeof (ARM_SMC_ARGS));
  CopyMem ((VOID *)&LocalArgs, (VOID *)Args, sizeof (ARM_SMC_ARGS));
  Status          = EFI_ACCESS_DENIED;
  MaxRetries      = PcdGet8 (PcdMmCommMaxRetries);
  BackOffTimeUsec = PcdGet64 (PcdMmCommRetryBackOffUs);

  StmmFfaSmc (&LocalArgs);

  if (LocalArgs.Arg0 != ARM_FID_FFA_MSG_SEND_DIRECT_RESP) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid Response Arg0:0x%x, Arg1:0x%x, Arg2:0x%x, Arg3:0x%x\n",
      __FUNCTION__,
      LocalArgs.Arg0,
      LocalArgs.Arg1,
      LocalArgs.Arg2,
      LocalArgs.Arg3
      ));
    for (Index = 0; Index < MaxRetries; Index++) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Sleep %lu us before Retry %u of %u\n",
        __FUNCTION__,
        BackOffTimeUsec,
        (Index + 1),
        MaxRetries
        ));
      MicroSecondDelay (BackOffTimeUsec);
      ZeroMem (&LocalArgs, sizeof (ARM_SMC_ARGS));
      CopyMem ((VOID *)&LocalArgs, (VOID *)Args, sizeof (ARM_SMC_ARGS));
      StmmFfaSmc (&LocalArgs);

      if (LocalArgs.Arg0 != ARM_FID_FFA_MSG_SEND_DIRECT_RESP) {
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
        Status = EFI_SUCCESS;
        break;
      }
    }
  } else {
    Status = EFI_SUCCESS;
  }

  CopyMem ((VOID *)Args, (VOID *)&LocalArgs, sizeof (ARM_SMC_ARGS));
  return Status;
}

/**
  Communicates with a registered handler.

  This function provides a service to send and receive messages from a registered UEFI service.

  @param[in] This                     The EFI_MM_COMMUNICATION_PROTOCOL instance.
  @param[in] CommBuffMemRegion        The ARM_MEMORY_REGION_DESCRIPTOR of the MM communication buffer.
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
STATIC
EFI_STATUS
EFIAPI
MmCommunication2Communicate (
  IN CONST EFI_MM_COMMUNICATION2_PROTOCOL  *This,
  IN ARM_MEMORY_REGION_DESCRIPTOR          *CommBuffMemRegion,
  IN OUT VOID                              *CommBufferPhysical,
  IN OUT VOID                              *CommBufferVirtual,
  IN OUT UINTN                             *CommSize OPTIONAL
  )
{
  EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader;
  ARM_SMC_ARGS               CommunicateSmcArgs;
  EFI_STATUS                 Status;
  UINTN                      BufferSize;
  UINTN                      Ret;

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
        (*CommSize > CommBuffMemRegion->Length))
    {
      *CommSize = CommBuffMemRegion->Length;
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
      (BufferSize > CommBuffMemRegion->Length))
  {
    CommunicateHeader->MessageLength = CommBuffMemRegion->Length -
                                       sizeof (CommunicateHeader->HeaderGuid) -
                                       sizeof (CommunicateHeader->MessageLength);
    Status = EFI_BAD_BUFFER_SIZE;
  }

  // MessageLength or CommSize check has failed, return here.
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Copy Communication Payload
  CopyMem ((VOID *)CommBuffMemRegion->VirtualBase, CommBufferVirtual, BufferSize);

  // FF-A Interface ID for direct message communication
  CommunicateSmcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ;

  // FF-A Destination EndPoint ID, not used as of now
  CommunicateSmcArgs.Arg1 = StmmVmId;

  // Reserved for future use(MBZ)
  CommunicateSmcArgs.Arg2 = 0x0;

  // Arg3 onwards are the IMPLEMENTATION DEFINED FF-A parameters
  // SMC Function ID
  CommunicateSmcArgs.Arg3 = ARM_SMC_ID_MM_COMMUNICATE_AARCH64;

  // Cookie
  CommunicateSmcArgs.Arg4 = 0x0;

  // comm_buffer_address (64-bit physical address)
  CommunicateSmcArgs.Arg5 = (UINTN)CommBuffMemRegion->PhysicalBase;

  // comm_size_address (not used, indicated by setting to zero)
  CommunicateSmcArgs.Arg6 = 0;

  Status = SendFfaDirectReqStMm (&CommunicateSmcArgs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FF-A Direct Msg Failed %r\n", __FUNCTION__, Status));
    goto ExitMmCommunication2Communicate;
  }

  Ret = CommunicateSmcArgs.Arg2;

  switch (Ret) {
    case ARM_SMC_MM_RET_SUCCESS:
      ZeroMem (CommBufferVirtual, BufferSize);
      // On successful return, the size of data being returned is inferred from
      // MessageLength + Header.
      CommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)CommBuffMemRegion->VirtualBase;
      BufferSize        = CommunicateHeader->MessageLength +
                          sizeof (CommunicateHeader->HeaderGuid) +
                          sizeof (CommunicateHeader->MessageLength);

      CopyMem (
        CommBufferVirtual,
        (VOID *)CommBuffMemRegion->VirtualBase,
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

ExitMmCommunication2Communicate:
  return Status;
}

/**
  Communicates via MM NS memory buffer.

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
**/
STATIC
EFI_STATUS
EFIAPI
NsMmCommunicate (
  IN CONST EFI_MM_COMMUNICATION2_PROTOCOL  *This,
  IN OUT VOID                              *CommBufferPhysical,
  IN OUT VOID                              *CommBufferVirtual,
  IN OUT UINTN                             *CommSize OPTIONAL
  )
{
  return MmCommunication2Communicate (
           This,
           &mNsCommBuffMemRegion,
           CommBufferPhysical,
           CommBufferVirtual,
           CommSize
           );
}

/**
  Communicates via MM PRM memory buffer.

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
**/
STATIC
EFI_STATUS
EFIAPI
PrmMmCommunicate (
  IN CONST EFI_MM_COMMUNICATION2_PROTOCOL  *This,
  IN OUT VOID                              *CommBufferPhysical,
  IN OUT VOID                              *CommBufferVirtual,
  IN OUT UINTN                             *CommSize OPTIONAL
  )
{
  return MmCommunication2Communicate (
           This,
           &mPrmCommBuffMemRegion,
           CommBufferPhysical,
           CommBufferVirtual,
           CommSize
           );
}

//
// MM Communication Protocol instance
//
STATIC EFI_MM_COMMUNICATION2_PROTOCOL  mMmCommunication2 = {
  NsMmCommunicate
};

STATIC EFI_MM_COMMUNICATION2_PROTOCOL  mMmPrmCommunication2 = {
  PrmMmCommunicate
};

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

  if (mPrmCommBuffMemRegion.VirtualBase != 0) {
    Status = gRT->ConvertPointer (
                    EFI_OPTIONAL_PTR,
                    (VOID **)&mPrmCommBuffMemRegion.VirtualBase
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "NotifySetVirtualAddressMap():"
        " Unable to convert PRM runtime pointer. Status:0x%r\n",
        Status
        ));
    }
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

  if (PcdGetBool (PcdTegraStmmEnabled) == FALSE) {
    DEBUG ((DEBUG_INFO, "PCD to Enable MM set to False\n"));
    return EFI_UNSUPPORTED;
  }

  MmVersionArgs.Arg0  = ARM_FID_FFA_VERSION;
  MmVersionArgs.Arg1  = MM_CALLER_MAJOR_VER << MM_MAJOR_VER_SHIFT;
  MmVersionArgs.Arg1 |= MM_CALLER_MINOR_VER;

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

    Status = GetStmmVmId ();
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get Stmm Partition Info %r\n",
        __FUNCTION__,
        Status
        ));
      Status = EFI_UNSUPPORTED;
      goto ExitGetMmCompatibility;
    }

    Status = GetNsBufferAddr ();
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get NS Buffer Details. %r\n",
        __FUNCTION__,
        Status
        ));
      Status = EFI_UNSUPPORTED;
      goto ExitGetMmCompatibility;
    }

    Status = GetErstBufferAddr ();
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get ERST Buffer Details. %r\n",
        __FUNCTION__,
        Status
        ));
    }

    Status = GetPrmBufferAddr ();
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get PRM Buffer Details. %r\n",
        __FUNCTION__,
        Status
        ));
      // Do not stop boot because of the absence of PRM service
      Status = EFI_SUCCESS;
    }
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

ExitGetMmCompatibility:
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
  NsMmCommunicate (&mMmCommunication2, &Header, &Header, &Size);
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
  EFI_STATUS               Status;
  UINTN                    Index;
  EFI_RT_PROPERTIES_TABLE  *RtProperties;

  // Check if we can make the MM call
  Status = GetMmCompatibility ();
  if (EFI_ERROR (Status)) {
    goto ReturnErrorStatus;
  }

  mNsCommBuffMemRegion.PhysicalBase =  PcdGet64 (PcdMmBufferBase);
  // During boot , Virtual and Physical are same
  mNsCommBuffMemRegion.VirtualBase = mNsCommBuffMemRegion.PhysicalBase;
  mNsCommBuffMemRegion.Length      = PcdGet64 (PcdMmBufferSize);

  ASSERT (mNsCommBuffMemRegion.PhysicalBase != 0);

  ASSERT (mNsCommBuffMemRegion.Length != 0);

  Status = gDS->SetMemorySpaceAttributes (
                  mNsCommBuffMemRegion.PhysicalBase,
                  mNsCommBuffMemRegion.Length,
                  EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "MmCommunicateInitialize: "
      "Failed to set MM-NS Buffer Memory attributes\n"
      ));
    goto ReturnErrorStatus;
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
    goto ReturnErrorStatus;
  }

  // Only install the protocol when the region length is valid
  if (mPrmCommBuffMemRegion.Length != 0) {
    Status = gDS->SetMemorySpaceAttributes (
                    mPrmCommBuffMemRegion.PhysicalBase,
                    mPrmCommBuffMemRegion.Length,
                    EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "MmCommunicateInitialize: "
        "Failed to set MM-PRM Buffer Memory attributes\n"
        ));
    } else {
      Status = gBS->InstallProtocolInterface (
                      &mMmPrmCommunicateHandle,
                      &gNVIDIAMmPrmCommunication2ProtocolGuid,
                      EFI_NATIVE_INTERFACE,
                      &mMmPrmCommunication2
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "MmCommunicationInitialize: "
          "Failed to install MM-PRM communication protocol\n"
          ));
      }
    }
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

  RtProperties = (EFI_RT_PROPERTIES_TABLE *)AllocateRuntimePool (sizeof (EFI_RT_PROPERTIES_TABLE));
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

  gBS->UninstallProtocolInterface (
         mMmPrmCommunicateHandle,
         &gNVIDIAMmPrmCommunication2ProtocolGuid,
         &mMmPrmCommunication2
         );

ReturnErrorStatus:
  return EFI_INVALID_PARAMETER;
}

/**
 * Allocate RX/TX buffers for FFA communication.
 * Note: the RX/TX buffers are shared for the entire NS world, so they must be
 * freed after use.
 *
 * @param[in]  pages           Number of pages for both buffers
 * @param[out] rx              Address of the RX buffer
 * @param[out] tx              Address of the TX buffer
 *
 * @return EFI_SUCCESS         Buffers allocated and mapped with Hafnium
 *
**/
STATIC
EFI_STATUS
FfaAllocateAndMapRxTxBuffers (
  IN  UINTN             pages,
  OUT PHYSICAL_ADDRESS  *rx,
  OUT PHYSICAL_ADDRESS  *tx
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData, pages, rx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RX buffer allocation failed\n", __FUNCTION__));
    goto out;
  }

  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData, pages, tx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TX buffer allocation failed\n", __FUNCTION__));
    goto out_rx;
  }

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RXTX_MAP;
  ArmSmcArgs.Arg1 = (UINTN)*tx;
  ArmSmcArgs.Arg2 = (UINTN)*rx;
  ArmSmcArgs.Arg3 = pages;

  ArmCallSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RXTX_MAP failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
    goto out_tx;
  }

  goto out;

out_tx:
  gBS->FreePages (*tx, pages);
out_rx:
  gBS->FreePages (*rx, pages);
out:
  return Status;
}

/**
 * Unmap RX/TX buffers and free them.
 *
 * @param[in]  pages           Number of pages for both buffers
 * @param[in]  rx              Address of the RX buffer
 * @param[in]  tx              Address of the TX buffer
 *
 * @return EFI_SUCCESS         Buffers unmapped and freed.
 *
**/
STATIC
EFI_STATUS
FfaFreeRxTxBuffers (
  IN  UINTN             pages,
  IN  PHYSICAL_ADDRESS  rx,
  IN  PHYSICAL_ADDRESS  tx
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RXTX_UNMAP;
  ArmSmcArgs.Arg1 = 0; /* NS World */

  ArmCallSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RXTX_UNMAP failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
  }

  gBS->FreePages (tx, pages);
  gBS->FreePages (rx, pages);

  return Status;
}

STATIC
EFI_STATUS
GetBufferAddr (
  UINT32  BufferId,
  UINT64  *BufferBase,
  UINT64  *BufferSize
  )
{
  EFI_STATUS    Status;
  ARM_SMC_ARGS  ArmSmcArgs;

  if (StmmVmId ==  0xFFFF) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));
  Status = EFI_SUCCESS;

  ArmSmcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ;
  ArmSmcArgs.Arg1 = StmmVmId;
  ArmSmcArgs.Arg3 = BufferId;

  Status = SendFfaDirectReqStMm (&ArmSmcArgs);

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid FFA response: %r\n",
      __FUNCTION__,
      Status
      ));
    goto exit;
  }

  *BufferBase = ArmSmcArgs.Arg5;
  *BufferSize = ArmSmcArgs.Arg6;

exit:
  return Status;
}

STATIC
EFI_STATUS
GetNsBufferAddr (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT64      NsBufferBase;
  UINT64      NsBufferSize;

  Status = GetBufferAddr (STMM_GET_NS_BUFFER, &NsBufferBase, &NsBufferSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get NS Buffer Details. %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  PcdSet64S (PcdMmBufferBase, NsBufferBase);
  PcdSet64S (PcdMmBufferSize, NsBufferSize);

  DEBUG ((
    DEBUG_ERROR,
    "%a: Set NsBufferBase to 0x%lx Size %lu\n",
    __FUNCTION__,
    NsBufferBase,
    NsBufferSize
    ));

  return Status;
}

STATIC
EFI_STATUS
GetErstBufferAddr (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT64      ErstBufferBase;
  UINT64      ErstBufferSize;

  Status = GetBufferAddr (STMM_GET_ERST_UNCACHED_BUFFER, &ErstBufferBase, &ErstBufferSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Erst Buffer Details. %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  PcdSet64S (PcdErstBufferBase, ErstBufferBase);
  PcdSet64S (PcdErstBufferSize, ErstBufferSize);

  DEBUG ((
    DEBUG_ERROR,
    "%a: Set ErstBufferBase to %llx Size %llx\n",
    __FUNCTION__,
    ErstBufferBase,
    ErstBufferSize
    ));

  return Status;
}

STATIC
EFI_STATUS
GetPrmBufferAddr (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT64      PrmBufferBase;
  UINT64      PrmBufferSize;

  Status = GetBufferAddr (STMM_GET_PRM0_BUFFER, &PrmBufferBase, &PrmBufferSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get PRM Buffer Details. %r\n",
      __FUNCTION__,
      Status
      ));

    mPrmCommBuffMemRegion.PhysicalBase = 0;
    mPrmCommBuffMemRegion.VirtualBase  = 0;
    mPrmCommBuffMemRegion.Length       = 0;
    return Status;
  }

  mPrmCommBuffMemRegion.PhysicalBase =  PrmBufferBase;
  // During boot , Virtual and Physical are same
  mPrmCommBuffMemRegion.VirtualBase = mPrmCommBuffMemRegion.PhysicalBase;
  mPrmCommBuffMemRegion.Length      = PrmBufferSize;

  DEBUG ((
    DEBUG_ERROR,
    "%a: Set PrmBufferBase to 0x%lx Size %lu\n",
    __FUNCTION__,
    PrmBufferBase,
    PrmBufferSize
    ));

  return Status;
}

STATIC
EFI_STATUS
FfaReleaseRxBuffer (
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RX_RELEASE;
  ArmSmcArgs.Arg1 = 0; /* NS World */

  ArmCallSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RX_RELEASE failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
  }

  return Status;
}

STATIC
EFI_STATUS
GetStmmVmId (
  VOID
  )
{
  PHYSICAL_ADDRESS  rx, tx;
  UINTN             pages  = 1;
  EFI_STATUS        Status = EFI_SUCCESS;

  Status = FfaAllocateAndMapRxTxBuffers (pages, &rx, &tx);
  if (EFI_ERROR (Status)) {
    return EFI_OUT_OF_RESOURCES;
  }

  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_PARTITION_INFO_GET;
  ArmSmcArgs.Arg1 = STMM_UUID_0;
  ArmSmcArgs.Arg2 = STMM_UUID_1;
  ArmSmcArgs.Arg3 = STMM_UUID_2;
  ArmSmcArgs.Arg4 = STMM_UUID_3;

  ArmCallSmc (&ArmSmcArgs);

  /* One SP should have been found */
  if (ArmSmcArgs.Arg2 != 1) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_PARTITION_INFO_GET failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = EFI_NOT_FOUND;
    goto exit;
  }

  StmmVmId = *((UINT16 *)rx);
  DEBUG ((DEBUG_ERROR, "%a: STMM VmId=0x%x\n", __FUNCTION__, StmmVmId));

exit:
  FfaReleaseRxBuffer ();
  FfaFreeRxTxBuffers (pages, rx, tx);
  return Status;
}
