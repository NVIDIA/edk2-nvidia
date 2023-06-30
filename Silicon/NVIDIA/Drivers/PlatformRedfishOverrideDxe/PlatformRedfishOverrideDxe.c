/** @file
  Platform driver to provide Redfish override protocol.

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PlatformRedfishOverrideDxe.h"

/**
  The callback function to notify platform and provide Redfish phase.

  @param[in] This           Pointer to EDKII_REDFISH_OVERRIDE_PROTOCOL instance.
  @param[in] PhaseType      The type of phase in Redfish operation.

  @retval EFI_SUCCESS       The notify function completed successfully.
  @retval Others            Some errors happened.

**/
EFI_STATUS
PlatformRedfishNotifyPhase (
  IN EDKII_REDFISH_OVERRIDE_PROTOCOL  *This,
  IN EDKII_REDFISH_PHASE_TYPE         PhaseType
  )
{
  switch (PhaseType) {
    case EdkiiRedfishPhaseBeforeReboot:
      //
      // Mark existing boot chain as good.
      //
      ValidateActiveBootChain ();
      DEBUG ((DEBUG_INFO, "%a: validate active boot chain\n", __func__));
      break;
    default:
      return EFI_SUCCESS;
  }

  return EFI_SUCCESS;
}

EDKII_REDFISH_OVERRIDE_PROTOCOL  mRedfishOverrideProtocol = {
  EDKII_REDFISH_OVERRIDE_PROTOCOL_REVISION,
  PlatformRedfishNotifyPhase
};

/**
  Main entry for this driver.

  @param[in] ImageHandle     Image handle this driver.
  @param[in] SystemTable     Pointer to SystemTable.

  @retval EFI_SUCCESS     This function always complete successfully.

**/
EFI_STATUS
EFIAPI
PlatformRedfishOverrideDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEdkiiRedfishOverrideProtocolGuid,
                  &mRedfishOverrideProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to install Redfish override protocol: %r\n", __func__, Status));
  }

  return EFI_SUCCESS;
}
