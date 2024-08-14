/** @file
  Configuration Manager Data of SMBIOS Type 4 and Type 7 table.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>
#include <ConfigurationManagerObject.h>
#include <Library/FloorSweepingLib.h>
#include <Library/ArmLib.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Library/ArmSmcLib.h>
#include <IndustryStandard/ArmCache.h>
#include <IndustryStandard/SmBios.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/OemMiscLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosProcSubParser.h"

#define CACHE_SOCKETED_SHIFT        3
#define CACHE_LOCATION_SHIFT        5
#define CACHE_ENABLED_SHIFT         7
#define CACHE_OPERATION_MODE_SHIFT  8
#define CACHE_16_SHIFT              15
#define CACHE_32_SHIFT              31
#define SMBIOS_TYPE4_MAX_SOCKET     4

typedef enum {
  CacheModeWriteThrough = 0,  ///< Cache is write-through
  CacheModeWriteBack,         ///< Cache is write-back
  CacheModeVariesWithAddress, ///< Cache mode varies by address
  CacheModeUnknown,           ///< Cache mode is unknown
  CacheModeMax
} CACHE_OPERATION_MODE;

typedef enum {
  CacheLocationInternal = 0, ///< Cache is internal to the processor
  CacheLocationExternal,     ///< Cache is external to the processor
  CacheLocationReserved,     ///< Reserved
  CacheLocationUnknown,      ///< Cache location is unknown
  CacheLocationMax
} CACHE_LOCATION;

// Global variables to store Type 7 cache table handles for L1 L2 L3 cache
STATIC CM_OBJECT_TOKEN  CacheInfoTokenL1[SMBIOS_TYPE4_MAX_SOCKET];
STATIC CM_OBJECT_TOKEN  CacheInfoTokenL2[SMBIOS_TYPE4_MAX_SOCKET];
STATIC CM_OBJECT_TOKEN  CacheInfoTokenL3[SMBIOS_TYPE4_MAX_SOCKET];

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType4 = {
  SMBIOS_TYPE_PROCESSOR_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType04),
  NULL
};

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType7 = {
  SMBIOS_TYPE_CACHE_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType07),
  NULL
};

// extern OemMiscLib.c functions. These functions are not declared in OemMiscLib.h file
extern
CHAR16 *
EFIAPI
GetCpuSerialNum (
  UINT8  ProcessorIndex
  );

extern
UINTN
EFIAPI
GetCpuEnabledCores (
  UINT8  ProcessorIndex
  );

/** Fetches the JEP106 code and SoC Revision.

    @param Jep106Code  JEP 106 code.
    @param SocRevision SoC revision.

    @retval EFI_SUCCESS Succeeded.
    @retval EFI_UNSUPPORTED Failed.
**/
STATIC
EFI_STATUS
SmbiosGetSmcArm64SocId (
  OUT INT32  *Jep106Code,
  OUT INT32  *SocRevision
  )
{
  INT32       SmcCallStatus;
  EFI_STATUS  Status;
  UINTN       SmcParam;

  Status = EFI_SUCCESS;

  SmcParam      = 0;
  SmcCallStatus = ArmCallSmc1 (SMCCC_ARCH_SOC_ID, &SmcParam, NULL, NULL);

  if (SmcCallStatus >= 0) {
    *Jep106Code = SmcCallStatus;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Got rc=%d from Smc call to get Jep106 Code\n", __FUNCTION__, SmcCallStatus));
    Status = EFI_UNSUPPORTED;
  }

  SmcParam      = 1;
  SmcCallStatus = ArmCallSmc1 (SMCCC_ARCH_SOC_ID, &SmcParam, NULL, NULL);

  if (SmcCallStatus >= 0) {
    *SocRevision = SmcCallStatus;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Got rc=%d from Smc call to get Soc Revision\n", __FUNCTION__, SmcCallStatus));
    Status = EFI_UNSUPPORTED;
  }

  return Status;
}

/** Checks if ther ARM64 SoC ID SMC call is supported

    @return Whether the ARM64 SoC ID call is supported.
**/
STATIC
BOOLEAN
HasSmcArm64SocId (
  VOID
  )
{
  INT32    SmcCallStatus;
  BOOLEAN  Arm64SocIdSupported;
  UINTN    SmcParam;

  Arm64SocIdSupported = FALSE;

  SmcCallStatus = ArmCallSmc0 (SMCCC_VERSION, NULL, NULL, NULL);

  if ((SmcCallStatus < 0) || ((SmcCallStatus >> 16) >= 1)) {
    SmcParam      = SMCCC_ARCH_SOC_ID;
    SmcCallStatus = ArmCallSmc1 (SMCCC_ARCH_FEATURES, &SmcParam, NULL, NULL);
    if (SmcCallStatus >= 0) {
      Arm64SocIdSupported = TRUE;
    }
  }

  return Arm64SocIdSupported;
}

/** Returns a value for the Processor ID field that conforms to SMBIOS
    requirements.

    @return Processor ID.
**/
STATIC
UINT64
SmbiosGetProcessorId (
  VOID
  )
{
  INT32   Jep106Code;
  INT32   SocRevision;
  UINT64  ProcessorId;

  if (HasSmcArm64SocId ()) {
    SmbiosGetSmcArm64SocId (&Jep106Code, &SocRevision);
    ProcessorId = ((UINT64)SocRevision << 32) | Jep106Code;
  } else {
    ProcessorId = ArmReadMidr ();
  }

  return ProcessorId;
}

/** Returns the SMBIOS Processor Characteristics.

    @return Processor Characteristics bitfield.
**/
STATIC
PROCESSOR_CHARACTERISTIC_FLAGS
SmbiosGetProcessorCharacteristics (
  VOID
  )
{
  PROCESSOR_CHARACTERISTIC_FLAGS  Characteristics;

  ZeroMem (&Characteristics, sizeof (Characteristics));

  Characteristics.ProcessorArm64SocId = HasSmcArm64SocId ();

  return Characteristics;
}

/** Returns the external clock frequency.

    @return The external clock frequency.
**/
STATIC
UINTN
SmbiosGetExternalClockFrequency (
  VOID
  )
{
  return ArmReadCntFrq ();
}

/** Returns the ProcessorFamily2 field value.

    @return The value for the ProcessorFamily2 field.
**/
STATIC
UINT16
SmbiosGetProcessorFamily2 (
  VOID
  )
{
  UINTN   MainIdRegister;
  UINT16  ProcessorFamily2;

  MainIdRegister = ArmReadMidr ();

  if (((MainIdRegister >> 16) & 0xF) < 8) {
    ProcessorFamily2 = ProcessorFamilyARM;
  } else {
    if (sizeof (VOID *) == 4) {
      ProcessorFamily2 = ProcessorFamilyARMv7;
    } else {
      ProcessorFamily2 = ProcessorFamilyARMv8;
    }
  }

  return ProcessorFamily2;
}

/**
  Fill in Type 4 socket related information strings read from the Device Tree

   @param[in]     DtbBase              Device Tree
   @param[in]     NodeOffset           NodeOffset for type 4
   @param[in]     String               Property
   @param[in,out] ProcessorInfoSring   Type 4 structure property string

   @return EFI_SUCCESS       Successful
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
GetPropertyFromDT (
  IN  VOID   *DtbBase,
  IN  INT32  NodeOffset,
  IN  CHAR8  *String,
  OUT CHAR8  **ProcessorInfoString
  )
{
  CONST CHAR8  *PropertyStr;
  INT32        Length;
  EFI_STATUS   Status         = EFI_SUCCESS;
  CHAR8        *ProcessorStep = NULL;
  UINTN        ProcessorStrLen;

  PropertyStr = fdt_getprop (DtbBase, NodeOffset, String, &Length);

  // check whether the property is processor-version
  if ((AsciiStrCmp (String, "processor-version") == 0)) {
    if ((PropertyStr != NULL) && (Length != 0)) {
      ProcessorStep = TegraGetMinorVersion ();
      if (ProcessorStep == NULL) {
        DEBUG ((DEBUG_INFO, "%a: No Processor Step Found\n", __FUNCTION__));
      } else {
        DEBUG ((DEBUG_INFO, "%a: Processor Step %a %u\n", __FUNCTION__, ProcessorStep, AsciiStrLen (ProcessorStep)));
      }

      ProcessorStrLen      = ((Length + AsciiStrLen (ProcessorStep) + 1));
      *ProcessorInfoString = AllocateZeroPool (ProcessorStrLen);
      if (*ProcessorInfoString == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      AsciiSPrint (*ProcessorInfoString, ProcessorStrLen, "%a %a", PropertyStr, ProcessorStep);
    } else {
      *ProcessorInfoString = NULL;
    }

    // all other properties have similar structure
  } else {
    if ((PropertyStr != NULL) && (Length != 0)) {
      *ProcessorInfoString = AllocateZeroPool (Length);
      if (*ProcessorInfoString == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      AsciiSPrint (*ProcessorInfoString, Length, PropertyStr);
    } else {
      *ProcessorInfoString = NULL;
    }
  }

  return Status;
}

/**
  Install CM object for SMBIOS Type 4
   @param [in]  ParserHandle A handle to the parser instance.
   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType4Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS                      Status;
  VOID                            *DtbBase;
  UINTN                           ProcessorCount;
  CM_SMBIOS_PROCESSOR_INFO        *ProcessorInfo;
  CHAR16                          *SerialNumberStr;
  CHAR8                           Type4NodeStr[] = "/firmware/smbios/type4@xx";
  INT32                           NodeOffset;
  UINTN                           Index;
  PROCESSOR_CHARACTERISTIC_FLAGS  ProcessorCharacteristics;
  OEM_MISC_PROCESSOR_DATA         ProcessorData;
  UINT8                           *LegacyVoltage;
  PROCESSOR_STATUS_DATA           ProcessorStatus;
  UINT64                          *ProcessorId;
  FRU_DEVICE_INFO                 *Type4FruInfo;
  CHAR8                           *FruDesc;
  CONST VOID                      *Property;
  UINTN                           ChipID;
  CM_OBJ_DESCRIPTOR               Desc;
  INT32                           DataSize;
  EFI_STRING                      MaxSpeedVarName;
  UINT64                          ProcessorMaxSpeed;

  ProcessorInfo = NULL;
  Status        = EFI_SUCCESS;
  DtbBase       = Private->DtbBase;

  // Get the socket count
  ProcessorCount = OemGetMaxProcessors ();

  // Allocate memory for Processor Info tables
  ProcessorInfo = AllocateZeroPool (sizeof (CM_SMBIOS_PROCESSOR_INFO) * ProcessorCount);
  if (ProcessorInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate 0x%llx bytes for ProcessorInfo - skipping Type 4 table\n", __FUNCTION__, sizeof (CM_SMBIOS_PROCESSOR_INFO) * ProcessorCount));
    goto ExitInstallSmbiosType4;
  }

  // Fill Type 4 data
  for (Index = 0; Index < ProcessorCount; Index++) {
    // Fill in respective cache table handles generated while installing type 7 table in type 4 table L1, L2, L3 parameters
    ProcessorInfo[Index].CacheInfoTokenL1 = CacheInfoTokenL1[Index];
    ProcessorInfo[Index].CacheInfoTokenL2 = CacheInfoTokenL2[Index];
    ProcessorInfo[Index].CacheInfoTokenL3 = CacheInfoTokenL3[Index];

    ProcessorInfo[Index].SocketDesignation     = NULL;
    ProcessorInfo[Index].ProcessorVersion      = NULL;
    ProcessorInfo[Index].ProcessorManufacturer = NULL;
    ProcessorInfo[Index].PartNumber            = NULL;
    ProcessorInfo[Index].AssetTag              = NULL;
    FruDesc                                    = NULL;

    AsciiSPrint (Type4NodeStr, sizeof (Type4NodeStr), "/firmware/smbios/type4@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type4NodeStr);
    if (NodeOffset < 0) {
      DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS Type 4 not found.\n", __FUNCTION__));
    } else {
      // Socket designation
      GetPropertyFromDT (DtbBase, NodeOffset, "socket-designation", &ProcessorInfo[Index].SocketDesignation);

      // Processor version
      GetPropertyFromDT (DtbBase, NodeOffset, "processor-version", &ProcessorInfo[Index].ProcessorVersion);

      // Processor Manufacturer
      GetPropertyFromDT (DtbBase, NodeOffset, "manufacturer", &ProcessorInfo[Index].ProcessorManufacturer);

      MaxSpeedVarName   = NULL;
      ProcessorMaxSpeed = 0;
      Property          = fdt_getprop (DtbBase, NodeOffset, "uefivar-maxspeed", &DataSize);
      if (Property != NULL) {
        MaxSpeedVarName = AllocateZeroPool ((DataSize + 1) * sizeof (CHAR16));
        if (MaxSpeedVarName != NULL) {
          AsciiStrToUnicodeStrS (
            Property,
            MaxSpeedVarName,
            DataSize
            );
          DataSize = sizeof (ProcessorMaxSpeed);
          Status   = gRT->GetVariable (MaxSpeedVarName, &gNVIDIATokenSpaceGuid, NULL, (UINTN *)&DataSize, &ProcessorMaxSpeed);
          if (!EFI_ERROR (Status)) {
            ProcessorInfo[Index].MaxSpeed = ProcessorMaxSpeed / 1000000;
          }

          FreePool (MaxSpeedVarName);
        }
      }

      //
      // Get data from FRU
      //
      Property = fdt_getprop (DtbBase, NodeOffset, "fru-desc", NULL);
      if (Property != NULL) {
        FruDesc      = (CHAR8 *)Property;
        Type4FruInfo = FindFruByDescription (Private, FruDesc);
        // Part Number
        if ((Type4FruInfo != NULL) && (Type4FruInfo->ProductPartNum != NULL)) {
          ProcessorInfo[Index].PartNumber = AllocateCopyString (Type4FruInfo->ProductPartNum);
        }

        // Asset Tag
        if ((Type4FruInfo != NULL) && (Type4FruInfo->ProductSerial != NULL)) {
          ProcessorInfo[Index].AssetTag = AllocateCopyString (Type4FruInfo->ProductSerial);
        }
      } else {
        //
        // Get data from DTB
        //
        // Part Number
        GetPropertyFromDT (DtbBase, NodeOffset, "part-number", &ProcessorInfo[Index].PartNumber);
        // Asset Tag
        GetPropertyFromDT (DtbBase, NodeOffset, "asset-tag", &ProcessorInfo[Index].AssetTag);
      }
    }

    // Processor serial number
    SerialNumberStr = GetCpuSerialNum (Index);
    if (SerialNumberStr != NULL) {
      ProcessorInfo[Index].SerialNumber = AllocateZeroPool ((StrLen (SerialNumberStr) + 1));
      if (ProcessorInfo[Index].SerialNumber == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Out of Resources. Type 4 will be skipped\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        goto ExitInstallSmbiosType4;
      }

      AsciiSPrint (
        ProcessorInfo[Index].SerialNumber,
        (StrLen (SerialNumberStr) + 1),
        "%s",
        SerialNumberStr
        );
    } else {
      ProcessorInfo[Index].SerialNumber = NULL;
    }

    // Processor info
    ProcessorData.Voltage                 = 0;
    ProcessorData.CurrentSpeed            = 0;
    ProcessorData.CoreCount               = 0;
    ProcessorData.CoresEnabled            = 0;
    ProcessorData.ThreadCount             = 0;
    ProcessorData.MaxSpeed                = 0;
    ProcessorInfo[Index].ProcessorType    = CentralProcessor;
    ProcessorInfo[Index].ProcessorUpgrade = ProcessorUpgradeNone;

    OemGetProcessorInformation (
      Index,
      &ProcessorStatus,
      (PROCESSOR_CHARACTERISTIC_FLAGS *)
      &ProcessorInfo[Index].ProcessorCharacteristics,
      &ProcessorData
      );

    LegacyVoltage                     = (UINT8 *)&ProcessorInfo[Index].Voltage;
    *LegacyVoltage                    = ProcessorData.Voltage;
    ProcessorInfo[Index].CurrentSpeed = ProcessorData.CurrentSpeed;
    ProcessorInfo[Index].Status       = ProcessorStatus.Data;

    if (ProcessorInfo[Index].MaxSpeed == 0) {
      ProcessorInfo[Index].MaxSpeed = ProcessorData.MaxSpeed;
    }

    if (ProcessorData.CoreCount > 255) {
      ProcessorInfo[Index].CoreCount = 0xFF;
    } else {
      ProcessorInfo[Index].CoreCount = ProcessorData.CoreCount;
    }

    ProcessorInfo[Index].CoreCount2 = ProcessorData.CoreCount;

    if (ProcessorData.CoresEnabled > 255) {
      ProcessorInfo[Index].EnabledCoreCount = 0xFF;
    } else {
      ProcessorInfo[Index].EnabledCoreCount = ProcessorData.CoresEnabled;
    }

    ProcessorInfo[Index].EnabledCoreCount2 = ProcessorData.CoresEnabled;

    ProcessorInfo[Index].ThreadCount  = ProcessorData.ThreadCount;
    ProcessorInfo[Index].ThreadCount2 = ProcessorData.ThreadCount;

    ProcessorInfo[Index].ExternalClock =
      (UINT16)(SmbiosGetExternalClockFrequency () / 1000 / 1000);

    ProcessorId  = (UINT64 *)&ProcessorInfo[Index].ProcessorId;
    *ProcessorId = SmbiosGetProcessorId ();

    ProcessorCharacteristics                       = SmbiosGetProcessorCharacteristics ();
    ProcessorInfo[Index].ProcessorCharacteristics |= *((UINT64 *)&ProcessorCharacteristics);
    ProcessorInfo[Index].ProcessorFamily           = ProcessorFamilyIndicatorFamily2;
    ChipID                                         = TegraGetChipID ();
    if (ChipID == TH500_CHIP_ID) {
      ProcessorInfo[Index].ProcessorFamily2 = ProcessorFamilyARMv9;
    } else {
      ProcessorInfo[Index].ProcessorFamily2 = SmbiosGetProcessorFamily2 ();
    }
  }

  //
  // Install CM object for type 4
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjProcessorInfo);
  Desc.Size     = ProcessorCount * sizeof (CM_SMBIOS_PROCESSOR_INFO);
  Desc.Count    = ProcessorCount;
  Desc.Data     = ProcessorInfo;

  Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add Type 4 to CM. Type 4 will not be installed.\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType4;
  }

  //
  // Add type 4 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType4,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

ExitInstallSmbiosType4:
  FREE_NON_NULL (ProcessorInfo);
  return Status;
}

/** Gets the size of the specified cache.

    @param CacheLevel       The cache level (L1, L2 etc.).
    @param DataCache        Whether the cache is a dedicated data cache.
    @param UnifiedCache     Whether the cache is a unified cache.

    @return The cache size.
**/
STATIC
UINT64
SmbiosProcessorGetCacheSize (
  IN UINT8    CacheLevel,
  IN BOOLEAN  DataCache,
  IN BOOLEAN  UnifiedCache
  )
{
  CCSIDR_DATA  Ccsidr;
  CSSELR_DATA  Csselr;
  BOOLEAN      CcidxSupported;
  UINT64       CacheSize;

  Csselr.Data       = 0;
  Csselr.Bits.Level = CacheLevel - 1;
  Csselr.Bits.InD   = (!DataCache && !UnifiedCache);

  Ccsidr.Data = ReadCCSIDR (Csselr.Data);

  CcidxSupported = ArmHasCcidx ();

  if (CcidxSupported) {
    CacheSize = (1 << (Ccsidr.BitsCcidxAA64.LineSize + 4)) *
                (Ccsidr.BitsCcidxAA64.Associativity + 1) *
                (Ccsidr.BitsCcidxAA64.NumSets + 1);
  } else {
    CacheSize = (1 << (Ccsidr.BitsNonCcidx.LineSize + 4)) *
                (Ccsidr.BitsNonCcidx.Associativity + 1) *
                (Ccsidr.BitsNonCcidx.NumSets + 1);
  }

  return CacheSize;
}

/** Gets the associativity of the specified cache.

    @param CacheLevel       The cache level (L1, L2 etc.).
    @param DataCache        Whether the cache is a dedicated data cache.
    @param UnifiedCache     Whether the cache is a unified cache.

    @return The cache associativity.
**/
STATIC
UINT32
SmbiosProcessorGetCacheAssociativity (
  IN UINT8    CacheLevel,
  IN BOOLEAN  DataCache,
  IN BOOLEAN  UnifiedCache
  )
{
  CCSIDR_DATA  Ccsidr;
  CSSELR_DATA  Csselr;
  BOOLEAN      CcidxSupported;
  UINT32       Associativity;

  Csselr.Data       = 0;
  Csselr.Bits.Level = CacheLevel - 1;
  Csselr.Bits.InD   = (!DataCache && !UnifiedCache);

  Ccsidr.Data = ReadCCSIDR (Csselr.Data);

  CcidxSupported = ArmHasCcidx ();

  if (CcidxSupported) {
    Associativity = Ccsidr.BitsCcidxAA64.Associativity + 1;
  } else {
    Associativity = Ccsidr.BitsNonCcidx.Associativity + 1;
  }

  return Associativity;
}

/** Fills the cache associativity.

  @param[in] Associativity       Associativity of cache.
  @param[out] CacheInfo          CacheInfo record to update

**/
STATIC
VOID
GetCacheAssociativity (
  IN       UINT32                Associativity,
  IN OUT   CM_SMBIOS_CACHE_INFO  *CacheInfo
  )
{
  switch (Associativity) {
    case 2:
      CacheInfo->Associativity = CacheAssociativity2Way;
      break;
    case 4:
      CacheInfo->Associativity = CacheAssociativity4Way;
      break;
    case 8:
      CacheInfo->Associativity = CacheAssociativity8Way;
      break;
    case 12:
      CacheInfo->Associativity = CacheAssociativity12Way;
      break;
    case 16:
      CacheInfo->Associativity = CacheAssociativity16Way;
      break;
    case 20:
      CacheInfo->Associativity = CacheAssociativity20Way;
      break;
    case 24:
      CacheInfo->Associativity = CacheAssociativity24Way;
      break;
    case 32:
      CacheInfo->Associativity = CacheAssociativity32Way;
      break;
    case 48:
      CacheInfo->Associativity = CacheAssociativity48Way;
      break;
    case 64:
      CacheInfo->Associativity = CacheAssociativity64Way;
      break;
    default:
      CacheInfo->Associativity = CacheAssociativityOther;
      break;
  }
}

/** Fills in the Type 7 record with the cache architecture information
    read from the CPU registers.

  @param[in]  CacheLevel       Cache level (e.g. L1, L2).
  @param[in]  DataCache        Cache is a data cache.
  @param[in]  UnifiedCache     Cache is a unified cache.
  @param[in]  EnabledCores     EnabledCores.
  @param[out] Type7Record      The Type 7 record to fill in.

**/
STATIC
VOID
ConfigureCacheArchitectureInformation (
  IN     UINT8                 CacheLevel,
  IN     BOOLEAN               DataCache,
  IN     BOOLEAN               UnifiedCache,
  IN     UINTN                 EnabledCores,
  OUT    CM_SMBIOS_CACHE_INFO  *Type7Record
  )
{
  UINT8   Associativity;
  UINT32  CacheSize32;
  UINT16  CacheSize16;
  UINT64  CacheSize64;

  if (!DataCache && !UnifiedCache) {
    Type7Record->SystemCacheType = CacheTypeInstruction;
  } else if (DataCache) {
    Type7Record->SystemCacheType = CacheTypeData;
  } else if (UnifiedCache) {
    Type7Record->SystemCacheType = CacheTypeUnified;
  }

  CacheSize64 = SmbiosProcessorGetCacheSize (
                  CacheLevel,
                  DataCache,
                  UnifiedCache
                  );

  Associativity = SmbiosProcessorGetCacheAssociativity (
                    CacheLevel,
                    DataCache,
                    UnifiedCache
                    );
  CacheSize64 *= EnabledCores;
  CacheSize64 /= 1024; // Minimum granularity is 1K

  // Encode the cache size into the format SMBIOS wants
  if (CacheSize64 < MAX_INT16) {
    CacheSize16 = CacheSize64;
    CacheSize32 = CacheSize16;
  } else if ((CacheSize64 / 64) < MAX_INT16) {
    CacheSize16 = (1 << CACHE_16_SHIFT) | (CacheSize64 / 64);
    CacheSize32 = (1U << CACHE_32_SHIFT) | (CacheSize64 / 64U);
  } else {
    if ((CacheSize64 / 1024) <= 2047) {
      CacheSize32 = CacheSize64;
    } else {
      CacheSize32 = (1U << CACHE_32_SHIFT) | (CacheSize64 / 64U);
    }

    CacheSize16 = -1;
  }

  Type7Record->MaximumCacheSize  = CacheSize16;
  Type7Record->InstalledSize     = CacheSize16;
  Type7Record->MaximumCacheSize2 = CacheSize32;
  Type7Record->InstalledSize2    = CacheSize32;

  GetCacheAssociativity (Associativity, Type7Record);

  Type7Record->CacheConfiguration = (CacheModeWriteBack << CACHE_OPERATION_MODE_SHIFT) |
                                    (1 << CACHE_ENABLED_SHIFT) |
                                    (CacheLocationInternal << CACHE_LOCATION_SHIFT) |
                                    (0 << CACHE_SOCKETED_SHIFT) |
                                    (CacheLevel - 1);
}

/** Gets a description of the specified cache.

  @param[in] CacheLevel       Zero-based cache level (e.g. L1 cache is 0).
  @param[in] DataCache        Cache is a data cache.
  @param[in] UnifiedCache     Cache is a unified cache.
  @param[out] CacheSocketStr  The description of the specified cache

  @return The number of Unicode characters in CacheSocketStr not including the
          terminating NUL.
**/
STATIC
UINTN
GetCacheSocketStr (
  IN  UINT8    CacheLevel,
  IN  BOOLEAN  DataCache,
  IN  BOOLEAN  UnifiedCache,
  OUT CHAR8    *CacheSocketStr
  )
{
  UINTN  CacheSocketStrLen = 0;

  if (CacheSocketStr != NULL) {
    if ((CacheLevel == CpuCacheL1) && !DataCache && !UnifiedCache) {
      CacheSocketStrLen = AsciiSPrint (
                            CacheSocketStr,
                            SMBIOS_STRING_MAX_LENGTH,
                            "L%x Instruction Cache",
                            CacheLevel
                            );
    } else if ((CacheLevel == CpuCacheL1) && DataCache) {
      CacheSocketStrLen = AsciiSPrint (
                            CacheSocketStr,
                            SMBIOS_STRING_MAX_LENGTH,
                            "L%x Data Cache",
                            CacheLevel
                            );
    } else {
      CacheSocketStrLen = AsciiSPrint (
                            CacheSocketStr,
                            SMBIOS_STRING_MAX_LENGTH,
                            "L%x Cache",
                            CacheLevel
                            );
    }
  }

  return CacheSocketStrLen;
}

/** Returns whether or not the specified cache level has separate I/D caches.

    @param CacheLevel The cache level (L1, L2 etc.).

    @return TRUE if the cache level has separate I/D caches, FALSE otherwise.
**/
STATIC
BOOLEAN
ProcessorHasSeparateCaches (
  UINT8  CacheLevel
  )
{
  CLIDR_CACHE_TYPE  CacheType;
  CLIDR_DATA        Clidr;
  BOOLEAN           SeparateCaches;

  SeparateCaches = FALSE;

  Clidr.Data = ReadCLIDR ();

  CacheType = CLIDR_GET_CACHE_TYPE (Clidr.Data, CacheLevel - 1);

  if (CacheType == ClidrCacheTypeSeparate) {
    SeparateCaches = TRUE;
  }

  return SeparateCaches;
}

/** Returns the maximum cache level implemented by the current CPU.

    @return The maximum cache level implemented.
**/
STATIC
UINT8
ProcessorGetMaxCacheLevel (
  VOID
  )
{
  CLIDR_DATA  Clidr;
  UINT8       CacheLevel;
  UINT8       MaxCacheLevel;

  MaxCacheLevel = 0;

  // Read the CLIDR register to find out what caches are present.
  Clidr.Data = ReadCLIDR ();

  // Get the cache type for the L1 cache. If it's 0, there are no caches.
  if (CLIDR_GET_CACHE_TYPE (Clidr.Data, 1) == ClidrCacheTypeNone) {
    return 0;
  }

  for (CacheLevel = 1; CacheLevel <= MAX_ARM_CACHE_LEVEL; CacheLevel++) {
    if (CLIDR_GET_CACHE_TYPE (Clidr.Data, CacheLevel) == ClidrCacheTypeNone) {
      MaxCacheLevel = CacheLevel;
      break;
    }
  }

  return MaxCacheLevel;
}

STATIC CONST CHAR8  *L3Compatible[] = {
  "cache",
  // Support old DTB compatible strings as well
  "l3-cache",
  NULL
};

/** Collect L3 cache data and fill CacheInfo record.

  @param[in]  Dtb           Device Tree
  @param[in]  SocketOffset  Socket offset to fetch socket data from DT
  @param[in]  Index         Index of socket
  @param[in]  TableCount         Index of socket
  @param[in, out]  CacheInfo     CacheInfo record to update

**/
STATIC
EFI_STATUS
EFIAPI
GetL3CacheInfo (
  IN       VOID                  *Dtb,
  IN       INT32                 SocketOffset,
  IN       UINTN                 Index,
  IN       UINTN                 TableCount,
  IN OUT   CM_SMBIOS_CACHE_INFO  *CacheInfo
  )
{
  EFI_STATUS                     Status;
  CHAR8                          CacheL3Str[] = "L3 Cache\0";
  UINT32                         Associativity;
  UINT32                         CacheSize32;
  UINT16                         CacheSize16;
  UINT64                         CacheSize64;
  NVIDIA_DEVICE_TREE_CACHE_DATA  CacheData;

  // Check if socket exists
  if (SocketOffset < 0) {
    return EFI_INVALID_PARAMETER;
  }

  // Get L3 Cache data from dt
  CacheData.CacheLevel = 0;
  while ((Status = DeviceTreeGetNextCompatibleNode (L3Compatible, &SocketOffset)) != EFI_NOT_FOUND) {
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get L3 cache data from DTB\n", __FUNCTION__, Status));
      return EFI_INVALID_PARAMETER;
    }

    CacheData.Type = CACHE_TYPE_UNIFIED;
    Status         = DeviceTreeGetCacheData (SocketOffset, &CacheData);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get cache data for cache node\n", __FUNCTION__, Status));
      return Status;
    }

    // Check if it's an L3 cache node
    if (CacheData.CacheLevel == 3) {
      break;
    }
  }

  if (CacheData.CacheLevel != 3) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to find an L3 cache\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  // Calculate Associativity
  if ((CacheData.CacheLineSize != 0) && (CacheData.CacheSets != 0)) {
    Associativity = CacheData.CacheSize / (CacheData.CacheLineSize* CacheData.CacheSets);
  } else {
    Associativity = 0;
  }

  GetCacheAssociativity (Associativity, CacheInfo);
  // Cache Configuration
  CacheInfo->CacheConfiguration = (CacheModeWriteBack << CACHE_OPERATION_MODE_SHIFT) |
                                  (1 << CACHE_ENABLED_SHIFT) |
                                  (CacheLocationInternal << CACHE_LOCATION_SHIFT) |
                                  (0 << CACHE_SOCKETED_SHIFT) |
                                  (2);

  CacheSize64  = CacheData.CacheSize;
  CacheSize64 /= 1024;   // Minimum granularity is 1K

  // Encode the cache size into the format SMBIOS wants
  if (CacheSize64 < MAX_INT16) {
    CacheSize16 = CacheSize64;
    CacheSize32 = CacheSize16;
  } else if ((CacheSize64 / 64) < MAX_INT16) {
    CacheSize16 = (1 << CACHE_16_SHIFT) | (CacheSize64 / 64);
    CacheSize32 = (1U << CACHE_32_SHIFT) | (CacheSize64 / 64U);
  } else {
    if ((CacheSize64 / 1024) <= 2047) {
      CacheSize32 = CacheSize64;
    } else {
      CacheSize32 = (1U << CACHE_32_SHIFT) | (CacheSize64 / 64U);
    }

    CacheSize16 = -1;
  }

  CacheInfo->MaximumCacheSize  = CacheSize16;
  CacheInfo->InstalledSize     = CacheSize16;
  CacheInfo->MaximumCacheSize2 = CacheSize32;
  CacheInfo->InstalledSize2    = CacheSize32;
  // Cache Socket Designation
  if (CacheL3Str != NULL) {
    CacheInfo->SocketDesignation = AllocateZeroPool (strlen (CacheL3Str) +1);
    if (CacheInfo->SocketDesignation == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Out of Resources.\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    AsciiSPrint (CacheInfo->SocketDesignation, strlen (CacheL3Str) +1, CacheL3Str);
  }

  CacheInfo->SupportedSRAMType.Other = 1;
  CacheInfo->CurrentSRAMType.Other   = 1;
  CacheInfo->CacheSpeed              = 0;
  CacheInfo->ErrorCorrectionType     = CacheErrorSingleBit;
  CacheInfo->SystemCacheType         = CacheTypeUnified;

  return EFI_SUCCESS;
}

/**
  Install CM object for SMBIOS Type 7
   @param [in]  ParserHandle A handle to the parser instance.
   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType7Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS            Status;
  VOID                  *Dtb;
  UINTN                 DtbSize;
  UINTN                 ProcessorCount;
  CM_SMBIOS_CACHE_INFO  *CacheInfo;
  UINTN                 Index;
  UINT8                 CacheLevel;
  UINT8                 MaxCacheLevel;
  BOOLEAN               DataCacheType;
  BOOLEAN               SeparateCaches;
  UINTN                 CacheSocketStrLen;
  CHAR8                 *SocketDesignationAsciiStr;
  UINTN                 TableCount;
  UINTN                 CoresEnabled;
  INT32                 SocketOffset;
  CHAR8                 SocketNodeStr[] = "/socket@xx";
  CM_OBJ_DESCRIPTOR     Desc;
  CM_OBJECT_TOKEN       *TokenMap;

  TokenMap   = NULL;
  CacheInfo  = NULL;
  Status     = EFI_SUCCESS;
  TableCount = 0;

  MaxCacheLevel = 0;
  // See if there's an L1 cache present.
  MaxCacheLevel = ProcessorGetMaxCacheLevel ();
  if (MaxCacheLevel < 1) {
    DEBUG ((DEBUG_ERROR, "%a: MaxCacheLevel must be at least 1 - Type 7 won't be installed\n", __FUNCTION__));
    goto ExitInstallSmbiosType7;
  }

  ProcessorCount = OemGetMaxProcessors ();

  // Calculate the numer of cache tables required
  for (Index = 0; Index < ProcessorCount; Index++) {
    for (CacheLevel = 1; CacheLevel <= MaxCacheLevel; CacheLevel++) {
      SeparateCaches = ProcessorHasSeparateCaches (CacheLevel);
      for (DataCacheType = 0; DataCacheType <= 1; DataCacheType++) {
        // If there's no separate data/instruction cache, skip the second iteration
        if ((DataCacheType == 1) && !SeparateCaches) {
          continue;
        }

        TableCount++;
      }
    }

    // Increment TableCount for each processor's L3 cache that is captured by dtb
    TableCount++;
  }

  if (TableCount == 0) {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "%a: No tables found\n", __FUNCTION__));
    return Status;
  }

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, TableCount, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 7: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType7;
  }

  // Allocate memory for the cache tables
  CacheInfo = AllocateZeroPool (sizeof (CM_SMBIOS_CACHE_INFO) * ProcessorCount * TableCount);
  if (CacheInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate 0x%llx bytes for CacheInfo - Type 7 will be skipped\n", __FUNCTION__, sizeof (CM_SMBIOS_CACHE_INFO) * ProcessorCount * TableCount));
    goto ExitInstallSmbiosType7;
  }

  TableCount = 0;

  // Fill Type 7 data
  for (Index = 0; Index < ProcessorCount; Index++) {
    Status                  = EFI_SUCCESS;
    CacheInfoTokenL1[Index] = 0xFFFF;
    CacheInfoTokenL2[Index] = 0xFFFF;
    CacheInfoTokenL3[Index] = 0xFFFF;

    // Get the Cores enabled count to calculate the total cache size
    CoresEnabled = GetCpuEnabledCores (Index);

    // Get Dtb
    Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
    if (EFI_ERROR (Status)) {
      goto ExitInstallSmbiosType7;
    }

    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", Index);
    SocketOffset = fdt_path_offset (Dtb, SocketNodeStr);

    for (CacheLevel = 1; CacheLevel <= MaxCacheLevel; CacheLevel++) {
      SeparateCaches = ProcessorHasSeparateCaches (CacheLevel);
      // At each level of cache, we can have a single type (unified, instruction or data),
      // or two types - separate data and instruction caches. If we have separate
      // instruction and data caches, then on the first iteration (CacheSubLevel = 0)
      // process the instruction cache.
      for (DataCacheType = 0; DataCacheType <= 1; DataCacheType++) {
        // If there's no separate data/instruction cache, skip the second iteration
        if ((DataCacheType == 1) && !SeparateCaches) {
          continue;
        }

        // Socket designation
        SocketDesignationAsciiStr = AllocateZeroPool (sizeof (CHAR8) * SMBIOS_STRING_MAX_LENGTH);
        CacheSocketStrLen         = GetCacheSocketStr (
                                      CacheLevel,
                                      DataCacheType,
                                      !SeparateCaches,
                                      SocketDesignationAsciiStr
                                      );
        if (SocketDesignationAsciiStr != NULL) {
          CacheInfo[TableCount].SocketDesignation = SocketDesignationAsciiStr;
        } else {
          CacheInfo[TableCount].SocketDesignation = NULL;
        }

        CacheInfo[TableCount].SupportedSRAMType.Other = 1;
        CacheInfo[TableCount].CurrentSRAMType.Other   = 1;
        CacheInfo[TableCount].CacheSpeed              = 0;

        ConfigureCacheArchitectureInformation (
          CacheLevel,
          DataCacheType,
          !SeparateCaches,
          CoresEnabled,
          &CacheInfo[TableCount]
          );

        // Record cache table handles to populate in Type 4 and set error correction type for each cache type
        CacheInfo[TableCount].CacheInfoToken = TokenMap[TableCount];
        if (CacheLevel == 1) {
          if (DataCacheType == 0) {
            CacheInfo[TableCount].ErrorCorrectionType = CacheErrorParity;
          } else if (DataCacheType == 1) {
            CacheInfo[TableCount].ErrorCorrectionType = CacheErrorSingleBit;
          }

          CacheInfoTokenL1[Index] = CacheInfo[TableCount].CacheInfoToken;
        } else if (CacheLevel == 2) {
          CacheInfo[TableCount].ErrorCorrectionType = CacheErrorSingleBit;
          CacheInfoTokenL2[Index]                   = CacheInfo[TableCount].CacheInfoToken;
        }

        // Additional logic added below for L3 cache info

        TableCount++;
      }
    }

    // Additional logic to generate type 7 L3 cache table
    Status = GetL3CacheInfo (Dtb, SocketOffset, Index, TableCount, &CacheInfo[TableCount]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get L3CacheInfo - Type 7 will be skipped\n", __FUNCTION__, Status));
      goto ExitInstallSmbiosType7;
    }

    CacheInfo[TableCount].CacheInfoToken = TokenMap[TableCount];
    CacheInfoTokenL3[Index]              = CacheInfo[TableCount].CacheInfoToken;

    TableCount++;
  }

  //
  // Install CM object for type 7
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjCacheInfo);
  Desc.Size     = TableCount * ProcessorCount * sizeof (CM_SMBIOS_CACHE_INFO);
  Desc.Count    = TableCount;
  Desc.Data     = CacheInfo;

  Status = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 7 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType7;
  }

  //
  // Add type 7 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType7,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

ExitInstallSmbiosType7:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (CacheInfo);
  return Status;
}

/**
Install CM objects for Processor Sub Class related SMBIOS tables.
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosProcSubCm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS  Status;

  Status = InstallSmbiosType7Cm (ParserHandle, Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: Failed to install Type 7 %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitInstallSmbiosType4Type7;
  }

  Status = InstallSmbiosType4Cm (ParserHandle, Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: Failed to install Type 4 %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitInstallSmbiosType4Type7;
  }

ExitInstallSmbiosType4Type7:
  return Status;
}
