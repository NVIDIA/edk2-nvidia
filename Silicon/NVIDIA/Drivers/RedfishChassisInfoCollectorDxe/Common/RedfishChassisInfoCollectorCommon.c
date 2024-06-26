/** @file
  Redfish chassis information collector - common functions

  (C) Copyright 2020-2022 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "RedfishChassisInfoCollectorCommon.h"
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/VariablePolicyHelperLib.h>

REDFISH_RESOURCE_COMMON_PRIVATE  *mRedfishResourcePrivate = NULL;

/**
  Protect chassis info variable from being changed or erased without authentication.

  @retval EFI_SUCCESS             Variables are locked successfully
  @retval EFI_SECURITY_VIOLATION  Fail to lock variables
**/
EFI_STATUS
EFIAPI
ProtectChassisVariable (
  IN     EFI_STRING  VariableName
  )
{
  EFI_STATUS                      Status;
  EDKII_VARIABLE_POLICY_PROTOCOL  *PolicyProtocol;

  Status = gBS->LocateProtocol (
                  &gEdkiiVariablePolicyProtocolGuid,
                  NULL,
                  (VOID **)&PolicyProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to locate Variable policy protocol\r\n"));
    return EFI_SECURITY_VIOLATION;
  }

  //
  // Lock variable that make them write protected for UEFI and only
  // MM can change or delete them.
  //
  Status = RegisterBasicVariablePolicy (
             PolicyProtocol,
             &gNVIDIATokenSpaceGuid,
             VariableName,
             VARIABLE_POLICY_NO_MIN_SIZE,
             VARIABLE_POLICY_NO_MAX_SIZE,
             VARIABLE_POLICY_NO_MUST_ATTR,
             VARIABLE_POLICY_NO_CANT_ATTR,
             VARIABLE_POLICY_TYPE_LOCK_NOW
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to lock %s- %r\r\n", VariableName, Status));
    return EFI_SECURITY_VIOLATION;
  }

  return EFI_SUCCESS;
}

/**
  Fetch property from given URI.

  @param[in]       Private             Pointer to REDFISH_RESOURCE_COMMON_PRIVATE instance.
  @param[in]       Uri                 The target URI to fetch.
  @param[in]       Property            The property in the payload of the given URI.
  @param[in]       VariableDataType    The expected data type to read.
  @param[in, out]  SizeOfReadingBuffer Size of the Reading
  @param[out]      Reading             The reading of the property.

  @retval EFI_SUCCESS              Value is returned successfully.
  @retval Others                   Some error happened.

**/
EFI_STATUS
GetRedfishChassisInfoProp (
  IN     REDFISH_RESOURCE_COMMON_PRIVATE  *Private,
  IN     EFI_STRING                       Uri,
  IN     CHAR8                            *Property,
  IN     UINT32                           VariableDataType,
  IN OUT UINT32                           *SizeOfReadingBuffer,
  OUT    VOID                             *Reading
  )
{
  EFI_STATUS        Status;
  REDFISH_RESPONSE  Response;
  EDKII_JSON_VALUE  JsonValue;
  EFI_STRING        JsonUnicodeString;

  ZeroMem (&Response, sizeof (Response));
  Status = RedfishHttpGetResource (Private->RedfishService, Uri, NULL, &Response, TRUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: get resource from: %s failed\n", __func__, Uri));
    return Status;
  }

  JsonValue = RedfishJsonInPayload (Response.Payload);
  if (!JsonValueIsObject (JsonValue)) {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "%a: Invalid JSON payload with %s - %r\n", __func__, Uri, Status));
    goto ON_RELEASE;
  }

  JsonValue = JsonObjectGetValue (JsonValueGetObject (JsonValue), Property);
  if (JsonValue != NULL) {
    switch (VariableDataType) {
      case EdkiiJsonTypeString:
        JsonUnicodeString = NULL;
        JsonUnicodeString = JsonValueGetUnicodeString (JsonValue);
        if (JsonUnicodeString != NULL) {
          if (StrSize (JsonUnicodeString) <= *SizeOfReadingBuffer) {
            CopyMem (Reading, JsonUnicodeString, StrSize (JsonUnicodeString));
            *SizeOfReadingBuffer = StrSize (JsonUnicodeString);
          } else {
            Status = EFI_BUFFER_TOO_SMALL;
            DEBUG ((DEBUG_ERROR, "%a, %a in %s - %r\n", __func__, Property, Uri, Status));
          }

          FreePool (JsonUnicodeString);
        }

        break;
      case EdkiiJsonTypeInteger:
        *(UINT64 *)Reading = JsonValueGetInteger (JsonValue);

        *SizeOfReadingBuffer = sizeof (UINT64);
        break;
      case EdkiiJsonTypeTrue:
      case EdkiiJsonTypeFalse:
        *(BOOLEAN *)Reading  = JsonValueGetBoolean (JsonValue);
        *SizeOfReadingBuffer = sizeof (BOOLEAN);
        break;
      default:
        DEBUG ((DEBUG_ERROR, "%a, unsupported value type: 0x%x\n", __func__, VariableDataType));
        Status = EFI_UNSUPPORTED;
    }
  } else {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "%a, %a in %s - %r\n", __func__, Property, Uri, Status));
  }

ON_RELEASE:

  RedfishHttpFreeResponse (&Response);

  return Status;
}

EFI_STATUS
HandleResource (
  IN  REDFISH_RESOURCE_COMMON_PRIVATE  *Private,
  IN  EFI_STRING                       Uri
  )
{
  EFI_STATUS              Status;
  VOID                    *DeviceTreeBase = NULL;
  UINTN                   DeviceTreeSize;
  UINTN                   ChassisInfoNodeIndex;
  INT32                   NodeOffset;
  UINT8                   Reading[256];
  UINT32                  SizeOfReadingBuffer;
  CHASSIS_INFO_PROP_ATTR  DtbChassisInfoPropAttr;
  CHAR8                   *DtbChassisInfoPropName;
  CHAR16                  DtbChassisInfoPropVarName[32];
  CHAR16                  DtbChassisInfoPropUri[MAX_URI_LENGTH];
  CONST VOID              *Property;
  INT32                   Length;
  CHAR8                   ChassisInfoNodeString[] = "/firmware/redfish/chassis-info/prop@xx";

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Load device tree redfish chassis-info node
  //
  Status = DtPlatformLoadDtb (&DeviceTreeBase, &DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to load device tree.\r\n", __func__));
    return EFI_DEVICE_ERROR;
  }

  for (ChassisInfoNodeIndex = 0; ChassisInfoNodeIndex < MAX_CHASSIS_INFO_NODE_COUNT; ChassisInfoNodeIndex++) {
    ZeroMem (DtbChassisInfoPropUri, sizeof (DtbChassisInfoPropUri));
    ZeroMem (DtbChassisInfoPropVarName, sizeof (DtbChassisInfoPropVarName));
    DtbChassisInfoPropName      = NULL;
    DtbChassisInfoPropAttr.Data = 0;

    AsciiSPrint (ChassisInfoNodeString, sizeof (ChassisInfoNodeString), "/firmware/redfish/chassis-info/prop@%u", ChassisInfoNodeIndex);
    NodeOffset = fdt_path_offset (DeviceTreeBase, ChassisInfoNodeString);
    if (NodeOffset < 0) {
      DEBUG ((DEBUG_INFO, "%a: Device tree node for chassis-info not found.\n", __func__));
      Status = EFI_SUCCESS;
      break;
    } else {
      Property = fdt_getprop (DeviceTreeBase, NodeOffset, "uri", &Length);
      if ((Property != NULL) && (Length > 0)) {
        if (ARRAY_SIZE (DtbChassisInfoPropUri) > Length) {
          AsciiStrToUnicodeStrS (
            Property,
            DtbChassisInfoPropUri,
            Length
            );
        } else {
          DEBUG ((DEBUG_ERROR, "%a: %a - %r .\n", __func__, (CHAR8 *)Property, EFI_BUFFER_TOO_SMALL));
          continue;
        }
      }

      Property = fdt_getprop (DeviceTreeBase, NodeOffset, "attr", &Length);
      if ((Property != NULL) && (Length > 0)) {
        DtbChassisInfoPropAttr.Data = (UINT32)fdt32_to_cpu (*(UINT32 *)Property);
      }

      Property = fdt_getprop (DeviceTreeBase, NodeOffset, "uefi-var", &Length);
      if ((Property != NULL) && (Length > 0)) {
        if (ARRAY_SIZE (DtbChassisInfoPropVarName) > Length) {
          AsciiStrToUnicodeStrS (
            Property,
            DtbChassisInfoPropVarName,
            Length
            );
        } else {
          DEBUG ((DEBUG_ERROR, "%a: %a - %r .\n", __func__, (CHAR8 *)Property, EFI_BUFFER_TOO_SMALL));
          continue;
        }
      }

      Property = fdt_getprop (DeviceTreeBase, NodeOffset, "prop-name", &Length);
      if ((Property != NULL) && (Length > 0)) {
        DtbChassisInfoPropName = (CHAR8 *)Property;
      }
    }

    if (DtbChassisInfoPropAttr.Bits.EfiVariableWriteOnceFlag) {
      Length = sizeof (Reading);
      Status = gRT->GetVariable (DtbChassisInfoPropVarName, &gNVIDIATokenSpaceGuid, NULL, (UINTN *)&Length, Reading);
      if (Status != EFI_NOT_FOUND) {
        // Variable is exist already.
        DEBUG (
          (DEBUG_INFO, "%a: chassis info var %s is exist.\n", __func__, DtbChassisInfoPropVarName)
          );
        continue;
      }
    }

    if ((DtbChassisInfoPropName == NULL) ||
        (StrSize (DtbChassisInfoPropUri) == 0) ||
        (StrSize (DtbChassisInfoPropVarName) == 0))
    {
      continue;
    }

    SizeOfReadingBuffer = sizeof (Reading);
    ZeroMem (Reading, SizeOfReadingBuffer);

    Status = GetRedfishChassisInfoProp (
               Private,
               DtbChassisInfoPropUri,
               DtbChassisInfoPropName,
               DtbChassisInfoPropAttr.Bits.EdkiiJsonType,
               &SizeOfReadingBuffer,
               Reading
               );
    if (!EFI_ERROR (Status)) {
      Status = gRT->SetVariable (
                      DtbChassisInfoPropVarName,
                      &gNVIDIATokenSpaceGuid,
                      DtbChassisInfoPropAttr.Bits.EfiVariableAttributes,
                      SizeOfReadingBuffer,
                      Reading
                      );
      if (!EFI_ERROR (Status)) {
        if ( DtbChassisInfoPropAttr.Bits.EfiVariableLockFlag) {
          Status = ProtectChassisVariable (DtbChassisInfoPropVarName);
          if (EFI_ERROR (Status)) {
            DEBUG ((
              DEBUG_ERROR,
              "%a: VariableLock (%s) failed - %r\n",
              __func__,
              DtbChassisInfoPropVarName,
              Status
              ));
          }
        }
      } else {
        DEBUG ((
          DEBUG_ERROR,
          "%a: SetVariable (%s) failed - %r\n",
          __func__,
          DtbChassisInfoPropVarName,
          Status
          ));
      }
    } else {
    }
  }

  return Status;
}
