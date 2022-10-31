/** @file

  PCIe Controller Driver

  Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.

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
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciRootBridgeIo.h>

#include "PcieControllerPrivate.h"

STATIC BOOLEAN  mPcieAcpiConfigInstalled = FALSE;

/** The platform ACPI table list.
*/
STATIC
CM_STD_OBJ_ACPI_TABLE_INFO  CmAcpiTableList[] = {
  // MCFG Table
  {
    EFI_ACPI_6_3_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision)
  },
  // SSDT Table - PCIe
  {
    EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtPciExpress),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision)
  }
};

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,th500-pcie", &gNVIDIANonDiscoverableTH500PcieDeviceGuid },
  { NULL,                NULL                                       }
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
  .DriverName                      = L"NVIDIA Pcie controller driver",
  .UseDriverBinding                = FALSE,
  .AutoEnableClocks                = FALSE,
  .AutoDeassertReset               = FALSE,
  .AutoDeassertPg                  = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .DirectEnumerationSupport        = TRUE
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
    DEBUG ((DEBUG_ERROR, "Register = %d\n", Register));
  }

  if (Register + Length > SIZE_4KB) {
    DEBUG ((DEBUG_ERROR, "Register = %d, Length = %d\n", Register, Length));
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

STATIC
UINT8
PCIeFindNextCap (
  UINT64  CfgBase,
  UINT8   cap_ptr,
  UINT8   cap
  )
{
  UINT8   cap_id, next_cap_ptr;
  UINT16  reg;

  if (!cap_ptr) {
    return 0;
  }

  reg    = MmioRead16 (CfgBase + cap_ptr);
  cap_id = (reg & 0x00ff);

  if (cap_id > 0x14) {
    return 0;
  }

  if (cap_id == cap) {
    return cap_ptr;
  }

  next_cap_ptr = (reg & 0xff00) >> 8;

  return PCIeFindNextCap (CfgBase, next_cap_ptr, cap);
}

STATIC
UINT8
PCIeFindCap (
  UINT64  CfgBase,
  UINT8   cap
  )
{
  UINT8   next_cap_ptr;
  UINT16  reg;

  reg          = MmioRead16 (CfgBase + PCI_CAPBILITY_POINTER_OFFSET);
  next_cap_ptr = (reg & 0x00ff);

  return PCIeFindNextCap (CfgBase, next_cap_ptr, cap);
}

STATIC
EFI_STATUS
EFIAPI
InitFWFIntr (
  PCIE_CONTROLLER_PRIVATE  *Private
  )
{
  UINT32                 PCIeCapOff;
  PCI_TYPE_GENERIC       *PciCap    = NULL;
  PCI_CAPABILITY_PCIEXP  *PciExpCap = NULL;

  PCIeCapOff = PCIeFindCap (Private->EcamBase, EFI_PCI_CAPABILITY_ID_PCIEXP);
  if (!PCIeCapOff) {
    DEBUG ((EFI_D_VERBOSE, "Failed to find PCIe capability registers\r\n"));
    return EFI_NOT_FOUND;
  }

  PciExpCap = (PCI_CAPABILITY_PCIEXP *)(Private->EcamBase + PCIeCapOff);

  PciExpCap->RootControl.Bits.SystemErrorOnCorrectableError = 1;
  PciExpCap->RootControl.Bits.SystemErrorOnNonFatalError    = 1;
  PciExpCap->RootControl.Bits.SystemErrorOnFatalError       = 1;

  PciExpCap->DeviceControl.Bits.CorrectableError   = 1;
  PciExpCap->DeviceControl.Bits.NonFatalError      = 1;
  PciExpCap->DeviceControl.Bits.FatalError         = 1;
  PciExpCap->DeviceControl.Bits.UnsupportedRequest = 1;

  PciCap = (PCI_TYPE_GENERIC *)(Private->EcamBase);

  PciCap->Bridge.Hdr.Command |= EFI_PCI_COMMAND_SERR;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
InitializeController (
  PCIE_CONTROLLER_PRIVATE  *Private,
  IN  EFI_HANDLE           ControllerHandle
  )
{
  UINT64      val;
  UINT32      count;
  EFI_STATUS  Status;

  /* Program XAL */
  MmioWrite32 (Private->XalBase + XAL_RC_MEM_32BIT_BASE_HI, upper_32_bits (Private->MemBase));
  MmioWrite32 (Private->XalBase + XAL_RC_MEM_32BIT_BASE_LO, lower_32_bits (Private->MemBase));

  MmioWrite32 (Private->XalBase + XAL_RC_MEM_32BIT_LIMIT_HI, upper_32_bits (Private->MemLimit));
  MmioWrite32 (Private->XalBase + XAL_RC_MEM_32BIT_LIMIT_LO, lower_32_bits (Private->MemLimit));

  MmioWrite32 (Private->XalBase + XAL_RC_MEM_64BIT_BASE_HI, upper_32_bits (Private->PrefetchMemBase));
  MmioWrite32 (Private->XalBase + XAL_RC_MEM_64BIT_BASE_LO, lower_32_bits (Private->PrefetchMemBase));

  MmioWrite32 (Private->XalBase + XAL_RC_MEM_64BIT_LIMIT_HI, upper_32_bits (Private->PrefetchMemLimit));
  MmioWrite32 (Private->XalBase + XAL_RC_MEM_64BIT_LIMIT_LO, lower_32_bits (Private->PrefetchMemLimit));

  MmioWrite32 (Private->XalBase + XAL_RC_IO_BASE_HI, upper_32_bits (Private->IoBase));
  MmioWrite32 (Private->XalBase + XAL_RC_IO_BASE_LO, lower_32_bits (Private->IoBase));

  MmioWrite32 (Private->XalBase + XAL_RC_IO_LIMIT_HI, upper_32_bits (Private->IoLimit));
  MmioWrite32 (Private->XalBase + XAL_RC_IO_LIMIT_LO, lower_32_bits (Private->IoLimit));

  val = XAL_RC_BAR_CNTL_STANDARD_IOBAR_EN | XAL_RC_BAR_CNTL_STANDARD_32B_BAR_EN |
        XAL_RC_BAR_CNTL_STANDARD_64B_BAR_EN;
  MmioWrite32 (Private->XalBase + XAL_RC_BAR_CNTL_STANDARD, val);

  DEBUG ((EFI_D_VERBOSE, "Programming XAL_RC registers is done\r\n"));

  /* Setup bus numbers */
  MmioAndThenOr32 (Private->EcamBase + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, 0xff000000, 0x00ff0100);

  /* Stup command register */
  MmioAndThenOr32 (
    Private->EcamBase + PCI_COMMAND_OFFSET,
    0xffff0000,
    EFI_PCI_COMMAND_IO_SPACE |
    EFI_PCI_COMMAND_MEMORY_SPACE |
    EFI_PCI_COMMAND_BUS_MASTER |
    EFI_PCI_COMMAND_SERR
    );

  /*
   * TODO: Make sure that it is called only in the Firmware-First flow
   * and is skipped in the Os-First flow
   */
  Status = InitFWFIntr (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Enable Firmware-First Interrupt(%r)\r\n", Status));
    return Status;
  }

  val  = MmioRead32 (Private->XtlPriBase + XTL_RC_MGMT_PERST_CONTROL);
  val |= XTL_RC_MGMT_PERST_CONTROL_PERST_O_N;
  MmioWrite32 (Private->XtlPriBase + XTL_RC_MGMT_PERST_CONTROL, val);

  /* Wait for link up */
  count = 0;
  do {
    MicroSecondDelay (100);
    val = MmioRead32 (Private->EcamBase + XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS);
    if (val & XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS_DLL_ACTIVE) {
      DEBUG ((EFI_D_ERROR, "PCIe Controller-%d Link is UP (Speed: %d)\r\n", Private->CtrlId, (val & 0xf0000) >> 16));
      break;
    }

    count++;
  } while (count < 10000);

  if (count == 10000) {
    DEBUG ((EFI_D_ERROR, "PCIe Controller-%d Link is DOWN\r\n", Private->CtrlId));
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
UninitializeController (
  IN  EFI_HANDLE  ControllerHandle
  )
{
  return EFI_SUCCESS;

  /* All this is not required at this point in time */

  return EFI_SUCCESS;
}

/**
  Exit Boot Services Event notification handler.

  Notify PCIe driver about the event.

  @param[in]  Event     Event whose notification function is being invoked.
  @param[in]  Context   Pointer to the notification function's context.

**/
VOID
EFIAPI
OnExitBootServices (
  IN      EFI_EVENT  Event,
  IN      VOID       *Context
  )
{
  UninitializeController ((EFI_HANDLE)Context);
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
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE           *RootBridge       = NULL;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath = NULL;
  CONST VOID                *BusProperty      = NULL;
  CONST VOID                *RangesProperty   = NULL;
  INT32                     PropertySize      = 0;
  INT32                     AddressCells;
  INT32                     PciAddressCells;
  INT32                     SizeCells;
  INT32                     RangeSize;
  CONST VOID                *SegmentNumber = NULL;
  PCIE_CONTROLLER_PRIVATE   *Private       = NULL;
  EFI_EVENT                 ExitBootServiceEvent;
  UINT32                    DeviceTreeHandle;
  UINT32                    NumberOfInterrupts;
  UINT32                    Index;
  UINT32                    Index2;
  BOOLEAN                   PcieFound;
  CONST UINT32              *InterruptMap;
  INT32                     RPNodeOffset;

  Status    = EFI_SUCCESS;
  PcieFound = FALSE;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
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

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &Private->XalBase, &Private->XalSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate XAL address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &Private->XtlBase, &Private->XtlSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate XTL address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 2, &Private->XtlPriBase, &Private->XtlPriSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate XTL-PRI address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 3, &Private->XplBase, &Private->XplSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate XPL address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 4, &Private->EcamBase, &Private->EcamSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate ECAM address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Private->Signature                                   = PCIE_CONTROLLER_SIGNATURE;
      Private->PcieRootBridgeConfigurationIo.Read          = PcieConfigurationRead;
      Private->PcieRootBridgeConfigurationIo.Write         = PcieConfigurationWrite;
      Private->PcieRootBridgeConfigurationIo.SegmentNumber = 0;

      SegmentNumber = fdt_getprop (
                        DeviceTreeNode->DeviceTreeBase,
                        DeviceTreeNode->NodeOffset,
                        "linux,pci-domain",
                        &PropertySize
                        );
      if ((SegmentNumber == NULL) || (PropertySize != sizeof (UINT32))) {
        DEBUG ((DEBUG_ERROR, "Failed to read segment number\n"));
      } else {
        CopyMem (&Private->PcieRootBridgeConfigurationIo.SegmentNumber, SegmentNumber, sizeof (UINT32));
        Private->PcieRootBridgeConfigurationIo.SegmentNumber = SwapBytes32 (Private->PcieRootBridgeConfigurationIo.SegmentNumber);
      }

      DEBUG ((DEBUG_ERROR, "Segment Number = %u\n", Private->PcieRootBridgeConfigurationIo.SegmentNumber));

      /* Currently Segment number is nothing but the controller-ID  */
      Private->CtrlId = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
      DEBUG ((DEBUG_ERROR, "Controller-ID = %u\n", Private->CtrlId));

      RPNodeOffset = fdt_first_subnode (
                       DeviceTreeNode->DeviceTreeBase,
                       DeviceTreeNode->NodeOffset
                       );
      if (RPNodeOffset > 0) {
        if (fdt_get_property (
              DeviceTreeNode->DeviceTreeBase,
              RPNodeOffset,
              "external-facing",
              NULL
              ) != NULL)
        {
          Private->PcieRootBridgeConfigurationIo.IsExternalFacingPort = TRUE;
        }
      }

      RootBridge->Segment               = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
      RootBridge->Supports              = 0;
      RootBridge->Attributes            = 0;
      RootBridge->DmaAbove4G            = TRUE;
      RootBridge->NoExtendedConfigSpace = FALSE;
      RootBridge->ResourceAssigned      = FALSE;
      RootBridge->AllocationAttributes  = EFI_PCI_HOST_BRIDGE_MEM64_DECODE;

      BusProperty = fdt_getprop (
                      DeviceTreeNode->DeviceTreeBase,
                      DeviceTreeNode->NodeOffset,
                      "bus-range",
                      &PropertySize
                      );
      if ((BusProperty == NULL) || (PropertySize != 2 * sizeof (UINT32))) {
        DEBUG ((EFI_D_INFO, "PCIe Controller: unknown bus size in fdt, default to 0-255\r\n"));
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
        DEBUG ((EFI_D_ERROR, "PCIe Controller, size 3 is required for address-cells, got %d\r\n", PciAddressCells));
        Status = EFI_DEVICE_ERROR;
        break;
      }

      RangesProperty = fdt_getprop (
                         DeviceTreeNode->DeviceTreeBase,
                         DeviceTreeNode->NodeOffset,
                         "ranges",
                         &PropertySize
                         );
      // Mark all regions as unsupported
      RootBridge->Io.Base          = MAX_UINT64;
      RootBridge->Mem.Base         = MAX_UINT64;
      RootBridge->MemAbove4G.Base  = MAX_UINT64;
      RootBridge->PMem.Base        = MAX_UINT64;
      RootBridge->PMemAbove4G.Base = MAX_UINT64;

      if ((RangesProperty == NULL) || ((PropertySize % RangeSize) != 0)) {
        DEBUG ((EFI_D_ERROR, "PCIe Controller: Unsupported ranges configuration\r\n"));
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
          DEBUG ((EFI_D_ERROR, "PCIe Controller: Invalid address cells (%d)\r\n", AddressCells));
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
          DEBUG ((EFI_D_ERROR, "PCIe Controller: Invalid size cells (%d)\r\n", SizeCells));
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
              DEBUG ((EFI_D_ERROR, "Non 1:1 mapping is NOT supported for Prefetchable aperture\n"));
              Status = EFI_DEVICE_ERROR;
              break;
            }

            RootBridge->PMemAbove4G.Base                                = DeviceAddress;
            RootBridge->PMemAbove4G.Limit                               = Limit;
            RootBridge->PMemAbove4G.Translation                         = Translation;
            Private->PrefetchMemBase                                    = HostAddress;
            Private->PrefetchMemLimit                                   = HostAddress + Size - 1;
            Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 3;
            DEBUG ((EFI_D_INFO, "PREF64: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", DeviceAddress, Limit, Translation));
          } else {
            if (Translation) {
              RootBridge->Mem.Base                                        = DeviceAddress;
              RootBridge->Mem.Limit                                       = Limit;
              RootBridge->Mem.Translation                                 = Translation;
              Private->MemBase                                            = HostAddress;
              Private->MemLimit                                           = HostAddress + Size - 1;
              Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 3;
              DEBUG ((EFI_D_INFO, "MEM32: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", DeviceAddress, Limit, Translation));
            } else {
              DEBUG ((EFI_D_ERROR, "1:1 mapping is NOT supported for Non-Prefetchable aperture\n"));
              Status = EFI_DEVICE_ERROR;
              break;
            }
          }
        } else if (Space == PCIE_DEVICETREE_SPACE_MEM32) {
          DEBUG ((EFI_D_ERROR, "32-bit aperture usage for memory is not supported\n"));
          Status = EFI_DEVICE_ERROR;
          break;
        } else {
          DEBUG ((EFI_D_ERROR, "PCIe Controller: Unknown region 0x%08x 0x%016llx-0x%016llx T 0x%016llx\r\n", Flags, DeviceAddress, Limit, Translation));
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

      Status = InitializeController (Private, ControllerHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Unable to initialize controller (%r)\r\n", __FUNCTION__, Status));
        break;
      }

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_NOTIFY,
                      OnExitBootServices,
                      ControllerHandle,
                      &gEfiEventExitBootServicesGuid,
                      &ExitBootServiceEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Unable to setup exit boot services uninitialize. (%r)\r\n", __FUNCTION__, Status));
        break;
      }

      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&ParentDevicePath
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get device path (%r)\r\n", __FUNCTION__, Status));
        break;
      }

      RootBridge->DevicePath = AppendDevicePathNode (
                                 ParentDevicePath,
                                 (EFI_DEVICE_PATH_PROTOCOL  *)&mPciRootBridgeDevicePathNode
                                 );

      // Setup configuration structure
      Private->ConfigSpaceInfo.BaseAddress           = Private->EcamBase;
      Private->ConfigSpaceInfo.PciSegmentGroupNumber = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
      Private->ConfigSpaceInfo.StartBusNumber        = Private->PcieRootBridgeConfigurationIo.MinBusNumber;
      Private->ConfigSpaceInfo.EndBusNumber          = Private->PcieRootBridgeConfigurationIo.MaxBusNumber;
      Private->ConfigSpaceInfo.AddressMapToken       = REFERENCE_TOKEN (Private->AddressMapRefInfo);
      Private->ConfigSpaceInfo.InterruptMapToken     = REFERENCE_TOKEN (Private->InterruptRefInfo);

      Status = GetDeviceTreeHandle (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, &DeviceTreeHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get device tree handle\r\n", __FUNCTION__));
        break;
      }

      InterruptMap = (CONST UINT32 *)fdt_getprop (
                                       DeviceTreeNode->DeviceTreeBase,
                                       DeviceTreeNode->NodeOffset,
                                       "interrupt-map",
                                       &PropertySize
                                       );
      if ((InterruptMap == NULL) || ((PropertySize % PCIE_INTERRUPT_MAP_ENTRY_SIZE) != 0)) {
        Status = EFI_DEVICE_ERROR;
        DEBUG ((DEBUG_ERROR, "%a: Failed to get pcie interrupts\r\n", __FUNCTION__));
        break;
      }

      NumberOfInterrupts = PropertySize / PCIE_INTERRUPT_MAP_ENTRY_SIZE;
      if (NumberOfInterrupts == 1) {
        for (Index = 0; Index < PCIE_NUMBER_OF_INTERUPT_MAP; Index++) {
          Private->InterruptRefInfo[Index].ReferenceToken          = REFERENCE_TOKEN (Private->InterruptMapInfo[Index]);
          Private->InterruptMapInfo[Index].PciInterrupt            = Index;
          Private->InterruptMapInfo[Index].IntcInterrupt.Interrupt = SwapBytes32 (InterruptMap[PCIE_PARENT_INTERRUPT_OFFSET]);
          Private->InterruptMapInfo[Index].IntcInterrupt.Flags     = BIT2;
        }
      } else if (NumberOfInterrupts == PCIE_NUMBER_OF_INTERUPT_MAP) {
        for (Index = 0; Index < PCIE_NUMBER_OF_INTERUPT_MAP; Index++) {
          Private->InterruptRefInfo[Index].ReferenceToken          = REFERENCE_TOKEN (Private->InterruptMapInfo[Index]);
          Private->InterruptMapInfo[Index].PciInterrupt            = SwapBytes32 (InterruptMap[(Index * PCIE_INTERRUPT_MAP_ENTRIES) + PCIE_CHILD_INT_OFFSET])-1;
          Private->InterruptMapInfo[Index].IntcInterrupt.Interrupt = SwapBytes32 (InterruptMap[(Index * PCIE_INTERRUPT_MAP_ENTRIES) + PCIE_PARENT_INTERRUPT_OFFSET]);
          Private->InterruptMapInfo[Index].IntcInterrupt.Flags     = BIT2;
        }
      } else {
        Status = EFI_DEVICE_ERROR;
        DEBUG ((DEBUG_ERROR, "%a: Expected %d interrupts, got %d\r\n", __FUNCTION__, PCIE_NUMBER_OF_INTERUPT_MAP, NumberOfInterrupts));
        break;
      }

      for (Index = 0; Index < Private->AddressMapCount; Index++) {
        Private->AddressMapRefInfo[Index].ReferenceToken = REFERENCE_TOKEN (Private->AddressMapInfo[Index]);
      }

      Index                                  = 0;
      Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjPciConfigSpaceInfo);
      Private->RepoInfo[Index].CmObjectToken = CM_NULL_TOKEN;
      Private->RepoInfo[Index].CmObjectSize  = sizeof (Private->ConfigSpaceInfo);
      Private->RepoInfo[Index].CmObjectCount = 1;
      Private->RepoInfo[Index].CmObjectPtr   = &Private->ConfigSpaceInfo;
      Index++;

      Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
      Private->RepoInfo[Index].CmObjectToken = REFERENCE_TOKEN (Private->InterruptRefInfo);
      Private->RepoInfo[Index].CmObjectSize  = sizeof (CM_ARM_OBJ_REF) * PCIE_NUMBER_OF_INTERUPT_MAP;
      Private->RepoInfo[Index].CmObjectCount = PCIE_NUMBER_OF_INTERUPT_MAP;
      Private->RepoInfo[Index].CmObjectPtr   = Private->InterruptRefInfo;
      Index++;

      Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
      Private->RepoInfo[Index].CmObjectToken = REFERENCE_TOKEN (Private->AddressMapRefInfo);
      Private->RepoInfo[Index].CmObjectSize  = sizeof (CM_ARM_OBJ_REF) * Private->AddressMapCount;
      Private->RepoInfo[Index].CmObjectCount = Private->AddressMapCount;
      Private->RepoInfo[Index].CmObjectPtr   = Private->AddressMapRefInfo;
      Index++;

      for (Index2 = 0; Index2 < PCIE_NUMBER_OF_MAPPING_SPACE; Index2++) {
        Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjPciAddressMapInfo);
        Private->RepoInfo[Index].CmObjectToken = REFERENCE_TOKEN (Private->AddressMapInfo[Index2]);
        Private->RepoInfo[Index].CmObjectSize  = sizeof (Private->AddressMapInfo[Index2]);
        Private->RepoInfo[Index].CmObjectCount = 1;
        Private->RepoInfo[Index].CmObjectPtr   = &Private->AddressMapInfo[Index2];
        Index++;
      }

      for (Index2 = 0; Index2 < PCIE_NUMBER_OF_INTERUPT_MAP; Index2++) {
        Private->RepoInfo[Index].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjPciInterruptMapInfo);
        Private->RepoInfo[Index].CmObjectToken = REFERENCE_TOKEN (Private->InterruptMapInfo[Index2]);
        Private->RepoInfo[Index].CmObjectSize  = sizeof (Private->InterruptMapInfo[Index2]);
        Private->RepoInfo[Index].CmObjectCount = 1;
        Private->RepoInfo[Index].CmObjectPtr   = &Private->InterruptMapInfo[Index2];
        Index++;
      }

      if (!mPcieAcpiConfigInstalled) {
        mPcieAcpiConfigInstalled               = TRUE;
        Private->RepoInfo[Index].CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
        Private->RepoInfo[Index].CmObjectToken = CM_NULL_TOKEN;
        Private->RepoInfo[Index].CmObjectSize  = sizeof (CmAcpiTableList);
        Private->RepoInfo[Index].CmObjectCount = sizeof (CmAcpiTableList) / sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
        Private->RepoInfo[Index].CmObjectPtr   = &CmAcpiTableList;
        for (Index2 = 0; Index2 < Private->RepoInfo[Index].CmObjectCount; Index2++) {
          CmAcpiTableList[Index2].OemTableId =  PcdGet64 (PcdAcpiDefaultOemTableId);
        }

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
                      &gNVIDIAPcieControllerInitCompleteProtocolGuid,
                      NULL,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to install PCI controller init complete protocol (%r)\r\n", __FUNCTION__, Status));
      }

    default:
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
