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
#include <Library/UefiRuntimeServicesTableLib.h>
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

  PCI_REG_PCIE_DEVICE_CAPABILITY  DeviceCap;

  CfgBase = MmcfgBase + PCI_ECAM_ADDRESS (Bus, Dev, Func, 0);
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
  DeviceCap.Uint32 = MmioRead32 (CfgBase + PcieOff + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability));
  *MaxPayload      = MIN (*MaxPayload, DeviceCap.Bits.MaxPayloadSize);

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
      CfgBaseLocal = MmcfgBase + PCI_ECAM_ADDRESS (SecBus, PcieDev, PcieFunc, 0);
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

  //
  // Set the subordinate bus for this device to the next bus to continue enumeration
  //
  MmioWrite8 (CfgBase + PCI_BRIDGE_SUBORDINATE_BUS_REGISTER_OFFSET, *NextBus);

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

  PCI_REG_PCIE_DEVICE_CONTROL  DeviceControl;

  CfgBase = MmcfgBase + PCI_ECAM_ADDRESS (Bus, Dev, Func, 0);
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
  DeviceControl.Uint16 = MmioRead16 (CfgBase + PcieOff + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl));
  // Update maxpayload field bit 5-7
  DeviceControl.Bits.MaxPayloadSize = MaxPayload;
  // Set MRRS to 5 bit 14-12
  DeviceControl.Bits.MaxReadRequestSize = PCIE_MAX_READ_REQ_SIZE_4096B;
  // Write back
  MmioWrite16 (CfgBase + PcieOff + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl), DeviceControl.Uint16);
  DeviceControl.Uint16 = MmioRead16 (CfgBase + PcieOff + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl));

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
      CfgBaseLocal = MmcfgBase + PCI_ECAM_ADDRESS (SecBus, PcieDev, PcieFunc, 0);
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

  CfgBase = MmcfgBase + PCI_ECAM_ADDRESS (Bus, Dev, Func, 0);
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
      CfgBaseLocal = MmcfgBase + PCI_ECAM_ADDRESS (SecBus, PcieDev, PcieFunc, 0);
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

  CfgBase = MmcfgBase + PCI_ECAM_ADDRESS (Bus, Dev, Func, 0);
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
      CfgBaseLocal = MmcfgBase + PCI_ECAM_ADDRESS (SecBus, PcieDev, PcieFunc, 0);
      // Read DIDVID
      Data32 = MmioRead32 (CfgBaseLocal);
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
  UINT8                                             MaxPayload = PCIE_MAX_PAYLOAD_SIZE_4096B;
  UINT8                                             NextBus = 0;
  UINT32                                            Socket, Ctrl;
  UINTN                                             BufferSize;
  UINT32                                            *MaxPayloadSize;

  Status = gBS->HandleProtocol (RootBridgeHandle, &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid, (VOID **)&RootBridgeCfgIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting RootBridgeCfgIo protocol: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  // root port config base
  CfgBase = RootBridgeCfgIo->EcamBase + PCI_ECAM_ADDRESS (0, 0, 0, 0);
  PciTreeTraverseDumpBus (CfgBase, 0, 0, 0);
  PciTreeTraverseResetBus (CfgBase, 0, 0, 0);
  PciTreeTraverseDumpBus (CfgBase, 0, 0, 0);
  PciTreeTraverseGetMaxpayload (CfgBase, 0, 0, 0, &MaxPayload, &NextBus);

  //
  // Get pcie mps setup variable size.
  //
  BufferSize = 0;
  Status     = gRT->GetVariable (
                      L"PcieMaxPayloadSize",
                      &gNVIDIAPublicVariableGuid,
                      NULL,
                      &BufferSize,
                      NULL
                      );

  if (Status == EFI_BUFFER_TOO_SMALL) {
    MaxPayloadSize = (UINT32 *)AllocateZeroPool (BufferSize);
    if (MaxPayloadSize != NULL) {
      //
      // Get pcie mps setup variable.
      //
      Status = gRT->GetVariable (
                      L"PcieMaxPayloadSize",
                      &gNVIDIAPublicVariableGuid,
                      NULL,
                      &BufferSize,
                      MaxPayloadSize
                      );

      if (!EFI_ERROR (Status)) {
        Socket = RootBridgeCfgIo->SocketID;
        Ctrl   = RootBridgeCfgIo->ControllerID;
        //
        // According to setup variable to map actual MaxPayload setting.
        //
        if (Socket < PcdGet32 (PcdTegraMaxSockets)) {
          switch ((MaxPayloadSize[Socket] >> (Ctrl * 3)) & 7ULL) {
            case 1:
              MaxPayload = (MaxPayload > PCIE_MAX_PAYLOAD_SIZE_128B) ? PCIE_MAX_PAYLOAD_SIZE_128B : MaxPayload;
              break;
            case 2:
              MaxPayload = (MaxPayload > PCIE_MAX_PAYLOAD_SIZE_256B) ? PCIE_MAX_PAYLOAD_SIZE_256B : MaxPayload;
              break;
            case 3:
              MaxPayload = (MaxPayload > PCIE_MAX_PAYLOAD_SIZE_512B) ? PCIE_MAX_PAYLOAD_SIZE_512B : MaxPayload;
              break;
            case 4:
              MaxPayload = (MaxPayload > PCIE_MAX_PAYLOAD_SIZE_1024B) ? PCIE_MAX_PAYLOAD_SIZE_1024B : MaxPayload;
              break;
            case 5:
              MaxPayload = (MaxPayload > PCIE_MAX_PAYLOAD_SIZE_2048B) ? PCIE_MAX_PAYLOAD_SIZE_2048B : MaxPayload;
              break;
            case 6:
              MaxPayload = (MaxPayload > PCIE_MAX_PAYLOAD_SIZE_4096B) ? PCIE_MAX_PAYLOAD_SIZE_4096B : MaxPayload;
              break;
            default:
            case 0:
              //
              // The default of MaxPayloadSize HII setting is Auto, which means
              // the DevCtrl.Mps will be set to NVIDIA recommended configuration.
              //
              break;
          }
        }
      }

      FreePool (MaxPayloadSize);
    }
  }

  PciTreeTraverseSetMaxpayload (CfgBase, 0, 0, 0, MaxPayload);
  PciTreeTraverseResetBus (CfgBase, 0, 0, 0);
  PciTreeTraverseDumpBus (CfgBase, 0, 0, 0);

  return (Status);
}
