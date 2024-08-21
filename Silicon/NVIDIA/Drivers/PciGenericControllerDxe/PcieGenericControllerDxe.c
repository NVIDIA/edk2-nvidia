/** @file

  PCIe Generic Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <libfdt.h>
#include <PiDxe.h>

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/SortLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/ConfigurationManagerTokenProtocol.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciIo.h>

#include "PcieGenericControllerPrivate.h"

#define PCIE_CONTROLLER_MAX_REGISTERS  6

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "pci-host-ecam-generic", &gNVIDIANonDiscoverableGenericPcieDeviceGuid },
  { NULL,                    NULL                                         }
};

STATIC ACPI_HID_DEVICE_PATH  mPciRootBridgeDevicePathNode = {
  {
    ACPI_DEVICE_PATH,
    ACPI_DP,
    {
      (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
      (UINT8)((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
    }
  },
  EISA_PNP_ID (0x0A03), // PCI
  0
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA Generic Pcie controller driver",
  .AutoEnableClocks                = TRUE,
  .AutoDeassertReset               = TRUE,
  .AutoDeassertPg                  = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .ThreadedDeviceStart             = FALSE
};

/**
  PCI configuration space access.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Read     TRUE indicating it's a read operation.
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Buffer   The destination buffer to store the results.

  @retval EFI_SUCCESS            The data was read/written from/to the PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Invalid parameters found.
**/
STATIC
EFI_STATUS
EFIAPI
PcieConfigurationAccess (
  IN     NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *This,
  IN     BOOLEAN                                           Read,
  IN     NVIDIA_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH          Width,
  IN     UINT64                                            Address,
  IN OUT VOID                                              *Buffer
  )
{
  EFI_STATUS                                   Status;
  PCIE_CONTROLLER_PRIVATE                      *Private;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS  PciAddress;
  UINT64                                       ConfigAddress;
  UINT32                                       Register;
  UINT8                                        Length;
  UINT64                                       offset;

  //
  // Read Pci configuration space
  //
  Private = PCIE_CONTROLLER_PRIVATE_DATA_FROM_THIS (This);
  CopyMem (&PciAddress, &Address, sizeof (PciAddress));

  if (PciAddress.ExtendedRegister == 0) {
    Register = PciAddress.Register;
  } else {
    Register = PciAddress.ExtendedRegister;
  }

  Length = 1 << Width;

  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "buffer is null\n"));
  }

  if ((UINT32)Width >= NvidiaPciWidthMaximum) {
    DEBUG ((DEBUG_ERROR, "Width = %d\n", Width));
  }

  if (Register >= SIZE_4KB) {
    DEBUG ((DEBUG_ERROR, "Register = %u\n", Register));
  }

  if (Register + Length > SIZE_4KB) {
    DEBUG ((DEBUG_ERROR, "Register = %u, Length = %u\n", Register, Length));
  }

  //
  // Check to see if Buffer is NULL
  // Check to see if Width is in the valid range
  // Check if Register is in correct space
  //
  if ((Buffer == NULL) ||
      ((UINT32)Width >= NvidiaPciWidthMaximum) ||
      (Register >= SIZE_4KB) ||
      (Register + Length > SIZE_4KB))
  {
    Status = EFI_INVALID_PARAMETER;
  } else {
    if (((PciAddress.Bus == This->MinBusNumber) ||
         (PciAddress.Bus == This->MinBusNumber + 1)) &&
        (PciAddress.Device != 0))
    {
      if (Read) {
        SetMem (Buffer, Length, 0xFF);
        Status = EFI_SUCCESS;
      } else {
        Status = EFI_SUCCESS;
      }
    } else {
      offset = (PciAddress.Bus) << 20 |
               (PciAddress.Device) << 15 |
               (PciAddress.Function) << 12;
      ConfigAddress = Private->EcamBase + offset;

      if (Read) {
        if (Width == NvidiaPciWidthUint8) {
          *(UINT8 *)Buffer = MmioRead8 (ConfigAddress + Register);
        } else if (Width == NvidiaPciWidthUint16) {
          *(UINT16 *)Buffer = MmioRead16 (ConfigAddress + Register);
        } else if (Width == NvidiaPciWidthUint32) {
          *(UINT32 *)Buffer = MmioRead32 (ConfigAddress + Register);
        } else {
          // No valid way to get here
          ASSERT (Width < NvidiaPciWidthMaximum);
        }
      } else {
        if (Width == NvidiaPciWidthUint8) {
          UINT32  Data = MmioRead32 (ConfigAddress + (Register & ~0x3));
          CopyMem (((VOID *)&Data) + (Register & 0x3), Buffer, 1);
          MmioWrite32 (ConfigAddress + (Register & ~0x3), Data);
        } else if (Width == NvidiaPciWidthUint16) {
          UINT32  Data = MmioRead32 (ConfigAddress + (Register & ~0x3));
          CopyMem (((VOID *)&Data) + (Register & 0x3), Buffer, 2);
          MmioWrite32 (ConfigAddress + (Register & ~0x3), Data);
        } else if (Width == NvidiaPciWidthUint32) {
          MmioWrite32 (ConfigAddress + Register, *(UINT32 *)Buffer);
        } else {
          // No valid way to get here
          ASSERT (Width < NvidiaPciWidthMaximum);
        }
      }

      Status = EFI_SUCCESS;
    }
  }

  return Status;
}

/**
  Allows read from PCI configuration space.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Buffer   The destination buffer to store the results.

  @retval EFI_SUCCESS           The data was read from the PCI root bridge.
  @retval EFI_INVALID_PARAMETER Invalid parameters found.
**/
STATIC
EFI_STATUS
EFIAPI
PcieConfigurationRead (
  IN     NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *This,
  IN     NVIDIA_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH          Width,
  IN     UINT64                                            Address,
  IN OUT VOID                                              *Buffer
  )
{
  return PcieConfigurationAccess (This, TRUE, Width, Address, Buffer);
}

/**
  Allows write to PCI configuration space.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Buffer   The source buffer to get the results.

  @retval EFI_SUCCESS            The data was written to the PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Invalid parameters found.
**/
STATIC
EFI_STATUS
EFIAPI
PcieConfigurationWrite (
  IN     NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *This,
  IN     NVIDIA_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH          Width,
  IN     UINT64                                            Address,
  IN OUT VOID                                              *Buffer
  )
{
  return PcieConfigurationAccess (This, FALSE, Width, Address, Buffer);
}

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                                   Status;
  PCI_ROOT_BRIDGE                              *RootBridge       = NULL;
  EFI_DEVICE_PATH_PROTOCOL                     *ParentDevicePath = NULL;
  CONST VOID                                   *BusProperty      = NULL;
  CONST VOID                                   *RangesProperty   = NULL;
  UINT32                                       PropertySize      = 0;
  INT32                                        AddressCells;
  INT32                                        PciAddressCells;
  INT32                                        SizeCells;
  INT32                                        RangeSize;
  PCIE_CONTROLLER_PRIVATE                      *Private = NULL;
  UINT32                                       Index;
  UINT32                                       Index2;
  NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA        *InterruptMap;
  UINT32                                       NumberOfInterruptMaps;
  NVIDIA_DEVICE_TREE_REGISTER_DATA             RegisterData[PCIE_CONTROLLER_MAX_REGISTERS];
  UINT32                                       RegisterCount;
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  *CMTokenProtocol;
  CM_OBJECT_TOKEN                              *TokenMap;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      Status = gBS->LocateProtocol (&gNVIDIAConfigurationManagerTokenProtocolGuid, NULL, (VOID **)&CMTokenProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to fird ConfigurationManagerTokenProtocol\n", __FUNCTION__));
        break;
      }

      RootBridge = AllocateZeroPool (sizeof (PCI_ROOT_BRIDGE));
      if (RootBridge == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate device bridge structure\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }

      Private = AllocateZeroPool (sizeof (PCIE_CONTROLLER_PRIVATE));
      if (Private == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate private structure\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }

      if (DeviceTreeNode == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: bad DeviceTreeNode\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      RegisterCount = ARRAY_SIZE (RegisterData);
      Status        = DeviceTreeGetRegisters (DeviceTreeNode->NodeOffset, RegisterData, &RegisterCount);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: GetRegisters failed: %r\n", __FUNCTION__, Status));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Private->EcamBase = RegisterData[0].BaseAddress;
      Private->EcamSize = RegisterData[0].Size;

      Private->Signature                                   = PCIE_CONTROLLER_SIGNATURE;
      Private->PcieRootBridgeConfigurationIo.Read          = PcieConfigurationRead;
      Private->PcieRootBridgeConfigurationIo.Write         = PcieConfigurationWrite;
      Private->PcieRootBridgeConfigurationIo.SegmentNumber = 0;

      Status = DeviceTreeGetNodePropertyValue32 (
                 DeviceTreeNode->NodeOffset,
                 "linux,pci-domain",
                 &Private->PcieRootBridgeConfigurationIo.SegmentNumber
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to read segment number\n"));
        break;
      }

      DEBUG ((DEBUG_INFO, "Segment Number = 0x%x\n", Private->PcieRootBridgeConfigurationIo.SegmentNumber));

      RootBridge->Segment               = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
      RootBridge->Supports              = 0;
      RootBridge->Attributes            = 0;
      RootBridge->DmaAbove4G            = TRUE;
      RootBridge->NoExtendedConfigSpace = FALSE;
      RootBridge->ResourceAssigned      = FALSE;
      RootBridge->AllocationAttributes  = EFI_PCI_HOST_BRIDGE_MEM64_DECODE;

      Status = DeviceTreeGetNodeProperty (
                 DeviceTreeNode->NodeOffset,
                 "bus-range",
                 &BusProperty,
                 &PropertySize
                 );
      if (EFI_ERROR (Status) || (PropertySize != (2 * sizeof (UINT32)))) {
        DEBUG ((DEBUG_INFO, "PCIe Controller: unknown bus size in fdt, default to 0-255\r\n"));
        RootBridge->Bus.Base  = 0x0;
        RootBridge->Bus.Limit = 0xff;
      } else {
        CopyMem (&RootBridge->Bus.Base, BusProperty, sizeof (UINT32));
        RootBridge->Bus.Base = SwapBytes32 (RootBridge->Bus.Base);
        CopyMem (&RootBridge->Bus.Limit, BusProperty + sizeof (UINT32), sizeof (UINT32));
        RootBridge->Bus.Limit = SwapBytes32 (RootBridge->Bus.Limit);
      }

      Private->PcieRootBridgeConfigurationIo.MinBusNumber = RootBridge->Bus.Base;
      Private->PcieRootBridgeConfigurationIo.MaxBusNumber = RootBridge->Bus.Limit;

      AddressCells    = fdt_address_cells (DeviceTreeNode->DeviceTreeBase, fdt_parent_offset (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset));
      PciAddressCells = fdt_address_cells (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset);
      SizeCells       = fdt_size_cells (DeviceTreeNode->DeviceTreeBase, fdt_parent_offset (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset));
      RangeSize       = (AddressCells + PciAddressCells + SizeCells) * sizeof (UINT32);

      if (PciAddressCells != 3) {
        DEBUG ((DEBUG_ERROR, "PCIe Controller, size 3 is required for address-cells, got %d\r\n", PciAddressCells));
        Status = EFI_DEVICE_ERROR;
        break;
      }

      Status = DeviceTreeGetNodeProperty (
                 DeviceTreeNode->NodeOffset,
                 "ranges",
                 &RangesProperty,
                 &PropertySize
                 );
      // Mark all regions as unsupported
      RootBridge->Io.Base          = MAX_UINT64;
      RootBridge->Mem.Base         = MAX_UINT64;
      RootBridge->MemAbove4G.Base  = MAX_UINT64;
      RootBridge->PMem.Base        = MAX_UINT64;
      RootBridge->PMemAbove4G.Base = MAX_UINT64;

      if ((EFI_ERROR (Status)) || ((PropertySize % RangeSize) != 0)) {
        DEBUG ((DEBUG_ERROR, "PCIe Controller: Unsupported ranges configuration\r\n"));
        Status = EFI_UNSUPPORTED;
        break;
      }

      while (PropertySize != 0) {
        UINT32   Flags         = 0;
        UINT32   Space         = 0;
        BOOLEAN  Prefetchable  = FALSE;
        UINT64   DeviceAddress = 0;
        UINT64   HostAddress   = 0;
        UINT64   Limit         = 0;
        UINT64   Size          = 0;
        UINT64   Translation   = 0;

        ASSERT (Private->AddressMapCount < PCIE_NUMBER_OF_MAPPING_SPACE);

        CopyMem (&Flags, RangesProperty, sizeof (UINT32));
        Flags = SwapBytes32 (Flags);

        CopyMem (&DeviceAddress, RangesProperty + sizeof (UINT32), sizeof (UINT64));
        DeviceAddress = SwapBytes64 (DeviceAddress);

        if (AddressCells == 2) {
          CopyMem (&HostAddress, RangesProperty + (PciAddressCells * sizeof (UINT32)), sizeof (UINT64));
          HostAddress = SwapBytes64 (HostAddress);
        } else if (AddressCells == 1) {
          CopyMem (&HostAddress, RangesProperty + (PciAddressCells * sizeof (UINT32)), sizeof (UINT32));
          HostAddress = SwapBytes32 (HostAddress);
        } else {
          DEBUG ((DEBUG_ERROR, "PCIe Controller: Invalid address cells (%d)\r\n", AddressCells));
          Status = EFI_DEVICE_ERROR;
          break;
        }

        if (SizeCells == 2) {
          CopyMem (&Size, RangesProperty + ((PciAddressCells + AddressCells) * sizeof (UINT32)), sizeof (UINT64));
          Size = SwapBytes64 (Size);
        } else if (SizeCells == 1) {
          CopyMem (&Size, RangesProperty + ((PciAddressCells + AddressCells) * sizeof (UINT32)), sizeof (UINT32));
          Size = SwapBytes32 (Size);
        } else {
          DEBUG ((DEBUG_ERROR, "PCIe Controller: Invalid size cells (%d)\r\n", SizeCells));
          Status = EFI_DEVICE_ERROR;
          break;
        }

        Space        = Flags & PCIE_DEVICETREE_SPACE_CODE;
        Prefetchable = ((Flags & PCIE_DEVICETREE_PREFETCHABLE) == PCIE_DEVICETREE_PREFETCHABLE);
        Limit        = DeviceAddress + Size - 1;
        Translation  = DeviceAddress - HostAddress;

        if (Space == PCIE_DEVICETREE_SPACE_IO) {
          ASSERT (RootBridge->Io.Base == MAX_UINT64);
          RootBridge->Io.Base                                         = DeviceAddress;
          RootBridge->Io.Limit                                        = Limit;
          RootBridge->Io.Translation                                  = Translation;
          Private->IoBase                                             = HostAddress;
          Private->IoLimit                                            = HostAddress + Size - 1;
          Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 1;
        } else if (Space == PCIE_DEVICETREE_SPACE_MEM64) {
          if (Prefetchable) {
            if (Translation) {
              DEBUG ((DEBUG_ERROR, "Non 1:1 mapping is NOT supported for Prefetchable aperture\n"));
              Status = EFI_DEVICE_ERROR;
              break;
            }

            RootBridge->PMemAbove4G.Base                                = DeviceAddress;
            RootBridge->PMemAbove4G.Limit                               = Limit;
            RootBridge->PMemAbove4G.Translation                         = Translation;
            Private->PrefetchMemBase                                    = HostAddress;
            Private->PrefetchMemLimit                                   = HostAddress + Size - 1;
            Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 3;
            DEBUG ((DEBUG_INFO, "PREF64: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", DeviceAddress, Limit, Translation));
          } else {
            if (Translation) {
              RootBridge->Mem.Base                                        = DeviceAddress;
              RootBridge->Mem.Limit                                       = Limit;
              RootBridge->Mem.Translation                                 = Translation;
              Private->MemBase                                            = HostAddress;
              Private->MemLimit                                           = HostAddress + Size - 1;
              Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 3;
              DEBUG ((DEBUG_INFO, "MEM64: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", DeviceAddress, Limit, Translation));
            } else {
              DEBUG ((DEBUG_ERROR, "1:1 mapping is NOT supported for Non-Prefetchable aperture\n"));
              Status = EFI_DEVICE_ERROR;
              break;
            }
          }
        } else if (Space == PCIE_DEVICETREE_SPACE_MEM32) {
          RootBridge->Mem.Base                                        = DeviceAddress;
          RootBridge->Mem.Limit                                       = Limit;
          RootBridge->Mem.Translation                                 = Translation;
          Private->MemBase                                            = HostAddress;
          Private->MemLimit                                           = HostAddress + Size - 1;
          Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 3;
          DEBUG ((DEBUG_INFO, "MEM32: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", DeviceAddress, Limit, Translation));
        } else {
          DEBUG ((DEBUG_ERROR, "PCIe Controller: Unknown region 0x%08x 0x%016llx-0x%016llx T 0x%016llx\r\n", Flags, DeviceAddress, Limit, Translation));
          ASSERT (FALSE);
          Status = EFI_DEVICE_ERROR;
          break;
        }

        Private->AddressMapInfo[Private->AddressMapCount].PciAddress  = DeviceAddress;
        Private->AddressMapInfo[Private->AddressMapCount].CpuAddress  = HostAddress;
        Private->AddressMapInfo[Private->AddressMapCount].AddressSize = Size;
        Private->AddressMapCount++;

        RangesProperty += RangeSize;
        PropertySize   -= RangeSize;
      }

      if (EFI_ERROR (Status)) {
        break;
      }

      if ((RootBridge->PMem.Base == MAX_UINT64) && (RootBridge->PMemAbove4G.Base == MAX_UINT64)) {
        RootBridge->AllocationAttributes |= EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
      }

      Private->BusMask = RootBridge->Bus.Limit;

      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&ParentDevicePath
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get device path (%r)\r\n", __FUNCTION__, Status));
        break;
      }

      mPciRootBridgeDevicePathNode.UID = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
      RootBridge->DevicePath           = AppendDevicePathNode (
                                           ParentDevicePath,
                                           (EFI_DEVICE_PATH_PROTOCOL  *)&mPciRootBridgeDevicePathNode
                                           );

      // Setup configuration structure
      Private->ConfigSpaceInfo.BaseAddress           = Private->EcamBase;
      Private->ConfigSpaceInfo.PciSegmentGroupNumber = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
      Private->ConfigSpaceInfo.StartBusNumber        = Private->PcieRootBridgeConfigurationIo.MinBusNumber;
      Private->ConfigSpaceInfo.EndBusNumber          = Private->PcieRootBridgeConfigurationIo.MaxBusNumber;
      Status                                         = CMTokenProtocol->AllocateTokens (CMTokenProtocol, 2, &TokenMap);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to allocate 2 tokens for the ConfigSpaceInfo token maps\n", __FUNCTION__));
        break;
      }

      Private->ConfigSpaceInfo.AddressMapToken   = TokenMap[0];
      Private->ConfigSpaceInfo.InterruptMapToken = TokenMap[1];
      FreePool (TokenMap);
      TokenMap = NULL;

      InterruptMap = AllocateZeroPool (PCIE_NUMBER_OF_INTERRUPT_MAP * sizeof (NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA));
      if (InterruptMap == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for %d interrupt maps\n", __FUNCTION__, PCIE_NUMBER_OF_INTERRUPT_MAP));
        return EFI_OUT_OF_RESOURCES;
      }

      NumberOfInterruptMaps = PCIE_NUMBER_OF_INTERRUPT_MAP;
      Status                = DeviceTreeGetInterruptMap (DeviceTreeNode->NodeOffset, InterruptMap, &NumberOfInterruptMaps);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get PCIE interrupt map\n", __FUNCTION__, Status));
        FreePool (InterruptMap);
        break;
      }

      Status = CMTokenProtocol->AllocateTokens (CMTokenProtocol, PCIE_NUMBER_OF_INTERRUPT_MAP, &TokenMap);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %d tokens for the InterruptMap token map\n", __FUNCTION__, PCIE_NUMBER_OF_INTERRUPT_MAP));
        FreePool (InterruptMap);
        break;
      }

      DEBUG ((DEBUG_VERBOSE, "%a: NumberOfInterruptMaps = %u\n", __FUNCTION__, NumberOfInterruptMaps));
      for (Index = 0; Index < PCIE_NUMBER_OF_INTERRUPT_MAP; Index++) {
        Private->InterruptRefInfo[Index].ReferenceToken = TokenMap[Index];
        if (NumberOfInterruptMaps == 1) {
          Private->InterruptMapInfo[Index].PciInterrupt = Index;
        } else {
          Private->InterruptMapInfo[Index].PciInterrupt = InterruptMap[Index].ChildInterrupt.Interrupt - 1;
        }

        Private->InterruptMapInfo[Index].IntcInterrupt.Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptMap[Index].ParentInterrupt);
        Private->InterruptMapInfo[Index].IntcInterrupt.Flags     = InterruptMap[Index].ParentInterrupt.Flag;
      }

      FreePool (InterruptMap);
      FreePool (TokenMap);
      TokenMap = NULL;

      Status = CMTokenProtocol->AllocateTokens (CMTokenProtocol, Private->AddressMapCount, &TokenMap);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u tokens for the AddressMap token map\n", __FUNCTION__, Private->AddressMapCount));
        break;
      }

      for (Index = 0; Index < Private->AddressMapCount; Index++) {
        Private->AddressMapRefInfo[Index].ReferenceToken = TokenMap[Index];
      }

      FreePool (TokenMap);
      TokenMap = NULL;

      Index                                  = 0;
      Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
      Private->RepoInfo[Index].CmObjectToken = Private->ConfigSpaceInfo.InterruptMapToken;
      Private->RepoInfo[Index].CmObjectSize  = sizeof (CM_ARM_OBJ_REF) * PCIE_NUMBER_OF_INTERRUPT_MAP;
      Private->RepoInfo[Index].CmObjectCount = PCIE_NUMBER_OF_INTERRUPT_MAP;
      Private->RepoInfo[Index].CmObjectPtr   = Private->InterruptRefInfo;
      Index++;

      Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
      Private->RepoInfo[Index].CmObjectToken = Private->ConfigSpaceInfo.AddressMapToken;
      Private->RepoInfo[Index].CmObjectSize  = sizeof (CM_ARM_OBJ_REF) * Private->AddressMapCount;
      Private->RepoInfo[Index].CmObjectCount = Private->AddressMapCount;
      Private->RepoInfo[Index].CmObjectPtr   = Private->AddressMapRefInfo;
      Index++;

      for (Index2 = 0; Index2 < PCIE_NUMBER_OF_MAPPING_SPACE; Index2++) {
        Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjPciAddressMapInfo);
        Private->RepoInfo[Index].CmObjectToken = Private->AddressMapRefInfo[Index2].ReferenceToken;
        Private->RepoInfo[Index].CmObjectSize  = sizeof (Private->AddressMapInfo[Index2]);
        Private->RepoInfo[Index].CmObjectCount = 1;
        Private->RepoInfo[Index].CmObjectPtr   = &Private->AddressMapInfo[Index2];
        Index++;
      }

      for (Index2 = 0; Index2 < PCIE_NUMBER_OF_INTERRUPT_MAP; Index2++) {
        Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjPciInterruptMapInfo);
        Private->RepoInfo[Index].CmObjectToken = Private->InterruptRefInfo[Index2].ReferenceToken;
        Private->RepoInfo[Index].CmObjectSize  = sizeof (Private->InterruptMapInfo[Index2]);
        Private->RepoInfo[Index].CmObjectCount = 1;
        Private->RepoInfo[Index].CmObjectPtr   = &Private->InterruptMapInfo[Index2];
        Index++;
      }

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gNVIDIAPciHostBridgeProtocolGuid,
                      RootBridge,
                      &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                      &Private->PcieRootBridgeConfigurationIo,
                      &gNVIDIAConfigurationManagerDataObjectGuid,
                      &Private->RepoInfo,
                      &gNVIDIAPciConfigurationDataProtocolGuid,
                      &Private->ConfigSpaceInfo,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to install root bridge info (%r)\r\n", __FUNCTION__, Status));
        break;
      }

      break;

    case DeviceDiscoveryEnumerationCompleted:
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &DriverHandle,
                      &gNVIDIAPcieGenericControllerInitCompleteProtocolGuid,
                      NULL,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to install PCI controller init complete protocol (%r)\r\n", __FUNCTION__, Status));
      }

    default:
      Status = EFI_SUCCESS;
      break;
  }

  if (EFI_ERROR (Status)) {
    if (RootBridge != NULL) {
      FreePool (RootBridge);
    }

    if (Private != NULL) {
      FreePool (Private);
    }
  }

  return Status;
}
