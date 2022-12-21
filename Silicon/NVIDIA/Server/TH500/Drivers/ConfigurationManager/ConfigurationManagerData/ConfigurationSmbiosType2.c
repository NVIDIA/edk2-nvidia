/** @file
  Configuration Manager Data of SMBIOS Type 2 table

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
#include <Library/PrintLib.h>
#include <libfdt.h>

#include <IndustryStandard/Ipmi.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType2 = {
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType02),
  NULL
};

/**
  Install CM object for SMBIOS Type 2

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType2Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo    = Private->Repo;
  VOID                            *DtbBase = Private->DtbBase;
  CM_STD_BASEBOARD_INFO           *BaseboardInfo;
  UINT32                          NumBaseboards;
  CONST VOID                      *Property;
  CONST CHAR8                     *PropertyStr;
  INT32                           Length;
  UINTN                           Index;
  INT32                           NodeOffset;
  CHAR8                           Type2tNodeStr[] = "/firmware/smbios/type2@xx";
  FRU_DEVICE_INFO                 *SystemFru;
  CHAR8                           *FruDesc;

  NumBaseboards = 0;
  BaseboardInfo = NULL;

  for (Index = 0; Index < 10; Index++) {
    AsciiSPrint (Type2tNodeStr, sizeof (Type2tNodeStr), "/firmware/smbios/type2@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type2tNodeStr);
    if (NodeOffset < 0) {
      break;
    }

    BaseboardInfo = ReallocatePool (
                      sizeof (CM_STD_BASEBOARD_INFO) * (NumBaseboards),
                      sizeof (CM_STD_BASEBOARD_INFO) * (NumBaseboards + 1),
                      BaseboardInfo
                      );
    if (BaseboardInfo == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: ReallocatePool failed\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    // Default
    BaseboardInfo[NumBaseboards].ProductName  = NULL;
    BaseboardInfo[NumBaseboards].Version      = NULL;
    BaseboardInfo[NumBaseboards].SerialNumber = NULL;
    BaseboardInfo[NumBaseboards].AssetTag     = NULL;

    // Get data from DTB
    PropertyStr = fdt_getprop (DtbBase, NodeOffset, "manufacturer", &Length);
    if (PropertyStr != NULL ) {
      BaseboardInfo[NumBaseboards].Manufacturer = AllocateZeroPool (Length + 1);
      AsciiSPrint (BaseboardInfo[NumBaseboards].Manufacturer, Length + 1, PropertyStr);
    } else {
      BaseboardInfo[NumBaseboards].Manufacturer = NULL;
    }

    PropertyStr = fdt_getprop (DtbBase, NodeOffset, "location-in-chassis", &Length);
    if (PropertyStr != NULL) {
      BaseboardInfo[NumBaseboards].LocationInChassis = AllocateZeroPool (Length + 1);
      AsciiSPrint (BaseboardInfo[NumBaseboards].LocationInChassis, Length + 1, PropertyStr);
    } else {
      BaseboardInfo[NumBaseboards].LocationInChassis = NULL;
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "feature-flags", &Length);
    if (Property != NULL) {
      BaseboardInfo[NumBaseboards].FeatureFlag = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "board-type", &Length);
    if (Property != NULL) {
      BaseboardInfo[NumBaseboards].BoardType = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    // Get data from FRU.
    Property = fdt_getprop (DtbBase, NodeOffset, "fru-desc", &Length);
    if (Property != NULL) {
      FruDesc   = (CHAR8 *)Property;
      SystemFru = FindFruByDescription (Private, FruDesc);
      if (SystemFru != NULL) {
        if (BaseboardInfo[NumBaseboards].Manufacturer == NULL) {
          // If not override by DTB. Copy from FRU.
          BaseboardInfo[NumBaseboards].Manufacturer = AllocateCopyString (SystemFru->ProductManufacturer);
        }

        BaseboardInfo[NumBaseboards].ProductName  = AllocateCopyString (SystemFru->ProductName);
        BaseboardInfo[NumBaseboards].Version      = AllocateCopyString (SystemFru->ProductVersion);
        BaseboardInfo[NumBaseboards].SerialNumber = AllocateCopyString (SystemFru->ProductSerial);
        BaseboardInfo[NumBaseboards].AssetTag     = AllocateCopyString (SystemFru->ProductAssetTag);
      }
    }

    BaseboardInfo[NumBaseboards].NumberOfContainedObjectHandles = 0;
    BaseboardInfo[NumBaseboards].ChassisToken                   = CM_NULL_TOKEN;

    NumBaseboards++;
  }

  for (Index = 0; Index < NumBaseboards; Index++) {
    BaseboardInfo[Index].BaseboardInfoToken = REFERENCE_TOKEN (BaseboardInfo[Index]);
  }

  DEBUG ((DEBUG_INFO, "%a: NumBaseboards = %d\n", __FUNCTION__, NumBaseboards));

  if (BaseboardInfo == NULL) {
    return EFI_NOT_FOUND;
  }

  //
  // Add type 2 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType2,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjBaseboardInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = NumBaseboards * sizeof (CM_STD_BASEBOARD_INFO);
  Repo->CmObjectCount = NumBaseboards;
  Repo->CmObjectPtr   = BaseboardInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
