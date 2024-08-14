/** @file
  Configuration Manager Data of SMBIOS Type 39 table.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FruLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

#include <IndustryStandard/Ipmi.h>

#include <ConfigurationManagerObject.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosType39Parser.h"

#define MAX_PSUS  8

#define PSU_NAME_STR  "PSU %u"
#define TYPE39_STR    "type39@%u"

#define PSU_MAX_PWR_UNKNOWN  0x8000

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType39 = {
  SMBIOS_TYPE_SYSTEM_POWER_SUPPLY,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType39),
  NULL
};

/**
  Install CM object for SMBIOS Type 39
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType39Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  VOID                              *DtbBase    = Private->DtbBase;
  CHAR8                             Type39Str[] = TYPE39_STR;
  INTN                              DtbOffset;
  CM_SMBIOS_POWER_SUPPLY_INFO       *PsuInfo;
  CONST VOID                        *Property;
  INT32                             Length;
  FRU_DEVICE_INFO                   *PsuFru;
  CHAR8                             *FruDesc[MAX_PSUS];
  CHAR8                             *PsuLoc[MAX_PSUS];
  UINT8                             PsuCount;
  UINT8                             Psu;
  SYS_POWER_SUPPLY_CHARACTERISTICS  *PsuChr;
  UINT8                             Index;
  CM_OBJ_DESCRIPTOR                 Desc;
  EFI_STATUS                        Status;
  CM_OBJECT_TOKEN                   *TokenMap;

  TokenMap = NULL;
  PsuInfo  = NULL;

  //
  // Get number of PSUs expected on the system
  //
  for (PsuCount = 0; PsuCount < MAX_PSUS; PsuCount++) {
    AsciiSPrint (Type39Str, sizeof (Type39Str), TYPE39_STR, PsuCount);
    DtbOffset = fdt_subnode_offset (DtbBase, Private->DtbSmbiosOffset, Type39Str);
    if (DtbOffset < 0) {
      break;
    }

    // '/firmware/smbios/type39@x/fru-desc' is required to specify which FRU is PSU FRU
    Property = fdt_getprop (DtbBase, DtbOffset, "fru-desc", &Length);
    if ((Property == NULL) || (Length == 0)) {
      DEBUG ((DEBUG_ERROR, "%a: DT property '%a/fru-desc' not found.\n", __FUNCTION__, Type39Str));
      break;
    }

    FruDesc[PsuCount] = (CHAR8 *)Property;

    Property = fdt_getprop (DtbBase, DtbOffset, "location", &Length);
    if ((Property != NULL) && (Length > 0)) {
      PsuLoc[PsuCount] = (CHAR8 *)Property;
    } else {
      PsuLoc[PsuCount] = NULL;
    }
  }

  if (PsuCount == 0) {
    DEBUG ((DEBUG_INFO, "%a: System does not have PSUs.\n", __FUNCTION__));
    return RETURN_NOT_FOUND;
  }

  //
  // Allocate and zero out PSU Info. The strings that are NULL will be set as "Unknown"
  //
  PsuInfo = (CM_SMBIOS_POWER_SUPPLY_INFO *)AllocateZeroPool (sizeof (CM_SMBIOS_POWER_SUPPLY_INFO) * PsuCount);
  if (PsuInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for PSU info\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  //
  // Populate PSU info for each table
  //
  for (Psu = 0; Psu < PsuCount; Psu++) {
    PsuChr = (SYS_POWER_SUPPLY_CHARACTERISTICS *)&PsuInfo[Psu].PowerSupplyCharacteristics;

    PsuChr->PowerSupplyHotReplaceable = 0;
    PsuChr->PowerSupplyPresent        = 0;
    PsuChr->PowerSupplyUnplugged      = 1;
    PsuChr->InputVoltageRangeSwitch   = PowerSupplyIvrsUnknown;
    PsuChr->PowerSupplyStatus         = PowerSupplyStatusUnknown;
    PsuChr->PowerSupplyType           = PowerSupplyTypeSwitching;

    PsuInfo[Psu].DeviceName = AllocatePool (sizeof (PSU_NAME_STR));
    if (PsuInfo[Psu].DeviceName != NULL) {
      AsciiSPrint (PsuInfo[Psu].DeviceName, sizeof (PSU_NAME_STR), PSU_NAME_STR, Psu);
    }

    PsuInfo[Psu].PowerUnitGroup           = 1;
    PsuInfo[Psu].MaxPowerCapacity         = PSU_MAX_PWR_UNKNOWN;
    PsuInfo[Psu].InputVoltageProbeCmToken = CM_NULL_TOKEN;
    PsuInfo[Psu].CoolingDeviceCmToken     = CM_NULL_TOKEN;
    PsuInfo[Psu].InputCurrentProbeCmToken = CM_NULL_TOKEN;
    PsuInfo[Psu].Location                 = AllocateCopyString (PsuLoc[Psu]);

    PsuFru = FindFruByDescription (Private, FruDesc[Psu]);

    if (PsuFru != NULL) {
      // If FRU is present, PSU is installed
      PsuChr->PowerSupplyPresent   = 1;
      PsuChr->PowerSupplyUnplugged = 0;  // TODO: Update PSU status when BMC supports it

      PsuInfo[Psu].Manufacturer    = AllocateCopyString (PsuFru->ProductManufacturer);
      PsuInfo[Psu].SerialNumber    = AllocateCopyString (PsuFru->ProductSerial);
      PsuInfo[Psu].ModelPartNumber = AllocateCopyString (PsuFru->ProductPartNum);
      PsuInfo[Psu].RevisionLevel   = AllocateCopyString (PsuFru->ProductVersion);
      PsuInfo[Psu].AssetTagNumber  = AllocateCopyString (PsuFru->ProductAssetTag);

      for (Index = 0; Index < MAX_FRU_MULTI_RECORDS; Index++) {
        if (PsuFru->MultiRecords[Index] == NULL) {
          break;
        }

        if (PsuFru->MultiRecords[Index]->Header.Type == FRU_MULTI_RECORD_TYPE_POWER_SUPPLY_INFO) {
          PsuInfo[Psu].MaxPowerCapacity     = PsuFru->MultiRecords[Index]->PsuInfo.Capacity;
          PsuChr->PowerSupplyHotReplaceable = PsuFru->MultiRecords[Index]->PsuInfo.HotSwap;
          if (PsuFru->MultiRecords[Index]->PsuInfo.AutoSwitch) {
            PsuChr->InputVoltageRangeSwitch = PowerSupplyIvrsAuto;
          }
        }
      }
    }
  }

  if (Psu == 0) {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "%a: Failed to get data for Psus\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, Psu, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 39: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < Psu; Index++) {
    PsuInfo[Index].PowerSupplyInfoToken = TokenMap[Index];
  }

  //
  // Install CM object for type 39
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjPowerSupplyInfo);
  Desc.Size     = sizeof (CM_SMBIOS_POWER_SUPPLY_INFO) * Psu;
  Desc.Count    = Psu;
  Desc.Data     = PsuInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 39 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  //
  // Add type 39 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType39,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (PsuInfo);
  return Status;
}
