/** @file

  PCIe Controller Driver private structures

  Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_CONTROLLER_PRIVATE_H__
#define __PCIE_CONTROLLER_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <TH500/TH500Definitions.h>

#define BIT(x)  (1 << (x))

#define upper_32_bits(n)  ((UINT32)((n) >> 32))
#define lower_32_bits(n)  ((UINT32)(n))

#define PCIE_NUMBER_OF_MAPPING_SPACE  3
#define PCIE_NUMBER_OF_INTERUPT_MAP   4
#define PCIE_REPO_OBJECTS             (5 + PCIE_NUMBER_OF_MAPPING_SPACE + PCIE_NUMBER_OF_INTERUPT_MAP)// Config Space, 2 Reference Arrays, Mappings, Acpi tables, End of list
#define SPI_OFFSET                    (32U)

#define PCIE_CHILD_ADDRESS_OFFSET           0
#define PCIE_CHILD_INT_OFFSET               3
#define PCIE_INTERRUPT_PARENT_OFFSET        4
#define PCIE_PARENT_ADDRESS_OFFSET          5
#define PCIE_PARENT_INTERRUPT_OFFSET        6
#define PCIE_PARENT_INTERRUPT_SENSE_OFFSET  7
#define PCIE_INTERRUPT_MAP_ENTRIES          8
#define PCIE_INTERRUPT_MAP_ENTRY_SIZE       (PCIE_INTERRUPT_MAP_ENTRIES * sizeof (UINT32))

#define PCIE_CONTROLLER_SIGNATURE  SIGNATURE_32('P','C','I','E')
typedef struct {
  //
  // Standard signature used to identify PCIe private data
  //
  UINT32                                              Signature;

  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL    PcieRootBridgeConfigurationIo;

  UINT32                                              CtrlId;

  UINT64                                              XalBase;
  UINT64                                              XalSize;
  UINT64                                              XtlBase;
  UINT64                                              XtlSize;
  UINT64                                              XtlPriBase;
  UINT64                                              XtlPriSize;
  UINT64                                              XplBase;
  UINT64                                              XplSize;
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
  CM_ARM_PCI_CONFIG_SPACE_INFO                        ConfigSpaceInfo;
  UINT32                                              AddressMapCount;
  CM_ARM_PCI_ADDRESS_MAP_INFO                         AddressMapInfo[PCIE_NUMBER_OF_MAPPING_SPACE];
  CM_ARM_OBJ_REF                                      AddressMapRefInfo[PCIE_NUMBER_OF_MAPPING_SPACE];
  CM_ARM_PCI_INTERRUPT_MAP_INFO                       InterruptMapInfo[PCIE_NUMBER_OF_INTERUPT_MAP];
  CM_ARM_OBJ_REF                                      InterruptRefInfo[PCIE_NUMBER_OF_INTERUPT_MAP];
  EDKII_PLATFORM_REPOSITORY_INFO                      RepoInfo[PCIE_REPO_OBJECTS];
} PCIE_CONTROLLER_PRIVATE;
#define PCIE_CONTROLLER_PRIVATE_DATA_FROM_THIS(a)  CR(a, PCIE_CONTROLLER_PRIVATE, PcieRootBridgeConfigurationIo, PCIE_CONTROLLER_SIGNATURE)

#define PCIE_DEVICETREE_PREFETCHABLE  BIT30
#define PCIE_DEVICETREE_SPACE_CODE    (BIT24 | BIT25)
#define PCIE                          DEVICETREE_SPACE_CONF   0
#define PCIE_DEVICETREE_SPACE_IO      BIT24
#define PCIE_DEVICETREE_SPACE_MEM32   BIT25
#define PCIE_DEVICETREE_SPACE_MEM64   (BIT24 | BIT25)

/* XAL registers */
#define XAL_RC_ECAM_BASE_HI                  0x0
#define XAL_RC_ECAM_BASE_LO                  0x4
#define XAL_RC_ECAM_BUSMASK                  0x8
#define XAL_RC_IO_BASE_HI                    0xc
#define XAL_RC_IO_BASE_LO                    0x10
#define XAL_RC_IO_LIMIT_HI                   0x14
#define XAL_RC_IO_LIMIT_LO                   0x18
#define XAL_RC_MEM_32BIT_BASE_HI             0x1c
#define XAL_RC_MEM_32BIT_BASE_LO             0x20
#define XAL_RC_MEM_32BIT_LIMIT_HI            0x24
#define XAL_RC_MEM_32BIT_LIMIT_LO            0x28
#define XAL_RC_MEM_64BIT_BASE_HI             0x2c
#define XAL_RC_MEM_64BIT_BASE_LO             0x30
#define XAL_RC_MEM_64BIT_LIMIT_HI            0x34
#define XAL_RC_MEM_64BIT_LIMIT_LO            0x38
#define XAL_RC_BAR_CNTL_STANDARD             0x40
#define XAL_RC_BAR_CNTL_STANDARD_IOBAR_EN    BIT(0)
#define XAL_RC_BAR_CNTL_STANDARD_32B_BAR_EN  BIT(1)
#define XAL_RC_BAR_CNTL_STANDARD_64B_BAR_EN  BIT(2)

/* XTL registers */
#define XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS             0x58
#define XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS_DLL_ACTIVE  BIT(29)

#define XTL_PRI_OFFSET  0x1000

#define XTL_RC_MGMT_PERST_CONTROL            0x218
#define XTL_RC_MGMT_PERST_CONTROL_PERST_O_N  BIT(0)

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
