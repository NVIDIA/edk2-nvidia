/** @file

  DW EQoS device tree binding driver

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
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DmaLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NetLib.h>
#include <Protocol/CvmEeprom.h>
#include <libfdt.h>

#include "DwEqosSnpDxe.h"
#include "EqosDeviceDxePrivate.h"
#include "DtAcpiMacUpdate.h"

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,eqos", &gDwEqosNetNonDiscoverableDeviceGuid },
    { "nvidia,nveqos", &gDwEqosNetNonDiscoverableDeviceGuid },
    { "nvidia,tegra186-eqos", &gDwEqosNetNonDiscoverableDeviceGuid },
    { "nvidia,tegra194-eqos", &gDwEqosNetT194NonDiscoverableDeviceGuid },
    { "snps,dwc-qos-ethernet-4.10", &gDwEqosNetNonDiscoverableDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA EQoS ethernet controller driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoResetModule = TRUE,
    .SkipEdkiiNondiscoverableInstall = TRUE
};

STATIC
SIMPLE_NETWORK_DEVICE_PATH PathTemplate = {
  {
    {
      MESSAGING_DEVICE_PATH, MSG_MAC_ADDR_DP,
      {(UINT8)(sizeof (MAC_ADDR_DEVICE_PATH)), (UINT8)((sizeof (MAC_ADDR_DEVICE_PATH)) >> 8)}
    },
    {{ 0 }},
    0
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {sizeof (EFI_DEVICE_PATH_PROTOCOL), 0}
  }
};


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
  EFI_STATUS                       Status;
  UINTN                            RegionSize;
  SIMPLE_NETWORK_DRIVER            *Snp;
  EFI_SIMPLE_NETWORK_PROTOCOL      *SnpProtocol;
  EFI_SIMPLE_NETWORK_MODE          *SnpMode;
  SIMPLE_NETWORK_DEVICE_PATH       *DevicePath;
  UINTN                            DescriptorSize;
  UINTN                            BufferSize;
  CONST UINT32                     *ResetGpioProp;
  NON_DISCOVERABLE_DEVICE          *Device;
  CONST CHAR8                      *NodeName;
  CHAR16                           VariableName[MAX_ETH_NAME];
  UINTN                            VariableSize;
  UINT32                           VariableAttributes;
  TEGRA_CVM_EEPROM_PROTOCOL        *CvmEeprom;


  switch (Phase) {

  case DeviceDiscoveryDriverBindingStart:
    Status = gBS->HandleProtocol (
                    ControllerHandle,
                    &gNVIDIANonDiscoverableDeviceProtocolGuid,
                    (VOID **)&Device
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get non discoverable protocol\r\n"));
      return Status;
    }

    // Allocate Resources
    Snp = AllocatePages (EFI_SIZE_TO_PAGES (sizeof (SIMPLE_NETWORK_DRIVER)));
    if (Snp == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    if (CompareGuid (Device->Type, &gDwEqosNetT194NonDiscoverableDeviceGuid)) {
      //Bit 39 is in use set to max of 38-bit
      Snp->MaxAddress = BIT39 - 1;
    } else {
      //40-bit address
      Snp->MaxAddress = BIT41 - 1;
    }

    // Size for transmit and receive buffer
    BufferSize = ETH_BUFSIZE;

    //DMA TxdescRing allocate buffer and map
    DescriptorSize = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (sizeof (DESIGNWARE_HW_DESCRIPTOR)*CONFIG_TX_DESCR_NUM));
    Status = DmaAllocateBuffer (EfiBootServicesData,
                                DescriptorSize, (VOID *)&Snp->MacDriver.TxdescRing);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a () for TxdescRing: %r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = DmaMap (MapOperationBusMasterCommonBuffer, Snp->MacDriver.TxdescRing,
               &DescriptorSize, &Snp->MacDriver.TxdescRingMap.AddrMap, &Snp->MacDriver.TxdescRingMap.Mapping);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a () for TxdescRing: %r\n", __FUNCTION__, Status));
      return Status;
    }

    // DMA RxdescRing allocate buffer and map
    DescriptorSize = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (sizeof (DESIGNWARE_HW_DESCRIPTOR)*CONFIG_RX_DESCR_NUM));
    Status = DmaAllocateBuffer (EfiBootServicesData,
                                DescriptorSize, (VOID *)&Snp->MacDriver.RxdescRing);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a () for RxdescRing: %r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = DmaMap (MapOperationBusMasterCommonBuffer, Snp->MacDriver.RxdescRing,
               &DescriptorSize, &Snp->MacDriver.RxdescRingMap.AddrMap, &Snp->MacDriver.RxdescRingMap.Mapping);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a () for RxdescRingMap: %r\n", __FUNCTION__, Status));
      return Status;
    }

    //DMA mapping for receive buffer
    DescriptorSize = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (RX_TOTAL_BUFSIZE));
    Status = DmaAllocateBuffer (EfiBootServicesData,
                                DescriptorSize, (VOID *)&Snp->MacDriver.RxBuffer);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a () for RxBuffer: %r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = DmaMap (MapOperationBusMasterCommonBuffer, Snp->MacDriver.RxBuffer,
               &DescriptorSize, &Snp->MacDriver.RxBufferRingMap.AddrMap, &Snp->MacDriver.RxBufferRingMap.Mapping);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a () for RxBufferRingMap: %r\n", __FUNCTION__, Status));
      return Status;
    }

    //DMA mapping for receive buffer
    DescriptorSize = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (TX_TOTAL_BUFSIZE));
    Status = DmaAllocateBuffer (EfiBootServicesData,
                                DescriptorSize, (VOID *)&Snp->MacDriver.TxCopyBuffer);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a () for TxCopyBuffer: %r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = DmaMap (MapOperationBusMasterCommonBuffer, Snp->MacDriver.TxCopyBuffer,
               &DescriptorSize, &Snp->MacDriver.TxCopyBufferRingMap.AddrMap, &Snp->MacDriver.TxCopyBufferRingMap.Mapping);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a () for TxCopyBufferRingMap: %r\n", __FUNCTION__, Status));
      return Status;
    }

    DevicePath = (SIMPLE_NETWORK_DEVICE_PATH*)AllocateCopyPool (sizeof (SIMPLE_NETWORK_DEVICE_PATH), &PathTemplate);
    if (DevicePath == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    // Initialized signature (used by INSTANCE_FROM_SNP_THIS macro)
    Snp->Signature = SNP_DRIVER_SIGNATURE;

    EfiInitializeLock (&Snp->Lock, TPL_CALLBACK);

    // Initialize pointers
    SnpMode = &Snp->SnpMode;
    Snp->Snp.Mode = SnpMode;

    // Get MAC controller base address
    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &Snp->MacBase, &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }

    // Assign fields and func pointers
    Snp->Snp.Revision = EFI_SIMPLE_NETWORK_PROTOCOL_REVISION;
    Snp->Snp.WaitForPacket = NULL;
    Snp->Snp.Initialize = SnpInitialize;
    Snp->Snp.Start = SnpStart;
    Snp->Snp.Stop = SnpStop;
    Snp->Snp.Reset = SnpReset;
    Snp->Snp.Shutdown = SnpShutdown;
    Snp->Snp.ReceiveFilters = SnpReceiveFilters;
    Snp->Snp.StationAddress = SnpStationAddress;
    Snp->Snp.Statistics = SnpStatistics;
    Snp->Snp.MCastIpToMac = SnpMcastIptoMac;
    Snp->Snp.NvData = SnpNvData;
    Snp->Snp.GetStatus = SnpGetStatus;
    Snp->Snp.Transmit = SnpTransmit;
    Snp->Snp.Receive = SnpReceive;

    // Start completing simple network mode structure
    SnpMode->State = EfiSimpleNetworkStopped;
    SnpMode->HwAddressSize = NET_ETHER_ADDR_LEN;    // HW address is 6 bytes
    SnpMode->MediaHeaderSize = sizeof (ETHER_HEAD);
    SnpMode->MaxPacketSize = EFI_PAGE_SIZE;         // Preamble + SOF + Ether Frame (with VLAN tag +4bytes)
    SnpMode->NvRamSize = 0;                         // No NVRAM with this device
    SnpMode->NvRamAccessSize = 0;                   // No NVRAM with this device

    // Update network mode information
    SnpMode->ReceiveFilterMask = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST     |
                                 EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST   |
                                 EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST   |
                                 EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS |
                                 EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST;

    // Initially-enabled receive filters
    SnpMode->ReceiveFilterSetting = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST     |
                                    EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST;

    // EMAC has 64bit hash table, can filter 64 MCast MAC Addresses
    SnpMode->MaxMCastFilterCount = MAX_MCAST_FILTER_CNT;
    SnpMode->MCastFilterCount = 0;
    ZeroMem (&SnpMode->MCastFilter, MAX_MCAST_FILTER_CNT * sizeof (EFI_MAC_ADDRESS));

    // Set the interface type (1: Ethernet or 6: IEEE 802 Networks)
    SnpMode->IfType = NET_IFTYPE_ETHERNET;

    // Mac address is changeable as it is loaded from erasable memory
    SnpMode->MacAddressChangeable = TRUE;

    // Can transmit more than one packet at a time
    SnpMode->MultipleTxSupported = TRUE;

    // MediaPresent checks for cable connection and partner link
    SnpMode->MediaPresentSupported = TRUE;
    SnpMode->MediaPresent = FALSE;

    // Set broadcast address
    SetMem (&SnpMode->BroadcastAddress, sizeof (EFI_MAC_ADDRESS), 0xFF);

    //Set current address
    ///TODO: Read from EEPROM
    NodeName = fdt_get_name (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, NULL);
    if (NodeName == NULL) {
      DEBUG ((DEBUG_ERROR, "Failed to get node name\r\n"));
      return EFI_DEVICE_ERROR;
    }
    Status = AsciiStrToUnicodeStrS (NodeName, VariableName, MAX_ETH_NAME);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to convert name to unicode %a: %r\r\n", NodeName, Status));
      return EFI_DEVICE_ERROR;
    }

    Status = gBS->LocateProtocol (&gNVIDIACvmEepromProtocolGuid, NULL, (VOID **)&CvmEeprom);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get eeprom protocol\r\n"));
      return EFI_DEVICE_ERROR;
    }

    ZeroMem (&SnpMode->PermanentAddress, sizeof(SnpMode->PermanentAddress));
    ZeroMem (&SnpMode->CurrentAddress, sizeof(SnpMode->CurrentAddress));

    if ((CompareMem (CvmEeprom->CustomerBlockSignature, CVM_EEPROM_CUSTOMER_BLOCK_SIGNATURE, sizeof (CvmEeprom->CustomerBlockSignature)) == 0) &&
        (CompareMem (CvmEeprom->CustomerTypeSignature, CVM_EEPROM_CUSTOMER_TYPE_SIGNATURE, sizeof (CvmEeprom->CustomerTypeSignature)) == 0)) {
      SnpMode->PermanentAddress.Addr[0] = CvmEeprom->CustomerEthernetMacAddress[5];
      SnpMode->PermanentAddress.Addr[1] = CvmEeprom->CustomerEthernetMacAddress[4];
      SnpMode->PermanentAddress.Addr[2] = CvmEeprom->CustomerEthernetMacAddress[3];
      SnpMode->PermanentAddress.Addr[3] = CvmEeprom->CustomerEthernetMacAddress[2];
      SnpMode->PermanentAddress.Addr[4] = CvmEeprom->CustomerEthernetMacAddress[1];
      SnpMode->PermanentAddress.Addr[5] = CvmEeprom->CustomerEthernetMacAddress[0];
    } else {
      SnpMode->PermanentAddress.Addr[0] = CvmEeprom->EthernetMacAddress[5];
      SnpMode->PermanentAddress.Addr[1] = CvmEeprom->EthernetMacAddress[4];
      SnpMode->PermanentAddress.Addr[2] = CvmEeprom->EthernetMacAddress[3];
      SnpMode->PermanentAddress.Addr[3] = CvmEeprom->EthernetMacAddress[2];
      SnpMode->PermanentAddress.Addr[4] = CvmEeprom->EthernetMacAddress[1];
      SnpMode->PermanentAddress.Addr[5] = CvmEeprom->EthernetMacAddress[0];
    }
    VariableSize = sizeof (EFI_MAC_ADDRESS);
    Status = gRT->GetVariable (VariableName, &gNVIDIATokenSpaceGuid, &VariableAttributes, &VariableSize, (VOID *)SnpMode->CurrentAddress.Addr);
    if (EFI_ERROR (Status) || (VariableSize < NET_ETHER_ADDR_LEN)) {
      CopyMem (&SnpMode->CurrentAddress, &SnpMode->PermanentAddress, sizeof (EFI_MAC_ADDRESS));
    }

    // Assign fields for device path
    CopyMem (&DevicePath->MacAddrDP.MacAddress, &Snp->Snp.Mode->CurrentAddress, NET_ETHER_ADDR_LEN);
    DevicePath->MacAddrDP.IfType = Snp->Snp.Mode->IfType;

    Snp->PhyDriver.ControllerHandle = ControllerHandle;
    ResetGpioProp = (CONST UINT32 *)fdt_getprop (
                                         DeviceTreeNode->DeviceTreeBase,
                                         DeviceTreeNode->NodeOffset,
                                         "nvidia,phy-reset-gpio",
                                         NULL);
    if (ResetGpioProp == NULL) {
      ResetGpioProp = (CONST UINT32 *)fdt_getprop (
                                           DeviceTreeNode->DeviceTreeBase,
                                           DeviceTreeNode->NodeOffset,
                                           "phy-reset-gpio",
                                           NULL);
      if (ResetGpioProp == NULL) {
        return EFI_DEVICE_ERROR;
      }
    }
    Snp->PhyDriver.ResetPin = GPIO (SwapBytes32 (ResetGpioProp[0]), SwapBytes32 (ResetGpioProp[1]));
    if (SwapBytes32 (ResetGpioProp[2]) == 0) {
      Snp->PhyDriver.ResetMode0 = GPIO_MODE_OUTPUT_0;
      Snp->PhyDriver.ResetMode1 = GPIO_MODE_OUTPUT_1;
    } else {
      Snp->PhyDriver.ResetMode0 = GPIO_MODE_OUTPUT_1;
      Snp->PhyDriver.ResetMode1 = GPIO_MODE_OUTPUT_0;
    }

    if (fdt_get_path (DeviceTreeNode->DeviceTreeBase,
                      DeviceTreeNode->NodeOffset,
                      Snp->DeviceTreePath,
                      sizeof (Snp->DeviceTreePath)) != 0) {
      DEBUG ((DEBUG_ERROR, "Failed to get device tree path\r\n"));
      return EFI_DEVICE_ERROR;
    }

    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    UpdateDTACPIMacAddress,
                    Snp,
                    &gFdtTableGuid,
                    &Snp->DeviceTreeNotifyEvent
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to register for DT installation\r\n"));
      return Status;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ControllerHandle,
                    &gEfiSimpleNetworkProtocolGuid, &(Snp->Snp),
                    NULL
                    );

    if (EFI_ERROR(Status)) {
      gBS->CloseEvent (Snp->DeviceTreeNotifyEvent);
      FreePages (Snp, EFI_SIZE_TO_PAGES (sizeof (SIMPLE_NETWORK_DRIVER)));
    } else {
      Snp->ControllerHandle = ControllerHandle;
    }

    return Status;

  case DeviceDiscoveryDriverBindingStop:
    Status = gBS->HandleProtocol (
                    ControllerHandle,
                    &gEfiSimpleNetworkProtocolGuid,
                    (VOID **)&SnpProtocol
                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a (): HandleProtocol: %r\n", __FUNCTION__, Status));
      return Status;
    }

    Snp = INSTANCE_FROM_SNP_THIS(SnpProtocol);
    gBS->CloseEvent (Snp->DeviceTreeNotifyEvent);

    Status = gBS->UninstallMultipleProtocolInterfaces (
                    ControllerHandle,
                    &gEfiSimpleNetworkProtocolGuid,
                    &Snp->Snp,
                    NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a (): UninstallMultipleProtocolInterfaces: %r\n", __FUNCTION__, Status));
      return Status;
    }

    FreePages (Snp, EFI_SIZE_TO_PAGES (sizeof (SIMPLE_NETWORK_DRIVER)));

    return Status;

  default:
    return EFI_SUCCESS;
  }
}
