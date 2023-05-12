/** @file
  Configuration Manager Data of SMBIOS Type 2 table

  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/FruLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType2 = {
  SMBIOS_TYPE_BASEBOARD_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType02),
  NULL
};

UINT8
EFIAPI
GetMemoryDeviceCount (
  IN  CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  VOID        *DtbBase = Private->DtbBase;
  CONST VOID  *Property;
  INT32       Length;
  UINTN       Index;
  INT32       NodeOffset;
  CHAR8       Type2tNodeStr[] = "/firmware/smbios/type2@xx";
  UINT8       HandleCount;
  UINT8       CurrentCount;

  HandleCount = 0;

  for (Index = 0; Index < MAX_TYPE2_COUNT; Index++) {
    AsciiSPrint (Type2tNodeStr, sizeof (Type2tNodeStr), "/firmware/smbios/type2@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type2tNodeStr);
    if (NodeOffset < 0) {
      break;
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "memory-device-count", &Length);
    if (Property != NULL) {
      CurrentCount = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);

      //
      // Make sure every Type 2 has the the same number of memory devices, if it has.
      //
      if (HandleCount == 0) {
        HandleCount = CurrentCount;
      } else if (HandleCount != CurrentCount) {
        DEBUG ((DEBUG_ERROR, "%a: Memory device count for every Type 2 is not the same\n", __FUNCTION__));
        HandleCount = 0;
        ASSERT (FALSE);
        break;
      }
    }
  }

  return HandleCount;
}

CONTAINED_CM_OBJECTS *
EFIAPI
GetMemoryDeviceInfoToken (
  IN  CM_SMBIOS_PRIVATE_DATA  *Private,
  IN  UINT8                   SocketNum,
  IN  UINT8                   HandleCount
  )
{
  UINTN                           Index;
  UINTN                           Index2;
  UINTN                           DramIndex;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CONTAINED_CM_OBJECTS            *Objects;
  CM_SMBIOS_MEMORY_DEVICE_INFO    *CmMemDevicesInfo;

  if ((HandleCount == 0) || (SocketNum == 0)) {
    return NULL;
  } else {
    // 0 based.
    SocketNum--;
  }

  Objects   = NULL;
  Repo      = Private->Repo;
  DramIndex = HandleCount * SocketNum;

  for (Index = 0; Index < Private->CmSmbiosTableCount; Index++) {
    Repo--;
    if (Repo->CmObjectId == CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjMemoryDeviceInfo)) {
      CmMemDevicesInfo = (CM_SMBIOS_MEMORY_DEVICE_INFO *)Repo->CmObjectPtr;
      Objects          = AllocateZeroPool (HandleCount * sizeof (CONTAINED_CM_OBJECTS));
      if (Objects != NULL) {
        for (Index2 = 0; Index2 < HandleCount; Index2++) {
          if (Repo->CmObjectCount > DramIndex) {
            Objects[Index2].GeneratorId = Repo->CmObjectId;
            Objects[Index2].CmObjToken  = CmMemDevicesInfo[DramIndex].MemoryDeviceInfoToken;
          } else {
            DEBUG ((DEBUG_ERROR, "%a: Not enough memory devices for Type2\n", __FUNCTION__));
            FreePool (Objects);
            return NULL;
          }

          DramIndex++;
        }
      }

      break;
    }
  }

  return Objects;
}

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
  CM_SMBIOS_BASEBOARD_INFO        *BaseboardInfo;
  UINT32                          NumBaseboards;
  CONST VOID                      *Property;
  CONST CHAR8                     *PropertyStr;
  INT32                           Length;
  UINTN                           Index;
  UINTN                           BindingInfoIndex;
  INT32                           NodeOffset;
  CHAR8                           Type2tNodeStr[] = "/firmware/smbios/type2@xx";
  FRU_DEVICE_INFO                 *Type2FruInfo;
  CHAR8                           *FruDesc;
  UINT8                           HandleCount;
  UINT8                           SocketNum;

  NumBaseboards = 0;
  BaseboardInfo = NULL;
  HandleCount   = GetMemoryDeviceCount (Private);

  for (Index = 0; Index < MAX_TYPE2_COUNT; Index++) {
    AsciiSPrint (Type2tNodeStr, sizeof (Type2tNodeStr), "/firmware/smbios/type2@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type2tNodeStr);
    if (NodeOffset < 0) {
      break;
    }

    BaseboardInfo = ReallocatePool (
                      sizeof (CM_SMBIOS_BASEBOARD_INFO) * (NumBaseboards),
                      sizeof (CM_SMBIOS_BASEBOARD_INFO) * (NumBaseboards + 1),
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
    FruDesc  = NULL;
    Property = fdt_getprop (DtbBase, NodeOffset, "fru-desc", &Length);
    if (Property != NULL) {
      FruDesc      = (CHAR8 *)Property;
      Type2FruInfo = FindFruByDescription (Private, FruDesc);
      if (Type2FruInfo != NULL) {
        //
        // Not all board FRUs have product info. Hence, use FRU product info if it is present.
        //
        if (Type2FruInfo->ProductName != NULL) {
          if (BaseboardInfo[NumBaseboards].Manufacturer == NULL) {
            // If not override by DTB. Copy from FRU.
            BaseboardInfo[NumBaseboards].Manufacturer = AllocateCopyString (Type2FruInfo->ProductManufacturer);
          }

          BaseboardInfo[NumBaseboards].ProductName  = AllocateCopyString (Type2FruInfo->ProductName);
          BaseboardInfo[NumBaseboards].Version      = AllocateCopyString (Type2FruInfo->ProductVersion);
          BaseboardInfo[NumBaseboards].SerialNumber = AllocateCopyString (Type2FruInfo->ProductSerial);
          BaseboardInfo[NumBaseboards].AssetTag     = AllocateCopyString (Type2FruInfo->ProductAssetTag);
        } else {
          //
          // And if FRU does not have product info, use board info instead
          //
          if (BaseboardInfo[NumBaseboards].Manufacturer == NULL) {
            // If not override by DTB. Copy from FRU.
            BaseboardInfo[NumBaseboards].Manufacturer = AllocateCopyString (Type2FruInfo->BoardManufacturer);
          }

          BaseboardInfo[NumBaseboards].ProductName  = AllocateCopyString (Type2FruInfo->BoardProduct);
          BaseboardInfo[NumBaseboards].SerialNumber = AllocateCopyString (Type2FruInfo->BoardSerial);
          BaseboardInfo[NumBaseboards].Version      = GetFruExtraStr (Type2FruInfo->BoardExtra, "Version: ");
          BaseboardInfo[NumBaseboards].AssetTag     = NULL;
        }

        BaseboardInfo[NumBaseboards].BaseboardInfoToken = REFERENCE_TOKEN (BaseboardInfo[NumBaseboards]);

        if (Private->EnclosureBaseboardBinding.Info != NULL) {
          for (BindingInfoIndex = 0; BindingInfoIndex < Private->EnclosureBaseboardBinding.Count; BindingInfoIndex++) {
            if (Private->EnclosureBaseboardBinding.Info[BindingInfoIndex].FruDeviceId == Type2FruInfo->FruDeviceId) {
              BaseboardInfo[NumBaseboards].ChassisToken = Private->EnclosureBaseboardBinding.Info[BindingInfoIndex].ChassisCmToken;
            }
          }
        }
      }
    }

    BaseboardInfo[NumBaseboards].NumberOfContainedObjectHandles = 0;
    BaseboardInfo[NumBaseboards].ContainedCmObjects             = NULL;

    if (BaseboardInfo[NumBaseboards].BoardType == BaseBoardTypeProcessorMemoryModule) {
      SocketNum = 0;
      Property  = fdt_getprop (DtbBase, NodeOffset, "socket-num", &Length);
      if (Property != NULL) {
        SocketNum = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
      }

      BaseboardInfo[NumBaseboards].ContainedCmObjects = GetMemoryDeviceInfoToken (Private, SocketNum, HandleCount);

      if (BaseboardInfo[NumBaseboards].ContainedCmObjects != NULL) {
        BaseboardInfo[NumBaseboards].NumberOfContainedObjectHandles += HandleCount;
      }
    }

    NumBaseboards++;
  }

  if (Private->EnclosureBaseboardBinding.Info != NULL) {
    FreePool (Private->EnclosureBaseboardBinding.Info);
  }

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

  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjBaseboardInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = NumBaseboards * sizeof (CM_SMBIOS_BASEBOARD_INFO);
  Repo->CmObjectCount = NumBaseboards;
  Repo->CmObjectPtr   = BaseboardInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
