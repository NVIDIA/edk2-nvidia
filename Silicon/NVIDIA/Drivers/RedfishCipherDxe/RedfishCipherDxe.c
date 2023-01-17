/** @file
  Install TLS v1.2 cipher suites.

  This driver listen to Redfish ready-to-provision event and install TLS
  cipher suites if it does not exist.

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishCipherDxe.h"

EFI_EVENT  mEvent = NULL;

EFI_TLS_CIPHER  mTlsHttpsCipher[] = {
  TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
  TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
  TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
  TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
};

/**
  Callback function executed when the ready-to-provisioning event group is signaled.
  @param[in]   Event    Event whose notification function is being invoked.
  @param[out]  Context  Pointer to the Context buffer
**/
VOID
EFIAPI
RedfishReadyToProvisioning (
  IN  EFI_EVENT  Event,
  OUT VOID       *Context
  )
{
  EFI_STATUS  Status;
  UINTN       CipherListSize;

  //
  // Install TLS Cipher Suites that BMC accepted
  //

  //
  // Try to read the HttpTlsCipherList variable.
  //
  Status = gRT->GetVariable (
                  EDKII_HTTP_TLS_CIPHER_LIST_VARIABLE,
                  &gEdkiiHttpTlsCipherListGuid,
                  NULL,
                  &CipherListSize,
                  NULL
                  );
  if (Status == EFI_NOT_FOUND) {
    //
    // Create TLS cipher suits.
    //
    Status = gRT->SetVariable (
                    EDKII_HTTP_TLS_CIPHER_LIST_VARIABLE, // VariableName
                    &gEdkiiHttpTlsCipherListGuid,        // VendorGuid
                    EFI_VARIABLE_BOOTSERVICE_ACCESS,     // Attributes
                    sizeof (mTlsHttpsCipher),            // DataSize
                    (VOID *)mTlsHttpsCipher              // Data
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to set %s variable: %r\n", __FUNCTION__, EDKII_HTTP_TLS_CIPHER_LIST_VARIABLE, Status));
      return;
    }

    DEBUG ((DEBUG_INFO, "%a: %s created\n", __FUNCTION__, EDKII_HTTP_TLS_CIPHER_LIST_VARIABLE));
  }

  //
  // Close event
  //
  gBS->CloseEvent (Event);
  mEvent = NULL;
}

/**
  Unloads an image.

  @param  ImageHandle           Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
  @retval EFI_INVALID_PARAMETER ImageHandle is not a valid image handle.

**/
EFI_STATUS
EFIAPI
RedfishCipherUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  if (mEvent != NULL) {
    gBS->CloseEvent (mEvent);
    mEvent = NULL;
  }

  return EFI_SUCCESS;
}

/**
  The entry point for platform redfish BIOS driver which installs the Redfish Resource
  Addendum protocol on its ImageHandle.

  @param[in]  ImageHandle        The image handle of the driver.
  @param[in]  SystemTable        The system table.

  @retval EFI_SUCCESS            Protocol install successfully.
  @retval Others                 Failed to install the protocol.

**/
EFI_STATUS
EFIAPI
RedfishCipherEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Register read-to-provisioning event
  //
  Status = CreateReadyToProvisioningEvent (
             RedfishReadyToProvisioning,
             NULL,
             &mEvent
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to register ready-to-provisioning event: %r\n", __FUNCTION__, Status));
  }

  return EFI_SUCCESS;
}
