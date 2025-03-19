/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2019-2025, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TH500_RESOURCE_CONFIG_PRIVATE_H__
#define __TH500_RESOURCE_CONFIG_PRIVATE_H__

#include <Uefi/UefiBaseType.h>
#include <Library/PlatformResourceLib.h>
#include <TH500/TH500Definitions.h>
#include <TH500/TH500MB1Configuration.h>

// NOTE The register map would change
#define TEGRA_UART_SUPPORT_FLAG  0x6        // UART_0..1

#define MAX_RETIRED_DRAM_PAGES  4096

#define TEGRA_UART_ADDRESS_0  0x0c280000
#define TEGRA_UART_ADDRESS_1  0x03100000

#define TH500_BOOT_CHAIN_SCRATCH_OFFSET  0x3cc // SCRATCH_SECURE_RSV109_SCRATCH
#define BOOT_CHAIN_MAX                   2
#define BOOT_CHAIN_BIT_FIELD_LO          4
#define BOOT_CHAIN_BIT_FIELD_HI          5
#define BOOT_CHAIN_STATUS_LO             0
#define BOOT_CHAIN_STATUS_HI             3
#define BOOT_CHAIN_GOOD                  0
#define BOOT_CHAIN_BAD                   1

#define CARVEOUT_NONE                     0
#define CARVEOUT_EGM                      1
#define CARVEOUT_BPMP_CPUTZ               2
#define CARVEOUT_BPMP_CPU_NS              3
#define CARVEOUT_CCPLEX_INTERWORLD_SHMEM  4
#define CARVEOUT_MB2_PARAMS               5
#define CARVEOUT_BPMP                     6
#define CARVEOUT_BPMP_PSC                 7
#define CARVEOUT_PSC_TZ                   8
#define CARVEOUT_PSC                      9
#define CARVEOUT_ETR                      10
#define CARVEOUT_UNUSED_GSC11             11
#define CARVEOUT_UNUSED_GSC12             12
#define CARVEOUT_UNUSED_GSC13             13
#define CARVEOUT_MCE_USB_CTRL             14
#define CARVEOUT_UNUSED_GSC15             15
#define CARVEOUT_UNUSED_GSC16             16
#define CARVEOUT_UNUSED_GSC17             17
#define CARVEOUT_UNUSED_GSC18             18
#define CARVEOUT_UNUSED_GSC19             19
#define CARVEOUT_UNUSED_GSC20             20
#define CARVEOUT_UNUSED_GSC21             21
#define CARVEOUT_UNUSED_GSC22             22
#define CARVEOUT_UNUSED_GSC23             23
#define CARVEOUT_UNUSED_GSC24             24
#define CARVEOUT_UNUSED_GSC25             25
#define CARVEOUT_UNUSED_GSC26             26
#define CARVEOUT_TEMP_MB2                 27
#define CARVEOUT_UNUSED_GSC28             28
#define CARVEOUT_UNUSED_GSC29             29
#define CARVEOUT_UNUSED_GSC30             30
#define CARVEOUT_UNUSED_GSC31             31
#define CARVEOUT_UNUSED_GSC_LITE32        32
#define CARVEOUT_MTS                      33
#define CARVEOUT_TZDRAM                   34

#define CARVEOUT_PROFILING          35
#define CARVEOUT_RCM_BLOB           36
#define CARVEOUT_UEFI               37
#define CARVEOUT_CCPLEX_LA_BUFFERS  38
#define CARVEOUT_OS                 39
#define CARVEOUT_HV                 40
#define CARVEOUT_RSVD1              41
#define CARVEOUT_RSVD2              42
#define CARVEOUT_RSVD3              43
#define CARVEOUT_RSVD4              44
#define CARVEOUT_RSVD5              45
#define CARVEOUT_RSVD6              46
#define CARVEOUT_RSVD7              47
#define CARVEOUT_RSVD8              48
#define CARVEOUT_RSVD9              49
#define CARVEOUT_RSVD10             50
#define CARVEOUT_OEM_COUNT          51

typedef struct {
  UINT64                  EgmNumRetiredPages;
  EFI_PHYSICAL_ADDRESS    EgmRetiredPageAddress[MAX_RETIRED_DRAM_PAGES];
} TH500_EGM_RETIRED_PAGES;

typedef enum {
  Th500MemoryModeNormal,
  Th500MemoryModeEgmNoHv,
  Th500MemoryModeEgmWithHv,
  Th500MemoryModeMax
} TH500_MEMORY_MODE;

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
  /* Partition Device (QSPI/RCM/NONE). On TH500 this can be QSPI only */
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
  UINT16    ManufacturerId;
  UINT8     Rank;
  UINT8     PartNumber[30];
  UINT8     Reserved[3];
} TEGRABL_DRAM_INFO_V0;

typedef struct {
  UINT64    SerialNumber[MAX_DIMMS_PER_SOCKET];
  UINT64    ChannelMap[MAX_DIMMS_PER_SOCKET];
  UINT16    TotalWidth;
  UINT16    DataWidth;
  UINT16    ManufacturerId;
  UINT8     Rank;
  UINT8     Attribute[MAX_DIMMS_PER_SOCKET];
  UINT8     PartNumber[MAX_DIMMS_PER_SOCKET][30];
  UINT8     NumModules;
  UINT8     Reserved[36];
} TEGRABL_DRAM_INFO_V1;

#pragma pack()
typedef struct {
  /**< version */
  UEFI_DECLARE_ALIGNED (UINT32 Version, 4);

  /**< Uart instance */
  UEFI_DECLARE_ALIGNED (UINT32 Uart_Instance, 4);

  /**< CVM EEPROM data */
  UEFI_DECLARE_ALIGNED (TEGRABL_FRU_EEPROM_DATA CvmEeprom[TH500_MAX_SOCKETS], 8);

  /**< CVB EEPROM data */
  UEFI_DECLARE_ALIGNED (TEGRABL_FRU_EEPROM_DATA CvbEeprom, 8);

  /**< Address of list of physical addresses of retired pages */
  UEFI_DECLARE_ALIGNED (UINT64 RetiredDramPageListAddr[TH500_MAX_SOCKETS], 8);

  /**< Bit mask to specify which sockets are enabled */
  UEFI_DECLARE_ALIGNED (UINT32 SocketMask, 8);

  /**< Base and size information of the DRAM connected to each socket */
  UEFI_DECLARE_ALIGNED (TEGRABL_SDRAM_INFO_DATA SdramInfo[TH500_MAX_SOCKETS], 8);

  /**
   * physical address and size of the carveouts allocated on each socket.
   * If carveout is not allocated on a particular socket then base and size
   * would be set to zero.
   */
  UEFI_DECLARE_ALIGNED (TEGRABL_CARVEOUT_INFO CarveoutInfo[TH500_MAX_SOCKETS][CARVEOUT_OEM_COUNT], 8);

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
  UEFI_DECLARE_ALIGNED (UINT8 UphyLinkChecksumStatusp[TH500_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_PARTITION_DESC PartitionInfo[TEGRABL_BINARY_MAX][TEGRABL_BINARY_COPY_MAX], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_EARLY_BOOT_VARIABLES EarlyBootVariables[TH500_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_TPM_COMMIT_LOG EarlyTpmCommitLog, 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_DRAM_INFO_V0 DramInfo[TH500_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_EARLY_BOOT_VARIABLES EarlyBootVariablesDefaults[TH500_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (UINT32 UniqueId[TH500_MAX_SOCKETS][UID_NUM_DWORDS], 8);
} TEGRA_CPUBL_PARAMS_V0;

typedef struct {
  /**< version */
  UEFI_DECLARE_ALIGNED (UINT32 Version, 4);

  /**< Uart instance */
  UEFI_DECLARE_ALIGNED (UINT32 Uart_Instance, 4);

  /**< CVM EEPROM data */
  UEFI_DECLARE_ALIGNED (TEGRABL_FRU_EEPROM_DATA CvmEeprom[TH500_MAX_SOCKETS], 8);

  /**< CVB EEPROM data */
  UEFI_DECLARE_ALIGNED (TEGRABL_FRU_EEPROM_DATA CvbEeprom, 8);

  /**< Address of list of physical addresses of retired pages */
  UEFI_DECLARE_ALIGNED (UINT64 RetiredDramPageListAddr[TH500_MAX_SOCKETS], 8);

  /**< Bit mask to specify which sockets are enabled */
  UEFI_DECLARE_ALIGNED (UINT32 SocketMask, 8);

  /**< Base and size information of DRAMs connected to each socket */
  UEFI_DECLARE_ALIGNED (TEGRABL_SDRAM_INFO_DATA SdramInfo[TH500_MAX_SOCKETS], 8);

  /**
   * physical address and size of the carveouts allocated on each socket.
   * If carveout is not allocated on a particular socket then base and size
   * would be set to zero.
   */
  UEFI_DECLARE_ALIGNED (TEGRABL_CARVEOUT_INFO CarveoutInfo[TH500_MAX_SOCKETS][CARVEOUT_OEM_COUNT], 8);

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
  UEFI_DECLARE_ALIGNED (UINT8 UphyLinkChecksumStatusp[TH500_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_PARTITION_DESC PartitionInfo[TEGRABL_BINARY_MAX][TEGRABL_BINARY_COPY_MAX], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_EARLY_BOOT_VARIABLES EarlyBootVariables[TH500_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_TPM_COMMIT_LOG EarlyTpmCommitLog, 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_DRAM_INFO_V1 DramInfo[TH500_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (TEGRABL_EARLY_BOOT_VARIABLES EarlyBootVariablesDefaults[TH500_MAX_SOCKETS], 8);

  UEFI_DECLARE_ALIGNED (UINT32 UniqueId[TH500_MAX_SOCKETS][UID_NUM_DWORDS], 8);
} TEGRA_CPUBL_PARAMS_V1;

typedef struct {
  union {
    TEGRA_CPUBL_PARAMS_V0    v0;
    TEGRA_CPUBL_PARAMS_V1    v1;
    struct {
      UEFI_DECLARE_ALIGNED (UINT32 Version, 4);
    } common;
  };
} TEGRA_CPUBL_PARAMS;

#define CPUBL_VERSION(PARAMS)                (((TEGRA_CPUBL_PARAMS *)PARAMS)->common.Version)
#define CPUBL_PARAMS(PARAMS, FIELD)          (((CPUBL_VERSION(PARAMS)) == 0)? \
                                              ((TEGRA_CPUBL_PARAMS *)PARAMS)->v0.FIELD:\
                                              ((CPUBL_VERSION(PARAMS)) == 1)? \
                                              ((TEGRA_CPUBL_PARAMS *)PARAMS)->v1.FIELD:\
                                               0)
#define ADDR_OF_CPUBL_PARAMS(PARAMS, FIELD)  (((CPUBL_VERSION(PARAMS)) == 0)? \
                                               &((TEGRA_CPUBL_PARAMS *)PARAMS)->v0.FIELD:\
                                              ((CPUBL_VERSION(PARAMS)) == 1)? \
                                               &((TEGRA_CPUBL_PARAMS *)PARAMS)->v1.FIELD:\
                                               NULL)
#define SIZE_OF_CPUBL_PARAMS(PARAMS, FIELD)  (((CPUBL_VERSION(PARAMS)) == 0)? \
                                               sizeof(((TEGRA_CPUBL_PARAMS *)PARAMS)->v0.FIELD):\
                                              ((CPUBL_VERSION(PARAMS)) == 1)? \
                                               sizeof(((TEGRA_CPUBL_PARAMS *)PARAMS)->v1.FIELD):\
                                               0)
#endif //__TH500_RESOURCE_CONFIG_PRIVATE_H__
