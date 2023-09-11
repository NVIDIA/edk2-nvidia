/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T234_RESOURCE_CONFIG_PRIVATE_H__
#define __T234_RESOURCE_CONFIG_PRIVATE_H__

#include <Uefi/UefiBaseType.h>
#include <Library/BaseCryptLib.h>
#include <Library/PlatformResourceLib.h>

#define BOOT_CHAIN_MAX           2
#define BOOT_CHAIN_BIT_FIELD_LO  4
#define BOOT_CHAIN_BIT_FIELD_HI  5
#define BOOT_CHAIN_STATUS_LO     0
#define BOOT_CHAIN_STATUS_HI     3
#define BOOT_CHAIN_GOOD          0
#define BOOT_CHAIN_BAD           1

#define BL_MAGIC_BIT_FIELD_LO       0
#define BL_MAGIC_BIT_FIELD_HI       15
#define BL_UPDATE_BR_BCT_BIT_FIELD  26
#define SR_BL_MAGIC                 0x4EF1UL

#define TEGRA_UART_SUPPORT_FLAG  0x77E    // UART_A..J except _G
#define TEGRA_UART_SUPPORT_SBSA  0x600

#define TEGRA_UART_ADDRESS_A  0x03100000
#define TEGRA_UART_ADDRESS_B  0x03110000
#define TEGRA_UART_ADDRESS_C  0x0c280000
#define TEGRA_UART_ADDRESS_D  0x03130000
#define TEGRA_UART_ADDRESS_E  0x03140000
#define TEGRA_UART_ADDRESS_F  0x03150000
#define TEGRA_UART_ADDRESS_G  0x0c290000
#define TEGRA_UART_ADDRESS_H  0x03170000
#define TEGRA_UART_ADDRESS_I  0x031d0000
#define TEGRA_UART_ADDRESS_J  0x0c270000

#define TEGRABL_MAX_VERSION_STRING  128       /* chars including null */
#define NUM_DRAM_BAD_PAGES          1024

#define BRBCT_UNSIGNED_CUSTOMER_DATA_SIZE  1024
#define BRBCT_SIGNED_CUSTOMER_DATA_SIZE    1024
#define BRBCT_CUSTOMER_DATA_SIZE           (BRBCT_UNSIGNED_CUSTOMER_DATA_SIZE +\
                                           BRBCT_SIGNED_CUSTOMER_DATA_SIZE)
#define MAX_CPUBL_OEM_FW_RATCHET_INDEX_V1  112
#define MAX_CPUBL_OEM_FW_RATCHET_INDEX_V2  304

#define T234_FUSE_BASE_ADDRESS  0x03810000
#define FUSE_OPT_ISP_DISABLE    0x4d8
#define FUSE_OPT_NVENC_DISABLE  0x4e0
#define FUSE_OPT_PVA_DISABLE    0x4e8
#define FUSE_OPT_DLA_DISABLE    0x4f0
#define FUSE_OPT_CV_DISABLE     0x4f8
#define FUSE_OPT_NVDEC_DISABLE  0x5f0

/*macro carve_out_type*/
#define CARVEOUT_NONE                       0
#define CARVEOUT_NVDEC                      1
#define CARVEOUT_WPR1                       2
#define CARVEOUT_WPR2                       3
#define CARVEOUT_TSEC                       4
#define CARVEOUT_XUSB                       5
#define CARVEOUT_BPMP                       6
#define CARVEOUT_APE                        7
#define CARVEOUT_SPE                        8
#define CARVEOUT_SCE                        9
#define CARVEOUT_APR                        10
#define CARVEOUT_BPMP_DCE                   11
#define CARVEOUT_UNUSED3                    12
#define CARVEOUT_BPMP_RCE                   13
#define CARVEOUT_BPMP_MCE                   14
#define CARVEOUT_ETR                        15
#define CARVEOUT_BPMP_SPE                   16
#define CARVEOUT_RCE                        17
#define CARVEOUT_BPMP_CPUTZ                 18
#define CARVEOUT_UNUSED1                    19
#define CARVEOUT_DCE                        20
#define CARVEOUT_BPMP_PSC                   21
#define CARVEOUT_PSC                        22
#define CARVEOUT_NV_SC7                     23
#define CARVEOUT_CAMERA_TASKLIST            24
#define CARVEOUT_BPMP_SCE                   25
#define CARVEOUT_CV_GOS                     26
#define CARVEOUT_PSC_TSEC                   27
#define CARVEOUT_CCPLEX_INTERWORLD_SHMEM    28
#define CARVEOUT_FSI                        29
#define CARVEOUT_MCE                        30
#define CARVEOUT_CCPLEX_IST                 31
#define CARVEOUT_TSEC_HOST1X                32
#define CARVEOUT_PSC_TZ                     33
#define CARVEOUT_SCE_CPU_NS                 34
#define CARVEOUT_OEM_SC7                    35
#define CARVEOUT_SYNCPT_IGPU_RO             36
#define CARVEOUT_SYNCPT_IGPU_NA             37
#define CARVEOUT_VM_ENCRYPT                 38
#define CARVEOUT_BLANKET_NSDRAM             CARVEOUT_VM_ENCRYPT
#define CARVEOUT_CCPLEX_SMMU_PTW            39
#define CARVEOUT_DISP_EARLY_BOOT_FB         CARVEOUT_CCPLEX_SMMU_PTW
#define CARVEOUT_BPMP_CPU_NS                40
#define CARVEOUT_FSI_CPU_NS                 41
#define CARVEOUT_TSEC_DCE                   42
#define CARVEOUT_TZDRAM                     43
#define CARVEOUT_VPR                        44
#define CARVEOUT_MTS                        45
#define CARVEOUT_RCM_BLOB                   46
#define CARVEOUT_UEFI                       47
#define CARVEOUT_UEFI_MM_IPC                48
#define CARVEOUT_DRAM_ECC_TEST              49
#define CARVEOUT_PROFILING                  50
#define CARVEOUT_OS                         51
#define CARVEOUT_FSI_KEY_BLOB               52
#define CARVEOUT_TEMP_MB2RF                 53
#define CARVEOUT_TEMP_MB2_LOAD              54
#define CARVEOUT_TEMP_MB2_PARAMS            55
#define CARVEOUT_TEMP_MB2_IO_BUFFERS        56
#define CARVEOUT_TEMP_MB2RF_DATA            57
#define CARVEOUT_TEMP_MB2                   58
#define CARVEOUT_TEMP_MB2_SYSRAM_DATA       59
#define CARVEOUT_TSEC_CCPLEX                60
#define CARVEOUT_TEMP_MB2_APLT_LOAD         61
#define CARVEOUT_TEMP_MB2_APLT_PARAMS       62
#define CARVEOUT_TEMP_MB2_APLT_IO_BUFFERS   63
#define CARVEOUT_TEMP_MB2_APLT_SYSRAM_DATA  64
#define CARVEOUT_GR                         65
#define CARVEOUT_TEMP_QB_DATA               66
#define CARVEOUT_TEMP_QB_IO_BUFFER          67
#define CARVEOUT_ATF_FSI                    68
#define CARVEOUT_OPTEE_DTB                  69
#define CARVEOUT_UNUSED2                    70
#define CARVEOUT_UNUSED4                    71
#define CARVEOUT_RAM_OOPS                   72
#define CARVEOUT_OEM_COUNT                  73

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

#pragma pack(1)
typedef struct {
  UINT32     MagicHeader;
  UINT32     ClockSource;
  UINT32     ClockDivider;
  UINT32     ClockSourceFrequency;
  UINT32     InterfaceFrequency;
  UINT32     MaxBusWidth;
  BOOLEAN    EnableDdrRead;
  UINT32     DmaType;
  UINT32     FifoAccessMode;
  UINT32     ReadDummyCycles;
  UINT32     Trimmer1Value;
  UINT32     Rrimmer2Value;
  UINT8      Reserved[8];
} TEGRABL_DEVICE_CONFIG_QSPI_FLASH_PARAMS;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT32     MagicHeader;
  UINT32     ClockSource;
  UINT32     ClockFrequency;
  UINT32     BestMode;
  UINT32     PdOffset;
  UINT32     PuOffset;
  BOOLEAN    DqsTrimHs400;
  BOOLEAN    EnableStrobeHs400;
  UINT8      Reserved[8];
} TEGRABL_DEVICE_CONFIG_SDMMC_PARAMS;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT32    MagicHeader;
  UINT8     TransferSpeed;
  UINT8     Reserved[8];
} TEGRABL_DEVICE_CONFIG_SATA_PARAMS;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT32     MagicHeader;
  UINT8      MaxHsMode;
  UINT8      MaxPwmMode;
  UINT8      MaxActiveLanes;
  UINT32     PageAlignSize;
  BOOLEAN    EnableHsModes;
  BOOLEAN    EnableFastAutoMode;
  BOOLEAN    EnableHsRateB;
  BOOLEAN    EnableHsRateA;
  BOOLEAN    SkipHsModeSwitch;
  UINT8      Reserved[8];
} TEGRABL_DEVICE_CONFIG_UFS_PARAMS;
#pragma pack()

#pragma pack(1)
typedef struct  {
  UINT32                                     Version;
  TEGRABL_DEVICE_CONFIG_SDMMC_PARAMS         Sdmmc;
  TEGRABL_DEVICE_CONFIG_QSPI_FLASH_PARAMS    QspiFlash;
  TEGRABL_DEVICE_CONFIG_UFS_PARAMS           Ufs;
  TEGRABL_DEVICE_CONFIG_SATA_PARAMS          Sata;
} TEGRABL_DEVICE_CONFIG_PARAMS;
#pragma pack()

typedef union {
  UINT32    Version;
  UINT8     VersionStr[TEGRABL_MAX_VERSION_STRING];
} TEGRABL_VERSION;

typedef struct {
  /**< sha512 digest */
  UEFI_DECLARE_ALIGNED (UINT8 Digest[SHA512_DIGEST_SIZE], 8);

  /**< version */
  UEFI_DECLARE_ALIGNED (UINT32 Version, 4);

  /**< Uart instance */
  UEFI_DECLARE_ALIGNED (UINT32 Uart_Instance, 4);

  /**< Enable logs */
  UEFI_DECLARE_ALIGNED (UINT32 EnableLog, 4);

  /**< device config params from mb1 bct */
  UEFI_DECLARE_ALIGNED (TEGRABL_DEVICE_CONFIG_PARAMS DeviceConfig, 8);

  /**< Address of i2c bus frequecy from mb1 bct */
  UEFI_DECLARE_ALIGNED (UINT64 I2cBusFrequencyAddress, 8);

  /**< Address of controller pad settings */
  UEFI_DECLARE_ALIGNED (UINT64 ControllerProdSettings, 8);

  /**< Total size of controller pad settings */
  UEFI_DECLARE_ALIGNED (UINT64 ControllerProdSettingsSize, 8);

  /**< Parameters for Secure_OS/TLK passed via GPR */
  UEFI_DECLARE_ALIGNED (UINT64 SecureOsParams[4], 8);
  UEFI_DECLARE_ALIGNED (UINT64 SecureOsStart, 8);

  /**< If tos loaded by mb2 has secureos or not. */
  UEFI_DECLARE_ALIGNED (UINT32 SecureosType, 4);

  /**< SDRAM size in bytes */
  UEFI_DECLARE_ALIGNED (UINT64 SdramSize, 8);

  /**< physical address and size of the carveouts */
  UEFI_DECLARE_ALIGNED (TEGRABL_CARVEOUT_INFO CarveoutInfo[CARVEOUT_OEM_COUNT], 8);

  /**< Indicate whether DRAM ECC page blacklisting feature is enabled
     or not
   */
  UEFI_DECLARE_ALIGNED (
    union {
    UINT64 FeatureFlagRaw;
    struct {
      UINT64 EnableDramPageRetirement: 1;
      UINT64 EnableCombinedUart: 1;
      UINT64 EnableDramStagedScrubbing: 1;
      UINT64 SwitchBootchain: 1;
      UINT64 ResetToRecovery: 1;
      UINT64 EnableSpe: 1;
      UINT64 EnableSce: 1;
      UINT64 EnableRce: 1;
      UINT64 EnableDce: 1;
      UINT64 EnableApe: 1;
      UINT64 EnableFsi: 1;
      UINT64 EnableBlanketNsdramCarveout: 1;
      UINT64 EnableNsdramEncryption: 1;
      UINT64 Unused: 51;
    } FeatureFlagData;
  },
    8
    );

  /**< Start address of SDRAM params used in MB1 as per RAMCODE */
  UEFI_DECLARE_ALIGNED (UINT64 SdramParamsOffset, 8);

  /**< Start address of DRAM ECC page blacklisting information
     structure
   */
  UEFI_DECLARE_ALIGNED (UINT64 DramPageRetirementInfoAddress, 8);

  /**< Start address of Golden register data region */
  UEFI_DECLARE_ALIGNED (UINT64 GoldenRegisterAddress, 8);

  /**< Size of Golden register data region */
  UEFI_DECLARE_ALIGNED (UINT32 GoldenRegisterSize, 8);

  /**< Start offset of unallocated/unused data in CPUBL carveout */
  UEFI_DECLARE_ALIGNED (UINT64 CpublCarveoutSafeEndOffset, 8);

  /**< Boot type set by nv3pserver based on boot command from host. */
  UEFI_DECLARE_ALIGNED (UINT32 RecoveryBootType, 8);

  /**< Boot mode can be cold boot, or RCM */
  UEFI_DECLARE_ALIGNED (UINT32 BootType, 8);

  /**< Uart_base Address for debug prints */
  UEFI_DECLARE_ALIGNED (UINT64 EarlyUartAddr, 8);

  /**< mb1 bct version information */
  UEFI_DECLARE_ALIGNED (UINT32 Mb1BctVersion, 8);

  /**< mb1 version */
  UEFI_DECLARE_ALIGNED (UINT8 Mb1Version[TEGRABL_MAX_VERSION_STRING], 8);

  /**< mb2 version */
  UEFI_DECLARE_ALIGNED (UINT8 Mb2Version[TEGRABL_MAX_VERSION_STRING], 8);

  UEFI_DECLARE_ALIGNED (UINT8 CpublVersion[TEGRABL_MAX_VERSION_STRING], 8);

  /**< Reset reason as read from PMIC */
  UEFI_DECLARE_ALIGNED (UINT32 PmicRstReason, 8);

  /**< BRBCT unsigned and signed customer data */
  UEFI_DECLARE_ALIGNED (UINT8 BrbctCustomerData[BRBCT_CUSTOMER_DATA_SIZE], 8);

  /** <BRBCT unsigned customer data valid */
  UEFI_DECLARE_ALIGNED (UINT8 BrbctUnsignedCustomerDataValid, 8);

  /** <BRBCT signed customer data valid */
  UEFI_DECLARE_ALIGNED (UINT8 BrbctSignedCustomerDataValid, 8);

  /**< SDRAM base address*/
  UEFI_DECLARE_ALIGNED (UINT64 SdramBase, 8);

  /**< EEPROM data*/
  UEFI_DECLARE_ALIGNED (TEGRABL_EEPROM_DATA Eeprom, 8);
} TEGRA_CPUBL_PARAMS_V0;

typedef struct {
  /**< sha512 digest */
  UEFI_DECLARE_ALIGNED (UINT8 Digest[SHA512_DIGEST_SIZE], 8);

  /**< version */
  UEFI_DECLARE_ALIGNED (UINT32 Version, 4);

  /**< Uart instance */
  UEFI_DECLARE_ALIGNED (UINT32 Uart_Instance, 4);

  /**< If tos loaded by mb2 has secureos or not. */
  UEFI_DECLARE_ALIGNED (UINT32 SecureosType, 4);

  /**< Boot mode can be cold boot, or RCM */
  UEFI_DECLARE_ALIGNED (UINT32 BootType, 4);

  /**< Indicate whether fw is enabled or not */
  UEFI_DECLARE_ALIGNED (UINT32 Reserved1, 4);

  /**< Reserved field */
  UEFI_DECLARE_ALIGNED (UINT32 Reserved2, 4);

  /**< Indicate whether DRAM ECC page blacklisting feature is enabled or not */
  UEFI_DECLARE_ALIGNED (
    union {
    UINT64 FeatureFlagRaw;
    struct {
      UINT64 EnableDramPageRetirement: 1;
      UINT64 EnableCombinedUart: 1;
      UINT64 EnableDramStagedScrubbing: 1;
      UINT64 EnableBlanketNsdramCarveout: 1;
      UINT64 EnableNsdramEncryption: 1;
    } FeatureFlagData;
  },
    8
    );

  /**< SDRAM base address*/
  UEFI_DECLARE_ALIGNED (UINT64 SdramBase, 8);

  /**< SDRAM size in bytes */
  UEFI_DECLARE_ALIGNED (UINT64 SdramSize, 8);

  /**< mb1 bct version information */
  UEFI_DECLARE_ALIGNED (TEGRABL_VERSION Mb1Bct, 4);

  /**< mb1 version */
  UEFI_DECLARE_ALIGNED (TEGRABL_VERSION Mb1, 4);

  /**< mb2 version */
  UEFI_DECLARE_ALIGNED (TEGRABL_VERSION Mb2, 4);

  /**< EEPROM data FIXME:CHECK eEPROM*/
  UEFI_DECLARE_ALIGNED (TEGRABL_EEPROM_DATA Eeprom, 8);

  /**< Boot chain selection, specifies if GPIO is used to select the chain to boot.
     Value 0 indicates BCT boot mode, value 1 indicates GPIO boot mode */
  UEFI_DECLARE_ALIGNED (UINT32 BootChainSelectionMode, 4);

  UEFI_DECLARE_ALIGNED (UINT32 U32NonGpioSelectBootChain, 4);

  /**< BRBCT unsigned and signed customer data */
  UEFI_DECLARE_ALIGNED (
    union {
    UINT8 BrbctCustomerData[2048];
    struct {
      UINT8 BrbctUnsignedCustomerData[BRBCT_UNSIGNED_CUSTOMER_DATA_SIZE];
      UINT8 BrbctSignedCustomerData[BRBCT_SIGNED_CUSTOMER_DATA_SIZE];
    };
  },
    8
    );

  /**< Start address of DRAM ECC page retirement information structure */
  UEFI_DECLARE_ALIGNED (UINT64 DramPageRetirementInfoAddress, 8);

  /** Start address of hvinfo page */
  UEFI_DECLARE_ALIGNED (UINT64 Reserved3, 8);

  /** Start address of PVIT page */
  UEFI_DECLARE_ALIGNED (UINT64 Reserved4, 8);

  UEFI_DECLARE_ALIGNED (UINT32 Reserved5, 4);
  UEFI_DECLARE_ALIGNED (UINT32 Reserved6, 4);

  /**< Min Ratchet Level */
  UEFI_DECLARE_ALIGNED (UINT8 MinRatchetLevel[MAX_CPUBL_OEM_FW_RATCHET_INDEX_V1], 8);

  /**< physical address and size of the carveouts */
  UEFI_DECLARE_ALIGNED (TEGRABL_CARVEOUT_INFO CarveoutInfo[CARVEOUT_OEM_COUNT], 8);
} TEGRA_CPUBL_PARAMS_V1;

typedef struct {
  /**< sha512 digest */
  UEFI_DECLARE_ALIGNED (UINT8 Digest[SHA512_DIGEST_SIZE], 8);

  /**< version */
  UEFI_DECLARE_ALIGNED (UINT32 Version, 4);

  /**< Uart instance */
  UEFI_DECLARE_ALIGNED (UINT32 Uart_Instance, 4);

  /**< If tos loaded by mb2 has secureos or not. */
  UEFI_DECLARE_ALIGNED (UINT32 SecureosType, 4);

  /**< Boot mode can be cold boot, or RCM */
  UEFI_DECLARE_ALIGNED (UINT32 BootType, 4);

  /**< Indicate whether fw is enabled or not */
  UEFI_DECLARE_ALIGNED (UINT32 Reserved1, 4);

  /**< Reserved field */
  UEFI_DECLARE_ALIGNED (UINT32 Reserved2, 4);

  /**< Indicate whether DRAM ECC page blacklisting feature is enabled or not */
  UEFI_DECLARE_ALIGNED (
    union {
    UINT64 FeatureFlagRaw;
    struct {
      UINT64 EnableDramPageRetirement: 1;
      UINT64 EnableCombinedUart: 1;
      UINT64 EnableDramStagedScrubbing: 1;
      UINT64 EnableBlanketNsdramCarveout: 1;
      UINT64 EnableNsdramEncryption: 1;
    } FeatureFlagData;
  },
    8
    );

  /**< SDRAM base address*/
  UEFI_DECLARE_ALIGNED (UINT64 SdramBase, 8);

  /**< SDRAM size in bytes */
  UEFI_DECLARE_ALIGNED (UINT64 SdramSize, 8);

  /**< mb1 bct version information */
  UEFI_DECLARE_ALIGNED (TEGRABL_VERSION Mb1Bct, 4);

  /**< mb1 version */
  UEFI_DECLARE_ALIGNED (TEGRABL_VERSION Mb1, 4);

  /**< mb2 version */
  UEFI_DECLARE_ALIGNED (TEGRABL_VERSION Mb2, 4);

  /**< EEPROM data FIXME:CHECK eEPROM*/
  UEFI_DECLARE_ALIGNED (TEGRABL_EEPROM_DATA Eeprom, 8);

  /**< Boot chain selection, specifies if GPIO is used to select the chain to boot.
     Value 0 indicates BCT boot mode, value 1 indicates GPIO boot mode */
  UEFI_DECLARE_ALIGNED (UINT32 BootChainSelectionMode, 4);

  UEFI_DECLARE_ALIGNED (UINT32 U32NonGpioSelectBootChain, 4);

  /**< BRBCT unsigned and signed customer data */
  UEFI_DECLARE_ALIGNED (
    union {
    UINT8 BrbctCustomerData[2048];
    struct {
      UINT8 BrbctUnsignedCustomerData[BRBCT_UNSIGNED_CUSTOMER_DATA_SIZE];
      UINT8 BrbctSignedCustomerData[BRBCT_SIGNED_CUSTOMER_DATA_SIZE];
    };
  },
    8
    );

  /**< Start address of DRAM ECC page retirement information structure */
  UEFI_DECLARE_ALIGNED (UINT64 DramPageRetirementInfoAddress, 8);

  /** Start address of hvinfo page */
  UEFI_DECLARE_ALIGNED (UINT64 Reserved3, 8);

  /** Start address of PVIT page */
  UEFI_DECLARE_ALIGNED (UINT64 Reserved4, 8);

  UEFI_DECLARE_ALIGNED (UINT32 Reserved5, 4);
  UEFI_DECLARE_ALIGNED (UINT32 Reserved6, 4);

  /**< Min Ratchet Level */
  UEFI_DECLARE_ALIGNED (UINT8 MinRatchetLevel[MAX_CPUBL_OEM_FW_RATCHET_INDEX_V2], 8);

  /**< physical address and size of the carveouts */
  UEFI_DECLARE_ALIGNED (TEGRABL_CARVEOUT_INFO CarveoutInfo[CARVEOUT_OEM_COUNT], 8);
} TEGRA_CPUBL_PARAMS_V2;

typedef struct {
  union {
    TEGRA_CPUBL_PARAMS_V0    v0;
    TEGRA_CPUBL_PARAMS_V1    v1;
    TEGRA_CPUBL_PARAMS_V2    v2;
    struct {
      UEFI_DECLARE_ALIGNED (UINT8 Digest[SHA512_DIGEST_SIZE], 8);
      UEFI_DECLARE_ALIGNED (UINT32 Version, 4);
    } common;
  };
} TEGRA_CPUBL_PARAMS;

#define CPUBL_VERSION(PARAMS)                (((TEGRA_CPUBL_PARAMS *)PARAMS)->common.Version)
#define CPUBL_PARAMS(PARAMS, FIELD)          (((CPUBL_VERSION(PARAMS)) == 0)? \
                                              ((TEGRA_CPUBL_PARAMS *)PARAMS)->v0.FIELD:\
                                              ((CPUBL_VERSION(PARAMS)) == 1)? \
                                              ((TEGRA_CPUBL_PARAMS *)PARAMS)->v1.FIELD:\
                                              ((CPUBL_VERSION(PARAMS)) == 2)? \
                                              ((TEGRA_CPUBL_PARAMS *)PARAMS)->v2.FIELD:\
                                               0)
#define ADDR_OF_CPUBL_PARAMS(PARAMS, FIELD)  (((CPUBL_VERSION(PARAMS)) == 0)? \
                                               &((TEGRA_CPUBL_PARAMS *)PARAMS)->v0.FIELD:\
                                              ((CPUBL_VERSION(PARAMS)) == 1)? \
                                               &((TEGRA_CPUBL_PARAMS *)PARAMS)->v1.FIELD:\
                                              ((CPUBL_VERSION(PARAMS)) == 2)? \
                                               &((TEGRA_CPUBL_PARAMS *)PARAMS)->v2.FIELD:\
                                               NULL)

#endif //__T234_RESOURCE_CONFIG_PRIVATE_H__
