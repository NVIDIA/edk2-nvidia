/** @file

  PCIe Segment Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/FdtLib.h>
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
#include <Library/PcieLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/SortLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciIo.h>
#include <Protocol/C2CNodeProtocol.h>
#include <Protocol/ConfigurationManagerTokenProtocol.h>

#include <T410/T410Definitions.h>
#include "PcieSegmentCtlrControllerPrivate.h"
#include <Protocol/PciPlatform.h>

#define PCIE_CONTROLLER_MAX_REGISTERS  6

#define CONFIG_BASE_FROM_ECAM_BASE(ecam_base, bus, dev, func) \
  ((ecam_base) + (((UINT64)(bus) << 20) | ((UINT64)(dev) << 15) | ((UINT64)(func) << 12)))

STATIC BOOLEAN  mPcieDisableOptionRom = FALSE;

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,t410-pcie", &gNVIDIANonDiscoverableT410PcieDeviceGuid },
  { NULL,               NULL                                      }
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
  .DriverName                      = L"NVIDIA PCIe Segment Controller driver",
  .AutoEnableClocks                = FALSE,
  .AutoDeassertReset               = FALSE,
  .AutoDeassertPg                  = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .ThreadedDeviceStart             = TRUE
};

BOOLEAN                           gPciPltProtInstalled     = FALSE;
BOOLEAN                           gPciPltProtInstallFailed = FALSE;
extern EFI_PCI_PLATFORM_PROTOCOL  mPciPlatformProtocol;

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
  PCIE_RP_PRIVATE                              *RpInfo;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS  PciAddress;
  UINT64                                       ConfigAddress;
  UINT32                                       Register;
  UINT8                                        Length;
  UINT64                                       offset;

  //
  // Read Pci configuration space
  //
  RpInfo = RP_PRIVATE_DATA_FROM_THIS (This);
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
    // Disable option ROM address for non-root bridge
    if (mPcieDisableOptionRom &&
        (PciAddress.Bus != This->MinBusNumber) &&
        (Register == PCI_EXPANSION_ROM_BASE) &&
        Read)
    {
      SetMem (Buffer, Length, 0x00);
      return EFI_SUCCESS;
    }

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
      ConfigAddress = RpInfo->EcamBase + offset;

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
BOOLEAN
WaitForBit (
  UINT16   *Feat,
  UINT16   Pos,
  UINT32   Count,
  UINT32   TimeUs,
  BOOLEAN  Status
  )
{
  UINT32  Index = 0;

  while (Index < Count) {
    if (!!(*Feat & BIT (Pos)) != Status) {
      DeviceDiscoveryThreadMicroSecondDelay (TimeUs);
      Index++;
    } else {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
EFI_STATUS
EFIAPI
InitializeController (
  IN PCIE_CONTROLLER_PRIVATE  *Private,
  IN UINT32                   RpId
  )
{
  EFI_STATUS             Status;
  PCI_CAPABILITY_PCIEXP  *PciExpCap = NULL;
  UINT64                 ConfigBase;
  PCIE_RP_PRIVATE        *RpInfo = &Private->RpInfo[RpId];
  UINT32                 Count   = 1000;

  ConfigBase = CONFIG_BASE_FROM_ECAM_BASE (RpInfo->EcamBase, RpInfo->PcieRootBridgeConfigurationIo.MinBusNumber, 0, 0);

  /* Setup bus numbers */
  MmioWrite8 (ConfigBase + PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, RpInfo->PcieRootBridgeConfigurationIo.MinBusNumber);
  MmioWrite8 (ConfigBase + PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET, RpInfo->PcieRootBridgeConfigurationIo.MinBusNumber + 1);
  MmioWrite8 (ConfigBase + PCI_BRIDGE_SUBORDINATE_BUS_REGISTER_OFFSET, RpInfo->PcieRootBridgeConfigurationIo.MaxBusNumber);

  /* Stup command register */
  MmioAndThenOr32 (
    ConfigBase + PCI_COMMAND_OFFSET,
    0xffff0000,
    EFI_PCI_COMMAND_IO_SPACE |
    EFI_PCI_COMMAND_MEMORY_SPACE |
    EFI_PCI_COMMAND_BUS_MASTER |
    EFI_PCI_COMMAND_SERR
    );

  RpInfo->PCIeCapOff = 0;
  Status             = PcieLibLocatePciCapabilityViaConfig (
                         ConfigBase,
                         EFI_PCI_CAPABILITY_ID_PCIEXP,
                         (UINT8 *)&RpInfo->PCIeCapOff,
                         NULL
                         );
  if (EFI_ERROR (Status) || !RpInfo->PCIeCapOff) {
    DEBUG ((DEBUG_VERBOSE, "Failed to find PCIe capability registers\r\n"));
    return EFI_NOT_FOUND;
  }

  // TODO: de-assert PERST and inform PXIR-FW

  /* Wait for link up */
  PciExpCap = (PCI_CAPABILITY_PCIEXP *)(ConfigBase + RpInfo->PCIeCapOff);

  if ((TegraGetPlatform () == TEGRA_PLATFORM_VDK) || (TegraGetPlatform () == TEGRA_PLATFORM_SYSTEM_FPGA)) {
    Count = 10;
  }

  if ( WaitForBit (&PciExpCap->LinkStatus.Uint16, 13, Count, 1000, TRUE)) {
    DEBUG ((
      DEBUG_ERROR,
      "PCIe Socket-0x%x:Ctrl-0x%x:Rp-0x%x Link is UP (Capable: Gen-%d,x%d  Negotiated: Gen-%d,x%d)\r\n",
      Private->SocketId,
      Private->SegmentCtlrId,
      RpInfo->RpId,
      PciExpCap->LinkCapability.Bits.MaxLinkSpeed,
      PciExpCap->LinkCapability.Bits.MaxLinkWidth,
      PciExpCap->LinkStatus.Bits.CurrentLinkSpeed,
      PciExpCap->LinkStatus.Bits.NegotiatedLinkWidth
      ));
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "PCIe Socket-0x%x:Ctrl-0x%x:Rp-0x%x Link is DOWN (Capable: Gen-%d,x%d)\r\n",
      Private->SocketId,
      Private->SegmentCtlrId,
      RpInfo->RpId,
      PciExpCap->LinkCapability.Bits.MaxLinkSpeed,
      PciExpCap->LinkCapability.Bits.MaxLinkWidth
      ));
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

typedef
EFI_STATUS
(EFIAPI *PROTOCOL_INSTANCE_CALLBACK)(
  IN EFI_HANDLE           Handle,
  IN VOID                 *Instance,
  IN VOID                 *Context
  );

STATIC
EFI_STATUS
VisitAllInstancesOfProtocol (
  IN EFI_GUID                    *Id,
  IN PROTOCOL_INSTANCE_CALLBACK  CallBackFunction,
  IN VOID                        *Context
  )
{
  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *HandleBuffer;
  UINTN       Index;
  VOID        *Instance;

  /* Start to check all the PciIo to find all possible device */
  HandleCount  = 0;
  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        Id,
                        NULL,
                        &HandleCount,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], Id, &Instance);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = (*CallBackFunction)(HandleBuffer[Index], Instance, Context);
  }

  gBS->FreePool (HandleBuffer);

  return EFI_SUCCESS;
}

STATIC
EFI_PCI_IO_PROTOCOL *
GetRPDev (
  EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS           Status;
  UINTN                HandleCount;
  EFI_HANDLE           *HandleBuffer;
  UINTN                Index;
  VOID                 *Instance;
  EFI_PCI_IO_PROTOCOL  *ParentPciIo;
  UINTN                Segment, Bus, Device, Function;
  UINTN                PSegment, PBus, PDevice, PFunction;

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);

  /* Start to check all the PciIo to find all possible devices */
  HandleCount  = 0;
  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiPciIoProtocolGuid,
                        NULL,
                        &HandleCount,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiPciIoProtocolGuid, &Instance);
    if (EFI_ERROR (Status)) {
      continue;
    }

    ParentPciIo = Instance;
    Status      = ParentPciIo->GetLocation (ParentPciIo, &PSegment, &PBus, &PDevice, &PFunction);
    ASSERT_EFI_ERROR (Status);

    if ((PSegment == Segment) && (PBus == 0)) {
      gBS->FreePool (HandleBuffer);
      return ParentPciIo;
    }
  }

  gBS->FreePool (HandleBuffer);
  return NULL;
}

STATIC
EFI_STATUS
PcieEnableErrorReporting (
  EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS                   Status;
  UINTN                        Segment, Bus, Device, Function;
  PCI_REG_PCIE_CAPABILITY      Capability;
  UINT32                       PciExpCapOffset, AerCapOffset, Offset, DevCapOffset;
  PCI_REG_PCIE_ROOT_CONTROL    RootControl;
  PCI_REG_PCIE_DEVICE_CONTROL  DeviceControl;
  PCI_REG_PCIE_DEVICE_STATUS   DeviceStatus;
  UINT32                       Val_32;
  UINT16                       Val_16;
  BOOLEAN                      SkipDPCEnable = FALSE;

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);

  PciExpCapOffset = 0;
  Status          = PcieLibLocatePciCapability (
                      PciIo,
                      EFI_PCI_CAPABILITY_ID_PCIEXP,
                      (UINT8 *)&PciExpCapOffset,
                      NULL
                      );

  if (EFI_ERROR (Status) || !PciExpCapOffset) {
    DEBUG ((
      DEBUG_WARN,
      "Device [%04x:%02x:%02x.%x] Doesn't have PCIe Express capability...!\n",
      Segment,
      Bus,
      Device,
      Function
      ));
    return EFI_UNSUPPORTED;
  }

  AerCapOffset = 0;
  Status       = PcieLibLocatePciExpressCapability (
                   PciIo,
                   PCI_EXPRESS_EXTENDED_CAPABILITY_ADVANCED_ERROR_REPORTING_ID,
                   &AerCapOffset,
                   NULL
                   );

  if (!EFI_ERROR (Status) && AerCapOffset) {
    // Clear AER Correctable Errror Status
    Offset = AerCapOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_ADVANCED_ERROR_REPORTING, CorrectableErrorStatus);
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    if (Status == EFI_SUCCESS) {
      // Write the same values back as they are RW1C bits
      PciIo->Pci.Write (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    }

    // Clear AER Uncorrectable Errror Status
    Offset = AerCapOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_ADVANCED_ERROR_REPORTING, UncorrectableErrorStatus);
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    if (Status == EFI_SUCCESS) {
      // Write the same values back as they are RW1C bits
      PciIo->Pci.Write (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    }

    Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, Capability);
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, Offset, 1, &Capability);
    if ((Status == EFI_SUCCESS) && ((Capability.Bits.DevicePortType == PCIE_DEVICE_PORT_TYPE_ROOT_PORT) || (Capability.Bits.DevicePortType == PCIE_DEVICE_PORT_TYPE_ROOT_COMPLEX_EVENT_COLLECTOR))) {
      // Clear AER root Errror Status
      Offset = AerCapOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_ADVANCED_ERROR_REPORTING, RootErrorStatus);
      Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
      if (Status == EFI_SUCCESS) {
        // Write the same values back as they are RW1C bits
        PciIo->Pci.Write (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
      }
    }

    // Enable ANF error
    Offset = AerCapOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_ADVANCED_ERROR_REPORTING, CorrectableErrorMask);
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    if (Status == EFI_SUCCESS) {
      Val_32 &= ~PCIE_AER_CORR_ERR_ADV_NONFATAL;
      PciIo->Pci.Write (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    }

    // Make sure set SEV with platform POR value such as PCIe spec default
    Offset = AerCapOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_ADVANCED_ERROR_REPORTING, UncorrectableErrorSeverity);
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    if (Status == EFI_SUCCESS) {
      // bit 4 - data link protocol error
      // bit 5 - surprise down
      // bit 13 - flow control protocol
      // bit 17 - receiver overflow
      // bit 18 - malformed tlp
      // bit 22 - internal error
      // bit 28 - IDE check fail
      // Set PCIe spec default sev as the platform POR setting
      Val_32 = ((1<<4) | (1<<5) | (1<<13) | (1<<17) | (1<<18) | (1<<22) | (1<<28));
      PciIo->Pci.Write (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    }
  }

  Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, Capability);
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        Offset,
                        1,
                        &Capability
                        );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  switch (Capability.Bits.DevicePortType) {
    case PCIE_DEVICE_PORT_TYPE_ROOT_PORT:
      /* Enable root port specific error reporting/forwarding */
      Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, RootControl);
      Status = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &RootControl.Uint16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      RootControl.Bits.SystemErrorOnCorrectableError = 1;
      RootControl.Bits.SystemErrorOnNonFatalError    = 1;
      RootControl.Bits.SystemErrorOnFatalError       = 1;

      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &RootControl.Uint16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((
        DEBUG_INFO,
        "Device [%04x:%02x:%02x.%x] : Enabled error reporting in RootControl register\n",
        Segment,
        Bus,
        Device,
        Function
        ));

    /* fall through */

    case PCIE_DEVICE_PORT_TYPE_DOWNSTREAM_PORT:
      /* Enable DPC which is applicable only for Root Ports and Switch Downstream ports */

      Offset = 0;
      Status = PcieLibLocatePciExpressCapability (
                 PciIo,
                 PCI_EXPRESS_EXTENDED_CAPABILITY_DPC_ID,
                 &Offset,
                 NULL
                 );

      if (!EFI_ERROR (Status) && Offset) {
        /* First clear the stale status */
        Val_16 = (PCIE_DPC_STS_TRIGGER_STATUS | PCIE_DPC_STS_SIG_SFW_STATUS);
        Status = PciIo->Pci.Write (
                              PciIo,
                              EfiPciIoWidthUint16,
                              Offset + PCIE_DPC_STS,
                              1,
                              &Val_16
                              );
        if (EFI_ERROR (Status)) {
          return EFI_DEVICE_ERROR;
        }

        /* Enable DPC */
        Status = PciIo->Pci.Read (
                              PciIo,
                              EfiPciIoWidthUint16,
                              Offset + PCIE_DPC_CTL,
                              1,
                              &Val_16
                              );
        if (EFI_ERROR (Status)) {
          return EFI_DEVICE_ERROR;
        }

        if (!SkipDPCEnable) {
          Val_16 |= (PCIE_DPC_CTL_DPC_TRIGGER_EN_NF_F | PCIE_DPC_CTL_DPC_ERR_COR_EN);
        }

        DevCapOffset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability);
        Status       = PciIo->Pci.Read (
                                    PciIo,
                                    EfiPciIoWidthUint32,
                                    DevCapOffset,
                                    1,
                                    &Val_32
                                    );
        if (EFI_ERROR (Status)) {
          return EFI_UNSUPPORTED;
        }

        if (Val_32 & PCIE_DEV_CAP_ERR_COR_SUB_CLASS) {
          Val_16 |= PCIE_DPC_CTL_DPC_SIG_SFW_EN;
        }

        Status = PciIo->Pci.Write (
                              PciIo,
                              EfiPciIoWidthUint16,
                              Offset + PCIE_DPC_CTL,
                              1,
                              &Val_16
                              );
        if (EFI_ERROR (Status)) {
          return EFI_DEVICE_ERROR;
        }

        DEBUG ((
          DEBUG_INFO,
          "Device [%04x:%02x:%02x.%x] : Enabled DPC\n",
          Segment,
          Bus,
          Device,
          Function
          ));

        /* If this is a switch downstream port, disable the DPC in the upstream RP */
        if (Bus > 0) {
          UINT32               RpDpcCapOffset;
          UINTN                RPSegment, RPBus, RPDevice, RPFunction;
          EFI_PCI_IO_PROTOCOL  *RPPciIo;

          RPPciIo        = GetRPDev (PciIo);
          RpDpcCapOffset = 0;
          Status         = PcieLibLocatePciExpressCapability (
                             PciIo,
                             PCI_EXPRESS_EXTENDED_CAPABILITY_DPC_ID,
                             &RpDpcCapOffset,
                             NULL
                             );

          if (!EFI_ERROR (Status) && RpDpcCapOffset) {
            Status = RPPciIo->Pci.Read (
                                    RPPciIo,
                                    EfiPciIoWidthUint16,
                                    RpDpcCapOffset + PCIE_DPC_CTL,
                                    1,
                                    &Val_16
                                    );
            if (EFI_ERROR (Status)) {
              return EFI_DEVICE_ERROR;
            }

            Val_16 &= ~(PCIE_DPC_CTL_DPC_TRIGGER_EN_NF_F | PCIE_DPC_CTL_DPC_INT_EN |
                        PCIE_DPC_CTL_DPC_ERR_COR_EN);

            Status = RPPciIo->Pci.Write (
                                    RPPciIo,
                                    EfiPciIoWidthUint16,
                                    RpDpcCapOffset + PCIE_DPC_CTL,
                                    1,
                                    &Val_16
                                    );
            if (EFI_ERROR (Status)) {
              return EFI_DEVICE_ERROR;
            }

            Status = RPPciIo->GetLocation (RPPciIo, &RPSegment, &RPBus, &RPDevice, &RPFunction);
            ASSERT_EFI_ERROR (Status);
            DEBUG ((
              DEBUG_INFO,
              "Device [%04x:%02x:%02x.%x] : Disabled DPC in the corresponding RootPort\n",
              RPSegment,
              RPBus,
              RPDevice,
              RPFunction
              ));
          }
        }
      } else {
        DEBUG ((
          DEBUG_INFO,
          "Device [%04x:%02x:%02x.%x] Doesn't have DPC capability...!\n",
          Segment,
          Bus,
          Device,
          Function
          ));
      }

    /* fall through */

    case PCIE_DEVICE_PORT_TYPE_UPSTREAM_PORT:
      /*
       * Enable SERR in Bridge Control register.
       * Applicalbe for all Type-1 config space devices
       * i.e.RPs, SWitch DPs and UPs
       */
      Status = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            PCI_BRIDGE_CONTROL_REGISTER_OFFSET,
                            1,
                            &Val_16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      Val_16 |= EFI_PCI_BRIDGE_CONTROL_SERR;

      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint16,
                            PCI_BRIDGE_CONTROL_REGISTER_OFFSET,
                            1,
                            &Val_16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((
        DEBUG_INFO,
        "Device [%04x:%02x:%02x.%x] : Enabled SERR in BridgeControl register\n",
        Segment,
        Bus,
        Device,
        Function
        ));

    /* fall through */

    case PCIE_DEVICE_PORT_TYPE_PCIE_ENDPOINT:
      /*
       * Enable error reporting in Device Control register in PCI Express
       * capability register which is applicable for all PCIe devices
       */

      /* Clear stale error status in Device Status register */
      Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceStatus);
      Status = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &DeviceStatus.Uint16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      /* Write the same values back as they are RW1C bits */
      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &DeviceStatus.Uint16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      /* Enable error reporting */
      Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl);
      Status = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &DeviceControl.Uint16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DeviceControl.Bits.CorrectableError   = 1;
      DeviceControl.Bits.NonFatalError      = 1;
      DeviceControl.Bits.FatalError         = 1;
      DeviceControl.Bits.UnsupportedRequest = 1;

      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &DeviceControl.Uint16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((
        DEBUG_INFO,
        "Device [%04x:%02x:%02x.%x] : Enabled error reporting in DeviceControl register\n",
        Segment,
        Bus,
        Device,
        Function
        ));

    /* fall through */

    default:
      /* Enable SERR in COMMAND register */
      Status = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            PCI_COMMAND_OFFSET,
                            1,
                            &Val_16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      Val_16 |= EFI_PCI_COMMAND_SERR;

      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint16,
                            PCI_COMMAND_OFFSET,
                            1,
                            &Val_16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((
        DEBUG_INFO,
        "Device [%04x:%02x:%02x.%x] : Enabled SERR in Command register\n",
        Segment,
        Bus,
        Device,
        Function
        ));
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PcieEnableECRC (
  EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS  Status;
  UINTN       Segment, Bus, Device, Function;
  UINT32      AerCapOffset, Offset;
  UINT32      Val, New_Val;

  /* Just return for now till early boot variable mechanism is in place */
  return EFI_SUCCESS;

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);

  AerCapOffset = 0;
  Status       = PcieLibLocatePciExpressCapability (
                   PciIo,
                   PCI_EXPRESS_EXTENDED_CAPABILITY_ADVANCED_ERROR_REPORTING_ID,
                   &AerCapOffset,
                   NULL
                   );

  if (!EFI_ERROR (Status) && AerCapOffset) {
    Offset = AerCapOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_ADVANCED_ERROR_REPORTING, AdvancedErrorCapabilitiesAndControl);
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint32,
                          Offset,
                          1,
                          &Val
                          );
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    New_Val = Val;

    if (Val & PCIE_AER_ECRC_GEN_CAP) {
      New_Val |= PCIE_AER_ECRC_GEN_EN;
    }

    if (Val & PCIE_AER_ECRC_CHK_CAP) {
      New_Val |= PCIE_AER_ECRC_CHK_EN;
    }

    if (New_Val != Val) {
      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint32,
                            Offset,
                            1,
                            &New_Val
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((
        DEBUG_INFO,
        "Device [%04x:%02x:%02x.%x] : Enabled ECRC\n",
        Segment,
        Bus,
        Device,
        Function
        ));
    }
  }

  return EFI_SUCCESS;
}

/**
  Get next PCI IO protocol

  @param  PciIo               The PCI IO protocol instance.
  @param  HandleCount         Handle Count of PCI IO protocol
  @param  HandleBuffer        Handle Buffer of PCI IO protocol

  @retval                     Next PciIo protocol. If NULL, means the input
                              pciio protocol is the last one within handle buffer

**/
STATIC
EFI_PCI_IO_PROTOCOL *
GetNextPciIoInstance (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINTN                HandleCount,
  IN EFI_HANDLE           *HandleBuffer
  )
{
  EFI_STATUS           Status;
  UINTN                Index;
  EFI_PCI_IO_PROTOCOL  *TempPciIo;

  if ((HandleBuffer == NULL) || (HandleCount == 0)) {
    return NULL;
  }

  TempPciIo = NULL;

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&TempPciIo
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Matching the instance.
    //
    if (TempPciIo == PciIo) {
      //
      // moving to the next instance.
      //
      Index++;
      break;
    }
  }

  //
  // Get the next valid PciIo protocol.
  //
  if (TempPciIo != NULL) {
    for (TempPciIo = NULL; Index < HandleCount; Index++) {
      Status = gBS->HandleProtocol (
                      HandleBuffer[Index],
                      &gEfiPciIoProtocolGuid,
                      (VOID **)&TempPciIo
                      );
      if (!EFI_ERROR (Status)) {
        break;
      }
    }
  }

  return TempPciIo;
}

/**
  Checks if Root Port and tree below given root port supports
  10Bit Tag Completer supported

  @param  PciIo               The PCI IO protocol instance.
  @param  RpSegment           Root port segment
  @param  HandleCount         Handle Count of PCI IO protocol
  @param  HandleBuffer        Handle Buffer of PCI IO protocol

  @retval  TRUE
           FALSE  Otherwise
**/
STATIC
BOOLEAN
Recursive10BitTagCompleterCheck (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINTN                RpSegment,
  IN UINTN                HandleCount,
  IN EFI_HANDLE           *HandleBuffer
  )
{
  EFI_STATUS                       Status;
  UINT32                           ExtTag10BitCompleterSupport;
  UINT32                           SubTree10BitTagSupport;
  EFI_PCI_IO_PROTOCOL              *NextPciIo;
  UINTN                            Segment, Bus, Device, Function;
  PCI_REG_PCIE_DEVICE_CAPABILITY   DeviceCapability;
  PCI_REG_PCIE_DEVICE_CAPABILITY2  DeviceCapability2;
  UINT32                           PciExpCapOffset, Offset;

  ExtTag10BitCompleterSupport = FALSE;
  SubTree10BitTagSupport      = FALSE;
  NextPciIo                   = NULL;

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);

  PciExpCapOffset = 0;
  Status          = PcieLibLocatePciCapability (
                      PciIo,
                      EFI_PCI_CAPABILITY_ID_PCIEXP,
                      (UINT8 *)&PciExpCapOffset,
                      NULL
                      );

  if (EFI_ERROR (Status) || (PciExpCapOffset == 0)) {
    DEBUG ((
      DEBUG_INFO,
      "Device [%04x:%02x:%02x.%x] doesn't support ExtendedTag. \
         Hence skipping 10-bit tags configuration\n",
      Segment,
      Bus,
      Device,
      Function
      ));
    goto Done;
  }

  Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability);
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        Offset,
                        1,
                        &DeviceCapability.Uint32
                        );

  if (EFI_ERROR (Status) || (DeviceCapability.Bits.ExtendedTagField == FALSE)) {
    DEBUG ((
      DEBUG_INFO,
      "Device [%04x:%02x:%02x.%x] doesn't support ExtendedTag. \
         Hence skipping 10-bit tags configuration\n",
      Segment,
      Bus,
      Device,
      Function
      ));
    goto Done;
  }

  Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability2);
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        Offset,
                        1,
                        &DeviceCapability2.Uint32
                        );

  if (EFI_ERROR (Status) ||
      (DeviceCapability2.Bits.TenBitTagCompleterSupported == FALSE))
  {
    DEBUG ((
      DEBUG_ERROR,
      "Device [%04x:%02x:%02x.%x] doesn't support 10 bit tag completer. \
         Hence skipping 10-bit tags configuration\n",
      Segment,
      Bus,
      Device,
      Function
      ));
    goto Done;
  }

  ExtTag10BitCompleterSupport = TRUE;

  if (ExtTag10BitCompleterSupport == TRUE) {
    NextPciIo = GetNextPciIoInstance (PciIo, HandleCount, HandleBuffer);
    if (NextPciIo != NULL) {
      if (!EFI_ERROR (Status) && (Segment == RpSegment)) {
        SubTree10BitTagSupport = Recursive10BitTagCompleterCheck (
                                   NextPciIo,
                                   RpSegment,
                                   HandleCount,
                                   HandleBuffer
                                   );
        ExtTag10BitCompleterSupport &= SubTree10BitTagSupport;
      }
    }
  }

Done:
  return ExtTag10BitCompleterSupport;
}

/**
  Enable 10Bit tag requester for all the downstream devices.

  @param  PciIo               The PCI IO protocol instance.
  @param  RpSegment           Root port segment
  @param  HandleCount         Handle Count of PCI IO protocol
  @param  HandleBuffer        Handle Buffer of PCI IO protocol

  @retval EFI_STATUS          Returns EFI_SUCCESS if 10Bit tag requester
                              enabled successfully.
**/
EFI_STATUS
Recursive10BitTagRequestSet (
  EFI_PCI_IO_PROTOCOL  *PciIo,
  UINTN                RpSegment,
  UINTN                HandleCount,
  EFI_HANDLE           *HandleBuffer
  )
{
  EFI_PCI_IO_PROTOCOL              *NextPciIo;
  EFI_STATUS                       Status;
  UINTN                            Segment, Bus, Device, Function;
  PCI_REG_PCIE_DEVICE_CAPABILITY2  DeviceCapability2;
  PCI_REG_PCIE_DEVICE_CONTROL      DeviceControl;
  PCI_REG_PCIE_DEVICE_CONTROL2     DeviceControl2;
  UINT32                           PciExpCapOffset, Offset;

  NextPciIo = NULL;
  Status    = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);

  /* Enable Extended Tag */
  PciExpCapOffset = 0;
  Status          = PcieLibLocatePciCapability (
                      PciIo,
                      EFI_PCI_CAPABILITY_ID_PCIEXP,
                      (UINT8 *)&PciExpCapOffset,
                      NULL
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Device [%04x:%02x:%02x.%x] : Enable ExtendedTagField failed\n",
      Segment,
      Bus,
      Device,
      Function
      ));
  }

  if (!EFI_ERROR (Status)) {
    Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl);
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint16,
                          Offset,
                          1,
                          &DeviceControl.Uint16
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "Device [%04x:%02x:%02x.%x] : Enabled ExtendedTagField failed\n",
        Segment,
        Bus,
        Device,
        Function
        ));
    }
  }

  if (!EFI_ERROR (Status)) {
    DeviceControl.Bits.ExtendedTagField = 1;

    Status = PciIo->Pci.Write (
                          PciIo,
                          EfiPciIoWidthUint16,
                          Offset,
                          1,
                          &DeviceControl.Uint16
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "Device [%04x:%02x:%02x.%x] : Enabled ExtendedTagField failed\n",
        Segment,
        Bus,
        Device,
        Function
        ));
    }
  }

  if (!EFI_ERROR (Status)) {
    Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability2);
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint32,
                          Offset,
                          1,
                          &DeviceCapability2.Uint32
                          );
  }

  if (!EFI_ERROR (Status) && (DeviceCapability2.Bits.TenBitTagRequesterSupported == TRUE)) {
    Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl2);
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint16,
                          Offset,
                          1,
                          &DeviceControl2.Uint16
                          );
    if (!EFI_ERROR (Status)) {
      DeviceControl2.Bits.TenBitTagRequesterEnable = 1;

      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &DeviceControl2.Uint16
                            );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "Device [%04x:%02x:%02x.%x] : Enabled 10Bit tag requester failed\n",
          Segment,
          Bus,
          Device,
          Function
          ));
      }
    }
  }

  NextPciIo = GetNextPciIoInstance (PciIo, HandleCount, HandleBuffer);
  if (NextPciIo != NULL) {
    Status = NextPciIo->GetLocation (NextPciIo, &Segment, &Bus, &Device, &Function);
    if (!EFI_ERROR (Status) && (Segment == RpSegment)) {
      Recursive10BitTagRequestSet (
        NextPciIo,
        RpSegment,
        HandleCount,
        HandleBuffer
        );
    }
  }

  return Status;
}

/**
  10Bit tag requester config.

  @param  PciIo               The PCI IO protocol instance.

  @retval EFI_STATUS          Returns EFI_SUCCESS if 10Bit tag requester
                              configured successfully.
**/
STATIC
EFI_STATUS
PcieEnable10BitExtendedTag (
  EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS  Status;
  UINTN       Segment, Bus, Device, Function;
  BOOLEAN     ExtTag10BitCompleterSupport;
  UINTN       HandleCount;
  EFI_HANDLE  *HandleBuffer;
  UINT64      Ext10bitTagReqEnable;
  UINTN       BufferSize;

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);
  //
  // Run on root port only
  //
  if (Bus != 0) {
    return EFI_SUCCESS;
  }

  BufferSize = sizeof (Ext10bitTagReqEnable);
  Status     = gRT->GetVariable (
                      L"Ext10bitTagReq",
                      &gNVIDIAPublicVariableGuid,
                      NULL,
                      &BufferSize,
                      &Ext10bitTagReqEnable
                      );
  if (EFI_ERROR (Status) ||
      (BufferSize != sizeof (Ext10bitTagReqEnable)) ||
      ((Ext10bitTagReqEnable & (1ULL << Segment)) == 0ULL))
  {
    DEBUG ((
      DEBUG_ERROR,
      "Device [%04x:%02x:%02x.%x] : Skipping 10Bit tag requester Enable\n",
      Segment,
      Bus,
      Device,
      Function
      ));

    return EFI_SUCCESS;
  }

  HandleCount  = 0;
  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiPciIoProtocolGuid,
                        NULL,
                        &HandleCount,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  //
  // Check 10-bit tag completer capability before setting 10-bit tag requester.
  //
  ExtTag10BitCompleterSupport = Recursive10BitTagCompleterCheck (
                                  PciIo,
                                  Segment,
                                  HandleCount,
                                  HandleBuffer
                                  );
  if (ExtTag10BitCompleterSupport == TRUE) {
    Recursive10BitTagRequestSet (PciIo, Segment, HandleCount, HandleBuffer);
  } else {
    DEBUG ((
      DEBUG_INFO,
      "Device [%04x:%02x:%02x.%x] : 10Bit tag requester not supported\n",
      Segment,
      Bus,
      Device,
      Function
      ));
  }

  if (HandleBuffer != NULL) {
    gBS->FreePool (HandleBuffer);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
VisitEachPcieDevice (
  IN EFI_HANDLE  Handle,
  IN VOID        *Instance,
  IN VOID        *Context
  )
{
  EFI_PCI_IO_PROTOCOL  *PciIo;

  PciIo = Instance;

  PcieEnable10BitExtendedTag (PciIo);
  PcieEnableErrorReporting (PciIo);
  PcieEnableECRC (PciIo);

  return EFI_SUCCESS;
}

STATIC
VOID
PcieConfigDevices (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  VisitAllInstancesOfProtocol (
    &gEfiPciIoProtocolGuid,
    VisitEachPcieDevice,
    NULL
    );
}

/**
  Dynamically patch a segment controller's RP subnodes (bus-range, ECAM, BAR ranges, status)
  from the shared cxl_brg register bank, per the bifurcation configuration
  programmed by PXIR-FW.

  @param[in] Socket                 Socket ID, for debug logging only.
  @param[in] SegmentCtlrId          Segment controller ID, for debug logging only.
  @param[in] Dtb                    Device tree blob base.
  @param[in] SegmentCtlrNodeOffset  Offset of the segment controller node in Dtb.

  @retval EFI_SUCCESS      All enabled RP subnodes patched.
  @retval others           Register lookup or DT patch failure.

**/
EFI_STATUS
EFIAPI
UpdatePcieNode (
  IN UINT32  Socket,
  IN UINT32  SegmentCtlrId,
  IN  VOID   *Dtb,
  IN INT32   SegmentCtlrNodeOffset
  )
{
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterData[T410_PCIE_MAX_SEGMENT_CTLR_REGISTERS];
  EFI_STATUS                        Status = EFI_SUCCESS;
  CONST VOID                        *Property;
  UINT64                            CxlBrgBase;
  UINT64                            EcamBase, EcamSize;
  UINT32                            RegisterCount;
  UINT32                            RegisterIndex;
  UINT32                            Tmp32;
  INT32                             NodeOffset;
  INT32                             Length;

  RegisterCount = ARRAY_SIZE (RegisterData);
  Status        = DeviceTreeGetRegisters (SegmentCtlrNodeOffset, RegisterData, &RegisterCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetRegisters failed: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  Status = DeviceTreeFindRegisterByName ("cxl_brg", RegisterData, RegisterCount, &RegisterIndex);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate cxl_brg address range\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  CxlBrgBase = RegisterData[RegisterIndex].BaseAddress;
  DEBUG ((DEBUG_VERBOSE, "Skt-%u:SegmentCtlr-%u: Cxl Brg Base Addr = 0x%llx\n", Socket, SegmentCtlrId, CxlBrgBase));

  EcamBase  = MmioRead32 (CxlBrgBase + NV_CXL_BRG_ECAM_BASE_HI_0);
  EcamBase  = EcamBase << 32;
  EcamBase |= MmioRead32 (CxlBrgBase + NV_CXL_BRG_ECAM_BASE_LO_0);
  DEBUG ((DEBUG_VERBOSE, "Skt-%u:SegmentCtlr-%u: ECAM Base Addr = 0x%llx\n", Socket, SegmentCtlrId, EcamBase));

  FdtForEachSubnode (NodeOffset, Dtb, SegmentCtlrNodeOffset) {
    UINT32  RpId;
    INT32   FdtErr;
    UINT8   StartBus, EndBus;
    UINT32  Bar32BusAddr, Bar32Size;
    UINT64  Bar32CpuAddrBase, Bar32CpuAddrLimit;
    UINT64  Bar64AddrBase, Bar64AddrLimit, Bar64Size;
    UINT8   OkayStr[] = { 'o', 'k', 'a', 'y', '\0', '\0', '\0', '\0', '\0' };

    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "nvidia,rp-id", &RpId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Skt-%u:SegmentCtlr-%u: \"nvidia,rp-id\" entry is missing in DT\n", Socket, SegmentCtlrId));
      return EFI_UNSUPPORTED;
    }

    if (RpId >= T410_MAX_RP_PER_SEGMENT_CTLR) {
      DEBUG ((DEBUG_ERROR, "Skt-%u:SegmentCtlr-%u: Invalid entry for \"nvidia,rp-id\": %u\n", Socket, SegmentCtlrId, RpId));
      return EFI_UNSUPPORTED;
    }

    DEBUG ((DEBUG_VERBOSE, "Skt-%u:SegmentCtlr-%u: nvidia,rp-id = %u\n", Socket, SegmentCtlrId, RpId));

    /* Find out if the RP is enabled */
    Tmp32 = MmioRead32 (CxlBrgBase + NV_CXL_BRG_BAR_CNTL_ECAM_0 + (RpId * NV_CXL_BRG_BAR_CNTL_ECAM_STRIDE));
    if (!(Tmp32 & NV_CXL_BRG_BAR_CNTL_ECAM_ENABLED)) {
      DEBUG ((DEBUG_INFO, "Skt-%u:SegmentCtlr-%u: Disabled RP-%u\n", Socket, SegmentCtlrId, RpId));
      continue;
    }

    /* RP is enabled. Set the DT status also accordingly */
    DEBUG ((DEBUG_INFO, "Skt-%u:SegmentCtlr-%u:RP-%u is enabled in bifurcation\n", Socket, SegmentCtlrId, RpId));
    FdtErr = FdtSetPropInplace (Dtb, NodeOffset, "status", OkayStr, sizeof (OkayStr));
    if (FdtErr < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "Skt-%u:SegmentCtlr-%u: Failed to enable RP-%u (node : %a)\n",
        Socket,
        SegmentCtlrId,
        RpId,
        FdtStrerror (FdtErr)
        ));
      return EFI_DEVICE_ERROR;
    }

    /* Patch bus-range property */
    Tmp32    = MmioRead32 (CxlBrgBase + NV_CXL_BRG_ECAM_RP_0 + (RpId * 4));
    StartBus = Tmp32 & NV_CXL_BRG_ECAM_RP_BUS_MASK;
    EndBus   = (Tmp32 >> NV_CXL_BRG_ECAM_RP_END_BUS_SHIFT) & NV_CXL_BRG_ECAM_RP_BUS_MASK;
    DEBUG ((
      DEBUG_VERBOSE,
      "Skt-%u:SegmentCtlr-%u:RP-%u: StartBus = %u, EndBus = %u\n",
      Socket,
      SegmentCtlrId,
      RpId,
      StartBus,
      EndBus
      ));

    Property = FdtGetProp (Dtb, NodeOffset, "bus-range", &Length);
    if ((Property == NULL) || (Length != sizeof (Tmp32) * T410_PCIE_BUS_RANGE_PROP_LEN)) {
      DEBUG ((DEBUG_ERROR, "Unexpected \"bus-range\" property. Length = %d\n", Length));
      return EFI_UNSUPPORTED;
    }

    ((UINT32 *)Property)[0] = CpuToFdt32 (StartBus);
    ((UINT32 *)Property)[1] = CpuToFdt32 (EndBus);

    /* Patch ECAM Address in 'reg' property */
    Property = FdtGetProp (Dtb, NodeOffset, "reg", &Length);
    if ((Property == NULL) || (Length != sizeof (Tmp32) * T410_PCIE_ECAM_PROP_LEN)) {
      DEBUG ((DEBUG_ERROR, "Unexpected \"reg\" property. Length = %d\n", Length));
      return EFI_UNSUPPORTED;
    }

    EcamSize = (EndBus + 1) * (SIZE_1MB);
    DEBUG ((
      DEBUG_VERBOSE,
      "Skt-%u:SegmentCtlr-%u:RP-%u: EcamBase = 0x%llx, EcamSize = 0x%llx\n",
      Socket,
      SegmentCtlrId,
      RpId,
      EcamBase,
      EcamSize
      ));

    ((UINT32 *)Property)[0] = CpuToFdt32 (EcamBase >> 32);
    ((UINT32 *)Property)[1] = CpuToFdt32 (EcamBase);
    ((UINT32 *)Property)[2] = CpuToFdt32 (EcamSize >> 32);
    ((UINT32 *)Property)[3] = CpuToFdt32 (EcamSize);

    /* Patch ranges property */
    Property = FdtGetProp (Dtb, NodeOffset, "ranges", &Length);
    if ((Property == NULL) ||
        ((Length != sizeof (Tmp32) * T410_PCIE_RANGES_IO_PREF_NONPREF_PROP_LEN) &&
         (Length != sizeof (Tmp32) * T410_PCIE_RANGES_PREF_NONPREF_PROP_LEN)))
    {
      DEBUG ((DEBUG_ERROR, "Unexpected \"ranges\" property. Length = %d\n", Length));
      return EFI_UNSUPPORTED;
    }

    Tmp32 = MmioRead32 (CxlBrgBase + NV_CXL_BRG_BAR_CNTL_STANDARD_0_0 + (RpId * NV_CXL_BRG_BAR_CNTL_STRIDE));
    // Check if 64B BAR is enabled
    if (Tmp32 & NV_CXL_BRG_BAR_CNTL_STANDARD_64B_BAR_EN) {
      Bar64AddrBase  = MmioRead32 (CxlBrgBase + NV_CXL_BRG_MEM_64BIT_BASE_HI_0_0 + (RpId * NV_CXL_BRG_BAR_CNTL_STRIDE));
      Bar64AddrBase  = (Bar64AddrBase << 32);
      Bar64AddrBase |= MmioRead32 (CxlBrgBase + NV_CXL_BRG_MEM_64BIT_BASE_LO_0_0 + (RpId * NV_CXL_BRG_BAR_CNTL_STRIDE));

      Bar64AddrLimit  = MmioRead32 (CxlBrgBase + NV_CXL_BRG_MEM_64BIT_LIMIT_HI_0_0 + (RpId * NV_CXL_BRG_BAR_CNTL_STRIDE));
      Bar64AddrLimit  = (Bar64AddrLimit << 32);
      Bar64AddrLimit |= MmioRead32 (CxlBrgBase + NV_CXL_BRG_MEM_64BIT_LIMIT_LO_0_0 + (RpId * NV_CXL_BRG_BAR_CNTL_STRIDE));
      Bar64Size       = Bar64AddrLimit - Bar64AddrBase;
      DEBUG ((
        DEBUG_VERBOSE,
        "Skt-%u:SegmentCtlr-%u:RP-%u: Bar64Addr = 0x%llx, Bar64Size = 0x%llx\n",
        Socket,
        SegmentCtlrId,
        RpId,
        Bar64AddrBase,
        Bar64Size
        ));

      ((UINT32 *)Property)[1] = CpuToFdt32 (Bar64AddrBase >> 32);
      ((UINT32 *)Property)[2] = CpuToFdt32 (Bar64AddrBase);
      ((UINT32 *)Property)[3] = CpuToFdt32 (Bar64AddrBase >> 32);
      ((UINT32 *)Property)[4] = CpuToFdt32 (Bar64AddrBase);
      ((UINT32 *)Property)[5] = CpuToFdt32 (Bar64Size >> 32);
      ((UINT32 *)Property)[6] = CpuToFdt32 (Bar64Size);
    }

    // Check if 32B BAR is enabled
    if (Tmp32 & NV_CXL_BRG_BAR_CNTL_STANDARD_32B_BAR_EN) {
      Bar32CpuAddrBase  = MmioRead32 (CxlBrgBase + NV_CXL_BRG_MEM_32BIT_BASE_HI_0_0 + (RpId * 0x24));
      Bar32CpuAddrBase  = (Bar32CpuAddrBase << 32);
      Bar32CpuAddrBase |= MmioRead32 (CxlBrgBase + NV_CXL_BRG_MEM_32BIT_BASE_LO_0_0 + (RpId * 0x24));

      Bar32CpuAddrLimit  = MmioRead32 (CxlBrgBase + NV_CXL_BRG_MEM_32BIT_LIMIT_HI_0_0 + (RpId * 0x24));
      Bar32CpuAddrLimit  = (Bar32CpuAddrLimit << 32);
      Bar32CpuAddrLimit |= MmioRead32 (CxlBrgBase + NV_CXL_BRG_MEM_32BIT_LIMIT_LO_0_0 + (RpId * 0x24));
      Bar32Size          = Bar32CpuAddrLimit - Bar32CpuAddrBase;
      Bar32BusAddr       = Bar32CpuAddrBase & MAX_UINT32;
      DEBUG ((
        DEBUG_VERBOSE,
        "Skt-%u:SegmentCtlr-%u:RP-%u: Bar32BusAddr = 0x%x, Bar32CpuAddr = 0x%llx, Bar32Size = 0x%llx\n",
        Socket,
        SegmentCtlrId,
        RpId,
        Bar32BusAddr,
        Bar32CpuAddrBase,
        Bar32Size
        ));

      ((UINT32 *)Property)[8]  = CpuToFdt32 (0x0);
      ((UINT32 *)Property)[9]  = CpuToFdt32 (Bar32BusAddr);
      ((UINT32 *)Property)[10] = CpuToFdt32 (Bar32CpuAddrBase >> 32);
      ((UINT32 *)Property)[11] = CpuToFdt32 (Bar32CpuAddrBase);
      ((UINT32 *)Property)[12] = CpuToFdt32 (0x0);
      ((UINT32 *)Property)[13] = CpuToFdt32 (Bar32Size);
    }
  }

  return EFI_SUCCESS;
}

/**
  Parse one bifurcated root port subnode under a segment controller, install its root
  bridge protocols on a handle of its own.

  @param[in] ControllerHandle  Handle of the segment controller (NOT reused
                                for the RP's own protocol install).
  @param[in] RpNodeOffset      Offset of the RP subnode in DeviceTreeBase.
  @param[in] DeviceTreeBase    Device tree blob base.
  @param[in] Private           SegmentCtlr-level private data; this RP's slot is
                                Private->RpInfo[RpId].

  @retval EFI_SUCCESS      RP parsed and installed, or found disabled.
  @retval EFI_NOT_FOUND    No "status" property on the RP subnode.
  @retval others           DT parse or protocol install failure.

**/
EFI_STATUS
ParseRpNode (
  IN  EFI_HANDLE               ControllerHandle,
  IN  INT32                    RpNodeOffset,
  IN  VOID                     *DeviceTreeBase,
  IN  PCIE_CONTROLLER_PRIVATE  *Private
  )
{
  EFI_STATUS                                   Status            = EFI_SUCCESS;
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  *CMTokenProtocol  = NULL;
  NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA        *InterruptMap     = NULL;
  EFI_DEVICE_PATH_PROTOCOL                     *ParentDevicePath = NULL;
  PCIE_RP_PRIVATE                              *RpInfo           = NULL;
  PCI_ROOT_BRIDGE                              *RootBridge       = NULL;
  EFI_HANDLE                                   RpHandle          = NULL;
  CM_OBJECT_TOKEN                              *TokenMap         = NULL;
  CONST VOID                                   *BusProperty      = NULL;
  CONST VOID                                   *Property         = NULL;
  NVIDIA_DEVICE_TREE_RANGES_DATA               *RangesArray      = NULL;
  NVIDIA_DEVICE_TREE_REGISTER_DATA             RegisterData[PCIE_CONTROLLER_MAX_REGISTERS];
  EFI_EVENT                                    ExitBootServiceEvent;
  UINT32                                       NumberOfInterruptMaps;
  UINT32                                       NumberOfRanges;
  UINT32                                       RangeIndex;
  UINT32                                       Index;
  UINT32                                       Index2;
  UINT32                                       RegisterCount;
  UINT32                                       RegisterIndex;
  UINT32                                       RpId;
  INT32                                        SubNodeOffset;
  INT32                                        PropertySize = 0;
  INT32                                        Length;

  Status = gBS->LocateProtocol (&gNVIDIAConfigurationManagerTokenProtocolGuid, NULL, (VOID **)&CMTokenProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find ConfigurationManagerTokenProtocol\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  Property = FdtGetProp (DeviceTreeBase, RpNodeOffset, "status", &Length);
  if ((Property == NULL)) {
    return EFI_NOT_FOUND;
  } else if ((AsciiStrCmp (Property, "okay") != 0)) {
    if (AsciiStrCmp (Property, "disabled") == 0) {
      return EFI_SUCCESS;
    } else {
      return EFI_UNSUPPORTED;
    }
  }

  RootBridge = AllocateZeroPool (sizeof (PCI_ROOT_BRIDGE));
  if (RootBridge == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate device bridge structure\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DeviceTreeGetNodePropertyValue32 (RpNodeOffset, "nvidia,rp-id", &RpId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Invalid RP-ID in SegmentCtlr:%u\n", Private->SegmentCtlrId));
    goto CleanupAndReturn;
  }

  RpInfo            = &Private->RpInfo[RpId];
  RpInfo->Signature = PCIE_CONTROLLER_SIGNATURE;
  RpInfo->RpId      = RpId;
  DEBUG ((DEBUG_INFO, "RP-ID:%u found in SegmentCtlr:%u\n", RpId, Private->SegmentCtlrId));

  RegisterCount = ARRAY_SIZE (RegisterData);
  Status        = DeviceTreeGetRegisters (RpNodeOffset, RegisterData, &RegisterCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetRegisters failed: %r\n", __FUNCTION__, Status));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  Status = DeviceTreeFindRegisterByName ("ecam", RegisterData, RegisterCount, &RegisterIndex);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate ECAM address range\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  RpInfo->EcamBase = RegisterData[RegisterIndex].BaseAddress;
  RpInfo->EcamSize = RegisterData[RegisterIndex].Size;
  DEBUG ((DEBUG_INFO, "EcamBase = 0x%llx\n", RpInfo->EcamBase));
  DEBUG ((DEBUG_INFO, "EcamSize = 0x%llx\n", RpInfo->EcamSize));

  Status = gDS->AddMemorySpace (
                  EfiGcdMemoryTypeMemoryMappedIo,
                  (EFI_PHYSICAL_ADDRESS)RpInfo->EcamBase & ~EFI_PAGE_MASK,
                  RpInfo->EcamSize,
                  EFI_MEMORY_UC
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to add ECAM address to memory space\n"));
    goto CleanupAndReturn;
  }

  Status = gDS->SetMemorySpaceAttributes (
                  (EFI_PHYSICAL_ADDRESS)RpInfo->EcamBase & ~EFI_PAGE_MASK,
                  RpInfo->EcamSize,
                  EFI_MEMORY_UC
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set ECAM address memory attributes\n"));
    goto CleanupAndReturn;
  }

  RpInfo->PcieRootBridgeConfigurationIo.Read          = PcieConfigurationRead;
  RpInfo->PcieRootBridgeConfigurationIo.Write         = PcieConfigurationWrite;
  RpInfo->PcieRootBridgeConfigurationIo.EcamBase      = RpInfo->EcamBase;
  RpInfo->PcieRootBridgeConfigurationIo.SegmentNumber = Private->Segment;
  RpInfo->PcieRootBridgeConfigurationIo.ControllerID  = Private->SegmentCtlrId;
  RpInfo->PcieRootBridgeConfigurationIo.RpID          = RpId;
  RpInfo->PcieRootBridgeConfigurationIo.SocketID      = Private->SocketId;

  DEBUG ((DEBUG_INFO, "Socket-ID = 0x%x\n", Private->SocketId));
  DEBUG ((DEBUG_INFO, "SegmentCtlr-ID  = 0x%x\n", Private->SegmentCtlrId));
  DEBUG ((DEBUG_INFO, "Segment   = 0x%x\n", Private->Segment));
  DEBUG ((DEBUG_INFO, "RP-ID     = 0x%x\n", RpId));

  SubNodeOffset = FdtFirstSubnode (DeviceTreeBase, RpNodeOffset);
  if (SubNodeOffset > 0) {
    if (FdtGetProperty (DeviceTreeBase, SubNodeOffset, "external-facing", NULL) != NULL) {
      RpInfo->PcieRootBridgeConfigurationIo.IsExternalFacingPort = TRUE;
    }
  }

  RpInfo->PcieRootBridgeConfigurationIo.OSCCtrl = (PCIE_FW_OSC_CTRL_PCIE_NATIVE_HP |
                                                   PCIE_FW_OSC_CTRL_PCIE_CAP_STRUCTURE |
                                                   PCIE_FW_OSC_CTRL_LTR);
  // TODO: Give AER and DPC capabilities to OS based on early boot vars (when they are available)

  RootBridge->Segment               = Private->Segment;
  RootBridge->Supports              = 0;
  RootBridge->Attributes            = 0;
  RootBridge->DmaAbove4G            = TRUE;
  RootBridge->NoExtendedConfigSpace = FALSE;
  RootBridge->ResourceAssigned      = FALSE;
  RootBridge->AllocationAttributes  = EFI_PCI_HOST_BRIDGE_MEM64_DECODE;

  BusProperty = FdtGetProp (
                  DeviceTreeBase,
                  RpNodeOffset,
                  "bus-range",
                  &PropertySize
                  );
  if ((BusProperty == NULL) || (PropertySize != 2 * sizeof (UINT32))) {
    DEBUG ((DEBUG_WARN, "PCIe Controller: unknown bus size in fdt, default to 0-255\r\n"));
    RootBridge->Bus.Base  = 0x0;
    RootBridge->Bus.Limit = 0xff;
  } else {
    CopyMem (&RootBridge->Bus.Base, BusProperty, sizeof (UINT32));
    RootBridge->Bus.Base = SwapBytes32 (RootBridge->Bus.Base);
    CopyMem (&RootBridge->Bus.Limit, BusProperty + sizeof (UINT32), sizeof (UINT32));
    RootBridge->Bus.Limit = SwapBytes32 (RootBridge->Bus.Limit);
  }

  RpInfo->PcieRootBridgeConfigurationIo.MinBusNumber = RootBridge->Bus.Base;
  RpInfo->PcieRootBridgeConfigurationIo.MaxBusNumber = RootBridge->Bus.Limit;
  DEBUG ((DEBUG_INFO, "Bus Range = [0x%x : 0x%x]\n", RootBridge->Bus.Base, RootBridge->Bus.Limit));

  // Mark all regions as unsupported
  RootBridge->Io.Base          = MAX_UINT64;
  RootBridge->Mem.Base         = MAX_UINT64;
  RootBridge->MemAbove4G.Base  = MAX_UINT64;
  RootBridge->PMem.Base        = MAX_UINT64;
  RootBridge->PMemAbove4G.Base = MAX_UINT64;

  NumberOfRanges = 0;
  Status         = DeviceTreeGetRanges (RpNodeOffset, "ranges", NULL, &NumberOfRanges);
  if (EFI_ERROR (Status) && (Status != EFI_BUFFER_TOO_SMALL)) {
    DEBUG ((DEBUG_ERROR, "PCIe Controller: Unsupported ranges configuration: %r\r\n", Status));
    goto CleanupAndReturn;
  }

  if (NumberOfRanges == 0) {
    DEBUG ((DEBUG_ERROR, "PCIe Controller: Unsupported ranges configuration: No ranges found\r\n"));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  RangesArray = AllocatePool (sizeof (NVIDIA_DEVICE_TREE_RANGES_DATA) * NumberOfRanges);
  if (RangesArray == NULL) {
    DEBUG ((DEBUG_ERROR, "PCIe Controller: Unable to allocate space for %u ranges\n", NumberOfRanges));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = DeviceTreeGetRanges (RpNodeOffset, "ranges", RangesArray, &NumberOfRanges);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "PCIe Controller: Unable to get ranges: %r\r\n", Status));
    goto CleanupAndReturn;
  }

  for (RangeIndex = 0; RangeIndex < NumberOfRanges; RangeIndex++) {
    UINT32   Flags         = 0;
    UINT32   Space         = 0;
    BOOLEAN  Prefetchable  = FALSE;
    UINT64   DeviceAddress = 0;
    UINT64   HostAddress   = 0;
    UINT64   Limit         = 0;
    UINT64   Size          = 0;
    UINT64   Translation   = 0;

    ASSERT (RpInfo->AddressMapCount < PCIE_NUMBER_OF_MAPPING_SPACE);

    Flags         = RangesArray[RangeIndex].ChildAddressHigh;
    DeviceAddress = RangesArray[RangeIndex].ChildAddress;
    HostAddress   = RangesArray[RangeIndex].ParentAddress;
    Size          = RangesArray[RangeIndex].Size;

    Space        = Flags & PCIE_DEVICETREE_SPACE_CODE;
    Prefetchable = ((Flags & PCIE_DEVICETREE_PREFETCHABLE) == PCIE_DEVICETREE_PREFETCHABLE);
    Limit        = DeviceAddress + Size - 1;
    Translation  = DeviceAddress - HostAddress;

    if (Space == PCIE_DEVICETREE_SPACE_IO) {
      ASSERT (RootBridge->Io.Base == MAX_UINT64);
      RootBridge->Io.Base                                       = DeviceAddress;
      RootBridge->Io.Limit                                      = Limit;
      RootBridge->Io.Translation                                = Translation;
      RpInfo->IoBase                                            = HostAddress;
      RpInfo->IoLimit                                           = HostAddress + Size - 1;
      RpInfo->AddressMapInfo[RpInfo->AddressMapCount].SpaceCode = 1;
    } else if (Space == PCIE_DEVICETREE_SPACE_MEM64) {
      if (Prefetchable) {
        if (Translation) {
          DEBUG ((DEBUG_ERROR, "Non 1:1 mapping is NOT supported for Prefetchable aperture\n"));
          Status = EFI_DEVICE_ERROR;
          break;
        }

        RootBridge->PMemAbove4G.Base                              = DeviceAddress;
        RootBridge->PMemAbove4G.Limit                             = Limit;
        RootBridge->PMemAbove4G.Translation                       = Translation;
        RpInfo->PrefetchMemBase                                   = HostAddress;
        RpInfo->PrefetchMemLimit                                  = HostAddress + Size - 1;
        RpInfo->AddressMapInfo[RpInfo->AddressMapCount].SpaceCode = 3;
        DEBUG ((DEBUG_INFO, "PREF64: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", DeviceAddress, Limit, Translation));
      } else {
        if (Translation) {
          RpInfo->MemBase  = HostAddress;
          RpInfo->MemLimit = HostAddress + Size - 1;
          // Split translated region into prefetchable and non-prefetchable
          Size                                                        = Size/2;
          RootBridge->Mem.Base                                        = DeviceAddress;
          RootBridge->Mem.Limit                                       = DeviceAddress + Size - 1;
          RootBridge->Mem.Translation                                 = Translation;
          RpInfo->AddressMapInfo[RpInfo->AddressMapCount].PciAddress  = DeviceAddress;
          RpInfo->AddressMapInfo[RpInfo->AddressMapCount].CpuAddress  = HostAddress;
          RpInfo->AddressMapInfo[RpInfo->AddressMapCount].AddressSize = Size;
          RpInfo->AddressMapInfo[RpInfo->AddressMapCount].SpaceCode   = 3;
          DEBUG ((DEBUG_INFO, "MEM32: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", RootBridge->Mem.Base, RootBridge->Mem.Limit, RootBridge->Mem.Translation));
          RpInfo->AddressMapCount++;
          ASSERT (RpInfo->AddressMapCount < PCIE_NUMBER_OF_MAPPING_SPACE);
          DeviceAddress                                             = DeviceAddress + Size;
          HostAddress                                               = HostAddress + Size;
          RootBridge->PMem.Base                                     = DeviceAddress;
          RootBridge->PMem.Limit                                    = DeviceAddress + Size - 1;
          RootBridge->PMem.Translation                              = DeviceAddress - HostAddress;
          RpInfo->AddressMapInfo[RpInfo->AddressMapCount].SpaceCode = 3;
          DEBUG ((DEBUG_INFO, "PREF32: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", RootBridge->PMem.Base, RootBridge->PMem.Limit, RootBridge->PMem.Translation));
        } else {
          DEBUG ((DEBUG_ERROR, "1:1 mapping is NOT supported for Non-Prefetchable aperture\n"));
          Status = EFI_DEVICE_ERROR;
          break;
        }
      }
    } else if (Space == PCIE_DEVICETREE_SPACE_MEM32) {
      RootBridge->Mem.Base                                      = DeviceAddress;
      RootBridge->Mem.Limit                                     = Limit;
      RootBridge->Mem.Translation                               = Translation;
      RpInfo->MemBase                                           = HostAddress;
      RpInfo->MemLimit                                          = HostAddress + Size - 1;
      RpInfo->AddressMapInfo[RpInfo->AddressMapCount].SpaceCode = 3;
      DEBUG ((DEBUG_INFO, "MEM32: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", DeviceAddress, Limit, Translation));
    } else {
      DEBUG ((DEBUG_ERROR, "PCIe Controller: Unknown region 0x%08x 0x%016llx-0x%016llx T 0x%016llx\r\n", Flags, DeviceAddress, Limit, Translation));
      ASSERT (FALSE);
      Status = EFI_DEVICE_ERROR;
      break;
    }

    RpInfo->AddressMapInfo[RpInfo->AddressMapCount].PciAddress  = DeviceAddress;
    RpInfo->AddressMapInfo[RpInfo->AddressMapCount].CpuAddress  = HostAddress;
    RpInfo->AddressMapInfo[RpInfo->AddressMapCount].AddressSize = Size;
    ASSERT (RpInfo->AddressMapCount < PCIE_NUMBER_OF_MAPPING_SPACE);
    RpInfo->AddressMapCount++;
  }

  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  if ((RootBridge->PMem.Base == MAX_UINT64) && (RootBridge->PMemAbove4G.Base == MAX_UINT64)) {
    RootBridge->AllocationAttributes |= EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
  }

  Status = InitializeController (Private, RpId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to initialize controller (%r)\r\n", __FUNCTION__, Status));
    Status = EFI_DEVICE_ERROR;
    goto CleanupAndReturn;
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
    DEBUG ((DEBUG_ERROR, "%a: Unable to setup exit boot services uninitialize. (%r)\r\n", __FUNCTION__, Status));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to get device path (%r)\r\n", __FUNCTION__, Status));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  mPciRootBridgeDevicePathNode.UID = ((Private->SocketId << 8) | (Private->SegmentCtlrId << 4) | (RpInfo->RpId));
  RootBridge->DevicePath           = AppendDevicePathNode (
                                       ParentDevicePath,
                                       (EFI_DEVICE_PATH_PROTOCOL  *)&mPciRootBridgeDevicePathNode
                                       );
  if (RootBridge->DevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // Setup configuration structure
  RpInfo->ConfigSpaceInfo.BaseAddress           = RpInfo->EcamBase;
  RpInfo->ConfigSpaceInfo.PciSegmentGroupNumber = RpInfo->PcieRootBridgeConfigurationIo.SegmentNumber;
  RpInfo->ConfigSpaceInfo.StartBusNumber        = RpInfo->PcieRootBridgeConfigurationIo.MinBusNumber;
  RpInfo->ConfigSpaceInfo.EndBusNumber          = RpInfo->PcieRootBridgeConfigurationIo.MaxBusNumber;
  Status                                        = CMTokenProtocol->AllocateTokens (CMTokenProtocol, 2, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate 2 tokens for the ConfigSpaceInfo token maps\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  RpInfo->ConfigSpaceInfo.AddressMapToken   = TokenMap[0];
  RpInfo->ConfigSpaceInfo.InterruptMapToken = TokenMap[1];
  FreePool (TokenMap);
  TokenMap = NULL;

  InterruptMap = AllocateZeroPool (PCIE_NUMBER_OF_INTERRUPT_MAP * sizeof (NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA));
  if (InterruptMap == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for %d interrupt maps\n", __FUNCTION__, PCIE_NUMBER_OF_INTERRUPT_MAP));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  NumberOfInterruptMaps = PCIE_NUMBER_OF_INTERRUPT_MAP;
  Status                = DeviceTreeGetInterruptMap (RpNodeOffset, InterruptMap, &NumberOfInterruptMaps);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get PCIE interrupt map\n", __FUNCTION__, Status));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  Status = CMTokenProtocol->AllocateTokens (CMTokenProtocol, PCIE_NUMBER_OF_INTERRUPT_MAP, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %d tokens for the InterruptMap token map\n", __FUNCTION__, PCIE_NUMBER_OF_INTERRUPT_MAP));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: NumberOfInterruptMaps = %u\n", __FUNCTION__, NumberOfInterruptMaps));
  if (NumberOfInterruptMaps == 1) {
    for (Index = 0; Index < PCIE_NUMBER_OF_INTERRUPT_MAP; Index++) {
      RpInfo->InterruptRefInfo[Index].ReferenceToken          = TokenMap[Index];
      RpInfo->InterruptMapInfo[Index].PciInterrupt            = Index;
      RpInfo->InterruptMapInfo[Index].IntcInterrupt.Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptMap[0].ParentInterrupt);
      RpInfo->InterruptMapInfo[Index].IntcInterrupt.Flags     = InterruptMap[0].ParentInterrupt.Flag;
    }
  } else if (NumberOfInterruptMaps == PCIE_NUMBER_OF_INTERRUPT_MAP) {
    for (Index = 0; Index < PCIE_NUMBER_OF_INTERRUPT_MAP; Index++) {
      RpInfo->InterruptRefInfo[Index].ReferenceToken          = TokenMap[Index];
      RpInfo->InterruptMapInfo[Index].PciInterrupt            = InterruptMap[Index].ChildInterrupt.Interrupt - 1;
      RpInfo->InterruptMapInfo[Index].IntcInterrupt.Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptMap[Index].ParentInterrupt);
      RpInfo->InterruptMapInfo[Index].IntcInterrupt.Flags     = InterruptMap[Index].ParentInterrupt.Flag;
    }
  } else {
    Status = EFI_DEVICE_ERROR;
    DEBUG ((DEBUG_ERROR, "%a: Expected %d interrupt maps, got %u\r\n", __FUNCTION__, PCIE_NUMBER_OF_INTERRUPT_MAP, NumberOfInterruptMaps));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < PCIE_NUMBER_OF_INTERRUPT_MAP; Index++) {
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: InterruptRefInfo[%u] has PcieInterrupt %d, IntcInterrupt %d, Flags 0x%x\n",
      __FUNCTION__,
      Index,
      RpInfo->InterruptMapInfo[Index].PciInterrupt,
      RpInfo->InterruptMapInfo[Index].IntcInterrupt.Interrupt,
      RpInfo->InterruptMapInfo[Index].IntcInterrupt.Flags
      ));
  }

  FreePool (TokenMap);
  TokenMap = NULL;

  Status = CMTokenProtocol->AllocateTokens (CMTokenProtocol, RpInfo->AddressMapCount, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u tokens for the AddressMap token map\n", __FUNCTION__, RpInfo->AddressMapCount));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < RpInfo->AddressMapCount; Index++) {
    RpInfo->AddressMapRefInfo[Index].ReferenceToken = TokenMap[Index];
  }

  FreePool (TokenMap);
  TokenMap = NULL;

  Index                                 = 0;
  RpInfo->RepoInfo[Index].CmObjectId    = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjCmRef);
  RpInfo->RepoInfo[Index].CmObjectToken = RpInfo->ConfigSpaceInfo.InterruptMapToken;
  RpInfo->RepoInfo[Index].CmObjectSize  = sizeof (CM_ARCH_COMMON_OBJ_REF) * PCIE_NUMBER_OF_INTERRUPT_MAP;
  RpInfo->RepoInfo[Index].CmObjectCount = PCIE_NUMBER_OF_INTERRUPT_MAP;
  RpInfo->RepoInfo[Index].CmObjectPtr   = RpInfo->InterruptRefInfo;
  Index++;

  RpInfo->RepoInfo[Index].CmObjectId    = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjCmRef);
  RpInfo->RepoInfo[Index].CmObjectToken = RpInfo->ConfigSpaceInfo.AddressMapToken;
  RpInfo->RepoInfo[Index].CmObjectSize  = sizeof (CM_ARCH_COMMON_OBJ_REF) * RpInfo->AddressMapCount;
  RpInfo->RepoInfo[Index].CmObjectCount = RpInfo->AddressMapCount;
  RpInfo->RepoInfo[Index].CmObjectPtr   = RpInfo->AddressMapRefInfo;
  Index++;

  for (Index2 = 0; Index2 < PCIE_NUMBER_OF_MAPPING_SPACE; Index2++) {
    RpInfo->RepoInfo[Index].CmObjectId    = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjPciAddressMapInfo);
    RpInfo->RepoInfo[Index].CmObjectToken = RpInfo->AddressMapRefInfo[Index2].ReferenceToken;
    RpInfo->RepoInfo[Index].CmObjectSize  = sizeof (RpInfo->AddressMapInfo[Index2]);
    RpInfo->RepoInfo[Index].CmObjectCount = 1;
    RpInfo->RepoInfo[Index].CmObjectPtr   = &RpInfo->AddressMapInfo[Index2];
    Index++;
  }

  for (Index2 = 0; Index2 < PCIE_NUMBER_OF_INTERRUPT_MAP; Index2++) {
    RpInfo->RepoInfo[Index].CmObjectId    = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjPciInterruptMapInfo);
    RpInfo->RepoInfo[Index].CmObjectToken = RpInfo->InterruptRefInfo[Index2].ReferenceToken;
    RpInfo->RepoInfo[Index].CmObjectSize  = sizeof (RpInfo->InterruptMapInfo[Index2]);
    RpInfo->RepoInfo[Index].CmObjectCount = 1;
    RpInfo->RepoInfo[Index].CmObjectPtr   = &RpInfo->InterruptMapInfo[Index2];
    Index++;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &RpHandle,
                  &gNVIDIAPciHostBridgeProtocolGuid,
                  RootBridge,
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  &RpInfo->PcieRootBridgeConfigurationIo,
                  &gNVIDIAConfigurationManagerDataObjectGuid,
                  &RpInfo->RepoInfo,
                  &gNVIDIAPciConfigurationDataProtocolGuid,
                  &RpInfo->ConfigSpaceInfo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to install root bridge info (%r)\r\n", __FUNCTION__, Status));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (RangesArray);
  FREE_NON_NULL (InterruptMap);
  FREE_NON_NULL (TokenMap);
  if (EFI_ERROR (Status)) {
    if (RootBridge != NULL) {
      FREE_NON_NULL (RootBridge->DevicePath);
    }

    FREE_NON_NULL (RootBridge);
  }

  return Status;
}

/**
  Parse a segment controller node: locate its shared cxl_brg register bank, patch its RP
  subnodes for the current bifurcation config, then parse each RP subnode
  into its own root bridge.

  @param[in] ControllerHandle  Handle of the segment controller.
  @param[in] DeviceTreeNode    Segment controller device tree node.

  @retval EFI_SUCCESS      All RP subnodes parsed (or found disabled).
  @retval others           DT parse or RP parse failure.

**/
EFI_STATUS
ParseSegmentCtlrNode (
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode
  )
{
  EFI_STATUS                        Status   = EFI_SUCCESS;
  PCIE_CONTROLLER_PRIVATE           *Private = NULL;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterData[PCIE_CONTROLLER_MAX_REGISTERS];
  UINT32                            RegisterCount;
  UINT32                            RegisterIndex;
  INT32                             RpNodeOffset;

  if (DeviceTreeNode == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: bad DeviceTreeNode\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  RegisterCount = ARRAY_SIZE (RegisterData);
  Status        = DeviceTreeGetRegisters (DeviceTreeNode->NodeOffset, RegisterData, &RegisterCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetRegisters failed: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  Private = AllocateZeroPool (sizeof (PCIE_CONTROLLER_PRIVATE));
  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate private structure\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DeviceTreeFindRegisterByName ("cxl_brg", RegisterData, RegisterCount, &RegisterIndex);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate cxl_brg address range\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto CleanupAndReturn;
  }

  Private->CxlBrgBase = RegisterData[RegisterIndex].BaseAddress;
  Private->CxlBrgSize = RegisterData[RegisterIndex].Size;
  DEBUG ((DEBUG_INFO, "cxl_brg base = 0x%llx\n", Private->CxlBrgBase));
  DEBUG ((DEBUG_INFO, "cxl_brg size = 0x%llx\n", Private->CxlBrgSize));

  Status = DeviceTreeGetNodePropertyValue32 (DeviceTreeNode->NodeOffset, "linux,pci-domain", &Private->Segment);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Entry \"linux,pci-domain\" not found\n"));
    goto CleanupAndReturn;
  }

  DEBUG ((DEBUG_INFO, "Segment = 0x%x\n", Private->Segment));

  Status = DeviceTreeGetNodePropertyValue32 (DeviceTreeNode->NodeOffset, "nvidia,socket-id", &Private->SocketId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Entry \"nvidia,socket-id\" not found\n"));
    goto CleanupAndReturn;
  }

  DEBUG ((DEBUG_INFO, "Socket-Id = %u\n", Private->SocketId));

  Status = DeviceTreeGetNodePropertyValue32 (DeviceTreeNode->NodeOffset, "nvidia,segmentctlr-id", &Private->SegmentCtlrId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Entry \"nvidia,segmentctlr-id\" not found\n"));
    goto CleanupAndReturn;
  }

  DEBUG ((DEBUG_INFO, "SegmentCtlr-ID = %u\n", Private->SegmentCtlrId));

  /* Since the ConfigReady check is already done in MB2 stage, there is no need to check it here */
  DEBUG ((DEBUG_INFO, "PcieSegmentCtlrConfigReady\n"));

  Status = UpdatePcieNode (Private->SocketId, Private->SegmentCtlrId, DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to update segment controller node (%r)\r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  FdtForEachSubnode (RpNodeOffset, DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset) {
    Status = ParseRpNode (ControllerHandle, RpNodeOffset, DeviceTreeNode->DeviceTreeBase, Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to parse RP node (%u)\r\n", __FUNCTION__, Status));
      goto CleanupAndReturn;
    }
  }

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      FreePool (Private);
    }
  }

  return Status;
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
  EFI_STATUS  Status = EFI_SUCCESS;
  VOID        *Registration;
  EFI_HANDLE  PciPlatformHandle;

  //
  // Install on a new handle
  //
  PciPlatformHandle = NULL;
  if (!gPciPltProtInstalled && !gPciPltProtInstallFailed) {
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &PciPlatformHandle,
                    &gEfiPciPlatformProtocolGuid,
                    &mPciPlatformProtocol,
                    NULL
                    );
    if (!EFI_ERROR (Status)) {
      gPciPltProtInstalled = TRUE;
    } else {
      gPciPltProtInstallFailed = TRUE;
    }
  }

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      Status = ParseSegmentCtlrNode (ControllerHandle, DeviceTreeNode);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to parse segment controller node (%r)\r\n", __FUNCTION__, Status));
      }

      break;

    case DeviceDiscoveryEnumerationCompleted:

      EfiCreateProtocolNotifyEvent (
        &gNVIDIABdsDeviceConnectCompleteGuid,
        TPL_CALLBACK,
        PcieConfigDevices,
        NULL,
        &Registration
        );

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
