/** @file

  PCIe Controller Driver

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeIo.h>
#include <libfdt.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <IndustryStandard/Pci.h>
#include <Protocol/BpmpIpc.h>

#include "PcieControllerPrivate.h"

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra194-pcie", &gNVIDIANonDiscoverableT194PcieDeviceGuid },
    { NULL, NULL }
};


STATIC ACPI_HID_DEVICE_PATH mPciRootBridgeDevicePathNode = {
  {
    ACPI_DEVICE_PATH,
    ACPI_DP,
    {
      (UINT8) (sizeof(ACPI_HID_DEVICE_PATH)),
      (UINT8) ((sizeof(ACPI_HID_DEVICE_PATH)) >> 8)
    }
  },
  EISA_PNP_ID(0x0A03), // PCI
  0
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA Pcie controller driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = FALSE,
    .AutoDeassertReset = FALSE,
    .AutoDeassertPg = TRUE,
    .SkipEdkiiNondiscoverableInstall = TRUE
};

CHAR8 CoreClockNames[][PCIE_CLOCK_RESET_NAME_LENGTH] = {
  "core",
  "core_clk"
};

CHAR8 CoreAPBResetNames[][PCIE_CLOCK_RESET_NAME_LENGTH] = {
  "apb",
  "core_apb",
  "core_apb_rst"
};

STATIC
VOID
AtuWrite (
    IN PCIE_CONTROLLER_PRIVATE *Private,
    IN UINT8                   Index,
    IN UINT32                  Offset,
    IN UINT32                  Value
    )
{
  MmioWrite32 (Private->AtuBase + (Index * 0x200) + Offset, Value);
  return;
}

/**
 * Configures the output ATU
 *
 * @param Private    - Private data structure
 * @param Index      - ATU Index
 * @param Type       - Memory Type
 * @param CpuAddress - Address to map to
 * @param PciAddress - Device Address
 * @param Size       - Size of the region
 */
STATIC
VOID
ConfigureAtu (
    IN PCIE_CONTROLLER_PRIVATE  *Private,
    IN UINT8                    Index,
    IN UINT8                    Type,
    IN UINT64                   CpuAddress,
    IN UINT64                   PciAddress,
    IN UINT64                   Size
    )
{
  UINT64 MaxAddress = CpuAddress + Size - 1;


  AtuWrite (Private, Index, TEGRA_PCIE_ATU_LOWER_BASE, CpuAddress & (SIZE_4GB-1));
  AtuWrite (Private, Index, TEGRA_PCIE_ATU_UPPER_BASE, CpuAddress >> 32);
  AtuWrite (Private, Index, TEGRA_PCIE_ATU_LIMIT,      MaxAddress & (SIZE_4GB-1));
  AtuWrite (Private, Index, TEGRA_PCIE_ATU_UPPER_LIMIT,      MaxAddress >> 32);

  AtuWrite (Private, Index, TEGRA_PCIE_ATU_LOWER_TARGET, PciAddress & (SIZE_4GB-1));
  AtuWrite (Private, Index, TEGRA_PCIE_ATU_UPPER_TARGET, PciAddress >> 32);

  AtuWrite (Private, Index, TEGRA_PCIE_ATU_CR1, Type | TEGRA_PCIE_ATU_INCREASE_REGION_SIZE);
  AtuWrite (Private, Index, TEGRA_PCIE_ATU_CR2, TEGRA_PCIE_ATU_ENABLE);
}

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
  IN     NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL *This,
  IN     BOOLEAN                                          Read,
  IN     NVIDIA_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH         Width,
  IN     UINT64                                           Address,
  IN OUT VOID                                             *Buffer
  )
{
  EFI_STATUS                                   Status;
  PCIE_CONTROLLER_PRIVATE                      *Private;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS  PciAddress;
  UINT64                                       ConfigAddress;
  UINT32                                       Register;
  UINT8                                        Length;

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

  //
  // Check to see if Buffer is NULL
  // Check to see if Width is in the valid range
  // Check if Register is in correct space
  //
  if ((Buffer == NULL) ||
      ((UINT32) Width >= NvidiaPciWidthMaximum) ||
      (Register >= SIZE_4KB) ||
      (Register + Length > SIZE_4KB)) {
    Status = EFI_INVALID_PARAMETER;
  } else {
    if (((PciAddress.Bus == This->MinBusNumber) ||
         (PciAddress.Bus == This->MinBusNumber + 1)) &&
         (PciAddress.Device != 0)) {
      if (Read) {
        SetMem (Buffer, Length, 0xFF);
        Status = EFI_SUCCESS;
      } else {
        Status = EFI_SUCCESS;
      }
    } else {

      ConfigAddress = Private->ConfigurationSpace;
      if (PciAddress.Bus != This->MinBusNumber) {
        //Setup ATU
        UINT8 AtuType;
        if (PciAddress.Bus == (This->MinBusNumber + 1)) {
          AtuType = TEGRA_PCIE_ATU_TYPE_CFG0;
        } else {
          AtuType = TEGRA_PCIE_ATU_TYPE_CFG1;
        }
        ConfigAddress += Private->ConfigurationSize/2;
        ConfigureAtu (Private,
                      PCIE_ATU_REGION_INDEX0,
                      AtuType,
                      ConfigAddress,
                      PCIE_ATU_BUS (PciAddress.Bus) | PCIE_ATU_DEV (PciAddress.Device) | PCIE_ATU_FUNC (PciAddress.Function),
                      Private->ConfigurationSize/2);
      }

      if (Read) {
        if (Width == NvidiaPciWidthUint8) {
          *(UINT8 *)Buffer  = MmioRead8  (ConfigAddress + Register);
        } else if (Width == NvidiaPciWidthUint16) {
          *(UINT16 *)Buffer = MmioRead16 (ConfigAddress + Register);
        } else if (Width == NvidiaPciWidthUint32) {
          *(UINT32 *)Buffer = MmioRead32 (ConfigAddress + Register);
        } else {
          //No valid way to get here
          ASSERT (Width < NvidiaPciWidthMaximum);
        }
      } else {
        if (Width == NvidiaPciWidthUint8) {
          UINT32 Data = MmioRead32 (ConfigAddress + (Register & ~0x3));
          CopyMem (((VOID *)&Data) + (Register & 0x3), Buffer, 1);
          MmioWrite32 (ConfigAddress + (Register & ~0x3), Data);
        } else if (Width == NvidiaPciWidthUint16) {
          UINT32 Data = MmioRead32 (ConfigAddress + (Register & ~0x3));
          CopyMem (((VOID *)&Data) + (Register & 0x3), Buffer, 2);
          MmioWrite32 (ConfigAddress + (Register & ~0x3), Data);
        } else if (Width == NvidiaPciWidthUint32) {
          MmioWrite32 (ConfigAddress + Register, *(UINT32 *)Buffer);
        } else {
          //No valid way to get here
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
  IN     NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL *This,
  IN     NVIDIA_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH         Width,
  IN     UINT64                                           Address,
  IN OUT VOID                                             *Buffer
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
  IN     NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL *This,
  IN     NVIDIA_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH         Width,
  IN     UINT64                                           Address,
  IN OUT VOID                                             *Buffer
  )
{
  return PcieConfigurationAccess (This, FALSE, Width, Address, Buffer);
}

STATIC
EFI_STATUS
EFIAPI
InitializeController (
    PCIE_CONTROLLER_PRIVATE   *Private,
    IN  EFI_HANDLE ControllerHandle
    )
{
  EFI_STATUS Status;
  UINT32 val;
  UINT32 Index;
  UINT32 Count;

  /* Enable core clock */
  Count = sizeof (CoreClockNames)/sizeof (CoreClockNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryEnableClock (ControllerHandle, CoreClockNames[Index], 1);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Enabled Core clock\r\n"));
      break;
    }
  }
  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to enable core_clk\r\n"));
    return Status;
  }

  /* De-assert reset to CORE_APB */
  Count = sizeof (CoreAPBResetNames)/sizeof (CoreAPBResetNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryConfigReset (ControllerHandle, CoreAPBResetNames[Index], 0);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "De-asserted Core APB reset\r\n"));
      break;
    }
  }
  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to de-assert Core APB reset\r\n"));
    return Status;
  }

  /* Program APPL */

  /* Setup DBI region */
  MmioWrite32 (Private->ApplSpace + APPL_CFG_BASE_ADDR, Private->ConfigurationSpace & APPL_CFG_BASE_ADDR_MASK);

  /* configure this core for RP mode operation */
  MmioWrite32 (Private->ApplSpace + APPL_DM_TYPE, APPL_DM_TYPE_RP);

  MmioWrite32 (Private->ApplSpace + APPL_CFG_SLCG_OVERRIDE, 0x0);

  val = MmioRead32(Private->ApplSpace + APPL_CTRL);
  MmioWrite32 (Private->ApplSpace + APPL_CTRL, val | APPL_CTRL_SYS_PRE_DET_STATE);

  val = MmioRead32(Private->ApplSpace + APPL_CFG_MISC);
  val |= (APPL_CFG_MISC_ARCACHE_VAL << APPL_CFG_MISC_ARCACHE_SHIFT);
  MmioWrite32 (Private->ApplSpace + APPL_CFG_MISC, val);

  /* Programming the following to avoid dependency on CLKREQ */
  val = MmioRead32(Private->ApplSpace + APPL_PINMUX);
  val |= APPL_PINMUX_CLKREQ_OVERRIDE_EN;
  val &= ~APPL_PINMUX_CLKREQ_OVERRIDE;
  MmioWrite32 (Private->ApplSpace + APPL_PINMUX, val);

  /* Setup DBI region */
  MmioWrite32 (Private->ApplSpace + APPL_CFG_IATU_DMA_BASE_ADDR, Private->AtuBase & APPL_CFG_IATU_DMA_BASE_ADDR_MASK);

  /* Enable interrupt generation for PCIe legacy interrupts (INTx) */
  val = MmioRead32(Private->ApplSpace + APPL_INTR_EN_L0_0);
  val |= APPL_INTR_EN_L0_0_INT_INT_EN;
  val |= APPL_INTR_EN_L0_0_SYS_INTR_EN;
  MmioWrite32 (Private->ApplSpace + APPL_INTR_EN_L0_0, val);

  val = MmioRead32(Private->ApplSpace + APPL_INTR_EN_L1_8_0);
  val |= APPL_INTR_EN_L1_8_INTX_EN;
  MmioWrite32 (Private->ApplSpace + APPL_INTR_EN_L1_8_0, val);

  DEBUG ((EFI_D_ERROR, "Programming APPL registers is done\r\n"));

  /* De-assert reset to CORE */
  Status = DeviceDiscoveryConfigReset (ControllerHandle, "core", 0);
  if (EFI_ERROR (Status)) {
    Status = DeviceDiscoveryConfigReset (ControllerHandle, "core_rst", 0);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, " Failed to de-assert Core reset\r\n"));
      return Status;
    }
  }
  DEBUG ((EFI_D_ERROR, "De-asserted Core reset\r\n"));

  /* Program CORE (i.e. DBI) */

  val = MmioRead32(Private->ConfigurationSpace + PCI_IO_BASE);
  val &= ~(IO_BASE_IO_DECODE | IO_BASE_IO_DECODE_BIT8);
  MmioWrite32 (Private->ConfigurationSpace + PCI_IO_BASE, val);

  val = MmioRead32(Private->ConfigurationSpace + PCI_PREF_MEMORY_BASE);
  val |= CFG_PREF_MEM_LIMIT_BASE_MEM_DECODE;
  val |= CFG_PREF_MEM_LIMIT_BASE_MEM_LIMIT_DECODE;
  MmioWrite32 (Private->ConfigurationSpace + PCI_PREF_MEMORY_BASE, val);

  /* setup RC BARs */
  MmioWrite32 (Private->ConfigurationSpace + PCI_BASE_ADDRESS_0, 0);
  MmioWrite32 (Private->ConfigurationSpace + PCI_BASE_ADDRESS_1, 0);

  /* Configure Max Speed to Gen-1 */
  val = MmioRead32(Private->ConfigurationSpace + PCI_EXP_LNKCAP);
  val &= ~PCI_EXP_LNKCAP_SLS;
  val |= 0x1; /* Limit the speed to Gen-1 for now */
  MmioWrite32 (Private->ConfigurationSpace + PCI_EXP_LNKCAP, val);

  val = MmioRead32 (Private->ConfigurationSpace + PCI_EXP_LNKCTL_STS_2);
  val &= ~PCI_EXP_LNKCAP_SLS;
  val |= 0x1;
  MmioWrite32 (Private->ConfigurationSpace + PCI_EXP_LNKCTL_STS_2, val);

  /* setup interrupt pins */
  MmioAndThenOr32 (Private->ConfigurationSpace + PCI_INT_LINE_OFFSET, 0xffff00ff, 0x00000100);

  /* setup bus numbers */
  MmioAndThenOr32 (Private->ConfigurationSpace + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, 0xff000000, 0x00ff0100);

  /* setup command register */
  MmioAndThenOr32 (Private->ConfigurationSpace + PCI_COMMAND_OFFSET,
                   0xffff0000,
                   EFI_PCI_COMMAND_IO_SPACE|EFI_PCI_COMMAND_MEMORY_SPACE|EFI_PCI_COMMAND_BUS_MASTER|EFI_PCI_COMMAND_SERR
                   );

  /* program correct class for RC */
  MmioWrite32(Private->ConfigurationSpace + PCI_REVISION_ID_OFFSET,
              (PCI_CLASS_BRIDGE << 24) | (PCI_CLASS_BRIDGE_P2P << 16) | (PCI_IF_BRIDGE_P2P << 8) | 0x1
              );

  //Program Vendor/DeviceID
  MmioWrite32 (Private->ConfigurationSpace + PCI_VENDOR_ID_OFFSET, 0x10DE1AD1);

  /* Disable write permission to DBI_RO_WR_EN protected registers */
  MmioAnd32 (Private->ConfigurationSpace + PCIE_MISC_CONTROL_1_OFF, ~PCIE_DBI_RO_WR_EN);

  DEBUG ((EFI_D_ERROR, "Programming CORE registers is done\r\n"));

  /* Apply PERST# to endpoint and go for link up */
  DEBUG ((EFI_D_ERROR, "Private->ApplSpace = 0x%08X\r\n", Private->ApplSpace));

  /* Assert PEX_RST */
  val = MmioRead32 (Private->ApplSpace + 0x0);
  val &= ~(0x1);
  MmioWrite32 (Private->ApplSpace + 0x0, val);

  MicroSecondDelay(100);

  /* enable LTSSM */
  val = MmioRead32 (Private->ApplSpace + 0x4);
  val |= (0x1 << 7);
  MmioWrite32 (Private->ApplSpace + 0x4, val);

  /* de-assert RST */
  val = MmioRead32 (Private->ApplSpace + 0x0);
  val |= (0x1);
  MmioWrite32 (Private->ApplSpace + 0x0, val);

  MicroSecondDelay(100000);

  val = MmioRead32 (Private->ConfigurationSpace + PCI_EXP_LNKCTL_STATUS);
  if (val & PCI_EXP_LNKCTL_STATUS_DLL_ACTIVE)
    DEBUG ((EFI_D_ERROR, "PCIe Controller-%d Link is UP\r\n", Private->CtrlId));
  else
    DEBUG ((EFI_D_ERROR, "PCIe Controller-%d Link is DOWN\r\n", Private->CtrlId));

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
UninitializeController (
    IN  EFI_HANDLE ControllerHandle
    )
{
  EFI_STATUS Status;

  /* Assert reset to CORE */
  Status = DeviceDiscoveryConfigReset (ControllerHandle, "core", 1);
  if (EFI_ERROR (Status)) {
    Status = DeviceDiscoveryConfigReset (ControllerHandle, "core_rst", 1);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, " Failed to assert Core reset\r\n"));
      return Status;
    }
  }
  DEBUG ((EFI_D_ERROR, "Asserted Core reset\r\n"));

  /* Assert reset to CORE_APB */
  Status = DeviceDiscoveryConfigReset (ControllerHandle, "core_apb", 1);
  if (EFI_ERROR (Status)) {
    Status = DeviceDiscoveryConfigReset (ControllerHandle, "core_apb_rst", 1);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Failed to assert Core APB reset\r\n"));
      return Status;
    }
  }
  DEBUG ((EFI_D_ERROR, "Asserted Core APB reset\r\n"));

  /* Disable core clock */
  Status = DeviceDiscoveryEnableClock (ControllerHandle, "core", 0);
  if (EFI_ERROR (Status)) {
    Status = DeviceDiscoveryEnableClock (ControllerHandle, "core_clk", 0);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Failed to disable core_clk\r\n"));
      return Status;
    }
  }
  DEBUG ((EFI_D_ERROR, "Disabled Core clock\r\n"));

  return EFI_SUCCESS;
}

struct cmd_uphy_pcie_controller_state_request {
    /** @brief PCIE controller number, valid: 0, 1, 2, 3, 4 */
    uint8_t pcie_controller;
    uint8_t enable;
} __attribute__((packed));

struct mrq_uphy_request {
    /** @brief Lane number. */
    uint16_t lane;
    /** @brief Sub-command id. */
    uint16_t cmd;

    union {
        struct cmd_uphy_pcie_controller_state_request controller_state;
    };
} __attribute__((packed));


EFI_STATUS
BpmpProcessSetCtrlState (
  IN NVIDIA_BPMP_IPC_PROTOCOL *BpmpIpcProtocol,
  IN UINT32                   CtrlId,
  IN BOOLEAN                  State
  )
{
  EFI_STATUS Status;
  struct mrq_uphy_request Request;

  if (BpmpIpcProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Request.cmd = 4;
  Request.controller_state.pcie_controller = CtrlId;
  Request.controller_state.enable = State;

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              69,
                              (VOID *)&Request,
                              sizeof (Request),
                              NULL,
                              0,
                              NULL
                              );
  if (Status == EFI_UNSUPPORTED) {
    Status = EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
  }
  return Status;
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
  IN      EFI_EVENT                         Event,
  IN      VOID                              *Context
  )
{
  UninitializeController ((EFI_HANDLE) Context);
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE           *RootBridge = NULL;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath = NULL;
  CONST VOID                *BusProperty = NULL;
  CONST VOID                *RangesProperty = NULL;
  INT32                     PropertySize  = 0;
  INT32                     AddressCells;
  INT32                     PciAddressCells;
  INT32                     SizeCells;
  INT32                     RangeSize;
  CONST VOID                *SegmentNumber = NULL;
  PCIE_CONTROLLER_PRIVATE   *Private = NULL;
  EFI_EVENT                 ExitBootServiceEvent;

  Status = EFI_SUCCESS;

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

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &Private->ApplSpace, &Private->ApplSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate appl address range\n", __FUNCTION__));
      Status = EFI_UNSUPPORTED;
      break;
    }

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &Private->ConfigurationSpace, &Private->ConfigurationSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate configuration address range\n", __FUNCTION__));
      Status = EFI_UNSUPPORTED;
      break;
    }

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 2, &Private->AtuBase, &Private->AtuSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate ATU address range\n", __FUNCTION__));
      Status = EFI_UNSUPPORTED;
      break;
    }

    Private->Signature = PCIE_CONTROLLER_SIGNATURE;
    Private->PcieRootBridgeConfigurationIo.Read  = PcieConfigurationRead;
    Private->PcieRootBridgeConfigurationIo.Write = PcieConfigurationWrite;
    Private->PcieRootBridgeConfigurationIo.SegmentNumber = 0;

    SegmentNumber = fdt_getprop (DeviceTreeNode->DeviceTreeBase,
                                 DeviceTreeNode->NodeOffset,
                                 "linux,pci-domain",
                                 &PropertySize);
    if ((SegmentNumber == NULL) || (PropertySize != sizeof(UINT32))) {
        DEBUG ((DEBUG_ERROR, "Failed to read segment number\n"));
    } else {
        CopyMem (&Private->PcieRootBridgeConfigurationIo.SegmentNumber, SegmentNumber, sizeof (UINT32));
        Private->PcieRootBridgeConfigurationIo.SegmentNumber = SwapBytes32 (Private->PcieRootBridgeConfigurationIo.SegmentNumber);
    }
    DEBUG ((DEBUG_ERROR, "Segment Number = %u\n", Private->PcieRootBridgeConfigurationIo.SegmentNumber));

    /* Currently Segment number is nothing but the controller-ID  */
    Private->CtrlId = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
    DEBUG ((DEBUG_ERROR, "Controller-ID = %u\n", Private->CtrlId));

    if (Private->CtrlId == 5) {
        Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 3, &Private->PexCtlBase, &Private->PexCtlSize);
        if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a: Unable to locate Pex_Ctl address range\n", __FUNCTION__));
            Status = EFI_UNSUPPORTED;
            break;
        }
        DEBUG ((EFI_D_ERROR, "Private->PexCtlBase = 0x%08X\r\n", Private->PexCtlBase));

        /* Configure C5's PEX_RST as output signal */
        MmioWrite32 (Private->PexCtlBase + 0x8, 0x520);
        DEBUG ((DEBUG_ERROR, "Configured Pex_rst for C5 controller\n"));
    } else {
        NVIDIA_BPMP_IPC_PROTOCOL *BpmpIpcProtocol = NULL;
        EFI_STATUS Status;

        Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
        if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to get BPMP-FW handle\n"));
            return EFI_NOT_READY;
        }

        if (PcdGetBool(PcdBPMPPCIeControllerEnable)) {
          Status = BpmpProcessSetCtrlState (BpmpIpcProtocol, Private->CtrlId, 1);
          if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "Failed to Enable Controller-%u\n", Private->CtrlId));
              return EFI_NOT_READY;
          }
          DEBUG ((DEBUG_ERROR, "Enabled Controller-%u through BPMP-FW\n", Private->CtrlId));
        }
    }

    Status = InitializeController(Private, ControllerHandle);
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

    RootBridge->Segment = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
    RootBridge->Supports = 0;
    RootBridge->Attributes = 0;
    RootBridge->DmaAbove4G = TRUE;
    RootBridge->NoExtendedConfigSpace = FALSE;
    RootBridge->ResourceAssigned = FALSE;
    RootBridge->AllocationAttributes = EFI_PCI_HOST_BRIDGE_MEM64_DECODE;

    BusProperty = fdt_getprop (DeviceTreeNode->DeviceTreeBase,
                               DeviceTreeNode->NodeOffset,
                               "bus-range",
                               &PropertySize);
    if ((BusProperty == NULL) || (PropertySize != 2 * sizeof(UINT32))) {
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

    AddressCells  = fdt_address_cells (DeviceTreeNode->DeviceTreeBase, fdt_parent_offset(DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset));
    PciAddressCells  = fdt_address_cells (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset);
    SizeCells     = fdt_size_cells (DeviceTreeNode->DeviceTreeBase, fdt_parent_offset(DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset));
    RangeSize     = (AddressCells + PciAddressCells + SizeCells) * sizeof (UINT32);

    if (PciAddressCells != 3) {
      DEBUG ((EFI_D_ERROR, "PCIe Controller, size 3 is required for address-cells, got %d\r\n", PciAddressCells));
      Status = EFI_DEVICE_ERROR;
      break;
    }

    RangesProperty = fdt_getprop (DeviceTreeNode->DeviceTreeBase,
                                  DeviceTreeNode->NodeOffset,
                                  "ranges",
                                  &PropertySize);
    //Mark all regions as unsupported
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
      UINT32 Flags = 0;
      UINT32 Space = 0;
      BOOLEAN Prefetchable = FALSE;
      UINT64 DeviceAddress = 0;
      UINT64 HostAddress = 0;
      UINT64 Limit = 0;
      UINT64 Size = 0;
      UINT64 Translation = 0;

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

      Space = Flags & PCIE_DEVICETREE_SPACE_CODE;
      Prefetchable = ((Flags & PCIE_DEVICETREE_PREFETCHABLE) == PCIE_DEVICETREE_PREFETCHABLE);
      Limit = DeviceAddress + Size - 1;
      Translation = DeviceAddress - HostAddress;

      if (Space == PCIE_DEVICETREE_SPACE_IO) {
        ASSERT (RootBridge->Io.Base == MAX_UINT64);
        RootBridge->Io.Base = DeviceAddress;
        RootBridge->Io.Limit = Limit;
        RootBridge->Io.Translation = Translation;
        ConfigureAtu (Private,
                      PCIE_ATU_REGION_INDEX1,
                      TEGRA_PCIE_ATU_TYPE_IO,
                      HostAddress,
                      DeviceAddress,
                      Size);
      } else if ((Space == PCIE_DEVICETREE_SPACE_MEM32) &&
                 (Limit < SIZE_4GB)) {
        ASSERT (RootBridge->Mem.Base == MAX_UINT64);
        RootBridge->Mem.Base = DeviceAddress;
        RootBridge->Mem.Limit = Limit;
        RootBridge->Mem.Translation = Translation;
        ConfigureAtu (Private,
                      PCIE_ATU_REGION_INDEX2,
                      TEGRA_PCIE_ATU_TYPE_MEM,
                      HostAddress,
                      DeviceAddress,
                      Size);
      } else if ((((Space == PCIE_DEVICETREE_SPACE_MEM32) &&
                 (Limit >= SIZE_4GB)) ||
                 (Space == PCIE_DEVICETREE_SPACE_MEM64))) {
        ASSERT (RootBridge->MemAbove4G.Base == MAX_UINT64);
        RootBridge->MemAbove4G.Base = DeviceAddress;
        RootBridge->MemAbove4G.Limit = Limit;
        RootBridge->MemAbove4G.Translation = Translation;
        ConfigureAtu (Private,
                      PCIE_ATU_REGION_INDEX3,
                      TEGRA_PCIE_ATU_TYPE_MEM,
                      HostAddress,
                      DeviceAddress,
                      Size);
      } else {
        DEBUG ((EFI_D_ERROR, "PCIe Controller: Unknown region 0x%08x 0x%016llx-0x%016llx T 0x%016llx\r\n", Flags, DeviceAddress, Limit, Translation));
        ASSERT (FALSE);
        Status = EFI_DEVICE_ERROR;
        break;
      }

      RangesProperty += RangeSize;
      PropertySize -= RangeSize;
    }

    if (EFI_ERROR (Status)) {
      break;
    }

    if ((RootBridge->PMem.Base == MAX_UINT64) && (RootBridge->PMemAbove4G.Base == MAX_UINT64)) {
      RootBridge->AllocationAttributes |= EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
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

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ControllerHandle,
                    &gNVIDIAPciHostBridgeProtocolGuid,
                    RootBridge,
                    &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                    &Private->PcieRootBridgeConfigurationIo,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to install root bridge info (%r)\r\n", __FUNCTION__, Status));
      break;
    }

    break;

  default:
    break;
  }

  if (EFI_ERROR (Status)) {
    if (RootBridge == NULL) {
      FreePool (RootBridge);
    }
    if (Private == NULL) {
      FreePool (Private);
    }
  }
  return Status;
}
