#
#  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent

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
  INF Silicon/NVIDIA/Drivers/DefaultVariableDxe/DefaultVariableDxe.inf
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableSmmRuntimeDxe.inf
  INF MdeModulePkg/Universal/TimestampDxe/TimestampDxe.inf
  INF MdeModulePkg/Universal/StatusCodeHandler/RuntimeDxe/StatusCodeHandlerRuntimeDxe.inf

  #
  # Esrt - EFI System Resource Table
  #
  INF MdeModulePkg/Universal/EsrtFmpDxe/EsrtFmpDxe.inf

  #
  # Fmp
  #
  INF FILE_GUID = $(SYSTEM_FMP_ESRT_GUID) FmpDevicePkg/FmpDxe/FmpDxe.inf

  #
  # Device discovery protocol
  #
  INF Silicon/NVIDIA/Drivers/DeviceDiscovery/DeviceDiscoveryDxe.inf

  #
  # Aml Generation
  #
  INF Silicon/NVIDIA/Drivers/AmlGenerationDxe/AmlGenerationDxe.inf

  #
  # Aml Patching
  #
  INF Silicon/NVIDIA/Drivers/AmlPatchDxe/AmlPatchDxe.inf

  #
  # ACPI Support
  #
  INF MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  #INF MdeModulePkg/Universal/Acpi/AcpiPlatformDxe/AcpiPlatformDxe.inf

  #
  # Configuration Manager
  #
  INF Silicon/NVIDIA/Drivers/ConfigurationManager/ConfigurationManagerDxe.inf

  #
  # Dynamic Tables support
  #
  !include DynamicTablesPkg/DynamicTables.fdf.inc

  INF MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf

  #
  # HTTP Boot support
  #
  INF MdeModulePkg/Universal/Disk/RamDiskDxe/RamDiskDxe.inf

  #
  # Multiple Console IO support
  #
  INF MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  INF MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  INF MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
  INF MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
  INF MdeModulePkg/Universal/PrintDxe/PrintDxe.inf

  #
  # I2C Support
  #
  INF MdeModulePkg/Bus/I2c/I2cDxe/I2cDxe.inf
  INF Silicon/NVIDIA/Drivers/TegraI2c/TegraI2cDxe.inf
  INF Silicon/NVIDIA/Drivers/I2cIoBmcSsifDxe/I2cIoBmcSsifDxe.inf
  INF Silicon/NVIDIA/Drivers/ArmSmbrStatusCodeDxe/ArmSmbrStatusCodeDxe.inf
  INF IpmiFeaturePkg/Frb/FrbDxe.inf

  #
  # Graphics console support
  #
  INF Silicon/NVIDIA/Drivers/Logo/LogoDxe.inf
  INF MdeModulePkg/Universal/Acpi/BootGraphicsResourceTableDxe/BootGraphicsResourceTableDxe.inf

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
  INF MdeModulePkg/Universal/Disk/UdfDxe/UdfDxe.inf

  # FV FileSystem
  INF MdeModulePkg/Universal/FvSimpleFileSystemDxe/FvSimpleFileSystemDxe.inf
  INF MdeModulePkg/Universal/SectionExtractionDxe/SectionExtractionDxe.inf

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
  INF MdeModulePkg/Bus/Usb/UsbMouseAbsolutePointerDxe/UsbMouseAbsolutePointerDxe.inf
  INF MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
  INF Drivers/ASIX/Bus/Usb/UsbNetworking/Ax88772c/Ax88772c.inf

  #
  # PCI Support
  #
  INF MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf
  INF MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciHostBridgeDxe.inf

  #
  # SATA Controller
  #
  INF MdeModulePkg/Bus/Ata/AtaBusDxe/AtaBusDxe.inf
  INF MdeModulePkg/Bus/Ata/AtaAtapiPassThru/AtaAtapiPassThru.inf
  INF MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
  INF MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf
  INF MdeModulePkg/Bus/Pci/SataControllerDxe/SataControllerDxe.inf

  #
  # NVMe boot devices
  #
  INF  MdeModulePkg/Bus/Pci/NvmExpressDxe/NvmExpressDxe.inf

  #
  # Networking stack
  #
  INF NetworkPkg/DpcDxe/DpcDxe.inf
  INF NetworkPkg/ArpDxe/ArpDxe.inf
  INF NetworkPkg/Dhcp4Dxe/Dhcp4Dxe.inf
  INF NetworkPkg/Ip4Dxe/Ip4Dxe.inf
  INF NetworkPkg/MnpDxe/MnpDxe.inf
  INF NetworkPkg/SnpDxe/SnpDxe.inf
  INF NetworkPkg/VlanConfigDxe/VlanConfigDxe.inf
  INF NetworkPkg/Mtftp4Dxe/Mtftp4Dxe.inf
  INF NetworkPkg/TcpDxe/TcpDxe.inf
  INF NetworkPkg/Udp4Dxe/Udp4Dxe.inf
  INF NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf
  INF NetworkPkg/IScsiDxe/IScsiDxe.inf
  INF NetworkPkg/Ip6Dxe/Ip6Dxe.inf
  INF NetworkPkg/Dhcp6Dxe/Dhcp6Dxe.inf
  INF NetworkPkg/Udp6Dxe/Udp6Dxe.inf
  INF NetworkPkg/Mtftp6Dxe/Mtftp6Dxe.inf

  INF NetworkPkg/TlsDxe/TlsDxe.inf
  INF NetworkPkg/TlsAuthConfigDxe/TlsAuthConfigDxe.inf
  INF NetworkPkg/DnsDxe/DnsDxe.inf
  INF NetworkPkg/HttpDxe/HttpDxe.inf
  INF NetworkPkg/HttpUtilitiesDxe/HttpUtilitiesDxe.inf
  INF NetworkPkg/HttpBootDxe/HttpBootDxe.inf

  #
  # Shell dynamic commands
  #
  INF ShellPkg/DynamicCommand/TftpDynamicCommand/TftpDynamicCommand.inf
  INF ShellPkg/DynamicCommand/DpDynamicCommand/DpDynamicCommand.inf

  #
  # UEFI applications
  #
  INF ShellPkg/Application/Shell/Shell.inf
  INF MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf
  INF MdeModulePkg/Application/VariableInfo/VariableInfo.inf

  #
  # SMBIOS/DMI
  #
  INF MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
  INF ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClassDxe.inf
  INF ArmPkg/Universal/Smbios/SmbiosMiscDxe/SmbiosMiscDxe.inf
  INF Silicon/NVIDIA/Drivers/SmbiosOemDxe/SmbiosMiscOemDxe/SmbiosMiscOemDxe.inf

  #
  # Bds
  #
  INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  INF MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  INF MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
  INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
  INF MdeModulePkg/Application/UiApp/UiApp.inf
  INF MdeModulePkg/Universal/DriverHealthManagerDxe/DriverHealthManagerDxe.inf

  #
  # EBC
  #
  INF MdeModulePkg/Universal/EbcDxe/EbcDxe.inf

  #
  # Test Applications
  #
  INF Silicon/NVIDIA/Application/StackCheck/StackCheck.inf

!if $(TARGET) != RELEASE
  INF Silicon/NVIDIA/Test/ShellTest/BootOrderTest/BootOrderTestUefiShell.inf
  INF Silicon/NVIDIA/Test/ShellTest/EfiGopTest/EfiGopTestUefiShell.inf
  INF Silicon/NVIDIA/Test/ShellTest/UefiMemMapTest/UefiMemMapTestUefiShell.inf
!endif

  #
  # Tegra Platform Boot Manager driver
  #
  INF Silicon/NVIDIA/Drivers/TegraPlatformBootManager/TegraPlatformBootManagerDxe.inf

  #
  # Tegra Platform Configuration
  #
  INF Silicon/NVIDIA/Drivers/TegraPlatformInit/TegraPlatformInitDxe.inf

  #
  # Custom HII Support
  #
  INF Silicon/NVIDIA/Drivers/ResourceConfigDxe/ResourceConfigDxe.inf

  #
  # Secure Boot Support
  #
  INF SecurityPkg/VariableAuthenticated/SecureBootConfigDxe/SecureBootConfigDxe.inf

  #
  # RCM Support
  #
  INF Silicon/NVIDIA/Drivers/RcmDxe/RcmDxe.inf

  #
  # Platform To Driver Configuration Support
  #
  INF Silicon/NVIDIA/Drivers/PlatformToDriverConfiguration/PlatformToDriverConfiguration.inf

  #
  # Logo Files
  #
  FILE FREEFORM = gNVIDIAPlatformLogoGuid {
    SECTION RAW = Silicon/NVIDIA/Assets/nvidiagray480.bmp
    SECTION RAW = Silicon/NVIDIA/Assets/nvidiagray720.bmp
    SECTION RAW = Silicon/NVIDIA/Assets/nvidiagray1080.bmp
  }

  #
  # FW Partition
  #
  INF Silicon/NVIDIA/Drivers/FwPartitionNorFlashDxe/FwPartitionNorFlashDxe.inf
  INF Silicon/NVIDIA/Drivers/FwPartitionBlockIoDxe/FwPartitionBlockIoDxe.inf
  INF Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionMmDxe.inf
  INF Silicon/NVIDIA/Drivers/FwImageDxe/FwImageDxe.inf

  #
  # Capsule Update Policy Protocol
  #
  INF Silicon/NVIDIA/Drivers/CapsuleUpdatePolicyDxe/CapsuleUpdatePolicyDxe.inf

  #
  # Tegra EEPROM Support
  #
  INF Silicon/NVIDIA/Drivers/EepromDxe/Eeprom.inf

  #
  # Third party drivers
  #
  INF Drivers/Realtek/Bus/Pcie/PcieNetworking/RtkUndiDxe.inf

  #
  # Tegra Pin Control support
  #
  INF Silicon/NVIDIA/Drivers/TegraPinControlDxe/TegraPinControlDxe.inf

  #
  # Boot Chain Protocol
  #
  INF Silicon/NVIDIA/Drivers/BootChainDxe/BootChainDxe.inf
  #
  # Ipmi Blob Transfer support
  #
  INF Silicon/NVIDIA/Drivers/IpmiBlobTransferDxe/IpmiBlobTransferDxe.inf
  INF Silicon/NVIDIA/Drivers/SmbiosBmcTransfer/SmbiosBmcTransfer.inf
