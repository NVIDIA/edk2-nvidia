/** @file
  Redfish feature driver - chassis information collector

  (C) Copyright 2020-2022 Hewlett Packard Enterprise Development LP<BR>
  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Common/RedfishChassisInfoCollectorCommon.h"

extern REDFISH_RESOURCE_COMMON_PRIVATE  *mRedfishResourcePrivate;

/**
  Initialize a Redfish configure handler.

  This function will be called by the Redfish config driver to initialize each Redfish configure
  handler.

  @param[in]   This                     Pointer to EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL instance.
  @param[in]   RedfishConfigServiceInfo Redfish service information.

  @retval EFI_SUCCESS                  The handler has been initialized successfully.
  @retval EFI_DEVICE_ERROR             Failed to create or configure the REST EX protocol instance.
  @retval EFI_ALREADY_STARTED          This handler has already been initialized.
  @retval Other                        Error happens during the initialization.

**/
EFI_STATUS
EFIAPI
RedfishResourceInit (
  IN  EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL  *This,
  IN  REDFISH_CONFIG_SERVICE_INFORMATION     *RedfishConfigServiceInfo
  )
{
  REDFISH_RESOURCE_COMMON_PRIVATE  *Private;

  Private = REDFISH_RESOURCE_COMMON_PRIVATE_DATA_FROM_CONFIG_PROTOCOL (This);

  Private->RedfishService = RedfishCreateService (RedfishConfigServiceInfo);
  if (Private->RedfishService == NULL) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Stop a Redfish configure handler.

  @param[in]   This                Pointer to EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL instance.

  @retval EFI_SUCCESS              This handler has been stoped successfully.
  @retval Others                   Some error happened.

**/
EFI_STATUS
EFIAPI
RedfishResourceStop (
  IN  EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL  *This
  )
{
  REDFISH_RESOURCE_COMMON_PRIVATE  *Private;

  Private = REDFISH_RESOURCE_COMMON_PRIVATE_DATA_FROM_CONFIG_PROTOCOL (This);

  if (Private->Event != NULL) {
    gBS->CloseEvent (Private->Event);
    Private->Event = NULL;
  }

  if (Private->RedfishService != NULL) {
    RedfishCleanupService (Private->RedfishService);
    Private->RedfishService = NULL;
  }

  if (Private->Payload != NULL) {
    RedfishCleanupPayload (Private->Payload);
    Private->Payload = NULL;
  }

  return EFI_SUCCESS;
}

EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL  mRedfishConfigHandler = {
  RedfishResourceInit,
  RedfishResourceStop
};

/**
  Unloads an image.

  @param  ImageHandle           Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
  @retval EFI_INVALID_PARAMETER ImageHandle is not a valid image handle.

**/
EFI_STATUS
EFIAPI
RedfishResourceUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                             Status;
  EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL  *ConfigHandler;

  if (mRedfishResourcePrivate == NULL) {
    return EFI_NOT_READY;
  }

  ConfigHandler = NULL;

  //
  // Firstly, find ConfigHandler Protocol interface in this ImageHandle.
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEdkIIRedfishConfigHandlerProtocolGuid,
                  (VOID **)&ConfigHandler,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
                  );
  if (EFI_ERROR (Status) || (ConfigHandler == NULL)) {
    return Status;
  }

  ConfigHandler->Stop (ConfigHandler);

  //
  // Last, uninstall ConfigHandler Protocol and resource protocol.
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  ImageHandle,
                  &gEdkIIRedfishConfigHandlerProtocolGuid,
                  ConfigHandler,
                  NULL
                  );

  FreePool (mRedfishResourcePrivate);
  mRedfishResourcePrivate = NULL;

  return Status;
}

/**
  The callback function provided by Redfish Feature driver.

  @param[in]     This                Pointer to EDKII_REDFISH_FEATURE_PROTOCOL instance.
  @param[in]     FeatureAction       The action Redfish feature driver should take.
  @param[in]     Context             The context of Redfish feature driver.
  @param[in,out] InformationExchange The pointer to RESOURCE_INFORMATION_EXCHANGE

  @retval EFI_SUCCESS              Redfish feature driver callback is executed successfully.
  @retval Others                   Some errors happened.
**/
EFI_STATUS
EFIAPI
RedfishExternalResourceResourceFeatureCallback (
  IN     EDKII_REDFISH_FEATURE_PROTOCOL  *This,
  IN     FEATURE_CALLBACK_ACTION         FeatureAction,
  IN     VOID                            *Context,
  IN OUT RESOURCE_INFORMATION_EXCHANGE   *InformationExchange
  )
{
  EFI_STATUS                       Status;
  REDFISH_SERVICE                  RedfishService;
  REDFISH_RESOURCE_COMMON_PRIVATE  *Private;
  EFI_STRING                       ResourceUri;

  if (FeatureAction != CallbackActionStartOperation) {
    return EFI_UNSUPPORTED;
  }

  Private = (REDFISH_RESOURCE_COMMON_PRIVATE *)Context;

  RedfishService = Private->RedfishService;
  if (RedfishService == NULL) {
    return EFI_NOT_READY;
  }

  //
  // Save in private structure.
  //
  Private->InformationExchange = InformationExchange;
  //
  // Find Redfish version on BMC
  //
  Private->RedfishVersion = RedfishGetVersion (RedfishService);
  //
  // Create the full URI from Redfish service root.
  //
  ResourceUri = (EFI_STRING)AllocateZeroPool (MAX_URI_LENGTH * sizeof (CHAR16));
  if (ResourceUri == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to allocate memory for full URI.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  StrCatS (ResourceUri, MAX_URI_LENGTH, Private->RedfishVersion);
  StrCatS (ResourceUri, MAX_URI_LENGTH, InformationExchange->SendInformation.FullUri);

  //
  // Initialize collection path
  //
  Private->Uri = RedfishGetUri (ResourceUri);
  if (Private->Uri == NULL) {
    ASSERT (FALSE);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = HandleResource (Private, Private->Uri);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: process external resource: %a failed: %r\n", __FUNCTION__, Private->Uri, Status));
  }

  FreePool (Private->Uri);
  return Status;
}

/**
  Callback function when gEdkIIRedfishFeatureProtocolGuid is installed.

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.
**/
VOID
EFIAPI
EdkIIRedfishFeatureProtocolIsReady (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS                      Status;
  EDKII_REDFISH_FEATURE_PROTOCOL  *FeatureProtocol;

  if (mRedfishResourcePrivate == NULL) {
    return;
  }

  if (mRedfishResourcePrivate->FeatureProtocol != NULL) {
    return;
  }

  Status = gBS->LocateProtocol (
                  &gEdkIIRedfishFeatureProtocolGuid,
                  NULL,
                  (VOID **)&FeatureProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to locate gEdkIIRedfishFeatureProtocolGuid: %r\n", __FUNCTION__, Status));
    gBS->CloseEvent (Event);
    return;
  }

  Status = FeatureProtocol->Register (
                              FeatureProtocol,
                              REDFISH_MANAGED_URI,
                              RedfishExternalResourceResourceFeatureCallback,
                              (VOID *)mRedfishResourcePrivate
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to register %s: %r\n", __FUNCTION__, REDFISH_MANAGED_URI, Status));
  }

  mRedfishResourcePrivate->FeatureProtocol = FeatureProtocol;

  gBS->CloseEvent (Event);
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers. It initialize the global variables and
  publish the driver binding protocol.

  @param[in]   ImageHandle      The firmware allocated handle for the UEFI image.
  @param[in]   SystemTable      A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                Other errors as indicated.
**/
EFI_STATUS
EFIAPI
RedfishResourceEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  VOID        *RedfishFeatureProtocolRegistration;

  if (mRedfishResourcePrivate != NULL) {
    return EFI_ALREADY_STARTED;
  }

  mRedfishResourcePrivate = AllocateZeroPool (sizeof (REDFISH_RESOURCE_COMMON_PRIVATE));
  CopyMem (&mRedfishResourcePrivate->ConfigHandler, &mRedfishConfigHandler, sizeof (EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL));

  //
  // Publish config handler protocol and resource protocol.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEdkIIRedfishConfigHandlerProtocolGuid,
                  &mRedfishResourcePrivate->ConfigHandler,
                  NULL
                  );

  EfiCreateProtocolNotifyEvent (
    &gEdkIIRedfishFeatureProtocolGuid,
    TPL_CALLBACK,
    EdkIIRedfishFeatureProtocolIsReady,
    (VOID *)mRedfishResourcePrivate,
    &RedfishFeatureProtocolRegistration
    );

  return Status;
}
