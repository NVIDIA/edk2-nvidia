#
#  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

[Defines]
  SKUID_IDENTIFIER            = ALL
  PLATFORM_VERSION            = 0.1
  DSC_SPECIFICATION           = 0x00010005
  SUPPORTED_ARCHITECTURES     = AARCH64
  BUILD_TARGETS               = DEBUG|RELEASE|NOOPT

  #
  # Define ESRT GUIDs for Firmware Management Protocol instances
  #
  DEFINE SYSTEM_FMP_ESRT_GUID = 7C374309-1649-4682-8BEE-04F3A8399414

  #
  # Define Image Type ID GUIDs for Firmware Management Protocol
  #
  DEFINE FMP_SYSTEM_IMAGE_TYPE_ID_GUID_T194 = be3f5d68-7654-4ed2-838c-2a2faf901a78
  DEFINE FMP_SYSTEM_IMAGE_TYPE_ID_GUID_T234 = bf0d4599-20d4-414e-b2c5-3595b1cda402

!include Features/Ext4Pkg/Ext4.dsc.inc
!include Silicon/NVIDIA/Drivers/NvGopDriver/NvGopDriver.dsc.inc
!include Platform/NVIDIA/NVIDIA.common.dsc.inc

[LibraryClasses.common]
  CacheMaintenanceLib|Silicon/NVIDIA/Library/TegraCacheMaintenanceLib/TegraCacheMaintenanceLib.inf

  TegraCombinedSerialPortLib|Silicon/NVIDIA/Library/TegraCombinedSerialPort/TegraCombinedSerialPortLib.inf
  Tegra16550SerialPortLib|Silicon/NVIDIA/Library/Tegra16550SerialPortLib/Tegra16550SerialPortLib.inf
  TegraSbsaSerialPortLib|Silicon/NVIDIA/Library/TegraSbsaSerialPortLib/TegraSbsaSerialPortLib.inf
  SerialPortLib|Silicon/NVIDIA/Library/TegraSerialPortLib/TegraSerialPortLib.inf

  RealTimeClockLib|Silicon/NVIDIA/Library/MaximRealTimeClockLib/MaximRealTimeClockLib.inf
  OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf
  FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf

  SsdtPcieSupportLib|DynamicTablesPkg/Library/Common/SsdtPcieSupportLib/SsdtPcieSupportLib.inf
  FmpDeviceLib|Silicon/NVIDIA/Library/FmpDeviceLib/FmpDeviceLib.inf

[LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER]
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf

  AndroidBootImgLib|EmbeddedPkg/Library/AndroidBootImgLib/AndroidBootImgLib.inf

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsPatchableInModule]
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceImageTypeIdGuid|{GUID("$(SYSTEM_FMP_ESRT_GUID)")}
  gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleOnDiskSupport|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleInRamSupport|TRUE

[PcdsDynamicDefault.common]
  # Console Resolution
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|0
  # SMBIOS
  ## Type 2 Table (Board Information)
  gNVIDIATokenSpaceGuid.PcdBoardChassisLocation|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdBoardVersion|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdBoardManufacturer|L"Unknown"
  ## Type 1 Table (System Information)
  gNVIDIATokenSpaceGuid.PcdSystemSerialNum|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdSystemManufacturer|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdSystemSku|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdSystemFamilyType|L"Unknown"
  ## Type 3 Table (Chassis Information)
  gNVIDIATokenSpaceGuid.PcdType3Info|{ 0 }
  gNVIDIATokenSpaceGuid.PcdType3Info.BootupState|ChassisStateUnknown
  gNVIDIATokenSpaceGuid.PcdType3Info.PowerSupplyState|ChassisStateUnknown
  gNVIDIATokenSpaceGuid.PcdType3Info.ThermalState|ChassisStateUnknown
  gNVIDIATokenSpaceGuid.PcdType3Info.SecurityStatus|ChassisSecurityStatusUnknown
  gNVIDIATokenSpaceGuid.PcdType3Info.Height|1
  gNVIDIATokenSpaceGuid.PcdType3Info.NumberofPowerCords|1
  gNVIDIATokenSpaceGuid.PcdType3Info.Type|1
  gNVIDIATokenSpaceGuid.PcdChassisSku|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdChassisManufacturer|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdChassisSerialNumber|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdChassisAssetTag|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdChassisVersion|L"Unknown"
  ##Type 32 (Boot Information)
  gNVIDIATokenSpaceGuid.PcdType32Info|{ 0 }
  gNVIDIATokenSpaceGuid.PcdType32Info.BootStatus|BootInformationStatusNoError

[PcdsDynamicDefault.common.T194]
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x03881000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x03882000
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TEGRA194'
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID("$(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_T194)")}
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000
  gNVIDIATokenSpaceGuid.PcdSdhciCoherentDMADisable|TRUE
  gNVIDIATokenSpaceGuid.PcdBPMPPCIeControllerEnable|TRUE
  gNVIDIATokenSpaceGuid.PcdBrBctLogicalSlotSize|0x4000
  gNVIDIATokenSpaceGuid.PcdBrBctDataSize|0x1000
  gNVIDIATokenSpaceGuid.PcdAffinityMpIdrSupported|FALSE

[PcdsDynamicHii.common.T194]
  gNVIDIATokenSpaceGuid.PcdPcieResourceConfigNeeded|L"PCIeResourceConfig"|gNVIDIATokenSpaceGuid|0x0|1|BS
  gNVIDIATokenSpaceGuid.PcdPcieEntryInAcpiConfigNeeded|L"PCIeInOsConfig"|gNVIDIATokenSpaceGuid|0x0|1|BS

[PcdsDynamicDefault.common.T234]
  gArmTokenSpaceGuid.PcdGicDistributorBase|0X0F400000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0X0F440000
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TEGRA234'
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID("$(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_T234)")}
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000
  gNVIDIATokenSpaceGuid.PcdSdhciCoherentDMADisable|TRUE
  gNVIDIATokenSpaceGuid.PcdBPMPPCIeControllerEnable|TRUE
  gNVIDIATokenSpaceGuid.PcdBrBctLogicalSlotSize|0x10000
  gNVIDIATokenSpaceGuid.PcdBrBctDataSize|0x2000
  gNVIDIATokenSpaceGuid.PcdAffinityMpIdrSupported|TRUE

[PcdsDynamicHii.common.T234SLT]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|2

[PcdsDynamicHii.common.T234Presil]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|2

[PcdsDynamicDefault.common.T234Presil]
  gNVIDIATokenSpaceGuid.PcdVirtualRTC|TRUE
  gNVIDIATokenSpaceGuid.PcdTegraStmmEnabled|FALSE

[PcdsFeatureFlag.common]
  gEmbeddedTokenSpaceGuid.PcdAndroidBootLoadFile2|TRUE

[PcdsFixedAtBuild]
  gNVIDIATokenSpaceGuid.PcdFramebufferHorizontalResolution|3840
  gNVIDIATokenSpaceGuid.PcdFramebufferVerticalResolution|2160
  gNVIDIATokenSpaceGuid.PcdFramebufferBarIndex|1
  gNVIDIATokenSpaceGuid.PcdL4TConfigurationSupport|TRUE

################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform
#
################################################################################
[Components.common]
  #
  # Tegra Platform Configuration
  #
  Silicon/NVIDIA/Drivers/TegraPlatformInit/TegraPlatformInitDxe.inf

  #
  # Tegra UART driver
  #
  Silicon/NVIDIA/Drivers/TegraUartDxe/TegraUartDxe.inf

  #
  # Platform Configuration Manager Data
  #
  Silicon/NVIDIA/Tegra/T194/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf {
    <LibraryClasses>
      PlatformAslTablesLib|Silicon/NVIDIA/Tegra/T194/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -I$(BIN_DIR)/Silicon/NVIDIA/Tegra/T194/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib/OUTPUT -I$(BIN_DIR)/Silicon/NVIDIA/Library/ASLTemplateTablesLib/ASLTemplateTablesLib/OUTPUT
  }
  Silicon/NVIDIA/Tegra/T234/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf {
    <LibraryClasses>
      PlatformAslTablesLib|Silicon/NVIDIA/Tegra/T234/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -I$(BIN_DIR)/Silicon/NVIDIA/Tegra/T234/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib/OUTPUT -I$(BIN_DIR)/Silicon/NVIDIA/Library/ASLTemplateTablesLib/ASLTemplateTablesLib/OUTPUT
  }

  #
  # Platform USB FW
  #
  Silicon/NVIDIA/Tegra/T194/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf
  Silicon/NVIDIA/Tegra/T234/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf

  #
  # BPMP-FW IPC protocol
  #
  Silicon/NVIDIA/Drivers/BpmpIpc/BpmpIpcDxe.inf
  Silicon/NVIDIA/Drivers/BpmpScmi/BpmpScmiDxe.inf

  Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.inf

  #
  # Regulator support
  #
  Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf
  Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf

  #
  # FDT Support
  #
  EmbeddedPkg/Drivers/DtPlatformDxe/DtPlatformDxe.inf {
    <LibraryClasses>
      DtPlatformDtbLoaderLib|Silicon/NVIDIA/Library/DxeDtPlatformDtbLoaderLib/DxeDtPlatformDtbKernelLoaderLib.inf
  }

  #
  # ACPI Support
  #
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf {
    <LibraryClasses>
      NULL|EmbeddedPkg/Library/PlatformHasAcpiLib/PlatformHasAcpiLib.inf
  }

  # Boot support for mkbootimg partitions
  Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf

  #
  # QSPI Support
  #
  Silicon/NVIDIA/Drivers/QspiControllerDxe/QspiControllerDxe.inf

  #
  # NOR Flash Support
  #
  Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashDxe.inf

  #
  # EMMC/SD
  #
  Silicon/NVIDIA/Drivers/SdMmcControllerDxe/SdMmcControllerDxe.inf
  MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf
  MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf
  MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf

  #
  # SE RNG
  #
  Silicon/NVIDIA/Tegra/T194/Drivers/SeRngDxe/SeRngDxe.inf
  Silicon/NVIDIA/Tegra/T234/Drivers/SeRngDxe/SeRngDxe.inf

  #
  # PCI Support
  #
  ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
  Silicon/NVIDIA/Drivers/TegraP2UDxe/TegraP2UDxe.inf
  Silicon/NVIDIA/Drivers/PcieControllerDxe/PcieControllerDxe.inf

  # FVB Support
  #
  Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
  Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashDxe.inf
  MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf {
    <LibraryClasses>
      NULL|EmbeddedPkg/Library/NvVarStoreFormattedLib/NvVarStoreFormattedLib.inf
  }

  #
  # Usb Support
  #
  Silicon/NVIDIA/Drivers/EFuseDxe/EFuseDxe.inf
  Silicon/NVIDIA/Drivers/PinMuxDxe/PinMuxDxe.inf
  Silicon/NVIDIA/Drivers/UsbPadCtlDxe/UsbPadCtlDxe.inf
  Silicon/NVIDIA/Drivers/XhciControllerDxe/XhciControllerDxe.inf
  Silicon/NVIDIA/Drivers/XusbControllerDxe/XusbControllerDxe.inf
  Silicon/NVIDIA/Application/FalconUtil/FalconUtil.inf

  #
  # Tegra Pwm clock driver
  #
  Silicon/NVIDIA/Drivers/TegraPwmDxe/TegraPwmDxe.inf

  #Security drivers
  SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
  SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf
  SecurityPkg/Pkcs7Verify/Pkcs7VerifyDxe/Pkcs7VerifyDxe.inf

  #
  # EQOS NIC Support
  #
  Silicon/NVIDIA/Drivers/EqosDeviceDxe/EqosDeviceDxe.inf

  #
  # VPR Support
  #
  Silicon/NVIDIA/Drivers/VPRDxe/VPRDxe.inf

  #
  # Golden Register Support
  #
  Silicon/NVIDIA/Drivers/GoldenRegisterDxe/GoldenRegisterDxe.inf

  #
  # FSI Support
  #
  Silicon/NVIDIA/Drivers/FSIDxe/FSIDxe.inf

  #
  # L4T launcher
  #
  Silicon/NVIDIA/Application/L4TLauncher/L4TLauncher.inf

  #
  # UFS support
  #
  Silicon/NVIDIA/Drivers/UfsControllerDxe/UfsControllerDxe.inf
  MdeModulePkg/Bus/Pci/UfsPciHcDxe/UfsPciHcDxe.inf
  MdeModulePkg/Bus/Ufs/UfsPassThruDxe/UfsPassThruDxe.inf {
      <LibraryClasses>
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }

  #
  # NV Non-Discoverable PCIe Device DXE
  #
  Silicon/NVIDIA/Drivers/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf

  #
  # NV Display Controller Driver
  #
  Silicon/NVIDIA/Drivers/NvDisplayControllerDxe/NvDisplayControllerDxe.inf

  #
  # T194 Display support
  #
  Silicon/NVIDIA/Tegra/T194/Drivers/T194GraphicsOutputDxe/T194GraphicsOutputDxe.inf {
      <LibraryClasses>
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }

  #
  # Default Variables Device Tree
  #
  Silicon/NVIDIA/Tegra/DeviceTree/DeviceTree.inf

  #
  # MM Communication
  #
  Silicon/NVIDIA/Drivers/MmCommunicationOpteeDxe/MmCommunicationOptee.inf {
    <LibraryClasses>
      NULL|StandaloneMmPkg/Library/VariableMmDependency/VariableMmDependency.inf
      NULL|Silicon/NVIDIA/Library/StandaloneMmUnavailableLib/StandaloneMmUnavailableLib.inf
  }

  #
  # FW Partition and FW Image
  #
  Silicon/NVIDIA/Drivers/FwPartitionNorFlashDxe/FwPartitionNorFlashDxe.inf
  Silicon/NVIDIA/Drivers/FwPartitionBlockIoDxe/FwPartitionBlockIoDxe.inf
  Silicon/NVIDIA/Drivers/FwImageDxe/FwImageDxe.inf
  Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionMmDxe.inf
