/** @file
  Platform implementation to support Redfish BIOS configuration.

  This driver uses EDKII_REDFISH_RESOURCE_ADDENDUM_PROTOCOL to provide BMC
  required data during Redfish operation and support BIOS configuration on
  Redfish service.

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PlatformRedfishBiosDxe.h"

//
// Specify the BIOS schema and version that we support.
// '*' means that we accept any of them.
//
REDFISH_RESOURCE_SCHEMA_INFO  mSupportBiosSchema[] = {
  {
    "*",
    "Bios",
    "v1_1_0"
  },
  {
    "*",
    "Bios",
    "v1_2_0"
  }
};

/**
  Dump Json object for debugging purpose.

  @param[in]      Message          Help message.
  @param[in]      JsonValue        Json object to dump.

**/
VOID
DumpJsonData (
  IN  CONST CHAR8       *Message,
  IN  EDKII_JSON_VALUE  JsonValue
  )
{
  CHAR8  *JsonString;

  if (Message != NULL) {
    DEBUG ((REDFISH_BIOS_DEBUG_DUMP, "%a: ", Message));
  }

  JsonString = JsonDumpString (JsonValue, EDKII_JSON_COMPACT);
  if (JsonString != NULL) {
    DEBUG ((REDFISH_BIOS_DEBUG_DUMP, "%a\n", JsonString));
    FreePool (JsonString);
  }
}

/**
  Check to see if this is supported BIOS schema or not.

  @param[in]      SchemaInfo       Schema information on request

  @retval TRUE                     This is supported schema.
  @retval FALSE                    This is NOT supported schema.

**/
BOOLEAN
IsSupportedBiosSchema (
  IN REDFISH_RESOURCE_SCHEMA_INFO  *SchemaInfo
  )
{
  UINTN  SchemaCount;
  UINTN  Index;

  if (SchemaInfo == NULL) {
    return FALSE;
  }

  SchemaCount = sizeof (mSupportBiosSchema) / sizeof (REDFISH_RESOURCE_SCHEMA_INFO);
  if (SchemaCount == 0) {
    return TRUE;
  }

  for (Index = 0; Index < SchemaCount; Index++) {
    //
    // URI
    //
    if ((mSupportBiosSchema[Index].Uri[0] != '*') && (AsciiStrCmp (mSupportBiosSchema[Index].Uri, SchemaInfo->Uri) != 0)) {
      continue;
    }

    //
    // Schema name
    //
    if ((mSupportBiosSchema[Index].Schema[0] != '*') && (AsciiStrCmp (mSupportBiosSchema[Index].Schema, SchemaInfo->Schema) != 0)) {
      continue;
    }

    //
    // Schema version
    //
    if ((mSupportBiosSchema[Index].Version[0] != '*') && (AsciiStrCmp (mSupportBiosSchema[Index].Version, SchemaInfo->Version) != 0)) {
      continue;
    }

    return TRUE;
  }

  return FALSE;
}

/**
  Convert JSON value to Redfish value.

  @param[in]      Value            JSON value to be converted.
  @param[out]     RedfishValue     Redfish value from given JSON value.

  @retval EFI_SUCCESS              Conversion is done successfully.
  @retval EFI_UNSUPPORTED          Unsupported JSON value type.
  @retval Others                   Some error happened.

**/
EFI_STATUS
JsonValueToRedfishValue (
  IN  EDKII_JSON_VALUE     Value,
  OUT EDKII_REDFISH_VALUE  *RedfishValue
  )
{
  EDKII_JSON_TYPE  type;

  if ((Value == NULL) || (RedfishValue == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  type = JsonGetType (Value);
  switch (type) {
    case EdkiiJsonTypeString:
      RedfishValue->Type         = RedfishValueTypeString;
      RedfishValue->Value.Buffer = (CHAR8 *)JsonValueGetAsciiString (Value);
      break;
    case EdkiiJsonTypeInteger:
      RedfishValue->Type          = RedfishValueTypeInteger;
      RedfishValue->Value.Integer = JsonValueGetInteger (Value);
      break;
    case EdkiiJsonTypeTrue:
    case EdkiiJsonTypeFalse:
      RedfishValue->Type          = RedfishValueTypeBoolean;
      RedfishValue->Value.Boolean = JsonValueGetBoolean (Value);
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a, unsupported value type: 0x%x\n", __FUNCTION__, type));
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Convert Redfish value to JSON value.

  @param[in]      RedfishValue     Redfish value to be converted.
  @param[out]     Value            JSON value from given Redfish value.

  @retval EFI_SUCCESS              Conversion is done successfully.
  @retval EFI_UNSUPPORTED          Unsupported Redfish value type.
  @retval Others                   Some error happened.

**/
EFI_STATUS
RedfishValueToJsonValue (
  IN EDKII_REDFISH_VALUE  *RedfishValue,
  OUT EDKII_JSON_VALUE    *Value
  )
{
  if ((Value == NULL) || (RedfishValue == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  switch (RedfishValue->Type) {
    case RedfishValueTypeString:
      *Value = JsonValueInitAsciiString (RedfishValue->Value.Buffer);
      break;
    case RedfishValueTypeInteger:
      *Value = JsonValueInitInteger (RedfishValue->Value.Integer);
      break;
    case RedfishValueTypeBoolean:
      *Value = JsonValueInitBoolean (RedfishValue->Value.Boolean);
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a, unsupported value type: 0x%x\n", __FUNCTION__, RedfishValue->Type));
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Convert Redfish attribute type to stirng in JSON object.

  @param[in]      Type     Redfish attribute type.
  @param[out]     Value    Type string in JSON object.

  @retval EFI_SUCCESS              Conversion is done successfully.
  @retval EFI_UNSUPPORTED          Unsupported Redfish attribute type.
  @retval Others                   Some error happened.

**/
EFI_STATUS
AttributeTypeToJsonValue (
  IN EDKII_REDFISH_ATTRIBUTE_TYPES  Type,
  OUT EDKII_JSON_VALUE              *Value
  )
{
  if (Value == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  switch (Type) {
    case RedfishAttributeTypeEnumeration:
      *Value = JsonValueInitAsciiString ("Enumeration");
      break;
    case RedfishAttributeTypeString:
      *Value = JsonValueInitAsciiString ("String");
      break;
    case RedfishAttributeTypeInteger:
      *Value = JsonValueInitAsciiString ("Integer");
      break;
    case RedfishAttributeTypeBoolean:
      *Value = JsonValueInitAsciiString ("Boolean");
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a, unsupported value type: 0x%x\n", __FUNCTION__, Type));
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  This function consume input BIOS attribute name and create all required
  JSON objects that is required by BMC. The format is basically the format
  of AttributeRegistry.v1_3_6

  @param[in]      Schema        The schema name.
  @param[in]      Version       The schema verion string.
  @param[in]      Key           The name of BIOS attribute.
  @param[in]      Value         The value of given BIOS attribute.
  @param[out]     AttributeObj  The object which follows BMC requirements.

  @retval EFI_SUCCESS              Attribute is created successfully.
  @retval Others                   Some error happened.

**/
EFI_STATUS
GenerateAttributeDetails (
  IN CHAR8              *Schema,
  IN CHAR8              *Version,
  IN CHAR8              *Key,
  IN EDKII_JSON_VALUE   Value,
  OUT EDKII_JSON_VALUE  *AttributeObj
  )
{
  EDKII_JSON_VALUE         AttributeValue;
  EDKII_JSON_VALUE         AttributeArray;
  EFI_STATUS               Status;
  EDKII_REDFISH_ATTRIBUTE  Attribute;
  EDKII_REDFISH_VALUE      DefaultValue;
  CHAR16                   ConfigureLang[REDFISH_BIOS_CONFIG_LANG_SIZE];
  UINTN                    Index;
  BOOLEAN                  NoDefaultValue;

  if ((Schema == NULL) || (Version == NULL) || (Key == NULL) || (AttributeObj == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  AttributeArray = NULL;
  AttributeValue = NULL;
  ZeroMem (&DefaultValue, sizeof (EDKII_REDFISH_VALUE));
  ZeroMem (&Attribute, sizeof (EDKII_REDFISH_ATTRIBUTE));

  UnicodeSPrint (ConfigureLang, sizeof (CHAR16) * REDFISH_BIOS_CONFIG_LANG_SIZE, L"%s%a", REDFISH_BIOS_CONFIG_LANG_PREFIX, Key);
  DEBUG ((DEBUG_INFO, "%a, generate %a attribute\n", __FUNCTION__, Key));

  //
  // Get HII question details of given attribute name
  //
  Status = RedfishPlatformConfigGetAttribute (Schema, Version, ConfigureLang, &Attribute);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, RedfishPlatformConfigGetAttribute for %s failed: %r\n", __FUNCTION__, ConfigureLang, Status));
    return Status;
  }

  //
  // Get default value of given attribute name if it is not read-only property.
  //
  NoDefaultValue = TRUE;
  if (!Attribute.ReadOnly) {
    Status = RedfishPlatformConfigGetDefaultValue (Schema, Version, ConfigureLang, EDKII_REDFISH_DEFAULT_CLASS_STANDARD, &DefaultValue);
    if (!EFI_ERROR (Status)) {
      NoDefaultValue = FALSE;
    } else {
      DEBUG ((DEBUG_ERROR, "%a, RedfishPlatformConfigGetDefaultValue for %s failed: %r\n", __FUNCTION__, ConfigureLang, Status));
    }
  }

  *AttributeObj = JsonValueInitObject ();
  if (*AttributeObj == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  //
  // Create required attributes for BMC
  // AttributeName
  //
  AttributeValue = JsonValueInitAsciiString (Key);
  if (AttributeValue == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  JsonObjectSetValue (*AttributeObj, "AttributeName", AttributeValue);

  //
  // CurrentValue
  //
  JsonObjectSetValue (*AttributeObj, "CurrentValue", Value);

  //
  // DefaultValue
  //
  if (NoDefaultValue) {
    AttributeValue = JsonValueInitNull ();
    if (AttributeValue == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto RELEASE;
    }
  } else {
    Status = RedfishValueToJsonValue (&DefaultValue, &AttributeValue);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, RedfishValueToJsonValue failed: %r\n", __FUNCTION__, Status));
      goto RELEASE;
    }
  }

  JsonObjectSetValue (*AttributeObj, "DefaultValue", AttributeValue);

  //
  // DisplayName
  //
  AttributeValue = JsonValueInitAsciiString (Attribute.DisplayName);
  if (AttributeValue == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  JsonObjectSetValue (*AttributeObj, "DisplayName", AttributeValue);

  //
  // HelpText (Description in BMC)
  //
  AttributeValue = JsonValueInitAsciiString (Attribute.HelpText);
  if (AttributeValue == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  JsonObjectSetValue (*AttributeObj, "Description", AttributeValue);

  //
  // MenuPath
  //
  AttributeValue = JsonValueInitAsciiString (Attribute.MenuPath);
  if (AttributeValue == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  JsonObjectSetValue (*AttributeObj, "MenuPath", AttributeValue);

  //
  // ReadOnly
  //
  AttributeValue = JsonValueInitBoolean (Attribute.ReadOnly);
  if (AttributeValue == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  JsonObjectSetValue (*AttributeObj, "ReadOnly", AttributeValue);

  //
  // ResetRequired
  //
  AttributeValue = JsonValueInitBoolean (Attribute.ResetRequired);
  if (AttributeValue == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  JsonObjectSetValue (*AttributeObj, "ResetRequired", AttributeValue);

  //
  // Type
  //
  Status = AttributeTypeToJsonValue (Attribute.Type, &AttributeValue);
  if (EFI_ERROR (Status)) {
    goto RELEASE;
  }

  JsonObjectSetValue (*AttributeObj, "Type", AttributeValue);

  //
  // String length
  //
  if (Attribute.Type == RedfishAttributeTypeString) {
    //
    // MaxLength
    //
    AttributeValue = JsonValueInitInteger (Attribute.StrMaxSize);
    if (AttributeValue == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto RELEASE;
    }

    JsonObjectSetValue (*AttributeObj, "MaxLength", AttributeValue);

    //
    // MinLength
    //
    AttributeValue = JsonValueInitInteger (Attribute.StrMinSize);
    if (AttributeValue == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto RELEASE;
    }

    JsonObjectSetValue (*AttributeObj, "MinLength", AttributeValue);
  }

  //
  // Numeric boundary
  //
  if (Attribute.Type == RedfishAttributeTypeInteger) {
    //
    // UpperBound
    //
    AttributeValue = JsonValueInitInteger (Attribute.NumMaximum);
    if (AttributeValue == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto RELEASE;
    }

    JsonObjectSetValue (*AttributeObj, "UpperBound", AttributeValue);

    //
    // LowerBound
    //
    AttributeValue = JsonValueInitInteger (Attribute.NumMinimum);
    if (AttributeValue == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto RELEASE;
    }

    JsonObjectSetValue (*AttributeObj, "LowerBound", AttributeValue);

    //
    // ScalarIncrement
    //
    AttributeValue = JsonValueInitInteger (Attribute.NumStep);
    if (AttributeValue == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto RELEASE;
    }

    JsonObjectSetValue (*AttributeObj, "ScalarIncrement", AttributeValue);
  }

  //
  // Values
  //
  if ((Attribute.Type == RedfishAttributeTypeEnumeration) && (Attribute.Values.ValueCount > 0)) {
    AttributeArray = JsonValueInitArray ();

    for (Index = 0; Index < Attribute.Values.ValueCount; Index++) {
      AttributeValue = JsonValueInitAsciiString (Attribute.Values.ValueArray[Index].ValueName);
      JsonArrayAppendValue (AttributeArray, AttributeValue);
    }

    JsonObjectSetValue (*AttributeObj, "Values", AttributeArray);
  }

  Status = EFI_SUCCESS;

RELEASE:
  //
  // Release string buffer
  //
  if (Attribute.AttributeName != NULL) {
    FreePool (Attribute.AttributeName);
  }

  if (Attribute.DisplayName != NULL) {
    FreePool (Attribute.DisplayName);
  }

  if (Attribute.HelpText != NULL) {
    FreePool (Attribute.HelpText);
  }

  if (Attribute.MenuPath != NULL) {
    FreePool (Attribute.MenuPath);
  }

  return Status;
}

/**
  Provision redfish resource with with addendum data in given schema.

  @param[in]      This             Pointer to EDKII_REDFISH_RESOURCE_ADDENDUM_PROTOCOL instance.
  @param[in]      SchemaInfo       Redfish schema information.
  @param[in,out]  Json             On input, this is the Redfish data in given Uri in JSON object.
                                   On output, This is the Redfish data with addendum information
                                   which platform would like to add to Redfish service.

  @retval EFI_SUCCESS              Addendum data is attached.
  @retval EFI_UNSUPPORTED          No addendum data is required in given schema.
  @retval Others                   Some error happened.

**/
EFI_STATUS
PlatformRedfishBiosAddendumData (
  IN     EDKII_REDFISH_RESOURCE_ADDENDUM_PROTOCOL  *This,
  IN     REDFISH_RESOURCE_SCHEMA_INFO              *SchemaInfo,
  IN OUT EDKII_JSON_VALUE                          JsonData
  )
{
  CHAR8             *Key;
  EDKII_JSON_VALUE  Value;
  EDKII_JSON_VALUE  BiosAttributes;
  EDKII_JSON_VALUE  AttributeObj;
  EDKII_JSON_VALUE  AttributeArray;
  EFI_STATUS        Status;
  VOID              *Iterator;
  UINTN             Index;

  if ((This == NULL) || (SchemaInfo == NULL) || (JsonData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!JsonValueIsObject (JsonData)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!IsSupportedBiosSchema (SchemaInfo)) {
    DEBUG ((REDFISH_BIOS_DEBUG_DUMP, "%a, unsupported schema: %a version: %a at %a\n", __FUNCTION__, SchemaInfo->Schema, SchemaInfo->Version, SchemaInfo->Uri));
    return EFI_UNSUPPORTED;
  }

  DEBUG_CODE_BEGIN ();
  DumpJsonData (__FUNCTION__, JsonData);
  DEBUG_CODE_END ();

  //
  // Check and see if there is "Attributes" object or not
  //
  BiosAttributes = NULL;
  Iterator       = JsonObjectIterator (JsonData);
  while (Iterator != NULL) {
    Key   = JsonObjectIteratorKey (Iterator);
    Value = JsonObjectIteratorValue (Iterator);

    if (AsciiStrCmp (Key, REDFISH_BIOS_ATTRIBUTES_NAME) == 0) {
      BiosAttributes = Value;
      break;
    }

    Iterator = JsonObjectIteratorNext (JsonData, Iterator);
  }

  if (BiosAttributes == NULL) {
    return EFI_NOT_FOUND;
  }

  //
  // Prepare attribute array
  //
  AttributeArray = JsonValueInitArray ();
  if (AttributeArray == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Generate attribute details for BMC one by one
  //
  Index    = 0;
  Iterator = JsonObjectIterator (BiosAttributes);
  while (Iterator != NULL) {
    Key   = JsonObjectIteratorKey (Iterator);
    Value = JsonObjectIteratorValue (Iterator);

    AttributeObj = NULL;
    Status       = GenerateAttributeDetails (SchemaInfo->Schema, SchemaInfo->Version, Key, Value, &AttributeObj);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, GenerateAttributeDetails failed: %r\n", __FUNCTION__, Status));
    }

    DEBUG_CODE_BEGIN ();
    if (AttributeObj != NULL) {
      DumpJsonData (__FUNCTION__, AttributeObj);
    }

    DEBUG_CODE_END ();

    if (AttributeObj != NULL) {
      JsonArrayAppendValue (AttributeArray, AttributeObj);
    }

    Index   += 1;
    Iterator = JsonObjectIteratorNext (BiosAttributes, Iterator);
  }

  //
  // If array is not empty, replace input JSON object with this one.
  //
  if (JsonArrayCount (AttributeArray) > 0) {
    Status = JsonObjectClear (JsonValueGetObject (JsonData));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to clear JSON object\n", __func__));
      JsonValueFree (AttributeArray);
      return EFI_DEVICE_ERROR;
    }

    JsonObjectSetValue (JsonData, REDFISH_BIOS_ATTRIBUTES_NAME, AttributeArray);
    return EFI_SUCCESS;
  }

  JsonValueFree (AttributeArray);

  return EFI_NOT_FOUND;
}

/**
  Provision redfish OEM resource in given schema information.

  @param[in]   This             Pointer to EDKII_REDFISH_RESOURCE_ADDENDUM_PROTOCOL instance.
  @param[in]   SchemaInfo       Redfish schema information.
  @param[out]  Json             This is the Redfish data which is attached to OEM object in given
                                schema.

  @retval EFI_SUCCESS              OEM data is attached.
  @retval EFI_UNSUPPORTED          No OEM data is required in given schema.
  @retval Others                   Some error happened.

**/
EFI_STATUS
PlatformRedfishBiosOemData (
  IN   EDKII_REDFISH_RESOURCE_ADDENDUM_PROTOCOL  *This,
  IN   REDFISH_RESOURCE_SCHEMA_INFO              *SchemaInfo,
  OUT  EDKII_JSON_VALUE                          JsonData
  )
{
  //
  // There is no OEM attributes in BIOS schema.
  //
  return EFI_UNSUPPORTED;
}

EDKII_REDFISH_RESOURCE_ADDENDUM_PROTOCOL  mRedfishResourceAddendum = {
  ADDENDUM_PROTOCOL_VERSION,
  PlatformRedfishBiosOemData,
  PlatformRedfishBiosAddendumData
};

/**
  Unloads an image.

  @param  ImageHandle           Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
  @retval EFI_INVALID_PARAMETER ImageHandle is not a valid image handle.

**/
EFI_STATUS
EFIAPI
PlatformRedfishBiosUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  Status = gBS->UninstallProtocolInterface (
                  ImageHandle,
                  &gEdkIIRedfishResourceAddendumProtocolGuid,
                  &mRedfishResourceAddendum
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to uninstall Redfish Resource Addendum Protocol: %r\n", __FUNCTION__, Status));
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
PlatformRedfishBiosEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEdkIIRedfishResourceAddendumProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mRedfishResourceAddendum
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to install Redfish Resource Addendum Protocol: %r\n", __FUNCTION__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}
