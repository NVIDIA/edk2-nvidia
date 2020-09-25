/** @file
*
*  Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __T186_RESOURCE_CONFIG_PRIVATE_H__
#define __T186_RESOURCE_CONFIG_PRIVATE_H__

#include <Uefi/UefiBaseType.h>
#include <Library/DramCarveoutLib.h>

#define TEGRABL_GLOBAL_DATA_VERSION 4

#define TEGRABL_MAX_VERSION_STRING 128 /* chars including null */

#define NUM_DRAM_BAD_PAGES 1024
#define TEGRA_BL_MAX_STORAGE_DEVICES  5U

typedef enum {
  CARVEOUT_NONE = 0,            /* 0x0 */
  CARVEOUT_NVDEC = 1,           /* 0x1 */
  CARVEOUT_WPR1 = 2,            /* 0x2 */
  CARVEOUT_WPR2 = 3,            /* 0x3 */
  CARVEOUT_TSECA = 4,           /* 0x4 */
  CARVEOUT_TSECB = 5,           /* 0x5 */
  CARVEOUT_BPMP = 6,            /* 0x6 */
  CARVEOUT_APE = 7,           /* 0x7 */
  CARVEOUT_SPE = 8,           /* 0x8 */
  CARVEOUT_SCE = 9,           /* 0x9 */
  CARVEOUT_APR = 10,            /* 0xA */
  CARVEOUT_TZRAM = 11,          /* 0xB */
  CARVEOUT_SE = 12,           /* 0xC */
  CARVEOUT_DMCE = 13,           /* 0xD */
  CARVEOUT_BPMP_TO_DMCE = 14,       /* 0xE */
  CARVEOUT_DMCE_TO_BPMP = 15,       /* 0xF */
  CARVEOUT_BPMP_TO_SPE = 16,        /* 0x10 */
  CARVEOUT_SPE_TO_BPMP = 17,        /* 0x11 */
  CARVEOUT_CPUTZ_TO_BPMP = 18,      /* 0x12 */
  CARVEOUT_BPMP_TO_CPUTZ = 19,      /* 0x13 */
  CARVEOUT_CPUNS_TO_BPMP = 20,      /* 0x14 */
  CARVEOUT_BPMP_TO_CPUNS = 21,      /* 0x15 */
  CARVEOUT_SE_SPE_SCE_BPMP = 22,      /* 0x16 */
  CARVEOUT_SC7_RESUME_FW = 23,      /* 0x17 */
  CARVEOUT_OEM_RSVD1 = 24,        /* 0x18 */
  CARVEOUT_OEM_RSVD2 = 25,        /* 0x19 */
  CARVEOUT_OEM_RSVD3 = 26,        /* 0x1A */
  CARVEOUT_NV_RSVD1 = 27,         /* 0x1B */
  CARVEOUT_BO_MTS_PACKAGE = 28,     /* 0x1C */
  CARVEOUT_BO_MCE_PREBOOT = 29,     /* 0x1D */
  CARVEOUT_MAX_GSC_CO = 29,       /* 0x1D */
  CARVEOUT_MTS = 30,            /* 0x1E */
  CARVEOUT_VPR = 31,            /* 0x1F */
  CARVEOUT_TZDRAM = 32,         /* 0x20 */
  CARVEOUT_PRIMARY = 33,          /* 0x21 */
  CARVEOUT_EXTENDED = 34,         /* 0x22 */
  CARVEOUT_NCK = 35,            /* 0x23 */
  CARVEOUT_DEBUG = 36,          /* 0x24 */
  CARVEOUT_RAMDUMP = 37,          /* 0x25 */
  CARVEOUT_MB2 = 38,            /* 0x26 */
  CARVEOUT_CPUBL = 39,          /* 0x27 */
  CARVEOUT_MB2_HEAP = 40,         /* 0x28 */
  CARVEOUT_CPUBL_PARAMS = 41,       /* 0x29 */
  CARVEOUT_RESERVED1 = 42,        /* 0x2A */
  CARVEOUT_RESERVED2 = 43,        /* 0x2B */
  CARVEOUT_NUM = 44,            /* 0x2C */
  CARVEOUT_FORCE32 = 2147483647ULL    /* 0x7FFFFFFF */
} CARVEOUT_IDS;

typedef PACKED struct {
  UINT8 Type;
  UINT8 Instance;
} TEGRA_BL_DEVICE;

typedef PACKED struct {
  /**< version */
  UINT64 Version;

  /**< Cmac-hash (using 0 key) of the data */
  UINT32 Hash[4];

  /**< Size of the data to be hashed */
  UINT64 HashDataSize;

  /**< Uart_base Address for debug prints */
  UINT64 EarlyUartAddr;

  /***< Address of bootrom bct */
  UINT64 BrbctCarveout;

  /***< Address of carveout containing profiling data */
  UINT64 ProfilingCarveout;

  /**< Location blob required for rcm boot */
  UINT64 RecoveryBlobCarveout;

  /**< Carveout Info */
  NVDA_MEMORY_REGION Carveout[CARVEOUT_NUM];

  /**< DRAM bad page info */
  UINT64 ValidDramBadPageCount;
  UINT64 DramBadPages[NUM_DRAM_BAD_PAGES];

  /**< Boot mode can be cold boot, or RCM */
  UINT32 BootType;

  /**< Boot type set by nv3pserver based on boot command from host. */
  UINT32 RecoveryBootType;

  /**< Reset reason as read from PMIC */
  UINT32 PmicResetReason;

  /**< mb1 bct version information */
  UINT32 Mb1BctVersion;

  /**< Address where mb1 version is present */
  UINT64 Mb1VersionPtr;

  /**< Address where mb2 version is present */
  UINT64 Mb2VersionPtr;

  /**< Safe data pointer, safe location to add any extra information. */
  UINT64 SafeDataPtr;

  /**< Parameter to unhalt SCE */
  UINT8 EnableSceSafety;

  /**< Parameter to enable full dram scrub at mb1. */
  UINT8 DisableStagedScrub;

  /**< Parameter to enable switching of bootchain for non gpio boot chain case */
  UINT8 SwitchBootchain;

  UINT8 Reserved[229];
} TEGRA_GLOBAL_DATA;

typedef struct {
  /**< Global Data shared across boot-binaries */
  TEGRA_GLOBAL_DATA GlobalData;

  UINT32 Version;

  UINT32 UartInstance;

  UINT32 EnableLog;

  /**< Address of device params from mb1 bct */
  UINT64 DevParamsAddress;

  /**< Address of i2c bus frequecy from mb1 bct */
  UINT64 I2cBusFrequencyAddress;

  /**< Address of controller pad settings */
  UINT64 ControllerProdSettings;

  /**< Total size of controller pad settings */
  UINT64 ControllerProdSettingsSize;

  /**< Parameters for Secure_OS/TLK passed via GPR */
  UINT64 SecureOsParams[4];
  UINT64 SecureOsStart;

  /**< If tos loaded by mb2 has secureos or not.
   * Added in version 3.
   */
  UINT32 SecureOsType;
  UINT64 GoldenRegStart;

  /**< dtb load address */
  UINT64 DtbLoadAddress;

  /**< rollback data address */
  UINT64 RollbackDataAddress;

  TEGRA_BL_DEVICE StorageDevices[TEGRA_BL_MAX_STORAGE_DEVICES];

  UINT8 Reserved[214];
} TEGRA_CPUBL_PARAMS;


#endif //__T186_RESOURCE_CONFIG_PRIVATE_H__
