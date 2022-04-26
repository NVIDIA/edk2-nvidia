/** @file

  PCIe Controller Driver

  Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/BpmpIpc.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PinMux.h>
#include <Protocol/PowerGateNodeProtocol.h>
#include <Protocol/Regulator.h>
#include <Protocol/TegraP2U.h>

#include <libfdt.h>

#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Pci30.h>
#include <IndustryStandard/PciExpress31.h>

#include "PcieControllerPrivate.h"
#include <T194/T194Definitions.h>
#include <T234/T234Definitions.h>

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
  { "nvidia,tegra194-pcie", &gNVIDIANonDiscoverableT194PcieDeviceGuid },
  { "nvidia,tegra234-pcie", &gNVIDIANonDiscoverableT234PcieDeviceGuid },
  { NULL,                   NULL                                      }
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
  .AutoDeassertPg                  = FALSE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .DirectEnumerationSupport        = TRUE
};

CHAR8  CoreClockNames[][PCIE_CLOCK_RESET_NAME_LENGTH] = {
  "core",
  "core_clk"
};

CHAR8  CoreAPBResetNames[][PCIE_CLOCK_RESET_NAME_LENGTH] = {
  "apb",
  "core_apb",
  "core_apb_rst"
};

CHAR8  CoreResetNames[][PCIE_CLOCK_RESET_NAME_LENGTH] = {
  "core",
  "core_rst"
};

#pragma pack (1)
struct cmd_uphy_pcie_controller_state_request {
  /*
   * @brief PCIE controller number
   * Valid numbers for T194: 0, 1, 2, 3, 4
   * Valid numbers for T234: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
   */
  uint8_t    pcie_controller;
  uint8_t    enable;
};

struct mrq_uphy_request {
  /** @brief Lane number. */
  uint16_t    lane;
  /** @brief Sub-command id. */
  uint16_t    cmd;

  union {
    struct cmd_uphy_pcie_controller_state_request    controller_state;
  };
};

#pragma pack ()

/*
 * These interfaces resemble the pci_find_*capability() interfaces, but these
 * are for configuring host controllers, which are bridges *to* PCI devices but
 * are not PCI devices themselves.
 */
STATIC
UINT8
__dw_pcie_find_next_cap (
  UINT64  DbiBase,
  UINT8   cap_ptr,
  UINT8   cap
  )
{
  UINT8   cap_id, next_cap_ptr;
  UINT16  reg;

  if (!cap_ptr) {
    return 0;
  }

  reg    = MmioRead16 (DbiBase + cap_ptr);
  cap_id = (reg & 0x00ff);

  if (cap_id > 0x14) {
    return 0;
  }

  if (cap_id == cap) {
    return cap_ptr;
  }

  next_cap_ptr = (reg & 0xff00) >> 8;

  return __dw_pcie_find_next_cap (DbiBase, next_cap_ptr, cap);
}

STATIC
UINT8
dw_pcie_find_capability (
  UINT64  DbiBase,
  UINT8   cap
  )
{
  UINT8   next_cap_ptr;
  UINT16  reg;

  reg          = MmioRead16 (DbiBase + PCI_CAPBILITY_POINTER_OFFSET);
  next_cap_ptr = (reg & 0x00ff);

  return __dw_pcie_find_next_cap (DbiBase, next_cap_ptr, cap);
}

STATIC
UINT16
dw_pcie_find_next_ext_capability (
  UINT64  DbiBase,
  UINT16  start,
  UINT8   cap
  )
{
  INT32   pos = PCI_CFG_SPACE_SIZE;
  UINT32  header;
  INT32   ttl;

  /* minimum 8 bytes per capability */
  ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

  if (start) {
    pos = start;
  }

  header = MmioRead32 (DbiBase + pos);

  /*
   * If we have no capabilities, this is indicated by cap ID,
   * cap version and next pointer all being 0.
   */
  if (header == 0) {
    return 0;
  }

  while (ttl-- > 0) {
    if ((PCI_EXT_CAP_ID (header) == cap) && (pos != start)) {
      return pos;
    }

    pos = PCI_EXT_CAP_NEXT (header);
    if (pos < PCI_CFG_SPACE_SIZE) {
      break;
    }

    header = MmioRead32 (DbiBase + pos);
  }

  return 0;
}

STATIC
UINT16
dw_pcie_find_ext_capability (
  UINT64  DbiBase,
  UINT8   cap
  )
{
  return dw_pcie_find_next_ext_capability (DbiBase, 0, cap);
}

STATIC VOID
config_gen3_gen4_eq_presets (
  PCIE_CONTROLLER_PRIVATE  *Private
  )
{
  UINT32  val, offset, i;

  /* Program init preset */
  for (i = 0; i < Private->NumLanes; i++) {
    val  = MmioRead16 (Private->DbiBase + CAP_SPCIE_CAP_OFF + (i * 2));
    val &= ~CAP_SPCIE_CAP_OFF_DSP_TX_PRESET0_MASK;
    val |= GEN3_GEN4_EQ_PRESET_INIT;
    val &= ~CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_MASK;
    val |= (GEN3_GEN4_EQ_PRESET_INIT <<
            CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_SHIFT);
    MmioWrite16 (Private->DbiBase + CAP_SPCIE_CAP_OFF + (i * 2), val);

    offset = dw_pcie_find_ext_capability (
               Private->DbiBase,
               PCI_EXT_CAP_ID_PL_16GT
               ) +
             PCI_PL_16GT_LE_CTRL;
    val  = MmioRead8 (Private->DbiBase + offset + i);
    val &= ~PCI_PL_16GT_LE_CTRL_DSP_TX_PRESET_MASK;
    val |= GEN3_GEN4_EQ_PRESET_INIT;
    val &= ~PCI_PL_16GT_LE_CTRL_USP_TX_PRESET_MASK;
    val |= (GEN3_GEN4_EQ_PRESET_INIT <<
            PCI_PL_16GT_LE_CTRL_USP_TX_PRESET_SHIFT);
    MmioWrite8 (Private->DbiBase + offset + i, val);
  }

  val  = MmioRead32 (Private->DbiBase + GEN3_RELATED_OFF);
  val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
  MmioWrite32 (Private->DbiBase + GEN3_RELATED_OFF, val);

  val  = MmioRead32 (Private->DbiBase + GEN3_EQ_CONTROL_OFF);
  val &= ~GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_MASK;
  val |= (0x3ff << GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT);
  val &= ~GEN3_EQ_CONTROL_OFF_FB_MODE_MASK;
  MmioWrite32 (Private->DbiBase + GEN3_EQ_CONTROL_OFF, val);

  val  = MmioRead32 (Private->DbiBase + GEN3_RELATED_OFF);
  val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
  val |= (0x1 << GEN3_RELATED_OFF_RATE_SHADOW_SEL_SHIFT);
  MmioWrite32 (Private->DbiBase + GEN3_RELATED_OFF, val);

  val  = MmioRead32 (Private->DbiBase + GEN3_EQ_CONTROL_OFF);
  val &= ~GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_MASK;
  if (Private->IsT194) {
    val |= (0x360 << GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT);
  }

  if (Private->IsT234) {
    val |= (0x340 << GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT);
  }

  val &= ~GEN3_EQ_CONTROL_OFF_FB_MODE_MASK;
  MmioWrite32 (Private->DbiBase + GEN3_EQ_CONTROL_OFF, val);

  val  = MmioRead32 (Private->DbiBase + GEN3_RELATED_OFF);
  val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
  MmioWrite32 (Private->DbiBase + GEN3_RELATED_OFF, val);
}

STATIC
VOID
ConfigureSidebandSignals (
  IN PCIE_CONTROLLER_PRIVATE  *Private
  )
{
  NVIDIA_PINMUX_PROTOCOL  *mPmux = NULL;
  UINT32                  RegVal;
  EFI_STATUS              Status = EFI_SUCCESS;

  Status = gBS->LocateProtocol (
                  &gNVIDIAPinMuxProtocolGuid,
                  NULL,
                  (VOID **)&mPmux
                  );
  if (EFI_ERROR (Status) || (mPmux == NULL)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: Couldn't get gNVIDIAPinMuxProtocolGuid Handle: %r\n",
      __FUNCTION__,
      Status
      ));
    return;
  }

  mPmux->ReadReg (mPmux, PADCTL_PEX_RST, &RegVal);
  RegVal &= ~PADCTL_PEX_RST_E_INPUT;
  mPmux->WriteReg (mPmux, PADCTL_PEX_RST, RegVal);
}

STATIC
VOID
AtuWrite (
  IN PCIE_CONTROLLER_PRIVATE  *Private,
  IN UINT8                    Index,
  IN UINT32                   Offset,
  IN UINT32                   Value
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
  UINT64  MaxAddress = CpuAddress + Size - 1;

  AtuWrite (Private, Index, TEGRA_PCIE_ATU_LOWER_BASE, CpuAddress & (SIZE_4GB-1));
  AtuWrite (Private, Index, TEGRA_PCIE_ATU_UPPER_BASE, CpuAddress >> 32);
  AtuWrite (Private, Index, TEGRA_PCIE_ATU_LIMIT, MaxAddress & (SIZE_4GB-1));
  AtuWrite (Private, Index, TEGRA_PCIE_ATU_UPPER_LIMIT, MaxAddress >> 32);

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
      if (Private->IsT234) {
        ConfigAddress = (PciAddress.Bus) << 20 |
                        (PciAddress.Device) << 15 |
                        (PciAddress.Function) << 12;
        ConfigAddress = Private->EcamBase + ConfigAddress;
      } else {
        ConfigAddress = Private->DbiBase;
        if (PciAddress.Bus != This->MinBusNumber) {
          // Setup ATU
          UINT8  AtuType;
          if (PciAddress.Bus == (This->MinBusNumber + 1)) {
            AtuType = TEGRA_PCIE_ATU_TYPE_CFG0;
          } else {
            AtuType = TEGRA_PCIE_ATU_TYPE_CFG1;
          }

          ConfigAddress = Private->ConfigurationSpace;
          ConfigureAtu (
            Private,
            PCIE_ATU_REGION_INDEX0,
            AtuType,
            ConfigAddress,
            PCIE_ATU_BUS (PciAddress.Bus) | PCIE_ATU_DEV (PciAddress.Device) | PCIE_ATU_FUNC (PciAddress.Function),
            Private->ConfigurationSize
            );
        }
      }

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
EFI_STATUS
AssertPgNodes (
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Assert
  )
{
  EFI_STATUS                       Status;
  UINTN                            Index;
  NVIDIA_POWER_GATE_NODE_PROTOCOL  *PgProtocol;

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gNVIDIAPowerGateNodeProtocolGuid,
                  (VOID **)&PgProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: failed to retrieve powergate node protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
    if (Assert) {
      Status = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
    } else {
      Status = PgProtocol->Deassert (PgProtocol, PgProtocol->PowerGateId[Index]);
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((
        EFI_D_ERROR,
        "%a: failed to %a powergate 0x%x: %r\r\n",
        __FUNCTION__,
        Assert ? "assert" : "deassert",
        (UINTN)PgProtocol->PowerGateId[Index],
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
PrepareHost (
  PCIE_CONTROLLER_PRIVATE                    *Private,
  IN EFI_HANDLE                              ControllerHandle,
  IN CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINT32      val;
  UINT32      Index;
  UINT32      Count;
  UINT16      val_16;

  val  = MmioRead32 (Private->DbiBase + PCI_IO_BASE);
  val &= ~(IO_BASE_IO_DECODE | IO_BASE_IO_DECODE_BIT8);
  MmioWrite32 (Private->DbiBase + PCI_IO_BASE, val);

  val  = MmioRead32 (Private->DbiBase + PCI_PREF_MEMORY_BASE);
  val |= CFG_PREF_MEM_LIMIT_BASE_MEM_DECODE;
  val |= CFG_PREF_MEM_LIMIT_BASE_MEM_LIMIT_DECODE;
  MmioWrite32 (Private->DbiBase + PCI_PREF_MEMORY_BASE, val);

  /* Enable as 0xFFFF0001 response for CRS */
  val  = MmioRead32 (Private->DbiBase + PORT_LOGIC_AMBA_ERROR_RESPONSE_DEFAULT);
  val &= ~(AMBA_ERROR_RESPONSE_CRS_MASK << AMBA_ERROR_RESPONSE_CRS_SHIFT);
  val |= (AMBA_ERROR_RESPONSE_CRS_OKAY_FFFF0001 << AMBA_ERROR_RESPONSE_CRS_SHIFT);
  MmioWrite32 (Private->DbiBase + PORT_LOGIC_AMBA_ERROR_RESPONSE_DEFAULT, val);

  /* Reduce the CBB Timeout value to 7ms */
  val  = MmioRead32 (Private->DbiBase + PORT_LOGIC_AMBA_LINK_TIMEOUT);
  val &= ~AMBA_LINK_TIMEOUT_PERIOD_MASK;
  val |= AMBA_LINK_TIMEOUT_PERIOD_VAL;
  MmioWrite32 (Private->DbiBase + PORT_LOGIC_AMBA_LINK_TIMEOUT, val);

  /* Set the Completion Timeout value in 1ms~10ms range */
  val_16  = MmioRead16 (Private->DbiBase + PCI_EXP_DEVCTL_STS_2);
  val_16 &= ~PCI_EXP_DEVCTL_STS_2_CPL_TO_MASK;
  val_16 |= PCI_EXP_DEVCTL_STS_2_CPL_TO_VAL;
  MmioWrite16 (Private->DbiBase + PCI_EXP_DEVCTL_STS_2, val_16);

  /* Configure Max lane width from DT */
  val  = MmioRead32 (Private->DbiBase + PCI_EXP_LNKCAP);
  val &= ~PCI_EXP_LNKCAP_MLW;
  val |= (Private->NumLanes << PCI_EXP_LNKSTA_NLW_SHIFT);
  MmioWrite32 (Private->DbiBase + PCI_EXP_LNKCAP, val);

  /* Clear Slot Clock Configuration bit if SRNS configuration */
  if (Private->EnableSRNS) {
    val  = MmioRead32 (Private->DbiBase + PCI_EXP_LNKCTL_STATUS);
    val &= PCI_EXP_LNKCTL_STATUS_SLOT_CLOCK_CONFIG;
    MmioWrite32 (Private->DbiBase + PCI_EXP_LNKCTL_STATUS, val);
  }

  config_gen3_gen4_eq_presets (Private);

  /* Disable ASPM sub-states (L1.1 & L1.2) as we have removed dependency on CLKREQ signal */
  val  = MmioRead32 (Private->DbiBase + Private->ASPML1SSCapOffset);
  val &= ~PCI_L1SS_CAP_ASPM_L1_1;
  val &= ~PCI_L1SS_CAP_ASPM_L1_2;
  MmioWrite32 (Private->DbiBase + Private->ASPML1SSCapOffset, val);

  if (Private->UpdateFCFixUp) {
    val  = MmioRead32 (Private->DbiBase + CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF);
    val |= 0x1 << CFG_TIMER_CTRL_ACK_NAK_SHIFT;
    MmioWrite32 (Private->DbiBase + CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF, val);
  }

  /* Configure Max speed from DT */
  val  = MmioRead32 (Private->DbiBase + PCI_EXP_LNKCAP);
  val &= ~PCI_EXP_LNKCAP_SLS;
  val |= Private->MaxLinkSpeed;
  MmioWrite32 (Private->DbiBase + PCI_EXP_LNKCAP, val);

  val  = MmioRead32 (Private->DbiBase + PCI_EXP_LNKCTL_STS_2);
  val &= ~PCI_EXP_LNKCAP_SLS;
  val |= Private->MaxLinkSpeed;
  MmioWrite32 (Private->DbiBase + PCI_EXP_LNKCTL_STS_2, val);

  /* Configure Gen1 N_FTS */
  val  = MmioRead32 (Private->DbiBase + PORT_LOGIC_ACK_F_ASPM_CTRL);
  val &= ~((N_FTS_MASK << N_FTS_SHIFT) | (N_FTS_MASK << CC_N_FTS_SHIFT));
  val |= ((N_FTS_VAL << N_FTS_SHIFT) | (N_FTS_VAL << CC_N_FTS_SHIFT));
  MmioWrite32 (Private->DbiBase + PORT_LOGIC_ACK_F_ASPM_CTRL, val);

  /* Configure Gen2+ N_FTS */
  val  = MmioRead32 (Private->DbiBase + PORT_LOGIC_GEN2_CTRL);
  val &= ~FTS_MASK;
  if (Private->IsT194) {
    val |= 52;
  }

  if (Private->IsT234) {
    val |= 80;
  }

  MmioWrite32 (Private->DbiBase + PORT_LOGIC_GEN2_CTRL, val);

  val  = MmioRead32 (Private->DbiBase + PCIE_PORT_LINK_CONTROL);
  val &= ~PORT_LINK_FAST_LINK_MODE;
  val |= PORT_LINK_DLL_LINK_EN;
  /* Set number of lanes */
  val &= ~PORT_LINK_CAP_MASK;
  switch (Private->NumLanes) {
    case 1:
      val |= (0x1 << PORT_LINK_CAP_SHIFT);
      break;
    case 2:
      val |= (0x3 << PORT_LINK_CAP_SHIFT);
      break;
    case 4:
      val |= (0x7 << PORT_LINK_CAP_SHIFT);
      break;
    case 8:
      val |= (0xF << PORT_LINK_CAP_SHIFT);
      break;
    default:
      DEBUG ((EFI_D_ERROR, "Invalid num-lanes %u, Setting default to '1'\r\n", Private->NumLanes));
      val |= (0x1 << PORT_LINK_CAP_SHIFT);
  }

  MmioWrite32 (Private->DbiBase + PCIE_PORT_LINK_CONTROL, val);

  val  = MmioRead32 (Private->DbiBase + PORT_LOGIC_GEN2_CTRL);
  val &= ~PORT_LOGIC_LINK_WIDTH_MASK;
  switch (Private->NumLanes) {
    case 1:
      val |= (0x1 << PORT_LOGIC_LINK_WIDTH_SHIFT);
      break;
    case 2:
      val |= (0x2 << PORT_LOGIC_LINK_WIDTH_SHIFT);
      break;
    case 4:
      val |= (0x4 << PORT_LOGIC_LINK_WIDTH_SHIFT);
      break;
    case 8:
      val |= (0x8 << PORT_LOGIC_LINK_WIDTH_SHIFT);
      break;
    default:
      val |= (0x1 << PORT_LOGIC_LINK_WIDTH_SHIFT);
  }

  MmioWrite32 (Private->DbiBase + PORT_LOGIC_GEN2_CTRL, val);

  /* Setup RC BARs */
  MmioWrite32 (Private->DbiBase + PCI_BASE_ADDRESS_0, 0);
  MmioWrite32 (Private->DbiBase + PCI_BASE_ADDRESS_1, 0);

  /* setup interrupt pins */
  MmioAndThenOr32 (Private->DbiBase + PCI_INT_LINE_OFFSET, 0xffff00ff, 0x00000100);

  /* setup bus numbers */
  MmioAndThenOr32 (Private->DbiBase + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, 0xff000000, 0x00ff0100);

  /* setup command register */
  MmioAndThenOr32 (
    Private->DbiBase + PCI_COMMAND_OFFSET,
    0xffff0000,
    EFI_PCI_COMMAND_IO_SPACE |
    EFI_PCI_COMMAND_MEMORY_SPACE |
    EFI_PCI_COMMAND_BUS_MASTER |
    EFI_PCI_COMMAND_SERR
    );

  /* Program correct class for RC */
  MmioWrite32 (
    Private->DbiBase + PCI_REVISION_ID_OFFSET,
    (PCI_CLASS_BRIDGE << 24) |
    (PCI_CLASS_BRIDGE_P2P << 16) |
    (PCI_IF_BRIDGE_P2P << 8) |
    0xa1
    );

  /* Enable Direct Speed Change */
  val  = MmioRead32 (Private->DbiBase + PORT_LOGIC_GEN2_CTRL);
  val |= PORT_LOGIC_GEN2_CTRL_DIRECT_SPEED_CHANGE;
  MmioWrite32 (Private->DbiBase + PORT_LOGIC_GEN2_CTRL, val);

  /* Disable write permission to DBI_RO_WR_EN protected registers */
  MmioAnd32 (Private->DbiBase + PCIE_MISC_CONTROL_1_OFF, ~PCIE_DBI_RO_WR_EN);

  DEBUG ((EFI_D_INFO, "Programming CORE registers is done\r\n"));

  Count = sizeof (CoreClockNames)/sizeof (CoreClockNames[0]);
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
  val  = MmioRead32 (Private->ApplSpace + 0x0);
  val &= ~(0x1);
  MmioWrite32 (Private->ApplSpace + 0x0, val);

  MicroSecondDelay (1000);

  /* enable LTSSM */
  val  = MmioRead32 (Private->ApplSpace + 0x4);
  val |= (0x1 << 7);
  MmioWrite32 (Private->ApplSpace + 0x4, val);

  /* de-assert RST */
  val  = MmioRead32 (Private->ApplSpace + 0x0);
  val |= (0x1);
  MmioWrite32 (Private->ApplSpace + 0x0, val);

  MicroSecondDelay (200000);

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
EFIAPI
CheckLinkUp (
  PCIE_CONTROLLER_PRIVATE  *Private
  )
{
  UINT32  val;

  val = MmioRead32 (Private->DbiBase + PCI_EXP_LNKCTL_STATUS);
  if (val & PCI_EXP_LNKCTL_STATUS_DLL_ACTIVE) {
    Private->LinkUp = TRUE;
    DEBUG ((
      EFI_D_INFO,
      "PCIe Controller-%d Link is UP (Speed: %d)\r\n",
      Private->CtrlId,
      (val & 0xf0000) >> 16
      ));
  } else {
    Private->LinkUp = FALSE;
    DEBUG ((EFI_D_ERROR, "PCIe Controller-%d Link is DOWN\r\n", Private->CtrlId));
  }

  return Private->LinkUp;
}

STATIC
BOOLEAN
EFIAPI
IsAGXXavier (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT32      NumberOfPlatformNodes;

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,p2972-0000", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,galen", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  return FALSE;
}

STATIC
EFI_STATUS
EFIAPI
InitializeController (
  PCIE_CONTROLLER_PRIVATE                     *Private,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                Status;
  UINT32                    val;
  UINT32                    Index;
  UINT32                    Count;
  NVIDIA_TEGRAP2U_PROTOCOL  *P2U         = NULL;
  CONST VOID                *Property    = NULL;
  INT32                     PropertySize = 0;

  /* Deassert powergate nodes */
  Status = AssertPgNodes (ControllerHandle, FALSE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

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
  if (EFI_ERROR (Status) || (P2U == NULL)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get gNVIDIATegraP2UProtocolGuid Handle: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  Property = fdt_getprop (
               DeviceTreeNode->DeviceTreeBase,
               DeviceTreeNode->NodeOffset,
               "phys",
               &PropertySize
               );
  if (Property == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get P2U PHY entries\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < PropertySize; Index += sizeof (UINT32)) {
    UINT32  P2UId;

    CopyMem ((VOID *)&P2UId, Property + Index, sizeof (UINT32));
    P2UId = SwapBytes32 (P2UId);

    if (EFI_ERROR (P2U->Init (P2U, P2UId))) {
      DEBUG ((EFI_D_ERROR, "Failed to Initialize P2U\n"));
    }
  }

  /* Program APPL */

  if (Private->IsT234) {
    /* Enable HW_HOT_RST mode */
    val  = MmioRead32 (Private->ApplSpace + APPL_CTRL);
    val &= ~(APPL_CTRL_HW_HOT_RST_MODE_MASK <<
             APPL_CTRL_HW_HOT_RST_MODE_SHIFT);
    val |= (APPL_CTRL_HW_HOT_RST_MODE_IMDT_RST_LTSSM_EN <<
            APPL_CTRL_HW_HOT_RST_MODE_SHIFT);
    val |= APPL_CTRL_HW_HOT_RST_EN;
    MmioWrite32 (Private->ApplSpace + APPL_CTRL, val);
  }

  /* Setup DBI region */
  MmioWrite32 (Private->ApplSpace + APPL_CFG_BASE_ADDR, Private->DbiBase & APPL_CFG_BASE_ADDR_MASK);

  /* configure this core for RP mode operation */
  MmioWrite32 (Private->ApplSpace + APPL_DM_TYPE, APPL_DM_TYPE_RP);

  MmioWrite32 (Private->ApplSpace + APPL_CFG_SLCG_OVERRIDE, 0x0);

  val = MmioRead32 (Private->ApplSpace + APPL_CTRL);
  MmioWrite32 (Private->ApplSpace + APPL_CTRL, val | APPL_CTRL_SYS_PRE_DET_STATE);

  val  = MmioRead32 (Private->ApplSpace + APPL_CFG_MISC);
  val |= (APPL_CFG_MISC_ARCACHE_VAL << APPL_CFG_MISC_ARCACHE_SHIFT);
  MmioWrite32 (Private->ApplSpace + APPL_CFG_MISC, val);

  /* Programming the following to avoid dependency on CLKREQ */
  val  = MmioRead32 (Private->ApplSpace + APPL_PINMUX);
  val |= APPL_PINMUX_CLKREQ_OVERRIDE_EN;
  val &= ~APPL_PINMUX_CLKREQ_OVERRIDE;
  MmioWrite32 (Private->ApplSpace + APPL_PINMUX, val);

  if (Private->EnableSRNS || Private->EnableExtREFCLK) {
    /*
     * When Tegra PCIe RP is using external clock, it cannot
     * supply same clock back to EP, which makes it separate clock.
     * Gate PCIe RP REFCLK out pads when RP & EP are using separate
     * clock or RP is using external REFCLK.
     */
    val  = MmioRead32 (Private->ApplSpace + APPL_PINMUX);
    val |= APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE_EN;
    val &= ~APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE;
    MmioWrite32 (Private->ApplSpace + APPL_PINMUX, val);
  }

  if (Private->IsT234) {
    /* Configure ECAM */
    MmioWrite32 (Private->ApplSpace + APPL_ECAM_REGION_LOWER_BASE, lower_32_bits (Private->EcamBase));
    MmioWrite32 (Private->ApplSpace + APPL_ECAM_REGION_UPPER_BASE, upper_32_bits (Private->EcamBase));
    if (Private->EcamSize < SZ_256M) {
      val  = MmioRead32 (Private->ApplSpace + APPL_ECAM_CONFIG_BASE);
      val &= ~APPL_ECAM_CONFIG_LIMIT;
      val |= Private->EcamSize - 1;
      MmioWrite32 (Private->ApplSpace + APPL_ECAM_CONFIG_BASE, val);
    }

    val  = MmioRead32 (Private->ApplSpace + APPL_ECAM_CONFIG_BASE);
    val |= APPL_ECAM_CONFIG_REGION_EN;
    MmioWrite32 (Private->ApplSpace + APPL_ECAM_CONFIG_BASE, val);
  }

  if (Private->EnableGicV2m) {
    val = lower_32_bits (Private->GicBase + V2M_MSI_SETSPI_NS);
    MmioWrite32 (Private->ApplSpace + APPL_SEC_EXTERNAL_MSI_ADDR_L, val);
    val = upper_32_bits (Private->GicBase + V2M_MSI_SETSPI_NS);
    MmioWrite32 (Private->ApplSpace + APPL_SEC_EXTERNAL_MSI_ADDR_H, val);

    val = lower_32_bits (Private->MsiBase);
    MmioWrite32 (Private->ApplSpace + APPL_SEC_INTERNAL_MSI_ADDR_L, val);
    val = upper_32_bits (Private->MsiBase);
    MmioWrite32 (Private->ApplSpace + APPL_SEC_INTERNAL_MSI_ADDR_H, val);
  }

  /* Setup DBI region */
  MmioWrite32 (Private->ApplSpace + APPL_CFG_IATU_DMA_BASE_ADDR, Private->AtuBase & APPL_CFG_IATU_DMA_BASE_ADDR_MASK);

  /* Enable interrupt generation for PCIe legacy interrupts (INTx) */
  val  = MmioRead32 (Private->ApplSpace + APPL_INTR_EN_L0_0);
  val |= APPL_INTR_EN_L0_0_INT_INT_EN;
  val |= APPL_INTR_EN_L0_0_SYS_INTR_EN;
  MmioWrite32 (Private->ApplSpace + APPL_INTR_EN_L0_0, val);

  val  = MmioRead32 (Private->ApplSpace + APPL_INTR_EN_L1_8_0);
  val |= APPL_INTR_EN_L1_8_INTX_EN;
  MmioWrite32 (Private->ApplSpace + APPL_INTR_EN_L1_8_0, val);

  DEBUG ((EFI_D_INFO, "Programming APPL registers is done\r\n"));

  /* De-assert reset to CORE */
  Count = sizeof (CoreResetNames)/sizeof (CoreResetNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryConfigReset (
               Private->ControllerHandle,
               CoreResetNames[Index],
               0
               );
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

  Private->PcieCapOffset = dw_pcie_find_capability (
                             Private->DbiBase,
                             EFI_PCI_CAPABILITY_ID_PCIEXP
                             );

  val = dw_pcie_find_ext_capability (
          Private->DbiBase,
          PCI_EXPRESS_EXTENDED_CAPABILITY_L1_PM_SUBSTATES_ID
          );
  Private->ASPML1SSCapOffset = val + PCI_L1SS_CAP;

  Status = PrepareHost (Private, ControllerHandle, DeviceTreeNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Unable to Prepare Host controller (%r)\r\n", Status));
    return Status;
  }

  if (!CheckLinkUp (Private)) {
    UINT32  tmp;
    UINT32  offset;

    /*
    * There are some endpoints which can't get the link up if
    * root port has Data Link Feature (DLF) enabled.
    * Refer Spec rev 4.0 ver 1.0 sec 3.4.2 & 7.7.4 for more info
    * on Scaled Flow Control and DLF.
    * So, need to confirm that is indeed the case here and attempt
    * link up once again with DLF disabled.
    */
    val   = MmioRead32 (Private->ApplSpace + APPL_DEBUG);
    val  &= APPL_DEBUG_LTSSM_STATE_MASK;
    val >>= APPL_DEBUG_LTSSM_STATE_SHIFT;
    tmp   = MmioRead32 (Private->ApplSpace + APPL_LINK_STATUS);
    tmp  &= APPL_LINK_STATUS_RDLH_LINK_UP;
    if (!((val == 0x11) && !tmp)) {
      /* Link is down for all good reasons */
      goto exit;
    }

    DEBUG ((EFI_D_INFO, "Link is down in DLL"));
    DEBUG ((EFI_D_INFO, "Trying again with DLFE disabled\n"));

    /* Disable LTSSM */
    val  = MmioRead32 (Private->ApplSpace + APPL_CTRL);
    val &= ~APPL_CTRL_LTSSM_EN;
    MmioWrite32 (Private->ApplSpace + APPL_CTRL, val);

    /* Assert reset to CORE */
    Count = sizeof (CoreResetNames)/sizeof (CoreResetNames[0]);
    for (Index = 0; Index < Count; Index++) {
      Status = DeviceDiscoveryConfigReset (
                 Private->ControllerHandle,
                 CoreResetNames[Index],
                 1
                 );
      if (!EFI_ERROR (Status)) {
        DEBUG ((EFI_D_INFO, "Asserted Core reset\r\n"));
        break;
      }
    }

    if (Index == Count) {
      DEBUG ((EFI_D_ERROR, "Failed to Assert Core reset\r\n"));
      return Status;
    }

    /* De-assert reset to CORE */
    Count = sizeof (CoreResetNames)/sizeof (CoreResetNames[0]);
    for (Index = 0; Index < Count; Index++) {
      Status = DeviceDiscoveryConfigReset (
                 Private->ControllerHandle,
                 CoreResetNames[Index],
                 0
                 );
      if (!EFI_ERROR (Status)) {
        DEBUG ((EFI_D_INFO, "De-asserted Core reset\r\n"));
        break;
      }
    }

    if (Index == Count) {
      DEBUG ((EFI_D_ERROR, "Failed to De-assert Core reset\r\n"));
      return Status;
    }

    offset = dw_pcie_find_ext_capability (Private->DbiBase, PCI_EXT_CAP_ID_DLF);
    val    = MmioRead32 (Private->DbiBase + offset + PCI_DLF_CAP);
    val   &= ~PCI_DLF_EXCHANGE_ENABLE;
    MmioWrite32 (Private->DbiBase + offset + PCI_DLF_CAP, val);

    Status = PrepareHost (Private, ControllerHandle, DeviceTreeNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Unable to Prepare Host controller (%r)\r\n", Status));
      return Status;
    }

    CheckLinkUp (Private);
  }

exit:
  return EFI_SUCCESS;
}

EFI_STATUS
BpmpProcessSetCtrlState (
  IN NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol,
  IN UINT32                    BpmpPhandle,
  IN UINT32                    CtrlId,
  IN BOOLEAN                   State
  )
{
  EFI_STATUS               Status;
  struct mrq_uphy_request  Request;

  if (BpmpIpcProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Request.cmd                              = 4;
  Request.controller_state.pcie_controller = CtrlId;
  Request.controller_state.enable          = State;

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpPhandle,
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

STATIC
BOOLEAN
TegraPcieTryLinkL2 (
  PCIE_CONTROLLER_PRIVATE  *Private
  )
{
  UINT32  val;

  val  = MmioRead32 (Private->ApplSpace + APPL_RADM_STATUS);
  val |= APPL_PM_XMT_TURNOFF_STATE;
  MmioWrite32 (Private->ApplSpace + APPL_RADM_STATUS, val);

  MicroSecondDelay (10000);

  val = MmioRead32 (Private->ApplSpace + APPL_DEBUG);
  if (val & APPL_DEBUG_PM_LINKST_IN_L2_LAT) {
    return 0;
  } else {
    return 1;
  }
}

STATIC
VOID
TegraPciePMETurnOff (
  PCIE_CONTROLLER_PRIVATE  *Private
  )
{
  UINT32  data;

  if (!Private->LinkUp) {
    DEBUG ((
      EFI_D_INFO,
      "PCIe Controller-%d Link is not UP\r\n",
      Private->CtrlId
      ));

    data  = MmioRead32 (Private->ApplSpace + APPL_CTRL);
    data &= ~APPL_CTRL_LTSSM_EN;
    MmioWrite32 (Private->ApplSpace + APPL_CTRL, data);

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
    data  = MmioRead32 (Private->ApplSpace + APPL_PINMUX);
    data &= ~APPL_PINMUX_PEX_RST;
    MmioWrite32 (Private->ApplSpace + APPL_PINMUX, data);

    MicroSecondDelay (120000);

    data = MmioRead32 (Private->ApplSpace + APPL_DEBUG);
    if (!(
          ((data & APPL_DEBUG_LTSSM_STATE_MASK) == LTSSM_STATE_DETECT_QUIET) ||
          ((data & APPL_DEBUG_LTSSM_STATE_MASK) == LTSSM_STATE_DETECT_ACT) ||
          ((data & APPL_DEBUG_LTSSM_STATE_MASK) == LTSSM_STATE_PRE_DETECT_QUIET) ||
          ((data & APPL_DEBUG_LTSSM_STATE_MASK) == LTSSM_STATE_DETECT_WAIT))
        )
    {
      DEBUG ((EFI_D_ERROR, "Link didn't go to detect state as well\r\n"));
    }

    data  = MmioRead32 (Private->ApplSpace + APPL_CTRL);
    data &= ~APPL_CTRL_LTSSM_EN;
    MmioWrite32 (Private->ApplSpace + APPL_CTRL, data);
  }

  /*
   * DBI registers may not be accessible after this as PLL-E would be
   * down depending on how CLKREQ is pulled by end point
   */
  data  = MmioRead32 (Private->ApplSpace + APPL_PINMUX);
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
  IN  EFI_HANDLE  Handle
  )
{
  EFI_STATUS               Status;
  UINT32                   Index;
  UINT32                   Count;
  PCIE_CONTROLLER_PRIVATE  *Private = (PCIE_CONTROLLER_PRIVATE *)Handle;

  TegraPciePMETurnOff (Private);

  /* Assert reset to CORE */
  Count = sizeof (CoreResetNames)/sizeof (CoreResetNames[0]);
  for (Index = 0; Index < Count; Index++) {
    Status = DeviceDiscoveryConfigReset (
               Private->ControllerHandle,
               CoreResetNames[Index],
               1
               );
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
    Status = DeviceDiscoveryConfigReset (
               Private->ControllerHandle,
               CoreAPBResetNames[Index],
               1
               );
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
    Status = DeviceDiscoveryEnableClock (
               Private->ControllerHandle,
               CoreClockNames[Index],
               0
               );
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "Disabled Core clock\r\n"));
      break;
    }
  }

  if (Index == Count) {
    DEBUG ((EFI_D_ERROR, "Failed to Disable core_clk\r\n"));
    return Status;
  }

  if (!((Private->CtrlId == 5) && Private->IsT194)) {
    if (PcdGetBool (PcdBPMPPCIeControllerEnable)) {
      Status = BpmpProcessSetCtrlState (Private->BpmpIpcProtocol, Private->BpmpPhandle, Private->CtrlId, 0);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "Failed to disable Controller-%u\n", Private->CtrlId));
        return Status;
      }

      DEBUG ((EFI_D_INFO, "Disabled Controller-%u through BPMP-FW\n", Private->CtrlId));
    }
  }

  /* Assert powergate nodes */
  Status = AssertPgNodes (Private->ControllerHandle, TRUE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
ParseGicMsiBase (
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *CONST  DeviceTreeNode,
  OUT       UINT64                           *CONST  GicBase,
  OUT       UINT64                           *CONST  MsiBase
  )
{
  CONST VOID  *Property;
  INT32       PropertySize;
  UINT32      MsiParentPhandle;
  INTN        MsiParentOffset;

  Property = fdt_getprop (
               DeviceTreeNode->DeviceTreeBase,
               DeviceTreeNode->NodeOffset,
               "msi-parent",
               &PropertySize
               );
  if (Property == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: cannot retrieve property 'msi-parent': %a\r\n",
      __FUNCTION__,
      fdt_strerror (PropertySize)
      ));
    return FALSE;
  } else if (PropertySize != 2 * sizeof (UINT32)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid size of property 'msi-parent': expected %u, got %d\r\n",
      __FUNCTION__,
      (UINTN)2 * sizeof (UINT32),
      (INTN)PropertySize
      ));
    return FALSE;
  }

  MsiParentPhandle = SwapBytes32 (*(UINT32 *)Property);

  MsiParentOffset = fdt_node_offset_by_phandle (
                      DeviceTreeNode->DeviceTreeBase,
                      MsiParentPhandle
                      );
  if (MsiParentOffset < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to locate GICv2m node by phandle 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)MsiParentPhandle,
      fdt_strerror (MsiParentOffset)
      ));
    return FALSE;
  }

  PropertySize = fdt_node_check_compatible (
                   DeviceTreeNode->DeviceTreeBase,
                   MsiParentOffset,
                   "arm,gic-v2m-frame"
                   );
  if (PropertySize < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to check GICv2m compatibility: %a\r\n",
      __FUNCTION__,
      fdt_strerror (PropertySize)
      ));
    return FALSE;
  } else if (PropertySize != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: GICv2m not compatible\r\n",
      __FUNCTION__
      ));
    return FALSE;
  }

  Property = fdt_getprop (
               DeviceTreeNode->DeviceTreeBase,
               MsiParentOffset,
               "reg",
               &PropertySize
               );
  if (Property == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: cannot retrieve GICv2m property 'reg': %a\r\n",
      __FUNCTION__,
      fdt_strerror (PropertySize)
      ));
    return FALSE;
  } else if (PropertySize != 4 * sizeof (UINT64)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid size of GICv2m property 'reg': expected %u, got %d\r\n",
      __FUNCTION__,
      (UINTN)4 * sizeof (UINT64),
      (INTN)PropertySize
      ));
    return FALSE;
  }

  *GicBase = SwapBytes64 (*(UINT64 *)Property);
  *MsiBase = SwapBytes64 (*((UINT64 *)Property + 2));
  return TRUE;
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
  EFI_STATUS  Status;
  VOID        *Rsdp = NULL;

  gBS->CloseEvent (Event);

  // Only Uninitialize if ACPI is not installed.
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &Rsdp);
  if (EFI_ERROR (Status)) {
    UninitializeController ((EFI_HANDLE)Context);
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                 Status;
  PCI_ROOT_BRIDGE            *RootBridge       = NULL;
  EFI_DEVICE_PATH_PROTOCOL   *ParentDevicePath = NULL;
  CONST VOID                 *BusProperty      = NULL;
  CONST VOID                 *RangesProperty   = NULL;
  INT32                      PropertySize      = 0;
  INT32                      AddressCells;
  INT32                      PciAddressCells;
  INT32                      SizeCells;
  INT32                      RangeSize;
  CONST VOID                 *SegmentNumber = NULL;
  CONST VOID                 *ControllerId  = NULL;
  CONST VOID                 *BpmpPhandle   = NULL;
  PCIE_CONTROLLER_PRIVATE    *Private       = NULL;
  EFI_EVENT                  ExitBootServiceEvent;
  NVIDIA_REGULATOR_PROTOCOL  *Regulator = NULL;
  CONST VOID                 *Property  = NULL;
  UINT32                     Val;
  UINTN                      ChipID;
  UINT32                     DeviceTreeHandle;
  UINT32                     NumberOfInterrupts;
  UINT32                     Index;
  UINT32                     Index2;
  BOOLEAN                    RegisterConfigurationData;
  BOOLEAN                    PcieFound;
  CONST UINT32               *InterruptMap;

  NumberOfInterrupts        = 2;
  RegisterConfigurationData = TRUE;
  Status                    = EFI_SUCCESS;
  PcieFound                 = FALSE;

  switch (Phase) {
    case DeviceDiscoveryDriverStart:

      for (Index = 0; Index < (sizeof (gDeviceCompatibilityMap) / sizeof (gDeviceCompatibilityMap[0])); Index++) {
        if (gDeviceCompatibilityMap[Index].Compatibility != NULL) {
          Val    = 0;
          Status = GetMatchingEnabledDeviceTreeNodes (gDeviceCompatibilityMap[Index].Compatibility, NULL, &Val);
          if (Status == EFI_BUFFER_TOO_SMALL) {
            PcieFound = TRUE;
            break;
          }
        }
      }

      Status = EFI_SUCCESS;
      if (!PcieFound) {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &DriverHandle,
                        &gNVIDIAConfigurationManagerDataObjectGuid,
                        NULL,
                        NULL
                        );
      }

      break;

    case DeviceDiscoveryDriverBindingStart:
      RootBridge = AllocateZeroPool (sizeof (PCI_ROOT_BRIDGE));
      if (RootBridge == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate device bridge structure\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        goto ErrorExit;
      }

      Private = AllocateZeroPool (sizeof (PCIE_CONTROLLER_PRIVATE));
      if (Private == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate private structure\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        goto ErrorExit;
      }

      ChipID = TegraGetChipID ();
      if (ChipID == T234_CHIP_ID) {
        Private->IsT234 = TRUE;
      } else if (ChipID == T194_CHIP_ID) {
        Private->IsT194 = TRUE;
      }

      Private->ControllerHandle = ControllerHandle;

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &Private->ApplSpace, &Private->ApplSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate appl address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &Private->ConfigurationSpace, &Private->ConfigurationSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate configuration address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 2, &Private->AtuBase, &Private->AtuSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate ATU address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 3, &Private->DbiBase, &Private->DbiSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate DBI address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }

      if (Private->IsT234) {
        Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 4, &Private->EcamBase, &Private->EcamSize);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Unable to locate ECAM address range\n", __FUNCTION__));
          Status = EFI_UNSUPPORTED;
          break;
        }
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

      DEBUG ((EFI_D_INFO, "Segment Number = %u\n", Private->PcieRootBridgeConfigurationIo.SegmentNumber));

      Private->CtrlId = Private->PcieRootBridgeConfigurationIo.SegmentNumber;

      ControllerId = fdt_getprop (
                       DeviceTreeNode->DeviceTreeBase,
                       DeviceTreeNode->NodeOffset,
                       "nvidia,controller-id",
                       &PropertySize
                       );
      if ((ControllerId == NULL) || (PropertySize != 2*sizeof (UINT32))) {
        DEBUG ((DEBUG_ERROR, "Failed to read controller number\n"));
      } else {
        CopyMem (&Private->CtrlId, ControllerId + sizeof (UINT32), sizeof (UINT32));
        Private->CtrlId = SwapBytes32 (Private->CtrlId);
      }

      DEBUG ((EFI_D_INFO, "Controller-ID = %u\n", Private->CtrlId));

      BpmpPhandle = fdt_getprop (
                      DeviceTreeNode->DeviceTreeBase,
                      DeviceTreeNode->NodeOffset,
                      "nvidia,bpmp",
                      &PropertySize
                      );
      if ((BpmpPhandle == NULL) || (PropertySize < sizeof (UINT32))) {
        DEBUG ((DEBUG_ERROR, "Failed to get Bpmp node phandle.\n"));
        goto ErrorExit;
      } else {
        CopyMem (&Private->BpmpPhandle, BpmpPhandle, sizeof (UINT32));
        Private->BpmpPhandle = SwapBytes32 (Private->BpmpPhandle);
        DEBUG ((EFI_D_ERROR, "PCIE Controller ID-%u, Bpmp Phandle-%u\n", Private->CtrlId, Private->BpmpPhandle));
      }

      Property = fdt_getprop (
                   DeviceTreeNode->DeviceTreeBase,
                   DeviceTreeNode->NodeOffset,
                   "nvidia,max-speed",
                   &PropertySize
                   );
      if (Property != NULL) {
        CopyMem (&Private->MaxLinkSpeed, Property, sizeof (UINT32));
        Private->MaxLinkSpeed = SwapBytes32 (Private->MaxLinkSpeed);
      } else {
        Property = fdt_getprop (
                     DeviceTreeNode->DeviceTreeBase,
                     DeviceTreeNode->NodeOffset,
                     "max-link-speed",
                     &PropertySize
                     );
        if (Property != NULL) {
          CopyMem (&Private->MaxLinkSpeed, Property, sizeof (UINT32));
          Private->MaxLinkSpeed = SwapBytes32 (Private->MaxLinkSpeed);
        }
      }

      if ((Private->MaxLinkSpeed <= 0) || (Private->MaxLinkSpeed > 4)) {
        Private->MaxLinkSpeed = 4;
      }

      DEBUG ((EFI_D_INFO, "Max Link Speed = %u\n", Private->MaxLinkSpeed));

      Private->EnableGicV2m = ParseGicMsiBase (DeviceTreeNode, &Private->GicBase, &Private->MsiBase);
      if (Private->EnableGicV2m) {
        DEBUG ((DEBUG_INFO, "Enabling GICv2m\r\n"));
        DEBUG ((DEBUG_INFO, "GIC base = 0x%lx\r\n", Private->GicBase));
        DEBUG ((DEBUG_INFO, "MSI base = 0x%lx\r\n", Private->MsiBase));
      }

      Property = fdt_getprop (
                   DeviceTreeNode->DeviceTreeBase,
                   DeviceTreeNode->NodeOffset,
                   "num-lanes",
                   &PropertySize
                   );
      if (Property != NULL) {
        CopyMem (&Private->NumLanes, Property, sizeof (UINT32));
        Private->NumLanes = SwapBytes32 (Private->NumLanes);
      }

      if ((Private->NumLanes != 1) && ((Private->NumLanes % 2) != 0) && (Private->NumLanes > 16)) {
        Private->NumLanes = 1;
      }

      DEBUG ((EFI_D_INFO, "Number of lanes = %u\n", Private->NumLanes));

      if (NULL != fdt_get_property (
                    DeviceTreeNode->DeviceTreeBase,
                    DeviceTreeNode->NodeOffset,
                    "nvidia,update-fc-fixup",
                    NULL
                    ))
      {
        Private->UpdateFCFixUp = TRUE;
      } else {
        Private->UpdateFCFixUp = FALSE;
      }

      /* Enable slot supplies */
      Status = gBS->LocateProtocol (&gNVIDIARegulatorProtocolGuid, NULL, (VOID **)&Regulator);
      if (EFI_ERROR (Status) || (Regulator == NULL)) {
        DEBUG ((EFI_D_ERROR, "%a: Couldn't get gNVIDIARegulatorProtocolGuid Handle: %r\n", __FUNCTION__, Status));
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }

      /* Get the vddio-pex-ctl supply */
      Property = fdt_getprop (
                   DeviceTreeNode->DeviceTreeBase,
                   DeviceTreeNode->NodeOffset,
                   "vddio-pex-ctl-supply",
                   &PropertySize
                   );
      if ((Property != NULL) && (PropertySize == sizeof (UINT32))) {
        Val = SwapBytes32 (*(UINT32 *)Property);
        /* Enable the vddio-pex-ctl supply */
        if (EFI_ERROR (Regulator->Enable (Regulator, Val, TRUE))) {
          DEBUG ((EFI_D_ERROR, "Failed to Enable vddio-pex-ctl supply regulator\n"));
        }
      } else {
        DEBUG ((EFI_D_INFO, "Failed to find vddio-pex-ctl supply regulator\n"));
      }

      /* Get the 3v3 supply */
      Property = fdt_getprop (
                   DeviceTreeNode->DeviceTreeBase,
                   DeviceTreeNode->NodeOffset,
                   "vpcie3v3-supply",
                   &PropertySize
                   );
      if ((Property != NULL) && (PropertySize == sizeof (UINT32))) {
        Val = SwapBytes32 (*(UINT32 *)Property);
        /* Enable the 3v3 supply */
        if (EFI_ERROR (Regulator->Enable (Regulator, Val, TRUE))) {
          DEBUG ((EFI_D_ERROR, "Failed to Enable 3v3 Regulator\n"));
        }
      } else {
        DEBUG ((EFI_D_INFO, "Failed to find 3v3 slot supply regulator\n"));
      }

      /* Get the 12v supply */
      Property = fdt_getprop (
                   DeviceTreeNode->DeviceTreeBase,
                   DeviceTreeNode->NodeOffset,
                   "vpcie12v-supply",
                   &PropertySize
                   );
      if ((Property != NULL) && (PropertySize == sizeof (UINT32))) {
        Val = SwapBytes32 (*(UINT32 *)Property);
        /* Enable the 12v supply */
        if (EFI_ERROR (Regulator->Enable (Regulator, Val, TRUE))) {
          DEBUG ((EFI_D_ERROR, "Failed to Enable 12v Regulator\n"));
        }
      } else {
        DEBUG ((EFI_D_INFO, "Failed to find 12v slot supply regulator\n"));
      }

      /* Spec defined T_PVPERL delay (100ms) after enabling power to the slot */
      MicroSecondDelay (100000);

      if ((Private->CtrlId == 5) && Private->IsT194) {
        ConfigureSidebandSignals (Private);
      } else {
        EFI_STATUS  Status;

        Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&Private->BpmpIpcProtocol);
        if (EFI_ERROR (Status)) {
          DEBUG ((EFI_D_ERROR, "Failed to get BPMP-FW handle\n"));
          Status = EFI_NOT_READY;
          goto ErrorExit;
        }

        if (PcdGetBool (PcdBPMPPCIeControllerEnable)) {
          Status = BpmpProcessSetCtrlState (Private->BpmpIpcProtocol, Private->BpmpPhandle, Private->CtrlId, 1);
          if (EFI_ERROR (Status)) {
            DEBUG ((EFI_D_ERROR, "Failed to Enable Controller-%u\n", Private->CtrlId));
            Status = EFI_NOT_READY;
            goto ErrorExit;
          }

          DEBUG ((EFI_D_INFO, "Enabled Controller-%u through BPMP-FW\n", Private->CtrlId));
        }
      }

      if (NULL != fdt_get_property (
                    DeviceTreeNode->DeviceTreeBase,
                    DeviceTreeNode->NodeOffset,
                    "nvidia,enable-srns",
                    NULL
                    ))
      {
        Private->EnableSRNS = TRUE;
      } else {
        Private->EnableSRNS = FALSE;
      }

      if (Private->IsT194) {
        Private->EnableExtREFCLK = FALSE;
      } else if (NULL != fdt_get_property (
                           DeviceTreeNode->DeviceTreeBase,
                           DeviceTreeNode->NodeOffset,
                           "nvidia,enable-ext-refclk",
                           NULL
                           ))
      {
        Private->EnableExtREFCLK = TRUE;
      } else {
        Private->EnableExtREFCLK = FALSE;
      }

      Status = InitializeController (Private, ControllerHandle, DeviceTreeNode);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Unable to initialize controller (%r)\r\n", __FUNCTION__, Status));
        goto ErrorExit;
      }

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      OnExitBootServices,
                      Private,
                      &gEfiEventExitBootServicesGuid,
                      &ExitBootServiceEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Unable to setup exit boot services uninitialize. (%r)\r\n", __FUNCTION__, Status));
        goto ErrorExit;
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
        goto ErrorExit;
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
        goto ErrorExit;
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
          goto ErrorExit;
        }

        Space        = Flags & PCIE_DEVICETREE_SPACE_CODE;
        Prefetchable = ((Flags & PCIE_DEVICETREE_PREFETCHABLE) == PCIE_DEVICETREE_PREFETCHABLE);
        Limit        = DeviceAddress + Size - 1;
        Translation  = DeviceAddress - HostAddress;

        if (Space == PCIE_DEVICETREE_SPACE_IO) {
          ASSERT (RootBridge->Io.Base == MAX_UINT64);
          RootBridge->Io.Base        = DeviceAddress;
          RootBridge->Io.Limit       = Limit;
          RootBridge->Io.Translation = Translation;
          ConfigureAtu (
            Private,
            PCIE_ATU_REGION_INDEX1,
            TEGRA_PCIE_ATU_TYPE_IO,
            HostAddress,
            DeviceAddress,
            Size
            );
          Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 1;
        } else if ((Space == PCIE_DEVICETREE_SPACE_MEM32) &&
                   (Limit < SIZE_4GB))
        {
          ASSERT (RootBridge->Mem.Base == MAX_UINT64);
          RootBridge->Mem.Base        = DeviceAddress;
          RootBridge->Mem.Limit       = Limit;
          RootBridge->Mem.Translation = Translation;
          ConfigureAtu (
            Private,
            PCIE_ATU_REGION_INDEX2,
            TEGRA_PCIE_ATU_TYPE_MEM,
            HostAddress,
            DeviceAddress,
            Size
            );
          Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 3;
        } else if ((((Space == PCIE_DEVICETREE_SPACE_MEM32) &&
                     (Limit >= SIZE_4GB)) ||
                    (Space == PCIE_DEVICETREE_SPACE_MEM64)))
        {
          ASSERT (RootBridge->MemAbove4G.Base == MAX_UINT64);
          RootBridge->MemAbove4G.Base        = DeviceAddress;
          RootBridge->MemAbove4G.Limit       = Limit;
          RootBridge->MemAbove4G.Translation = Translation;
          ConfigureAtu (
            Private,
            PCIE_ATU_REGION_INDEX3,
            TEGRA_PCIE_ATU_TYPE_MEM,
            HostAddress,
            DeviceAddress,
            Size
            );
          Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 3;
        } else {
          DEBUG ((EFI_D_ERROR, "PCIe Controller: Unknown region 0x%08x 0x%016llx-0x%016llx T 0x%016llx\r\n", Flags, DeviceAddress, Limit, Translation));
          ASSERT (FALSE);
          Status = EFI_DEVICE_ERROR;
          goto ErrorExit;
        }

        Private->AddressMapInfo[Private->AddressMapCount].PciAddress  = DeviceAddress;
        Private->AddressMapInfo[Private->AddressMapCount].CpuAddress  = HostAddress;
        Private->AddressMapInfo[Private->AddressMapCount].AddressSize = Size;
        Private->AddressMapCount++;
        RangesProperty += RangeSize;
        PropertySize   -= RangeSize;
      }

      if (EFI_ERROR (Status)) {
        goto ErrorExit;
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
        goto ErrorExit;
      }

      RootBridge->DevicePath = AppendDevicePathNode (
                                 ParentDevicePath,
                                 (EFI_DEVICE_PATH_PROTOCOL  *)&mPciRootBridgeDevicePathNode
                                 );

      // Setup configuration structure
      if (Private->EcamBase != 0) {
        Private->ConfigSpaceInfo.BaseAddress = Private->EcamBase;
      } else {
        Private->ConfigSpaceInfo.BaseAddress = Private->ConfigurationSpace;
      }

      Private->ConfigSpaceInfo.PciSegmentGroupNumber = Private->PcieRootBridgeConfigurationIo.SegmentNumber;
      if (Private->IsT194) {
        Private->ConfigSpaceInfo.StartBusNumber = T194_PCIE_BUS_MIN;
        Private->ConfigSpaceInfo.EndBusNumber   = T194_PCIE_BUS_MAX;
      } else if (Private->IsT234) {
        Private->ConfigSpaceInfo.StartBusNumber = T234_PCIE_BUS_MIN;
        Private->ConfigSpaceInfo.EndBusNumber   = T234_PCIE_BUS_MAX;
      } else {
        Private->ConfigSpaceInfo.StartBusNumber = Private->PcieRootBridgeConfigurationIo.MinBusNumber;
        Private->ConfigSpaceInfo.EndBusNumber   = Private->PcieRootBridgeConfigurationIo.MaxBusNumber;
      }

      Private->ConfigSpaceInfo.AddressMapToken   = REFERENCE_TOKEN (Private->AddressMapRefInfo);
      Private->ConfigSpaceInfo.InterruptMapToken = REFERENCE_TOKEN (Private->InterruptRefInfo);

      Status = GetDeviceTreeHandle (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, &DeviceTreeHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get device tree handle\r\n", __FUNCTION__));
        goto ErrorExit;
      }

      InterruptMap = (CONST UINT32 *)fdt_getprop (
                                       DeviceTreeNode->DeviceTreeBase,
                                       DeviceTreeNode->NodeOffset,
                                       "interrupt-map",
                                       &PropertySize
                                       );
      if ((InterruptMap == NULL) || ((PropertySize % PCIE_INTERRUPT_MAP_ENTRY_SIZE) != 0)) {
        Status = EFI_DEVICE_ERROR;
        DEBUG ((DEBUG_ERROR, "%a: Failed to get pcie interrupts, size = %d\r\n", __FUNCTION__, PropertySize));
        ASSERT (FALSE);
        break;
      }

      NumberOfInterrupts = PropertySize / PCIE_INTERRUPT_MAP_ENTRY_SIZE;
      if (NumberOfInterrupts == 1) {
        for (Index = 0; Index < PCIE_NUMBER_OF_INTERUPT_MAP; Index++) {
          Private->InterruptRefInfo[Index].ReferenceToken          = REFERENCE_TOKEN (Private->InterruptMapInfo[Index]);
          Private->InterruptMapInfo[Index].PciInterrupt            = Index;
          Private->InterruptMapInfo[Index].IntcInterrupt.Interrupt = SwapBytes32 (InterruptMap[PCIE_PARENT_INTERRUPT_OFFSET]) + SPI_OFFSET;
          Private->InterruptMapInfo[Index].IntcInterrupt.Flags     = BIT2;
        }
      } else if (NumberOfInterrupts == PCIE_NUMBER_OF_INTERUPT_MAP) {
        for (Index = 0; Index < PCIE_NUMBER_OF_INTERUPT_MAP; Index++) {
          Private->InterruptRefInfo[Index].ReferenceToken          = REFERENCE_TOKEN (Private->InterruptMapInfo[Index]);
          Private->InterruptMapInfo[Index].PciInterrupt            = SwapBytes32 (InterruptMap[(Index * PCIE_INTERRUPT_MAP_ENTRIES) + PCIE_CHILD_INT_OFFSET])-1;
          Private->InterruptMapInfo[Index].IntcInterrupt.Interrupt = SwapBytes32 (InterruptMap[(Index * PCIE_INTERRUPT_MAP_ENTRIES) + PCIE_PARENT_INTERRUPT_OFFSET]) + SPI_OFFSET;
          Private->InterruptMapInfo[Index].IntcInterrupt.Flags     = BIT2;
        }

        if (Private->IsT234) {
          MmioOr32 (Private->ApplSpace + APPL_PCIE_MISC0_BASE, APPL_PCIE_MISC0_INT_SEGREGATION_EN);
        }
      } else {
        Status = EFI_DEVICE_ERROR;
        DEBUG ((DEBUG_ERROR, "%a: Expected %d interrupts, got %d\r\n", __FUNCTION__, PCIE_NUMBER_OF_INTERUPT_MAP, NumberOfInterrupts));
        break;
      }

      for (Index = 0; Index < Private->AddressMapCount; Index++) {
        Private->AddressMapRefInfo[Index].ReferenceToken = REFERENCE_TOKEN (Private->AddressMapInfo[Index]);
      }

      // Limit configuration manager entries for T194 as it does not support ECAM so needs special OS support
      if (Private->IsT194) {
        if (PcdGet8 (PcdPcieEntryInAcpi) != 1) {
          RegisterConfigurationData = FALSE;
        }

        // Do not register segment that AHCI controller is on as this is exposed as a native ACPI device
        if (IsAGXXavier () && (Private->ConfigSpaceInfo.PciSegmentGroupNumber == AGX_XAVIER_AHCI_SEGMENT)) {
          RegisterConfigurationData = FALSE;
        }
      }

      if (RegisterConfigurationData) {
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
      }

ErrorExit:
      if (EFI_ERROR (Status)) {
        if (RootBridge != NULL) {
          FreePool (RootBridge);
        }

        if (Private != NULL) {
          FreePool (Private);
        }
      }

      break;

    case DeviceDiscoveryDriverBindingStop:

      Status = EFI_PROTOCOL_ERROR;
      DEBUG ((DEBUG_ERROR, "%a: Rejecting Driver Binding Stop (%r)\r\n", __FUNCTION__, Status));
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

  return Status;
}
