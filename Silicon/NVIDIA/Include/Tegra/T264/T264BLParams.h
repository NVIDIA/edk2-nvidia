/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T264_BL_PARAMS_H__
#define __T264_BL_PARAMS_H__

#include <Uefi/UefiBaseType.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>

#define TEGRABL_SHA512_DIGEST_BYTES             64U
#define TEGRABL_MAX_CPUBL_OEM_FW_RATCHET_INDEX  304U
#define TEGRABL_NUM_DRAM_BAD_PAGES              1024

/*macro carve_out_type*/
#define CARVEOUT_NONE                     0
#define CARVEOUT_IGPU_BOOT                1
#define CARVEOUT_WPR1                     2
#define CARVEOUT_GPU_COMPTAGS             3
#define CARVEOUT_TSEC                     4
#define CARVEOUT_XUSB                     5
#define CARVEOUT_BPMP                     6
#define CARVEOUT_APE                      7
#define CARVEOUT_AON                      8
#define CARVEOUT_SB_CPUTZ                 9
#define CARVEOUT_APE1                     10
#define CARVEOUT_BPMP_DCE                 11
#define CARVEOUT_DISP_EARLY_BOOT_FB       12
#define CARVEOUT_BPMP_RCE                 13
#define CARVEOUT_HPSE_CCPLEX              14
#define CARVEOUT_HPSE_SB                  15
#define CARVEOUT_VI_TASKLIST              16
#define CARVEOUT_RCE                      17
#define CARVEOUT_BPMP_CPUTZ               18
#define CARVEOUT_PVA                      19
#define CARVEOUT_DCE                      20
#define CARVEOUT_ETR                      21
#define CARVEOUT_PSC                      22
#define CARVEOUT_NV_SC7                   23
#define CARVEOUT_RCE_RW                   24
#define CARVEOUT_VI1_TASKLIST             25
#define CARVEOUT_ISP_TASKLIST             26
#define CARVEOUT_ISP1_TASKLIST            27
#define CARVEOUT_CCPLEX_INTERWORLD_SHMEM  28
#define CARVEOUT_FSI                      29
#define CARVEOUT_HPSE_DCE                 30
#define CARVEOUT_UNUSED1                  31
#define CARVEOUT_HPSE_PSC                 32
#define CARVEOUT_HPSE_RCE                 33
#define CARVEOUT_ATF_FSI                  34
#define CARVEOUT_OEM_SC7                  35
#define CARVEOUT_HPSE                     36
#define CARVEOUT_SB                       37
#define CARVEOUT_VM_ENCRYPT               38
#define CARVEOUT_CCPLEX_SMMU_PTW          39
#define CARVEOUT_BPMP_CPU_NS              40
#define CARVEOUT_FSI_CPU_NS               41
#define CARVEOUT_TSEC_DCE                 42
#define CARVEOUT_TSEC_CCPLEX              43
#define CARVEOUT_TZDRAM                   44
#define CARVEOUT_VPR                      45
#define CARVEOUT_MTS                      46
#define CARVEOUT_UEFI                     47
#define CARVEOUT_DISP_SCANOUT_FB          48
#define CARVEOUT_RCM_BLOB                 49
#define CARVEOUT_PROFILING                50
#define CARVEOUT_OS                       51
#define CARVEOUT_FSI_KEY_BLOB             52
#define CARVEOUT_TEMP_MB2RF               53
#define CARVEOUT_TEMP_MB2_LOAD            54
#define CARVEOUT_TEMP_MB2_PARAMS          55
#define CARVEOUT_TEMP_MB2_IO_BUFFERS      56
#define CARVEOUT_TEMP_MB2RF_SRAM_CPU      57
#define CARVEOUT_TEMP_MB2_SRAM_CPU        58
#define CARVEOUT_BPMP_GPMU                59
#define CARVEOUT_DRAM_ECC_TEST            60
#define CARVEOUT_TEMP_MB2_APLT            61
#define CARVEOUT_TEMP_MB2_APLT_PARAMS     62
#define CARVEOUT_TEMP_MB2_SRAM_CPU_IO     63
#define CARVEOUT_GR                       64
#define CARVEOUT_TEMP_MEMDTB_LOAD         65
#define CARVEOUT_TEMP_BRBCT               66
#define CARVEOUT_TEMP_MB2_PGTABLES        67
#define CARVEOUT_OPTEE_DTB                68
#define CARVEOUT_BPMP_IST                 69
#define CARVEOUT_CCPLEX_IST               70
#define CARVEOUT_RAM_OOPS                 71
#define CARVEOUT_TEMP_TSECFW_LOAD         72
#define CARVEOUT_CCPLEX_LA_BUFFERS        73
#define CARVEOUT_OEM_COUNT                74

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

#define TEGRABL_MAX_CONTROLLER_PROD_WORDS  (64U)

#pragma pack(1)
typedef struct {
  UINT32    NumWords;
  UINT32    Reserved1;
  UINT32    Data[TEGRABL_MAX_CONTROLLER_PROD_WORDS];
} T264_CONTROLLER_PROD_DATA;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT32    IsMultiSkuEnabled;
  UINT32    SkuValue;
} T264_MULTI_SKU_DATA;
#pragma pack()

#pragma pack(1)
typedef struct {
  UINT64    Base;
  UINT64    Size;
} T264_SDRAM_INFO_DATA;
#pragma pack()

#define BRBCT_UNSIGNED_CUSTOMER_DATA_SIZE  1024U
#define BRBCT_SIGNED_CUSTOMER_DATA_SIZE    1024U
#define BRBCT_CUSTOMER_DATA_SIZE           (BRBCT_UNSIGNED_CUSTOMER_DATA_SIZE + BRBCT_SIGNED_CUSTOMER_DATA_SIZE)

#pragma pack(1)
typedef union {
  UINT8    BrbctCustomerData[BRBCT_CUSTOMER_DATA_SIZE];
  struct {
    UINT8    BrbctUnsignedCustomerData[BRBCT_UNSIGNED_CUSTOMER_DATA_SIZE];
    UINT8    BrbctSignedCustomerData[BRBCT_SIGNED_CUSTOMER_DATA_SIZE];
  };
} T264_BRBCT_CUSTOMER_DATA;
#pragma pack()

#pragma pack(1)
typedef struct {
  union {
    UINT64    FeatureFlagRaw1;
    struct {
      UINT64    EnableDramPageRetirement : 1;

      /**
        * Boot chain selection mode
        *  0: BCT Marker Mode
        *  1: GPIO Mode
        */
      UINT64    BootChainSelectionMode   : 1;
      UINT64    FeatureFlagRaw1Reserved  : 62;
    };
  };

  union {
    UINT64    FeatureFlagRaw2;
    struct {
      UINT32    enable_ape  : 1;
      UINT32    enable_dce  : 1;
      UINT32    enable_fsi  : 1;
      UINT32    enable_rce  : 1;
      UINT32    enable_aon  : 1;
      UINT32    enable_pvit : 1;
    };
  };
} T264_FEATURE_FLAG_DATA;
#pragma pack()

#define MAX_RATCHET_UPDATE_FWS  20U

/* VECU-ID Size */
#define TEGRABL_MB2BCT_VECU_ID_SIZE  16U

#pragma pack(1)
typedef struct {
  union {
    /** PKC0 and 1 revoke fuse burn error bitmap */
    UINT64    pkc_revoke_err_bitmap;
    struct {
      /** PKC0 revoke fuse burn error codes. */
      /** Valid range is 0 to 4. */
      UINT64    pkc0_revoke_err     : 4;
      /** PKC1 revoke fuse burn error codes. */
      /** Valid range is 0 to 4 .*/
      UINT64    pkc1_revoke_err     : 4;
      /** PKC2 revoke fuse burn error codes. */
      /** Valid range is 0 to 4. */
      UINT64    pkc2_revoke_err     : 4;
      /** PKC3 revoke fuse burn error codes. */
      /** Valid range is 0 to 4 .*/
      UINT64    pkc3_revoke_err     : 4;
      /** PKC4 revoke fuse burn error codes. */
      /** Valid range is 0 to 4. */
      UINT64    pkc4_revoke_err     : 4;
      /** PKC5 revoke fuse burn error codes. */
      /** Valid range is 0 to 4 .*/
      UINT64    pkc5_revoke_err     : 4;
      /** PKC6 revoke fuse burn error codes. */
      /** Valid range is 0 to 4. */
      UINT64    pkc6_revoke_err     : 4;
      /** PKC7 revoke fuse burn error codes. */
      /** Valid range is 0 to 4 .*/
      UINT64    pkc7_revoke_err     : 4;
      /** PKC8 revoke fuse burn error codes. */
      /** Valid range is 0 to 4. */
      UINT64    pkc8_revoke_err     : 4;
      /** PKC9 revoke fuse burn error codes. */
      /** Valid range is 0 to 4 .*/
      UINT64    pkc9_revoke_err     : 4;
      /** PKC10 revoke fuse burn error codes. */
      /** Valid range is 0 to 4. */
      UINT64    pkc10_revoke_err    : 4;
      /** PKC11 revoke fuse burn error codes. */
      /** Valid range is 0 to 4 .*/
      UINT64    pkc11_revoke_err    : 4;
      /** PKC12 revoke fuse burn error codes. */
      /** Valid range is 0 to 4. */
      UINT64    pkc12_revoke_err    : 4;
      /** PKC13 revoke fuse burn error codes. */
      /** Valid range is 0 to 4 .*/
      UINT64    pkc13_revoke_err    : 4;
      /** PKC14 revoke fuse burn error codes. */
      /** Valid range is 0 to 4. */
      UINT64    pkc14_revoke_err    : 4;
      /** Reseved field. */
      UINT64    pkc_revoke_reserved : 4;
    };
  };
} T264_PKC_REVOKE_STATUS;

#pragma pack(1)
typedef struct {
  /** Binary type */
  UINT8    bin_type;
  /** Ratchet update status */
  UINT8    status;
} T264_RATCHET_UPDATE_STATUS;
#pragma pack()

typedef struct {
  /**< sha512 digest */
  UEFI_DECLARE_ALIGNED (UINT8 Digest[TEGRABL_SHA512_DIGEST_BYTES], 8);

  /**< version */
  UEFI_DECLARE_ALIGNED (UINT32 Version, 4);

  /**< If tos loaded by mb2 has secureos or not. */
  UEFI_DECLARE_ALIGNED (UINT32 SecureOsType, 4);

  /**< Boot mode can be cold boot, uart, recovery or RCM */
  UEFI_DECLARE_ALIGNED (UINT32 BootType, 4);

  /**< Uart instance */
  UEFI_DECLARE_ALIGNED (UINT32 UartInstance, 4);

  /**< EEPROM data CVB */
  UEFI_DECLARE_ALIGNED (TEGRABL_EEPROM_DATA Eeprom, 8);

  /* reserved  */
  UEFI_DECLARE_ALIGNED (UINT8 Reserved1[520], 8);

  /**< Controller prod data */
  UEFI_DECLARE_ALIGNED (T264_CONTROLLER_PROD_DATA ControllerProdSettings, 8);

  /* reserved */
  UEFI_DECLARE_ALIGNED (UINT8 Reserved2[4], 4);

  /**< Multi SKU data */
  UEFI_DECLARE_ALIGNED (T264_MULTI_SKU_DATA MultiSkuData, 8);

  /**< Base and size information of the DRAM */
  UEFI_DECLARE_ALIGNED (T264_SDRAM_INFO_DATA SdramInfo, 8);

  /* reserved */
  UEFI_DECLARE_ALIGNED (UINT8 Reserved3[16], 8);

  /**< physical address and size of the carveouts */
  UEFI_DECLARE_ALIGNED (TEGRABL_CARVEOUT_INFO CarveoutInfo[CARVEOUT_OEM_COUNT], 8);

  /* reserved */
  UEFI_DECLARE_ALIGNED (UINT8 Reserved4[1776], 8);

  /**< BRBCT unsigned and signed customer data */
  UEFI_DECLARE_ALIGNED (T264_BRBCT_CUSTOMER_DATA BrbctCustomerData, 8);

  /**< Start address of DRAM ECC page-retirement information */
  UEFI_DECLARE_ALIGNED (UINT64 DramPageRetirementAddress, 8);

  /**< Start address of hvinfo page */
  UEFI_DECLARE_ALIGNED (UINT64 HvinfoPageAddress, 8);

  /**< Start address of PVIT page */
  UEFI_DECLARE_ALIGNED (UINT64 PvitPageAddress, 8);

  /**< Base address of the RIST TID table */
  UEFI_DECLARE_ALIGNED (UINT64 RistTidInfo, 8);

  /* reserved */
  UEFI_DECLARE_ALIGNED (UINT8 Reserved5[8], 8);

  UEFI_DECLARE_ALIGNED (UINT8 MinRatchetLevel[TEGRABL_MAX_CPUBL_OEM_FW_RATCHET_INDEX], 8);

  /**< Feature flags */
  UEFI_DECLARE_ALIGNED (T264_FEATURE_FLAG_DATA FeatureFlag, 8);

  /**< Ratchet update status of FWs loaded by MB2 including RIST auth */
  UEFI_DECLARE_ALIGNED (T264_RATCHET_UPDATE_STATUS ratchet_update_status[MAX_RATCHET_UPDATE_FWS], 8);

  /* reserved */
  UEFI_DECLARE_ALIGNED (UINT8 Reserved6[40], 8);

  /** VECU-ID */
  UEFI_DECLARE_ALIGNED (UINT8 vecu_id[TEGRABL_MB2BCT_VECU_ID_SIZE], 8);

  /** PKC revoke fuse burn error bitmap*/
  UEFI_DECLARE_ALIGNED (T264_PKC_REVOKE_STATUS pkc_revoke_status, 8);

  /* reserved */
  UEFI_DECLARE_ALIGNED (UINT8 Reserved7[8], 8);
} TEGRA_CPUBL_PARAMS;

#endif //__T264_BL_PARAMS_H__
