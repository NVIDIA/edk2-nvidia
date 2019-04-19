#
#  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
  INF MdeModulePkg/Core/Dxe/DxeMain.inf
  INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf

  #
  # Firmware Performance Data Table (FPDT)
  #
  INF MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  INF MdeModulePkg/Universal/Acpi/FirmwarePerformanceDataTableDxe/FirmwarePerformanceDxe.inf

  #
  # PI DXE Drivers producing Architectural Protocols (EFI Services)
  #
  INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  INF MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  INF MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
  INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  INF MdeModulePkg/Application/CapsuleApp/CapsuleApp.inf
  INF MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
  INF MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  INF EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  INF EmbeddedPkg/MetronomeDxe/MetronomeDxe.inf
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf

  #
  # Fmp
  #
  INF FILE_GUID = $(SYSTEM_FMP_ESRT_GUID) FmpDevicePkg/FmpDxe/FmpDxe.inf

  #
  # Device discovery protocol
  #
  INF Silicon/NVIDIA/Drivers/DeviceDiscovery/DeviceDiscoveryDxe.inf

  #
  # ACPI Support
  #
  INF MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  #INF MdeModulePkg/Universal/Acpi/AcpiPlatformDxe/AcpiPlatformDxe.inf

  # Configuration Manager
  INF Silicon/NVIDIA/$(CHIPSET)/Drivers/CMDxe/CMDxe/CMDxe.inf

  #
  # Dynamic Table Factory Dxe
  #
  INF DynamicTablesPkg/Drivers/DynamicTableFactoryDxe/DynamicTableFactoryDxe.inf

  #
  # Dynamic Tables Dxe
  #
  INF DynamicTablesPkg/Drivers/DynamicTableManagerDxe/DynamicTableManagerDxe.inf

  INF MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf

  #
  # Multiple Console IO support
  #
  INF MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  INF MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  INF MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
  INF MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
  INF MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

  INF ArmPkg/Drivers/ArmGic/ArmGicDxe.inf
  INF ArmPkg/Drivers/TimerDxe/TimerDxe.inf
  INF MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf

  #
  # FAT filesystem + GPT/MBR partitioning
  #
  INF MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  INF MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  INF FatPkg/EnhancedFatDxe/Fat.inf
  INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf

  # FV FileSystem
  INF MdeModulePkg/Universal/FvSimpleFileSystemDxe/FvSimpleFileSystemDxe.inf

  #
  # Usb Support
  #
  INF MdeModulePkg/Bus/Pci/UhciDxe/UhciDxe.inf
  INF MdeModulePkg/Bus/Pci/EhciDxe/EhciDxe.inf
  INF MdeModulePkg/Bus/Pci/XhciDxe/XhciDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbMouseDxe/UsbMouseDxe.inf
  INF MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf

  #
  # PCI Support
  #
  INF MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf
  #INF MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciHostBridgeDxe.inf

  #
  # SATA Controller
  #
  INF MdeModulePkg/Bus/Ata/AtaBusDxe/AtaBusDxe.inf

  #
  # NVMe boot devices
  #
  INF  MdeModulePkg/Bus/Pci/NvmExpressDxe/NvmExpressDxe.inf

  #
  # Networking stack
  #
  INF MdeModulePkg/Universal/Network/DpcDxe/DpcDxe.inf
  INF MdeModulePkg/Universal/Network/ArpDxe/ArpDxe.inf
  INF MdeModulePkg/Universal/Network/Dhcp4Dxe/Dhcp4Dxe.inf
  INF MdeModulePkg/Universal/Network/Ip4Dxe/Ip4Dxe.inf
  INF MdeModulePkg/Universal/Network/MnpDxe/MnpDxe.inf
  INF MdeModulePkg/Universal/Network/VlanConfigDxe/VlanConfigDxe.inf
  INF MdeModulePkg/Universal/Network/Mtftp4Dxe/Mtftp4Dxe.inf
  INF NetworkPkg/TcpDxe/TcpDxe.inf
  INF MdeModulePkg/Universal/Network/Udp4Dxe/Udp4Dxe.inf
  INF NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf
  INF NetworkPkg/IScsiDxe/IScsiDxe.inf
  INF NetworkPkg/Ip6Dxe/Ip6Dxe.inf
  INF NetworkPkg/Dhcp6Dxe/Dhcp6Dxe.inf
  INF NetworkPkg/Udp6Dxe/Udp6Dxe.inf
  INF NetworkPkg/Mtftp6Dxe/Mtftp6Dxe.inf

  #
  # Shell dynamic commands
  #
  INF ShellPkg/DynamicCommand/TftpDynamicCommand/TftpDynamicCommand.inf
  INF ShellPkg/DynamicCommand/DpDynamicCommand/DpDynamicCommand.inf

  #
  # UEFI applications
  #
  INF ShellPkg/Application/Shell/Shell.inf


  #
  # SMBIOS/DMI
  #
  INF MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
  #TODO Add SMBIOS tables

  #
  # Bds
  #
  INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  INF MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  INF MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
  INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
  INF MdeModulePkg/Application/UiApp/UiApp.inf

  #
  # EBC
  #
  INF MdeModulePkg/Universal/EbcDxe/EbcDxe.inf

  #
  # Test Applications
  #
  INF Silicon/NVIDIA/Application/StackCheck/StackCheck.inf

