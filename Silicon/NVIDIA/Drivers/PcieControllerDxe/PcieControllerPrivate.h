/** @file

  PCIe Controller Driver private structures

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __PCIE_CONTROLLER_PRIVATE_H__
#define __PCIE_CONTROLLER_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#define BIT(x)   (1 << (x))

#define PCIE_CONTROLLER_SIGNATURE SIGNATURE_32('P','C','I','E')
typedef struct {
  //
  // Standard signature used to identify PCIe private data
  //
  UINT32                                           Signature;

  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL PcieRootBridgeConfigurationIo;

  UINT64                                           ApplSpace;
  UINT64                                           ApplSize;
  UINT64                                           ConfigurationSpace;
  UINT64                                           ConfigurationSize;
  UINT64                                           AtuBase;
  UINT64                                           AtuSize;
  UINT64                                           PexCtlBase;
  UINT64                                           PexCtlSize;
  UINT32                                           CtrlId;
} PCIE_CONTROLLER_PRIVATE;
#define PCIE_CONTROLLER_PRIVATE_DATA_FROM_THIS(a) CR(a, PCIE_CONTROLLER_PRIVATE, PcieRootBridgeConfigurationIo, PCIE_CONTROLLER_SIGNATURE)

#define PCIE_DEVICETREE_PREFETCHABLE BIT30
#define PCIE_DEVICETREE_SPACE_CODE   (BIT24 | BIT25)
#define PCIE DEVICETREE_SPACE_CONF   0
#define PCIE_DEVICETREE_SPACE_IO     BIT24
#define PCIE_DEVICETREE_SPACE_MEM32  BIT25
#define PCIE_DEVICETREE_SPACE_MEM64  (BIT24 | BIT25)

/* OUTBOUND */
#define TEGRA_PCIE_ATU_CR1      0x0
#define TEGRA_PCIE_ATU_TYPE_MEM     (0x0 << 0)
#define TEGRA_PCIE_ATU_TYPE_IO      (0x2 << 0)
#define TEGRA_PCIE_ATU_TYPE_CFG0    (0x4 << 0)
#define TEGRA_PCIE_ATU_TYPE_CFG1    (0x5 << 0)
#define TEGRA_PCIE_ATU_TYPE_TD_SHIFT    8
#define TEGRA_PCIE_ATU_INCREASE_REGION_SIZE BIT13
#define TEGRA_PCIE_ATU_CR2      0x4
#define TEGRA_PCIE_ATU_ENABLE     (0x1 << 31)
#define TEGRA_PCIE_ATU_LOWER_BASE   0x8
#define TEGRA_PCIE_ATU_UPPER_BASE   0xC
#define TEGRA_PCIE_ATU_LIMIT      0x10
#define TEGRA_PCIE_ATU_LOWER_TARGET   0x14
#define TEGRA_PCIE_ATU_UPPER_TARGET   0x18
#define TEGRA_PCIE_ATU_UPPER_LIMIT    0x20

#define PCIE_ATU_REGION_INDEX0  0 /* used for EXT-CFG accesses */
#define PCIE_ATU_REGION_INDEX1  1 /* used for IO accesses */
#define PCIE_ATU_REGION_INDEX2  2 /* used for Non-Prefetchable MEM accesses */
#define PCIE_ATU_REGION_INDEX3  3 /* used for Prefetchable MEM accesses */

#define PCIE_ATU_BUS(x)     (((x) & 0xff) << 24)
#define PCIE_ATU_DEV(x)     (((x) & 0x1f) << 19)
#define PCIE_ATU_FUNC(x)    (((x) & 0x7) << 16)

#define APPL_PINMUX                             0x0
#define APPL_PINMUX_PEX_RST                     BIT(0)
#define APPL_PINMUX_CLKREQ_OVERRIDE_EN          BIT(2)
#define APPL_PINMUX_CLKREQ_OVERRIDE             BIT(3)
#define APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE_EN   BIT(4)
#define APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE      BIT(5)
#define APPL_PINMUX_CLKREQ_OUT_OVRD_EN          BIT(9)
#define APPL_PINMUX_CLKREQ_OUT_OVRD             BIT(10)

#define APPL_CTRL                               0x4
#define APPL_CTRL_SYS_PRE_DET_STATE             BIT(6)
#define APPL_CTRL_LTSSM_EN                      BIT(7)

#define APPL_DM_TYPE                            0x100
#define APPL_DM_TYPE_MASK                       0xF
#define APPL_DM_TYPE_RP                         0x4
#define APPL_DM_TYPE_EP                         0x0

#define APPL_CFG_BASE_ADDR                      0x104
#define APPL_CFG_BASE_ADDR_MASK                 0xFFFFF000

#define APPL_CFG_IATU_DMA_BASE_ADDR             0x108
#define APPL_CFG_IATU_DMA_BASE_ADDR_MASK        0xFFFC0000

#define APPL_CFG_MISC                           0x110
#define APPL_CFG_MISC_SLV_EP_MODE               BIT(14)
#define APPL_CFG_MISC_ARCACHE_SHIFT             10
#define APPL_CFG_MISC_ARCACHE_VAL               3

#define APPL_CFG_SLCG_OVERRIDE    0x114

#define PCI_BASE_ADDRESS_0        0x10  /* 32 bits */
#define PCI_BASE_ADDRESS_1        0x14  /* 32 bits */

#define PCI_IO_BASE               0x1c  /* I/O range behind the bridge */
#define IO_BASE_IO_DECODE         BIT(0)
#define IO_BASE_IO_DECODE_BIT8    BIT(8)

#define PCI_PREF_MEMORY_BASE      0x24  /* Prefetchable memory range behind */
#define CFG_PREF_MEM_LIMIT_BASE_MEM_DECODE              BIT(0)
#define CFG_PREF_MEM_LIMIT_BASE_MEM_LIMIT_DECODE        BIT(16)

#define PCI_EXP_LNKCAP          0x7C
#define  PCI_EXP_LNKCAP_SLS     0x0000000f /* Supported Link Speeds */

#define PCI_EXP_LNKCTL_STATUS   0x80
#define PCI_EXP_LNKCTL_STATUS_DLL_ACTIVE   BIT(29)

#define PCI_EXP_LNKCTL_STS_2    0xa0

#define PCIE_MISC_CONTROL_1_OFF   0x8BC
#define PCIE_DBI_RO_WR_EN         (0x1 << 0)

#endif
