#
#  SPDX-FileCopyrightText: Copyright (c) 2018-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent

  INF MdeModulePkg/Core/Dxe/DxeMain.inf
  INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf

  #
  # Firmware Performance Data Table (FPDT)
  #
  INF MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  INF Silicon/NVIDIA/Drivers/DebugStatusCodeDxe/DebugStatusCodeDxe.inf
  INF MdeModulePkg/Universal/Acpi/FirmwarePerformanceDataTableDxe/FirmwarePerformanceDxe.inf

  #
  # PI DXE Drivers producing Architectural Protocols (EFI Services)
  #
  INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  INF MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  INF MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
  INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  INF MdeModulePkg/Universal/Metronome/Metronome.inf
  INF MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
  INF MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  INF EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
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

!ifdef CONFIG_ACPI
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

  #
  # ACPI device from DTB
  #
  INF Silicon/NVIDIA/Drivers/AcpiDtbSsdtGenerator/AcpiDtbSsdtGenerator.inf
!endif

  #
  # FDT Support
  #
!ifdef CONFIG_DEVICETREE
  !ifdef CONFIG_ACPI
    INF EmbeddedPkg/Drivers/DtPlatformDxe/DtPlatformDxe.inf
  !else
    INF Silicon/NVIDIA/Drivers/DtOnlyPlatformDxe/DtOnlyPlatformDxe.inf
  !endif
!endif


!ifdef CONFIG_CONFIG_MANAGER
  #
  # Configuration Manager
  #
  INF Silicon/NVIDIA/Drivers/ConfigurationManager/ConfigurationManagerDxe.inf

  #
  # Dynamic Tables support
  #
  !include DynamicTablesPkg/DynamicTables.fdf.inc
!endif

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
  # BPMP-FW IPC protocol
  #
  INF Silicon/NVIDIA/Drivers/BpmpIpc/BpmpIpcDxe.inf
  INF Silicon/NVIDIA/Drivers/BpmpScmi/BpmpScmiDxe.inf
  INF Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.inf

  #
  # GPIO driver/utility
  #
  INF Silicon/NVIDIA/Drivers/TegraGpio/TegraGpioDxe.inf
  INF Silicon/NVIDIA/Drivers/I2cExpanderGpio/I2cExpanderGpio.inf

  #
  # I2C Support
  #
  INF MdeModulePkg/Bus/I2c/I2cDxe/I2cDxe.inf
  INF Silicon/NVIDIA/Drivers/TegraI2c/TegraI2cDxe.inf
  INF Silicon/NVIDIA/Drivers/I2cIoBmcSsifDxe/I2cIoBmcSsifDxe.inf
  INF Silicon/NVIDIA/Drivers/ArmSbmrStatusCodeDxe/ArmSbmrStatusCodeDxe.inf
  INF IpmiFeaturePkg/Frb/FrbDxe.inf

  #
  # IPMI Null Driver
  #
  INF Silicon/NVIDIA/Drivers/IpmiNullDxe/IpmiNullDxe.inf

  #
  # Logo support
  #
!ifdef CONFIG_LOGO
  INF Silicon/NVIDIA/Drivers/Logo/LogoDxe.inf
!ifdef CONFIG_ACPI
  INF MdeModulePkg/Universal/Acpi/BootGraphicsResourceTableDxe/BootGraphicsResourceTableDxe.inf
!endif

  #
  # Logo Files
  #
  FILE FREEFORM = gNVIDIAPlatformLogoGuid {
    SECTION RAW = Silicon/NVIDIA/Assets/nvidiagray480.bmp
    SECTION RAW = Silicon/NVIDIA/Assets/nvidiagray720.bmp
    SECTION RAW = Silicon/NVIDIA/Assets/nvidiagray1080.bmp
  }
!endif

  INF ArmPkg/Drivers/ArmGic/ArmGicDxe.inf
  INF ArmPkg/Drivers/TimerDxe/TimerDxe.inf
!ifdef CONFIG_ARM_WATCHDOG
  INF ArmPkg/Drivers/GenericWatchdogDxe/GenericWatchdogDxe.inf
  INF MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
!else
  INF MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
!endif
  INF Silicon/NVIDIA/Drivers/BootWatchdog/BootWatchdog.inf

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
!ifdef CONFIG_SOC_USB
  INF Silicon/NVIDIA/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf
  INF Silicon/NVIDIA/Drivers/UsbPadCtlDxe/UsbPadCtlDxe.inf
  INF Silicon/NVIDIA/Drivers/XhciControllerDxe/XhciControllerDxe.inf
  INF Silicon/NVIDIA/Drivers/XusbControllerDxe/XusbControllerDxe.inf
!endif
!ifdef CONFIG_UHCI
  INF MdeModulePkg/Bus/Pci/UhciDxe/UhciDxe.inf
!endif
!ifdef CONFIG_EHCI
  INF MdeModulePkg/Bus/Pci/EhciDxe/EhciDxe.inf
!endif
!ifdef CONFIG_XHCI
  INF MdeModulePkg/Bus/Pci/XhciDxe/XhciDxe.inf
!endif
!ifdef CONFIG_USB
  INF MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
!endif
!ifdef CONFIG_USB_KEYBOARD
  INF MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
!endif
!ifdef CONFIG_USB_MOUSE
  INF MdeModulePkg/Bus/Usb/UsbMouseDxe/UsbMouseDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbMouseAbsolutePointerDxe/UsbMouseAbsolutePointerDxe.inf
!endif
!ifdef CONFIG_USB_MASS_STORAGE
  INF MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf
!endif

  #
  # Generic non-discoverable pcie used for many soc devices
  #
  INF MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf

  # USB NIC
  INF Drivers/ASIX/Bus/Usb/UsbNetworking/Ax88772c/Ax88772c.inf

  #
  # PCI Support
  #
!ifdef CONFIG_PCIE
  INF MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf
  INF MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciHostBridgeDxe.inf
  INF ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
!endif
!ifdef CONFIG_DESIGNWARE_PCIE
  INF Silicon/NVIDIA/Drivers/TegraP2UDxe/TegraP2UDxe.inf
  INF Silicon/NVIDIA/Drivers/PcieDWControllerDxe/PcieControllerDxe.inf
!endif
!ifdef CONFIG_NVIDIA_PCIE
  INF Silicon/NVIDIA/Drivers/PcieControllerDxe/PcieControllerDxe.inf
!endif

  #
  # NVMe boot devices
  #
!ifdef CONFIG_NVME
  INF  MdeModulePkg/Bus/Pci/NvmExpressDxe/NvmExpressDxe.inf
!endif

  #
  # EMMC/SD
  #
!ifdef CONFIG_NVIDIA_SDHCI
  INF Silicon/NVIDIA/Drivers/SdMmcControllerDxe/SdMmcControllerDxe.inf
!endif
!ifdef CONFIG_SDCARD
  INF MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf
!endif
!ifdef CONFIG_EMMC
  INF MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf
!endif
!ifdef CONFIG_SDHCI
  INF MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf
!endif

  #
  # UFS support
  #
!ifdef CONFIG_UFS
  INF Silicon/NVIDIA/Drivers/UfsControllerDxe/UfsControllerDxe.inf
  INF MdeModulePkg/Bus/Pci/UfsPciHcDxe/UfsPciHcDxe.inf
  INF MdeModulePkg/Bus/Ufs/UfsPassThruDxe/UfsPassThruDxe.inf
!endif

  #
  # SATA Controller
  #
!ifdef CONFIG_SATA
  INF MdeModulePkg/Bus/Ata/AtaBusDxe/AtaBusDxe.inf
  INF MdeModulePkg/Bus/Ata/AtaAtapiPassThru/AtaAtapiPassThru.inf
!endif

  #
  # SCSI support (Used for UFS, SATA, iSCSI)
  #
!ifdef CONFIG_SCSI
  INF MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
  INF MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf
  INF MdeModulePkg/Bus/Pci/SataControllerDxe/SataControllerDxe.inf
!endif

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
  # UEFI application (Shell Embedded Boot Loader)
  #
!ifdef CONFIG_SHELL
  INF ShellPkg/Application/Shell/Shell.inf
!endif
!ifdef CONFIG_SHELL_NETWORK_COMMANDS
  INF ShellPkg/DynamicCommand/HttpDynamicCommand/HttpDynamicCommand.inf
  INF ShellPkg/DynamicCommand/TftpDynamicCommand/TftpDynamicCommand.inf
  INF ShellPkg/DynamicCommand/DpDynamicCommand/DpDynamicCommand.inf
!endif
  INF MdeModulePkg/Application/VariableInfo/VariableInfo.inf
  #
  # Test Applications
  #
!ifdef CONFIG_NVIDIA_SHELL_APPLICATIONS
  INF Silicon/NVIDIA/Application/StackCheck/StackCheck.inf

!if $(TARGET) != RELEASE
  INF Silicon/NVIDIA/Test/ShellTest/BootOrderTest/BootOrderTestUefiShell.inf
  INF Silicon/NVIDIA/Test/ShellTest/EfiGopTest/EfiGopTestUefiShell.inf
  INF Silicon/NVIDIA/Test/ShellTest/UefiMemMapTest/UefiMemMapTestUefiShell.inf
!endif

  INF Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf
  INF Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  INF Silicon/NVIDIA/Application/GpioUtil/GpioUtil.inf
!ifdef CONFIG_SOC_USB
  INF Silicon/NVIDIA/Application/FalconUtil/FalconUtil.inf
!endif
!endif

  INF SecurityPkg/EnrollFromDefaultKeysApp/EnrollFromDefaultKeysApp.inf

  #
  # SMBIOS/DMI
  #
!ifdef CONFIG_SMBIOS
  INF MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
!endif

  #
  # Bds
  #
  INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  INF MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
!ifndef CONFIG_BOOT_UI_MENU_DISABLED
  INF MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
  INF RuleOverride = UI MdeModulePkg/Application/UiApp/UiApp.inf
  INF MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf
  INF MdeModulePkg/Universal/DriverHealthManagerDxe/DriverHealthManagerDxe.inf
!endif

  #
  # EBC
  #
!ifdef CONFIG_EBC
  INF MdeModulePkg/Universal/EbcDxe/EbcDxe.inf
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
  # Platform Configuration Manager Data
  #
!ifdef CONFIG_CONFIG_MANAGER
!ifdef CONFIG_SOC_XAVIER
  INF Silicon/NVIDIA/Tegra/T194/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
!endif
!ifdef CONFIG_SOC_ORIN
  INF Silicon/NVIDIA/Tegra/T234/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
!endif
!ifdef CONFIG_SOC_GRACE
  INF Silicon/NVIDIA/Server/TH500/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
!endif
!endif

  #
  # Custom HII Support
  #
  INF Silicon/NVIDIA/Drivers/NvidiaConfigDxe/NvidiaConfigDxe.inf

  #
  # Secure Boot Support
  #
  INF SecurityPkg/VariableAuthenticated/SecureBootConfigDxe/SecureBootConfigDxe.inf

  #
  # Security drivers
  #
  INF SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
  INF SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf
  INF SecurityPkg/Pkcs7Verify/Pkcs7VerifyDxe/Pkcs7VerifyDxe.inf

  #
  # RCM Support
  #
  INF Silicon/NVIDIA/Drivers/RcmDxe/RcmDxe.inf

  #
  # Platform To Driver Configuration Support
  #
  INF Silicon/NVIDIA/Drivers/PlatformToDriverConfiguration/PlatformToDriverConfiguration.inf

  #
  # Regulator support
  #
  INF Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf

  #
  # QSPI Support
  #
  INF Silicon/NVIDIA/Drivers/QspiControllerDxe/QspiControllerDxe.inf

  #
  # NOR Flash Support
  #
  INF Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashDxe.inf

  #
  # FVB Support
  #
  INF Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
  INF Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashDxe.inf
  INF MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf

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

  #
  # CPU Frequency support
  #
  INF Silicon/NVIDIA/Drivers/TegraCpuFreqDxe/TegraCpuFreqDxe.inf

  #
  # Tegra Controller Enable Driver
  #
  INF Silicon/NVIDIA/Drivers/TegraControllerEnableDxe/TegraControllerEnableDxe.inf

  #
  # Uphy Configuration Driver
  #
  INF Silicon/NVIDIA/Drivers/UphyConfigurationDxe/UphyConfigurationDxe.inf

  #
  # Fuse Support
  #
  INF Silicon/NVIDIA/Drivers/EFuseDxe/EFuseDxe.inf

  #
  # Secure Boot Provision
  #
  INF Silicon/NVIDIA/Drivers/NvSecureBootProvisionDxe/NvSecureBootProvisionDxe.inf

  #
  # Memory Tests
  #
!ifdef CONFIG_MEMORY_TEST
  INF Silicon/NVIDIA/Drivers/GenericMpMemoryTestDxe/GenericMemoryTestDxe.inf
!endif

  #
  # Multiprocessor support
  #
  INF ArmPkg/Drivers/ArmPsciMpServicesDxe/ArmPsciMpServicesDxe.inf
  INF Silicon/NVIDIA/Drivers/ThreadingDxe/ThreadingDxe.inf

  #
  # L4T launcher
  #
!ifdef CONFIG_SINGLE_BOOT_L4T_LAUNCHER
  INF Silicon/NVIDIA/Application/L4TLauncher/L4TLauncher.inf
!endif

  # WFI App
!ifdef CONFIG_SINGLE_BOOT_DEADLOOP_APP
  INF Silicon/NVIDIA/Application/WfiApp/WfiApp.inf
!endif

  # Boot support for mkbootimg partitions
!ifdef CONFIG_BOOT_NATIVE_ANDROID_PARTITION_SUPPORT
  INF Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf
!endif

  #
  # Gpio Power Off Driver
  #
  INF Silicon/NVIDIA/Drivers/GpioPowerOffDxe/GpioPowerOffDxe.inf
