/** @file
*  OemMiscLib.c
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <ConfigurationManagerObject.h>
#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/OemMiscLib.h>
#include <Library/PrintLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraCpuFreqHelper.h>
#include <Library/FloorSweepingLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/TimerLib.h>
#include <Library/HobLib.h>
#include <Protocol/Eeprom.h>
#include <Protocol/EFuse.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Protocol/ConfigurationManagerProtocol.h>
#include <Protocol/TegraCpuFreq.h>
#include <libfdt.h>

#define HZ_TO_MHZ(x)   (x/1000000)
#define GENMASK_32(n)  (~(0U) >> (32 - n))

#define CPUSNMAXSTR  (320)

STATIC TEGRA_EEPROM_BOARD_INFO  *SmEepromData;
STATIC SMBIOS_TABLE_TYPE32      *Type32Record;
STATIC SMBIOS_TABLE_TYPE3       *Type3Record;
STATIC CHAR16                   *BoardProductName;
STATIC CHAR16                   *ProcessorVersion;
STATIC CHAR16                   *AssetTag;
STATIC CHAR16                   *SerialNumber;
STATIC UINT32                   SocketMask;

/**
  GetCpuFreqHz
  Get the Current and Max frequencies for a given socket.

  @param[in]  ProcessorId   SocketId to compute Frequency for.
  @param[out] CurFreqHz     Current Frequency in Hz.
  @param[out] MaxFreqHz     Max Frequency in Hz.

  @retval EFI_SUCCESS on Success
          OTHER       on Failure (from the TegraCpuFreq Protocol)

**/
STATIC
EFI_STATUS
GetCpuFreqHz (
  UINT8   ProcessorId,
  UINT64  *CurFreqHz,
  UINT64  *MaxFreqHz
  )
{
  NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *CpuFreq;
  UINTN                           LinearCoreId;
  UINT64                          MpIdr;
  EFI_STATUS                      Status;

  Status = gBS->LocateProtocol (
                  &gNVIDIATegraCpuFrequencyProtocolGuid,
                  NULL,
                  (VOID **)&CpuFreq
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Tegra Cpu Freq Protocol %r\n",
      __FUNCTION__,
      Status
      ));
    goto exitGetFreqMhz;
  }

  Status = GetFirstEnabledCoreOnSocket (ProcessorId, &LinearCoreId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get First Enabled core in Socket %u\n", __FUNCTION__, Status));
    goto exitGetFreqMhz;
  }

  MpIdr  = GetMpidrFromLinearCoreID (LinearCoreId);
  Status = CpuFreq->GetInfo (
                      CpuFreq,
                      MpIdr,
                      CurFreqHz,
                      MaxFreqHz,
                      NULL,
                      NULL,
                      NULL
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get CpuFrequency %r\n", Status));
    goto exitGetFreqMhz;
  }

exitGetFreqMhz:
  return Status;
}

/**
  GetCpuEnabledCores
  Get the Number of Enabled Cores for a socket.

  @param[in]  ProcessorId   SocketId to get enabled core count for.

  @retval Number of Enabled Cores (0 on failure)

**/
UINTN
EFIAPI
GetCpuEnabledCores (
  UINT8  ProcessorIndex
  )
{
  UINTN       EnabledCoreCount = 0;
  EFI_STATUS  Status;

  Status = GetNumEnabledCoresOnSocket (ProcessorIndex, &EnabledCoreCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get Enabled Core Count for Socket %u %r\n",
      __FUNCTION__,
      ProcessorIndex,
      Status
      ));
  }

  return EnabledCoreCount;
}

/**
  PopulateCpuData
  Helper Function to get CPU/Core data.The Enabled Cores count
  is obtained using the Floorsweeping Library.

  @param ProcessorIndex         Index of the processor to get the CpuData for.
  @param MiscProcessorData      Pointer to the Processor Data structure which
                                is used by the Smbios drivers.
                                it should be ignored by device driver.

  @retval                       NONE

**/
STATIC
VOID
PopulateCpuData (
  IN UINT8                 ProcessorIndex,
  OEM_MISC_PROCESSOR_DATA  *MiscProcessorData
  )
{
  UINTN   CoresEnabled;
  UINT64  CurFreqHz = 0;
  UINT64  MaxFreqHz = 0;
  UINTN   ChipId;

  EFI_STATUS  Status;

  Status = GetCpuFreqHz (ProcessorIndex, &CurFreqHz, &MaxFreqHz);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get CPUFreq %r\n", __FUNCTION__, Status));
    CurFreqHz = 0;
    MaxFreqHz = 0;
  }

  ChipId = TegraGetChipID ();
  switch (ChipId) {
    case TH500_CHIP_ID:
      MiscProcessorData->MaxSpeed = 0;
      break;
    default:
      MiscProcessorData->MaxSpeed = HZ_TO_MHZ (MaxFreqHz);
      break;
  }

  MiscProcessorData->CurrentSpeed = HZ_TO_MHZ (CurFreqHz);
  CoresEnabled                    = GetCpuEnabledCores (ProcessorIndex);
  MiscProcessorData->CoreCount    = CoresEnabled;
  MiscProcessorData->CoresEnabled = CoresEnabled;
  MiscProcessorData->ThreadCount  = CoresEnabled;
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
STATIC
VOID
PopulateCpuCharData (
  PROCESSOR_CHARACTERISTIC_FLAGS  *ProcessorCharacteristics
  )
{
  ProcessorCharacteristics->ProcessorReserved1              = 0;
  ProcessorCharacteristics->ProcessorUnknown                = 0;
  ProcessorCharacteristics->Processor64BitCapable           = 1;
  ProcessorCharacteristics->ProcessorMultiCore              = 0;
  ProcessorCharacteristics->ProcessorHardwareThread         = 0;
  ProcessorCharacteristics->ProcessorExecuteProtection      = 1;
  ProcessorCharacteristics->ProcessorEnhancedVirtualization = 0;
  ProcessorCharacteristics->ProcessorPowerPerformanceCtrl   = 0;
  ProcessorCharacteristics->Processor128BitCapable          = 0;
  ProcessorCharacteristics->ProcessorArm64SocId             = 1;
  ProcessorCharacteristics->ProcessorReserved2              = 0;
}

/**
  OemGetCpuFreq
  Gets the CPU frequency of the specified processor.

  @param ProcessorIndex Index of the processor to get the frequency for.

  @return               CPU frequency in Hz
**/
UINTN
EFIAPI
OemGetCpuFreq (
  IN UINT8  ProcessorIndex
  )
{
  UINT64      CurFreqHz;
  EFI_STATUS  Status;

  Status = GetCpuFreqHz (ProcessorIndex, &CurFreqHz, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get CpuFreq %r\n", __FUNCTION__, Status));
    CurFreqHz = 0;
  }

  return CurFreqHz;
}

/**
  OemGetProcessorInformation
  Gets information about the specified processor and stores it in
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
  IN UINTN                               ProcessorIndex,
  IN OUT PROCESSOR_STATUS_DATA           *ProcessorStatus,
  IN OUT PROCESSOR_CHARACTERISTIC_FLAGS  *ProcessorCharacteristics,
  IN OUT OEM_MISC_PROCESSOR_DATA         *MiscProcessorData
  )
{
  DEBUG ((DEBUG_INFO, "%a: ProcessorIndex %x ", __FUNCTION__, ProcessorIndex));

  if ((SocketMask & (1UL << ProcessorIndex)) != 0) {
    DEBUG ((DEBUG_INFO, "is enabled\n"));
    ProcessorStatus->Bits.CpuStatus       = 1; // CPU enabled
    ProcessorStatus->Bits.Reserved1       = 0;
    ProcessorStatus->Bits.SocketPopulated = 1;
    ProcessorStatus->Bits.Reserved2       = 0;
    PopulateCpuData (ProcessorIndex, MiscProcessorData);
    PopulateCpuCharData (ProcessorCharacteristics);
  } else {
    DEBUG ((DEBUG_INFO, "is disbled\n"));
    ProcessorStatus->Bits.CpuStatus       = 0; // CPU disabled
    ProcessorStatus->Bits.Reserved1       = 0;
    ProcessorStatus->Bits.SocketPopulated = 0;
    ProcessorStatus->Bits.Reserved2       = 0;
  }

  return TRUE;
}

/**
  OemGetCacheInformation
  Gets information about the cache at the specified cache level.

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
  IN UINT8                   ProcessorIndex,
  IN UINT8                   CacheLevel,
  IN BOOLEAN                 DataCache,
  IN BOOLEAN                 UnifiedCache,
  IN OUT SMBIOS_TABLE_TYPE7  *SmbiosCacheTable
  )
{
  if ((SocketMask & (1UL << ProcessorIndex)) == 0) {
    return FALSE;
  }

  return TRUE;
}

/**
  OemGetMaxProcessors
  Gets the maximum number of processors supported by the platform.

  @return The maximum number of processors.
**/
UINT8
EFIAPI
OemGetMaxProcessors (
  VOID
  )
{
  UINTN  NumProcessorSockets;
  UINTN  Index;

  NumProcessorSockets = 0;

  for (Index = 0; Index < PcdGet32 (PcdTegraMaxSockets); Index++) {
    if ((SocketMask & (1UL << Index)) != 0) {
      NumProcessorSockets++;
    }
  }

  return NumProcessorSockets;
}

/**
  OemGetChassisType
  Gets the type of chassis for the system.

  @retval The type of the chassis.
**/
MISC_CHASSIS_TYPE
EFIAPI
OemGetChassisType (
  VOID
  )
{
  if (Type3Record) {
    return Type3Record->Type;
  } else {
    return MiscChassisTypeUnknown;
  }
}

/**
  OemIsProcessorPresent
  Returns whether the specified processor is present or not.

  @param ProcessIndex The processor index to check.

  @return TRUE is the processor is present, FALSE otherwise.
**/
BOOLEAN
EFIAPI
OemIsProcessorPresent (
  IN UINTN  ProcessorIndex
  )
{
  if ((SocketMask & (1UL << ProcessorIndex)) != 0) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
  OemGetProductName
  Get the Name of the current product from the DT.

  @param  NONE.

  @return Null terminated unicode string for the product name.
**/
STATIC
CHAR16 *
OemGetProductName (
  VOID
  )
{
  VOID        *DtbBase;
  UINTN       DtbSize;
  CONST VOID  *Property;
  INT32       Length;
  EFI_STATUS  Status;

  if (BoardProductName == NULL) {
    Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
    if (EFI_ERROR (Status)) {
      return NULL;
    }

    Property = fdt_getprop (DtbBase, 0, "model", &Length);
    if ((Property != NULL) && (Length != 0)) {
      BoardProductName = AllocateZeroPool (Length * sizeof (CHAR16));
      if (BoardProductName == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
        return NULL;
      }

      AsciiStrToUnicodeStrS (
        Property,
        BoardProductName,
        (Length * sizeof (CHAR16))
        );
    }
  }

  return BoardProductName;
}

/**
  GetProcessorVersion
  Get the processor version from the DT. For multisocket designs,
  we can assume all processors in the system are same version.

  @param  NONE.

  @return Null terminated unicode string for the processor version.
**/
STATIC
CHAR16 *
GetProcessorVersion (
  VOID
  )
{
  VOID        *DtbBase;
  UINTN       DtbSize;
  CONST VOID  *Property;
  INT32       Length;
  EFI_STATUS  Status;
  INTN        DtbSmbiosOffset;
  INTN        Type4Offset;
  CHAR8       *ProcessorStep = NULL;
  UINTN       ProcessorStrLen;

  if (ProcessorVersion == NULL) {
    Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
    if (EFI_ERROR (Status)) {
      return NULL;
    }

    DtbSmbiosOffset = fdt_path_offset (DtbBase, "/firmware/smbios");
    if (DtbSmbiosOffset < 0) {
      return NULL;
    }

    Type4Offset = fdt_subnode_offset (DtbBase, DtbSmbiosOffset, "type4@0");
    if (Type4Offset < 0) {
      return NULL;
    }

    Property = fdt_getprop (DtbBase, Type4Offset, "processor-version", &Length);
    if ((Property != NULL) && (Length != 0)) {
      ProcessorStep = TegraGetMinorVersion ();
      if (ProcessorStep == NULL) {
        DEBUG ((DEBUG_INFO, "%a: No Processor Step Found\n", __FUNCTION__));
      } else {
        DEBUG ((DEBUG_INFO, "%a: Processor Step %a %u\n", __FUNCTION__, ProcessorStep, AsciiStrLen (ProcessorStep)));
      }

      ProcessorStrLen  = ((Length + AsciiStrLen (ProcessorStep) + 2) * sizeof (CHAR16));
      ProcessorVersion = AllocateZeroPool (ProcessorStrLen);
      if (ProcessorVersion == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
        return NULL;
      }

      UnicodeSPrint (
        ProcessorVersion,
        ProcessorStrLen,
        L"%a %a",
        Property,
        ProcessorStep
        );
    }
  }

  return ProcessorVersion;
}

/**
  OemGetAssetTag
  Get the AssetTag of the current product from the EEPROM Info.
  This should match the tag physically present on the board.

  @param  EepromInfo   Data structure read from the EEPROM.

  @return Null terminated unicode string for the AssetTag.
**/
STATIC
CHAR16 *
OemGetAssetTag (
  TEGRA_EEPROM_BOARD_INFO  *EepromInfo
  )
{
  if (AssetTag == NULL) {
    UINTN  AssetTagLen = (TEGRA_PRODUCT_ID_LEN + 1);
    AssetTag = AllocateZeroPool (AssetTagLen * sizeof (CHAR16));
    if (AssetTag == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
      return NULL;
    }

    AsciiStrToUnicodeStrS (
      EepromInfo->ProductId,
      AssetTag,
      (AssetTagLen * sizeof (CHAR16))
      );
  }

  return AssetTag;
}

/**
  OemGetSerialNumber
  Get the Serial Number of the current product.

  @param  EepromInfo   Data structure read from the EEPROM.

  @return Null terminated unicode string for the SerialNumber.
**/
STATIC
CHAR16 *
OemGetSerialNumber (
  TEGRA_EEPROM_BOARD_INFO  *EepromInfo
  )
{
  if (SerialNumber == NULL) {
    SerialNumber = AllocateZeroPool (TEGRA_SERIAL_NUM_LEN * sizeof (CHAR16));
    if (SerialNumber == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
      return NULL;
    }

    AsciiStrToUnicodeStrS (
      EepromInfo->SerialNumber,
      SerialNumber,
      (TEGRA_SERIAL_NUM_LEN * sizeof (CHAR16))
      );
  }

  return SerialNumber;
}

/**
  OemGetSocketDesignation
  Get the Socket Designation of the Processor Index from the DT.

  @param Index The Processor Index.

  @return Null terminated unicode string for the SocketDesignation.
**/
STATIC
CHAR16 *
OemGetSocketDesignation (
  IN UINTN  Index
  )
{
  CHAR16      *SocketDesignation;
  VOID        *DtbBase;
  UINTN       DtbSize;
  CONST VOID  *Property;
  INT32       Length;
  EFI_STATUS  Status;
  CHAR8       Type4tNodeStr[] = "/firmware/smbios/type4@xx";
  INT32       NodeOffset;

  SocketDesignation = NULL;

  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  AsciiSPrint (Type4tNodeStr, sizeof (Type4tNodeStr), "/firmware/smbios/type4@%u", Index);
  NodeOffset = fdt_path_offset (DtbBase, Type4tNodeStr);
  if (NodeOffset < 0) {
    return NULL;
  }

  Property = fdt_getprop (DtbBase, NodeOffset, "socket-designation", &Length);
  if ((Property != NULL) && (Length != 0)) {
    SocketDesignation = AllocateZeroPool (Length * sizeof (CHAR16));
    if (SocketDesignation == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
      return NULL;
    }

    AsciiStrToUnicodeStrS (
      Property,
      SocketDesignation,
      (Length * sizeof (CHAR16))
      );
  }

  return SocketDesignation;
}

/**
 * Get the Serial Number for a given Socket Index
 *
 * @param[in] ProcessorIdx Socket Num to get Serial Num.
 *
 * @return Null terminated Unicode string containing SerialNum on Success.
 *         NULL string on failure.
 **/
CHAR16 *
EFIAPI
GetCpuSerialNum (
  UINT8  ProcessorIndex
  )
{
  UINTN   ChipId;
  UINT32  *EcId;
  CHAR16  *CpuSn;
  VOID    *Hob;

  CpuSn  = NULL;
  EcId   = NULL;
  ChipId = TegraGetChipID ();

  if (ChipId != TH500_CHIP_ID) {
    return NULL;
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    EcId = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->UniqueId[ProcessorIndex];
  } else {
    ASSERT (FALSE);
  }

  CpuSn = AllocateZeroPool (CPUSNMAXSTR);
  if (CpuSn == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate String\n", __FUNCTION__));
    return NULL;
  }

  UnicodeSPrint (
    CpuSn,
    CPUSNMAXSTR,
    L"0x%08x%08x%08x%08x",
    EcId[3],
    EcId[2],
    EcId[1],
    EcId[0]
    );

  return CpuSn;
}

/**
  OemUpdateSmbiosInfo
  Updates the HII string for the specified field.

  @param HiiHandle     The HII handle.
  @param TokenToUpdate The string to update.
  @param Field         The field to get information about.
**/
VOID
EFIAPI
OemUpdateSmbiosInfo (
  IN EFI_HII_HANDLE                    HiiHandle,
  IN EFI_STRING_ID                     TokenToUpdate,
  IN OEM_MISC_SMBIOS_HII_STRING_FIELD  Field
  )
{
  CHAR16  *HiiString = NULL;

  switch (Field) {
    case SystemManufacturerType01:
      HiiString = (CHAR16 *)PcdGetPtr (PcdSystemManufacturer);
      break;
    case FamilyType01:
      HiiString = (CHAR16 *)PcdGetPtr (PcdSystemFamilyType);
      break;
    case SkuNumberType01:
      HiiString = (CHAR16 *)PcdGetPtr (PcdSystemSku);
      break;
    case AssetTagType03:
    case AssetTagType02:
      if (SmEepromData) {
        HiiString = OemGetAssetTag (SmEepromData);
      }

      break;
    case ChassisLocationType02:
      HiiString = (CHAR16 *)PcdGetPtr (PcdBoardChassisLocation);
      break;
    case SerialNumType01:
    case SerialNumberType02:
      if (SmEepromData) {
        HiiString = OemGetSerialNumber (SmEepromData);
      }

      break;
    case ProductNameType02:
    case ProductNameType01:
      HiiString =  OemGetProductName ();
      break;
    case VersionType03:
      HiiString = (CHAR16 *)PcdGetPtr (PcdChassisVersion);
      break;
    case ManufacturerType03:
      HiiString = (CHAR16 *)PcdGetPtr (PcdChassisManufacturer);
      break;
    case SkuNumberType03:
      HiiString = (CHAR16 *)PcdGetPtr (PcdChassisSku);
      break;
    case SerialNumberType03:
      HiiString = (CHAR16 *)PcdGetPtr (PcdChassisSerialNumber);
      break;
    case ProcessorSocketDesType04_0 ... ProcessorSocketDesType04_15:
      ASSERT (FIELD_TO_INDEX (Field, ProcessorSocketDesType04_0) < PcdGet32 (PcdTegraMaxSockets));
      HiiString = OemGetSocketDesignation (FIELD_TO_INDEX (Field, ProcessorSocketDesType04_0));
      break;
    case ProcessorSerialNumType04_0 ... ProcessorSerialNumType04_15:
      HiiString = GetCpuSerialNum (FIELD_TO_INDEX (Field, ProcessorSerialNumType04_0));
      break;
    case ProcessorVersionType04:
      HiiString = GetProcessorVersion ();
      break;
    default:
      break;
  }

  if (HiiString != NULL) {
    HiiSetString (HiiHandle, TokenToUpdate, HiiString, NULL);
  }
}

/**
  OemGetBootStatus
  Fetches the Type 32 boot information status.

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

/**
  OemGetChassisBootupState
  Fetches the chassis status when it was last booted.

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

/**
  OemGetChassisPowerSupplyState
  Fetches the chassis power supply/supplies status when last booted.

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

/**
  OemGetChassisThermalState
  Fetches the chassis thermal status when last booted.

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

/**
  OemGetChassisSecurityStatus
  Fetches the chassis security status when last booted.

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

/**
  OemGetChassisHeight
  Fetches the chassis height in RMUs (Rack Mount Units).

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

/**
  OemGetChassisNumPowerCords
  Fetches the number of power cords.

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
  OemGetSystemUuid
  Fetches the system UUID.

  @param[out] SystemUuid     The pointer to the buffer to store the System UUID.

**/
VOID
EFIAPI
OemGetSystemUuid (
  OUT GUID  *SystemUuid
  )
{
  CopyGuid (SystemUuid, &gZeroGuid);
}

/**
  OemGetBiosRelease
  Fetches the BIOS release.

  @return The BIOS release.
**/
UINT16
EFIAPI
OemGetBiosRelease (
  VOID
  )
{
  return 0;
}

/**
   OemGetEmbeddedControllerFirmwareRelease
   Fetches the embedded controller firmware release.

  @return The embedded controller firmware release.
**/
UINT16
EFIAPI
OemGetEmbeddedControllerFirmwareRelease (
  VOID
  )
{
  return 0;
}

/**
  OemMiscLibConstructor
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
  EFI_STATUS  Status;
  VOID        *Hob;

  Status = gBS->LocateProtocol (&gNVIDIACvmEepromProtocolGuid, NULL, (VOID **)&SmEepromData);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: SMBIOS: Failed to get Board Data protocol %r\n",
      __FUNCTION__,
      Status
      ));
    SmEepromData = NULL;
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    SocketMask = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->SocketMask;
  } else {
    ASSERT (FALSE);
    SocketMask = 0x1;
  }

  DEBUG ((DEBUG_INFO, "%a: SocketMask = 0x%x\n", __FUNCTION__, SocketMask));

  Type32Record = (SMBIOS_TABLE_TYPE32 *)PcdGetPtr (PcdType32Info);
  Type3Record  = (SMBIOS_TABLE_TYPE3 *)PcdGetPtr (PcdType3Info);
  return EFI_SUCCESS;
}
