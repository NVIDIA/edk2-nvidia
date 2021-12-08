/** @file

  DW EQoS device tree binding driver

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

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
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/Eeprom.h>
#include <libfdt.h>

#include "DwEqosSnpDxe.h"
#include "EqosDeviceDxePrivate.h"
#include "DtAcpiMacUpdate.h"

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,eqos", &gDwEqosNetNonDiscoverableDeviceGuid },
    { "nvidia,nveqos", &gDwEqosNetNonDiscoverableDeviceGuid },
    { "nvidia,nvmgbe", &gDwMgbeNetNonDiscoverableDeviceGuid },
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
    .SkipEdkiiNondiscoverableInstall = TRUE,
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
  On Exit Boot Services Event notification handler.

  Invoke Check Auto Negotiation for Eqos Driver.
  Perform Link Initialization

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
  SIMPLE_NETWORK_DRIVER   *Snp;
  EFI_STATUS              Status;
  VOID                    *AcpiBase;

  Snp = (SIMPLE_NETWORK_DRIVER *) Context;

  // Check Instance
  if (Snp == NULL) {
    DEBUG ((DEBUG_INFO, "SNP:DXE: Received NULL context\r\n"));
    return;
  }

  // closing event
  gBS->CloseEvent (Snp->ExitBootServiceEvent);

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    // Check for Auto Neg completion
    Snp->PhyDriver.CheckAutoNeg( &Snp->PhyDriver);

    // Init Link
    DEBUG ((DEBUG_INFO, "SNP:DXE: Auto-Negotiating Ethernet PHY Link\r\n"));
    Status = PhyLinkAdjustEmacConfig (&Snp->PhyDriver);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_INFO, "SNP:DXE: Link is Down - Network Cable is not plugged in?\r\n"));
    }
  }

  return;
}

//Helper function for getting fdt values as UINT32
//As swapping is needed based on size
STATIC
EFI_STATUS
GetFdtProperty (
  VOID        *DeviceTreeBase,
  INT32        NodeOffset,
  CONST CHAR8 *PropertyName,
  UINT32      *PropertyValue
  )
{
  CONST VOID *Property;
  INT32       PropertySize;

  Property = fdt_getprop (DeviceTreeBase, NodeOffset, PropertyName, &PropertySize);

  if (Property == NULL) {
    return EFI_NOT_FOUND;
  }

  if (PropertySize == 1) {
    *PropertyValue = *(UINT8 *)Property;
  } else if (PropertySize == 2) {
    *PropertyValue = SwapBytes16 (*(UINT16 *)Property);
  } else if (PropertySize == 4) {
    *PropertyValue = SwapBytes32 (*(UINT32 *)Property);
  } else {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
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
  EFI_STATUS                       Status;
  UINTN                            RegionSize;
  SIMPLE_NETWORK_DRIVER            *Snp;
  EFI_SIMPLE_NETWORK_PROTOCOL      *SnpProtocol;
  EFI_SIMPLE_NETWORK_MODE          *SnpMode;
  SIMPLE_NETWORK_DEVICE_PATH       *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL         *DevicePathOrig;
  CONST UINT32                     *ResetGpioProp;
  NON_DISCOVERABLE_DEVICE          *Device;
  CONST CHAR8                      *NodeName;
  CHAR16                           VariableName[MAX_ETH_NAME];
  UINTN                            VariableSize;
  UINT32                           VariableAttributes;
  TEGRA_CVMEEPROM_BOARD_INFO       *CvmEeprom;
  TEGRA_PLATFORM_TYPE              PlatformType;
  BOOLEAN                          FlipResetMode;
  UINT32                           PhyNodeHandle;
  INT32                            PhyNodeOffset;

  PlatformType = TegraGetPlatform();
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

    ZeroMem (Snp, sizeof (SIMPLE_NETWORK_DRIVER));

    if (CompareGuid (Device->Type, &gDwEqosNetT194NonDiscoverableDeviceGuid)) {
      //Bit 39 is in use set to max of 38-bit
      Snp->MaxAddress = BIT39 - 1;
    } else {
      //40-bit address
      Snp->MaxAddress = BIT41 - 1;
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

    Snp->BroadcastEnabled = FALSE;
    Snp->MulticastFiltersEnabled = 0;

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

    SnpMode->PermanentAddress.Addr[0] = CvmEeprom->MacAddr[5];
    SnpMode->PermanentAddress.Addr[1] = CvmEeprom->MacAddr[4];
    SnpMode->PermanentAddress.Addr[2] = CvmEeprom->MacAddr[3];
    SnpMode->PermanentAddress.Addr[3] = CvmEeprom->MacAddr[2];
    SnpMode->PermanentAddress.Addr[4] = CvmEeprom->MacAddr[1];
    SnpMode->PermanentAddress.Addr[5] = CvmEeprom->MacAddr[0];
    Snp->NumMacs = CvmEeprom->NumMacs;

    VariableSize = sizeof (EFI_MAC_ADDRESS);
    Status = gRT->GetVariable (VariableName, &gNVIDIATokenSpaceGuid, &VariableAttributes, &VariableSize, (VOID *)SnpMode->CurrentAddress.Addr);
    if (EFI_ERROR (Status) || (VariableSize < NET_ETHER_ADDR_LEN)) {
      CopyMem (&SnpMode->CurrentAddress, &SnpMode->PermanentAddress, sizeof (EFI_MAC_ADDRESS));
    }

    UpdateDTACPIMacAddress (NULL, (VOID *)Snp);

    // Assign fields for device path
    CopyMem (&DevicePath->MacAddrDP.MacAddress, &Snp->Snp.Mode->CurrentAddress, NET_ETHER_ADDR_LEN);
    DevicePath->MacAddrDP.IfType = Snp->Snp.Mode->IfType;
    //Update the device path to add MAC node
    Status = gBS->HandleProtocol (
                    ControllerHandle,
                    &gEfiDevicePathProtocolGuid,
                    (VOID **)&DevicePathOrig
                  );
    if (!EFI_ERROR (Status)) {
      BOOLEAN MacPresent = FALSE;
      EFI_DEVICE_PATH_PROTOCOL *Node = DevicePathOrig;

      //Check to make sure we haven't already added mac address
      while (!IsDevicePathEnd (Node)) {
        if ((DevicePathType (Node) == MESSAGING_DEVICE_PATH) &&
            (DevicePathSubType (Node) == MSG_MAC_ADDR_DP)) {
          MacPresent = TRUE;
          break;
        }
        Node = NextDevicePathNode (Node);
      }

      if (!MacPresent) {
        EFI_DEVICE_PATH_PROTOCOL *NewPath;
        NewPath = AppendDevicePath (DevicePathOrig, (EFI_DEVICE_PATH_PROTOCOL *)DevicePath);
        if (NewPath != NULL) {
          Status = gBS->UninstallMultipleProtocolInterfaces (
                          ControllerHandle,
                          &gEfiDevicePathProtocolGuid,
                          DevicePathOrig,
                          NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall device path (%r)\r\n", __FUNCTION__, Status));
          } else {
            Status = gBS->InstallMultipleProtocolInterfaces (
                            &ControllerHandle,
                            &gEfiDevicePathProtocolGuid, NewPath,
                            NULL
                            );
            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "%a: Failed to install device path (%r)\r\n", __FUNCTION__, Status));
            }
          }
        } else {
          DEBUG ((DEBUG_ERROR, "%a: Failed to append device path\r\n", __FUNCTION__));
        }
      }
    }

    Snp->PhyDriver.ControllerHandle = ControllerHandle;
    FlipResetMode = FALSE;
    ResetGpioProp = (CONST UINT32 *)fdt_getprop (
                                         DeviceTreeNode->DeviceTreeBase,
                                         DeviceTreeNode->NodeOffset,
                                         "nvidia,phy-reset-gpio",
                                         NULL);
    if (ResetGpioProp == NULL) {
      // TODO: Revert FlipResetMode based changes once upstream DTB has been
      // updated.
      FlipResetMode = TRUE;
      ResetGpioProp = (CONST UINT32 *)fdt_getprop (
                                           DeviceTreeNode->DeviceTreeBase,
                                           DeviceTreeNode->NodeOffset,
                                           "phy-reset-gpios",
                                           NULL);
    }

    if (ResetGpioProp != NULL) {
      // Populate ResetPin from the device tree
      Snp->PhyDriver.ResetPin = GPIO (SwapBytes32 (ResetGpioProp[0]), SwapBytes32 (ResetGpioProp[1]));
      if (SwapBytes32 (ResetGpioProp[2]) == FlipResetMode) {
        Snp->PhyDriver.ResetMode0 = GPIO_MODE_OUTPUT_0;
        Snp->PhyDriver.ResetMode1 = GPIO_MODE_OUTPUT_1;
      } else {
        Snp->PhyDriver.ResetMode0 = GPIO_MODE_OUTPUT_1;
        Snp->PhyDriver.ResetMode1 = GPIO_MODE_OUTPUT_0;
      }
    } else {
      // Give a fake setting to ResetPin
      Snp->PhyDriver.ResetPin = NON_EXISTENT_ON_PLATFORM;
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

    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    UpdateDTACPIMacAddress,
                    Snp,
                    &gEfiAcpiTableGuid,
                    &Snp->AcpiNotifyEvent
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to register for ACPI installation\r\n"));
      return Status;
    }

    // Init EMAC
    if (CompareGuid (Device->Type, &gDwMgbeNetNonDiscoverableDeviceGuid)) {
      Status = EmacDxeInitialization (&Snp->MacDriver, Snp->MacBase, OSI_MAC_HW_MGBE);
    } else {
      Status = EmacDxeInitialization (&Snp->MacDriver, Snp->MacBase, OSI_MAC_HW_EQOS);
    }
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "SNP:DXE: Failed to initialize EMAC\n"));
      return EFI_DEVICE_ERROR;
    }

    SnpMode->MaxPacketSize = Snp->MacDriver.osi_dma->rx_buf_len;         // Preamble + SOF + Ether Frame (with VLAN tag +4bytes)

    //Set PHY driver defaults will override as needed
    Snp->PhyDriver.PhyAddress = PHY_DEFAULT_ADDRESS;
    Snp->PhyDriver.ResetDelay = PHY_DEFAULT_RESET_DELAY_USEC;
    Snp->PhyDriver.PostResetDelay = PHY_DEFAULT_POST_RESET_DELAY_USEC;

    Status = GetFdtProperty (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "phy-handle", &PhyNodeHandle);
    if (!EFI_ERROR (Status)) {
      PhyNodeOffset = fdt_node_offset_by_phandle (DeviceTreeNode->DeviceTreeBase, PhyNodeHandle);
      if (PhyNodeOffset > 0) {
        //Get values can ignore status as we already setup defaults and these nodes are optional.
        GetFdtProperty (DeviceTreeNode->DeviceTreeBase, PhyNodeOffset, "reg", &Snp->PhyDriver.PhyAddress);
        GetFdtProperty (DeviceTreeNode->DeviceTreeBase, PhyNodeOffset, "nvidia,phy-rst-duration-usec", &Snp->PhyDriver.ResetDelay);
        Status = GetFdtProperty (DeviceTreeNode->DeviceTreeBase, PhyNodeOffset, "nvidia,phy-rst-pdelay-msec", &Snp->PhyDriver.PostResetDelay);
        if (!EFI_ERROR (Status)) {
          //Convert to usec from msec
          Snp->PhyDriver.PostResetDelay *= 1000;
        }
      }
    }

    // Init PHY
    Status = PhyDxeInitialization (&Snp->PhyDriver, &Snp->MacDriver);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    // Set MAC Address
    CopyMem (&Snp->SnpMode.CurrentAddress, &Snp->SnpMode.PermanentAddress, sizeof (Snp->SnpMode.CurrentAddress));
    SnpCommitFilters (Snp, TRUE, FALSE);
    UpdateDTACPIMacAddress (NULL, (VOID *)Snp);

    // Check for Auto Negotiation completion and rest of Phy setup
    // happens at the exit boot services stage. Creating event for the same.
    Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                                 TPL_NOTIFY,
                                 OnExitBootServices,
                                 Snp,
                                 &gEfiEventExitBootServicesGuid,
                                 &Snp->ExitBootServiceEvent
                                 );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to create event for auto neg completion upon exiting boot services \r\n"));
      return Status;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ControllerHandle,
                    &gEfiSimpleNetworkProtocolGuid, &(Snp->Snp),
                    NULL
                    );

    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_ERROR, "SNP:DXE: Could not install multiple protocol interfaces\n"));
      gBS->CloseEvent (Snp->DeviceTreeNotifyEvent);
      gBS->CloseEvent (Snp->AcpiNotifyEvent);
      gBS->CloseEvent (Snp->ExitBootServiceEvent);
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
    gBS->CloseEvent (Snp->AcpiNotifyEvent);
    gBS->CloseEvent (Snp->ExitBootServiceEvent);

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
