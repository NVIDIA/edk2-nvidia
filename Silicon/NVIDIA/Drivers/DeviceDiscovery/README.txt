/** @file

  Component description file for the Device Discovery DXE platform driver.

  Copyright (c) 2018, NVIDIA Corporation. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

= Device discovery service driver =
  Provides system services to enumerate and install components for otherwise non-discoverable devices

== Details ==

The driver will cause the device tree to be processed for each driver that exposes
gNVIDIADeviceTreeCompatibilityProtocolGuid. Each node on the device tree will be passed to the driver's Supported
function. If the driver returns EFI_SUCCESS the device discovery driver will detect and map the memory of the node
into the system MMU. After this a device handle will be created to expose this to the device driver.
This handle will contain information regarding the location of the device tree node, a device path based on the
first memory map region, and the non-discoverable device protocol. If the device driver supports the driver binding
protocol, then the non-discoverable device protocol will be installed with a non-standard GUID and a non-recursive
ConnectController() will be invoked.

== Device Driver Usage ==

The primary task the driver needs to do is install a device handle with the compatibilty protocol and return
EFI_SUCCESS for correct device nodes. If the driver needs to perform controller initialization then it should
expose a driver binding protocol on the same handle. If the controller initialization is not required to be done
if the hardware is not in the boot path the functional driver binding protocol should be exposed on a seperate device
handle.

If the driver initiates the hardware initialization in an asynch manner then the DriverHealth protocol should be
supported and RepairRequired should be returned while this process is occuring.

== Device Library Usage ==

There may be cases where a full UEFI driver model method can not be used as the interface with external components
require a library to be implemented. In this case the library should register a depex with
gNVIDIADeviceEnumerationPresentProtocolGuid and prior to detecting the hardware they should install the compatibilty
protocol. This will trigger an immediate callback to the device discovery driver to perform an enumeration.
When the DxeCore returns from the install function the library can then get the non-discoverable device protocols to
locate the appropriate instances.

== UML details on flow of architecture ==
@startuml
title Device Discovery Infrastructure
participant SystemResourceLib as SRL
participant "Device Discovery Driver" as DDD
database    "Dxe Core" as DXE
database    "HOB List" as HOB
participant BDS
participant "HW Device Driver" as HWDD
participant "HW Device Driver (non UEFI driver model)" as HWDD_S
== SEC ==
activate SRL
SRL -> SRL : Detect Devicetree
SRL -> HOB : Add DTB entry
deactivate SRL
== DXE ==
activate HWDD
HWDD -> DXE : Install Driver binding protocol \n Function should detect non-standard non-discoverable protocol guid
note right  : Needed unless there are no custom operations needed by driver
HWDD -> DXE : Install Component name protocol
HWDD -> DXE : Install DTB Compatibility protocol
HWDD -> DXE : Install Driver Diagnostics protocol, if supported
HWDD -> DXE : Install Driver Health protocol, if async init is needed
deactivate HWDD
activate DDD
DDD <-> HOB : Get DTB entry
DDD -> DDD : Validate DTB
DDD -> DXE : Register Protocol Notification for DTB compatibility protocol
DDD -> DXE : Signal Register Notification event
note left : To process any drivers already loaded
DXE -> DDD : Invoke Register Notification handler
note right : This will get called in the future for any future installation of DTB compatibility protocol
group Register Notification handler
  DDD <-> DXE : LocateHandle () - Gets all new DTB compatibility handlers
  loop for all DTB nodes
    loop for all driver handles
      DDD -> HWDD : Is node supported
      alt Supported
        HWDD -> DDD : Returns UUID, PciIO Init function
        DDD -> DDD : Get MMIO resources
        DDD -> DXE : Add Memory Space
        note right : Aligned to 4K
        DXE -> "CPU Driver" : Set MMU mappings
        DXE -> DDD : Memory space added
        DDD -> DDD : Detect dma-coherent parameter
        DDD -> DXE : Install DTB info protocol
        DDD -> DXE : Install device path based on MMIO address
        note right : DXE will prevent duplicate device entries from being created
        alt Driver handle support UEFI driver binding
          DDD -> DXE : Install non-discovery protocol with custom GUID
          DDD -> HWDD : Call DriverBinding->Supported()
          HWDD -> DDD : Return EFI_SUCCESS
          DDD -> HWDD : Call DriverBinding->Start()
          HWDD -> HWDD: Validate node resources
          HWDD -> HWDD: Perform any simple init
          HWDD -> HWDD: Start async init
          HWDD -> DXE : Install any additional protocols
          note right
            Protocols can either be services exposed by hardware
            or protocols that service driver binds to.
            Example of second type would be the NonDiscoverable protocol with type XHCI.
            This would allow for other drivers to provide USB services
            Processing in start() should mainly be limited actions required for boot regardless of boot device.
          end note
          HWDD -> DDD : return EFI_SUCCESS
        else Driver handle does not support driver binding
            DDD -> DXE : Install non-discovery protocol with standard GUID
        end
        DDD -> DDD : Exit device handle loop
      else Unsupported
        DDD -> DDD : Move to next driver handle
      end
    end
    DDD -> DDD : Move to next node
  end
end
DDD -> DXE: Install DDD presence protocol
deactivate DDD
DXE -> HWDD_S : Dispatch driver
note left : Has dependency registered with DDD presence protocol installation
activate  HWDD_S
HWDD_S -> DXE : Install DTB Compatibility protocol
DXE -> DDD : Invoke Register Notification handler
note right : Flow is same as above handler group
DDD -> HWDD_S : Check for supported on all nodes
DDD -> DXE : Install non-discovery protocol for supported nodes
DDD -> DXE : Return from notification
DXE -> HWDD_S : Return from protocol installation
HWDD_S -> DXE : Get all discoverable GUID protocols
HWDD_S -> HWDD_S : Check device type for match
HWDD_S -> HWDD_S : Perform any needed hardware initialization
deactivate HWDD_S
== BDS (Device discovery specific ==
activate BDS
BDS -> BDS : Set Repair Needed to TRUE
loop while Repair Needed
  BDS -> BDS : Set Repair Needed to FALSE
  loop for all instance of DeviceHealth
    BDS -> HWDD : GetDeviceHealth ()
    HWDD -> BDS : Return device health, RepairNeeded if initialization required
    BDS -> BDS  : If RepairNeeded returned set Repair Needed to TRUE
    BDS -> HWDD : Repair ()
    HWDD -> HWDD : Initiate async hardware initialization
    HWDD -> BDS  : RepairNotify () if not NULL for progress
    HWDD -> BDS  : Return
  end
end
== BDS (Standard) ==
BDS -> DXE : Connect boot required devices (i.e. console)
loop for boot options
  BDS -> DXE : Connect recursively for boot device's device path
  BDS -> BDS : Attempt boot of boot devices
end
@enduml

