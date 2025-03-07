/** @file

  SMMUv3 Driver data structures and definitions

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMMU_V3_DXE_PRIVATE_H_
#define _SMMU_V3_DXE_PRIVATE_H_

#include <Protocol/SmmuV3Protocol.h>

#define SMMU_V3_CONTROLLER_SIGNATURE  SIGNATURE_32('S','M','U','3')

#define BIT_FIELD_SET(value, mask, shift)  (((value) & (mask)) << (shift))
#define BIT_FIELD_GET(value, mask, shift)  (((value) >> (shift)) & (mask))

#define SMMU_V3_IDR0_OFFSET          0x0          // Identification Register 0
#define SMMU_V3_IDR0_ST_LEVEL_SHIFT  27
#define SMMU_V3_IDR0_ST_LEVEL_MASK   0x3
#define SMMU_V3_IDR0_TTENDIAN_SHIFT  21
#define SMMU_V3_IDR0_TTENDIAN_MASK   0x3
#define SMMU_V3_IDR0_BTM_SHIFT       5
#define SMMU_V3_IDR0_BTM_MASK        0x1
#define SMMU_V3_IDR0_TTF_SHIFT       2
#define SMMU_V3_IDR0_TTF_MASK        0x3
#define SMMU_V3_IDR0_XLAT_STG_SHIFT  0
#define SMMU_V3_IDR0_XLAT_STG_MASK   0x3

#define SMMU_V3_IDR1_OFFSET         0x4           // Identification Register 1
#define SMMU_V3_IDR1_PRESET_SHIFT   29
#define SMMU_V3_IDR1_PRESET_MASK    0x3
#define SMMU_V3_IDR1_CMDQS_SHIFT    21
#define SMMU_V3_IDR1_CMDQS_MASK     0x1F
#define SMMU_V3_IDR1_EVTQS_SHIFT    16
#define SMMU_V3_IDR1_EVTQS_MASK     0x1F
#define SMMU_V3_IDR1_SUB_SID_SHIFT  6
#define SMMU_V3_IDR1_SUB_SID_MASK   0x1F
#define SMMU_V3_IDR1_SID_SHIFT      0
#define SMMU_V3_IDR1_SID_MASK       0x1F

#define SMMU_V3_IDR5_OFFSET     0x14              // Identification Register 5
#define SMMU_V3_IDR5_OAS_SHIFT  0
#define SMMU_V3_IDR5_OAS_MASK   0x7

#define SMMU_V3_AIDR_OFFSET          0x1C         // Architecture Identification Register
#define SMMU_V3_AIDR_ARCH_REV_SHIFT  0
#define SMMU_V3_AIDR_ARCH_REV_MASK   0xFF

#define SMMU_V3_CR0_OFFSET        0x20           // Control Register 0
#define SMMU_V3_CR0ACK_OFFSET     0x24           // Control Register 0 Acknowledgment
#define SMMU_V3_CR0_SMMUEN_SHIFT  0
#define SMMU_V3_CR0_SMMUEN_MASK   0x1
#define SMMU_V3_CR0_SMMUEN_BIT    0

#define SMMU_V3_CR1_OFFSET        0x28           // Control Register 1
#define SMMU_V3_CR1_INSH          3
#define SMMU_V3_CR1_WBCACHE       1
#define SMMU_V3_CR1_TAB_SH_SHIFT  10
#define SMMU_V3_CR1_TAB_OC_SHIFT  8
#define SMMU_V3_CR1_TAB_IC_SHIFT  6
#define SMMU_V3_CR1_QUE_SH_SHIFT  4
#define SMMU_V3_CR1_QUE_OC_SHIFT  2
#define SMMU_V3_CR1_QUE_IC_SHIFT  0
#define SMMU_V3_CR1_SH_MASK       0x3
#define SMMU_V3_CR1_OC_MASK       0x3
#define SMMU_V3_CR1_IC_MASK       0x3

#define SMMU_V3_CR2_OFFSET      0x2C             // Control Register 2
#define SMMU_V3_CR2_PTM_SHIFT   2
#define SMMU_V3_CR2_PTM_MASK    0x1
#define SMMU_V3_CR2_PTM_ENABLE  0

#define SMMU_V3_GBPA_OFFSET         0x44         // Global Bypass
#define SMMU_V3_GBPA_UPDATE_SHIFT   31
#define SMMU_V3_GBPA_UPDATE_MASK    0x1
#define SMMU_V3_GBPA_ABORT_SHIFT    20
#define SMMU_V3_GBPA_ABORT_MASK     0x1
#define SMMU_V3_GBPA_INSTCFG_SHIFT  18
#define SMMU_V3_GBPA_INSTCFG_MASK   0x3
#define SMMU_V3_GBPA_PRIVCFG_SHIFT  16
#define SMMU_V3_GBPA_PRIVCFG_MASK   0x3
#define SMMU_V3_GBPA_SHCFG_SHIFT    12
#define SMMU_V3_GBPA_SHCFG_MASK     0x3
#define SMMU_V3_GBPA_ALLOCFG_SHIFT  8
#define SMMU_V3_GBPA_ALLOCFG_MASK   0xF
#define SMMU_V3_GBPA_MTCFG_SHIFT    4
#define SMMU_V3_GBPA_MTCFG_MASK     0x1

#define SMMU_V3_LINEAR_STR_TABLE   0
#define SMMU_V3_TWO_LVL_STR_TABLE  1

#define SMMU_V3_MIX_ENDIAN  0
#define SMMU_V3_RES_ENDIAN  1
#define SMMU_V3_LIT_ENDIAN  2
#define SMMU_V3_BIG_ENDIAN  3

#define SMMU_V3_RES_TTF         0
#define SMMU_V3_AARCH32_TTF     1
#define SMMU_V3_AARCH64_TTF     2
#define SMMU_V3_AARCH32_64_TTF  3

#define SMMU_V3_OAS_32BITS  0
#define SMMU_V3_OAS_36BITS  1
#define SMMU_V3_OAS_40BITS  2
#define SMMU_V3_OAS_42BITS  3
#define SMMU_V3_OAS_44BITS  4
#define SMMU_V3_OAS_48BITS  5
#define SMMU_V3_OAS_52BITS  6
#define SMMU_V3_OAS_RES     7

#define SMMU_V3_CMDQS_MAX         19
#define SMMU_V3_EVTQS_MAX         19
#define SMMU_V3_SUB_SID_SIZE_MAX  20
#define SMMU_V3_SID_SIZE_MAX      32

typedef struct {
  BOOLEAN    LinearStrTable;
  UINT32     Endian;
  BOOLEAN    BroadcastTlb;
  UINT32     XlatFormat;
  UINT32     XlatStages;
  UINT32     CmdqEntriesLog2;
  UINT32     EvtqEntriesLog2;
  UINT32     SubStreamNBits;
  UINT32     StreamNBits;
  UINT64     Ias;
  UINT64     Oas;
  UINT32     OasEncoding;
  UINT32     MinorVersion;
} SMMU_V3_CONTROLLER_FEATURES;

typedef struct {
  UINT32                               Signature;
  EFI_PHYSICAL_ADDRESS                 BaseAddress;
  SMMU_V3_CONTROLLER_FEATURES          Features;
  VOID                                 *DeviceTreeBase;
  INT32                                NodeOffset;
  EFI_EVENT                            ExitBootServicesEvent;
  NVIDIA_SMMUV3_CONTROLLER_PROTOCOL    SmmuV3ControllerProtocol;
} SMMU_V3_CONTROLLER_PRIVATE_DATA;

#define SMMU_V3_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL(a)  CR(a, SMMU_V3_CONTROLLER_PRIVATE_DATA, SmmuV3ControllerProtocol, SMMU_V3_CONTROLLER_SIGNATURE)

#endif
