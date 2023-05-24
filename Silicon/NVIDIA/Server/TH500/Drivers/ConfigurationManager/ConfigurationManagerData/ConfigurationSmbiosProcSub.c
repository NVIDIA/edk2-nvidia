/**
  Configuration Manager Data of SMBIOS Type 4 and Type 7 table.

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Library/FloorSweepingLib.h>
#include <Library/ArmLib.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Library/ArmSmcLib.h>
#include <IndustryStandard/ArmCache.h>
#include <IndustryStandard/SmBios.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/OemMiscLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include "ConfigurationSmbiosPrivate.h"

#define CACHE_SOCKETED_SHIFT        3
#define CACHE_LOCATION_SHIFT        5
#define CACHE_ENABLED_SHIFT         7
#define CACHE_OPERATION_MODE_SHIFT  8
#define CACHE_16_SHIFT              15
#define CACHE_32_SHIFT              31
#define SMBIOS_TYPE4_MAX_STRLEN     65
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
    Status = EFI_UNSUPPORTED;
  }

  SmcParam      = 1;
  SmcCallStatus = ArmCallSmc1 (SMCCC_ARCH_SOC_ID, &SmcParam, NULL, NULL);

  if (SmcCallStatus >= 0) {
    *SocRevision = SmcCallStatus;
  } else {
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

   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType4Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
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

  Status  = EFI_SUCCESS;
  Repo    = Private->Repo;
  DtbBase = Private->DtbBase;

  // Get the socket count
  ProcessorCount = OemGetMaxProcessors ();

  // Allocate memory for Processor Info tables
  ProcessorInfo = AllocateZeroPool (sizeof (CM_SMBIOS_PROCESSOR_INFO) * ProcessorCount);
  if (ProcessorInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitInstallSmbiosType4;
  }

  // Fill Type 4 data
  for (Index = 0; Index < ProcessorCount; Index++) {
    // Fill in respective cache table handles generated while installing type 7 table in type 4 table L1, L2, L3 parameters
    ProcessorInfo[Index].CacheInfoTokenL1 = CacheInfoTokenL1[Index];
    ProcessorInfo[Index].CacheInfoTokenL2 = CacheInfoTokenL2[Index];
    ProcessorInfo[Index].CacheInfoTokenL3 = CacheInfoTokenL3[Index];

    AsciiSPrint (Type4NodeStr, sizeof (Type4NodeStr), "/firmware/smbios/type4@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type4NodeStr);
    if (NodeOffset < 0) {
      Status = EFI_UNSUPPORTED;
      goto ExitInstallSmbiosType4;
    }

    // Socket designation
    Status = GetPropertyFromDT (DtbBase, NodeOffset, "socket-designation", &ProcessorInfo[Index].SocketDesignation);
    if (Status != EFI_SUCCESS) {
      goto ExitInstallSmbiosType4;
    }

    // Processor version
    Status = GetPropertyFromDT (DtbBase, NodeOffset, "processor-version", &ProcessorInfo[Index].ProcessorVersion);
    if (Status != EFI_SUCCESS) {
      goto ExitInstallSmbiosType4;
    }

    // Processor Manufacturer
    Status = GetPropertyFromDT (DtbBase, NodeOffset, "manufacturer", &ProcessorInfo[Index].ProcessorManufacturer);
    if (Status != EFI_SUCCESS) {
      goto ExitInstallSmbiosType4;
    }

    // Part Number
    Status = GetPropertyFromDT (DtbBase, NodeOffset, "part-number", &ProcessorInfo[Index].PartNumber);
    if (Status != EFI_SUCCESS) {
      goto ExitInstallSmbiosType4;
    }

    // Assest Tag
    Status = GetPropertyFromDT (DtbBase, NodeOffset, "assest-tag", &ProcessorInfo[Index].AssetTag);
    if (Status != EFI_SUCCESS) {
      goto ExitInstallSmbiosType4;
    }

    // Processor serial number
    SerialNumberStr = GetCpuSerialNum (Index);
    if (SerialNumberStr != NULL) {
      ProcessorInfo[Index].SerialNumber = AllocateZeroPool (SMBIOS_TYPE4_MAX_STRLEN);
      if (ProcessorInfo[Index].SerialNumber == NULL) {
        DEBUG ((DEBUG_INFO, "%a: Out of Resources.\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        goto ExitInstallSmbiosType4;
      }

      AsciiSPrint (
        ProcessorInfo[Index].SerialNumber,
        SMBIOS_TYPE4_MAX_STRLEN,
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
    ProcessorInfo[Index].ProcessorUpgrade = ProcessorUpgradeUnknown;

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
    ProcessorInfo[Index].MaxSpeed     = ProcessorData.MaxSpeed;
    ProcessorInfo[Index].Status       = ProcessorStatus.Data;

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
    ProcessorInfo[Index].ProcessorFamily2          = SmbiosGetProcessorFamily2 ();
  }

  //
  // Install CM object for type 4
  //
  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjProcessorInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = ProcessorCount * sizeof (CM_SMBIOS_PROCESSOR_INFO);
  Repo->CmObjectCount = ProcessorCount;
  Repo->CmObjectPtr   = ProcessorInfo;
  Repo++;

  //
  // Add type 4 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType4,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

ExitInstallSmbiosType4:
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

/** Fills in the Type 7 record with the cache architecture information
    read from the CPU registers.

  @param[in]  CacheLevel       Cache level (e.g. L1, L2).
  @param[in]  DataCache        Cache is a data cache.
  @param[in]  UnifiedCache     Cache is a unified cache.
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

  switch (Associativity) {
    case 2:
      Type7Record->Associativity = CacheAssociativity2Way;
      break;
    case 4:
      Type7Record->Associativity = CacheAssociativity4Way;
      break;
    case 8:
      Type7Record->Associativity = CacheAssociativity8Way;
      break;
    case 12:
      Type7Record->Associativity = CacheAssociativity12Way;
      break;
    case 16:
      Type7Record->Associativity = CacheAssociativity16Way;
      break;
    case 20:
      Type7Record->Associativity = CacheAssociativity20Way;
      break;
    case 24:
      Type7Record->Associativity = CacheAssociativity24Way;
      break;
    case 32:
      Type7Record->Associativity = CacheAssociativity32Way;
      break;
    case 48:
      Type7Record->Associativity = CacheAssociativity48Way;
      break;
    case 64:
      Type7Record->Associativity = CacheAssociativity64Way;
      break;
    default:
      Type7Record->Associativity = CacheAssociativityOther;
      break;
  }

  Type7Record->CacheConfiguration = (CacheModeUnknown << CACHE_OPERATION_MODE_SHIFT) |
                                    (1 << CACHE_ENABLED_SHIFT) |
                                    (CacheLocationUnknown << CACHE_LOCATION_SHIFT) |
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

/**
  Install CM object for SMBIOS Type 7

   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType7Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  VOID                            *DtbBase;
  UINTN                           ProcessorCount;
  CM_SMBIOS_CACHE_INFO            *CacheInfo;
  UINTN                           Index;
  UINT8                           CacheLevel;
  UINT8                           MaxCacheLevel;
  BOOLEAN                         DataCacheType;
  BOOLEAN                         SeparateCaches;
  UINTN                           CacheSocketStrLen;
  CHAR8                           *SocketDesignationAsciiStr;
  UINTN                           TableCount;
  UINTN                           CoresEnabled;

  Status     = EFI_SUCCESS;
  Repo       = Private->Repo;
  DtbBase    = Private->DtbBase;
  TableCount = 0;

  ProcessorCount = OemGetMaxProcessors ();

  // Calculate the numer of cache tables required
  for (Index = 0; Index < ProcessorCount; Index++) {
    MaxCacheLevel = 0;
    // See if there's an L1 cache present.
    MaxCacheLevel = ProcessorGetMaxCacheLevel ();
    if (MaxCacheLevel < 1) {
      goto ExitInstallSmbiosType7;
    }

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
  }

  // Allocate memory for the cache tables
  CacheInfo = AllocateZeroPool (sizeof (CM_SMBIOS_CACHE_INFO) * ProcessorCount * TableCount);
  if (CacheInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitInstallSmbiosType7;
  }

  TableCount = 0;

  // Fill Type 7 data
  for (Index = 0; Index < ProcessorCount; Index++) {
    Status                  = EFI_SUCCESS;
    CacheInfoTokenL1[Index] = 0xFFFF;
    CacheInfoTokenL2[Index] = 0xFFFF;
    CacheInfoTokenL3[Index] = 0xFFFF;

    MaxCacheLevel = 0;

    // See if there's an L1 cache present.
    MaxCacheLevel = ProcessorGetMaxCacheLevel ();

    if (MaxCacheLevel < 1) {
      goto ExitInstallSmbiosType7;
    }

    // Get the Cores enabled count to calculate the total cache size
    CoresEnabled = GetCpuEnabledCores (Index);

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

        CacheInfo[TableCount].SupportedSRAMType.Unknown = 1;
        CacheInfo[TableCount].CurrentSRAMType.Unknown   = 1;
        CacheInfo[TableCount].CacheSpeed                = 0;
        CacheInfo[TableCount].ErrorCorrectionType       = CacheErrorUnknown;

        ConfigureCacheArchitectureInformation (
          CacheLevel,
          DataCacheType,
          !SeparateCaches,
          CoresEnabled,
          &CacheInfo[TableCount]
          );

        // Record Cache Table handles to populate in Type 4
        CacheInfo[TableCount].CacheInfoToken = (CM_OBJECT_TOKEN)&CacheInfo[TableCount];
        if (CacheLevel == 1) {
          CacheInfoTokenL1[Index] = CacheInfo[TableCount].CacheInfoToken;
        } else if (CacheLevel == 2) {
          CacheInfoTokenL2[Index] = CacheInfo[TableCount].CacheInfoToken;
        } else if (CacheLevel == 3) {
          CacheInfoTokenL3[Index] = CacheInfo[TableCount].CacheInfoToken;
        }

        TableCount++;
      }
    }
  }

  //
  // Install CM object for type 7
  //
  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjCacheInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = TableCount * ProcessorCount * sizeof (CM_SMBIOS_CACHE_INFO);
  Repo->CmObjectCount = TableCount;
  Repo->CmObjectPtr   = CacheInfo;
  Repo++;

  //
  // Add type 7 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType7,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

ExitInstallSmbiosType7:
  return Status;
}

/**
Install CM objects for Processor Sub Class related SMBIOS tables.

@param[in, out] Private   Pointer to the private data of SMBIOS creators

@return EFI_SUCCESS       Successful installation
@retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosProcSubCm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  Status = InstallSmbiosType7Cm (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: Failed to install Type 7 %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitInstallSmbiosType4Type7;
  }

  Status = InstallSmbiosType4Cm (Private);
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
