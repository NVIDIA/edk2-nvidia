/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#define TEGRABL_MAX_MPAM_PARTID           5

#define TEGRABL_MB1_BCT_MAJOR_VERSION  0
#define TEGRABL_MB1_BCT_MINOR_VERSION  20

#pragma pack(1)

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

typedef struct  {
  UINT32    MajorVersion;
  UINT32    MinorVersion;
} TEGRABL_EARLY_BOOT_VARS_HEADER;

typedef struct {
  union {
    UINT64    FeatureFields;
    struct {
      UINT64    EgmEnable              : 1;
      UINT64    SpreadSpecEnable       : 1;
      UINT64    ModsSpEnable           : 1;
      UINT64    TpmEnable              : 1;
      UINT64    GpuSmmuBypassEnable    : 1;
      UINT64    UartBaudRate           : 4;
      UINT64    EInjEnable             : 1;
      UINT64    DisableChannelSparing  : 1;
      UINT64    EccAlgorithm           : 2;
      UINT64    MaxAllowedNumSpares    : 2;
      UINT64    DisplayAllSpareOptions : 1;
      UINT64    FeatureFieldsUnused    : 48;
    };
  };
} TEGRABL_FEATURE_DATA;

typedef struct {
  UINT8    UphyConfig[TEGRABL_SOC_MAX_SOCKETS][TEGRABL_MAX_UPHY_PER_SOCKET];
} TEGRABL_MB1BCT_UPHY_CONFIG;

typedef struct {
  union {
    UINT64    Features;
    struct {
      /* ASPM L1 support */
      UINT64    AdvertiseAspmL1        : 1;
      /* ASPM L1.1 support */
      UINT64    AdvertiseAspmL1_1      : 1;
      /* ASPM L1.2 support */
      UINT64    AdvertiseAspmL1_2      : 1;
      /* PCI-PM L1.2 support */
      UINT64    AdvertisePciPmL1_2     : 1;
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
      /* Advertise ACS capability */
      UINT64    AdvertiseACS           : 1;
      /* Enable OS native handling of AER errors */
      UINT64    OsNativeAER            : 1;
      /* Disable PME transition during warm reset */
      UINT64    DisableL23AtWarmReset  : 1;
      /* Disable DPC */
      UINT64    DisableDPC             : 1;
      /* Reserved */
      UINT64    FeaturesReserved       : 48;
    };
  };

  UINT32    MaxSpeed;
  UINT32    MaxWidth;
  UINT8     SlotType;
  UINT16    SlotNum;
  UINT16    Segment;
  UINT8     Reserved[11];
} TEGRABL_MB1BCT_PCIE_CONFIG;

typedef struct {
  union {
    UINT64    PartIdFields;
    struct {
      UINT64    CporWayMask : 12;
      UINT64    MinBw       : 7;
      UINT64    MaxBw       : 7;
    };
  };
} TEGRABL_MPAM_PARTID_CONFIG;

#pragma pack()

typedef struct {
  UEFI_DECLARE_ALIGNED (TEGRABL_EARLY_BOOT_VARS_HEADER Header, 8);
  UEFI_DECLARE_ALIGNED (UINT8 Mb1BctHash[TEGRABL_MB1BCT_HASH_MAX_SIZE], 8);
  UEFI_DECLARE_ALIGNED (TEGRABL_FEATURE_DATA FeatureData, 8);
  UEFI_DECLARE_ALIGNED (UINT32 HvRsvdMemSize, 4);
  UEFI_DECLARE_ALIGNED (UINT32 UefiDebugLevel, 4);
  UEFI_DECLARE_ALIGNED (TEGRABL_MB1BCT_UPHY_CONFIG UphyConfig, 8);
  UEFI_DECLARE_ALIGNED (TEGRABL_MB1BCT_PCIE_CONFIG PcieConfig[TEGRABL_SOC_MAX_SOCKETS][TEGRABL_MAX_PCIE_PER_SOCKET], 8);
  UEFI_DECLARE_ALIGNED (UINT32 PerfVersion, 8);
  UEFI_DECLARE_ALIGNED (UINT32 ActiveCores[TEGRABL_SOC_MAX_SOCKETS], 8);
  UEFI_DECLARE_ALIGNED (UINT32 NvIntConfig0, 4);
  UEFI_DECLARE_ALIGNED (UINT32 NvIntConfig1, 4);
  UEFI_DECLARE_ALIGNED (TEGRABL_MPAM_PARTID_CONFIG MpamConfig[TEGRABL_MAX_MPAM_PARTID], 8);
  UEFI_DECLARE_ALIGNED (UINT32 NvIntConfig2, 4);
  UEFI_DECLARE_ALIGNED (UINT32 HvMinEgmSize, 4);
  UEFI_DECLARE_ALIGNED (UINT32 HvVirtUefiSize, 4);
} TH500_MB1_CONFIGURATION;

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

#endif // TH500_MB1_CONFIGURATION_H__
