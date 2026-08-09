/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T410_RESOURCE_CONFIG_PRIVATE_H__
#define __T410_RESOURCE_CONFIG_PRIVATE_H__

#include <Uefi/UefiBaseType.h>
#include <Library/PlatformResourceLib.h>
#include <T410/T410Definitions.h>
#include <TH500/TH500MB1Configuration.h>

// NOTE The register map would change
#define TEGRA_UART_SUPPORT_FLAG  0x6        // UART_0..1

#define MAX_RETIRED_DRAM_PAGES  4096

#define TEGRA_UART_ADDRESS_0  0x0c280000
#define TEGRA_UART_ADDRESS_1  0x03100000

#define T410_BOOT_CHAIN_SCRATCH_OFFSET  0x908C // // SCRATCH_SECURE_RAM_L2_35_0

#define BOOT_CHAIN_MAX           2
#define BOOT_CHAIN_BIT_FIELD_LO  4
#define BOOT_CHAIN_BIT_FIELD_HI  5
#define BOOT_CHAIN_STATUS_LO     0
#define BOOT_CHAIN_STATUS_HI     3
#define BOOT_CHAIN_GOOD          0
#define BOOT_CHAIN_BAD           1

/* Permanent carveouts */
#define CARVEOUT_ROOT_RMM            1
#define CARVEOUT_CSWP                2
#define CARVEOUT_RAS_APEI            3
#define CARVEOUT_GPT                 4
#define CARVEOUT_BPMP_RAS            5
#define CARVEOUT_BPMP                6
#define CARVEOUT_BPMP_ROOT           7
#define CARVEOUT_PSC_HSS_KEYROT      8
#define CARVEOUT_MSEQ_DATA           9
#define CARVEOUT_EMC_TRAINING_R0     10
#define CARVEOUT_EMC_TRAINING_R1     11
#define CARVEOUT_MSEQP               12
#define CARVEOUT_RAS_HSS_SCRUB       13
#define CARVEOUT_RAS_HSS_MSEQ        14
#define CARVEOUT_ETR                 15
#define CARVEOUT_PSC                 16
#define CARVEOUT_PSC_CPUTZ           17
#define CARVEOUT_LFA_STAGING_NVAUXP  18
#define CARVEOUT_LFA_STAGING_CCPLEX  19
/* 20 to 32 reserved */
#define CARVEOUT_ROOT_SRAM                33
#define CARVEOUT_RAS_UEFIMM               34
#define CARVEOUT_RMM                      35
#define CARVEOUT_CCPLEX_INTERWORLD_SHMEM  36
#define CARVEOUT_ROOT_RAS                 37
#define CARVEOUT_TZDRAM                   38
#define CARVEOUT_BPMP_CPUNS               39
#define CARVEOUT_RAS                      40
#define CARVEOUT_PSC_ROOT                 41
#define CARVEOUT_L1L2_PT                  42
#define CARVEOUT_VPR                      43
/* Unprotected carveouts */
#define CARVEOUT_UEFI           44
#define CARVEOUT_PROFILING      45
#define CARVEOUT_OS             46
#define CARVEOUT_PRE_UEFI_DIAG  47
/* Other carveouts */
#define CARVEOUT_IST_USB  48
/* 40 to 60 reserved */
#define CARVEOUT_OEM_COUNT  60

#define CARVEOUT_ONE  CARVEOUT_ROOT_RMM

typedef struct {
  UINT64    Base;
  UINT64    Size;
  union {
    struct {
      UINT64    EccProtected : 1;
      UINT64    Reserved     : 63;
    };

    UINT64    Flags;
  };
} TEGRABL_CARVEOUT_INFO;

#define BLOCK_SIZE                          (512)
#define PRIMARY_COPY                        (0)
#define TEGRABL_BINARY_MAX                  (33U)
#define TEGRABL_BINARY_COPY_MAX             (4)
#define TEGRABL_PARTITION_DEVICE_TYPE_QSPI  (1)

#define MAX_DIGEST_SIZE       48
#define MAX_NUM_MEASUREMENTS  50

#define ALGO_TYPE_SHA384  0
#define ALGO_TYPE_SHA256  1

typedef struct {
  /* Partition Device (QSPI/RCM/NONE). On T410 this can be QSPI only */
  UINT32    DeviceType;

  /* Device Instance. In QSPI cases, this represents which NOR-FLASH
   * device the partition is on. The upper 8 bits represent the Chip
   * Select Number , lower 8 bits represents the QSPI instance.
   */
  UINT16    DeviceInstance;
  /* Start LBA of partition. */
  UINT32    StartBlock;
  /* Partiton Size. */
  UINT32    Size;
  /* MB2 may call this "Attributes", but for now this field is reserved. */
  UINT32    Reserved;
} TEGRABL_PARTITION_DESC;

#pragma pack(1)
typedef struct  {
  UINT64    Base;
  UINT64    Size;
} TEGRABL_SDRAM_INFO_DATA;

#define TEGRABL_TEGRABL_FRU_EEPROM_DATA_SIZE  256

#pragma pack(1)
typedef struct {
  UINT8     Data[TEGRABL_TEGRABL_FRU_EEPROM_DATA_SIZE];
  UINT32    DataSize;
  UINT32    Reserved;
} TEGRABL_FRU_EEPROM_DATA;

typedef struct {
  UINT32    MagicId;                 /// Unique ID to indentify each measurement
  UINT32    SocketId;                /// Socket id where the measurement was made
  UINT32    PcrIndex;                /// PCR index to which the measurement was extended
  UINT8     Digest[MAX_DIGEST_SIZE]; /// if (algo_type ? SHA384) then consume 48 bytes else 32 bytes.
} TEGRABL_TPM_COMMIT_LOG_ENTRY;

typedef struct {
  UINT32                          AlgoType;        /// if (algo_type = 0) then SHA384 else SHA256
  UINT32                          NumMeasurements; /// Total number of entries in event log
  TEGRABL_TPM_COMMIT_LOG_ENTRY    Measurements[MAX_NUM_MEASUREMENTS];
} TEGRABL_TPM_COMMIT_LOG;

typedef struct {
  UINT64    SerialNumber;
  UINT16    TotalWidth;
  UINT16    DataWidth;
  UINT8     ManufacturerId;
  UINT8     Rank;
  UINT8     Reserved[34];
} TEGRABL_DRAM_INFO;

#pragma pack()
typedef struct {
  /**< version */
  UEFI_DECLARE_ALIGNED (UINT32 Version, 4);

  /**< Uart instance */
  UEFI_DECLARE_ALIGNED (UINT32 Uart_Instance, 4);

  /**< CVM EEPROM data */
  UEFI_DECLARE_ALIGNED (TEGRABL_FRU_EEPROM_DATA CvmEeprom[T410_MAX_SOCKETS], 8);

  /**< CVB EEPROM data */
  UEFI_DECLARE_ALIGNED (TEGRABL_FRU_EEPROM_DATA CvbEeprom, 8);

  /**< Address of list of physical addresses of retired pages */
  UEFI_DECLARE_ALIGNED (UINT64 RetiredDramPageListAddr[T410_MAX_SOCKETS], 8);

  /**< Bit mask to specify which sockets are enabled */
  UEFI_DECLARE_ALIGNED (UINT32 SocketMask, 8);

  /**< Base and size information of the DRAM connected to each socket */
  UEFI_DECLARE_ALIGNED (TEGRABL_SDRAM_INFO_DATA SdramInfo[T410_MAX_SOCKETS], 8);

  /**
   * physical address and size of the carveouts allocated on each socket.
   * If carveout is not allocated on a particular socket then base and size
   * would be set to zero.
   */
  UEFI_DECLARE_ALIGNED (TEGRABL_CARVEOUT_INFO CarveoutInfo[T410_MAX_SOCKETS][CARVEOUT_OEM_COUNT], 8);

  /**< Feature flags */
  UEFI_DECLARE_ALIGNED (
    struct {
    union {
      UINT64 FeatureFlagRaw1;
      struct {
        /**
         * Boot chain selection mode
         * 0: BCT Marker Mode
         * 1: GPIO Mode
         */
        UINT64 BootChainSelectionMode: 1;
        UINT64 FeatureFlagRaw1Reserved: 63;
      };
    };

    UINT64 FeatureFlagRaw2;
  },
    8
    );

  /**
   * Uphy link checksum status bit mask from each socket.
   * There are 6 uphy controllers per socket. A bit is set when
   * checksum verification is failed for corresponding uphy controller,
   * otherwise checksum verification is passed.
   */
  UEFI_DECLARE_ALIGNED (UINT8 UphyLinkChecksumStatusp[T410_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_PARTITION_DESC PartitionInfo[TEGRABL_BINARY_MAX][TEGRABL_BINARY_COPY_MAX], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_EARLY_BOOT_VARIABLES EarlyBootVariables[T410_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_TPM_COMMIT_LOG EarlyTpmCommitLog, 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_DRAM_INFO DramInfo[T410_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_EARLY_BOOT_VARIABLES EarlyBootVariablesDefaults[T410_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (UINT32 UniqueId[T410_MAX_SOCKETS][UID_NUM_DWORDS], 8);
} TEGRA_CPUBL_PARAMS;

#endif //__T410_RESOURCE_CONFIG_PRIVATE_H__
