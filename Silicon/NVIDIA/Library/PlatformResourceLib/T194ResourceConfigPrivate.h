/** @file
*
*  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T194_RESOURCE_CONFIG_PRIVATE_H__
#define __T194_RESOURCE_CONFIG_PRIVATE_H__

#include <Uefi/UefiBaseType.h>
#include <Library/PlatformResourceLib.h>

#define TEGRABL_MAX_VERSION_STRING 128 /* chars including null */
#define NUM_DRAM_BAD_PAGES 1024
#define TEGRABL_MAX_STORAGE_DEVICES 8
#define MAX_OEM_FW_RATCHET_INDEX 104

#define T194_FUSE_BASE_ADDRESS 0x03820000

/*macro carve_out_type*/
#define CARVEOUT_NONE 0U
#define CARVEOUT_GSC1 1U
#define CARVEOUT_NVDEC CARVEOUT_GSC1
#define CARVEOUT_GSC2 2U
#define CARVEOUT_WPR1 CARVEOUT_GSC2
#define CARVEOUT_GSC3 3U
#define CARVEOUT_WPR2 CARVEOUT_GSC3
#define CARVEOUT_GSC4 4U
#define CARVEOUT_TSECA CARVEOUT_GSC4
#define CARVEOUT_GSC5 5U
#define CARVEOUT_TSECB CARVEOUT_GSC5
#define CARVEOUT_GSC6 6U
#define CARVEOUT_BPMP CARVEOUT_GSC6
#define CARVEOUT_GSC7 7U
#define CARVEOUT_APE CARVEOUT_GSC7
#define CARVEOUT_GSC8 8U
#define CARVEOUT_SPE CARVEOUT_GSC8
#define CARVEOUT_GSC9 9U
#define CARVEOUT_SCE CARVEOUT_GSC9
#define CARVEOUT_GSC10 10U
#define CARVEOUT_APR CARVEOUT_GSC10
#define CARVEOUT_GSC11 11U
#define CARVEOUT_TZRAM CARVEOUT_GSC11
#define CARVEOUT_GSC12 12U
#define CARVEOUT_IPC_SE_TSEC CARVEOUT_GSC12
#define CARVEOUT_GSC13 13U
#define CARVEOUT_BPMP_RCE CARVEOUT_GSC13
#define CARVEOUT_GSC14 14U
#define CARVEOUT_BPMP_DMCE CARVEOUT_GSC14
#define CARVEOUT_GSC15 15U
#define CARVEOUT_SE_SC7 CARVEOUT_GSC15
#define CARVEOUT_GSC16 16U
#define CARVEOUT_BPMP_SPE CARVEOUT_GSC16
#define CARVEOUT_GSC17 17U
#define CARVEOUT_RCE CARVEOUT_GSC17
#define CARVEOUT_GSC18 18U
#define CARVEOUT_CPU_TZ_BPMP CARVEOUT_GSC18
#define CARVEOUT_GSC19 19U
#define CARVEOUT_VM_ENCRYPT1 CARVEOUT_GSC19
#define CARVEOUT_GSC20 20U
#define CARVEOUT_CPU_NS_BPMP CARVEOUT_GSC20
#define CARVEOUT_GSC21 21U
#define CARVEOUT_OEM_SC7 CARVEOUT_GSC21
#define CARVEOUT_GSC22 22U
#define CARVEOUT_IPC_SE_SPE_SCE_BPMP CARVEOUT_GSC22
#define CARVEOUT_GSC23 23U
#define CARVEOUT_SC7_RF CARVEOUT_GSC23
#define CARVEOUT_GSC24 24U
#define CARVEOUT_CAMERA_TASK CARVEOUT_GSC24
#define CARVEOUT_GSC25 25U
#define CARVEOUT_SCE_BPMP CARVEOUT_GSC25
#define CARVEOUT_GSC26 26U
#define CARVEOUT_CV CARVEOUT_GSC26
#define CARVEOUT_GSC27 27U
#define CARVEOUT_VM_ENCRYPT2 CARVEOUT_GSC27
#define CARVEOUT_GSC28 28U
#define CARVEOUT_HYPERVISOR CARVEOUT_GSC28
#define CARVEOUT_GSC29 29U
#define CARVEOUT_SMMU CARVEOUT_GSC29
#define CARVEOUT_GSC30 30U
#define CARVEOUT_GSC31 31U
#define CARVEOUT_MTS 32U
#define CARVEOUT_VPR 33U
#define CARVEOUT_TZDRAM 34U
#define CARVEOUT_MB2 35U
#define CARVEOUT_CPUBL 36U
#define CARVEOUT_MISC 37U
#define CARVEOUT_OS 38U
#define CARVEOUT_RCM_BLOB 39U
#define CARVEOUT_ECC_TEST 40U
#define CARVEOUT_RESERVED1 41U
#define CARVEOUT_RESERVED2 42U
#define CARVEOUT_RESERVED3 43U
#define CARVEOUT_RESERVED4 44U
#define CARVEOUT_RESERVED5 45U
#define CARVEOUT_NUM 46U

typedef struct {
  UINT64 Base;
  UINT64 Size;
  union {
    struct {
      UINT64 EccProtected:1;
      UINT64 Reserved:63;
    };
    UINT64 Flags;
  };
} TEGRABL_CARVEOUT_INFO;

#pragma pack(1)
typedef struct {
  UINT8 Type;
  UINT8 Instance;
} TEGRABL_DEVICE;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT32  MagicHeader;
  UINT32  ClockSource;
  UINT32  ClockDivider;
  UINT32  ClockSourceFrequency;
  UINT32  InterfaceFrequency;
  UINT32  MaxBusWidth;
  BOOLEAN EnableDdrRead;
  UINT32  DmaType;
  UINT32  FifoAccessMode;
  UINT32  ReadDummyCycles;
  UINT32  Trimmer1Value;
  UINT32  Rrimmer2Value;
  UINT8   Reserved[8];
} TEGRABL_DEVICE_CONFIG_QSPI_FLASH_PARAMS;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT32  MagicHeader;
  UINT32  ClockSource;
  UINT32  ClockFrequency;
  UINT32  BestMode;
  UINT32  PdOffset;
  UINT32  PuOffset;
  BOOLEAN DqsTrimHs400;
  BOOLEAN EnableStrobeHs400;
  UINT8   Reserved[8];
} TEGRABL_DEVICE_CONFIG_SDMMC_PARAMS;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT32 MagicHeader;
  UINT8  TransferSpeed;
  UINT8  Reserved[8];
} TEGRABL_DEVICE_CONFIG_SATA_PARAMS;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT32  MagicHeader;
  UINT8   MaxHsMode;
  UINT8   MaxPwmMode;
  UINT8   MaxActiveLanes;
  UINT32  PageAlignSize;
  BOOLEAN EnableHsModes;
  BOOLEAN EnableFastAutoMode;
  BOOLEAN EnableHsRateB;
  BOOLEAN EnableHsRateA;
  BOOLEAN SkipHsModeSwitch;
  UINT8   Reserved[8];
} TEGRABL_DEVICE_CONFIG_UFS_PARAMS;
#pragma pack()

#pragma pack(1)
typedef struct  {
  UINT32 Version;
  TEGRABL_DEVICE_CONFIG_SDMMC_PARAMS Sdmmc;
  TEGRABL_DEVICE_CONFIG_QSPI_FLASH_PARAMS QspiFlash;
  TEGRABL_DEVICE_CONFIG_UFS_PARAMS Ufs;
  TEGRABL_DEVICE_CONFIG_SATA_PARAMS Sata;
} TEGRABL_DEVICE_CONFIG_PARAMS;
#pragma pack()

typedef struct {
  /**< version */
  UEFI_DECLARE_ALIGNED(UINT32 Version, 4);

  /**< Uart instance */
  UEFI_DECLARE_ALIGNED(UINT32 Uart_Instance, 4);

  /**< Enable logs */
  UEFI_DECLARE_ALIGNED(UINT32 EnableLog, 4);

  /**< device config params from mb1 bct */
  UEFI_DECLARE_ALIGNED(TEGRABL_DEVICE_CONFIG_PARAMS DeviceConfig, 8);

  /**< Address of i2c bus frequecy from mb1 bct */
  UEFI_DECLARE_ALIGNED(UINT64 I2cBusFrequencyAddress, 8);

  /**< Address of controller pad settings */
  UEFI_DECLARE_ALIGNED(UINT64 ControllerProdSettings, 8);

  /**< Total size of controller pad settings */
  UEFI_DECLARE_ALIGNED(UINT64 ControllerProdSettingsSize, 8);

  /**< Parameters for Secure_OS/TLK passed via GPR */
  UEFI_DECLARE_ALIGNED(UINT64 SecureOsParams[4], 8);
  UEFI_DECLARE_ALIGNED(UINT64 SecureOsStart, 8);

  /**< If tos loaded by mb2 has secureos or not. */
  UEFI_DECLARE_ALIGNED(UINT32 SecureosType, 4);

  /**< SDRAM size in bytes */
  UEFI_DECLARE_ALIGNED(UINT64 SdramSize, 8);

  /**< bootloader dtb load address */
  UEFI_DECLARE_ALIGNED(UINT64 BlDtbLoadAddress, 8);

  /**< physical address and size of the carveouts */
  UEFI_DECLARE_ALIGNED(TEGRABL_CARVEOUT_INFO CarveoutInfo[CARVEOUT_NUM], 8);

  /**< Indicate whether DRAM ECC page blacklisting feature is enabled
     or not
   */
  UEFI_DECLARE_ALIGNED(union {
    UINT64 FeatureFlagRaw;
    struct {
      UINT32 EnableDramPageBlacklisting:1;
      UINT32 EnableCombinedUart:1;
      UINT32 EnableDramStagedScrubbing:1;
      UINT32 EnableSce:1;
      UINT32 SwitchBootchain:1;
      UINT32 ResetToRecovery:1;
      UINT32 EnableRce:1;
      UINT32 EnableApe:1;
      UINT32 Reserved1:24;
      UINT32 Reserved2;
    };
  }, 8);

  /**< Start address of SDRAM params used in MB1 as per RAMCODE */
  UEFI_DECLARE_ALIGNED(UINT64 SdramParamsOffset, 8);

  /**< Start address of DRAM ECC page blacklisting information
     structure
   */
  UEFI_DECLARE_ALIGNED(UINT64 DramPageBlacklistInfoAddress, 8);

  /**< Start address of Golden register data region */
  UEFI_DECLARE_ALIGNED(UINT64 GoldenRegisterAddress, 8);

  /**< Size of Golden register data region */
  UEFI_DECLARE_ALIGNED(UINT32 GoldenRegisterSize, 8);

  /**< Start address of Profiling data */
  UEFI_DECLARE_ALIGNED(UINT64 ProfilingDataAddress, 8);

  /**< Size of Profiling data */
  UEFI_DECLARE_ALIGNED(UINT32 ProfilingDataSize, 8);

  /**< Start offset of unallocated/unused data in CPUâ€BL carveout */
  UEFI_DECLARE_ALIGNED(UINT64 CpublCarveoutSafeEndOffset, 8);

  /**< Start offset of unallocated/unused data in MISC carveout */
  UEFI_DECLARE_ALIGNED(UINT64 MiscCarveoutSafeStartOffset, 8);

  /**< Boot type set by nv3pserver based on boot command from host. */
  UEFI_DECLARE_ALIGNED(UINT32 RecoveryBootType, 8);

  /**< Boot mode can be cold boot, or RCM */
  UEFI_DECLARE_ALIGNED(UINT32 BootType, 8);

  /**< Uart_base Address for debug prints */
  UEFI_DECLARE_ALIGNED(UINT64 EarlyUartAddr, 8);

  /**< mb1 bct version information */
  UEFI_DECLARE_ALIGNED(UINT32 Mb1BctVersion, 8);

  /**< mb1 version */
  UEFI_DECLARE_ALIGNED(UINT8 Mb1Version[TEGRABL_MAX_VERSION_STRING], 8);

  /**< mb2 version */
  UEFI_DECLARE_ALIGNED(UINT8 Mb2Version[TEGRABL_MAX_VERSION_STRING], 8);

  UEFI_DECLARE_ALIGNED(UINT8 CpublVersion[TEGRABL_MAX_VERSION_STRING], 8);

  /**< Reset reason as read from PMIC */
  UEFI_DECLARE_ALIGNED(UINT32 PmicRstReason, 8);

  /**< Pointer to BRBCT location in sdram */
  UEFI_DECLARE_ALIGNED(UINT64 BrbctCarveout, 8);

  /**< Storage devices to be used */
  UEFI_DECLARE_ALIGNED(TEGRABL_DEVICE StorageDevices[TEGRABL_MAX_STORAGE_DEVICES], 8);

  /** Minimum ratchet version of OEM-FW bins */
  UEFI_DECLARE_ALIGNED(UINT8 MinRatchet[MAX_OEM_FW_RATCHET_INDEX], 8);

  /** Enable encryption of OS managed memory */
  UEFI_DECLARE_ALIGNED(UINT32 EnableOsMemEncryption, 8);

  /** Bit-vector representing which of the GSCs get used for encrypting OS managed memory */
  UEFI_DECLARE_ALIGNED(UINT32 OsMemEncryptionGscList, 8);

  /** Blob size in rcm mode */
  UEFI_DECLARE_ALIGNED(UINT32 RcmBlobSize, 8);

  /** EEPROM data*/
  UEFI_DECLARE_ALIGNED(TEGRABL_EEPROM_DATA Eeprom, 8);
} TEGRA_CPUBL_PARAMS;

#endif //__T194_RESOURCE_CONFIG_PRIVATE_H__
