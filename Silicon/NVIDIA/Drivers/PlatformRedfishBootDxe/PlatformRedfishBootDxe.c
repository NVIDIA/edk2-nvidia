/** @file
  Platform Redfish boot order driver.

  (C) Copyright 2022 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PlatformRedfishBootDxe.h"

EFI_HII_HANDLE                              mHiiHandle;
EFI_HANDLE                                  mDriverHandle;
PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA  mBootOptionsVarData;
EFI_EVENT                                   mEvent = NULL;

//
// Specify the Computer System schema and version that we support.
// '*' means that we accept any of them.
//
REDFISH_RESOURCE_SCHEMA_INFO  mSupportComputerSystemSchema[] = {
  {
    "*",
    "ComputerSystem",
    "v1_17_0"
  }
};

///
/// HII specific Vendor Device Path definition.
///
HII_VENDOR_DEVICE_PATH  mHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    PLATFORM_REDFISH_BOOT_FORMSET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

/**
  This function add 'x-uefi-' configuration language to given string ID.

  @param[in] HiiHandle                HII handle
  @param[in] StringId                 String token ID
  @param[in] Index                    The index of boot option
  @param[in] BootOption               Boot option context

  @retval EFI_STATUS

**/
EFI_STATUS
UpdateConfigLanguageToValues (
  IN  EFI_HII_HANDLE                HiiHandle,
  IN  EFI_STRING_ID                 StringId,
  IN  UINTN                         Index,
  IN  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  CHAR16  ConfigLanguage[10];

  if ((HiiHandle == NULL) || (StringId == 0) || (BootOption == NULL)) {
    return EFI_INVALID_LANGUAGE;
  }

  UnicodeSPrint (ConfigLanguage, sizeof (ConfigLanguage), L"Boot%04x", BootOption->OptionNumber);

  DEBUG ((REDFISH_BOOT_DEBUG_DUMP, "%a: add config-language for string(0x%x): %s\n", __func__, StringId, ConfigLanguage));

  HiiSetString (
    HiiHandle,
    StringId,
    ConfigLanguage,
    COMPUTER_SYSTEM_SCHEMA_VERSION
    );

  return EFI_SUCCESS;
}

/**
  This function creates boot order with ordered-list op-codes in runtime.

  @retval EFI_STATUS

**/
EFI_STATUS
RefreshBootOrderList (
  VOID
  )
{
  UINTN                         Index;
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption;
  UINTN                         BootOptionCount;
  EFI_STRING_ID                 Token;
  VOID                          *StartOpCodeHandle;
  VOID                          *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL            *StartLabel;
  EFI_IFR_GUID_LABEL            *EndLabel;
  BOOLEAN                       IsLegacyOption;
  VOID                          *OptionsOpCodeHandle;
  UINTN                         OptionIndex;

  //
  // for better user experience
  // 1. User changes HD configuration (e.g.: unplug HDD), here we have a chance to remove the HDD boot option
  // 2. User enables/disables UEFI PXE, here we have a chance to add/remove EFI Network boot option
  //
  EfiBootManagerRefreshAllBootOption ();

  BootOption = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);

  if (BootOptionCount == 0) {
    return EFI_NOT_FOUND;
  }

  //
  // Initial var store
  //
  ZeroMem (&mBootOptionsVarData, sizeof (PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA));

  //
  // Allocate space for creation of UpdateData Buffer
  //
  StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (StartOpCodeHandle != NULL);

  EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (EndOpCodeHandle != NULL);

  //
  // Create Hii Extend Label OpCode as the start opcode
  //
  StartLabel               = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (StartOpCodeHandle, &gEfiIfrTianoGuid, NULL, sizeof (EFI_IFR_GUID_LABEL));
  StartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number       = LABEL_BOOT_OPTION;

  //
  // Create Hii Extend Label OpCode as the end opcode
  //
  EndLabel               = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (EndOpCodeHandle, &gEfiIfrTianoGuid, NULL, sizeof (EFI_IFR_GUID_LABEL));
  EndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number       = LABEL_BOOT_OPTION_END;

  OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  for (Index = 0, OptionIndex = 0; Index < BootOptionCount; Index++) {
    //
    // Don't display hidden boot options, but retain inactive ones.
    //
    if ((BootOption[Index].Attributes & LOAD_OPTION_HIDDEN) != 0) {
      continue;
    }

    //
    // Group the legacy boot option in the sub title created dynamically
    //
    IsLegacyOption = (BOOLEAN)(
                               (DevicePathType (BootOption[Index].FilePath) == BBS_DEVICE_PATH) &&
                               (DevicePathSubType (BootOption[Index].FilePath) == BBS_BBS_DP)
                               );

    //
    // Don't display legacy boot options
    //
    if (IsLegacyOption) {
      continue;
    }

    mBootOptionsVarData.BootOptionOrder[OptionIndex++] = (UINT32)BootOption[Index].OptionNumber;

    ASSERT (BootOption[Index].Description != NULL);

    Token = HiiSetString (mHiiHandle, 0, BootOption[Index].Description, NULL);

    //
    // Add boot option
    //
    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      Token,
      0,
      EFI_IFR_TYPE_NUM_SIZE_32,
      BootOption[Index].OptionNumber
      );

    //
    // Add x-uefi configure language for boot options.
    //
    UpdateConfigLanguageToValues (mHiiHandle, Token, OptionIndex, &BootOption[Index]);
  }

  //
  // Create ordered list op-code
  //
  HiiCreateOrderedListOpCode (
    StartOpCodeHandle,                        // Container for dynamic created opcodes
    BOOT_ORDER_LIST,                          // Question ID
    BOOT_OPTION_VAR_STORE_ID,                 // VarStore ID
    (UINT16)VAR_OFFSET (BootOptionOrder),     // Offset in Buffer Storage
    STRING_TOKEN (STR_BOOT_ORDER_LIST),       // Question prompt text
    STRING_TOKEN (STR_BOOT_ORDER_LIST_HELP),  // Question help text
    0,                                        // Question flag
    EFI_IFR_UNIQUE_SET,                       // Ordered list flag, e.g. EFI_IFR_UNIQUE_SET
    EFI_IFR_TYPE_NUM_SIZE_32,                 // Data type of Question value
    MAX_BOOT_OPTIONS,                         // Maximum container
    OptionsOpCodeHandle,                      // Option Opcode list
    NULL                                      // Default Opcode is NULL
    );

  //
  // Update HII form
  //
  HiiUpdateForm (
    mHiiHandle,
    &gPlatformRedfishBootFormsetGuid,
    FORM_ID,
    StartOpCodeHandle,
    EndOpCodeHandle
    );

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);
  HiiFreeOpCodeHandle (OptionsOpCodeHandle);

  EfiBootManagerFreeLoadOptions (BootOption, BootOptionCount);

  return EFI_SUCCESS;
}

/**
  This function update the "BootOrder" EFI Variable based on
  BMM Formset's NV map. It then refresh BootOptionMenu
  with the new "BootOrder" list.

  @param[in] BootOptionVar    Boot option NV data

  @retval EFI_SUCCESS             The function complete successfully.
  @retval EFI_OUT_OF_RESOURCES    Not enough memory to complete the function.
  @return The EFI variable can not be saved. See gRT->SetVariable for detail return information.

**/
EFI_STATUS
UpdateBootOrderList (
  IN PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA  *BootOptionVar
  )
{
  EFI_STATUS  Status;
  UINT16      Index;
  UINT16      OrderIndex;
  UINT16      *BootOrder;
  UINTN       BootOrderSize;
  UINT16      OptionNumber;

  if (BootOptionVar == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // First check whether BootOrder is present in current configuration
  //
  GetEfiGlobalVariable2 (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &BootOrderSize);
  if (BootOrder == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // OptionOrder is subset of BootOrder
  //
  for (OrderIndex = 0; (OrderIndex < MAX_BOOT_OPTIONS) && (BootOptionVar->BootOptionOrder[OrderIndex] != 0); OrderIndex++) {
    for (Index = OrderIndex; Index < BootOrderSize / sizeof (UINT16); Index++) {
      if ((BootOrder[Index] == (UINT16)BootOptionVar->BootOptionOrder[OrderIndex]) && (OrderIndex != Index)) {
        OptionNumber = BootOrder[Index];
        CopyMem (&BootOrder[OrderIndex + 1], &BootOrder[OrderIndex], (Index - OrderIndex) * sizeof (UINT16));
        BootOrder[OrderIndex] = OptionNumber;
      }
    }
  }

  Status = gRT->SetVariable (
                  EFI_BOOT_ORDER_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  BootOrderSize,
                  BootOrder
                  );
  FreePool (BootOrder);

  return Status;
}

/**
  Initial HII variable if it does not exist.

  @retval EFI_SUCCESS     HII variable is initialized.

**/
EFI_STATUS
InitialHiiVariable (
  VOID
  )
{
  //
  // Initial var store
  //
  ZeroMem (&mBootOptionsVarData, sizeof (PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA));

  return EFI_SUCCESS;
}

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.

  @param[in]  This               Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Request            A null-terminated Unicode string in
                                 <ConfigRequest> format.
  @param[out]  Progress          On return, points to a character in the Request
                                 string. Points to the string's null terminator if
                                 request was successful. Points to the most recent
                                 '&' before the first failing name/value pair (or
                                 the beginning of the string if the failure is in
                                 the first name/value pair) if the request was not
                                 successful.
  @param[out]  Results           A null-terminated Unicode string in
                                 <ConfigAltResp> format which has all values filled
                                 in for the names in the Request string. String to
                                 be allocated by the called function.

  @retval EFI_SUCCESS            The Results is filled with the requested values.
  @retval EFI_OUT_OF_RESOURCES   Not enough memory to store the results.
  @retval EFI_INVALID_PARAMETER  Request is illegal syntax, or unknown name.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
PlatformRedfishBootExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Request,
  OUT EFI_STRING                            *Progress,
  OUT EFI_STRING                            *Results
  )
{
  EFI_STATUS  Status;

  if ((Progress == NULL) || (Results == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Request;

  if (Request == NULL) {
    return EFI_UNSUPPORTED;
  }

  //
  // Handle boot order list
  //
  if (HiiIsConfigHdrMatch (Request, &gPlatformRedfishBootFormsetGuid, L"PlatformRedfishBootOptionVar")) {
    Status = gHiiConfigRouting->BlockToConfig (
                                  gHiiConfigRouting,
                                  Request,
                                  (UINT8 *)&mBootOptionsVarData,
                                  sizeof (PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA),
                                  Results,
                                  Progress
                                  );

    //
    // Set Progress string to the original request string.
    //
    if (Request == NULL) {
      *Progress = NULL;
    } else if (StrStr (Request, L"OFFSET") == NULL) {
      *Progress = Request + StrLen (Request);
    }

    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

/**
  This function processes the results of changes in configuration.

  @param[in]  This               Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Configuration      A null-terminated Unicode string in <ConfigResp>
                                 format.
  @param[out] Progress           A pointer to a string filled in with the offset of
                                 the most recent '&' before the first failing
                                 name/value pair (or the beginning of the string if
                                 the failure is in the first name/value pair) or
                                 the terminating NULL if all was successful.

  @retval EFI_SUCCESS            The Results is processed successfully.
  @retval EFI_INVALID_PARAMETER  Configuration is NULL.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
PlatformRedfishBootRouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Configuration,
  OUT EFI_STRING                            *Progress
  )
{
  EFI_STATUS                                  Status;
  UINTN                                       BufferSize;
  PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA  BootOptionsVar;

  *Progress = Configuration;

  //
  // Handle boot order list
  //
  if (HiiIsConfigHdrMatch (Configuration, &gPlatformRedfishBootFormsetGuid, L"PlatformRedfishBootOptionVar")) {
    BufferSize = sizeof (PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA);
    ZeroMem (&BootOptionsVar, sizeof (PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA));
    Status = gHiiConfigRouting->ConfigToBlock (
                                  gHiiConfigRouting,
                                  Configuration,
                                  (UINT8 *)&BootOptionsVar,
                                  &BufferSize,
                                  Progress
                                  );

    if (CompareMem (BootOptionsVar.BootOptionOrder, mBootOptionsVarData.BootOptionOrder, (sizeof (UINT32) * MAX_BOOT_OPTIONS))) {
      Status = UpdateBootOrderList (&BootOptionsVar);
      if (!EFI_ERROR (Status)) {
        //
        // Boot order update successfully. Copy it to local copy.
        //
        CopyMem (mBootOptionsVarData.BootOptionOrder, BootOptionsVar.BootOptionOrder, (sizeof (UINT32) * MAX_BOOT_OPTIONS));
      }
    }

    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

/**
  This function processes the results of changes in configuration.

  @param[in]  This               Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Action             Specifies the type of action taken by the browser.
  @param[in]  QuestionId         A unique value which is sent to the original
                                 exporting driver so that it can identify the type
                                 of data to expect.
  @param[in]  Type               The type of value for the question.
  @param[in]  Value              A pointer to the data being sent to the original
                                 exporting driver.
  @param[out] ActionRequest      On return, points to the action requested by the
                                 callback function.

  @retval EFI_SUCCESS            The callback successfully handled the action.
  @retval EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the
                                 variable and its data.
  @retval EFI_DEVICE_ERROR       The variable could not be saved.
  @retval EFI_UNSUPPORTED        The specified Action is not supported by the
                                 callback.

**/
EFI_STATUS
EFIAPI
PlatformRedfishBootDriverCallback (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  EFI_BROWSER_ACTION                    Action,
  IN  EFI_QUESTION_ID                       QuestionId,
  IN  UINT8                                 Type,
  IN  EFI_IFR_TYPE_VALUE                    *Value,
  OUT EFI_BROWSER_ACTION_REQUEST            *ActionRequest
  )
{
  DEBUG ((REDFISH_BOOT_DEBUG_DUMP, "%a: action: 0x%x QID: 0x%x\n", __func__, Action, QuestionId));

  if (Action == EFI_BROWSER_ACTION_FORM_OPEN) {
    RefreshBootOrderList ();

    return EFI_SUCCESS;
  }

  return EFI_UNSUPPORTED;
}

EFI_HII_CONFIG_ACCESS_PROTOCOL  mHii2RedfishConfigAccess = {
  PlatformRedfishBootExtractConfig,
  PlatformRedfishBootRouteConfig,
  PlatformRedfishBootDriverCallback
};

/**
  Callback function executed when the ready-to-provisioning event group is signaled.

  @param[in]   Event    Event whose notification function is being invoked.
  @param[out]  Context  Pointer to the Context buffer

**/
VOID
EFIAPI
PlatformRedfishBootReadyToProvisioning (
  IN  EFI_EVENT  Event,
  OUT VOID       *Context
  )
{
  DEBUG ((REDFISH_BOOT_DEBUG_DUMP, "%a: update boot order configure language\n", __FUNCTION__));
  //
  // Refresh boot order and create configure language
  //
  RefreshBootOrderList ();
}

/**
  Check to see if this is supported Computer System schema or not.

  @param[in]      SchemaInfo       Schema information on request

  @retval TRUE                     This is supported schema.
  @retval FALSE                    This is NOT supported schema.

**/
BOOLEAN
IsSupportedComputerSystemSchema (
  IN REDFISH_RESOURCE_SCHEMA_INFO  *SchemaInfo
  )
{
  UINTN  SchemaCount;
  UINTN  Index;

  if (SchemaInfo == NULL) {
    return FALSE;
  }

  SchemaCount = sizeof (mSupportComputerSystemSchema) / sizeof (REDFISH_RESOURCE_SCHEMA_INFO);
  if (SchemaCount == 0) {
    return TRUE;
  }

  for (Index = 0; Index < SchemaCount; Index++) {
    //
    // URI
    //
    if ((mSupportComputerSystemSchema[Index].Uri[0] != '*') && (AsciiStrCmp (mSupportComputerSystemSchema[Index].Uri, SchemaInfo->Uri) != 0)) {
      continue;
    }

    //
    // Schema name
    //
    if ((mSupportComputerSystemSchema[Index].Schema[0] != '*') && (AsciiStrCmp (mSupportComputerSystemSchema[Index].Schema, SchemaInfo->Schema) != 0)) {
      continue;
    }

    //
    // Schema version
    //
    if ((mSupportComputerSystemSchema[Index].Version[0] != '*') && (AsciiStrCmp (mSupportComputerSystemSchema[Index].Version, SchemaInfo->Version) != 0)) {
      continue;
    }

    return TRUE;
  }

  return FALSE;
}

/**
  The function is used to get a JSON value corresponding to the input key from a JSON object.

  It only returns a reference to this value and any changes on this value will impact the
  original JSON object. If that is not expected, please call JsonValueClone() to clone it to
  use.

  @param[in]   JsonObj           The provided JSON object.
  @param[in]   SearchKey         The key of the JSON value to be retrieved.

  @retval      Return the corresponding JSON value to key, or NULL on error.

**/
EDKII_JSON_VALUE
JsonObjectFind (
  IN EDKII_JSON_OBJECT  JsonObj,
  IN CHAR8              *SearchKey
  )
{
  CHAR8             *Key;
  VOID              *Iterator;
  EDKII_JSON_VALUE  Value;
  EDKII_JSON_VALUE  Target;

  if (!JsonValueIsObject (JsonObj) || (SearchKey == NULL)) {
    return NULL;
  }

  Target   = NULL;
  Iterator = JsonObjectIterator (JsonObj);
  while (Iterator != NULL) {
    Key   = JsonObjectIteratorKey (Iterator);
    Value = JsonObjectIteratorValue (Iterator);

    if (AsciiStrCmp (Key, SearchKey) == 0) {
      Target = Value;
      break;
    }

    Iterator = JsonObjectIteratorNext (JsonObj, Iterator);
  }

  return Target;
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
PlatformRedfishBootAddendumData (
  IN     EDKII_REDFISH_RESOURCE_ADDENDUM_PROTOCOL  *This,
  IN     REDFISH_RESOURCE_SCHEMA_INFO              *SchemaInfo,
  IN OUT EDKII_JSON_VALUE                          JsonData
  )
{
  EFI_STATUS        Status;
  EDKII_JSON_VALUE  Target;
  EDKII_JSON_VALUE  BootOrderObj;
  EDKII_JSON_VALUE  BootObj;

  if ((This == NULL) || (SchemaInfo == NULL) || (JsonData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!JsonValueIsObject (JsonData)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!IsSupportedComputerSystemSchema (SchemaInfo)) {
    DEBUG ((REDFISH_BOOT_DEBUG_DUMP, "%a, unsupported schema: %a version: %a at %a\n", __FUNCTION__, SchemaInfo->Schema, SchemaInfo->Version, SchemaInfo->Uri));
    return EFI_UNSUPPORTED;
  }

  BootObj      = NULL;
  BootOrderObj = NULL;
  Target       = NULL;

  DEBUG_CODE_BEGIN ();
  DumpJsonValue (REDFISH_BOOT_DEBUG_DUMP, JsonData);
  DEBUG_CODE_END ();

  //
  // Only Boot->BootOrder is patchable attribute to BMC.
  // We have to remove other attributes if any.
  //
  Target = JsonObjectFind (JsonValueGetObject (JsonData), REDFISH_BOOT_OBJECT_NAME);
  if (!JsonValueIsObject (Target)) {
    DEBUG ((DEBUG_ERROR, "%a, cannot find boot attribute\n", __func__));
    return EFI_NOT_FOUND;
  }

  Target = JsonObjectFind (JsonValueGetObject (Target), REDFISH_BOOTORDER_OBJECT_NAME);
  if (!JsonValueIsArray (Target)) {
    DEBUG ((DEBUG_ERROR, "%a, cannot find boot order attribute\n", __func__));
    return EFI_NOT_FOUND;
  }

  //
  // Make a copy of boot order object because JsonData will be
  // cleared later.
  //
  BootOrderObj = JsonValueClone (Target);
  if (BootOrderObj == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = JsonObjectClear (JsonValueGetObject (JsonData));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to clear JSON object\n", __func__));
    Status = EFI_DEVICE_ERROR;
    goto ON_RELEASE;
  }

  BootObj = JsonValueInitObject ();
  if (BootObj == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: failed to init JSON object\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_RELEASE;
  }

  Status = JsonObjectSetValue (JsonValueGetObject (BootObj), REDFISH_BOOTORDER_OBJECT_NAME, BootOrderObj);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to add BootOrder\n", __func__));
    Status = EFI_DEVICE_ERROR;
    goto ON_RELEASE;
  }

  Status = JsonObjectSetValue (JsonValueGetObject (JsonData), REDFISH_BOOT_OBJECT_NAME, BootObj);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to add Boot\n", __func__));
    Status = EFI_DEVICE_ERROR;
    goto ON_RELEASE;
  }

  return EFI_SUCCESS;

ON_RELEASE:

  if (BootObj != NULL) {
    JsonValueFree (BootObj);
  }

  if (BootOrderObj != NULL) {
    JsonValueFree (BootOrderObj);
  }

  return Status;
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
PlatformRedfishBootOemData (
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
  PlatformRedfishBootOemData,
  PlatformRedfishBootAddendumData
};

/**
  Main entry for this driver.

  @param[in] ImageHandle     Image handle this driver.
  @param[in] SystemTable     Pointer to SystemTable.

  @retval EFI_SUCCESS     This function always complete successfully.

**/
EFI_STATUS
EFIAPI
PlatformRedfishBootDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  mDriverHandle = NULL;
  Status        = gBS->InstallMultipleProtocolInterfaces (
                         &mDriverHandle,
                         &gEfiDevicePathProtocolGuid,
                         &mHiiVendorDevicePath,
                         &gEfiHiiConfigAccessProtocolGuid,
                         &mHii2RedfishConfigAccess,
                         NULL
                         );

  //
  // Publish our HII data
  //
  mHiiHandle = HiiAddPackages (
                 &gPlatformRedfishBootFormsetGuid,
                 mDriverHandle,
                 PlatformRedfishBootDxeStrings,
                 PlatformRedfishBootVfrBin,
                 NULL
                 );
  if (mHiiHandle == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = InitialHiiVariable ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to initial variable: %r\n", __func__, Status));
  }

  //
  // Register read-to-provisioning event
  //
  Status = CreateReadyToProvisioningEvent (
             PlatformRedfishBootReadyToProvisioning,
             NULL,
             &mEvent
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to register ready-to-provisioning event: %r\n", __func__, Status));
  }

  //
  // Provide addendum protocol to format JSON in the way that BMC accepted.
  //
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

/**
  Unloads the application and its installed protocol.

  @param[in]  ImageHandle       Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
**/
EFI_STATUS
EFIAPI
PlatformRedfishBootDxeDriverUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  if (mHiiHandle != NULL) {
    HiiRemovePackages (mHiiHandle);
  }

  if (mEvent != NULL) {
    gBS->CloseEvent (mEvent);
  }

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
