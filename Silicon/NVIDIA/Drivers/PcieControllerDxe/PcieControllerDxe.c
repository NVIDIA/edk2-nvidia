/** @file

  PCIe Controller Driver

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

#include <TH500/TH500Definitions.h>
#include <TH500/TH500MB1Configuration.h>

#include "PcieControllerConfigGPU.h"
#include "PcieControllerPrivate.h"
#include <Protocol/PciPlatform.h>

#define PCIE_CONTROLLER_MAX_REGISTERS  6

STATIC BOOLEAN  mPcieDisableOptionRom = FALSE;

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
  .AutoEnableClocks                = FALSE,
  .AutoDeassertReset               = FALSE,
  .AutoDeassertPg                  = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .ThreadedDeviceStart             = TRUE
};

BOOLEAN                           gPciPltProtInstalled = FALSE;
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
BOOLEAN
WaitForBit16 (
  PCIE_CONTROLLER_PRIVATE  *Private,
  UINT16                   *Feat,
  UINT16                   Pos,
  UINT32                   Count,
  UINT32                   TimeUs,
  BOOLEAN                  Status
  )
{
  UINT32  Index = 0;

  while (Index < Count) {
    if (!!(*Feat & BIT (Pos)) != Status) {
      if (Private->C2cInitRequired) {
        MicroSecondDelay (TimeUs);
      } else {
        DeviceDiscoveryThreadMicroSecondDelay (TimeUs);
      }

      Index++;
    } else {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
VOID
RetrainLink (
  PCIE_CONTROLLER_PRIVATE  *Private
  )
{
  PCI_CAPABILITY_PCIEXP  *PciExpCap = (PCI_CAPABILITY_PCIEXP *)(Private->EcamBase + Private->PCIeCapOff);

  /* Wait for previous link training to complete */
  if (WaitForBit16 (Private, &PciExpCap->LinkStatus.Uint16, 11, 10000, 100, FALSE)) {
    /* Clear Link Bandwith */
    PciExpCap->LinkStatus.Bits.LinkBandwidthManagement = 1;

    /* Set Retrain Link */
    PciExpCap->LinkControl2.Bits.TargetLinkSpeed = PciExpCap->LinkCapability.Bits.MaxLinkSpeed;
    PciExpCap->LinkControl.Bits.RetrainLink      = 1;

    /* Retraining: Wait for link training to clear */
    if (WaitForBit16 (Private, &PciExpCap->LinkStatus.Uint16, 11, 10000, 100, FALSE)) {
      /* Wait for Link Bandwith set */
      if (WaitForBit16 (Private, &PciExpCap->LinkStatus.Uint16, 14, 10000, 100, TRUE)) {
        /* Clear Link Bandwith */
        PciExpCap->LinkStatus.Bits.LinkBandwidthManagement = 1;
        /* Wait for 20 ms for link to appear */
        if (Private->C2cInitRequired) {
          MicroSecondDelay (20*1000);
        } else {
          DeviceDiscoveryThreadMicroSecondDelay (20*1000);
        }

        DEBUG ((
          DEBUG_ERROR,
          "PCIe Socket-0x%x:Ctrl-0x%x Link Status after re-train (Capable: Gen-%d,x%d  Negotiated: Gen-%d,x%d)\r\n",
          Private->SocketId,
          Private->CtrlId,
          PciExpCap->LinkCapability.Bits.MaxLinkSpeed,
          PciExpCap->LinkCapability.Bits.MaxLinkWidth,
          PciExpCap->LinkStatus.Bits.CurrentLinkSpeed,
          PciExpCap->LinkStatus.Bits.NegotiatedLinkWidth
          ));
      } else {
        DEBUG ((DEBUG_ERROR, "PCIe Socket-0x%x:Ctrl-0x%x wait for Link Bandwith Timeout\r\n", Private->SocketId, Private->CtrlId));
      }
    } else {
      DEBUG ((DEBUG_ERROR, "PCIe Socket-0x%x:Ctrl-0x%x Link Retrain Timeout\r\n", Private->SocketId, Private->CtrlId));
    }
  } else {
    DEBUG ((DEBUG_ERROR, "PCIe Socket-0x%x:Ctrl-0x%x Previous Link train Timeout\r\n", Private->SocketId, Private->CtrlId));
  }
}

STATIC
EFI_STATUS
EFIAPI
ReadSenseGpio (
  IN  PCIE_CONTROLLER_PRIVATE  *Private,
  IN  EMBEDDED_GPIO            *Gpio,
  OUT BOOLEAN                  *Sensed
  )
{
  EFI_STATUS  Status;
  UINTN       SenseCount;
  UINTN       Value;

  if ((Private == NULL) || (Gpio == NULL) || (Sensed == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (SenseCount = 0; SenseCount < GPU_SENSE_MAX_COUNT; SenseCount++) {
    Status = Gpio->Get (Gpio, Private->GpuKickGpioSense, &Value);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Gpio get failed: %r\r\n", Status));
      return Status;
    }

    if (Value == 0) {
      *Sensed = TRUE;
      break;
    }

    MicroSecondDelay (GPU_SENSE_DELAY);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
ToggleKickGpio (
  IN PCIE_CONTROLLER_PRIVATE  *Private,
  IN EMBEDDED_GPIO            *Gpio
  )
{
  EFI_STATUS  Status;

  if ((Private == NULL) || (Gpio == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = Gpio->Set (Gpio, Private->GpuKickGpioReset, GPIO_MODE_OUTPUT_0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Gpio set failed: %r\r\n", Status));
    return Status;
  }

  DeviceDiscoveryThreadMicroSecondDelay (GPU_RESET_DELAY);

  Status = Gpio->Set (Gpio, Private->GpuKickGpioReset, GPIO_MODE_OUTPUT_1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Gpio set failed: %r\r\n", Status));
    return Status;
  }

  DeviceDiscoveryThreadMicroSecondDelay (2 * GPU_RESET_DELAY);

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
SenseGpu (
  IN PCIE_CONTROLLER_PRIVATE  *Private,
  IN EFI_HANDLE               ControllerHandle
  )
{
  EFI_STATUS     Status;
  EMBEDDED_GPIO  *Gpio;
  BOOLEAN        GPUSensed;
  UINT32         KickCount;

  if (!Private->GpuKickGpioSupported) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gEmbeddedGpioProtocolGuid, NULL, (VOID **)&Gpio);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get GPIO protocol - %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  KickCount = 0;
  GPUSensed = FALSE;

  Status = ReadSenseGpio (Private, Gpio, &GPUSensed);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Gpio sense failed: %r\r\n", Status));
    return Status;
  }

  if (GPUSensed == TRUE) {
    return Status;
  }

  for (KickCount = 0; KickCount < GPU_KICK_MAX_COUNT; KickCount++) {
    Status = ToggleKickGpio (Private, Gpio);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Gpio kick toggle failed: %r\r\n", Status));
      return Status;
    }

    Status = ReadSenseGpio (Private, Gpio, &GPUSensed);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Gpio sense failed: %r\r\n", Status));
      return Status;
    }

    if (GPUSensed == TRUE) {
      return Status;
    }
  }

  return EFI_NOT_READY;
}

STATIC
EFI_STATUS
EFIAPI
InitializeController (
  PCIE_CONTROLLER_PRIVATE  *Private,
  IN  EFI_HANDLE           ControllerHandle
  )
{
  UINT64                 val;
  EFI_STATUS             Status;
  PCI_CAPABILITY_PCIEXP  *PciExpCap = NULL;
  UINT8                  C2cStatus;
  VOID                   *Hob;

  TEGRABL_EARLY_BOOT_VARIABLES  *Mb1Config = NULL;
  UINTN                         ChipId;

  ChipId = TegraGetChipID ();

  if (ChipId == TH500_CHIP_ID) {
    Hob = GetFirstGuidHob (&gNVIDIATH500MB1DataGuid);
    if ((Hob != NULL) &&
        (GET_GUID_HOB_DATA_SIZE (Hob) == (sizeof (TEGRABL_EARLY_BOOT_VARIABLES) * PcdGet32 (PcdTegraMaxSockets))))
    {
      Mb1Config = (TEGRABL_EARLY_BOOT_VARIABLES *)GET_GUID_HOB_DATA (Hob);
    }

    NV_ASSERT_RETURN (Mb1Config != NULL, return EFI_DEVICE_ERROR, "%a: no HOB list\n", __FUNCTION__);
  }

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

  DEBUG ((DEBUG_VERBOSE, "Programming XAL_RC registers is done\r\n"));

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

  Private->PCIeCapOff = PCIeFindCap (Private->EcamBase, EFI_PCI_CAPABILITY_ID_PCIEXP);
  if (!Private->PCIeCapOff) {
    DEBUG ((DEBUG_VERBOSE, "Failed to find PCIe capability registers\r\n"));
    return EFI_NOT_FOUND;
  }

  /* Wait for 100ms before releasing PERST# */
  MicroSecondDelay (100 * 1000);

  val  = MmioRead32 (Private->XtlPriBase + XTL_RC_MGMT_PERST_CONTROL);
  val |= XTL_RC_MGMT_PERST_CONTROL_PERST_O_N;
  MmioWrite32 (Private->XtlPriBase + XTL_RC_MGMT_PERST_CONTROL, val);

  /* Wait for link up */
  PciExpCap = (PCI_CAPABILITY_PCIEXP *)(Private->EcamBase + Private->PCIeCapOff);

  if ( WaitForBit16 (Private, &PciExpCap->LinkStatus.Uint16, 13, 10000, 100, TRUE)) {
    DEBUG ((
      DEBUG_ERROR,
      "PCIe Socket-0x%x:Ctrl-0x%x Link is UP (Capable: Gen-%d,x%d  Negotiated: Gen-%d,x%d)\r\n",
      Private->SocketId,
      Private->CtrlId,
      PciExpCap->LinkCapability.Bits.MaxLinkSpeed,
      PciExpCap->LinkCapability.Bits.MaxLinkWidth,
      PciExpCap->LinkStatus.Bits.CurrentLinkSpeed,
      PciExpCap->LinkStatus.Bits.NegotiatedLinkWidth
      ));

    if (ChipId == TH500_CHIP_ID) {
      /**
       * Re-train link if disable_ltssm_auto_train set in BCT.
       */
      if (Mb1Config->Data.Mb1Data.PcieConfig[Private->SocketId][Private->CtrlId].DisableLTSSMAutoTrain) {
        RetrainLink (Private);
      }
    }

    if (Private->C2cInitRequired) {
      DEBUG ((DEBUG_ERROR, "%a: Requesting C2C Initialization\r\n", __FUNCTION__));
      Status = Private->C2cProtocol->Init (Private->C2cProtocol, Private->C2cProtocol->Partitions, &C2cStatus);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: C2C initialization mrq failed: %r\r\n", __FUNCTION__, Status));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: C2C initialization mrq successful.\r\n", __FUNCTION__));
        if (C2cStatus == C2C_STATUS_C2C_LINK_TRAIN_PASS) {
          DEBUG ((DEBUG_ERROR, "%a: C2C link training successful.\r\n", __FUNCTION__));
          Private->C2cInitSuccessful = TRUE;
        } else {
          DEBUG ((DEBUG_ERROR, "%a: C2C link training failed with error code: 0x%x\r\n", __FUNCTION__, C2cStatus));
        }
      }

      Private->PcieRootBridgeConfigurationIo.BpmpPhandle = Private->C2cProtocol->BpmpPhandle;
    }
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "PCIe Socket-0x%x:Ctrl-0x%x Link is DOWN (Capable: Gen-%d,x%d)\r\n",
      Private->SocketId,
      Private->CtrlId,
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

STATIC
UINT8
PcieFindCap (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT8                CapId
  )
{
  UINT16  CapabilityEntry;
  UINT8   CapabilityPtr;
  UINT8   CapabilityID;

  CapabilityPtr = 0;
  PciIo->Pci.Read (
               PciIo,
               EfiPciIoWidthUint8,
               PCI_CAPBILITY_POINTER_OFFSET,
               1,
               &CapabilityPtr
               );

  while ((CapabilityPtr >= 0x40) && ((CapabilityPtr & 0x03) == 0x00)) {
    PciIo->Pci.Read (
                 PciIo,
                 EfiPciIoWidthUint16,
                 CapabilityPtr,
                 1,
                 &CapabilityEntry
                 );

    CapabilityID = (UINT8)CapabilityEntry;

    if (CapabilityID == CapId) {
      return CapabilityPtr;
    }

    /*
     * Certain PCI device may incorrectly have capability pointing to itself,
     * break to avoid dead loop.
     */
    if (CapabilityPtr == (UINT8)(CapabilityEntry >> 8)) {
      return 0;
    }

    CapabilityPtr = (UINT8)(CapabilityEntry >> 8);
  }

  return 0;
}

STATIC
UINT16
PcieFindExtCap (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT16               CapId
  )
{
  EFI_STATUS  Status;
  UINT32      CapabilityPtr;
  UINT32      CapabilityEntry;
  UINT16      CapabilityID;
  UINTN       Segment, Bus, Device, Function;

  CapabilityPtr = EFI_PCIE_CAPABILITY_BASE_OFFSET;

  while (CapabilityPtr != 0) {
    /* Mask it to DWORD alignment per PCI spec */
    CapabilityPtr &= 0xFFC;
    Status         = PciIo->Pci.Read (
                                  PciIo,
                                  EfiPciIoWidthUint32,
                                  CapabilityPtr,
                                  1,
                                  &CapabilityEntry
                                  );
    if (EFI_ERROR (Status)) {
      break;
    }

    if (CapabilityEntry == MAX_UINT32) {
      Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
      ASSERT_EFI_ERROR (Status);
      DEBUG ((
        DEBUG_WARN,
        "%a: [%04x:%02x:%02x.%x] failed to access config space at offset 0x%x\n",
        __FUNCTION__,
        Segment,
        Bus,
        Device,
        Function,
        CapabilityPtr
        ));
      break;
    }

    CapabilityID = (UINT16)CapabilityEntry;

    if (CapabilityID == CapId) {
      return CapabilityPtr;
    }

    CapabilityPtr = (CapabilityEntry >> 20) & 0xFFF;
  }

  return 0;
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
  EFI_STATUS                    Status;
  UINTN                         Segment, Bus, Device, Function;
  PCI_REG_PCIE_CAPABILITY       Capability;
  UINT32                        PciExpCapOffset, AerCapOffset, Offset, DevCapOffset;
  PCI_REG_PCIE_ROOT_CONTROL     RootControl;
  PCI_REG_PCIE_DEVICE_CONTROL   DeviceControl;
  PCI_REG_PCIE_DEVICE_STATUS    DeviceStatus;
  UINT32                        Val_32;
  UINT16                        Val_16;
  UINT32                        Socket, Ctrl;
  VOID                          *Hob;
  TEGRABL_EARLY_BOOT_VARIABLES  *Mb1Config = NULL;
  UINTN                         ChipId;
  BOOLEAN                       SkipDPCEnable = FALSE;

  ChipId = TegraGetChipID ();

  if (ChipId == TH500_CHIP_ID) {
    Hob = GetFirstGuidHob (&gNVIDIATH500MB1DataGuid);
    if ((Hob != NULL) &&
        (GET_GUID_HOB_DATA_SIZE (Hob) == (sizeof (TEGRABL_EARLY_BOOT_VARIABLES) * PcdGet32 (PcdTegraMaxSockets))))
    {
      Mb1Config = (TEGRABL_EARLY_BOOT_VARIABLES *)GET_GUID_HOB_DATA (Hob);
    }

    ASSERT (Mb1Config);
  }

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);

  Socket = (Segment >> 4) & 0xF;
  Ctrl   = (Segment) & 0xF;

  PciExpCapOffset = PcieFindCap (PciIo, EFI_PCI_CAPABILITY_ID_PCIEXP);

  if (!PciExpCapOffset) {
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

  AerCapOffset = PcieFindExtCap (PciIo, PCI_EXPRESS_EXTENDED_CAPABILITY_ADVANCED_ERROR_REPORTING_ID);
  if (AerCapOffset) {
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

    // Make UR based on option setting
    Offset = AerCapOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_ADVANCED_ERROR_REPORTING, UncorrectableErrorMask);
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Offset, 1, &Val_32);
    if (Status == EFI_SUCCESS) {
      // bit 20 - UR masking
      if (Mb1Config && Mb1Config->Data.Mb1Data.PcieConfig[Socket][Ctrl].MaskUnsupportedRequest) {
        DEBUG ((DEBUG_INFO, "Device [%04x:%02x:%02x.%x] : Enable unsupported request AER\n", Segment, Bus, Device, Function));
        Val_32 |= (1<<20);
      } else {
        DEBUG ((DEBUG_INFO, "Device [%04x:%02x:%02x.%x] : Disable unsupported request AER\n", Segment, Bus, Device, Function));
        Val_32 &= ~(1<<20);
      }

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

      /*
       * In the case of DPC capable PCIe switch connected to RP, disable the
       * DPC at RP and keep DPC enabled in the PCIe switch. This makes sure that
       * any malfunctioning device is contained at switch downstream port
       * level and RP is saved from going into containment
       */
      if (ChipId == TH500_CHIP_ID) {
        if ((Bus == 0) &&
            Mb1Config->Data.Mb1Data.PcieConfig[Socket][Ctrl].DisableDPCAtRP)
        {
          SkipDPCEnable = TRUE;
        }
      }

      Offset = PcieFindExtCap (PciIo, PCI_EXPRESS_EXTENDED_CAPABILITY_DPC_ID);
      if (Offset) {
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
          RpDpcCapOffset = PcieFindExtCap (RPPciIo, PCI_EXPRESS_EXTENDED_CAPABILITY_DPC_ID);
          if (RpDpcCapOffset) {
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

      DeviceControl.Bits.CorrectableError = 1;
      DeviceControl.Bits.NonFatalError    = 1;
      DeviceControl.Bits.FatalError       = 1;
      // Set UR based on option
      if (Mb1Config && Mb1Config->Data.Mb1Data.PcieConfig[Socket][Ctrl].MaskUnsupportedRequest) {
        DEBUG ((DEBUG_INFO, "Device [%04x:%02x:%02x.%x] : Enable unspported request\n", Segment, Bus, Device, Function));
        DeviceControl.Bits.UnsupportedRequest = 0;
      } else {
        DEBUG ((DEBUG_INFO, "Device [%04x:%02x:%02x.%x] : Disable unspported request\n", Segment, Bus, Device, Function));
        DeviceControl.Bits.UnsupportedRequest = 1;
      }

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
  EFI_STATUS                    Status;
  UINTN                         Segment, Bus, Device, Function;
  UINT32                        AerCapOffset, Offset;
  UINT32                        Val, New_Val;
  UINT32                        Socket, Ctrl;
  VOID                          *Hob;
  TEGRABL_EARLY_BOOT_VARIABLES  *Mb1Config = NULL;
  UINTN                         ChipId;

  ChipId = TegraGetChipID ();
  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);

  Socket = (Segment >> 4) & 0xF;
  Ctrl   = (Segment) & 0xF;

  if (ChipId == TH500_CHIP_ID) {
    Hob = GetFirstGuidHob (&gNVIDIATH500MB1DataGuid);
    if ((Hob != NULL) &&
        (GET_GUID_HOB_DATA_SIZE (Hob) == (sizeof (TEGRABL_EARLY_BOOT_VARIABLES) * PcdGet32 (PcdTegraMaxSockets))))
    {
      Mb1Config = (TEGRABL_EARLY_BOOT_VARIABLES *)GET_GUID_HOB_DATA (Hob);
    }

    ASSERT (Mb1Config);

    if (!Mb1Config->Data.Mb1Data.PcieConfig[Socket][Ctrl].EnableECRC) {
      DEBUG ((
        DEBUG_INFO,
        "Device [%04x:%02x:%02x.%x] : Skipping ECRC Enable\n",
        Segment,
        Bus,
        Device,
        Function
        ));

      return EFI_SUCCESS;
    }
  }

  AerCapOffset = PcieFindExtCap (PciIo, PCI_EXPRESS_EXTENDED_CAPABILITY_ADVANCED_ERROR_REPORTING_ID);
  if (AerCapOffset) {
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

  PciExpCapOffset = PcieFindCap (PciIo, EFI_PCI_CAPABILITY_ID_PCIEXP);
  if (PciExpCapOffset == 0) {
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
  PciExpCapOffset = PcieFindCap (PciIo, EFI_PCI_CAPABILITY_ID_PCIEXP);
  Offset          = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl);
  Status          = PciIo->Pci.Read (
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

  Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability2);
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        Offset,
                        1,
                        &DeviceCapability2.Uint32
                        );

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
  PcieConfigGPUDevice (PciIo);
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
  PCI_ROOT_BRIDGE                              *RootBridge          = NULL;
  EFI_DEVICE_PATH_PROTOCOL                     *ParentDevicePath    = NULL;
  CONST VOID                                   *BusProperty         = NULL;
  CONST UINT32                                 *GpuKickGpioProperty = NULL;
  CONST UINT32                                 *PxmDmnStartProperty = NULL;
  CONST UINT32                                 *NumPxmDmnProperty   = NULL;
  INT32                                        PropertySize         = 0;
  CONST VOID                                   *SegmentNumber       = NULL;
  CONST VOID                                   *CtrlId              = NULL;
  CONST VOID                                   *SocketId            = NULL;
  PCIE_CONTROLLER_PRIVATE                      *Private             = NULL;
  EFI_EVENT                                    ExitBootServiceEvent;
  UINT32                                       NumberOfInterruptMaps;
  UINT32                                       Index;
  UINT32                                       Index2;
  BOOLEAN                                      PcieFound;
  NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA        *InterruptMap;
  INT32                                        RPNodeOffset;
  UINTN                                        ChipId;
  VOID                                         *Registration;
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  *CMTokenProtocol;
  CM_OBJECT_TOKEN                              *TokenMap;
  TEGRA_PLATFORM_TYPE                          PlatformType;
  VOID                                         *Hob;
  TEGRABL_EARLY_BOOT_VARIABLES                 *Mb1Config = NULL;
  NVIDIA_DEVICE_TREE_REGISTER_DATA             RegisterData[PCIE_CONTROLLER_MAX_REGISTERS];
  UINT32                                       RegisterCount;
  UINT32                                       RegisterIndex;
  EFI_HANDLE                                   PciPlatformHandle;
  UINT32                                       NumberOfRanges;
  NVIDIA_DEVICE_TREE_RANGES_DATA               *RangesArray;
  UINT32                                       RangeIndex;

  PlatformType = TegraGetPlatform ();
  Status       = EFI_SUCCESS;
  PcieFound    = FALSE;
  ChipId       = TegraGetChipID ();

  if (ChipId == TH500_CHIP_ID) {
    Hob = GetFirstGuidHob (&gNVIDIATH500MB1DataGuid);
    if ((Hob != NULL) &&
        (GET_GUID_HOB_DATA_SIZE (Hob) == (sizeof (TEGRABL_EARLY_BOOT_VARIABLES) * PcdGet32 (PcdTegraMaxSockets))))
    {
      Mb1Config = (TEGRABL_EARLY_BOOT_VARIABLES *)GET_GUID_HOB_DATA (Hob);
    }

    ASSERT (Mb1Config);
  }

  //
  // Install on a new handle
  //
  PciPlatformHandle = NULL;
  if (!gPciPltProtInstalled) {
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &PciPlatformHandle,
                    &gEfiPciPlatformProtocolGuid,
                    &mPciPlatformProtocol,
                    NULL
                    );
    gPciPltProtInstalled = TRUE;
  }

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

      Status = DeviceTreeFindRegisterByName ("xal", RegisterData, RegisterCount, &RegisterIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate XAL address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Private->XalBase = RegisterData[RegisterIndex].BaseAddress;
      Private->XalSize = RegisterData[RegisterIndex].Size;

      Status = DeviceTreeFindRegisterByName ("xtl", RegisterData, RegisterCount, &RegisterIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate XTL address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Private->XtlBase = RegisterData[RegisterIndex].BaseAddress;
      Private->XtlSize = RegisterData[RegisterIndex].Size;

      Status = DeviceTreeFindRegisterByName ("xtl-pri", RegisterData, RegisterCount, &RegisterIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate XTL-PRI address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Private->XtlPriBase = RegisterData[RegisterIndex].BaseAddress;
      Private->XtlPriSize = RegisterData[RegisterIndex].Size;

      Status = DeviceTreeFindRegisterByName ("ecam", RegisterData, RegisterCount, &RegisterIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate ECAM address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Private->EcamBase = RegisterData[RegisterIndex].BaseAddress;
      Private->EcamSize = RegisterData[RegisterIndex].Size;

      Private->Signature                                   = PCIE_CONTROLLER_SIGNATURE;
      Private->PcieRootBridgeConfigurationIo.Read          = PcieConfigurationRead;
      Private->PcieRootBridgeConfigurationIo.Write         = PcieConfigurationWrite;
      Private->PcieRootBridgeConfigurationIo.EcamBase      = Private->EcamBase;
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

      DEBUG ((DEBUG_INFO, "Segment Number = 0x%x\n", Private->PcieRootBridgeConfigurationIo.SegmentNumber));

      if (ChipId == TH500_CHIP_ID) {
        CtrlId = fdt_getprop (
                   DeviceTreeNode->DeviceTreeBase,
                   DeviceTreeNode->NodeOffset,
                   "nvidia,controller-id",
                   &PropertySize
                   );
        if ((CtrlId == NULL) || (PropertySize != sizeof (UINT32))) {
          DEBUG ((DEBUG_ERROR, "Failed to read Controller ID\n"));
        } else {
          CopyMem (&Private->CtrlId, CtrlId, sizeof (UINT32));
          Private->CtrlId = SwapBytes32 (Private->CtrlId);
        }

        SocketId = fdt_getprop (
                     DeviceTreeNode->DeviceTreeBase,
                     DeviceTreeNode->NodeOffset,
                     "nvidia,socket-id",
                     &PropertySize
                     );
        if ((SocketId == NULL) || (PropertySize != sizeof (UINT32))) {
          DEBUG ((DEBUG_ERROR, "Failed to read Socket ID\n"));
        } else {
          CopyMem (&Private->SocketId, SocketId, sizeof (UINT32));
          Private->SocketId = SwapBytes32 (Private->SocketId);
        }
      } else {
        DEBUG ((DEBUG_ERROR, "%a: unsupported chip=0x%x\n", __FUNCTION__, ChipId));
        ASSERT (FALSE);
      }

      Private->PcieRootBridgeConfigurationIo.ControllerID = Private->CtrlId;
      DEBUG ((DEBUG_INFO, "Controller-ID = 0x%x\n", Private->CtrlId));

      Private->PcieRootBridgeConfigurationIo.SocketID = Private->SocketId;
      DEBUG ((DEBUG_INFO, "Socket-ID = 0x%x\n", Private->SocketId));

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

      Private->PcieRootBridgeConfigurationIo.OSCCtrl = (PCIE_FW_OSC_CTRL_PCIE_NATIVE_HP |
                                                        PCIE_FW_OSC_CTRL_PCIE_CAP_STRUCTURE |
                                                        PCIE_FW_OSC_CTRL_LTR);

      if (ChipId == TH500_CHIP_ID) {
        if (Mb1Config->Data.Mb1Data.PcieConfig[Private->SocketId][Private->CtrlId].OsNativeAER) {
          /* As per PCIe spec recommendation, both AER and DPC capabilities are
           * expected to be owned by the same SW entity i.e. either Firmware
           * or the Operation system. Hence, give the ownership of both AER and
           * DPC to the OS.
           */

          Private->PcieRootBridgeConfigurationIo.OSCCtrl |= (PCIE_FW_OSC_CTRL_PCIE_AER |
                                                             PCIE_FW_OSC_CTRL_PCIE_DPC);
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

      NumberOfRanges = 0;
      Status         = DeviceTreeGetRanges (DeviceTreeNode->NodeOffset, "ranges", NULL, &NumberOfRanges);
      if (EFI_ERROR (Status) && (Status != EFI_BUFFER_TOO_SMALL)) {
        DEBUG ((DEBUG_ERROR, "PCIe Controller: Unsupported ranges configuration: %r\r\n", Status));
        break;
      }

      if (NumberOfRanges == 0) {
        DEBUG ((DEBUG_ERROR, "PCIe Controller: Unsupported ranges configuration: No ranges found\r\n"));
        Status = EFI_UNSUPPORTED;
        break;
      }

      RangesArray = AllocatePool (sizeof (NVIDIA_DEVICE_TREE_RANGES_DATA) * NumberOfRanges);
      if (RangesArray == NULL) {
        DEBUG ((DEBUG_ERROR, "PCIe Controller: Unable to allocate space for %u ranges\n", NumberOfRanges));
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }

      Status = DeviceTreeGetRanges (DeviceTreeNode->NodeOffset, "ranges", RangesArray, &NumberOfRanges);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "PCIe Controller: Unable to get ranges: %r\r\n", Status));
        break;
      }

      // Mark all regions as unsupported
      RootBridge->Io.Base          = MAX_UINT64;
      RootBridge->Mem.Base         = MAX_UINT64;
      RootBridge->MemAbove4G.Base  = MAX_UINT64;
      RootBridge->PMem.Base        = MAX_UINT64;
      RootBridge->PMemAbove4G.Base = MAX_UINT64;

      for (RangeIndex = 0; RangeIndex < NumberOfRanges; RangeIndex++) {
        UINT32   Flags         = 0;
        UINT32   Space         = 0;
        BOOLEAN  Prefetchable  = FALSE;
        UINT64   DeviceAddress = 0;
        UINT64   HostAddress   = 0;
        UINT64   Limit         = 0;
        UINT64   Size          = 0;
        UINT64   Translation   = 0;

        ASSERT (Private->AddressMapCount < PCIE_NUMBER_OF_MAPPING_SPACE);

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
              Private->MemBase  = HostAddress;
              Private->MemLimit = HostAddress + Size - 1;
              // Split translated region into prefetchable and non-prefetchable
              Size                                                          = Size/2;
              RootBridge->Mem.Base                                          = DeviceAddress;
              RootBridge->Mem.Limit                                         = DeviceAddress + Size - 1;
              RootBridge->Mem.Translation                                   = Translation;
              Private->AddressMapInfo[Private->AddressMapCount].PciAddress  = DeviceAddress;
              Private->AddressMapInfo[Private->AddressMapCount].CpuAddress  = HostAddress;
              Private->AddressMapInfo[Private->AddressMapCount].AddressSize = Size;
              Private->AddressMapInfo[Private->AddressMapCount].SpaceCode   = 3;
              DEBUG ((DEBUG_INFO, "MEM32: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", RootBridge->Mem.Base, RootBridge->Mem.Limit, RootBridge->Mem.Translation));
              Private->AddressMapCount++;
              ASSERT (Private->AddressMapCount < PCIE_NUMBER_OF_MAPPING_SPACE);
              DeviceAddress                                               = DeviceAddress + Size;
              HostAddress                                                 = HostAddress + Size;
              RootBridge->PMem.Base                                       = DeviceAddress;
              RootBridge->PMem.Limit                                      = DeviceAddress + Size - 1;
              RootBridge->PMem.Translation                                = DeviceAddress - HostAddress;
              Private->AddressMapInfo[Private->AddressMapCount].SpaceCode = 3;
              DEBUG ((DEBUG_INFO, "PREF32: DevAddr = 0x%lX Limit = 0x%lX Trans = 0x%lX\n", RootBridge->PMem.Base, RootBridge->PMem.Limit, RootBridge->PMem.Translation));
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
        ASSERT (Private->AddressMapCount < PCIE_NUMBER_OF_MAPPING_SPACE);
        Private->AddressMapCount++;
      }

      if (EFI_ERROR (Status)) {
        break;
      }

      GpuKickGpioProperty = fdt_getprop (
                              DeviceTreeNode->DeviceTreeBase,
                              DeviceTreeNode->NodeOffset,
                              "nvidia,gpukick-gpio",
                              &PropertySize
                              );
      if ((GpuKickGpioProperty != NULL) && (PropertySize == (6 * sizeof (UINT32)))) {
        Private->GpuKickGpioSense     = GPIO (SwapBytes32 (GpuKickGpioProperty[0]), SwapBytes32 (GpuKickGpioProperty[1]));
        Private->GpuKickGpioReset     = GPIO (SwapBytes32 (GpuKickGpioProperty[3]), SwapBytes32 (GpuKickGpioProperty[4]));
        Private->GpuKickGpioSupported = TRUE;
      }

      if ((RootBridge->PMem.Base == MAX_UINT64) && (RootBridge->PMemAbove4G.Base == MAX_UINT64)) {
        RootBridge->AllocationAttributes |= EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
      }

      Private->BusMask = RootBridge->Bus.Limit;

      Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAC2cNodeProtocolGuid, (VOID **)&Private->C2cProtocol);
      if (!EFI_ERROR (Status)) {
        Private->C2cInitRequired = TRUE;
      }

      Status = SenseGpu (Private, ControllerHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to sense gpu (%r)\r\n", __FUNCTION__, Status));
      }

      Private->PcieRootBridgeConfigurationIo.BpmpPhandle = MAX_UINT32;
      Status                                             = InitializeController (Private, ControllerHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to initialize controller (%r)\r\n", __FUNCTION__, Status));
        break;
      }

      if ((Private->C2cInitRequired && Private->C2cInitSuccessful) ||
          ((ChipId == TH500_CHIP_ID) && (PlatformType == TEGRA_PLATFORM_VDK)))
      {
        NumberOfRanges = 1;
        Status         = DeviceTreeGetRanges (DeviceTreeNode->NodeOffset, "hbm-ranges", RangesArray, &NumberOfRanges);
        if (!EFI_ERROR (Status)) {
          Private->PcieRootBridgeConfigurationIo.HbmRangeStart = RangesArray[0].ParentAddress;
          Private->PcieRootBridgeConfigurationIo.HbmRangeSize  = RangesArray[0].Size;

          PxmDmnStartProperty = fdt_getprop (
                                  DeviceTreeNode->DeviceTreeBase,
                                  DeviceTreeNode->NodeOffset,
                                  "pxm-domain-start",
                                  &PropertySize
                                  );
          if (PxmDmnStartProperty != NULL) {
            Private->PcieRootBridgeConfigurationIo.ProximityDomainStart = SwapBytes32 (PxmDmnStartProperty[0]);
          } else if (ChipId == TH500_CHIP_ID) {
            Private->PcieRootBridgeConfigurationIo.ProximityDomainStart = TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (Private->PcieRootBridgeConfigurationIo.SocketID);
          }

          NumPxmDmnProperty = fdt_getprop (
                                DeviceTreeNode->DeviceTreeBase,
                                DeviceTreeNode->NodeOffset,
                                "num-pxm-domain",
                                &PropertySize
                                );
          if (NumPxmDmnProperty != NULL) {
            Private->PcieRootBridgeConfigurationIo.NumProximityDomains = SwapBytes32 (NumPxmDmnProperty[0]);
          } else if (ChipId == TH500_CHIP_ID) {
            Private->PcieRootBridgeConfigurationIo.NumProximityDomains = TH500_GPU_MAX_NR_MEM_PARTITIONS;
          }
        } else {
          DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get hbm-ranges\n", __FUNCTION__, Status));
        }
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
      if (NumberOfInterruptMaps == 1) {
        for (Index = 0; Index < PCIE_NUMBER_OF_INTERRUPT_MAP; Index++) {
          Private->InterruptRefInfo[Index].ReferenceToken          = TokenMap[Index];
          Private->InterruptMapInfo[Index].PciInterrupt            = Index;
          Private->InterruptMapInfo[Index].IntcInterrupt.Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptMap[0].ParentInterrupt);
          Private->InterruptMapInfo[Index].IntcInterrupt.Flags     = InterruptMap[0].ParentInterrupt.Flag;
        }
      } else if (NumberOfInterruptMaps == PCIE_NUMBER_OF_INTERRUPT_MAP) {
        for (Index = 0; Index < PCIE_NUMBER_OF_INTERRUPT_MAP; Index++) {
          Private->InterruptRefInfo[Index].ReferenceToken          = TokenMap[Index];
          Private->InterruptMapInfo[Index].PciInterrupt            = InterruptMap[Index].ChildInterrupt.Interrupt - 1;
          Private->InterruptMapInfo[Index].IntcInterrupt.Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptMap[Index].ParentInterrupt);
          Private->InterruptMapInfo[Index].IntcInterrupt.Flags     = InterruptMap[Index].ParentInterrupt.Flag;
        }
      } else {
        Status = EFI_DEVICE_ERROR;
        DEBUG ((DEBUG_ERROR, "%a: Expected %d interrupt maps, got %u\r\n", __FUNCTION__, PCIE_NUMBER_OF_INTERRUPT_MAP, NumberOfInterruptMaps));
        FreePool (InterruptMap);
        break;
      }

      for (Index = 0; Index < PCIE_NUMBER_OF_INTERRUPT_MAP; Index++) {
        DEBUG ((DEBUG_VERBOSE, "%a: InterruptRefInfo[%u] has PcieInterrupt %d, IntcInterrupt %d, Flags 0x%x\n", __FUNCTION__, Index, Private->InterruptMapInfo[Index].PciInterrupt, Private->InterruptMapInfo[Index].IntcInterrupt.Interrupt, Private->InterruptMapInfo[Index].IntcInterrupt.Flags));
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
