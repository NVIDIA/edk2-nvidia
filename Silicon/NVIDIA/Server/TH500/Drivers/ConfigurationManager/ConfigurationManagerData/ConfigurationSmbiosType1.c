/** @file
  Configuration Manager Data of SMBIOS Type 1 table

  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType1 = {
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
      "%a: Unexpected response size, Got: %d, Expected: %d\n",
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

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType1Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo    = Private->Repo;
  VOID                            *DtbBase = Private->DtbBase;
  CM_ARM_SYSTEM_INFO              *SystemInfo;
  EFI_STATUS                      Status;
  INTN                            DtbOffset;
  CONST VOID                      *Property;
  INT32                           Length;
  FRU_DEVICE_INFO                 *SystemFru;
  CHAR8                           *FruDesc;
  CHAR8                           *ManufacturerStr;
  CHAR8                           *ProductNameStr;

  //
  // Allocate and zero out System Info. The strings that are NULL will be set as "Unknown"
  //
  SystemInfo = (CM_ARM_SYSTEM_INFO *)AllocateZeroPool (sizeof (CM_ARM_SYSTEM_INFO));
  if (SystemInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for system info\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Get system info from FRU data
  // '/firmware/smbios/type1/fru-desc' is required to specify which FRU is used
  //
  DtbOffset = fdt_subnode_offset (DtbBase, Private->DtbSmbiosOffset, "type1");
  if (DtbOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS Type 1 not found.\n", __FUNCTION__));
    FreePool (SystemInfo);
    return RETURN_NOT_FOUND;
  }

  Property = fdt_getprop (DtbBase, DtbOffset, "fru-desc", &Length);
  if ((Property == NULL) || (Length == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree property 'fru-desc' not found.\n", __FUNCTION__));
    FreePool (SystemInfo);
    return RETURN_NOT_FOUND;
  }

  FruDesc   = (CHAR8 *)Property;
  SystemFru = FindFruByDescription (Private, FruDesc);
  if (SystemFru == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: FRU '%a' not found.\n", __FUNCTION__, FruDesc));
    FreePool (SystemInfo);
    return RETURN_NOT_FOUND;
  }

  //
  // Check if there are OEM overrides
  //
  Property = fdt_getprop (DtbBase, DtbOffset, "manufacturer", &Length);
  if ((Property != NULL) && (Length != 0)) {
    ManufacturerStr = (CHAR8 *)Property;
  } else {
    ManufacturerStr = SystemFru->ProductManufacturer;
  }

  Property = fdt_getprop (DtbBase, DtbOffset, "product-name", &Length);
  if ((Property != NULL) && (Length != 0)) {
    ProductNameStr = (CHAR8 *)Property;
  } else {
    ProductNameStr = SystemFru->ProductName;
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
  SystemInfo->Version      = AllocateCopyString (SystemFru->ProductVersion);
  SystemInfo->SerialNum    = AllocateCopyString (SystemFru->ProductSerial);
  SystemInfo->SkuNum       = AllocateCopyString (SystemFru->ProductPartNum);

  //
  // UUID is the same as BMC's System GUID
  //
  Status = GetSystemGuid (&SystemInfo->Uuid);
  if (EFI_ERROR (Status)) {
    SetMem (&SystemInfo->Uuid, sizeof (SystemInfo->Uuid), 0);
  }

  SystemInfo->WakeUpType = SystemWakeupTypePowerSwitch;

  //
  // Add type 1 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType1,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  //
  // Install CM object for type 1
  //
  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjSystemInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_SYSTEM_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = SystemInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
