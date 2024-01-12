/** @file
  Translate Redfish firmware inventory to UEFI FMP protocol. - common functions

  (C) Copyright 2020-2022 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "RedfishFirmwareInfoCommon.h"
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>

REDFISH_RESOURCE_COMMON_PRIVATE  *mRedfishResourcePrivate    = NULL;
EFI_REGULAR_EXPRESSION_PROTOCOL  *mRegularExpressionProtocol = NULL;

EFI_FIRMWARE_MANAGEMENT_PROTOCOL  mRedfishFmpProtocol = {
  FmpGetImageInfo,
  FmpGetImage,
  FmpSetImage,
  FmpCheckImage,
  FmpGetPackageInfo,
  FmpSetPackageInfo
};

/**
  Consume resource from given URI.

  @param[in]   Private             Pointer to REDFISH_RESOURCE_COMMON_PRIVATE instance.
  @param[in]   Uri                 The target URI to consume.

  @retval EFI_SUCCESS              Value is returned successfully.
  @retval Others                   Some error happened.

**/
EFI_STATUS
GetFirmwareComponentInfo (
  IN     REDFISH_RESOURCE_COMMON_PRIVATE  *Private,
  IN     EFI_STRING                       Uri
  )
{
  EFI_STATUS                Status;
  REDFISH_RESPONSE          Response;
  EDKII_JSON_VALUE          JsonValue;
  REDFISH_FMP_PRIVATE_DATA  *RedfishFmpPrivate;
  UINT64                    ImageAttributes;
  EFI_STRING                ImageIdName;
  BOOLEAN                   RfUpdatable;

  Status = RedfishHttpGetResource (Private->RedfishService, Uri, &Response, TRUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: get resource from: %s failed\n", __FUNCTION__, Uri));
    return Status;
  }

  JsonValue = RedfishJsonInPayload (Response.Payload);

  ImageAttributes = 0;

  RedfishFmpPrivate = (REDFISH_FMP_PRIVATE_DATA *)AllocateZeroPool (sizeof (*RedfishFmpPrivate));

  RedfishFmpPrivate->DescriptorCount = 1;

  RedfishFmpPrivate->ImageDescriptor = AllocateZeroPool (RedfishFmpPrivate->DescriptorCount * sizeof (EFI_FIRMWARE_IMAGE_DESCRIPTOR));
  RedfishFmpPrivate->Signature       = REDFISH_FMP_PRIVATE_DATA_SIGNATURE;
  if (RedfishFmpPrivate->ImageDescriptor == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ImageIdName = JsonValueGetUnicodeString (
                  JsonObjectGetValue (JsonValueGetObject (JsonValue), "Id")
                  );
  RedfishFmpPrivate->ImageDescriptor->ImageIndex  = 1;
  RedfishFmpPrivate->ImageDescriptor->ImageIdName = ImageIdName;

  RedfishFmpPrivate->PackageVersionName = JsonValueGetUnicodeString (
                                            JsonObjectGetValue (JsonValueGetObject (JsonValue), "Description")
                                            );

  RedfishFmpPrivate->ImageDescriptor->VersionName = JsonValueGetUnicodeString (
                                                      JsonObjectGetValue (JsonValueGetObject (JsonValue), "Version")
                                                      );

  RedfishFmpPrivate->ImageDescriptor->Size = FMP_SIZE_UNKNOWN;

  RfUpdatable = JsonValueGetBoolean (
                  JsonObjectGetValue (JsonValueGetObject (JsonValue), "Updateable")
                  );

  ImageAttributes |= IMAGE_ATTRIBUTE_IN_USE;

  if (RfUpdatable == TRUE) {
    ImageAttributes |= IMAGE_ATTRIBUTE_IMAGE_UPDATABLE;
  }

  RedfishFmpPrivate->ImageDescriptor->AttributesSupported = ImageAttributes;

  //
  // Mark AttributesSetting as non-updateable.
  //
  RedfishFmpPrivate->ImageDescriptor->AttributesSetting = ImageAttributes & (~IMAGE_ATTRIBUTE_IMAGE_UPDATABLE);
  CopyMem (&RedfishFmpPrivate->Fmp, &mRedfishFmpProtocol, sizeof (EFI_FIRMWARE_MANAGEMENT_PROTOCOL));

  //
  // Install FMP protocol.
  //
  Status = gBS->InstallProtocolInterface (
                  &RedfishFmpPrivate->Handle,
                  &gEfiFirmwareManagementProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &RedfishFmpPrivate->Fmp
                  );
  //
  // Release resource
  //
  if (Response.Payload != NULL) {
    RedfishHttpFreeResource (&Response);
  }

  return Status;
}

/**
  Consume resource from given URI.

  @param[in]   Private             Pointer to REDFISH_RESOURCE_COMMON_PRIVATE instance.
  @param[in]   Json                The JSON to consume.
  @param[in]   HeaderEtag          The Etag string returned in HTTP header.

  @retval EFI_SUCCESS              Value is returned successfully.
  @retval Others                   Some error happened.

**/
EFI_STATUS
RedfishConsumeResourceCommon (
  IN  REDFISH_RESOURCE_COMMON_PRIVATE  *Private,
  IN  CHAR8                            *Json,
  IN  CHAR8                            *HeaderEtag OPTIONAL
  )
{
  UINTN             Index;
  EDKII_JSON_VALUE  JsonValue;
  EDKII_JSON_VALUE  JsonArrayObj;
  EDKII_JSON_VALUE  JsonObj;
  EFI_STRING        FirmwareComponentUri;
  EFI_STATUS        Status;
  VOID              *DeviceTreeBase = NULL;
  INT32             FirmwareInventoryOffset;
  UINTN             DeviceTreeSize;
  CHAR8             FirmwareIdProperty[] = "id??";
  BOOLEAN           IsMatch;
  UINTN             CaptureCount;
  UINT32            DtbFirmwareIdIndex;
  EFI_STRING        DtbFirmwareId[MAX_REDFISH_FMP_COUNT];
  CONST VOID        *Property;
  INT32             Length;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FirmwareComponentUri = NULL;
  ZeroMem (DtbFirmwareId, sizeof (DtbFirmwareId));

  //
  // Load device tree redfish firmware-inventory node
  //
  Status = DtPlatformLoadDtb (&DeviceTreeBase, &DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to load device tree..\r\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  FirmwareInventoryOffset = fdt_path_offset (DeviceTreeBase, "/firmware/redfish/update-service/firmware-inventory");
  if (FirmwareInventoryOffset < 0) {
    DEBUG ((DEBUG_INFO, "%a: Device tree node for firmware-inventory not found.\n", __FUNCTION__));
    Status = EFI_SUCCESS;
    goto Exit;
  } else {
    for (DtbFirmwareIdIndex = 1; DtbFirmwareIdIndex < MAX_REDFISH_FMP_COUNT; DtbFirmwareIdIndex++) {
      AsciiSPrint (FirmwareIdProperty, sizeof (FirmwareIdProperty), "id%u", DtbFirmwareIdIndex);
      Property = fdt_getprop (DeviceTreeBase, FirmwareInventoryOffset, FirmwareIdProperty, &Length);
      if ((Property != NULL) && (Length > 0)) {
        DtbFirmwareId[DtbFirmwareIdIndex] = AllocateZeroPool ((Length * sizeof (CHAR16)));
        if (DtbFirmwareId[DtbFirmwareIdIndex] == NULL) {
          DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
          break;
        }

        AsciiStrToUnicodeStrS (
          Property,
          DtbFirmwareId[DtbFirmwareIdIndex],
          Length
          );
      } else {
        break;
      }
    }
  }

  JsonObj = RedfishJsonInPayload (Private->Payload);
  if ((JsonObj == NULL) || !JsonValueIsObject (JsonObj)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid JSON payload\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  JsonArrayObj = JsonObjectGetValue (JsonValueGetObject (JsonObj), "Members");
  if ((JsonArrayObj == NULL) || !JsonValueIsArray (JsonArrayObj)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid JSON payload\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  //
  // Seeking valid URI link for firmware inventory info collection.
  //
  if (mRegularExpressionProtocol != NULL) {
    EDKII_JSON_ARRAY_FOREACH (JsonArrayObj, Index, JsonValue) {
      JsonValue            = JsonObjectGetValue (JsonValueGetObject (JsonValue), "@odata.id");
      FirmwareComponentUri = JsonValueGetUnicodeString (JsonValue);

      //
      // Gather the necessary firmware info that DTB defined.
      //
      IsMatch = FALSE;
      for (DtbFirmwareIdIndex = 1; DtbFirmwareIdIndex < MAX_REDFISH_FMP_COUNT; DtbFirmwareIdIndex++) {
        if (DtbFirmwareId[DtbFirmwareIdIndex] == NULL) {
          break;
        }

        Status = mRegularExpressionProtocol->MatchString (
                                               mRegularExpressionProtocol,
                                               FirmwareComponentUri,
                                               DtbFirmwareId[DtbFirmwareIdIndex],
                                               &gEfiRegexSyntaxTypePerlGuid,
                                               &IsMatch,
                                               NULL,
                                               &CaptureCount
                                               );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: MatchString \"%s\" failed: %r\n", __FUNCTION__, DtbFirmwareId[DtbFirmwareIdIndex], Status));
        }

        if (IsMatch) {
          GetFirmwareComponentInfo (Private, FirmwareComponentUri);
          break;
        }
      }
    }
  }

Exit:
  for (DtbFirmwareIdIndex = 1; DtbFirmwareIdIndex < MAX_REDFISH_FMP_COUNT; DtbFirmwareIdIndex++) {
    if (DtbFirmwareId[DtbFirmwareIdIndex] == NULL) {
      break;
    }

    FreePool (DtbFirmwareId[DtbFirmwareIdIndex]);
  }

  if (FirmwareComponentUri != NULL) {
    FreePool (FirmwareComponentUri);
    FirmwareComponentUri = NULL;
  }

  return Status;
}

EFI_STATUS
HandleResource (
  IN  REDFISH_RESOURCE_COMMON_PRIVATE  *Private,
  IN  EFI_STRING                       Uri
  )
{
  EFI_STATUS  Status;

  if ((Private == NULL) || IS_EMPTY_STRING (Uri)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Resource match
  //
  DEBUG ((REDFISH_DEBUG_TRACE, "%a: process resource for: %s\n", __FUNCTION__, Uri));

  //
  // Consume.
  //
  DEBUG ((REDFISH_DEBUG_TRACE, "%a consume for %s\n", __FUNCTION__, Uri));
  Status = Private->RedfishResourceConfig.Consume (&Private->RedfishResourceConfig, Uri);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to consume resource for: %s: %r\n", __FUNCTION__, Uri, Status));
  }

  return Status;
}
