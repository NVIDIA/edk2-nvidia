/** @file

  PCIe Segment Controller Driver private structures

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_CONTROLLER_PRIVATE_H__
#define __PCIE_CONTROLLER_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Protocol/EmbeddedGpio.h>
#include <T410/T410Definitions.h>

#define BIT(x)  (1 << (x))

#define upper_32_bits(n)  ((UINT32)((n) >> 32))
#define lower_32_bits(n)  ((UINT32)(n))

#define PCIE_NUMBER_OF_MAPPING_SPACE  4
#define PCIE_NUMBER_OF_INTERRUPT_MAP  4
#define PCIE_REPO_OBJECTS             (3 + PCIE_NUMBER_OF_MAPPING_SPACE + PCIE_NUMBER_OF_INTERRUPT_MAP) // 2 Reference Arrays, Mappings, End of list
#define PCIE_COMMON_REPO_OBJECTS      (3)                                                               // Config Space, Acpi Tables, end of list

#define GPU_SENSE_MAX_COUNT  500
#define GPU_KICK_MAX_COUNT   5
#define GPU_SENSE_DELAY      1
#define GPU_RESET_DELAY      30000

#define PCIE_FW_OSC_CTRL_PCIE_NATIVE_HP      BIT(0)
#define PCIE_FW_OSC_CTRL_SHPC_NATIVE_HP      BIT(1)
#define PCIE_FW_OSC_CTRL_PCIE_NATIVE_PME     BIT(2)
#define PCIE_FW_OSC_CTRL_PCIE_AER            BIT(3)
#define PCIE_FW_OSC_CTRL_PCIE_CAP_STRUCTURE  BIT(4)
#define PCIE_FW_OSC_CTRL_LTR                 BIT(5)
#define PCIE_FW_OSC_CTRL_RSVD                BIT(6)
#define PCIE_FW_OSC_CTRL_PCIE_DPC            BIT(7)
#define PCIE_FW_OSC_CTRL_PCIE_CMPL_TO        BIT(8)
#define PCIE_FW_OSC_CTRL_PCIE_SFI            BIT(9)

#define PCIE_CONTROLLER_SIGNATURE  SIGNATURE_32('P','C','I','E')

//
// One bifurcated root port under a segment controller. RpId identifies which
// of the controller's up-to-T410_MAX_RP_PER_SEGMENT_CTLR DT "pcie@N" subnodes
// this is; each
// gets its own handle (see ParseRpNode) so InstallMultipleProtocolInterfaces
// never collides across root ports sharing one controller.
//
typedef struct {
  //
  // Standard signature used to identify PCIe private data
  //
  UINT32                                              Signature;
  UINT32                                              RpId;

  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL    PcieRootBridgeConfigurationIo;

  UINT32                                              PCIeCapOff;

  UINT64                                              MemBase;
  UINT64                                              MemLimit;
  UINT64                                              PrefetchMemBase;
  UINT64                                              PrefetchMemLimit;
  UINT64                                              IoBase;
  UINT64                                              IoLimit;

  UINT64                                              EcamBase;
  UINT64                                              EcamSize;

  // Configuration data
  CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO                ConfigSpaceInfo;
  UINT32                                              AddressMapCount;
  CM_ARCH_COMMON_PCI_ADDRESS_MAP_INFO                 AddressMapInfo[PCIE_NUMBER_OF_MAPPING_SPACE];
  CM_ARCH_COMMON_OBJ_REF                              AddressMapRefInfo[PCIE_NUMBER_OF_MAPPING_SPACE];
  CM_ARCH_COMMON_PCI_INTERRUPT_MAP_INFO               InterruptMapInfo[PCIE_NUMBER_OF_INTERRUPT_MAP];
  CM_ARCH_COMMON_OBJ_REF                              InterruptRefInfo[PCIE_NUMBER_OF_INTERRUPT_MAP];
  LEGACY_CM_PROTOCOL_OBJECT                           RepoInfo[PCIE_REPO_OBJECTS];
} PCIE_RP_PRIVATE;

//
// One segment controller: one shared cxl_brg register bank
// (programmed to dynamically patch each enabled RP's ECAM/bus-range/BAR
// windows) and up to T410_MAX_RP_PER_SEGMENT_CTLR bifurcated root ports under it.
//
typedef struct {
  UINT32             SegmentCtlrId;
  UINT32             SocketId;
  UINT32             Segment;
  UINT64             CxlBrgBase;
  UINT64             CxlBrgSize;
  PCIE_RP_PRIVATE    RpInfo[T410_MAX_RP_PER_SEGMENT_CTLR];
} PCIE_CONTROLLER_PRIVATE;

#define RP_PRIVATE_DATA_FROM_THIS(a)  CR(a, PCIE_RP_PRIVATE, PcieRootBridgeConfigurationIo, PCIE_CONTROLLER_SIGNATURE)

#define PCIE_DEVICETREE_PREFETCHABLE  BIT30
#define PCIE_DEVICETREE_SPACE_CODE    (BIT24 | BIT25)
#define PCIE                          DEVICETREE_SPACE_CONF   0
#define PCIE_DEVICETREE_SPACE_IO      BIT24
#define PCIE_DEVICETREE_SPACE_MEM32   BIT25
#define PCIE_DEVICETREE_SPACE_MEM64   (BIT24 | BIT25)

#define T410_PCIE_BUS_RANGE_PROP_LEN               2
#define T410_PCIE_ECAM_PROP_LEN                    4
#define T410_PCIE_RANGES_PREF_NONPREF_PROP_LEN     14
#define T410_PCIE_RANGES_IO_PREF_NONPREF_PROP_LEN  21

//
// CXL_BRG register offsets.  The bank is shared by every root port
// under a segment controller; each RP's ECAM window and bus range are read
// from it.
//
#define NV_CXL_BRG_BAR_CNTL_ECAM_0               0x10800
#define NV_CXL_BRG_BAR_CNTL_ECAM_STRIDE          4
#define NV_CXL_BRG_BAR_CNTL_ECAM_ENABLED         BIT0
#define NV_CXL_BRG_ECAM_BASE_HI_0                0x10824
#define NV_CXL_BRG_ECAM_BASE_LO_0                0x10828
#define NV_CXL_BRG_ECAM_RP_0                     0x10830
#define NV_CXL_BRG_ECAM_RP_BUS_MASK              0xFFUL
#define NV_CXL_BRG_ECAM_RP_END_BUS_SHIFT         8
#define NV_CXL_BRG_BAR_CNTL_STANDARD_0_0         0x1092c
#define NV_CXL_BRG_BAR_CNTL_STANDARD_64B_BAR_EN  BIT1
#define NV_CXL_BRG_BAR_CNTL_STANDARD_32B_BAR_EN  BIT0
#define NV_CXL_BRG_MEM_32BIT_BASE_HI_0_0         0x10930
#define NV_CXL_BRG_MEM_32BIT_BASE_LO_0_0         0x10934
#define NV_CXL_BRG_MEM_32BIT_LIMIT_HI_0_0        0x10938
#define NV_CXL_BRG_MEM_32BIT_LIMIT_LO_0_0        0x1093c
#define NV_CXL_BRG_MEM_64BIT_BASE_HI_0_0         0x10940
#define NV_CXL_BRG_MEM_64BIT_BASE_LO_0_0         0x10944
#define NV_CXL_BRG_MEM_64BIT_LIMIT_HI_0_0        0x10948
#define NV_CXL_BRG_MEM_64BIT_LIMIT_LO_0_0        0x1094c
#define NV_CXL_BRG_BAR_CNTL_STRIDE               0x24

/* PCIExpress Capability */
#define PCIE_DEV_CAP_ERR_COR_SUB_CLASS  BIT(16)

/* AER Registers */
#define  PCIE_AER_CORR_ERR_ADV_NONFATAL  0x2000

/* DPC Registers */
#define PCI_EXPRESS_EXTENDED_CAPABILITY_DPC_ID  0x001D

#define PCIE_DPC_CAP                      0x4
#define PCIE_DPC_CAP_RP_EXT               BIT(5)
#define PCIE_DPC_CTL                      0x6
#define PCIE_DPC_CTL_DPC_TRIGGER_EN_F     0x1
#define PCIE_DPC_CTL_DPC_TRIGGER_EN_NF_F  0x2
#define PCIE_DPC_CTL_DPC_INT_EN           BIT(3)
#define PCIE_DPC_CTL_DPC_ERR_COR_EN       BIT(4)
#define PCIE_DPC_CTL_DPC_SIG_SFW_EN       BIT(8)

#define PCIE_DPC_STS                 0x8
#define PCIE_DPC_STS_TRIGGER_STATUS  BIT(0)
#define PCIE_DPC_STS_SIG_SFW_STATUS  BIT(13)

#define PCIE_DPC_ERR_SRC_ID  0xA

#define PCIE_AER_ECRC_GEN_CAP  BIT(5)
#define PCIE_AER_ECRC_GEN_EN   BIT(6)
#define PCIE_AER_ECRC_CHK_CAP  BIT(7)
#define PCIE_AER_ECRC_CHK_EN   BIT(8)

#endif
