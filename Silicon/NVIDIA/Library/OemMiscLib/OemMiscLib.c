/** @file
*  OemMiscLib.c
*
*  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#include <Uefi.h>
#include <string.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/OemMiscLib.h>
#include <Library/PrintLib.h>
#include <Library/SmbiosData.h>
#include <Library/UefiBootServicesTableLib.h>

static SmbiosMiscData *SmMiscData;
static SmbiosCpuData *SmCpuData;
SMBIOS_TABLE_TYPE32   *Type32Record;
SMBIOS_TABLE_TYPE3    *Type3Record;

/** Gets the CPU frequency of the specified processor.

  @param ProcessorIndex Index of the processor to get the frequency for.

  @return               CPU frequency in Hz
**/
UINTN
EFIAPI
OemGetCpuFreq (
  IN UINT8 ProcessorIndex
  )
{
  UINTN CurSpeed = 0;

  if (SmCpuData) {
    CurSpeed = SmCpuData->CpuData.CurrentSpeed;
  }
  return CurSpeed;
}

/** Gets information about the specified processor and stores it in
    the structures provided.

  @param ProcessorIndex  Index of the processor to get the information for.
  @param ProcessorStatus Processor status.
  @param ProcessorCharacteristics Processor characteritics.
  @param MiscProcessorData        Miscellaneous processor information.

  @return  TRUE on success, FALSE on failure.
**/
BOOLEAN
EFIAPI
OemGetProcessorInformation (
  IN UINTN ProcessorIndex,
  IN OUT PROCESSOR_STATUS_DATA *ProcessorStatus,
  IN OUT PROCESSOR_CHARACTERISTIC_FLAGS *ProcessorCharacteristics,
  IN OUT OEM_MISC_PROCESSOR_DATA *MiscProcessorData
  )
{
  UINT16 ProcessorCount;

  if (SmCpuData)
  {
    ProcessorCount = SmCpuData->CpuData.CoresEnabled;

    if (ProcessorIndex < ProcessorCount) {
      ProcessorStatus->Bits.CpuStatus       = 1; // CPU enabled
      ProcessorStatus->Bits.Reserved1       = 0;
      ProcessorStatus->Bits.SocketPopulated = 1;
      ProcessorStatus->Bits.Reserved2       = 0;
    } else {
      ProcessorStatus->Bits.CpuStatus       = 0; // CPU disabled
      ProcessorStatus->Bits.Reserved1       = 0;
      ProcessorStatus->Bits.SocketPopulated = 0;
      ProcessorStatus->Bits.Reserved2       = 0;
    }

    memcpy(ProcessorCharacteristics, &SmCpuData->CpuCapability,
           sizeof(PROCESSOR_CHARACTERISTIC_FLAGS));
    memcpy(MiscProcessorData, &SmCpuData->CpuData,
           sizeof(OEM_MISC_PROCESSOR_DATA));
  }
  return TRUE;
}

/** Gets information about the cache at the specified cache level.

  @param ProcessorIndex The processor to get information for.
  @param CacheLevel The cache level to get information for.
  @param DataCache  Whether the cache is a data cache.
  @param UnifiedCache Whether the cache is a unified cache.
  @param SmbiosCacheTable The SMBIOS Type7 cache information structure.

  @return TRUE on success, FALSE on failure.
**/
BOOLEAN
EFIAPI
OemGetCacheInformation (
  IN UINT8   ProcessorIndex,
  IN UINT8   CacheLevel,
  IN BOOLEAN DataCache,
  IN BOOLEAN UnifiedCache,
  IN OUT SMBIOS_TABLE_TYPE7 *SmbiosCacheTable
  )
{
  UINT8 CacheDataIdx = 0;
  UINT32 WritePolicy;

  SmbiosCacheTable->CacheConfiguration = CacheLevel - 1;
  // Unknown operational mode
  if (!(SmCpuData && SmCpuData->CacheData))
  {
    SmbiosCacheTable->CacheConfiguration |= (3 << 8);
  }
  else
  {
    CacheDataIdx = SmCpuData->NumCacheLevels - CacheLevel -1;
    if (CacheDataIdx > SmCpuData->NumCacheLevels) {
      SmbiosCacheTable->CacheConfiguration |= (3 << 8);
    } else {
      WritePolicy = SmCpuData->CacheData[CacheDataIdx].Attributes;
      WritePolicy = (WritePolicy >> 4) & 0x1;
      if (WritePolicy == EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_THROUGH)
        SmbiosCacheTable->CacheConfiguration |= (0 << 8);
      else if (WritePolicy == EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK)
        SmbiosCacheTable->CacheConfiguration |= (1 << 8);
      else
        SmbiosCacheTable->CacheConfiguration |= (3 << 8);
    }
  }
  return TRUE;
}

/** Gets the maximum number of processors supported by the platform.

  @return The maximum number of processors.
**/
UINT8
EFIAPI
OemGetMaxProcessors (
  VOID
  )
{
  UINT8 CoresEnabled = 0;
  if (SmCpuData) {
    CoresEnabled = SmCpuData->CpuData.CoresEnabled;
  }
  return CoresEnabled;
}

/** Gets the type of chassis for the system.

  @retval The type of the chassis.
**/
MISC_CHASSIS_TYPE
EFIAPI
OemGetChassisType (
  VOID
  )
{
  return MiscChassisTypeUnknown;
}

/** Returns whether the specified processor is present or not.

  @param ProcessIndex The processor index to check.

  @return TRUE is the processor is present, FALSE otherwise.
**/
BOOLEAN
EFIAPI
OemIsProcessorPresent (
  IN UINTN ProcessorIndex
  )
{
  if (SmCpuData && (ProcessorIndex < SmCpuData->CpuData.CoresEnabled)) {
    return TRUE;
  }

  return FALSE;
}

/** Updates the HII string for the specified field.

  @param HiiHandle     The HII handle.
  @param TokenToUpdate The string to update.
  @param Field         The field to get information about.
**/
VOID
EFIAPI
OemUpdateSmbiosInfo (
  IN EFI_HII_HANDLE HiiHandle,
  IN EFI_STRING_ID TokenToUpdate,
  IN OEM_MISC_SMBIOS_HII_STRING_FIELD Field
  )
{
  CHAR16* HiiString = NULL
;
  switch (Field) {
    case SystemManufacturerType01:
      HiiString = (CHAR16 *) PcdGetPtr (PcdSystemManufacturer);
      break;
    case FamilyType01:
      HiiString = (CHAR16 *) PcdGetPtr (PcdSystemFamilyType);
      break;
    case SerialNumType01:
      HiiString = (CHAR16 *) PcdGetPtr (PcdSystemSerialNum);
      break;
    case SkuNumberType01:
      HiiString = (CHAR16 *) PcdGetPtr (PcdSystemSku);
      break;
    case AssertTagType02:
      if (SmMiscData) {
        HiiString = SmMiscData->BoardAssetTag;
      }
      break;
    case ChassisLocationType02:
      HiiString = (CHAR16 *) PcdGetPtr (PcdBoardChassisLocation);
      break;
    case BoardManufacturerType02:
      HiiString = (CHAR16 *) PcdGetPtr (PcdBoardManufacturer);
      break;
    case SerialNumberType02:
      if (SmMiscData) {
        HiiString = SmMiscData->BoardSerialNumber;
      }
      break;
    case VersionType03:
      HiiString = (CHAR16 *) PcdGetPtr (PcdChassisVersion);
      break;
    case ManufacturerType03:
      HiiString = (CHAR16 *) PcdGetPtr (PcdChassisManufacturer);
      break;
    case AssetTagType03:
      HiiString = (CHAR16 *) PcdGetPtr (PcdChassisAssetTag);
      break;
    case SkuNumberType03:
      HiiString = (CHAR16 *) PcdGetPtr (PcdChassisSku);
      break;
    case SerialNumberType03:
      HiiString = (CHAR16 *) PcdGetPtr (PcdChassisSerialNumber);
      break;
    default:
      break;
  }
  if (HiiString != NULL) {
    HiiSetString (HiiHandle, TokenToUpdate, HiiString, NULL);
  }
}

/** Fetches the Type 32 boot information status.

  @return Boot status.
**/
MISC_BOOT_INFORMATION_STATUS_DATA_TYPE
EFIAPI
OemGetBootStatus (
  VOID
  )
{
  if (Type32Record) {
    return Type32Record->BootStatus;
  } else {
    return BootInformationStatusNoError;
  }
}

/** Fetches the chassis status when it was last booted.

 @return Chassis status.
**/
MISC_CHASSIS_STATE
EFIAPI
OemGetChassisBootupState (
  VOID
  )
{
  if (Type3Record) {
    return Type3Record->BootupState;
  } else {
    return ChassisStateUnknown;
  }
}

/** Fetches the chassis power supply/supplies status when last booted.

 @return Chassis power supply/supplies status.
**/
MISC_CHASSIS_STATE
EFIAPI
OemGetChassisPowerSupplyState (
  VOID
  )
{
  if (Type3Record) {
    return Type3Record->PowerSupplyState;
  } else {
    return ChassisStateUnknown;
  }
}

/** Fetches the chassis thermal status when last booted.

 @return Chassis thermal status.
**/
MISC_CHASSIS_STATE
EFIAPI
OemGetChassisThermalState (
  VOID
  )
{
  if (Type3Record) {
    return Type3Record->PowerSupplyState;
  } else {
    return ChassisStateUnknown;
  }
}

/** Fetches the chassis security status when last booted.

 @return Chassis security status.
**/
MISC_CHASSIS_SECURITY_STATE
EFIAPI
OemGetChassisSecurityStatus (
  VOID
  )
{
  if (Type3Record) {
    return Type3Record->SecurityStatus;
  } else {
    return ChassisSecurityStatusUnknown;
  }
}

/** Fetches the chassis height in RMUs (Rack Mount Units).

  @return The height of the chassis.
**/
UINT8
EFIAPI
OemGetChassisHeight (
  VOID
  )
{
  if (Type3Record) {
    return Type3Record->Height;
  } else {
    return 0;
  }
}

/** Fetches the number of power cords.

  @return The number of power cords.
**/
UINT8
EFIAPI
OemGetChassisNumPowerCords (
  VOID
  )
{
  if (Type3Record) {
    return Type3Record->NumberofPowerCords;
  } else {
    return 0;
  }
}

/**
  The Constructor Function gets the Platform specific data which is installed
  by SOC specific Libraries

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/

EFI_STATUS
EFIAPI
OemMiscLibConstructor (
    VOID
  )
{
    EFI_STATUS Status;

    Status = gBS->LocateProtocol (&gNVIDIASmbiosMiscDataProtocolGuid, NULL, (VOID **)&SmMiscData);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: SMBIOS: Failed to get Board Data protocol %r\n",
                            __FUNCTION__, Status));
      SmMiscData = NULL;
    }

    Status = gBS->LocateProtocol (&gNVIDIASmbiosCpuDataProtocolGuid, NULL, (VOID **)&SmCpuData);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: SMBIOS: Failed to get CPU Data protocol %r\n",
                             __FUNCTION__, Status));
      SmCpuData = NULL;
    }
    Type32Record = (SMBIOS_TABLE_TYPE32 *)PcdGetPtr (PcdType32Info);
    Type3Record = (SMBIOS_TABLE_TYPE3 *)PcdGetPtr (PcdType3Info);
    return EFI_SUCCESS;
}
