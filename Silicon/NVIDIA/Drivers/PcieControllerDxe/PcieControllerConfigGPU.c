/** @file

  PCIe Controller GPU specific configuration

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/PciIo.h>

#include "PcieControllerConfigGPU.h"

#define PCIE_SUBSYSTEM_VEN_ID_NVIDIA  (0x10DE)

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
EFI_PCI_IO_PROTOCOL *
GetParent (
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

EFI_STATUS
PcieConfigGPUDevice (
  EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS                       Status;
  UINTN                            Segment, Bus, Device, Function;
  UINTN                            PSegment, PBus, PDevice, PFunction;
  PCI_REG_PCIE_CAPABILITY          Capability;
  UINT32                           PciExpCapOffset, ParentPciExpCapOffset, Offset;
  PCI_REG_PCIE_DEVICE_CAPABILITY   DeviceCapability;
  PCI_REG_PCIE_DEVICE_CONTROL      DeviceControl;
  PCI_REG_PCIE_DEVICE_CAPABILITY2  DeviceCapability2;
  PCI_REG_PCIE_DEVICE_CONTROL2     DeviceControl2;
  PCI_TYPE00                       Type0_Cfg;

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  ASSERT_EFI_ERROR (Status);

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

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        0,
                        sizeof (Type0_Cfg),
                        &Type0_Cfg
                        );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  if (((Capability.Bits.DevicePortType == PCIE_DEVICE_PORT_TYPE_PCIE_ENDPOINT) ||
       (Capability.Bits.DevicePortType == PCIE_DEVICE_PORT_TYPE_LEGACY_PCIE_ENDPOINT)) &&
      (Type0_Cfg.Hdr.ClassCode[2] == PCI_CLASS_DISPLAY) &&
      (Type0_Cfg.Device.SubsystemVendorID == PCIE_SUBSYSTEM_VEN_ID_NVIDIA))
  {
    if (Bus != 1) {
      DEBUG ((
        DEBUG_WARN,
        "GPU [%04x:%02x:%02x.%x] isn't connected directly to the root port.\
         Hence skipping 256B MPS and 10-bit tags configuration\n",
        Segment,
        Bus,
        Device,
        Function
        ));
      return EFI_UNSUPPORTED;
    }

    Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability);
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint32,
                          Offset,
                          1,
                          &DeviceCapability.Uint32
                          );
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    /* Configure 256B MPS */
    if (DeviceCapability.Bits.MaxPayloadSize >= PCIE_MAX_PAYLOAD_SIZE_256B) {
      EFI_PCI_IO_PROTOCOL  *ParentPciIo;

      ParentPciIo = GetParent (PciIo);
      Status      = ParentPciIo->GetLocation (ParentPciIo, &PSegment, &PBus, &PDevice, &PFunction);
      ASSERT_EFI_ERROR (Status);
      DEBUG ((
        DEBUG_INFO,
        "Device [%04x:%02x:%02x.%x] is the parent of the Device [%04x:%02x:%02x.%x]\n",
        PSegment,
        PBus,
        PDevice,
        PFunction,
        Segment,
        Bus,
        Device,
        Function
        ));

      /* Update MPS in the child device */
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

      DeviceControl.Bits.MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_256B;

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
        "Device [%04x:%02x:%02x.%x] : Enabled MPS=256B in child device\n",
        Segment,
        Bus,
        Device,
        Function
        ));

      /* Update MPS in the parent device */
      ParentPciExpCapOffset = PcieFindCap (ParentPciIo, EFI_PCI_CAPABILITY_ID_PCIEXP);
      Offset                = ParentPciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl);
      Status                = ParentPciIo->Pci.Read (
                                                 ParentPciIo,
                                                 EfiPciIoWidthUint16,
                                                 Offset,
                                                 1,
                                                 &DeviceControl.Uint16
                                                 );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DeviceControl.Bits.MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_256B;

      Status = ParentPciIo->Pci.Write (
                                  ParentPciIo,
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
        "Device [%04x:%02x:%02x.%x] : Enabled MPS=256B in parent device\n",
        PSegment,
        PBus,
        PDevice,
        PFunction
        ));
    }

    /* Enable Extended Tag */
    if (DeviceCapability.Bits.ExtendedTagField) {
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

      DeviceControl.Bits.ExtendedTagField = 1;

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
        "Device [%04x:%02x:%02x.%x] : Enabled ExtendedTagField\n",
        Segment,
        Bus,
        Device,
        Function
        ));
    }

    /* Enable 10bit Tag */
    Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceCapability2);
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint32,
                          Offset,
                          1,
                          &DeviceCapability2.Uint32
                          );
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    if (DeviceCapability2.Bits.TenBitTagRequesterSupported) {
      Offset = PciExpCapOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl2);
      Status = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &DeviceControl2.Uint16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DeviceControl2.Bits.TenBitTagRequesterEnable = 1;

      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint16,
                            Offset,
                            1,
                            &DeviceControl2.Uint16
                            );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((
        DEBUG_INFO,
        "Device [%04x:%02x:%02x.%x] : Enabled 10Bit Tag Requester\n",
        Segment,
        Bus,
        Device,
        Function
        ));
    }
  }

  return EFI_SUCCESS;
}
