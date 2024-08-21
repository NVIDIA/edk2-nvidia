/** @file

  PCIe Generic Controller Driver private structures

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_GENERIC_CONTROLLER_PRIVATE_H__
#define __PCIE_GENERIC_CONTROLLER_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#define BIT(x)  (1 << (x))

#define upper_32_bits(n)  ((UINT32)((n) >> 32))
#define lower_32_bits(n)  ((UINT32)(n))

#define PCIE_NUMBER_OF_MAPPING_SPACE  3
#define PCIE_NUMBER_OF_INTERRUPT_MAP  4
#define PCIE_REPO_OBJECTS             (3 + PCIE_NUMBER_OF_MAPPING_SPACE + PCIE_NUMBER_OF_INTERRUPT_MAP) // 2 Reference Arrays, Mappings, End of list
#define PCIE_COMMON_REPO_OBJECTS      (3)                                                               // Config Space, Acpi Tables, end of list

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
  CM_ARM_PCI_INTERRUPT_MAP_INFO                       InterruptMapInfo[PCIE_NUMBER_OF_INTERRUPT_MAP];
  CM_ARM_OBJ_REF                                      InterruptRefInfo[PCIE_NUMBER_OF_INTERRUPT_MAP];
  EDKII_PLATFORM_REPOSITORY_INFO                      RepoInfo[PCIE_REPO_OBJECTS];
} PCIE_CONTROLLER_PRIVATE;
#define PCIE_CONTROLLER_PRIVATE_DATA_FROM_THIS(a)  CR(a, PCIE_CONTROLLER_PRIVATE, PcieRootBridgeConfigurationIo, PCIE_CONTROLLER_SIGNATURE)

#define PCIE_DEVICETREE_PREFETCHABLE  BIT30
#define PCIE_DEVICETREE_SPACE_CODE    (BIT24 | BIT25)
#define PCIE                          DEVICETREE_SPACE_CONF   0
#define PCIE_DEVICETREE_SPACE_IO      BIT24
#define PCIE_DEVICETREE_SPACE_MEM32   BIT25
#define PCIE_DEVICETREE_SPACE_MEM64   (BIT24 | BIT25)

#endif
