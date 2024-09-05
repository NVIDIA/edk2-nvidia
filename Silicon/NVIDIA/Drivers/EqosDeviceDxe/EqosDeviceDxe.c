/** @file

  DW EQoS device tree binding driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
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

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,eqos",                &gDwEqosNetNonDiscoverableDeviceGuid     },
  { "nvidia,nveqos",              &gDwEqosNetNonDiscoverableDeviceGuid     },
  { "nvidia,nvmgbe",              &gDwMgbeNetNonDiscoverableDeviceGuid     },
  { "nvidia,tegra186-eqos",       &gDwEqosNetNonDiscoverableDeviceGuid     },
  { "nvidia,tegra194-eqos",       &gDwEqosNetT194NonDiscoverableDeviceGuid },
  { "nvidia,tegra234-mgbe",       &gDwMgbeNetNonDiscoverableDeviceGuid     },
  { "snps,dwc-qos-ethernet-4.10", &gDwEqosNetNonDiscoverableDeviceGuid     },
  { NULL,                         NULL                                     }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA EQoS ethernet controller driver",
  .AutoEnableClocks                = TRUE,
  .AutoResetModule                 = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .ThreadedDeviceStart             = TRUE
};

STATIC
SIMPLE_NETWORK_DEVICE_PATH  PathTemplate = {
  {
    {
      MESSAGING_DEVICE_PATH, MSG_MAC_ADDR_DP,
      { (UINT8)(sizeof (MAC_ADDR_DEVICE_PATH)), (UINT8)((sizeof (MAC_ADDR_DEVICE_PATH)) >> 8) }
    },
    {
      { 0 }
    },
    0
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL),      0                                             }
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
  IN      EFI_EVENT  Event,
  IN      VOID       *Context
  )
{
  SIMPLE_NETWORK_DRIVER  *Snp;
  EFI_STATUS             Status;
  VOID                   *AcpiBase;

  Snp = (SIMPLE_NETWORK_DRIVER *)Context;

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
    Snp->PhyDriver.CheckAutoNeg (&Snp->PhyDriver);

    // Init Link
    DEBUG ((DEBUG_INFO, "SNP:DXE: Auto-Negotiating Ethernet PHY Link\r\n"));
    Status = PhyLinkAdjustEmacConfig (&Snp->PhyDriver);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "SNP:DXE: Link is Down - Network Cable is not plugged in?\r\n"));
    }
  }

  SnpShutdown (&Snp->Snp);

  return;
}

// Helper function for getting fdt values as UINT32
// As swapping is needed based on size
STATIC
EFI_STATUS
GetFdtProperty (
  VOID         *DeviceTreeBase,
  INT32        NodeOffset,
  CONST CHAR8  *PropertyName,
  UINT32       *PropertyValue
  )
{
  CONST VOID  *Property;
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                   Status;
  UINTN                        ChipID;
  UINTN                        RegionSize;
  SIMPLE_NETWORK_DRIVER        *Snp;
  EFI_SIMPLE_NETWORK_PROTOCOL  *SnpProtocol;
  EFI_SIMPLE_NETWORK_MODE      *SnpMode;
  SIMPLE_NETWORK_DEVICE_PATH   *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL     *DevicePathOrig;
  CONST UINT32                 *ResetGpioProp;
  NON_DISCOVERABLE_DEVICE      *Device;
  TEGRA_PLATFORM_TYPE          PlatformType;
  BOOLEAN                      FlipResetMode;
  UINT32                       PhyNodeHandle;
  INT32                        PhyNodeOffset;
  INT32                        OsiReturn;
  struct osi_hw_features       hw_feat;
  INT32                        MacRegionIndex;
  INT32                        XpcsRegionIndex;
  CONST VOID                   *Property;
  INT32                        PropertySize;
  CONST UINT8                  *MacAddress;

  PlatformType = TegraGetPlatform ();
  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      if ((DeviceTreeNode == NULL) ||
          (DeviceTreeNode->NodeOffset < 0))
      {
        return EFI_INVALID_PARAMETER;
      }

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
        // Bit 39 is in use set to max of 38-bit
        Snp->MaxAddress = BIT39 - 1;
      } else {
        // 40-bit address
        Snp->MaxAddress = BIT41 - 1;
      }

      DevicePath = (SIMPLE_NETWORK_DEVICE_PATH *)AllocateCopyPool (sizeof (SIMPLE_NETWORK_DEVICE_PATH), &PathTemplate);
      if (DevicePath == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      // Initialized signature (used by INSTANCE_FROM_SNP_THIS macro)
      Snp->Signature = SNP_DRIVER_SIGNATURE;

      EfiInitializeLock (&Snp->Lock, TPL_CALLBACK);

      // Initialize pointers
      SnpMode       = &Snp->SnpMode;
      Snp->Snp.Mode = SnpMode;

      MacRegionIndex = fdt_stringlist_search (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "reg-names", "mac");
      if (MacRegionIndex < 0) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to retrieve mac region details from node at offset 0x%x: %a assuming 0\r\n",
          __FUNCTION__,
          (UINTN)DeviceTreeNode->NodeOffset,
          fdt_strerror (MacRegionIndex)
          ));
        MacRegionIndex = 0;
      }

      // Get MAC controller base address
      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, (UINTN)MacRegionIndex, &Snp->MacBase, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
        return EFI_UNSUPPORTED;
      }

      // Assign Adapter Information Protocol Pointers
      Snp->Aip.GetInformation    = EqosAipGetInformation;
      Snp->Aip.SetInformation    = EqosAipSetInformation;
      Snp->Aip.GetSupportedTypes = EqosAipGetSupportedTypes;

      // Assign fields and func pointers
      Snp->Snp.Revision       = EFI_SIMPLE_NETWORK_PROTOCOL_REVISION;
      Snp->Snp.WaitForPacket  = NULL;
      Snp->Snp.Initialize     = SnpInitialize;
      Snp->Snp.Start          = SnpStart;
      Snp->Snp.Stop           = SnpStop;
      Snp->Snp.Reset          = SnpReset;
      Snp->Snp.Shutdown       = SnpShutdown;
      Snp->Snp.ReceiveFilters = SnpReceiveFilters;
      Snp->Snp.StationAddress = SnpStationAddress;
      Snp->Snp.Statistics     = SnpStatistics;
      Snp->Snp.MCastIpToMac   = SnpMcastIptoMac;
      Snp->Snp.NvData         = SnpNvData;
      Snp->Snp.GetStatus      = SnpGetStatus;
      Snp->Snp.Transmit       = SnpTransmit;
      Snp->Snp.Receive        = SnpReceive;

      // Start completing simple network mode structure
      SnpMode->State           = EfiSimpleNetworkStopped;
      SnpMode->HwAddressSize   = NET_ETHER_ADDR_LEN; // HW address is 6 bytes
      SnpMode->MediaHeaderSize = sizeof (ETHER_HEAD);
      SnpMode->NvRamSize       = 0;                 // No NVRAM with this device
      SnpMode->NvRamAccessSize = 0;                 // No NVRAM with this device

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
      SnpMode->MCastFilterCount    = 0;
      ZeroMem (&SnpMode->MCastFilter, MAX_MCAST_FILTER_CNT * sizeof (EFI_MAC_ADDRESS));

      // Set the interface type (1: Ethernet or 6: IEEE 802 Networks)
      SnpMode->IfType = NET_IFTYPE_ETHERNET;

      // Mac address is changeable as it is loaded from erasable memory
      SnpMode->MacAddressChangeable = TRUE;

      // Can transmit more than one packet at a time
      SnpMode->MultipleTxSupported = TRUE;

      // MediaPresent checks for cable connection and partner link
      SnpMode->MediaPresentSupported = TRUE;
      SnpMode->MediaPresent          = FALSE;

      // Set broadcast address
      SetMem (&SnpMode->BroadcastAddress, sizeof (EFI_MAC_ADDRESS), 0xFF);

      Snp->BroadcastEnabled        = FALSE;
      Snp->MulticastFiltersEnabled = 0;

      // Set MAC addresses
      ZeroMem (&SnpMode->PermanentAddress, sizeof (SnpMode->PermanentAddress));
      ZeroMem (&SnpMode->CurrentAddress, sizeof (SnpMode->CurrentAddress));

      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "mac-address", &PropertySize);
      if (Property == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: no mac-address for %a\n", __FUNCTION__, fdt_get_name (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, NULL)));
        return EFI_DEVICE_ERROR;
      }

      MacAddress = (UINT8 *)Property;
      DEBUG ((DEBUG_INFO, "%a: mac=%02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__, MacAddress[0], MacAddress[1], MacAddress[2], MacAddress[3], MacAddress[4], MacAddress[5]));

      CopyMem (&SnpMode->PermanentAddress, Property, PropertySize);
      CopyMem (&SnpMode->CurrentAddress, &SnpMode->PermanentAddress, sizeof (EFI_MAC_ADDRESS));

      // Assign fields for device path
      CopyMem (&DevicePath->MacAddrDP.MacAddress, &Snp->Snp.Mode->CurrentAddress, NET_ETHER_ADDR_LEN);
      DevicePath->MacAddrDP.IfType = Snp->Snp.Mode->IfType;
      // Update the device path to add MAC node
      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&DevicePathOrig
                      );
      if (!EFI_ERROR (Status)) {
        BOOLEAN                   MacPresent = FALSE;
        EFI_DEVICE_PATH_PROTOCOL  *Node      = DevicePathOrig;

        // Check to make sure we haven't already added mac address
        while (!IsDevicePathEnd (Node)) {
          if ((DevicePathType (Node) == MESSAGING_DEVICE_PATH) &&
              (DevicePathSubType (Node) == MSG_MAC_ADDR_DP))
          {
            MacPresent = TRUE;
            break;
          }

          Node = NextDevicePathNode (Node);
        }

        if (!MacPresent) {
          EFI_DEVICE_PATH_PROTOCOL  *NewPath;
          NewPath = AppendDevicePath (DevicePathOrig, (EFI_DEVICE_PATH_PROTOCOL *)DevicePath);
          if (NewPath != NULL) {
            Status = gBS->UninstallMultipleProtocolInterfaces (
                            ControllerHandle,
                            &gEfiDevicePathProtocolGuid,
                            DevicePathOrig,
                            NULL
                            );
            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall device path (%r)\r\n", __FUNCTION__, Status));
            } else {
              Status = gBS->InstallMultipleProtocolInterfaces (
                              &ControllerHandle,
                              &gEfiDevicePathProtocolGuid,
                              NewPath,
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
      FlipResetMode                   = FALSE;
      ResetGpioProp                   = (CONST UINT32 *)fdt_getprop (
                                                          DeviceTreeNode->DeviceTreeBase,
                                                          DeviceTreeNode->NodeOffset,
                                                          "nvidia,phy-reset-gpio",
                                                          NULL
                                                          );
      if (ResetGpioProp == NULL) {
        // TODO: Revert FlipResetMode based changes once upstream DTB has been
        // updated.
        FlipResetMode = TRUE;
        ResetGpioProp = (CONST UINT32 *)fdt_getprop (
                                          DeviceTreeNode->DeviceTreeBase,
                                          DeviceTreeNode->NodeOffset,
                                          "phy-reset-gpios",
                                          NULL
                                          );
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

      if (fdt_get_path (
            DeviceTreeNode->DeviceTreeBase,
            DeviceTreeNode->NodeOffset,
            Snp->DeviceTreePath,
            sizeof (Snp->DeviceTreePath)
            ) != 0)
      {
        DEBUG ((DEBUG_ERROR, "Failed to get device tree path\r\n"));
        return EFI_DEVICE_ERROR;
      }

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      UpdateACPIMacAddress,
                      Snp,
                      &gEfiAcpiTableGuid,
                      &Snp->AcpiNotifyEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to register for ACPI installation\r\n"));
        return Status;
      }

      // If booting Android on T234, skip ethernet initialization in UEFI
      ChipID = TegraGetChipID ();
      if ((ChipID == T234_CHIP_ID) &&
          (PcdGetBool (PcdBootAndroidImage)))
      {
        return EFI_UNSUPPORTED;
      }

      Snp->PhyDriver.MgbeDevice = CompareGuid (Device->Type, &gDwMgbeNetNonDiscoverableDeviceGuid);

      // Init EMAC
      if (Snp->PhyDriver.MgbeDevice) {
        // Get XPCS base address
        XpcsRegionIndex = fdt_stringlist_search (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "reg-names", "xpcs");
        if (XpcsRegionIndex < 0) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: failed to retrieve xpcs region details from node at offset 0x%x: %a\r\n",
            __FUNCTION__,
            (UINTN)DeviceTreeNode->NodeOffset,
            fdt_strerror (XpcsRegionIndex)
            ));
          return EFI_UNSUPPORTED;
        }

        Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, (UINTN)XpcsRegionIndex, &Snp->XpcsBase, &RegionSize);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
          return EFI_UNSUPPORTED;
        }

        Status = EmacDxeInitialization (&Snp->MacDriver, Snp->MacBase, Snp->XpcsBase, OSI_MAC_HW_MGBE);
      } else {
        Status = EmacDxeInitialization (&Snp->MacDriver, Snp->MacBase, 0, OSI_MAC_HW_EQOS);
      }

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "SNP:DXE: Failed to initialize EMAC\n"));
        return EFI_DEVICE_ERROR;
      }

      SnpMode->MaxPacketSize = Snp->MacDriver.osi_dma->mtu;

      // Set PHY driver defaults will override as needed
      Snp->PhyDriver.PhyAddress     = PHY_DEFAULT_ADDRESS;
      Snp->PhyDriver.ResetDelay     = PHY_DEFAULT_RESET_DELAY_USEC;
      Snp->PhyDriver.PostResetDelay = PHY_DEFAULT_POST_RESET_DELAY_USEC;

      Status = GetFdtProperty (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "phy-handle", &PhyNodeHandle);
      if (!EFI_ERROR (Status)) {
        PhyNodeOffset = fdt_node_offset_by_phandle (DeviceTreeNode->DeviceTreeBase, PhyNodeHandle);
        if (PhyNodeOffset > 0) {
          // Get values can ignore status as we already setup defaults and these nodes are optional.
          GetFdtProperty (DeviceTreeNode->DeviceTreeBase, PhyNodeOffset, "reg", &Snp->PhyDriver.PhyAddress);
          GetFdtProperty (DeviceTreeNode->DeviceTreeBase, PhyNodeOffset, "nvidia,phy-rst-duration-usec", &Snp->PhyDriver.ResetDelay);
          Status = GetFdtProperty (DeviceTreeNode->DeviceTreeBase, PhyNodeOffset, "nvidia,phy-rst-pdelay-msec", &Snp->PhyDriver.PostResetDelay);
          if (!EFI_ERROR (Status)) {
            // Convert to usec from msec
            Snp->PhyDriver.PostResetDelay *= 1000;
          }
        }
      }

      // Init PHY
      Status = PhyDxeInitialization (&Snp->PhyDriver, &Snp->MacDriver);
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      osi_get_hw_features (Snp->MacDriver.osi_core, &hw_feat);

      osi_poll_for_mac_reset_complete (Snp->MacDriver.osi_core);

      // Init EMAC DMA
      // Ignore error message on these failure to allow OS to initialize controller
      OsiReturn = osi_hw_dma_init (Snp->MacDriver.osi_dma);
      if (OsiReturn < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to initialize MAC DMA\n"));
      } else {
        Snp->DmaInitialized = TRUE;
        OsiReturn           = osi_hw_core_init (Snp->MacDriver.osi_core, hw_feat.tx_fifo_size, hw_feat.rx_fifo_size);
        if (OsiReturn < 0) {
          DEBUG ((DEBUG_ERROR, "Failed to initialize MAC Core: %d\n", OsiReturn));
        }
      }

      SnpCommitFilters (Snp, TRUE, FALSE);

      // Check for Auto Negotiation completion and rest of Phy setup
      // happens at the exit boot services stage. Creating event for the same.
      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
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
                      &gEfiSimpleNetworkProtocolGuid,
                      &(Snp->Snp),
                      &gEfiAdapterInformationProtocolGuid,
                      &(Snp->Aip),
                      NULL
                      );

      if (EFI_ERROR (Status)) {
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

      Snp = INSTANCE_FROM_SNP_THIS (SnpProtocol);
      gBS->CloseEvent (Snp->DeviceTreeNotifyEvent);
      gBS->CloseEvent (Snp->AcpiNotifyEvent);
      gBS->CloseEvent (Snp->ExitBootServiceEvent);

      Status = gBS->UninstallMultipleProtocolInterfaces (
                      ControllerHandle,
                      &gEfiSimpleNetworkProtocolGuid,
                      &Snp->Snp,
                      &gEfiAdapterInformationProtocolGuid,
                      &(Snp->Aip),
                      NULL
                      );
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
