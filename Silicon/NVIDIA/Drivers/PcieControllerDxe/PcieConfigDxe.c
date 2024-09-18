/** @file

  PCIe Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciIo.h>

UINT8
PCIeFindCap (
  UINT64  CfgBase,
  UINT8   cap
  );

/**
  Walk root port and downstream devices for min of max payload size settings

  @param[in] MmcfgBase - MMCFG base address of the root port
  @param[in] Bus       - The bus number of this device
  @param[in] Dev       - The device number of this device
  @param[in] Func      - The function number of this device
  @param[in,out] *MaxPayload  - This records the min maxpayload value of the topology
  @param[in,out] *NextBus     - Next bus used by this topology starting from the root port
**/
VOID
PciTreeTraverseGetMaxpayload (
  UINT64  MmcfgBase,
  UINT8   Bus,
  UINT8   Dev,
  UINT8   Func,
  UINT8   *MaxPayload,
  UINT8   *NextBus
  )
{
  UINT64   CfgBase;
  UINT32   Data32;
  UINT32   PcieOff = 0;
  UINT8    PcieDev;
  UINT8    PcieFunc;
  UINT8    SecBus;
  BOOLEAN  DeviceFound = FALSE;

  CfgBase = MmcfgBase + ((Bus << 20)|(Dev << 15)|(Func << 12));
  // Read DIDVID
  Data32 =  MmioRead32 (CfgBase);
  // Device not present, return
  if ((Data32 == 0) || (Data32 == 0xFFFFFFFF)) {
    return;
  }

  // read root port maxpayload capability and use as a base
  PcieOff = PCIeFindCap (CfgBase, EFI_PCI_CAPABILITY_ID_PCIEXP);
  // Not a PCIe device, return
  if (!PcieOff) {
    return;
  }

  // Read device capability register
  Data32      = MmioRead32 (CfgBase + PcieOff + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability));
  *MaxPayload = MIN (*MaxPayload, Data32 & 7);

  // Is this bridge device?
  if (!(MmioRead8 (CfgBase + PCI_HEADER_TYPE_OFFSET) & 0x7F)) {
    return;
  }

  // Assign secondary and subordinate bus numbers to start enumeration at the secondary bus.
  *NextBus = *NextBus+1;
  MmioWrite8 (CfgBase + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, Bus);
  MmioWrite8 (CfgBase + PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET, *NextBus);
  MmioWrite8 (CfgBase + PCI_BRIDGE_SUBORDINATE_BUS_REGISTER_OFFSET, 0xFF);
  SecBus = *NextBus;

  for (PcieDev = 0; PcieDev <= PCI_MAX_DEVICE; PcieDev++) {
    for (PcieFunc = 0; PcieFunc <= PCI_MAX_FUNC; PcieFunc++) {
      UINT64  CfgBaseLocal;
      CfgBaseLocal = MmcfgBase + ((SecBus << 20)|(PcieDev << 15)|(PcieFunc << 12));
      // Read DIDVID
      Data32 =  MmioRead32 (CfgBaseLocal);
      // Device not present, return
      if ((Data32 == 0) || (Data32 == 0xFFFFFFFF)) {
        if (PcieFunc != 0) {
          continue; // go to next function
        } else {
          break;    // if no function 0 then go to next device
        }
      }

      DeviceFound = TRUE;
      PciTreeTraverseGetMaxpayload (MmcfgBase, SecBus, PcieDev, PcieFunc, MaxPayload, NextBus);

      // Not multi function device, go to next dev
      if ((PcieFunc == 0) && !(MmioRead8 (CfgBaseLocal + PCI_HEADER_TYPE_OFFSET) & HEADER_TYPE_MULTI_FUNCTION)) {
        break;
      }
    }
  }

  // No device behind the bridge
  if (!DeviceFound) {
    *NextBus = *NextBus - 1;
    MmioWrite8 (CfgBase + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, 0);
    MmioWrite8 (CfgBase + PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET, 0);
    MmioWrite8 (CfgBase + PCI_BRIDGE_SUBORDINATE_BUS_REGISTER_OFFSET, 0);
  } else {
    // Update the sub-ordinate bus used so as to set maxpayload value in next recursive call
    MmioWrite8 (CfgBase + PCI_BRIDGE_SUBORDINATE_BUS_REGISTER_OFFSET, *NextBus);
  }

  return;
}

/**
  Walk root port and downstream devices for set min of max payload size settings. This routine also set MRRS to 4k as default

  @param[in] MmcfgBase - MMCFG base address of the root port
  @param[in] Bus       - The bus number of this device
  @param[in] Dev       - The device number of this device
  @param[in] Func      - The function number of this device
  @param[in] MaxPayload  - This records the min maxpayload value of the topology under a root port
**/
VOID
PciTreeTraverseSetMaxpayload (
  UINT64  MmcfgBase,
  UINT8   Bus,
  UINT8   Dev,
  UINT8   Func,
  UINT8   MaxPayload
  )
{
  UINT8   SecBus;
  UINT32  Data32;
  UINT64  CfgBase;
  UINT32  PcieOff;
  UINT8   PcieDev;
  UINT8   PcieFunc;
  UINT32  Data16;

  CfgBase = MmcfgBase + ((Bus << 20)|(Dev << 15)|(Func << 12));
  // Read DIDVID
  Data32 =  MmioRead32 (CfgBase);
  // Device not present, return
  if ((Data32 == 0) || (Data32 == 0xFFFFFFFF)) {
    return;
  }

  // read root port maxpayload capability and use as a base
  PcieOff = PCIeFindCap (CfgBase, EFI_PCI_CAPABILITY_ID_PCIEXP);
  // Not a PCIe device, return
  if (!PcieOff) {
    return;
  }

  // Read device control register
  Data16 = MmioRead16 (CfgBase + PcieOff + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl));
  // Update maxpayload field bit 5-7
  Data16 &= ~0xE0;
  Data16 |= MaxPayload << 5;
  // Set MRRS to 5 bit 14-12
  Data16 &= ~0x7000;
  Data16 |= 5 << 12;
  // Write back
  MmioWrite16 (CfgBase + PcieOff + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl), Data16);
  Data16 = MmioRead16 (CfgBase + PcieOff + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl));

  // Is this bridge device?
  if (!(MmioRead8 (CfgBase + PCI_HEADER_TYPE_OFFSET) & 0x7F)) {
    return;
  }

  SecBus = MmioRead8 (CfgBase+PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET);
  if (!((SecBus > 0) && (SecBus < 0xFF))) {
    return;
  }

  for (PcieDev = 0; PcieDev <= PCI_MAX_DEVICE; PcieDev++) {
    for (PcieFunc = 0; PcieFunc <= PCI_MAX_FUNC; PcieFunc++) {
      UINT64  CfgBaseLocal;
      CfgBaseLocal = MmcfgBase + (((SecBus) << 20)|(PcieDev << 15)|(PcieFunc << 12));
      // Read DIDVID
      Data32 =  MmioRead32 (CfgBaseLocal);
      // Device not present, return
      if ((Data32 == 0) || (Data32 == 0xFFFFFFFF)) {
        if (PcieFunc != 0) {
          continue; // go to next function
        } else {
          break;    // if no function 0 then go to next device
        }
      }

      PciTreeTraverseSetMaxpayload (MmcfgBase, SecBus, PcieDev, PcieFunc, MaxPayload);

      // Not multi function device, go to next dev
      if ((PcieFunc == 0) && !(MmioRead8 (CfgBaseLocal + PCI_HEADER_TYPE_OFFSET) & HEADER_TYPE_MULTI_FUNCTION)) {
        break;
      }
    }
  }

  return;
}

/**
  Walk root port and downstream devices to clear all temp bus assigned to a bridge

  @param[in] MmcfgBase - MMCFG base address of the root port
  @param[in] Bus       - The bus number of this device
  @param[in] Dev       - The device number of this device
  @param[in] Func      - The function number of this device
**/
VOID
PciTreeTraverseResetBus (
  UINT64  MmcfgBase,
  UINT8   Bus,
  UINT8   Dev,
  UINT8   Func
  )
{
  UINT8   SecBus;
  UINT32  Data32;
  UINT64  CfgBase;
  UINT8   PcieDev;
  UINT8   PcieFunc;

  CfgBase = MmcfgBase + ((Bus << 20)|(Dev << 15)|(Func << 12));
  // Read DIDVID
  Data32 =  MmioRead32 (CfgBase);
  // Device not present, return
  if ((Data32 == 0) || (Data32 == 0xFFFFFFFF)) {
    return;
  }

  // Is this bridge device?
  if (!(MmioRead8 (CfgBase + PCI_HEADER_TYPE_OFFSET) & 0x7F)) {
    return;
  }

  SecBus = MmioRead8 (CfgBase+PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET);
  if (!((SecBus > 0) && (SecBus < 0xFF))) {
    return;
  }

  for (PcieDev = 0; PcieDev <= PCI_MAX_DEVICE; PcieDev++) {
    for (PcieFunc = 0; PcieFunc <= PCI_MAX_FUNC; PcieFunc++) {
      UINT64  CfgBaseLocal;
      CfgBaseLocal = MmcfgBase + (((SecBus) << 20)|(PcieDev << 15)|(PcieFunc << 12));
      // Read DIDVID
      Data32 =  MmioRead32 (CfgBaseLocal);
      // Device not present, return
      if ((Data32 == 0) || (Data32 == 0xFFFFFFFF)) {
        if (PcieFunc != 0) {
          continue; // go to next function
        } else {
          break;    // if no function 0 then go to next device
        }
      }

      PciTreeTraverseResetBus (MmcfgBase, SecBus, PcieDev, PcieFunc);

      // Not multi function device, go to next dev
      if ((PcieFunc == 0) && !(MmioRead8 (CfgBaseLocal + PCI_HEADER_TYPE_OFFSET) & HEADER_TYPE_MULTI_FUNCTION)) {
        break;
      }
    }
  }

  MmioWrite8 (CfgBase + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, 0);
  MmioWrite8 (CfgBase + PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET, 0);
  MmioWrite8 (CfgBase + PCI_BRIDGE_SUBORDINATE_BUS_REGISTER_OFFSET, 0);

  return;
}

/**
  Walk root port and downstream devices to dump all temp bus assigned to bridges

  @param[in] MmcfgBase - MMCFG base address of the root port
  @param[in] Bus       - The bus number of this device
  @param[in] Dev       - The device number of this device
  @param[in] Func      - The function number of this device
**/
VOID
PciTreeTraverseDumpBus (
  UINT64  MmcfgBase,
  UINT8   Bus,
  UINT8   Dev,
  UINT8   Func
  )
{
  UINT8   SecBus;
  UINT32  Data32;
  UINT64  CfgBase;
  UINT8   PcieDev;
  UINT8   PcieFunc;

  CfgBase = MmcfgBase + ((Bus << 20)|(Dev << 15)|(Func << 12));
  // Read DIDVID
  Data32 =  MmioRead32 (CfgBase);
  // Device not present, return
  if ((Data32 == 0) || (Data32 == 0xFFFFFFFF)) {
    return;
  }

  // Is this bridge device?
  if (!(MmioRead8 (CfgBase + PCI_HEADER_TYPE_OFFSET) & 0x7F)) {
    return;
  }

  SecBus = MmioRead8 (CfgBase+PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET);
  if (!((SecBus > 0) && (SecBus < 0xFF))) {
    return;
  }

  for (PcieDev = 0; PcieDev <= PCI_MAX_DEVICE; PcieDev++) {
    for (PcieFunc = 0; PcieFunc <= PCI_MAX_FUNC; PcieFunc++) {
      UINT64  CfgBaseLocal;
      CfgBaseLocal = MmcfgBase + (((SecBus) << 20)|(PcieDev << 15)|(PcieFunc << 12));
      // Read DIDVID
      Data32 =  MmioRead32 (CfgBaseLocal);
      // Device not present, return
      if ((Data32 == 0) || (Data32 == 0xFFFFFFFF)) {
        if (PcieFunc != 0) {
          continue; // go to next function
        } else {
          break;    // if no function 0 then go to next device
        }
      }

      PciTreeTraverseDumpBus (MmcfgBase, SecBus, PcieDev, PcieFunc);

      // Not multi function device, go to next dev
      if ((PcieFunc == 0) && !(MmioRead8 (CfgBaseLocal + PCI_HEADER_TYPE_OFFSET) & HEADER_TYPE_MULTI_FUNCTION)) {
        break;
      }
    }
  }

  DEBUG ((DEBUG_INFO, "%a: Bus:Dev:Func %02X:%02X:%02X Dump Bus - ", __FUNCTION__, Bus, Dev, Func));
  DEBUG ((DEBUG_INFO, "pribus:secbus:subbus=0x%02X:0x%02X:0x%02X\n", MmioRead8 (CfgBase + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET), MmioRead8 (CfgBase + PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET), MmioRead8 (CfgBase + PCI_BRIDGE_SUBORDINATE_BUS_REGISTER_OFFSET)));

  return;
}

/**
  This routine will config maxpayload and mrrs capability, expandable to add other settings

  @param[in]  RootBridges           PCIE Root bridge Instance handle
**/
EFI_STATUS
RootPortConfigPcieCapability (
  EFI_HANDLE  RootBridgeHandle
  )
{
  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *RootBridgeCfgIo = NULL;
  EFI_STATUS                                        Status           = EFI_SUCCESS;
  UINT64                                            CfgBase;
  UINT8                                             MaxPayload = 5;
  UINT8                                             NextBus    = 0;

  Status = gBS->HandleProtocol (RootBridgeHandle, &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid, (VOID **)&RootBridgeCfgIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting RootBridgeCfgIo protocol: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  // root port config base
  CfgBase = RootBridgeCfgIo->EcamBase + ((0 << 20) | (0 << 15) | (0 << 12));
  PciTreeTraverseDumpBus (CfgBase, 0, 0, 0);
  PciTreeTraverseResetBus (CfgBase, 0, 0, 0);
  PciTreeTraverseDumpBus (CfgBase, 0, 0, 0);
  PciTreeTraverseGetMaxpayload (CfgBase, 0, 0, 0, &MaxPayload, &NextBus);
  PciTreeTraverseSetMaxpayload (CfgBase, 0, 0, 0, MaxPayload);
  PciTreeTraverseResetBus (CfgBase, 0, 0, 0);
  PciTreeTraverseDumpBus (CfgBase, 0, 0, 0);
  return (Status);
}
