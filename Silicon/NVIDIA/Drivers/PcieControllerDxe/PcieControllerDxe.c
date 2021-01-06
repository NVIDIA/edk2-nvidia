/** @file

  PCIe Controller Driver

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/BpmpIpc.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PinMux.h>
#include <Protocol/Regulator.h>
#include <Protocol/TegraP2U.h>

#include <libfdt.h>

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Pci30.h>
#include <IndustryStandard/PciExpress31.h>

#include "PcieControllerPrivate.h"

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra194-pcie", &gNVIDIANonDiscoverableT194PcieDeviceGuid },
    { "nvidia,tegra234-pcie", &gNVIDIANonDiscoverableT234PcieDeviceGuid },
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

CHAR8 CoreResetNames[][PCIE_CLOCK_RESET_NAME_LENGTH] = {
  "core",
  "core_rst"
};

/*
 * These interfaces resemble the pci_find_*capability() interfaces, but these
 * are for configuring host controllers, which are bridges *to* PCI devices but
 * are not PCI devices themselves.
 */
STATIC
UINT8
__dw_pcie_find_next_cap(UINT64 DbiBase, UINT8 cap_ptr, UINT8 cap)
{
  UINT8 cap_id, next_cap_ptr;
  UINT16 reg;

  if (!cap_ptr)
    return 0;

  reg = MmioRead16(DbiBase + cap_ptr);
  cap_id = (reg & 0x00ff);

  if (cap_id > 0x14)
    return 0;

  if (cap_id == cap)
    return cap_ptr;

  next_cap_ptr = (reg & 0xff00) >> 8;

  return __dw_pcie_find_next_cap(DbiBase, next_cap_ptr, cap);
}

STATIC
UINT8
dw_pcie_find_capability(UINT64 DbiBase, UINT8 cap)
{
  UINT8 next_cap_ptr;
  UINT16 reg;

  reg = MmioRead16(DbiBase + PCI_CAPBILITY_POINTER_OFFSET);
  next_cap_ptr = (reg & 0x00ff);

  return __dw_pcie_find_next_cap(DbiBase, next_cap_ptr, cap);
}

STATIC
UINT16
dw_pcie_find_next_ext_capability(UINT64 DbiBase, UINT16 start, UINT8 cap)
{
  INT32 pos = PCI_CFG_SPACE_SIZE;
  UINT32 header;
  INT32 ttl;

  /* minimum 8 bytes per capability */
  ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

  if (start)
    pos = start;

  header = MmioRead32(DbiBase + pos);
  /*
   * If we have no capabilities, this is indicated by cap ID,
   * cap version and next pointer all being 0.
   */
  if (header == 0)
    return 0;

  while (ttl-- > 0) {
    if (PCI_EXT_CAP_ID(header) == cap && pos != start)
      return pos;

    pos = PCI_EXT_CAP_NEXT(header);
    if (pos < PCI_CFG_SPACE_SIZE)
      break;

    header = MmioRead32(DbiBase + pos);
  }

  return 0;
}

STATIC
UINT16
dw_pcie_find_ext_capability(UINT64 DbiBase, UINT8 cap)
{
  return dw_pcie_find_next_ext_capability(DbiBase, 0, cap);
}

STATIC VOID config_gen3_gen4_eq_presets(PCIE_CONTROLLER_PRIVATE *Private)
{
  UINT32 val, offset, i;

  /* Program init preset */
  for (i = 0; i < Private->NumLanes; i++) {
    val = MmioRead16(Private->DbiBase + CAP_SPCIE_CAP_OFF + (i * 2));
    val &= ~CAP_SPCIE_CAP_OFF_DSP_TX_PRESET0_MASK;
    val |= GEN3_GEN4_EQ_PRESET_INIT;
    val &= ~CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_MASK;
    val |= (GEN3_GEN4_EQ_PRESET_INIT <<
         CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_SHIFT);
    MmioWrite16(Private->DbiBase + CAP_SPCIE_CAP_OFF + (i * 2), val);

    offset = dw_pcie_find_ext_capability(Private->DbiBase,
                 PCI_EXT_CAP_ID_PL_16GT) +
        PCI_PL_16GT_LE_CTRL;
    val = MmioRead8(Private->DbiBase + offset + i);
    val &= ~PCI_PL_16GT_LE_CTRL_DSP_TX_PRESET_MASK;
    val |= GEN3_GEN4_EQ_PRESET_INIT;
    val &= ~PCI_PL_16GT_LE_CTRL_USP_TX_PRESET_MASK;
    val |= (GEN3_GEN4_EQ_PRESET_INIT <<
      PCI_PL_16GT_LE_CTRL_USP_TX_PRESET_SHIFT);
    MmioWrite8(Private->DbiBase + offset + i, val);
  }

  val = MmioRead32(Private->DbiBase + GEN3_RELATED_OFF);
  val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
  MmioWrite32(Private->DbiBase + GEN3_RELATED_OFF, val);

  val = MmioRead32(Private->DbiBase + GEN3_EQ_CONTROL_OFF);
  val &= ~GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_MASK;
  val |= (0x3ff << GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT);
  val &= ~GEN3_EQ_CONTROL_OFF_FB_MODE_MASK;
  MmioWrite32(Private->DbiBase + GEN3_EQ_CONTROL_OFF, val);

  val = MmioRead32(Private->DbiBase + GEN3_RELATED_OFF);
  val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
  val |= (0x1 << GEN3_RELATED_OFF_RATE_SHADOW_SEL_SHIFT);
  MmioWrite32(Private->DbiBase + GEN3_RELATED_OFF, val);

  val = MmioRead32(Private->DbiBase + GEN3_EQ_CONTROL_OFF);
  val &= ~GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_MASK;
  val |= (0x360 << GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT);
  val &= ~GEN3_EQ_CONTROL_OFF_FB_MODE_MASK;
  MmioWrite32(Private->DbiBase + GEN3_EQ_CONTROL_OFF, val);

  val = MmioRead32(Private->DbiBase + GEN3_RELATED_OFF);
  val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
  MmioWrite32(Private->DbiBase + GEN3_RELATED_OFF, val);
}

STATIC
VOID
ConfigureSidebandSignals (
    IN PCIE_CONTROLLER_PRIVATE  *Private
    )
{
    NVIDIA_PINMUX_PROTOCOL       *mPmux = NULL;
    UINT32 RegVal;
    EFI_STATUS Status = EFI_SUCCESS;

  Status = gBS->LocateProtocol (&gNVIDIAPinMuxProtocolGuid, NULL,
                (VOID **)&mPmux);
  if (EFI_ERROR (Status) || mPmux == NULL) {
    DEBUG ((EFI_D_ERROR,
    "%a: Couldn't get gNVIDIAPinMuxProtocolGuid Handle: %r\n",
    __FUNCTION__, Status));
  }

  mPmux->ReadReg(mPmux, PADCTL_PEX_RST, &RegVal);
  RegVal &= ~PADCTL_PEX_RST_E_INPUT;
  mPmux->WriteReg(mPmux, PADCTL_PEX_RST, RegVal);
}

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

      ConfigAddress = Private->DbiBase;
      if (PciAddress.Bus != This->MinBusNumber) {
        //Setup ATU
        UINT8 AtuType;
        if (PciAddress.Bus == (This->MinBusNumber + 1)) {
          AtuType = TEGRA_PCIE_ATU_TYPE_CFG0;
        } else {
          AtuType = TEGRA_PCIE_ATU_TYPE_CFG1;
        }
        ConfigAddress = Private->ConfigurationSpace;
        ConfigureAtu (Private,
                      PCIE_ATU_REGION_INDEX0,
                      AtuType,
                      ConfigAddress,
                      PCIE_ATU_BUS (PciAddress.Bus) | PCIE_ATU_DEV (PciAddress.Device) | PCIE_ATU_FUNC (PciAddress.Function),
                      Private->ConfigurationSize);
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
    IN  EFI_HANDLE ControllerHandle,
    IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
    )
{
  EFI_STATUS Status;
  UINT32 val;
  UINT32 Index;
  UINT32 Count;
  NVIDIA_TEGRAP2U_PROTOCOL *P2U = NULL;
  CONST VOID               *Property = NULL;
  INT32                     PropertySize  = 0;

  /* Enable core clock */
  Count = sizeof (CoreClockNames)/sizeof (CoreClockNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryEnableClock (ControllerHandle, CoreClockNames[Index], 1);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "Enabled Core clock\r\n"));
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
      DEBUG ((EFI_D_INFO, "De-asserted Core APB reset\r\n"));
      break;
    }
  }
  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to de-assert Core APB reset\r\n"));
    return Status;
  }

  /* Configure P2U */
  Status = gBS->LocateProtocol (&gNVIDIATegraP2UProtocolGuid, NULL, (VOID **)&P2U);
  if (EFI_ERROR (Status) || P2U == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get gNVIDIATegraP2UProtocolGuid Handle: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  Property = fdt_getprop(DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset,
                         "phys", &PropertySize);
  if (Property == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get P2U PHY entries\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < PropertySize; Index += sizeof (UINT32)) {
    UINT32 P2UId;

    CopyMem ((VOID *)&P2UId, Property + Index, sizeof (UINT32));
    P2UId = SwapBytes32 (P2UId);

    if EFI_ERROR(P2U->Init(P2U, P2UId)) {
      DEBUG ((EFI_D_ERROR, "Failed to Initialize P2U\n"));
    }
  }

  /* Program APPL */

  /* Setup DBI region */
  MmioWrite32 (Private->ApplSpace + APPL_CFG_BASE_ADDR, Private->DbiBase & APPL_CFG_BASE_ADDR_MASK);

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

  DEBUG ((EFI_D_INFO, "Programming APPL registers is done\r\n"));

  /* De-assert reset to CORE */
  Count = sizeof (CoreResetNames)/sizeof (CoreResetNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryConfigReset (Private->ControllerHandle,
                                         CoreResetNames[Index], 0);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "De-asserted Core reset\r\n"));
      break;
    }
  }
  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to De-assert Core reset\r\n"));
    return Status;
  }

  /* Program Core Registers (i.e. DBI) */

  Private->PcieCapOffset = dw_pcie_find_capability(Private->DbiBase,
                                                   EFI_PCI_CAPABILITY_ID_PCIEXP);

  val = dw_pcie_find_ext_capability(Private->DbiBase,
                                    PCI_EXPRESS_EXTENDED_CAPABILITY_L1_PM_SUBSTATES_ID);
  Private->ASPML1SSCapOffset = val + PCI_L1SS_CAP;

  /* Disable ASPM sub-states (L1.1 & L1.2) as we have removed dependency on CLKREQ signal */
  val = MmioRead32(Private->DbiBase + Private->ASPML1SSCapOffset);
  val &= ~PCI_L1SS_CAP_ASPM_L1_1;
  val &= ~PCI_L1SS_CAP_ASPM_L1_2;
  MmioWrite32(Private->DbiBase + Private->ASPML1SSCapOffset, val);

  val = MmioRead32(Private->DbiBase + PCI_IO_BASE);
  val &= ~(IO_BASE_IO_DECODE | IO_BASE_IO_DECODE_BIT8);
  MmioWrite32 (Private->DbiBase + PCI_IO_BASE, val);

  val = MmioRead32(Private->DbiBase + PCI_PREF_MEMORY_BASE);
  val |= CFG_PREF_MEM_LIMIT_BASE_MEM_DECODE;
  val |= CFG_PREF_MEM_LIMIT_BASE_MEM_LIMIT_DECODE;
  MmioWrite32 (Private->DbiBase + PCI_PREF_MEMORY_BASE, val);

  /* Setup RC BARs */
  MmioWrite32 (Private->DbiBase + PCI_BASE_ADDRESS_0, 0);
  MmioWrite32 (Private->DbiBase + PCI_BASE_ADDRESS_1, 0);

  /* Configure FTS */
  val = MmioRead32(Private->DbiBase + PORT_LOGIC_ACK_F_ASPM_CTRL);
  val &= ~(N_FTS_MASK << N_FTS_SHIFT);
  val |= N_FTS_VAL << N_FTS_SHIFT;
  MmioWrite32(Private->DbiBase + PORT_LOGIC_ACK_F_ASPM_CTRL, val);

  val = MmioRead32(Private->DbiBase + PORT_LOGIC_GEN2_CTRL);
  val &= ~FTS_MASK;
  val |= FTS_VAL;
  MmioWrite32(Private->DbiBase + PORT_LOGIC_GEN2_CTRL, val);

  /* Enable as 0xFFFF0001 response for CRS */
  val = MmioRead32(Private->DbiBase + PORT_LOGIC_AMBA_ERROR_RESPONSE_DEFAULT);
  val &= ~(AMBA_ERROR_RESPONSE_CRS_MASK << AMBA_ERROR_RESPONSE_CRS_SHIFT);
  val |= (AMBA_ERROR_RESPONSE_CRS_OKAY_FFFF0001 << AMBA_ERROR_RESPONSE_CRS_SHIFT);
  MmioWrite32(Private->DbiBase + PORT_LOGIC_AMBA_ERROR_RESPONSE_DEFAULT, val);

  /* Configure Max speed from DT */
  val = MmioRead32(Private->DbiBase + PCI_EXP_LNKCAP);
  val &= ~PCI_EXP_LNKCAP_SLS;
  val |= Private->MaxLinkSpeed;
  MmioWrite32(Private->DbiBase + PCI_EXP_LNKCAP, val);

  val = MmioRead32(Private->DbiBase + PCI_EXP_LNKCTL_STS_2);
  val &= ~PCI_EXP_LNKCAP_SLS;
  val |= Private->MaxLinkSpeed;
  MmioWrite32(Private->DbiBase + PCI_EXP_LNKCTL_STS_2, val);

  /* Configure Max lane width from DT */
  val = MmioRead32(Private->DbiBase + PCI_EXP_LNKCAP);
  val &= ~PCI_EXP_LNKCAP_MLW;
  val |= (Private->NumLanes << PCI_EXP_LNKSTA_NLW_SHIFT);
  MmioWrite32(Private->DbiBase + PCI_EXP_LNKCAP, val);

  config_gen3_gen4_eq_presets(Private);

  val = MmioRead32(Private->DbiBase + GEN3_RELATED_OFF);
  val &= ~GEN3_RELATED_OFF_GEN3_ZRXDC_NONCOMPL;
  MmioWrite32(Private->DbiBase + GEN3_RELATED_OFF, val);

  if (Private->UpdateFCFixUp) {
    val = MmioRead32(Private->DbiBase + CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF);
    val |= 0x1 << CFG_TIMER_CTRL_ACK_NAK_SHIFT;
    MmioWrite32(Private->DbiBase + CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF, val);
  }

  /*
   * NOTE: Linux programs following regiesters which is not really required
   * PCIE_PORT_LINK_CONTROL & PCIE_LINK_WIDTH_SPEED_CONTROL
   * It also programs PCIE_PL_CHK_REG_CONTROL_STATUS to enable CDM check but
   * that configuration is not required just yet
   */

  /* setup interrupt pins */
  MmioAndThenOr32 (Private->DbiBase + PCI_INT_LINE_OFFSET, 0xffff00ff, 0x00000100);

  /* setup bus numbers */
  MmioAndThenOr32 (Private->DbiBase + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, 0xff000000, 0x00ff0100);

  /* setup command register */
  MmioAndThenOr32 (Private->DbiBase + PCI_COMMAND_OFFSET,
                   0xffff0000,
                   EFI_PCI_COMMAND_IO_SPACE|EFI_PCI_COMMAND_MEMORY_SPACE|EFI_PCI_COMMAND_BUS_MASTER|EFI_PCI_COMMAND_SERR
                   );

  /* Program correct class for RC */
  MmioWrite32(Private->DbiBase + PCI_REVISION_ID_OFFSET,
              (PCI_CLASS_BRIDGE << 24) | (PCI_CLASS_BRIDGE_P2P << 16) | (PCI_IF_BRIDGE_P2P << 8) | 0x1
              );

  /* Enable Direct Speed Change */
  val = MmioRead32(Private->DbiBase + PORT_LOGIC_GEN2_CTRL);
  val |= PORT_LOGIC_GEN2_CTRL_DIRECT_SPEED_CHANGE;
  MmioWrite32(Private->DbiBase + PORT_LOGIC_GEN2_CTRL, val);

  /* Disable write permission to DBI_RO_WR_EN protected registers */
  MmioAnd32 (Private->DbiBase + PCIE_MISC_CONTROL_1_OFF, ~PCIE_DBI_RO_WR_EN);

  DEBUG ((EFI_D_INFO, "Programming CORE registers is done\r\n"));

  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoverySetClockFreq (ControllerHandle, CoreClockNames[Index], 500000000);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "Core clock is set\r\n"));
      break;
    }
  }
  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to set core_clk\r\n"));
    return Status;
  }

  /* Apply PERST# to endpoint and go for link up */
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

  val = MmioRead32 (Private->DbiBase + PCI_EXP_LNKCTL_STATUS);
  if (val & PCI_EXP_LNKCTL_STATUS_DLL_ACTIVE) {
    Private->LinkUp = TRUE;
    DEBUG ((EFI_D_INFO, "PCIe Controller-%d Link is UP (Speed: %d)\r\n",
           Private->CtrlId, (val & 0xf0000) >> 16));
  } else {
    DEBUG ((EFI_D_ERROR, "PCIe Controller-%d Link is DOWN\r\n", Private->CtrlId));
  }

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
TegraPcieTryLinkL2 (PCIE_CONTROLLER_PRIVATE *Private)
{
  UINT32 val;

  val = MmioRead32 (Private->ApplSpace + APPL_RADM_STATUS);
  val |= APPL_PM_XMT_TURNOFF_STATE;
  MmioWrite32 (Private->ApplSpace + APPL_RADM_STATUS, val);

  MicroSecondDelay(10000);

  val = MmioRead32 (Private->ApplSpace + APPL_DEBUG);
  if (val & APPL_DEBUG_PM_LINKST_IN_L2_LAT)
        return 0;
  else
        return 1;
}

STATIC
VOID
TegraPciePMETurnOff (PCIE_CONTROLLER_PRIVATE *Private)
{
  UINT32 data;

  if (!Private->LinkUp) {
    DEBUG ((EFI_D_INFO, "PCIe Controller-%d Link is not UP\r\n",
           Private->CtrlId));
    return;
  }

  if (TegraPcieTryLinkL2 (Private)) {
    DEBUG ((EFI_D_ERROR, "Link didn't transition to L2 state\r\n"));

    /*
     * TX lane clock freq will reset to Gen1 only if link is in L2
     * or detect state.
     * So apply pex_rst to end point to force RP to go into detect
     * state
     */
    data = MmioRead32 (Private->ApplSpace + APPL_PINMUX);
    data &= ~APPL_PINMUX_PEX_RST;
    MmioWrite32 (Private->ApplSpace + APPL_PINMUX, data);

    MicroSecondDelay(25000);

    data = MmioRead32 (Private->ApplSpace + APPL_CTRL);
    data &= ~APPL_CTRL_LTSSM_EN;
    MmioWrite32 (Private->ApplSpace + APPL_CTRL, data);

    MicroSecondDelay(25000);

    data = MmioRead32 (Private->ApplSpace + APPL_DEBUG);
    if (((data & APPL_DEBUG_LTSSM_STATE_MASK) >>  APPL_DEBUG_LTSSM_STATE_SHIFT)
        !=  LTSSM_STATE_PRE_DETECT) {
      DEBUG ((EFI_D_ERROR, "Link didn't go to detect state as well\r\n"));
    }
  }

  /*
   * DBI registers may not be accessible after this as PLL-E would be
   * down depending on how CLKREQ is pulled by end point
   */
  data = MmioRead32 (Private->ApplSpace + APPL_PINMUX);
  data |= (APPL_PINMUX_CLKREQ_OVERRIDE_EN | APPL_PINMUX_CLKREQ_OVERRIDE);
  /* Cut REFCLK to slot */
  data |= APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE_EN;
  data &= ~APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE;
  MmioWrite32 (Private->ApplSpace + APPL_PINMUX, data);
}

STATIC
EFI_STATUS
EFIAPI
UninitializeController (
    IN  EFI_HANDLE Handle
    )
{
  EFI_STATUS Status;
  UINT32 Index;
  UINT32 Count;
  PCIE_CONTROLLER_PRIVATE *Private = (PCIE_CONTROLLER_PRIVATE *)Handle;

  TegraPciePMETurnOff (Private);

  /* Assert reset to CORE */
  Count = sizeof (CoreResetNames)/sizeof (CoreResetNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryConfigReset (Private->ControllerHandle,
                                         CoreResetNames[Index], 1);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "Asserted Core reset\r\n"));
      break;
    }
  }
  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to assert Core reset\r\n"));
    return Status;
  }

  /* Assert reset to CORE_APB */
  Count = sizeof (CoreAPBResetNames)/sizeof (CoreAPBResetNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryConfigReset (Private->ControllerHandle,
                                         CoreAPBResetNames[Index], 1);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "Asserted Core APB reset\r\n"));
      break;
    }
  }
  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to assert Core APB reset\r\n"));
    return Status;
  }

  /* Disable core clock */
  Count = sizeof (CoreClockNames)/sizeof (CoreClockNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryEnableClock (Private->ControllerHandle,
                                         CoreClockNames[Index], 0);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "Disabled Core clock\r\n"));
      break;
    }
  }
  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to Disable core_clk\r\n"));
    return Status;
  }

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
  EFI_STATUS Status;
  VOID       *Rsdp = NULL;

  //Only Uninitialize if ACPI is not installed.
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &Rsdp);
  if (EFI_ERROR (Status)) {
    UninitializeController ((EFI_HANDLE) Context);
  }
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
  NVIDIA_REGULATOR_PROTOCOL *Regulator = NULL;
  CONST VOID                *Property = NULL;
  UINT32                    Val;

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

    Private->ControllerHandle = ControllerHandle;

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

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 3, &Private->DbiBase, &Private->DbiSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate DBI address range\n", __FUNCTION__));
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
    DEBUG ((EFI_D_INFO, "Segment Number = %u\n", Private->PcieRootBridgeConfigurationIo.SegmentNumber));

    /* Currently Segment number is nothing but the controller-ID  */
    Private->CtrlId = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
    DEBUG ((EFI_D_INFO, "Controller-ID = %u\n", Private->CtrlId));

    Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase,
                            DeviceTreeNode->NodeOffset,
                            "max-link-speed",
                            &PropertySize);
    if (Property != NULL) {
      CopyMem (&Private->MaxLinkSpeed, Property, sizeof (UINT32));
      Private->MaxLinkSpeed = SwapBytes32(Private->MaxLinkSpeed);
    }
    if (Private->MaxLinkSpeed <= 0 || Private->MaxLinkSpeed > 4)
      Private->MaxLinkSpeed = 4;
    DEBUG ((EFI_D_INFO, "Max Link Speed = %u\n", Private->MaxLinkSpeed));

    Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase,
                            DeviceTreeNode->NodeOffset,
                            "num-lanes",
                            &PropertySize);
    if (Property != NULL) {
      CopyMem (&Private->NumLanes, Property, sizeof (UINT32));
      Private->NumLanes = SwapBytes32(Private->NumLanes);
    }
    if (Private->NumLanes != 1 && ((Private->NumLanes % 2) != 0) && Private->NumLanes > 16)
      Private->NumLanes = 1;
    DEBUG ((EFI_D_INFO, "Number of lanes = %u\n", Private->NumLanes));

    if (NULL != fdt_get_property (DeviceTreeNode->DeviceTreeBase,
                                  DeviceTreeNode->NodeOffset,
                                  "nvidia,update-fc-fixup",
                                  NULL)) {
      Private->UpdateFCFixUp = TRUE;
    } else {
      Private->UpdateFCFixUp = FALSE;
    }

    /* Enable slot supplies */
    Status = gBS->LocateProtocol (&gNVIDIARegulatorProtocolGuid, NULL, (VOID **)&Regulator);
    if (EFI_ERROR (Status) || Regulator == NULL) {
      DEBUG ((EFI_D_ERROR, "%a: Couldn't get gNVIDIARegulatorProtocolGuid Handle: %r\n", __FUNCTION__, Status));
      Status = EFI_UNSUPPORTED;
      break;
    }

    /* Get the 3v3 supply */
    Property = fdt_getprop(DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset,
                           "vpcie3v3-supply", &PropertySize);
    if ((Property != NULL) && (PropertySize == sizeof (UINT32))) {
      Val = SwapBytes32 (*(UINT32 *)Property);
      /* Enable the 3v3 supply */
      if EFI_ERROR(Regulator-> Enable(Regulator, Val, TRUE)) {
        DEBUG ((EFI_D_ERROR, "Failed to Enable 3v3 Regulator\n"));
      }
    } else {
      DEBUG ((EFI_D_INFO, "Failed to find 3v3 slot supply regulator\n"));
    }

    /* Get the 12v supply */
    Property = fdt_getprop(DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset,
                           "vpcie12v-supply", &PropertySize);
    if ((Property != NULL) && (PropertySize == sizeof (UINT32))) {
      Val = SwapBytes32 (*(UINT32 *)Property);
      /* Enable the 12v supply */
      if EFI_ERROR(Regulator-> Enable(Regulator, Val, TRUE)) {
        DEBUG ((EFI_D_ERROR, "Failed to Enable 12v Regulator\n"));
      }
    } else {
      DEBUG ((EFI_D_INFO, "Failed to find 12v slot supply regulator\n"));
    }

    /* Spec defined T_PVPERL delay (100ms) after enabling power to the slot */
    MicroSecondDelay(100000);

    if (Private->CtrlId == 5) {
      ConfigureSidebandSignals(Private);
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
          DEBUG ((DEBUG_INFO, "Enabled Controller-%u through BPMP-FW\n", Private->CtrlId));
        }
    }

    Status = InitializeController(Private, ControllerHandle, DeviceTreeNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Unable to initialize controller (%r)\r\n", __FUNCTION__, Status));
      break;
    }

    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_NOTIFY,
                    OnExitBootServices,
                    Private,
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
