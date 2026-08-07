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
typedef struct {
  //
  // Standard signature used to identify PCIe private data
  //
  UINT32                                              Signature;

  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL    PcieRootBridgeConfigurationIo;

  UINT32                                              CtrlId;
  UINT32                                              SocketId;
  UINT64                                              EcamBase;
  UINT64                                              EcamSize;
  UINT32                                              PCIeCapOff;

  UINT64                                              MemBase;
  UINT64                                              MemLimit;
  UINT64                                              PrefetchMemBase;
  UINT64                                              PrefetchMemLimit;
  UINT64                                              IoBase;
  UINT64                                              IoLimit;
  UINT32                                              BusMask;
  // Configuration data
  CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO                ConfigSpaceInfo;
  UINT32                                              AddressMapCount;
  CM_ARCH_COMMON_PCI_ADDRESS_MAP_INFO                 AddressMapInfo[PCIE_NUMBER_OF_MAPPING_SPACE];
  CM_ARCH_COMMON_OBJ_REF                              AddressMapRefInfo[PCIE_NUMBER_OF_MAPPING_SPACE];
  CM_ARCH_COMMON_PCI_INTERRUPT_MAP_INFO               InterruptMapInfo[PCIE_NUMBER_OF_INTERRUPT_MAP];
  CM_ARCH_COMMON_OBJ_REF                              InterruptRefInfo[PCIE_NUMBER_OF_INTERRUPT_MAP];
  LEGACY_CM_PROTOCOL_OBJECT                           RepoInfo[PCIE_REPO_OBJECTS];
} PCIE_CONTROLLER_PRIVATE;
#define PCIE_CONTROLLER_PRIVATE_DATA_FROM_THIS(a)  CR(a, PCIE_CONTROLLER_PRIVATE, PcieRootBridgeConfigurationIo, PCIE_CONTROLLER_SIGNATURE)

#define PCIE_DEVICETREE_PREFETCHABLE  BIT30
#define PCIE_DEVICETREE_SPACE_CODE    (BIT24 | BIT25)
#define PCIE                          DEVICETREE_SPACE_CONF   0
#define PCIE_DEVICETREE_SPACE_IO      BIT24
#define PCIE_DEVICETREE_SPACE_MEM32   BIT25
#define PCIE_DEVICETREE_SPACE_MEM64   (BIT24 | BIT25)

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
