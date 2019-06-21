/** @file
*
*  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#ifndef __SYSTEM_RESOURCE_LIB_PRIVATE_H__
#define __SYSTEM_RESOURCE_LIB_PRIVATE_H__

#include <Uefi/UefiBaseType.h>

#define TEGRABL_MAX_VERSION_STRING 128 /* chars including null */
#define NUM_DRAM_BAD_PAGES 1024

/*macro carve_out_type*/
typedef UINT32 carve_out_type_t;
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
  UINT32 Version;

  /**< Uart instance */
  UINT32 Uart_Instance;

  /**< Enable logs */
  UINT32 EnableLog;

  UINT32 Reserved0;

  /**< device config params from mb1 bct */
  TEGRABL_DEVICE_CONFIG_PARAMS DeviceConfig;

  /**< Address of i2c bus frequecy from mb1 bct */
  UINT64 I2cBusFrequencyAddress;

  /**< Address of controller pad settings */
  UINT64 ControllerProdSettings;

  /**< Total size of controller pad settings */
  UINT64 ControllerProdSettingsSize;

  /**< Parameters for Secure_OS/TLK passed via GPR */
  UINT64 SecureOsParams[4];
  UINT64 SecureOsStart;

  /**< If tos loaded by mb2 has secureos or not. */
  UINT32 SecureosType;

  /**< SDRAM size in bytes */
  UINT64 SdramSize;

  /**< bootloader dtb load address */
  UINT64 BlDtbLoadAddress;

  /**< physical address and size of the carveouts */
  TEGRABL_CARVEOUT_INFO CarveoutInfo[CARVEOUT_NUM];

  /**< Indicate whether DRAM ECC page blacklisting feature is enabled
     or not
   */
  union {
    UINT64 FeatureFlagRaw;
    struct {
      UINT64 EnableDramPageBlacklisting:1;
      UINT64 EnableCombinedUart:1;
      UINT64 EnableDramStagedScrubbing:1;
    };
  };

  /**< Start address of SDRAM params used in MB1 as per RAMCODE */
  UINT64 SdramParamsOffset;

  /**< Start address of DRAM ECC page blacklisting information
     structure
   */
  UINT64 DramPageBlacklistInfoAddress;

  /**< Start address of Golden register data region */
  UINT64 GoldenRegisterAddress;

  /**< Size of Golden register data region */
  UINT32 GoldenRegisterSize;

  /**< Start address of Profiling data */
  UINT64 ProfilingDataAddress;

  /**< Size of Profiling data */
  UINT32 ProfilingDataSize;

  /**< Start offset of unallocated/unused data in CPUâ€BL carveout */
  UINT64 CpublCarveoutSafeEndOffset;

  /**< Start offset of unallocated/unused data in MISC carveout */
  UINT64 MiscCarveoutSafeStartOffset;

  /**< Boot type set by nv3pserver based on boot command from host. */
  UINT32 RecoveryBootType;

  UINT32 Reserved1;

  /**< Boot mode can be cold boot, uart, recovery or RCM */
  UINT32 BootType;

  /**< Uart_base Address for debug prints */
  UINT64 EarlyUartAddr;

  /**< mb1 bct version information */
  UINT32 Mb1BctVersion;

  UINT32 Reserved2;

  /**< mb1 version */
  UINT8 Mb1Version[TEGRABL_MAX_VERSION_STRING];

  /**< mb2 version */
  UINT8 Mb2Version[TEGRABL_MAX_VERSION_STRING];

  UINT8 CpublVersion[TEGRABL_MAX_VERSION_STRING];

  /**< Reset reason as read from PMIC */
  UINT32 PmicRstReason;

  /**< Pointer to BRBCT location in sdram */
  UINT64 BrbctCarveout;
} TEGRA_CPUBL_PARAMS;

#endif //__SYSTEM_RESOURCE_LIB_PRIVATE_H__
