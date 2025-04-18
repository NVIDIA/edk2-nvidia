#
#  SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent

################################################################################
#
# FV Section
#
# [FV] section is used to define what components or modules are placed within a flash
# device file.  This section also defines order the components and modules are positioned
# within the image.  The [FV] section consists of define statements, set statements and
# module statements.
#
################################################################################

[FV.FvMain]
BlockSize          = 0x10000
NumBlocks          = 0         # This FV gets compressed so make it just big enough
FvAlignment        = 8
ERASE_POLARITY     = 1
MEMORY_MAPPED      = TRUE
STICKY_WRITE       = TRUE
LOCK_CAP           = TRUE
LOCK_STATUS        = TRUE
WRITE_DISABLED_CAP = TRUE
WRITE_ENABLED_CAP  = TRUE
WRITE_STATUS       = TRUE
WRITE_LOCK_CAP     = TRUE
WRITE_LOCK_STATUS  = TRUE
READ_DISABLED_CAP  = TRUE
READ_ENABLED_CAP   = TRUE
READ_STATUS        = TRUE
READ_LOCK_CAP      = TRUE
READ_LOCK_STATUS   = TRUE
FvNameGuid         = $(PLATFORM_GUID)

  APRIORI DXE {
    INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf
    INF MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
    INF Silicon/NVIDIA/Drivers/DebugStatusCodeDxe/DebugStatusCodeDxe.inf
    INF Silicon/NVIDIA/Drivers/TegraPlatformInit/TegraPlatformInitDxe.inf
    INF ArmPkg/Drivers/ArmGic/ArmGicDxe.inf
    INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
    INF MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
!ifdef CONFIG_SDHCI
    INF MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf
!endif
!ifdef CONFIG_UFS
    INF MdeModulePkg/Bus/Pci/UfsPciHcDxe/UfsPciHcDxe.inf
!endif
!ifdef CONFIG_EMMC
    INF MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf
!endif
!ifdef CONFIG_SDCARD
    INF MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf
!endif
!ifdef CONFIG_UFS
    INF MdeModulePkg/Bus/Ufs/UfsPassThruDxe/UfsPassThruDxe.inf
!endif
    INF MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
    INF MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
    INF MdeModulePkg/Bus/I2c/I2cDxe/I2cDxe.inf
!ifdef CONFIG_SCSI
    INF MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
    INF MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf
!endif
!ifdef CONFIG_FS_FAT
    INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
    INF FatPkg/EnhancedFatDxe/Fat.inf
!endif
    }

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
  INF MdeModulePkg/Universal/SectionExtractionDxe/SectionExtractionDxe.inf
  INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  INF MdeModulePkg/Universal/Metronome/Metronome.inf
  INF MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
  INF MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  INF EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  INF Silicon/NVIDIA/Drivers/DefaultVariableDxe/DefaultVariableDxe.inf
!ifdef CONFIG_VARIABLE_NON_SECURE_SUPPORTED
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf
!endif
!ifdef CONFIG_SECURITY_MM_SUPPORT
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableSmmRuntimeDxe.inf
!endif
  INF MdeModulePkg/Universal/TimestampDxe/TimestampDxe.inf

!ifdef CONFIG_FIRMWARE_PARTITION_SUPPORT
  #
  # FW Partition and FW Image
  #
  INF Silicon/NVIDIA/Drivers/FwPartitionNorFlashDxe/FwPartitionNorFlashDxe.inf
  INF Silicon/NVIDIA/Drivers/FwPartitionBlockIoDxe/FwPartitionBlockIoDxe.inf
  INF Silicon/NVIDIA/Drivers/FwImageDxe/FwImageDxe.inf
!endif
!ifdef CONFIG_FIRMWARE_PARTITION_MM_DXE_DRIVER
  INF Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionMmDxe.inf
!endif

!ifdef CONFIG_FIRMWARE_MANAGEMENT
  #
  # Esrt - EFI System Resource Table
  #
  INF MdeModulePkg/Universal/EsrtFmpDxe/EsrtFmpDxe.inf

  #
  # Fmp
  #
  INF FILE_GUID = $(FMP_SYSTEM_IMAGE_TYPE_ID) FmpDevicePkg/FmpDxe/FmpDxe.inf
!endif

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
  INF Silicon/NVIDIA/Drivers/ConfigurationManagerTokenDxe/ConfigurationManagerTokenDxe.inf
  INF Silicon/NVIDIA/Drivers/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
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
!ifdef CONFIG_IPMI_BMC
  INF Silicon/NVIDIA/Drivers/I2cIoBmcSsifDxe/I2cIoBmcSsifDxe.inf
  INF Silicon/NVIDIA/Drivers/ArmSbmrStatusCodeDxe/ArmSbmrStatusCodeDxe.inf
  INF Silicon/NVIDIA/Drivers/OemDescStatusCodeDxe/OemDescStatusCodeDxe.inf
  INF Silicon/NVIDIA/Drivers/ResetNotifyRuntimeDxe/ResetNotifyRuntimeDxe.inf
  INF IpmiFeaturePkg/Frb/FrbDxe.inf

  #
  # IPMI Null Driver
  #
  INF Silicon/NVIDIA/Drivers/IpmiNullDxe/IpmiNullDxe.inf

  #
  # Ipmi Blob Transfer support
  #
  INF Silicon/NVIDIA/Drivers/IpmiBlobTransferDxe/IpmiBlobTransferDxe.inf
  INF Silicon/NVIDIA/Drivers/SmbiosBmcTransfer/SmbiosBmcTransfer.inf
  INF Silicon/NVIDIA/Drivers/SmbiosHmcTransfer/SmbiosHmcTransfer.inf
!endif

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
  INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  INF MdeModulePkg/Universal/Disk/UdfDxe/UdfDxe.inf

  # FileSystems
!ifdef CONFIG_FS_FIRMWARE_VOLUME
  INF MdeModulePkg/Universal/FvSimpleFileSystemDxe/FvSimpleFileSystemDxe.inf
!endif
!ifdef CONFIG_FS_FAT
  INF FatPkg/EnhancedFatDxe/Fat.inf
!endif
!ifdef CONFIG_FS_EXT4
!include Features/Ext4Pkg/Ext4.fdf.inc
!endif

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
!ifdef CONFIG_USB_FW_RENESAS
!ifdef $(ENABLE_RENESAS_USB_FW)
  #
  # Renesas USB controller firmware upload support
  #
  INF Silicon/NVIDIA/Drivers/RenesasUsbCtlr/RenesasUsbCtlr.inf
  #
  # Renesas USB controller firmware binary
  #
  FILE FREEFORM = gNVIDIAuPD72020xFirmwareGuid {
    SECTION RAW = Silicon/Renesas/uPD72020x/K2026090.mem
  }
!endif
!endif

  #
  # Generic non-discoverable pcie used for many soc devices
  #
  INF MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf

  #
  # Serial console driver
  #
!ifdef CONFIG_SERIAL_PORT_CONSOLE_MDEMODULE
  INF MdeModulePkg/Universal/SerialDxe/SerialDxe.inf
!endif
!ifdef CONFIG_SERIAL_PORT_CONSOLE_TEGRA
  INF Silicon/NVIDIA/Drivers/TegraUartDxe/TegraUartDxe.inf
!endif

  #
  # PCI Support
  #
!ifdef CONFIG_PCIE
  INF MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf
  INF MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciHostBridgeDxe.inf
  INF ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
  INF Silicon/NVIDIA/Drivers/PciGenericControllerDxe/PcieGenericControllerDxe.inf
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
  # Display hardware
  #
!ifdef CONFIG_NVIDIA_DISPLAY
  !include Silicon/NVIDIA/Drivers/NvGopDriver/NvGopDriver.fdf.inc
  INF Silicon/NVIDIA/Drivers/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
  INF Silicon/NVIDIA/Drivers/NvDisplayControllerDxe/NvDisplayControllerDxe.inf
!endif
!ifdef CONFIG_ASPEED_2600_DISPLAY
  #
  # AST2600 VGA GOP Support
  FILE DRIVER = b403dcf9-8b43-44f7-b9ea-383eb33e548d {
    SECTION PE32 = Drivers/ASpeed/ASpeedGopBinPkg/AArch64/ASpeedAst2600Gop.efi
  }
!endif

  #
  # SE RNG
  #
!ifdef CONFIG_SOC_ORIN
  INF Silicon/NVIDIA/Tegra/T234/Drivers/SeRngDxe/SeRngDxe.inf
!endif

  #
  # Common Jetson devices
  #
!ifdef CONFIG_SOC_JETSON
  #
  # PinMux Support
  #
  INF Silicon/NVIDIA/Drivers/PinMuxDxe/PinMuxDxe.inf

  #
  # Tegra PWM clock driver
  #
  INF Silicon/NVIDIA/Drivers/TegraPwmDxe/TegraPwmDxe.inf

  #
  # Golden Register Support
  #
  INF Silicon/NVIDIA/Drivers/GoldenRegisterDxe/GoldenRegisterDxe.inf

  #
  # BasicProfiler Support
  #
  INF Silicon/NVIDIA/Drivers/BasicProfilerDxe/BasicProfilerDxe.inf
!endif

!ifdef CONFIG_RAS
  #
  # APEI
  #
  INF Silicon/NVIDIA/Server/TH500/Drivers/ApeiDxe/Apei.inf
!endif

!ifdef CONFIG_BOOTLOADER_RAMDISK
  #
  # Ramloaded OS
  #
  INF Silicon/NVIDIA/Drivers/RamDiskOS/RamDiskOS.inf
!endif


!ifdef CONFIG_SOC_T24X
  #
  # GH100 GPU Driver
  #
  INF Silicon/NVIDIA/Server/TH500/Drivers/TH500GpuDxe/TH500GpuDxe.inf

  #
  # Oem Partition Sample Driver.
  # Sample driver only not included in firmware image
  #
  # INF Silicon/NVIDIA/Server/TH500/Drivers/OemPartitionDxeMm/OemPartitionDxe.inf
!endif

!ifdef CONFIG_SOC_DATACENTER
  #
  # FPGA I2C Support
  #
  INF Silicon/NVIDIA/Drivers/FpgaI2c/FpgaI2c.inf
!endif

  #
  # Networking stack
  #
!ifdef CONFIG_NETWORKING
  INF NetworkPkg/DpcDxe/DpcDxe.inf
  INF NetworkPkg/ArpDxe/ArpDxe.inf
  INF NetworkPkg/MnpDxe/MnpDxe.inf
  INF NetworkPkg/SnpDxe/SnpDxe.inf
  INF NetworkPkg/TcpDxe/TcpDxe.inf
  INF NetworkPkg/DnsDxe/DnsDxe.inf
!ifdef CONFIG_NETWORKING_IPV4
  INF NetworkPkg/Dhcp4Dxe/Dhcp4Dxe.inf
  INF NetworkPkg/Ip4Dxe/Ip4Dxe.inf
  INF NetworkPkg/Mtftp4Dxe/Mtftp4Dxe.inf
  INF NetworkPkg/Udp4Dxe/Udp4Dxe.inf
!endif
!ifdef CONFIG_NETWORKING_IPV6
  INF NetworkPkg/Dhcp6Dxe/Dhcp6Dxe.inf
  INF NetworkPkg/Ip6Dxe/Ip6Dxe.inf
  INF NetworkPkg/Mtftp6Dxe/Mtftp6Dxe.inf
  INF NetworkPkg/Udp6Dxe/Udp6Dxe.inf
!endif
!ifdef CONFIG_NETWORKING_VLAN
  INF NetworkPkg/VlanConfigDxe/VlanConfigDxe.inf
!endif
!ifdef CONFIG_NETWORKING_PXE
  INF NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf
!endif
!ifdef CONFIG_NETWORKING_ISCSI
  INF NetworkPkg/IScsiDxe/IScsiDxe.inf
!endif
!ifdef CONFIG_NETWORKING_HTTP
  INF NetworkPkg/TlsDxe/TlsDxe.inf
  INF NetworkPkg/TlsAuthConfigDxe/TlsAuthConfigDxe.inf
  INF NetworkPkg/HttpDxe/HttpDxe.inf
  INF NetworkPkg/HttpUtilitiesDxe/HttpUtilitiesDxe.inf
  INF NetworkPkg/HttpBootDxe/HttpBootDxe.inf
!endif
!endif

  #
  # NIC drivers
  #
!ifdef CONFIG_NETWORKING_DEVICE_NVIDIA
  INF Silicon/NVIDIA/Drivers/EqosDeviceDxe/EqosDeviceDxe.inf
!endif
!ifdef CONFIG_NETWORKING_DEVICE_INTEL
!ifdef $(ENABLE_INTELUNDI)
  #
  # Intel 500 series NIC
  #
  INF IntelUndiPkg/XGigUndiDxe/XGigUndiDxe.inf
  #
  # Intel 700 series NIC
  #
  INF IntelUndiPkg/I40eUndiDxe/I40eUndiDxe.inf
  #
  # Intel 200 series NIC
  #
  INF IntelUndiPkg/GigUndiDxe/GigUndiDxe.inf
!endif
!endif
!ifdef CONFIG_NETWORKING_DEVICE_REALTEK
!ifdef CONFIG_USB
  INF Drivers/Realtek/Bus/Usb/UsbNetworking/RtkUsbUndiDxe.inf
!endif
  INF Drivers/Realtek/Bus/Pcie/PcieNetworking/RtkUndiDxe.inf
!endif
!ifdef CONFIG_NETWORKING_DEVICE_ASIX
  INF Drivers/ASIX/Bus/Usb/UsbNetworking/Ax88772c/Ax88772c.inf
!endif
!ifdef CONFIG_NETWORKING_DEVICE_USB_RNDIS
  INF Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/UsbRndisDxe.inf
!endif

  #
  # UEFI application (Shell Embedded Boot Loader)
  #
!ifdef CONFIG_SHELL
  INF ShellPkg/Application/Shell/Shell.inf
!endif
!ifdef CONFIG_SHELL_NETWORK_COMMANDS
!ifdef CONFIG_NETWORKING_HTTP
  INF ShellPkg/DynamicCommand/HttpDynamicCommand/HttpDynamicCommand.inf
!endif
  INF ShellPkg/DynamicCommand/TftpDynamicCommand/TftpDynamicCommand.inf
  INF ShellPkg/DynamicCommand/DpDynamicCommand/DpDynamicCommand.inf
!endif
  INF MdeModulePkg/Application/VariableInfo/VariableInfo.inf
  INF Silicon/NVIDIA/Application/HiiKeywordUtil/HiiKeywordUtil.inf

!ifdef CONFIG_FIRMWARE_CAPSULE_SUPPORTED
  #
  # Capsule application (EFI Services)
  #
  INF MdeModulePkg/Application/CapsuleApp/CapsuleApp.inf
!endif
!ifdef CONFIG_RUNTIME_FIRMWARE_CAPSULE_SUPPORTED
  INF Silicon/NVIDIA/Drivers/NvidiaCapsuleRuntimeDxe/NvidiaCapsuleRuntimeDxe.inf
!endif

  #
  # Test Applications
  #
!ifdef CONFIG_NVIDIA_SHELL_APPLICATIONS
  INF Silicon/NVIDIA/Application/StackCheck/StackCheck.inf

!ifdef CONFIG_IPMI_BMC
  INF Silicon/NVIDIA/Application/IpmiUtility/IpmiUtility.inf
!endif

!if $(TARGET) != RELEASE
  INF Silicon/NVIDIA/Test/ShellTest/BootOrderTest/BootOrderTestUefiShell.inf
  INF Silicon/NVIDIA/Test/ShellTest/EfiGopTest/EfiGopTestUefiShell.inf
  INF Silicon/NVIDIA/Test/ShellTest/UefiMemMapTest/UefiMemMapTestUefiShell.inf
!endif

  INF Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf
  INF Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  INF Silicon/NVIDIA/Application/GpioUtil/GpioUtil.inf
  INF Silicon/NVIDIA/Application/GicUtil/GicUtil.inf
  INF Silicon/NVIDIA/Application/PcieUtil/PcieUtil.inf
  INF Silicon/NVIDIA/Application/TimerUtil/TimerUtil.inf
!ifdef CONFIG_SOC_USB
  INF Silicon/NVIDIA/Application/FalconUtil/FalconUtil.inf
!endif
  #
  # Inject Assert
  #
!ifdef CONFIG_SOC_DATACENTER
  INF Silicon/NVIDIA/Application/TestInjectAssert/TestInjectAssert.inf
!endif
!endif

!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  INF SecurityPkg/EnrollFromDefaultKeysApp/EnrollFromDefaultKeysApp.inf
!endif

  #
  # SMBIOS/DMI
  #
!ifdef CONFIG_SMBIOS
  INF MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
!endif
!ifdef CONFIG_SMBIOS_ARM
  INF ArmPkg/Universal/Smbios/SmbiosMiscDxe/SmbiosMiscDxe.inf
  INF Silicon/NVIDIA/Drivers/SmbiosOemDxe/SmbiosMiscOemDxe/SmbiosMiscOemDxe.inf
  INF ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClassDxe.inf
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
  # Custom HII Support
  #
  INF Silicon/NVIDIA/Drivers/NvidiaConfigDxe/NvidiaConfigDxe.inf

!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  #
  # Secure Boot Support
  #
  INF SecurityPkg/VariableAuthenticated/SecureBootConfigDxe/SecureBootConfigDxe.inf
!endif

!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT_BMC_EVENT_REPORT
  #
  # Secure Boot Event Reporting.
  #
  INF Silicon/NVIDIA/Drivers/SecureBootBmcEvent/SecureBootBmcEvent.inf
!endif

  #
  # Security drivers
  #
  INF SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  INF SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf
  INF SecurityPkg/Pkcs7Verify/Pkcs7VerifyDxe/Pkcs7VerifyDxe.inf
!endif

!ifdef CONFIG_RCM_BOOT
  #
  # RCM Support
  #
  INF Silicon/NVIDIA/Drivers/RcmDxe/RcmDxe.inf
!endif

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

!ifdef CONFIG_VARIABLE_NON_SECURE_SUPPORTED
!ifndef CONFIG_VARIABLE_EMULATED
  #
  # FVB Support
  #
  INF Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
  INF Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashDxe.inf
  INF MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf
!endif
!endif

  #
  # Capsule Update Policy Protocol
  #
  INF Silicon/NVIDIA/Drivers/CapsuleUpdatePolicyDxe/CapsuleUpdatePolicyDxe.inf

  #
  # Tegra EEPROM Support
  #
  INF Silicon/NVIDIA/Drivers/EepromDxe/Eeprom.inf

  #
  # Tegra Pin Control support
  #
  INF Silicon/NVIDIA/Drivers/TegraPinControlDxe/TegraPinControlDxe.inf

  #
  # Boot Chain Protocol
  #
  INF Silicon/NVIDIA/Drivers/BootChainDxe/BootChainDxe.inf

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

!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  #
  # Secure Boot Provision
  #
  INF Silicon/NVIDIA/Drivers/NvSecureBootProvisionDxe/NvSecureBootProvisionDxe.inf
!endif

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
!ifdef CONFIG_L4T_LAUNCHER_IN_FV
  INF Silicon/NVIDIA/Application/L4TLauncher/L4TLauncher.inf
!endif
!ifdef CONFIG_L4T
  INF Silicon/NVIDIA/Drivers/L4TLauncherSupportDxe/L4TLauncherSupportDxe.inf
!endif

  # WFI App
!ifdef CONFIG_DEADLOOP_APP_SUPPORT
  INF Silicon/NVIDIA/Application/WfiApp/WfiApp.inf
!endif

  # Boot support for mkbootimg partitions
!ifdef CONFIG_BOOT_NATIVE_ANDROID_PARTITION_SUPPORT
  INF Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf
!endif

  #
  # Android Fastboot
  #
!ifdef CONFIG_BOOT_ANDROID_FASTBOOT
  INF Silicon/NVIDIA/Drivers/TegraUsbDeviceControllerDxe/TegraUsbDeviceControllerDxe.inf
  INF EmbeddedPkg/Application/AndroidFastboot/AndroidFastbootApp.inf
  INF EmbeddedPkg/Drivers/AndroidFastbootTransportUsbDxe/FastbootTransportUsbDxe.inf
  INF Silicon/NVIDIA/Drivers/TegraFastBootDxe/TegraFastBootDxe.inf
!endif

  #
  # Gpio Power Off Driver
  #
  INF Silicon/NVIDIA/Drivers/GpioPowerOffDxe/GpioPowerOffDxe.inf

!ifdef CONFIG_FIRMWARE_MANAGEMENT_EROT
  #
  # NS ERoT
  #
  INF Silicon/NVIDIA/Drivers/ErotQspiDxe/ErotQspiDxe.inf

  #
  # MM MCTP
  #
  INF Silicon/NVIDIA/Drivers/MctpMmDxe/MctpMmDxe.inf
!endif

!ifdef CONFIG_REDFISH
  #
  # Redfish Host Interface Support
  #
  INF RedfishPkg/RedfishHostInterfaceDxe/RedfishHostInterfaceDxe.inf
  INF Silicon/NVIDIA/Drivers/RedfishHostInterfaceControlDxe/RedfishHostInterfaceControlDxe.inf

  #
  # Default TLS Cipher Suites
  #
  INF Silicon/NVIDIA/Drivers/RedfishCipherDxe/RedfishCipherDxe.inf

  #
  # Redfish support
  #
  INF RedfishPkg/RestJsonStructureDxe/RestJsonStructureDxe.inf
  INF RedfishPkg/RedfishRestExDxe/RedfishRestExDxe.inf
  INF RedfishPkg/RedfishCredentialDxe/RedfishCredentialDxe.inf
  INF RedfishPkg/RedfishDiscoverDxe/RedfishDiscoverDxe.inf
  INF RedfishPkg/RedfishConfigHandler/RedfishConfigHandlerDriver.inf
  INF RedfishPkg/RedfishPlatformConfigDxe/RedfishPlatformConfigDxe.inf
  INF RedfishPkg/RedfishHttpDxe/RedfishHttpDxe.inf
  INF MdeModulePkg/Universal/RegularExpressionDxe/RegularExpressionDxe.inf
  INF RedfishClientPkg/RedfishFeatureCoreDxe/RedfishFeatureCoreDxe.inf
  INF RedfishClientPkg/RedfishETagDxe/RedfishETagDxe.inf
  INF RedfishClientPkg/RedfishConfigLangMapDxe/RedfishConfigLangMapDxe.inf
  INF RedfishClientPkg/Features/ComputerSystem/v1_17_0/Dxe/ComputerSystemDxe.inf
  INF RedfishClientPkg/Features/ComputerSystem/v1_22_0/Dxe/ComputerSystemDxe.inf
  INF RedfishClientPkg/Features/ComputerSystemCollectionDxe/ComputerSystemCollectionDxe.inf
  INF RedfishClientPkg/Features/Bios/v1_2_0/Dxe/BiosDxe.inf
  INF RedfishClientPkg/Features/BootOptionCollection/BootOptionCollectionDxe.inf
  INF RedfishClientPkg/Features/BootOption/v1_0_4/Dxe/BootOptionDxe.inf
  INF RedfishClientPkg/Converter/ComputerSystem/v1_17_0/RedfishComputerSystem_V1_17_0_Dxe.inf
  INF RedfishClientPkg/Converter/ComputerSystem/v1_22_0/RedfishComputerSystem_V1_22_0_Dxe.inf
  INF RedfishClientPkg/Converter/ComputerSystemCollection/RedfishComputerSystemCollection_Dxe.inf
  INF RedfishClientPkg/Converter/SecureBoot/v1_1_0/RedfishSecureBoot_V1_1_0_Dxe.inf
  INF RedfishClientPkg/Converter/Bios/v1_2_0/RedfishBios_V1_2_0_Dxe.inf
  INF RedfishClientPkg/Converter/BootOptionCollection/RedfishBootOptionCollection_Dxe.inf
  INF RedfishClientPkg/Converter/BootOption/v1_0_4/RedfishBootOption_V1_0_4_Dxe.inf
  INF RedfishClientPkg/RedfishBootstrapAccountDxe/RedfishBootstrapAccountDxe.inf
  INF Silicon/NVIDIA/Drivers/PlatformRedfishBiosDxe/PlatformRedfishBiosDxe.inf
  INF Silicon/NVIDIA/Drivers/RedfishFirmwareInfoDxe/RedfishFirmwareInfoDxe.inf
  INF RedfishClientPkg/Features/SecureBoot/v1_1_0/Dxe/SecureBootDxe.inf
  INF Silicon/NVIDIA/Drivers/PlatformRedfishBootDxe/PlatformRedfishBootDxe.inf
  INF Silicon/NVIDIA/Drivers/PlatformRedfishOverrideDxe/PlatformRedfishOverrideDxe.inf
  INF Silicon/NVIDIA/Drivers/RedfishChassisInfoCollectorDxe/RedfishChassisInfoCollectorDxe.inf
  INF RedfishClientPkg/Features/SecureBootDatabaseCollectionDxe/SecureBootDatabaseCollectionDxe.inf
  INF RedfishClientPkg/Converter/SecureBootDatabaseCollection/RedfishSecureBootDatabaseCollection_Dxe.inf
  INF RedfishClientPkg/Features/SecureBootDatabase/v1_0_1/Dxe/SecureBootDatabaseDxe.inf
  INF RedfishClientPkg/Converter/SecureBootDatabase/v1_0_1/RedfishSecureBootDatabase_V1_0_1_Dxe.inf
  INF RedfishClientPkg/RedfishTaskServiceDxe/RedfishTaskServiceDxe.inf
  INF RedfishClientPkg/RedfishSecureBootKeysDxe/RedfishSecureBootKeysDxe.inf
!endif

  #
  # MM Communication
  #
!ifdef CONFIG_SECURITY_MM_FFA
  INF Silicon/NVIDIA/Drivers/MmCommunicateFfaDxe/MmCommunicationFfa.inf
!endif
!ifdef CONFIG_SECURITY_MM_OPTEE
  INF Silicon/NVIDIA/Drivers/MmCommunicationOpteeDxe/MmCommunicationOptee.inf
!endif

  #
  # TPM
  #
!ifdef CONFIG_SECURITY_TPM_SUPPORT
!ifdef CONFIG_SECURITY_TPM_DISCRETE
  INF Silicon/NVIDIA/Drivers/Tpm2Dxe/Tpm2Dxe.inf
  INF MdeModulePkg/Universal/SmbiosMeasurementDxe/SmbiosMeasurementDxe.inf
!endif
  INF SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf
  INF Silicon/NVIDIA/Drivers/Tcg2Config/Tcg2ConfigDxe.inf
!endif

!ifdef CONFIG_SECURITY_UEFI_MENU_USER_AUTH
  INF Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationDxe.inf
!endif

  #
  # Server Power Control Driver
  #
!ifdef CONFIG_SERVER_POWER_CONTROLLERS
  INF Silicon/NVIDIA/Drivers/ServerPowerControlDxe/ServerPowerControlDxe.inf
!endif

  #
  # VIRTIO support
  #
!ifdef CONFIG_VIRTIO
  INF Silicon/NVIDIA/Drivers/VirtioMmioDxe/VirtioMmioDxe.inf
  INF OvmfPkg/VirtioPciDeviceDxe/VirtioPciDeviceDxe.inf
  INF OvmfPkg/Virtio10Dxe/Virtio10.inf
!ifdef CONFIG_NETWORKING_DEVICE_VIRTIO
  INF OvmfPkg/VirtioNetDxe/VirtioNet.inf
!endif
!endif

  #
  # Infineon TPM FW Update Driver
  #
!ifdef CONFIG_INFINEON_TPM_FW_UPDATE
  INF IFXTPMUpdate_Driver/Src/TPMToolsUEFIPkg/IFXTPMUpdate.inf
!endif

!ifdef CONFIG_PRM_PACKAGE_SUPPORT
  #
  # PRM package support
  #
  INF PrmPkg/PrmConfigDxe/PrmConfigDxe.inf
  INF PrmPkg/PrmLoaderDxe/PrmLoaderDxe.inf
  INF PrmPkg/PrmSsdtInstallDxe/PrmSsdtInstallDxe.inf
!ifdef CONFIG_PRM_RAS_CPER_MODULE
  INF Silicon/NVIDIA/Drivers/PrmRasModule/PrmRasModule.inf
!endif
!endif

  #
  # SMMUv3 support
  #
!ifdef CONFIG_SMMU_V3
  INF Silicon/NVIDIA/Drivers/SmmuV3Dxe/SmmuV3Dxe.inf
  INF Silicon/NVIDIA/Drivers/SmmuDriverDxe/SmmuDriverDxe.inf
!endif
