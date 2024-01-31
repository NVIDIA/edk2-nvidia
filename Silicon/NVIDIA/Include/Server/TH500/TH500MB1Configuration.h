/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef TH500_MB1_CONFIGURATION_H__
#define TH500_MB1_CONFIGURATION_H__

#include <Library/TegraPlatformInfoLib.h>

#define TEGRABL_MB1BCT_HASH_MAX_SIZE      64U
#define TEGRABL_EARLY_BOOT_VARS_MAX_SIZE  2048U
#define TEGRABL_SOC_MAX_SOCKETS           4
#define TEGRABL_MAX_UPHY_PER_SOCKET       6
#define TEGRABL_MAX_PCIE_PER_SOCKET       10

#define TEGRABL_MB1_BCT_MAJOR_VERSION  0
#define TEGRABL_MB1_BCT_MINOR_VERSION  9

#pragma pack(1)

typedef struct  {
  UINT32    MajorVersion;
  UINT32    MinorVersion;
} TEGRABL_EARLY_BOOT_VARS_HEADER;

typedef struct {
  union {
    UINT64    FeatureFields;
    struct {
      UINT64    EgmEnable           : 1;
      UINT64    SpreadSpecEnable    : 1;
      UINT64    ModsSpEnable        : 1;
      UINT64    TpmEnable           : 1;
      UINT64    GpuSmmuBypassEnable : 1;
      UINT64    UartBaudRate        : 4;
      UINT64    EInjEnable          : 1;
      UINT64    FeatureFieldsUnused : 54;
    };
  };
} TEGRABL_FEATURE_DATA;

typedef struct {
  UINT8    UphyConfig[TEGRABL_SOC_MAX_SOCKETS][TEGRABL_MAX_UPHY_PER_SOCKET];
} TEGRABL_MB1BCT_UPHY_CONFIG;

typedef struct {
  union {
    UINT64    features;
    struct {
      /* ASPM L1 support */
      UINT64    EnableAspmL1           : 1;
      /* ASPM L1.1 support */
      UINT64    EnableAspmL1_1         : 1;
      /* ASPM L1.2 support */
      UINT64    EnableAspmL1_2         : 1;
      /* PCI-PM L1.2 support */
      UINT64    EnablePciPmL1_2        : 1;
      /* Availability of CLKREQ signal from RP to EP */
      UINT64    SupportsClkReq         : 1;
      /* Disable DLFE */
      UINT64    DisableDLFE            : 1;
      /* Enable ECRC in the PCIe hierarchy */
      UINT64    EnableECRC             : 1;
      /* Disable DPC at RP */
      UINT64    DisableDPCAtRP         : 1;
      /* Disable LTSSM link auto training */
      UINT64    DisableLTSSMAutoTrain  : 1;
      /* Mask Unsupported Request (UR) */
      UINT64    MaskUnsupportedRequest : 1;
      /* Mask Completer Abort (CA) */
      UINT64    MaskCompleterAbort     : 1;
      /* Supports Presence Detect */
      UINT64    SupportsPRSNT          : 1;
      /* Reserved */
      UINT64    reserved               : 52;
    };
  };

  UINT32    MaxSpeed;
  UINT32    MaxWidth;
  UINT8     SlotType;
  UINT16    SlotNum;
  UINT8     Reserved[13];
} TEGRABL_MB1BCT_PCIE_CONFIG;

typedef struct {
  UEFI_DECLARE_ALIGNED (TEGRABL_EARLY_BOOT_VARS_HEADER Header, 8);
  UEFI_DECLARE_ALIGNED (UINT8 Mb1BctHash[TEGRABL_MB1BCT_HASH_MAX_SIZE], 8);
  UEFI_DECLARE_ALIGNED (TEGRABL_FEATURE_DATA FeatureData, 8);
  UEFI_DECLARE_ALIGNED (UINT32 HvRsvdMemSize, 4);
  UEFI_DECLARE_ALIGNED (UINT32 UefiDebugLevel, 4);
  UEFI_DECLARE_ALIGNED (TEGRABL_MB1BCT_UPHY_CONFIG UphyConfig, 8);
  UEFI_DECLARE_ALIGNED (TEGRABL_MB1BCT_PCIE_CONFIG PcieConfig[TEGRABL_SOC_MAX_SOCKETS][TEGRABL_MAX_PCIE_PER_SOCKET], 8);
  UEFI_DECLARE_ALIGNED (UINT32 PerfVersion, 8);
} TH500_MB1_CONFIGURATION;

typedef struct {
  /** Flags for active page*/
  UINT8     Flags;
  /** Reserved */
  UINT8     Reserved[2];
  /** Checksum for entire early boot vars starting fron size */
  UINT8     Checksum;
  /** Size of early boot vars */
  UINT32    Size;
} TEGRABL_EARLY_BOOT_VARS_DATA_HEADER;

typedef struct {
  TEGRABL_EARLY_BOOT_VARS_DATA_HEADER    DataHeader;
  TH500_MB1_CONFIGURATION                Mb1Data;
} TEGRABL_EARLY_BOOT_VARIABLES_DATA;

typedef struct {
  union {
    UINT8                                ByteArray[TEGRABL_EARLY_BOOT_VARS_MAX_SIZE];
    TEGRABL_EARLY_BOOT_VARIABLES_DATA    Data;
  };
} TEGRABL_EARLY_BOOT_VARIABLES;

#pragma pack()

#endif // TH500_MB1_CONFIGURATION_H__
