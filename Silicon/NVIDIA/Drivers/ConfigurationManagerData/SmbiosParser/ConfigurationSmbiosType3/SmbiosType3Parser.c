/** @file
  Configuration Manager Data of SMBIOS Type 3 table.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FruLib.h>
#include <libfdt.h>
#include <Library/PrintLib.h>
#include <ConfigurationManagerObject.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <NVIDIAConfiguration.h>
#include "SmbiosParserPrivate.h"
#include "SmbiosType3Parser.h"
#include <Library/IpmiLib.h>
#include <IndustryStandard/Ipmi.h>

#define  LAST_POWER_EVENT_AC_FAILED                 0x01
#define  LAST_POWER_EVENT_POWER_OVERLOAD            0x02
#define  LAST_POWER_EVENT_POWER_INTERLOCK           0x04
#define  LAST_POWER_EVENT_POWER_FAULT               0x08
#define  MISC_CHASSIS_STATE_THERMAL_FAULT_DETECTED  0x04

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType3 = {
  SMBIOS_TYPE_SYSTEM_ENCLOSURE,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType03),
  NULL
};

/**
  Get system fru data for SMBIOS Type 3 data collection.

  @param[in ] Private       Pointer to the private data of SMBIOS creators
  @param[in ] DtbBase       Pointer to device tree
  @param[in ] DtbOffset     SMBIOS type 3 Offset in device tree binary
  @param[in ] PropertyName  PropertyName for FRU description
  @param[out] FruData       A pointer to the FRU data to return. Return NULL if
                            any failure.
  @return EFI_SUCCESS       Successful for FRU data query.
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
EFIAPI
GetFruDataType3 (
  IN CM_SMBIOS_PRIVATE_DATA  *Private,
  IN VOID                    *DtbBase,
  IN INTN                    DtbOffset,
  IN CHAR8                   *PropertyName,
  OUT FRU_DEVICE_INFO        **FruData
  )
{
  INT32  Length;
  CHAR8  *FruDesc;

  FruDesc = NULL;

  FruDesc = (CHAR8 *)fdt_getprop (DtbBase, DtbOffset, PropertyName, &Length);
  if ((FruDesc == NULL) || (Length == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree property '%a' not found.\n", __FUNCTION__, PropertyName));
    return EFI_NOT_FOUND;
  }

  *FruData = FindFruByDescription (Private, FruDesc);
  if (*FruData == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: FRU '%a' not found.\n", __FUNCTION__, FruDesc));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Install CM object for SMBIOS Type 3
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType3Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  VOID                              *DtbBase = Private->DtbBase;
  CM_SMBIOS_ENCLOSURE_INFO          *EnclosureInfo;
  EFI_STATUS                        Status;
  INTN                              Type3ContainedElementOffset;
  CONST VOID                        *Property;
  INT32                             Length;
  FRU_DEVICE_INFO                   *Type3FruInfo;
  UINT8                             ChassisType;
  UINTN                             Type3Index;
  UINTN                             Index;
  INT32                             NodeOffset;
  UINT32                            NumEnclosures;
  CONTAINED_ELEMENT                 *ContainedElements;
  UINT8                             ContainedElementCount;
  UINTN                             ProductInfoSize;
  CHAR8                             *ManufacturerStr    = NULL;
  CHAR8                             *SkuNumberStr       = NULL;
  CHAR16                            *SkuNumberUniStr    = NULL;
  CHAR8                             *SerialNumberStr    = NULL;
  CHAR16                            *SerialNumberUniStr = NULL;
  CHAR8                             *AssetTagStr        = NULL;
  CHAR8                             *VersionStr         = NULL;
  SMBIOS_TABLE_TYPE3                *Type3RecordPcd;
  NVIDIA_PRODUCT_INFO               ProductInfo;
  UINTN                             AssetTagLenWithNullTerminator;
  IPMI_GET_CHASSIS_STATUS_RESPONSE  ChassisStatusResponse;
  UINT32                            DataSize;
  CHAR16                            ProductInfoVariableName[]  = L"ProductInfo";
  CHAR8                             Type3NodeStr[]             = "/firmware/smbios/type3@xx";
  CHAR8                             DtContainedElementFormat[] = "/firmware/smbios/type3@xx/contained-element@xx";
  CM_OBJ_DESCRIPTOR                 Desc;
  CM_OBJECT_TOKEN                   *TokenMap;

  TokenMap                      = NULL;
  NumEnclosures                 = 0;
  ProductInfoSize               = sizeof (ProductInfo);
  EnclosureInfo                 = NULL;
  Type3RecordPcd                = (SMBIOS_TABLE_TYPE3 *)PcdGetPtr (PcdType3Info);
  AssetTagLenWithNullTerminator = 0;

  Status = gRT->GetVariable (ProductInfoVariableName, &gNVIDIAPublicVariableGuid, NULL, &ProductInfoSize, &ProductInfo);
  if (!EFI_ERROR (Status)) {
    AssetTagLenWithNullTerminator = StrSize (ProductInfo.ChassisAssetTag);
  }

  for (Type3Index = 0; Type3Index < MAX_TYPE3_COUNT; Type3Index++) {
    //
    // Get enclosure info from FRU data
    // '/firmware/smbios/type3/fru-desc' is required to specify which FRU is used
    //
    AsciiSPrint (Type3NodeStr, sizeof (Type3NodeStr), "/firmware/smbios/type3@%u", Type3Index);
    NodeOffset = fdt_path_offset (DtbBase, Type3NodeStr);
    if (NodeOffset < 0) {
      break;
    }

    //
    // Check if there are overrides from FRU device.
    //
    Type3FruInfo = NULL;
    Status       = GetFruDataType3 (Private, DtbBase, NodeOffset, "fru-desc", &Type3FruInfo);

    if ((Status == EFI_SUCCESS) && (Type3FruInfo != NULL)) {
      if (Type3FruInfo->ChassisSerial != NULL) {
        SerialNumberStr = Type3FruInfo->ChassisSerial;
      } else {
        SerialNumberUniStr = (CHAR16 *)PcdGetPtr (PcdChassisSerialNumber);
        if (StrLen (SerialNumberUniStr) > 0) {
          SerialNumberStr = (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (StrLen (SerialNumberUniStr) +1));
          if (SerialNumberStr != NULL) {
            UnicodeStrToAsciiStrS (SerialNumberUniStr, SerialNumberStr, StrLen (SerialNumberUniStr) +1);
          }
        }
      }

      if (Type3FruInfo->ChassisPartNum != NULL) {
        SkuNumberStr = Type3FruInfo->ChassisPartNum;
      } else {
        SkuNumberUniStr = (CHAR16 *)PcdGetPtr (PcdChassisSku);
        if (StrLen (SkuNumberUniStr) > 0) {
          SkuNumberStr = (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (StrLen (SkuNumberUniStr) +1));
          if (SkuNumberStr != NULL) {
            UnicodeStrToAsciiStrS (SkuNumberUniStr, SkuNumberStr, StrLen (SkuNumberUniStr) +1);
          }
        }
      }

      if (Type3FruInfo->ChassisType != 0) {
        ChassisType = Type3FruInfo->ChassisType;
      } else {
        ChassisType = Type3RecordPcd->Type;
      }

      ManufacturerStr = Type3FruInfo->ProductManufacturer;
      VersionStr      = Type3FruInfo->ProductVersion;
    } else {
      continue;
    }

    EnclosureInfo = ReallocatePool (
                      sizeof (CM_SMBIOS_ENCLOSURE_INFO) * (NumEnclosures),
                      sizeof (CM_SMBIOS_ENCLOSURE_INFO) * (NumEnclosures + 1),
                      EnclosureInfo
                      );
    if (EnclosureInfo == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate enclosure info\r\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    Private->EnclosureBaseboardBinding.Info = ReallocatePool (
                                                sizeof (*Private->EnclosureBaseboardBinding.Info) * NumEnclosures,
                                                sizeof (*Private->EnclosureBaseboardBinding.Info) * (NumEnclosures + 1),
                                                Private->EnclosureBaseboardBinding.Info
                                                );
    if (Private->EnclosureBaseboardBinding.Info == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate enclosure baseboard binding info\r\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    //
    // Check if there are OEM overrides from DTB for contained elements.
    //
    ContainedElementCount = 0;
    ContainedElements     = NULL;
    for (Index = 0; Index < MAX_TYPE3_CONTAINED_ELEMENT_COUNT; Index++) {
      AsciiSPrint (DtContainedElementFormat, sizeof (DtContainedElementFormat), "%a/contained-element@%u", Type3NodeStr, Index);
      Type3ContainedElementOffset = fdt_path_offset (DtbBase, DtContainedElementFormat);
      if (Type3ContainedElementOffset < 0) {
        DEBUG ((DEBUG_INFO, "%a: SMBIOS Type 3 enclosure[%u] contained element count = %u.\n", __FUNCTION__, Type3Index, ContainedElementCount));
        break;
      } else {
        ContainedElements = ReallocatePool (
                              sizeof (CONTAINED_ELEMENT) * (ContainedElementCount),
                              sizeof (CONTAINED_ELEMENT) * (ContainedElementCount + 1),
                              ContainedElements
                              );
        if (ContainedElements != NULL) {
          ContainedElementCount++;

          Property = fdt_getprop (DtbBase, Type3ContainedElementOffset, "type", &Length);
          if (Property != NULL) {
            ContainedElements[Index].ContainedElementType = (UINT8)fdt32_to_cpu (*((UINT32 *)Property));
          }

          Property = fdt_getprop (DtbBase, Type3ContainedElementOffset, "minimum", &Length);
          if (Property != NULL) {
            ContainedElements[Index].ContainedElementMinimum = (UINT8)fdt32_to_cpu (*((UINT32 *)Property));
          }

          Property = fdt_getprop (DtbBase, Type3ContainedElementOffset, "maximum", &Length);
          if (Property != NULL) {
            ContainedElements[Index].ContainedElementMaximum = (UINT8)fdt32_to_cpu (*((UINT32 *)Property));
          }
        }
      }
    }

    //
    // Check if there are OEM overrides from DTB for chassis power cords.
    //
    Property = fdt_getprop (DtbBase, NodeOffset, "number-of-power-cords", &Length);
    if (Property != NULL) {
      EnclosureInfo[NumEnclosures].NumberofPowerCords = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
    }

    //
    // Check if there are OEM overrides from DTB for chassis height.
    //
    Property = fdt_getprop (DtbBase, NodeOffset, "height", &Length);
    if (Property != NULL) {
      EnclosureInfo[NumEnclosures].Height = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
    }

    //
    // Check if there are OEM overrides from DTB for oem defined data field.
    //
    Property = fdt_getprop (DtbBase, NodeOffset, "oem-defined", &Length);
    if (Property != NULL) {
      *(UINT32 *)&EnclosureInfo[NumEnclosures].OemDefined[0] = fdt32_to_cpu (*(UINT32 *)Property);
    }

    //
    // Update chassis state by Bmc Ipmi chassis status.
    //
    DataSize = sizeof (ChassisStatusResponse);
    Status   = IpmiSubmitCommand (
                 IPMI_NETFN_CHASSIS,
                 IPMI_CHASSIS_GET_STATUS,
                 NULL,
                 0,
                 (VOID *)&ChassisStatusResponse,
                 &DataSize
                 );
    if (!EFI_ERROR (Status) && (ChassisStatusResponse.CompletionCode == IPMI_COMP_CODE_NORMAL)) {
      if ((ChassisStatusResponse.LastPowerEvent &
           (LAST_POWER_EVENT_AC_FAILED |
            LAST_POWER_EVENT_POWER_OVERLOAD |
            LAST_POWER_EVENT_POWER_INTERLOCK |
            LAST_POWER_EVENT_POWER_FAULT)) == 0)
      {
        EnclosureInfo[NumEnclosures].PowerSupplyState = ChassisStateSafe;
      } else {
        EnclosureInfo[NumEnclosures].PowerSupplyState = ChassisStateCritical;
      }

      if ((ChassisStatusResponse.LastPowerEvent &
           MISC_CHASSIS_STATE_THERMAL_FAULT_DETECTED) == 0)
      {
        EnclosureInfo[NumEnclosures].ThermalState = ChassisStateSafe;
      } else {
        EnclosureInfo[NumEnclosures].ThermalState = ChassisStateCritical;
      }
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get chassis status - %r\r\n", __FUNCTION__, Status));
      EnclosureInfo[NumEnclosures].PowerSupplyState = Type3RecordPcd->PowerSupplyState;
      EnclosureInfo[NumEnclosures].ThermalState     = Type3RecordPcd->ThermalState;
    }

    //
    //  Chassis boot state is the most serious state between the PSU and thermal state.
    //
    if (EnclosureInfo[NumEnclosures].ThermalState > EnclosureInfo[NumEnclosures].PowerSupplyState) {
      EnclosureInfo[NumEnclosures].BootupState = EnclosureInfo[NumEnclosures].ThermalState;
    } else {
      EnclosureInfo[NumEnclosures].BootupState = EnclosureInfo[NumEnclosures].PowerSupplyState;
    }

    //
    // Security setting for external input, for example, a keyboard, to a system.
    //
    EnclosureInfo[NumEnclosures].SecurityStatus = Type3RecordPcd->SecurityStatus;

    //
    // Get asset tag info from UEFI variable.
    //
    AssetTagStr = NULL;

    if ((ChassisType != MiscChassisBladeEnclosure) && (AssetTagLenWithNullTerminator != 0)) {
      AssetTagStr = AllocateZeroPool (AssetTagLenWithNullTerminator);
      if (AssetTagStr != NULL) {
        UnicodeStrToAsciiStrS (ProductInfo.ChassisAssetTag, AssetTagStr, AssetTagLenWithNullTerminator);
      }
    }

    //
    // Copy strings to CM object
    //
    EnclosureInfo[NumEnclosures].Manufacturer                             = AllocateCopyString (ManufacturerStr);
    EnclosureInfo[NumEnclosures].Version                                  = AllocateCopyString (VersionStr);
    EnclosureInfo[NumEnclosures].SerialNum                                = AllocateCopyString (SerialNumberStr);
    EnclosureInfo[NumEnclosures].SkuNum                                   = AllocateCopyString (SkuNumberStr);
    EnclosureInfo[NumEnclosures].AssetTag                                 = AssetTagStr;
    EnclosureInfo[NumEnclosures].Type                                     = ChassisType;
    EnclosureInfo[NumEnclosures].ContainedElements                        = (CONTAINED_ELEMENT *)ContainedElements;
    EnclosureInfo[NumEnclosures].ContainedElementRecordLength             = sizeof (CONTAINED_ELEMENT);
    EnclosureInfo[NumEnclosures].ContainedElementCount                    = ContainedElementCount;
    Private->EnclosureBaseboardBinding.Info[NumEnclosures].ChassisCmToken = EnclosureInfo[NumEnclosures].EnclosureInfoToken;
    Private->EnclosureBaseboardBinding.Info[NumEnclosures].FruDeviceId    = Type3FruInfo->FruDeviceId;

    NumEnclosures++;
  }

  if (NumEnclosures == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS Type 3 not found.\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  Private->EnclosureBaseboardBinding.Count = NumEnclosures;
  DEBUG (
    (DEBUG_INFO, "%a: NumEnclosures = %u\n", __FUNCTION__, NumEnclosures)
    );

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, NumEnclosures, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 3: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumEnclosures; Index++) {
    EnclosureInfo[Index].EnclosureInfoToken =   TokenMap[Index];
  }

  //
  // Install CM object for type 3
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjEnclosureInfo);
  Desc.Size     = NumEnclosures * sizeof (CM_SMBIOS_ENCLOSURE_INFO);
  Desc.Count    = NumEnclosures;
  Desc.Data     = EnclosureInfo;

  Status = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 3 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  //
  // Add type 3 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType3,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (EnclosureInfo);
  return Status;
}
