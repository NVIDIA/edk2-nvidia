/** @file

  PCIe Controller Driver private structures

  Copyright (c) 2019-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_CONTROLLER_PRIVATE_H__
#define __PCIE_CONTROLLER_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#define BIT(x)  (1UL << (x))

#define upper_32_bits(n)  ((UINT32)((n) >> 32))
#define lower_32_bits(n)  ((UINT32)(n))

#define SZ_256M  0x10000000

#define PCIE_CLOCK_RESET_NAME_LENGTH  16

#define PCIE_CONTROLLER_SIGNATURE  SIGNATURE_32('P','C','I','E')
typedef struct {
  //
  // Standard signature used to identify PCIe private data
  //
  UINT32                                              Signature;

  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL    PcieRootBridgeConfigurationIo;
  NVIDIA_BPMP_IPC_PROTOCOL                            *BpmpIpcProtocol;

  EFI_HANDLE                                          ControllerHandle;

  UINT64                                              ApplSpace;
  UINT64                                              ApplSize;
  UINT64                                              ConfigurationSpace;
  UINT64                                              ConfigurationSize;
  UINT64                                              AtuBase;
  UINT64                                              AtuSize;
  UINT64                                              DbiBase;
  UINT64                                              DbiSize;
  UINT64                                              EcamBase;
  UINT64                                              EcamSize;
  UINT64                                              PexCtlBase;
  UINT64                                              PexCtlSize;
  UINT32                                              CtrlId;
  UINT32                                              MaxLinkSpeed;
  UINT32                                              NumLanes;
  BOOLEAN                                             UpdateFCFixUp;
  UINT32                                              PcieCapOffset;
  UINT32                                              ASPML1SSCapOffset;
  BOOLEAN                                             LinkUp;
  BOOLEAN                                             IsT194;
  BOOLEAN                                             IsT234;
  BOOLEAN                                             EnableSRNS;
  BOOLEAN                                             EnableExtREFCLK;
} PCIE_CONTROLLER_PRIVATE;
#define PCIE_CONTROLLER_PRIVATE_DATA_FROM_THIS(a)  CR(a, PCIE_CONTROLLER_PRIVATE, PcieRootBridgeConfigurationIo, PCIE_CONTROLLER_SIGNATURE)

#define PCI_CFG_SPACE_SIZE      256
#define PCI_CFG_SPACE_EXP_SIZE  4096

/* Extended Capabilities (PCI-X 2.0 and Express) */
#define PCI_EXT_CAP_ID(header)    (header & 0x0000ffff)
#define PCI_EXT_CAP_VER(header)   ((header >> 16) & 0xf)
#define PCI_EXT_CAP_NEXT(header)  ((header >> 20) & 0xffc)

#define PCIE_DEVICETREE_PREFETCHABLE  BIT30
#define PCIE_DEVICETREE_SPACE_CODE    (BIT24 | BIT25)
#define PCIE                          DEVICETREE_SPACE_CONF   0
#define PCIE_DEVICETREE_SPACE_IO      BIT24
#define PCIE_DEVICETREE_SPACE_MEM32   BIT25
#define PCIE_DEVICETREE_SPACE_MEM64   (BIT24 | BIT25)

/* OUTBOUND */
#define TEGRA_PCIE_ATU_CR1                   0x0
#define TEGRA_PCIE_ATU_TYPE_MEM              (0x0 << 0)
#define TEGRA_PCIE_ATU_TYPE_IO               (0x2 << 0)
#define TEGRA_PCIE_ATU_TYPE_CFG0             (0x4 << 0)
#define TEGRA_PCIE_ATU_TYPE_CFG1             (0x5 << 0)
#define TEGRA_PCIE_ATU_TYPE_TD_SHIFT         8
#define TEGRA_PCIE_ATU_INCREASE_REGION_SIZE  BIT13
#define TEGRA_PCIE_ATU_CR2                   0x4
#define TEGRA_PCIE_ATU_ENABLE                (0x1UL << 31)
#define TEGRA_PCIE_ATU_LOWER_BASE            0x8
#define TEGRA_PCIE_ATU_UPPER_BASE            0xC
#define TEGRA_PCIE_ATU_LIMIT                 0x10
#define TEGRA_PCIE_ATU_LOWER_TARGET          0x14
#define TEGRA_PCIE_ATU_UPPER_TARGET          0x18
#define TEGRA_PCIE_ATU_UPPER_LIMIT           0x20

#define PCIE_ATU_REGION_INDEX0  0 /* used for EXT-CFG accesses */
#define PCIE_ATU_REGION_INDEX1  1 /* used for IO accesses */
#define PCIE_ATU_REGION_INDEX2  2 /* used for Non-Prefetchable MEM accesses */
#define PCIE_ATU_REGION_INDEX3  3 /* used for Prefetchable MEM accesses */

#define PCIE_ATU_BUS(x)   (((x) & 0xff) << 24)
#define PCIE_ATU_DEV(x)   (((x) & 0x1f) << 19)
#define PCIE_ATU_FUNC(x)  (((x) & 0x7) << 16)

#define APPL_PINMUX                            0x0
#define APPL_PINMUX_PEX_RST                    BIT(0)
#define APPL_PINMUX_CLKREQ_OVERRIDE_EN         BIT(2)
#define APPL_PINMUX_CLKREQ_OVERRIDE            BIT(3)
#define APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE_EN  BIT(4)
#define APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE     BIT(5)
#define APPL_PINMUX_CLKREQ_OUT_OVRD_EN         BIT(9)
#define APPL_PINMUX_CLKREQ_OUT_OVRD            BIT(10)
#define APPL_PINMUX_CLKREQ_DEFAULT_VALUE       BIT(13)

#define APPL_CTRL                                    0x4
#define APPL_CTRL_SYS_PRE_DET_STATE                  BIT(6)
#define APPL_CTRL_LTSSM_EN                           BIT(7)
#define APPL_CTRL_HW_HOT_RST_EN                      BIT(20)
#define APPL_CTRL_HW_HOT_RST_MODE_MASK               0x3
#define APPL_CTRL_HW_HOT_RST_MODE_SHIFT              22
#define APPL_CTRL_HW_HOT_RST_MODE_IMDT_RST           0x1
#define APPL_CTRL_HW_HOT_RST_MODE_IMDT_RST_LTSSM_EN  0x2

#define APPL_INTR_EN_L0_0                  0x8
#define APPL_INTR_EN_L0_0_MSI_RCV_INT_EN   BIT(4)
#define APPL_INTR_EN_L0_0_INT_INT_EN       BIT(8)
#define APPL_INTR_EN_L0_0_SYS_INTR_EN      BIT(30)
#define APPL_INTR_EN_L0_0_SYS_MSI_INTR_EN  BIT(31)

#define APPL_INTR_EN_L1_8_0           0x44
#define APPL_INTR_EN_L1_8_INTX_EN     BIT(11)
#define APPL_INTR_EN_L1_8_AER_INT_EN  BIT(15)

#define APPL_LINK_STATUS               0xCC
#define APPL_LINK_STATUS_RDLH_LINK_UP  BIT(0)

#define APPL_DEBUG                      0xD0
#define APPL_DEBUG_PM_LINKST_IN_L2_LAT  BIT(21)
#define APPL_DEBUG_PM_LINKST_IN_L0      0x11
#define APPL_DEBUG_LTSSM_STATE_MASK     0x1F8
#define APPL_DEBUG_LTSSM_STATE_SHIFT    3
#define LTSSM_STATE_PRE_DETECT          5
#define LTSSM_STATE_DETECT_QUIET        0x00
#define LTSSM_STATE_DETECT_ACT          0x08
#define LTSSM_STATE_PRE_DETECT_QUIET    0x28
#define LTSSM_STATE_DETECT_WAIT         0x30
#define LTSSM_STATE_L2_IDLE             0xa8

#define APPL_RADM_STATUS           0xE4
#define APPL_PM_XMT_TURNOFF_STATE  BIT(0)

#define APPL_DM_TYPE       0x100
#define APPL_DM_TYPE_MASK  0xF
#define APPL_DM_TYPE_RP    0x4
#define APPL_DM_TYPE_EP    0x0

#define APPL_CFG_BASE_ADDR       0x104
#define APPL_CFG_BASE_ADDR_MASK  0xFFFFF000

#define APPL_CFG_IATU_DMA_BASE_ADDR       0x108
#define APPL_CFG_IATU_DMA_BASE_ADDR_MASK  0xFFFC0000

#define APPL_CFG_MISC                0x110
#define APPL_CFG_MISC_SLV_EP_MODE    BIT(14)
#define APPL_CFG_MISC_ARCACHE_SHIFT  10
#define APPL_CFG_MISC_ARCACHE_VAL    3

#define APPL_CFG_SLCG_OVERRIDE  0x114

#define APPL_ECAM_REGION_LOWER_BASE  0x150
#define APPL_ECAM_REGION_UPPER_BASE  0x154
#define APPL_ECAM_CONFIG_BASE        0x158
#define APPL_ECAM_CONFIG_REGION_EN   BIT(31)
#define APPL_ECAM_CONFIG_MODE_EN     BIT(30)
#define APPL_ECAM_CONFIG_LIMIT       0x0FFFFFFF

#define PCI_BASE_ADDRESS_0  0x10        /* 32 bits */
#define PCI_BASE_ADDRESS_1  0x14        /* 32 bits */

#define PCI_IO_BASE             0x1c    /* I/O range behind the bridge */
#define IO_BASE_IO_DECODE       BIT(0)
#define IO_BASE_IO_DECODE_BIT8  BIT(8)

#define PCI_PREF_MEMORY_BASE                      0x24/* Prefetchable memory range behind */
#define CFG_PREF_MEM_LIMIT_BASE_MEM_DECODE        BIT(0)
#define CFG_PREF_MEM_LIMIT_BASE_MEM_LIMIT_DECODE  BIT(16)

#define PCI_EXP_LNKCAP             0x7C
#define  PCI_EXP_LNKCAP_SLS        0x0000000f /* Supported Link Speeds */
#define  PCI_EXP_LNKCAP_MLW        0x000003f0 /* Maximum Link Width */
#define  PCI_EXP_LNKSTA_NLW_SHIFT  4          /* start of NLW mask in link status */

#define PCI_EXP_LNKCTL_STATUS                    0x80
#define PCI_EXP_LNKCTL_STATUS_SLOT_CLOCK_CONFIG  BIT(28)
#define PCI_EXP_LNKCTL_STATUS_DLL_ACTIVE         BIT(29)

#define PCI_EXP_LNKCTL_STS_2  0xa0

#define PCIE_MISC_CONTROL_1_OFF  0x8BC
#define PCIE_DBI_RO_WR_EN        (0x1 << 0)

#define PADCTL_PEX_RST          0x14008
#define PADCTL_PEX_RST_E_INPUT  BIT(6)

#define PORT_LOGIC_ACK_F_ASPM_CTRL  0x70C
#define ENTER_ASPM                  BIT(30)
#define L0S_ENTRANCE_LAT_SHIFT      24
#define L0S_ENTRANCE_LAT_MASK       0x07000000
#define L1_ENTRANCE_LAT_SHIFT       27
#define L1_ENTRANCE_LAT_MASK        0x38000000
#define CC_N_FTS_SHIFT              16
#define N_FTS_SHIFT                 8
#define N_FTS_MASK                  0xff
#define N_FTS_VAL                   52

#define PCIE_PORT_LINK_CONTROL    0x710
#define PORT_LINK_CAP_MASK        0x3f0000
#define PORT_LINK_CAP_SHIFT       16
#define PORT_LINK_DLL_LINK_EN     BIT(5)
#define PORT_LINK_FAST_LINK_MODE  BIT(7)

#define PORT_LOGIC_GEN2_CTRL                      0x80C
#define PORT_LOGIC_LINK_WIDTH_MASK                0x1f00
#define PORT_LOGIC_LINK_WIDTH_SHIFT               8
#define PORT_LOGIC_GEN2_CTRL_DIRECT_SPEED_CHANGE  BIT(17)
#define FTS_MASK                                  0xff
#define FTS_VAL                                   52

#define PORT_LOGIC_AMBA_ERROR_RESPONSE_DEFAULT  0x8D0
#define AMBA_ERROR_RESPONSE_CRS_SHIFT           3
#define AMBA_ERROR_RESPONSE_CRS_MASK            0x3
#define AMBA_ERROR_RESPONSE_CRS_OKAY            0
#define AMBA_ERROR_RESPONSE_CRS_OKAY_FFFFFFFF   1
#define AMBA_ERROR_RESPONSE_CRS_OKAY_FFFF0001   2

/* ASPM L1 PM Substates */
#define PCI_L1SS_CAP                     0x04       /* Capabilities Register */
#define  PCI_L1SS_CAP_PCIPM_L1_2         0x00000001 /* PCI-PM L1.2 Supported */
#define  PCI_L1SS_CAP_PCIPM_L1_1         0x00000002 /* PCI-PM L1.1 Supported */
#define  PCI_L1SS_CAP_ASPM_L1_2          0x00000004 /* ASPM L1.2 Supported */
#define  PCI_L1SS_CAP_ASPM_L1_1          0x00000008 /* ASPM L1.1 Supported */
#define  PCI_L1SS_CAP_L1_PM_SS           0x00000010 /* L1 PM Substates Supported */
#define  PCI_L1SS_CAP_CM_RESTORE_TIME    0x0000ff00 /* Port Common_Mode_Restore_Time */
#define  PCI_L1SS_CAP_P_PWR_ON_SCALE     0x00030000 /* Port T_POWER_ON scale */
#define  PCI_L1SS_CAP_P_PWR_ON_VALUE     0x00f80000 /* Port T_POWER_ON value */
#define PCI_L1SS_CTL1                    0x08       /* Control 1 Register */
#define  PCI_L1SS_CTL1_PCIPM_L1_1        0x00000002 /* PCI-PM L1.1 Enable */
#define  PCI_L1SS_CTL1_PCIPM_L1_2        0x00000001 /* PCI-PM L1.2 Enable */
#define  PCI_L1SS_CTL1_ASPM_L1_2         0x00000004 /* ASPM L1.2 Enable */
#define  PCI_L1SS_CTL1_ASPM_L1_1         0x00000008 /* ASPM L1.1 Enable */
#define  PCI_L1SS_CTL1_L1SS_MASK         0x0000000f
#define  PCI_L1SS_CTL1_CM_RESTORE_TIME   0x0000ff00 /* Common_Mode_Restore_Time */
#define  PCI_L1SS_CTL1_LTR_L12_TH_VALUE  0x03ff0000 /* LTR_L1.2_THRESHOLD_Value */
#define  PCI_L1SS_CTL1_LTR_L12_TH_SCALE  0xe0000000 /* LTR_L1.2_THRESHOLD_Scale */
#define PCI_L1SS_CTL2                    0x0c       /* Control 2 Register */

#define CAP_SPCIE_CAP_OFF                       0x154
#define CAP_SPCIE_CAP_OFF_DSP_TX_PRESET0_MASK   0xf
#define CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_MASK   0xf00
#define CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_SHIFT  8

#define GEN3_GEN4_EQ_PRESET_INIT  5

#define PCI_EXT_CAP_ID_DLF  0x25            /* Data Link Feature */
/* Data Link Feature */
#define PCI_DLF_CAP               0x04            /* Capabilities Register */
#define  PCI_DLF_EXCHANGE_ENABLE  0x80000000      /* Data Link Feature Exchange Enable */

#define PCI_EXT_CAP_ID_PL_16GT  0x26        /* Physical Layer 16.0 GT/s */
/* Physical Layer 16.0 GT/s */
#define PCI_PL_16GT_LE_CTRL                       0x20/* Lane Equalization Control Register */
#define  PCI_PL_16GT_LE_CTRL_DSP_TX_PRESET_MASK   0x0000000F
#define  PCI_PL_16GT_LE_CTRL_USP_TX_PRESET_MASK   0x000000F0
#define  PCI_PL_16GT_LE_CTRL_USP_TX_PRESET_SHIFT  4

#define CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF  0x718
#define CFG_TIMER_CTRL_ACK_NAK_SHIFT     (19)

#define GEN3_EQ_CONTROL_OFF                     0x8a8
#define GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT  8
#define GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_MASK   0x00ffff00
#define GEN3_EQ_CONTROL_OFF_FB_MODE_MASK        0xf

#define GEN3_RELATED_OFF                        0x890
#define GEN3_RELATED_OFF_GEN3_ZRXDC_NONCOMPL    BIT(0)
#define GEN3_RELATED_OFF_GEN3_EQ_DISABLE        BIT(16)
#define GEN3_RELATED_OFF_RATE_SHADOW_SEL_SHIFT  24
#define GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK   0x03000000

#endif
