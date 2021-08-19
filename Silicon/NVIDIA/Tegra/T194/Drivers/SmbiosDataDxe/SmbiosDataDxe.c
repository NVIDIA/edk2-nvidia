/** @file
This module installs the SMBIOS data tables for T194.

Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <ConfigurationManagerObject.h>
#include <Uefi.h>
#include <string.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/SmbiosData.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/TimerLib.h>
#include <Library/TegraCpuFreqHelper.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Protocol/ConfigurationManagerProtocol.h>
#include <Protocol/Eeprom.h>

#define LOWER_32(x)  ((UINT32)x)
#define UPPER_32(x)  ((UINT32)(x >> 32))
#define MAX_CNTR     (~0U)
#define REF_CLK_MHZ  (408)

STATIC SmbiosMiscData  *SmMiscData;
STATIC SmbiosCpuData   *SmbCpuData;

/**
  NvGetCpuFreqHz
  Helper function to get the CPU frequency. This could be part of the generic
  OemMiscLib if we can use the generic ARM PMUs to compute this value.
  For now keep this SOC specific.

  @retval       The Current CPU Frequency in HZ.

**/
STATIC UINT16 NvGetCpuFreqMhz ( VOID  )
{
  EFI_TPL  CurrentTpl;
  UINT64   BeginValue;
  UINT64   EndValue;
  UINT32   BeginCcnt;
  UINT32   BeginRefCnt;
  UINT32   EndCcnt;
  UINT32   EndRefCnt;
  UINT32   DeltaRefCnt;
  UINT32   DeltaCcnt;
  UINT32   FreqMHz;
  UINT16   ReturnFreq;


  CurrentTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);
  BeginValue = NvReadPmCntr();
  MicroSecondDelay (100);
  EndValue = NvReadPmCntr();
  gBS->RestoreTPL (CurrentTpl);

  BeginRefCnt = LOWER_32 (BeginValue);
  BeginCcnt = UPPER_32 (BeginValue);
  EndRefCnt = LOWER_32 (EndValue);
  EndCcnt = UPPER_32 (EndValue);

  if (EndRefCnt < BeginRefCnt) {
    DeltaRefCnt = EndRefCnt + (MAX_CNTR - BeginRefCnt);
  }  else {
    DeltaRefCnt = EndRefCnt - BeginRefCnt;
  }

  if (EndCcnt < BeginCcnt) {
    DeltaCcnt = EndCcnt + (MAX_CNTR - BeginCcnt);
  }  else {
    DeltaCcnt = EndCcnt - BeginCcnt;
  }

  FreqMHz = (DeltaCcnt * REF_CLK_MHZ) / DeltaRefCnt;
  ReturnFreq = (UINT16) FreqMHz;
  return ReturnFreq;
}

/**
  PopulateCpuCharData
  Helper function to populate the CPU characteristics data. For
  now most of these are left hard-coded.

  @param ProcessorCharacteristics  Pointer to the Processor Characteristics
                                   data structure that will be used by the
                                   Smbios drivers.

  @retval                        NONE

**/
STATIC void PopulateCpuCharData(
            PROCESSOR_CHARACTERISTIC_FLAGS *ProcessorCharacteristics
            )
{
  ProcessorCharacteristics->ProcessorReserved1      = 0;
  ProcessorCharacteristics->ProcessorUnknown        = 0;
  ProcessorCharacteristics->Processor64BitCapable   = 1;
  ProcessorCharacteristics->ProcessorMultiCore      = 0;
  ProcessorCharacteristics->ProcessorHardwareThread = 0;
  ProcessorCharacteristics->ProcessorExecuteProtection      = 1;
  ProcessorCharacteristics->ProcessorEnhancedVirtualization = 0;
  ProcessorCharacteristics->ProcessorPowerPerformanceCtrl   = 0;
  ProcessorCharacteristics->Processor128BitCapable = 0;
  ProcessorCharacteristics->ProcessorArm64SocId = 1;
  ProcessorCharacteristics->ProcessorReserved2  = 0;
}

/**
  PopulateCpuData
  Helper Function to get CPU/Core data.The CoreCount/Enabled Cores count
  is obtained using the Floorsweeping Library.

  @param MiscProcessorData      Pointer to the Processor Data structure which
                                is used by the Smbios drivers.
                                it should be ignored by device driver.

  @retval 		        NONE

**/
STATIC void PopulateCpuData(
  OEM_MISC_PROCESSOR_DATA *MiscProcessorData
)
{
  UINT16 CpuSpeedMhz = NvGetCpuFreqMhz();

  MiscProcessorData->CurrentSpeed = CpuSpeedMhz;
  MiscProcessorData->MaxSpeed     = CpuSpeedMhz;
  MiscProcessorData->CoreCount    = GetNumberOfEnabledCpuCores();
  MiscProcessorData->CoresEnabled = GetNumberOfEnabledCpuCores();
  MiscProcessorData->ThreadCount  = 1;
}

/**
  PopulateCacheData
  Helper function to populate the cache data which is obtained from the Config
  Manager Data.

  @param CpuData                Pointer to the Cpu data structure which
                                includes Cache data.

  @retval 		        NONE

**/
STATIC void PopulateCacheData (
  SmbiosCpuData *CpuData
)
{
  EFI_STATUS                                Status;
  EDKII_CONFIGURATION_MANAGER_PROTOCOL     *CfgMgrProtocol;
  CM_OBJ_DESCRIPTOR                        CmObjDesc;

  Status = gBS->LocateProtocol (
                  &gEdkiiConfigurationManagerProtocolGuid,
                  NULL,
                  (VOID**)&CfgMgrProtocol
                  );
  if ( EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "%a:Failed to Locate Config Manager Protocol: %r",
                          __FUNCTION__, Status));
  } else {
    Status = CfgMgrProtocol->GetObject (
                       CfgMgrProtocol,
                       CREATE_CM_ARM_OBJECT_ID(EArmObjCacheInfo),
                       CM_NULL_TOKEN,
                       &CmObjDesc
                       );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Failed to Get Cache Info. Status = %r\n",
         Status
        ));
      CpuData->CacheData = NULL;
    } else {
      CpuData->NumCacheLevels = CmObjDesc.Count;
      CpuData->CacheData = (CM_ARM_CACHE_INFO *)CmObjDesc.Data;
    }
  }
}

/**
  Helper Function to populate board specific data obtained from the
  EEPROM.

  @param BoardData              Pointer to Board Specific data

  @retval EFI_SUCCESS           Found and populated the board data.
  @retval EFI_OUT_OF_RESOURCES  Couldn't allocate a buffer for the Board
                                Specific data.

**/
STATIC
EFI_STATUS
PopulateMiscData (
  SmbiosMiscData *MiscData
)
{
  T194_EEPROM_DATA       *T194CvmEeprom;
  UINT8                  *CvmEeprom;
  EFI_STATUS              Status;
  INTN                    SnSize;
  INTN                    AssetTagSize;
  INTN                    SkuSize;
  INTN                    VersionSize;
  INTN                    c;
  INTN                    CharCount;

  Status = gBS->LocateProtocol (&gNVIDIACvmEepromProtocolGuid, NULL, (VOID **)&CvmEeprom);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Failed to get eeprom protocol %r \n",
                          __FUNCTION__, Status));
    goto Error;
  } else {
    T194CvmEeprom = (T194_EEPROM_DATA *) CvmEeprom;

    /* Type2 Table Data */
    SkuSize = (sizeof (T194CvmEeprom->Sku) + 1) * sizeof (CHAR16);
    MiscData->BoardSku = AllocateZeroPool (SkuSize);
    if (MiscData->BoardSku == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Error;
    }
    UnicodeSPrint(MiscData->BoardSku,
                  SkuSize,
                  L"%x", T194CvmEeprom->Sku);

    VersionSize = (sizeof (T194CvmEeprom->Version) + 1) * sizeof (CHAR16);
    MiscData->BoardVersion = AllocateZeroPool (VersionSize);
    if (MiscData->BoardVersion == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Error;
    }

    UnicodeSPrint(MiscData->BoardVersion,
                  VersionSize,
                  L"%u", T194CvmEeprom->Version);

    SnSize = (sizeof (T194CvmEeprom->SerialNumber) + 1) * sizeof(CHAR16);
    MiscData->BoardSerialNumber = AllocateZeroPool (SnSize);
    if (MiscData->BoardSerialNumber == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Error;
    }

    for (c = 0; c <  sizeof (T194CvmEeprom->SerialNumber); c++) {
      UnicodeSPrint(&MiscData->BoardSerialNumber[c],
                    SnSize,
                    L"%c", T194CvmEeprom->SerialNumber[c]);
    }

    AssetTagSize = (sizeof(TEGRA_EEPROM_PART_NUMBER) + 1) * sizeof(CHAR16);
    MiscData->BoardAssetTag = AllocateZeroPool (AssetTagSize);
    if (MiscData->BoardAssetTag == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Error;
    }

    CharCount = 0;
    for (c = 0; c <  sizeof (T194CvmEeprom->PartNumber.Leading); c++) {
      UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                    AssetTagSize,
                    L"%c", T194CvmEeprom->PartNumber.Leading[c]);
    }

    AssetTagSize -= (c * sizeof (CHAR16));
    UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                  AssetTagSize,
                  L"%c", T194CvmEeprom->PartNumber.Separator0);

    AssetTagSize -= sizeof (CHAR16);
    UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                  AssetTagSize,
                  L"%c", T194CvmEeprom->PartNumber.Class);

    AssetTagSize -= sizeof (CHAR16);
    for (c = 0; c <  sizeof (T194CvmEeprom->PartNumber.Id); c++) {
      UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                    AssetTagSize,
                    L"%c", T194CvmEeprom->PartNumber.Id[c]);
    }

    AssetTagSize -= (c * sizeof (CHAR16));
    UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                  AssetTagSize,
                  L"%c", T194CvmEeprom->PartNumber.Separator1);

    AssetTagSize -= sizeof (CHAR16);
    for (c = 0; c <  sizeof (T194CvmEeprom->PartNumber.Sku); c++) {
      UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                    AssetTagSize,
                    L"%c", T194CvmEeprom->PartNumber.Sku[c]);
    }

    AssetTagSize -= (c * sizeof (CHAR16));
    UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                  AssetTagSize,
                  L"%c", T194CvmEeprom->PartNumber.Separator2);

    AssetTagSize -= sizeof (CHAR16);
    for (c = 0; c <  sizeof (T194CvmEeprom->PartNumber.Fab); c++) {
      UnicodeSPrint (&MiscData->BoardAssetTag[CharCount++],
                    AssetTagSize,
                    L"%c", T194CvmEeprom->PartNumber.Fab[c]);
    }

    AssetTagSize -= (c * sizeof (CHAR16));
    UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                  AssetTagSize,
                  L"%c", T194CvmEeprom->PartNumber.Separator3);

    AssetTagSize -= sizeof (CHAR16);

    UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                  AssetTagSize,
                  L"%c", T194CvmEeprom->PartNumber.Revision);
    AssetTagSize -= sizeof (CHAR16);
    UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                  AssetTagSize,
                  L"%c", T194CvmEeprom->PartNumber.Separator4);

    AssetTagSize -= sizeof (CHAR16);
    UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                  AssetTagSize,
                  L"%c", T194CvmEeprom->PartNumber.Ending);

    AssetTagSize -= sizeof (CHAR16);
    for (c = 0; c <  sizeof (T194CvmEeprom->PartNumber.Pad); c++) {
      UnicodeSPrint(&MiscData->BoardAssetTag[CharCount++],
                    AssetTagSize,
                    L"%c", T194CvmEeprom->PartNumber.Pad[c]);
    }
    AssetTagSize -= (c * sizeof (CHAR16));
  }
  return Status;
Error:
  if (MiscData->BoardSku != NULL) {
    FreePool (MiscData->BoardSku);
  }
  if (MiscData->BoardVersion != NULL) {
    FreePool (MiscData->BoardVersion);
  }
  if (MiscData->BoardSerialNumber != NULL) {
    FreePool (MiscData->BoardSerialNumber);
  }
  if (MiscData->BoardAssetTag != NULL) {
    FreePool (MiscData->BoardAssetTag);
  }
  return Status;
}


/**
  SmbiosDataDxeEntryPoint
  Constructor for the Smbios Data Lib that populates the SOC specific data
  for the SMBIOS tables and installs the NV specific GUIDs that the OemMiscLib
  can use.

  @param ImageHandle            Of the Loaded Driver.
  @param SystemTable            Pointer to System Table.

  @retval EFI_SUCCESS           The destructor returns EFI_SUCCESS only.
**/
EFI_STATUS
EFIAPI
SmbiosDataDxeEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
)
{
  EFI_STATUS            Status;
  UINTN                 ChipID;

  ChipID = TegraGetChipID();
  if (ChipID == T194_CHIP_ID) {
    SmMiscData = (SmbiosMiscData *)AllocateZeroPool (sizeof (SmbiosMiscData));
    if (SmMiscData == NULL) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to allocate Smbios structure\r\n",
                             __FUNCTION__));
        return EFI_OUT_OF_RESOURCES;
    }
    PopulateMiscData(SmMiscData);
    Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                               &gNVIDIASmbiosMiscDataProtocolGuid,
                               SmMiscData,
                               NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to install EEPROM protocols\n", __FUNCTION__));
      return Status;
    }

    SmbCpuData = (SmbiosCpuData *)AllocateZeroPool (sizeof (SmbiosCpuData));
    if (SmbCpuData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate Smbios structure\r\n",
                          __FUNCTION__));
      return Status;
    }
    PopulateCpuData (&SmbCpuData->CpuData);
    PopulateCpuCharData (&SmbCpuData->CpuCapability);
    PopulateCacheData (SmbCpuData);

    Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                             &gNVIDIASmbiosCpuDataProtocolGuid,
                             SmbCpuData,
                             NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Install CPU data %d \n", Status));
      return Status;
    }
  }
  return EFI_SUCCESS;
}
