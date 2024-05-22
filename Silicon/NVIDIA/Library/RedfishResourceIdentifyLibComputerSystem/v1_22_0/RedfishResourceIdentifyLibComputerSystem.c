/** @file
  Redfish resource identify library implementation for computer system version 1.17.0

  (C) Copyright 2022 Hewlett Packard Enterprise Development LP<BR>
  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <RedfishBase.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/RedfishResourceIdentifyLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/RestJsonStructure.h>

#include <RedfishJsonStructure/ComputerSystem/v1_22_0/EfiComputerSystemV1_22_0.h>

EFI_REST_JSON_STRUCTURE_PROTOCOL  *mJsonStructProtocol = NULL;

/**
  Identify resource from given URI and context in JSON format

  @param[in]   Uri    URI of given Redfish resource
  @param[in]   Json   Context in JSON format of give Redfish resource

  @retval TRUE        This is the Redfish resource that we have to handle.
  @retval FALSE       We don't handle this Redfish resource.

**/
BOOLEAN
RedfishIdentifyResource (
  IN     EFI_STRING  Uri,
  IN     CHAR8       *Json
  )
{
  EFI_STATUS                             Status;
  EFI_REDFISH_COMPUTERSYSTEM_V1_22_0     *ComputerSystem;
  EFI_REDFISH_COMPUTERSYSTEM_V1_22_0_CS  *ComputerSystemCs;
  RedfishCS_Link                         *List;
  RedfishCS_Header                       *Header;
  RedfishCS_Type_Uri_Data                *UriData;
  BOOLEAN                                Supported;

  if (IS_EMPTY_STRING (Uri) || IS_EMPTY_STRING (Json)) {
    return FALSE;
  }

  Supported        = FALSE;
  ComputerSystem   = NULL;
  ComputerSystemCs = NULL;

  if (mJsonStructProtocol == NULL) {
    return FALSE;
  }

  Status = mJsonStructProtocol->ToStructure (
                                  mJsonStructProtocol,
                                  NULL,
                                  Json,
                                  (EFI_REST_JSON_STRUCTURE_HEADER **)&ComputerSystem
                                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, ToStructure() failed: %r\n", __func__, Status));
    return FALSE;
  }

  ComputerSystemCs = ComputerSystem->ComputerSystem;

  if (IsLinkEmpty (&ComputerSystemCs->Bios)) {
    goto ON_RELEASE;
  }

  List = GetFirstLink (&ComputerSystemCs->Bios);
  if (List == NULL) {
    goto ON_RELEASE;
  }

  Header = (RedfishCS_Header *)List;
  if (Header->ResourceType == RedfishCS_Type_Uri) {
    UriData   = (RedfishCS_Type_Uri_Data *)Header;
    Supported = TRUE;
    DEBUG ((DEBUG_MANAGEABILITY, "%a: Bios found: %a\n", __func__, UriData->Uri));
  }

ON_RELEASE:

  mJsonStructProtocol->DestoryStructure (
                         mJsonStructProtocol,
                         (EFI_REST_JSON_STRUCTURE_HEADER *)ComputerSystem
                         );

  return Supported;
}

/**
  Callback function when gEfiRestJsonStructureProtocolGuid is installed.

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.
**/
VOID
EFIAPI
RestJasonStructureProtocolIsReady (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS  Status;

  if (mJsonStructProtocol != NULL) {
    return;
  }

  Status = gBS->LocateProtocol (
                  &gEfiRestJsonStructureProtocolGuid,
                  NULL,
                  (VOID **)&mJsonStructProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, failed to locate gEfiRestJsonStructureProtocolGuid: %r\n", __func__, Status));
  }

  gBS->CloseEvent (Event);
}

/**

  Install JSON protocol notification

  @param[in] ImageHandle     The image handle.
  @param[in] SystemTable     The system table.

  @retval  EFI_SUCCESS  Install Boot manager menu success.
  @retval  Other        Return error status.

**/
EFI_STATUS
EFIAPI
RedfishResourceIdentifyComputerSystemConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  VOID  *Registration;

  EfiCreateProtocolNotifyEvent (
    &gEfiRestJsonStructureProtocolGuid,
    TPL_CALLBACK,
    RestJasonStructureProtocolIsReady,
    NULL,
    &Registration
    );

  return EFI_SUCCESS;
}
