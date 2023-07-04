/** @file

  Copyright (c) 2022 - 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved. <BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HashLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/Tpm2CommandLib.h>

#include <Guid/TpmInstance.h>
#include <IndustryStandard/TpmPtp.h>

#include "Tpm2DeviceLibInternal.h"

STATIC VOID                  *mSearchToken = NULL;
STATIC NVIDIA_TPM2_PROTOCOL  *mTpm2        = NULL;
STATIC EFI_EVENT             mTpm2Event    = NULL;

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
Tpm2SubmitCommandInternal (
  IN UINT32      InputParameterBlockSize,
  IN UINT8       *InputParameterBlock,
  IN OUT UINT32  *OutputParameterBlockSize,
  IN UINT8       *OutputParameterBlock
  )
{
  if (mTpm2 == NULL) {
    return EFI_DEVICE_ERROR;
  }

  return TisTpmCommand (
           mTpm2,
           InputParameterBlock,
           InputParameterBlockSize,
           OutputParameterBlock,
           OutputParameterBlockSize
           );
}

/**
  This service requests use TPM2.

  @retval EFI_SUCCESS      Get the control of TPM2 chip.
  @retval EFI_NOT_FOUND    TPM2 not found.
  @retval EFI_DEVICE_ERROR Unexpected device behavior.
**/
EFI_STATUS
EFIAPI
Tpm2RequestUseTpmInternal (
  VOID
  )
{
  if (mTpm2 == NULL) {
    return EFI_DEVICE_ERROR;
  }

  return TisRequestUseTpm (mTpm2);
}

TPM2_DEVICE_INTERFACE  mInternalTpm2Device = {
  TPM_DEVICE_INTERFACE_TPM20_DTPM,
  Tpm2SubmitCommandInternal,
  Tpm2RequestUseTpmInternal,
};

/**
  Initialize TPM

  @retval EFI_SUCCESS      TPM initialized successfully.
  @retval EFI_DEVICE_ERROR Unexpected device behavior.
**/
STATIC
EFI_STATUS
Tpm2Initialize (
  VOID
  )
{
  EFI_STATUS          Status;
  UINT32              TpmHashAlgorithmBitmap;
  UINT32              ActivePCRBanks;
  UINT32              Pcr;
  UINT32              Event;
  TPML_DIGEST_VALUES  DigestList;

  Status = Tpm2RequestUseTpmInternal ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to request to use TPM.\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  if (PcdGet8 (PcdTpm2InitializationPolicy) == 1) {
    DEBUG ((DEBUG_INFO, "%a: TPM Startup STATE\n", __FUNCTION__));
    Status = Tpm2Startup (TPM_SU_STATE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: TPM Startup STATE failed - %r\n", __FUNCTION__, Status));
      DEBUG ((DEBUG_INFO, "%a: TPM Startup CLEAR\n", __FUNCTION__));
      Status = Tpm2Startup (TPM_SU_CLEAR);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: TPM Startup CLEAR failed - %r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }
    }
  }

  //
  // If PcdTpm2InitializationPolicy=0, TPM is assumed to be started by pre-UEFI images.
  // If TPM is not accessible here, it has not been started successfully, possibly TPM is
  // disabled in the fuse, or TPM is disabled or not present on the platform,...
  //
  Status = Tpm2GetCapabilitySupportedAndActivePcrs (&TpmHashAlgorithmBitmap, &ActivePCRBanks);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TPM has not been started successfully.\n", __FUNCTION__));
    return Status;
  }

  //
  // Select hash algorithm based on active PCR bank
  //
  if ((ActivePCRBanks & TPM_ALG_SHA384) != 0) {
    PcdSet32S (PcdTpm2HashMask, 0x00000004);
  } else if ((ActivePCRBanks & TPM_ALG_SHA256) != 0) {
    PcdSet32S (PcdTpm2HashMask, 0x00000002);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Unsupported PCR banks - %x\n", __FUNCTION__, ActivePCRBanks));
    ASSERT (FALSE);
  }

  PcdSet32S (PcdTcg2HashAlgorithmBitmap, 0x00000006);

  //
  // If TPM is disabled by users, lock down the TPM and remove the driver
  //
  if (!PcdGetBool (PcdTpmEnable)) {
    //
    // Disable Storage and Endorsement hierarchies
    //
    Status = Tpm2HierarchyControl (TPM_RH_PLATFORM, NULL, TPM_RH_OWNER, NO);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Disable Owner Hierarchy Failed! %r\n", __FUNCTION__, Status));
    }

    Status = Tpm2HierarchyControl (TPM_RH_PLATFORM, NULL, TPM_RH_ENDORSEMENT, NO);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Disable Endorsement Hierarchy Failed! %r\n", __FUNCTION__, Status));
    }

    //
    // Cap PCRs 0-7 by extending EV_SEPARATOR to them
    //
    Event = 0;
    for (Pcr = 0; Pcr < 8; Pcr++) {
      Status = HashAndExtend (Pcr, (UINT8 *)&Event, sizeof (Event), &DigestList);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Fail to extend EV_SEPARATOR to PCR%u - %r\n", __FUNCTION__, Pcr, Status));
      }
    }

    //
    // Disable Platform hierarchy
    //
    Status = Tpm2HierarchyControl (TPM_RH_PLATFORM, NULL, TPM_RH_PLATFORM, NO);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Disable Platform Hierarchy Failed! %r\n", __FUNCTION__, Status));
    }

    //
    // Relinqish TPM locality to allow TPM to enter low power state
    //
    TisReleaseTpm (mTpm2);

    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Get TPM2 protocol

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
STATIC
VOID
EFIAPI
Tpm2RegistrationEvent (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_HANDLE            *Handles = NULL;
  UINTN                 NumHandles;
  EFI_STATUS            Status;
  NVIDIA_TPM2_PROTOCOL  *Tpm2;

  //
  // Check the list of handles that support TPM
  //
  Status = gBS->LocateHandleBuffer (
                  ByRegisterNotify,
                  &gNVIDIATpm2ProtocolGuid,
                  mSearchToken,
                  &NumHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  if (NumHandles > 1) {
    DEBUG ((DEBUG_ERROR, "%a: Only support one TPM. But there are %u TPMs present.\n", __FUNCTION__, NumHandles));
    ASSERT (FALSE);
  }

  if (NumHandles > 0) {
    Status = gBS->HandleProtocol (
                    Handles[0],
                    &gNVIDIATpm2ProtocolGuid,
                    (VOID **)&Tpm2
                    );
    if (EFI_ERROR (Status)) {
      //
      // Disable if fail to obtain the protocol
      //
      DEBUG ((DEBUG_ERROR, "%a: Fail to handle TPM protocol.\n", __FUNCTION__));
      mTpm2 = NULL;
      goto Exit;
    }

    if ((mTpm2 != NULL) && (mTpm2 != Tpm2)) {
      DEBUG ((DEBUG_WARN, "%a: TPM protocol reinstalled.\n", __FUNCTION__));
    }

    mTpm2 = Tpm2;

    Status = Tpm2Initialize ();
    if (EFI_ERROR (Status)) {
      //
      // Disable if fail to initialize TPM
      //
      DEBUG ((DEBUG_ERROR, "%a: Disable TPM driver.\n", __FUNCTION__));
      mTpm2 = NULL;
    }
  } else {
    //
    // Disable if the protocol had been uninstalled
    //
    mTpm2 = NULL;
  }

Exit:
  FreePool (Handles);
}

/**
  Destructor for TPM2 device library.

  @retval EFI_SUCCESS on Success.
 **/
EFI_STATUS
EFIAPI
Tpm2DeviceLibDestructor (
  VOID
  )
{
  gBS->CloseEvent (mTpm2Event);

  return EFI_SUCCESS;
}

/**
  Constructor for TPM2 device library.

  @retval EFI_SUCCESS           The operation completed successfully.
**/
EFI_STATUS
EFIAPI
Tpm2DeviceLibConstructor (
  VOID
  )
{
  EFI_STATUS  Status;

  //
  // Check if TPM driver is wanted by platform, if not, exit early.
  //
  Status = Tpm2RegisterTpm2DeviceLib (&mInternalTpm2Device);
  if (EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  //
  // Only support TIS
  //
  PcdSet8S (PcdActiveTpmInterfaceType, Tpm2PtpInterfaceTis);

  //
  // Register a protocol registration notification callback on the TPM2
  // protocol. This will notify us even if the protocol instance we are looking
  // for has already been installed or reinstalled.
  //
  mTpm2Event = EfiCreateProtocolNotifyEvent (
                 &gNVIDIATpm2ProtocolGuid,
                 TPL_CALLBACK,
                 Tpm2RegistrationEvent,
                 NULL,
                 &mSearchToken
                 );
  if (mTpm2Event == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to create protocol event\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}
