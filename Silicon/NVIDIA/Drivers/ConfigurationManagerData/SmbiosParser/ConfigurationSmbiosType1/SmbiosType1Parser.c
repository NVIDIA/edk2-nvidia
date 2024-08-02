/** @file
  Configuration Manager Data of SMBIOS Type 1 table.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/FruLib.h>
#include <libfdt.h>

#include <IndustryStandard/Ipmi.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosType1Parser.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType1 = {
  SMBIOS_TYPE_SYSTEM_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType01),
  NULL
};

/**
  Get system GUID from BMC

  @param[out]  SystemGuid   System GUID

  @return EFI_SUCCESS       Successful
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
GetSystemGuid (
  OUT GUID  *SystemGuid
  )
{
  EFI_STATUS                     Status;
  IPMI_GET_DEVICE_GUID_RESPONSE  ResponseData;
  UINT32                         ResponseDataSize = sizeof (ResponseData);

  Status = IpmiSubmitCommand (
             IPMI_NETFN_APP,
             IPMI_APP_GET_SYSTEM_GUID,
             NULL,
             0,
             (UINT8 *)&ResponseData,
             &ResponseDataSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: IPMI transaction failure - %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseDataSize != sizeof (ResponseData)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Unexpected response size, Got: %u, Expected: %u\n",
      __FUNCTION__,
      ResponseDataSize,
      sizeof (ResponseData)
      ));
    return EFI_DEVICE_ERROR;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Unexpected command completion code, Got: %x, Expected: %x\n",
      __FUNCTION__,
      ResponseData.CompletionCode,
      IPMI_COMP_CODE_NORMAL
      ));
    return EFI_DEVICE_ERROR;
  }

  CopyGuid (SystemGuid, (GUID *)&ResponseData.Guid);
  return EFI_SUCCESS;
}

/**
  Install CM object for SMBIOS Type 1
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType1Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  VOID                   *DtbBase = Private->DtbBase;
  CM_SMBIOS_SYSTEM_INFO  *SystemInfo;
  EFI_STATUS             Status;
  INTN                   DtbOffset;
  CONST VOID             *Property;
  INT32                  Length;
  FRU_DEVICE_INFO        *SystemFru;
  CHAR8                  *FruDesc;
  CHAR8                  *ManufacturerStr;
  CHAR8                  *ProductNameStr;
  CHAR8                  *ProductVersionStr;
  CHAR8                  *ProductSerialStr;
  CHAR8                  *ProductPartNumStr;
  CM_OBJECT_TOKEN        *TokenMap;
  CM_OBJ_DESCRIPTOR      Desc;

  TokenMap   = NULL;
  SystemInfo = NULL;
  //
  // Allocate and zero out System Info. The strings that are NULL will be set as "Unknown"
  //
  SystemInfo = (CM_SMBIOS_SYSTEM_INFO *)AllocateZeroPool (sizeof (CM_SMBIOS_SYSTEM_INFO));
  if (SystemInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for system info\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  //
  // Get system info from FRU data
  // '/firmware/smbios/type1/fru-desc' is required to specify which FRU is used
  //
  DtbOffset = fdt_subnode_offset (DtbBase, Private->DtbSmbiosOffset, "type1");
  if (DtbOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS Type 1 not found.\n", __FUNCTION__));
    Status = RETURN_NOT_FOUND;
    goto CleanupAndReturn;
  }

  Property = fdt_getprop (DtbBase, DtbOffset, "fru-desc", &Length);
  if ((Property == NULL) || (Length == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree property 'fru-desc' not found.\n", __FUNCTION__));
    Status = RETURN_NOT_FOUND;
    goto CleanupAndReturn;
  }

  FruDesc   = (CHAR8 *)Property;
  SystemFru = FindFruByDescription (Private, FruDesc);
  if (SystemFru == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: FRU '%a' not found.\n", __FUNCTION__, FruDesc));
    ManufacturerStr   = NULL;
    ProductNameStr    = NULL;
    ProductVersionStr = NULL;
    ProductSerialStr  = NULL;
    ProductPartNumStr = NULL;
  } else {
    ManufacturerStr   = SystemFru->ProductManufacturer;
    ProductNameStr    = SystemFru->ProductName;
    ProductVersionStr = SystemFru->ProductVersion;
    ProductSerialStr  = SystemFru->ProductSerial;
    ProductPartNumStr = SystemFru->ProductPartNum;
  }

  //
  // Check if there are OEM overrides
  //
  Property = fdt_getprop (DtbBase, DtbOffset, "manufacturer", &Length);
  if ((Property != NULL) && (Length != 0)) {
    ManufacturerStr = (CHAR8 *)Property;
  }

  Property = fdt_getprop (DtbBase, DtbOffset, "product-name", &Length);
  if ((Property != NULL) && (Length != 0)) {
    ProductNameStr = (CHAR8 *)Property;
  }

  Property = fdt_getprop (DtbBase, DtbOffset, "family", &Length);
  if ((Property != NULL) && (Length != 0)) {
    SystemInfo->Family = (CHAR8 *)AllocateCopyPool (Length, Property);
  }

  //
  // Copy strings to CM object
  //
  SystemInfo->Manufacturer = AllocateCopyString (ManufacturerStr);
  SystemInfo->ProductName  = AllocateCopyString (ProductNameStr);
  SystemInfo->Version      = AllocateCopyString (ProductVersionStr);
  SystemInfo->SerialNum    = AllocateCopyString (ProductSerialStr);
  SystemInfo->SkuNum       = AllocateCopyString (ProductPartNumStr);

  //
  // UUID is the same as BMC's System GUID
  //
  Status = GetSystemGuid (&SystemInfo->Uuid);
  if (EFI_ERROR (Status)) {
    SetMem (&SystemInfo->Uuid, sizeof (SystemInfo->Uuid), 0);
  }

  SystemInfo->WakeUpType = SystemWakeupTypePowerSwitch;

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, 1, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 1: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  SystemInfo->SystemInfoToken = TokenMap[0];

  // Add the CmObj to the Configuration Manager.
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjSystemInfo);
  Desc.Size     = sizeof (CM_SMBIOS_SYSTEM_INFO);
  Desc.Count    = 1;
  Desc.Data     = SystemInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 1 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  //
  // Add type 1 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType1,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (SystemInfo);
  return Status;
}
