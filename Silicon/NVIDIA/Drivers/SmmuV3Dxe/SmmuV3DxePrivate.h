/** @file

  SMMUv3 Driver data structures and definitions

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMMU_V3_DXE_PRIVATE_H_
#define _SMMU_V3_DXE_PRIVATE_H_

#include <Protocol/SmmuV3Protocol.h>
#include <Protocol/IoMmu.h>
#include <Library/SmmuLib.h>

#define SMMU_V3_CONTROLLER_SIGNATURE  SIGNATURE_32('S','M','U','3')

#define BIT_FIELD_SET(value, mask, shift)  (((value) & (mask)) << (shift))
#define BIT_FIELD_GET(value, mask, shift)  (((value) >> (shift)) & (mask))
#define SMMUV3_ALL_ONES(NumberOfBits)      ((UINT64)((1ULL << (NumberOfBits)) - 1))

#define SMMU_V3_IDR0_OFFSET          (0x0)          // Identification Register 0
#define SMMU_V3_IDR0_ST_LEVEL_SHIFT  (27)
#define SMMU_V3_IDR0_ST_LEVEL_MASK   (0x3)
#define SMMU_V3_IDR0_TTENDIAN_SHIFT  (21)
#define SMMU_V3_IDR0_TTENDIAN_MASK   (0x3)
#define SMMU_V3_IDR0_BTM_SHIFT       (5)
#define SMMU_V3_IDR0_BTM_MASK        (0x1)
#define SMMU_V3_IDR0_TTF_SHIFT       (2)
#define SMMU_V3_IDR0_TTF_MASK        (0x3)
#define SMMU_V3_IDR0_XLAT_STG_SHIFT  (0)
#define SMMU_V3_IDR0_XLAT_STG_MASK   (0x3)

#define SMMU_V3_IDR1_OFFSET         (0x4)           // Identification Register 1
#define SMMU_V3_IDR1_PRESET_SHIFT   (29)
#define SMMU_V3_IDR1_PRESET_MASK    (0x3)
#define SMMU_V3_IDR1_CMDQS_SHIFT    (21)
#define SMMU_V3_IDR1_CMDQS_MASK     (0x1F)
#define SMMU_V3_IDR1_EVTQS_SHIFT    (16)
#define SMMU_V3_IDR1_EVTQS_MASK     (0x1F)
#define SMMU_V3_IDR1_SUB_SID_SHIFT  (6)
#define SMMU_V3_IDR1_SUB_SID_MASK   (0x1F)
#define SMMU_V3_IDR1_SID_SHIFT      (0)
#define SMMU_V3_IDR1_SID_MASK       (0x3F)

#define SMMU_V3_IDR5_OFFSET     (0x14)              // Identification Register 5
#define SMMU_V3_IDR5_OAS_SHIFT  (0)
#define SMMU_V3_IDR5_OAS_MASK   (0x7)

#define SMMU_V3_AIDR_OFFSET          (0x1C)         // Architecture Identification Register
#define SMMU_V3_AIDR_ARCH_REV_SHIFT  (0)
#define SMMU_V3_AIDR_ARCH_REV_MASK   (0xFF)

#define SMMU_V3_CR0_OFFSET      (0x20)             // Control Register 0
#define SMMU_V3_CR0_SMMUEN_BIT  (0)
#define SMMU_V3_EVTQEN_BIT      (2)
#define SMMU_V3_CMDQEN_BIT      (3)

#define SMMU_V3_CR0ACK_OFFSET        (0x24)        // Control Register 0 Acknowledgment
#define SMMU_V3_CR0ACK_SMMUEN_SHIFT  (0)
#define SMMU_V3_CR0ACK_SMMUEN_MASK   (0x1)
#define SMMU_V3_CR0ACK_EVTQEN_SHIFT  (2)
#define SMMU_V3_CR0ACK_EVTQEN_MASK   (0x1)
#define SMMU_V3_CR0ACK_CMDQEN_SHIFT  (3)
#define SMMU_V3_CR0ACK_CMDQEN_MASK   (0x1)

#define SMMU_V3_CR1_OFFSET        (0x28)           // Control Register 1
#define SMMU_V3_CR1_INSH          (3)
#define SMMU_V3_CR1_WBCACHE       (1)
#define SMMU_V3_CR1_TAB_SH_SHIFT  (10)
#define SMMU_V3_CR1_TAB_OC_SHIFT  (8)
#define SMMU_V3_CR1_TAB_IC_SHIFT  (6)
#define SMMU_V3_CR1_QUE_SH_SHIFT  (4)
#define SMMU_V3_CR1_QUE_OC_SHIFT  (2)
#define SMMU_V3_CR1_QUE_IC_SHIFT  (0)
#define SMMU_V3_CR1_SH_MASK       (0x3)
#define SMMU_V3_CR1_OC_MASK       (0x3)
#define SMMU_V3_CR1_IC_MASK       (0x3)

#define SMMU_V3_CR2_OFFSET             (0x2C)     // Control Register 2
#define SMMU_V3_CR2_RECINVSID_SHIFT    (1)
#define SMMU_V3_CR2_RECINVSID_MASK     (0x1)
#define SMMU_V3_CR2_RECINVSID_DISABLE  (0)
#define SMMU_V3_CR2_RECINVSID_ENABLE   (1)
#define SMMU_V3_CR2_PTM_SHIFT          (2)
#define SMMU_V3_CR2_PTM_MASK           (0x1)
#define SMMU_V3_CR2_PTM_ENABLE         (0)

#define SMMU_V3_GBPA_OFFSET         (0x44)         // Global Bypass
#define SMMU_V3_GBPA_UPDATE_SHIFT   (31)
#define SMMU_V3_GBPA_UPDATE_MASK    (0x1)
#define SMMU_V3_GBPA_ABORT_SHIFT    (20)
#define SMMU_V3_GBPA_ABORT_MASK     (0x1)
#define SMMU_V3_GBPA_INSTCFG_SHIFT  (18)
#define SMMU_V3_GBPA_INSTCFG_MASK   (0x3)
#define SMMU_V3_GBPA_PRIVCFG_SHIFT  (16)
#define SMMU_V3_GBPA_PRIVCFG_MASK   (0x3)
#define SMMU_V3_GBPA_SHCFG_SHIFT    (12)
#define SMMU_V3_GBPA_SHCFG_MASK     (0x3)
#define SMMU_V3_GBPA_ALLOCFG_SHIFT  (8)
#define SMMU_V3_GBPA_ALLOCFG_MASK   (0xF)
#define SMMU_V3_GBPA_MTCFG_SHIFT    (4)
#define SMMU_V3_GBPA_MTCFG_MASK     (0x1)

#define SMMU_V3_GERROR_OFFSET          (0x60)      // Global Error
#define SMMU_V3_GERROR_SFM_ERR_SHIFT   (8)
#define SMMU_V3_GERROR_SFM_ERR_MASK    (0x1)
#define SMMU_V3_GERROR_CMDQ_ERR_SHIFT  (0)
#define SMMU_V3_GERROR_CMDQ_ERR_MASK   (0x1)

#define SMMU_V3_GERRORN_OFFSET          (0x64)    // Global Error Acknowledgement
#define SMMU_V3_GERRORN_SFM_ERR_SHIFT   (8)
#define SMMU_V3_GERRORN_SFM_ERR_MASK    (0x1)
#define SMMU_V3_GERRORN_CMDQ_ERR_SHIFT  (0)
#define SMMU_V3_GERRORN_CMDQ_ERR_MASK   (0x1)

#define SMMU_V3_STRTAB_BASE_OFFSET      (0x80)
#define SMMU_V3_STRTAB_BASE_ADDR_SHIFT  (6)
#define SMMU_V3_STRTAB_BASE_ADDR_MASK   (0x3FFFFFFFFFFFFULL)

#define SMMU_V3_STRTAB_BASE_CFG_OFFSET  (0x88)

#define SMMU_V3_CMDQ_BASE_OFFSET      (0x90)      // Command Queue Base Address Register
#define SMMU_V3_CMDQ_BASE_ADDR_SHIFT  (5)
#define SMMU_V3_CMDQ_BASE_ADDR_MASK   (0x7FFFFFFFFFFFFULL)

#define SMMU_V3_CMDQ_PROD_OFFSET  (0x98)

#define SMMU_V3_CMDQ_CONS_OFFSET          (0x9C)
#define SMMU_V3_CMDQ_ERRORCODE_SHIFT      (24)
#define SMMU_V3_CMDQ_ERRORCODE_MASK       (0x7F)
#define SMMU_V3_CMDQ_CERROR_NONE          (0)
#define SMMU_V3_CMDQ_CERROR_ILL           (1)
#define SMMU_V3_CMDQ_CERROR_ABT           (2)
#define SMMU_V3_CMDQ_CERROR_ATC_INV_SYNC  (3)

#define SMMU_V3_EVTQ_BASE_OFFSET      (0xA0)      // Event Queue Base Address Register
#define SMMU_V3_EVTQ_BASE_ADDR_SHIFT  (5)
#define SMMU_V3_EVTQ_BASE_ADDR_MASK   (0x7FFFFFFFFFFFFULL)

#define SMMU_V3_EVTQ_PROD_OFFSET  ((0xA8))
#define SMMU_V3_EVTQ_CONS_OFFSET  ((0xAC))

#define SMMU_V3_LINEAR_STR_TABLE   (0)
#define SMMU_V3_TWO_LVL_STR_TABLE  (1)

#define SMMU_V3_MIX_ENDIAN  (0)
#define SMMU_V3_RES_ENDIAN  (1)
#define SMMU_V3_LIT_ENDIAN  (2)
#define SMMU_V3_BIG_ENDIAN  (3)

#define SMMU_V3_RES_TTF         (0)
#define SMMU_V3_AARCH32_TTF     (1)
#define SMMU_V3_AARCH64_TTF     (2)
#define SMMU_V3_AARCH32_64_TTF  (3)

#define SMMU_V3_OAS_32BITS  (0)
#define SMMU_V3_OAS_36BITS  (1)
#define SMMU_V3_OAS_40BITS  (2)
#define SMMU_V3_OAS_42BITS  (3)
#define SMMU_V3_OAS_44BITS  (4)
#define SMMU_V3_OAS_48BITS  (5)
#define SMMU_V3_OAS_52BITS  (6)
#define SMMU_V3_OAS_RES     (7)

#define SMMU_V3_CMDQS_MAX         (19)
#define SMMU_V3_EVTQS_MAX         (19)
#define SMMU_V3_SUB_SID_SIZE_MAX  (20)
#define SMMU_V3_SID_SIZE_MAX      (32)

#define SMMU_V3_POLL_ATTEMPTS  (100000)

#define SMMU_V3_Q_DISABLE  (0)
#define SMMU_V3_Q_ENABLE   (1)

#define SMMU_V3_DISABLE  (0)
#define SMMU_V3_ENABLE   (1)

#define SMMU_V3_CMD_SIZE         (16)
#define SMMU_V3_CMD_SIZE_DW      ((SMMU_V3_CMD_SIZE) / (sizeof (UINT64)))
#define SMMU_V3_EVT_RECORD_SIZE  (32)

#define SMMU_V3_STRTAB_ENTRY_SIZE     (64)
#define SMMU_V3_STRTAB_ENTRY_SIZE_DW  ((SMMU_V3_STRTAB_ENTRY_SIZE) / (sizeof (UINT64)))

#define SMMU_V3_RA_HINT_SHIFT  (62)
#define SMMU_V3_WA_HINT_SHIFT  (62)
#define SMMU_V3_STR_FMT_SHIFT  (16)
#define SMMU_V3_WRAP_MASK      (1)

// Bit fields related to command format
#define SMMU_V3_OP_SHIFT         (0)
#define SMMU_V3_OP_MASK          (0xFF)
#define SMMU_V3_SSEC_SHIFT       (10)
#define SMMU_V3_SSEC_MASK        (1)
#define SMMU_V3_CMD_SID_SHIFT    (32)
#define SMMU_V3_CMD_SID_MASK     (0xFFFFFFFF)
#define SMMU_V3_SID_ALL          (0x1F)
#define SMMU_V3_SID_RANGE_SHIFT  (0)
#define SMMU_V3_SID_RANGE_MASK   (0x1F)
#define SMMU_V3_LEAF_STE         (1)
#define SMMU_V3_S_STREAM         (1)
#define SMMU_V3_NS_STREAM        (0)

// Completion Signal
#define SMMU_V3_CSIGNAL_NONE   (0)
#define SMMU_V3_CSIGNAL_SHIFT  (12)
#define SMMU_V3_CSIGNAL_MASK   (0x3)

// Command opcodes
#define SMMU_V3_OP_CFGI_STE       (0x03)
#define SMMU_V3_OP_CFGI_ALL       (0x04)
#define SMMU_V3_OP_TLBI_EL2_ALL   (0x20)
#define SMMU_V3_OP_TLBI_NSNH_ALL  (0x30)
#define SMMU_V3_OP_CMD_SYNC       (0x46)

// Stream Table Entry fields
#define SMMU_V3_STE_VALID              (1ULL)
#define SMMU_V3_STE_CFG_ABORT          (0ULL)
#define SMMU_V3_STE_CFG_BYPASS         (4ULL)
#define SMMU_V3_STE_CFG_STG2           (6ULL)
#define SMMU_V3_STW_EL2                (2ULL)
#define SMMU_V3_UEFI_VM_ID             (1ULL)
#define SMMU_V3_USE_INCOMING_ATTR      (0ULL)
#define SMMU_V3_USE_INCOMING_SH_ATTR   (1ULL)
#define SMMU_V3_WB_CACHEABLE           (1ULL)
#define SMMU_V3_INNER_SHAREABLE        (3ULL)
#define SMMU_V3_S2TF_4KB               (0ULL)
#define SMMU_V3_S2AA64                 (1ULL)
#define SMMU_V3_S2_LITTLEENDIAN        (0ULL)
#define SMMU_V3_AF_DISABLED            (1ULL)
#define SMMU_V3_PTW_DEVICE_FAULT       (1ULL)
#define SMMU_V3_VTTBR_BASE_ADDR_MASK   (0xFFFFFFFFFFFFULL)
#define SMMU_V3_VTTBR_BASE_ADDR_SHIFT  (4ULL)

#define SMMU_V3_WRAP_1DW            (64)
#define SMMU_V3_WRAP_2DW            (128)
#define SMMU_V3_WRAP_3DW            (192)
#define SMMU_V3_STE_CFG_SHIFT       (1)
#define SMMU_V3_STE_CFG_MASK        (0x7)
#define SMMU_V3_STE_STW_SHIFT       (94 - SMMU_V3_WRAP_1DW)
#define SMMU_V3_STE_STW_MASK        (0x3)
#define SMMU_V3_STE_MTCFG_SHIFT     (100 - SMMU_V3_WRAP_1DW)
#define SMMU_V3_STE_MTCFG_MASK      (0x1)
#define SMMU_V3_STE_ALLOCCFG_SHIFT  (101 - SMMU_V3_WRAP_1DW)
#define SMMU_V3_STE_ALLOCCFG_MASK   (0xF)
#define SMMU_V3_STE_SHCFG_SHIFT     (108 - SMMU_V3_WRAP_1DW)
#define SMMU_V3_STE_SHCFG_MASK      (0x3)
#define SMMU_V3_STE_NSCFG_SHIFT     (110 - SMMU_V3_WRAP_1DW)
#define SMMU_V3_STE_NSCFG_MASK      (0x3)
#define SMMU_V3_STE_PRIVCFG_SHIFT   (112 - SMMU_V3_WRAP_1DW)
#define SMMU_V3_STE_PRIVCFG_MASK    (0x3)
#define SMMU_V3_STE_INSTCFG_SHIFT   (114 - SMMU_V3_WRAP_1DW)
#define SMMU_V3_STE_INSTCFG_MASK    (0x3)
#define SMMU_V3_STE_VMID_SHIFT      (128 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_VMID_MASK       0xFFFF
#define SMMU_V3_STE_S2T0SZ_SHIFT    (160 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2T0SZ_MASK     (0x3F)
#define SMMU_V3_STE_S2SL0_SHIFT     (166 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2SL0_MASK      (0x3)
#define SMMU_V3_STE_S2IR0_SHIFT     (168 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2IR0_MASK      (0x3)
#define SMMU_V3_STE_S2OR0_SHIFT     (170 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2OR0_MASK      (0x3)
#define SMMU_V3_STE_S2SH0_SHIFT     (172 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2SH0_MASK      (0x3)
#define SMMU_V3_STE_S2TG_SHIFT      (174 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2TG_MASK       (0x3)
#define SMMU_V3_STE_S2PS_SHIFT      (176 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2PS_MASK       (0x7)
#define SMMU_V3_STE_S2AA64_SHIFT    (179 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2AA64_MASK     (0x1)
#define SMMU_V3_STE_S2ENDI_SHIFT    (180 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2ENDI_MASK     (0x1)
#define SMMU_V3_STE_S2AFFD_SHIFT    (181 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2AFFD_MASK     (0x1)
#define SMMU_V3_STE_S2PTW_SHIFT     (182 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2PTW_MASK      (0x1)
#define SMMU_V3_STE_S2RS_SHIFT      (185 - SMMU_V3_WRAP_2DW)
#define SMMU_V3_STE_S2RS_MASK       (0x3)
#define SMMU_V3_STE_S2TTB_SHIFT     (196 - SMMU_V3_WRAP_3DW)
#define SMMU_V3_STE_S2TTB_MASK      SMMUV3_ALL_ONES(48)

#define SMMU_V3_MAX_PAGE_TABLE_LEVEL    (4)
#define SMMU_V3_PAGE_TABLE_START_LEVEL  (0)

#define SMMU_V3_SMMU_READ   (1)
#define SMMU_V3_SMMU_WRITE  (2)

// Page Table Entry Types
#define SMMU_V3_PTE_TYPE_BLOCK  (0x1)
#define SMMU_V3_PTE_TYPE_TABLE  (0x3)
#define SMMU_V3_PTE_TYPE_PAGE   (0x3)

// Page Table Entry Attributes
#define SMMU_V3_PTE_NG                  (BIT11)        // Non-Global
#define SMMU_V3_PTE_SH_INNER_SHAREABLE  (BIT8 | BIT9)  // Inner Shareable
#define SMMU_V3_PTE_AF                  (BIT10)        // Access Flag
#define SMMU_V3_PTE_AP_RDONLY           (BIT6)         // Read-only
#define SMMU_V3_PTE_AP_WRONLY           (BIT7)         // Write-only
#define SMMU_V3_PTE_AP_READ_WRITE       (BIT7 | BIT6)  // Read/Write

// Memory Attribute and Address Masks
#define SMMU_V3_MAIR_ATTR_IDX_CACHE   (1)
#define SMMU_V3_PTE_ATTR_INDEX_SHIFT  (2)
#define SMMU_V3_PTE_ADDR_MASK         (0xFFFFFFFFF000ULL)

// Combined PTE Flags
#define SMMU_V3_PTE_FLAGS  (SMMU_V3_PTE_NG | /*SMMU_V3_PTE_AP_UNPRIV |*/             \
                                       SMMU_V3_PTE_SH_INNER_SHAREABLE | SMMU_V3_PTE_AF)

#define SMMU_V3_LEVEL0_PAGE_SHIFT  (39)
#define SMMU_V3_LEVEL1_PAGE_SHIFT  (30)
#define SMMU_V3_LEVEL2_PAGE_SHIFT  (21)
#define SMMU_V3_LEVEL3_PAGE_SHIFT  (12)

#define SMMU_V3_LEVEL0_PAGE_SIZE  (1ULL << SMMU_V3_LEVEL0_PAGE_SHIFT)
#define SMMU_V3_LEVEL1_PAGE_SIZE  (1ULL << SMMU_V3_LEVEL1_PAGE_SHIFT)
#define SMMU_V3_LEVEL2_PAGE_SIZE  (1ULL << SMMU_V3_LEVEL2_PAGE_SHIFT)
#define SMMU_V3_LEVEL3_PAGE_SIZE  (1ULL << SMMU_V3_LEVEL3_PAGE_SHIFT)

#define SMMU_V3_PAGE_INDEX_SIZE  (SMMU_V3_LEVEL2_PAGE_SHIFT - SMMU_V3_LEVEL3_PAGE_SHIFT)

typedef struct {
  UINT64    Size;    // Page size for translation level
  UINT32    Shift;   // Page shift value for level
} SMMU_V3_TT_LEVEL;

STATIC CONST SMMU_V3_TT_LEVEL  mSmmuV3TtLevel[] = {
  { SMMU_V3_LEVEL0_PAGE_SIZE, SMMU_V3_LEVEL0_PAGE_SHIFT },
  { SMMU_V3_LEVEL1_PAGE_SIZE, SMMU_V3_LEVEL1_PAGE_SHIFT },
  { SMMU_V3_LEVEL2_PAGE_SIZE, SMMU_V3_LEVEL2_PAGE_SHIFT },
  { SMMU_V3_LEVEL3_PAGE_SIZE, SMMU_V3_LEVEL3_PAGE_SHIFT }
};

typedef struct {
  EFI_PHYSICAL_ADDRESS    QBase;
  UINT32                  RdIdx;
  UINT32                  WrIdx;
  UINT32                  QEntries;
  EFI_PHYSICAL_ADDRESS    ConsRegBase;
  EFI_PHYSICAL_ADDRESS    ProdRegBase;
} SMMU_V3_QUEUE;

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
  SMMU_V3_QUEUE                        CmdQueue;
  SMMU_V3_QUEUE                        EvtQueue;
  EFI_PHYSICAL_ADDRESS                 SteBase;
  EFI_PHYSICAL_ADDRESS                 SteS2TtbBaseAddresses;
  VOID                                 *DeviceTreeBase;
  INT32                                NodeOffset;
  EFI_EVENT                            ReadyToBootEvent;
  NVIDIA_SMMUV3_CONTROLLER_PROTOCOL    SmmuV3ControllerProtocol;
} SMMU_V3_CONTROLLER_PRIVATE_DATA;

#define SMMU_V3_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL(a)  CR(a, SMMU_V3_CONTROLLER_PRIVATE_DATA, SmmuV3ControllerProtocol, SMMU_V3_CONTROLLER_SIGNATURE)

#endif
