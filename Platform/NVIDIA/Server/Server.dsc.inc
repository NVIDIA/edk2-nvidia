#
#  Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

[Defines]
  SKUID_IDENTIFIER               = ALL
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT

  #
  # Define ESRT GUIDs for Firmware Management Protocol instances
  #
  DEFINE SYSTEM_FMP_ESRT_GUID    = 92597B71-0ABE-48F3-9F06-93D3E8CD7EDF

[SkuIds]
  0|DEFAULT
  1|TH500
  255|TH500Presil|TH500

!include Platform/NVIDIA/NVIDIA.common.dsc.inc

[LibraryClasses.common]

  # PL011 Uart
  SerialPortLib|ArmPlatformPkg/Library/PL011SerialPortLib/PL011SerialPortLib.inf
  PL011UartClockLib|ArmPlatformPkg/Library/PL011UartClockLib/PL011UartClockLib.inf
  PL011UartLib|ArmPlatformPkg/Library/PL011UartLib/PL011UartLib.inf

  RealTimeClockLib|Silicon/NVIDIA/Library/NuvotonRealTimeClockLib/NuvotonRealTimeClockLib.inf

  SsdtPcieSupportLib|Silicon/NVIDIA/Server/TH500/Library/SsdtPcieSupportLib/SsdtPcieSupportLib.inf
  FmpDeviceLib|Silicon/NVIDIA/Library/FmpErotLib/FmpErotLib.inf

  ErotQspiLib|Silicon/NVIDIA/Library/ErotQspiLib/ErotQspiLib.inf
  PldmBaseLib|Silicon/NVIDIA/Library/PldmBaseLib/PldmBaseLib.inf
  PldmFwUpdateLib|Silicon/NVIDIA/Library/PldmFwUpdateLib/PldmFwUpdateLib.inf

[LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER]
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsPatchableInModule]
  # Console Resolution (Full HD)
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|1920
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|1080

  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceImageTypeIdGuid|{GUID("$(SYSTEM_FMP_ESRT_GUID)")}

[PcdsFixedAtBuild.common]
  gEfiMdeModulePkgTokenSpaceGuid.PcdStatusCodeUseSerial|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdReportStatusCodePropertyMask|0x07
  gNVIDIATokenSpaceGuid.PcdConfigMgrObjMax|0x400

[PcdsDynamicDefault]
  ## MM Communicate
  gArmTokenSpaceGuid.PcdMmBufferBase|0x8B5FE000
  gArmTokenSpaceGuid.PcdMmBufferSize|0x20000

[PcdsDynamicDefault.common.TH500]
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TH500   '
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID(8655e5cf-297b-4213-84d5-b6817203a432)}
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000
  gArmTokenSpaceGuid.PcdPciBusMin|0
  gArmTokenSpaceGuid.PcdPciBusMax|1
  gNVIDIATokenSpaceGuid.PcdPciConfigurationSpaceBaseAddress|0x2DA00000
  # SBSA Watchdog
  gArmTokenSpaceGuid.PcdGenericWatchdogControlBase|0x000000000c6a9000
  gArmTokenSpaceGuid.PcdGenericWatchdogRefreshBase|0x000000000c6a8000
  gArmTokenSpaceGuid.PcdGenericWatchdogEl2IntrNum|37
  # SMBIOS for TH500
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
  ## RTC
  gNVIDIATokenSpaceGuid.PcdCpuHasRtcControl|TRUE
  ## TPM
  gEfiSecurityPkgTokenSpaceGuid.PcdTpmInstanceGuid|{0x5a, 0xf2, 0x6b, 0x28, 0xc3, 0xc2, 0x8c, 0x40, 0xb3, 0xb4, 0x25, 0xe6, 0x75, 0x8b, 0x73, 0x17}
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2InitializationPolicy|1

  gNVIDIATokenSpaceGuid.PcdAffinityMpIdrSupported|TRUE
  gNVIDIATokenSpaceGuid.PcdNonSecureQspiAvailable|TRUE

[PcdsDynamicHii.common.TH500Presil]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|2

[PcdsDynamicDefault.common.TH500Presil]
  gNVIDIATokenSpaceGuid.PcdVirtualRTC|TRUE

[PcdsFixedAtBuild.common]
  #
  # PL011 Serial Port Library & UART Parameters
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterBase|0x0C280000

  gArmPlatformTokenSpaceGuid.PL011UartClkInHz|1843200
  gArmPlatformTokenSpaceGuid.PL011UartInteger|0
  gArmPlatformTokenSpaceGuid.PL011UartFractional|0
  gArmPlatformTokenSpaceGuid.PL011UartRegOffsetVariant|0

  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultDataBits|8
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultParity|1
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultStopBits|1
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultReceiveFifoDepth|32

  # HACK: Disable SR-IOV support to workaround a simulator bug
  # TODO: Remove once the simulator issue is fixed.
  gEfiMdeModulePkgTokenSpaceGuid.PcdSrIovSupport|FALSE

  gEdkiiDynamicTablesPkgTokenSpaceGuid.PcdForceTopLevelProcessorContainer|TRUE

[PcdsFeatureFlag]
  gArmTokenSpaceGuid.PcdFfaEnable|TRUE

################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform
#
################################################################################
[Components.common]
  #
  # Serial Console driver
  #
  MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

  #
  # IPMI Null Driver
  #
  Silicon/NVIDIA/Drivers/IpmiNullDxe/IpmiNullDxe.inf

  #
  # Platform Configuration Manager Data
  #
  Silicon/NVIDIA/Server/TH500/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf {
    <LibraryClasses>
      # Platform ASL Tables
      PlatformAslTablesLib|Silicon/NVIDIA/Server/TH500/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -I$(BIN_DIR)/Silicon/NVIDIA/Server/TH500/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib/OUTPUT
  }

  #
  # BPMP-FW IPC protocol
  #
  Silicon/NVIDIA/Drivers/BpmpIpc/BpmpIpcDxe.inf
  Silicon/NVIDIA/Drivers/BpmpScmi/BpmpScmiDxe.inf

  Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.inf

  #
  # GPIO driver/utility
  #
  Silicon/NVIDIA/Drivers/TegraGpio/TegraGpioDxe.inf
  Silicon/NVIDIA/Application/GpioUtil/GpioUtil.inf

  #
  # Regulator support
  #
  Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf
  Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf

  #
  # ACPI Support
  #
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf

  #
  # QSPI Support
  #
  Silicon/NVIDIA/Drivers/QspiControllerDxe/QspiControllerDxe.inf

  #
  # NOR Flash Support
  #
  Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashDxe.inf

  #
  # FVB Support
  #
  Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
  Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashDxe.inf
  MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf {
    <LibraryClasses>
      NULL|EmbeddedPkg/Library/NvVarStoreFormattedLib/NvVarStoreFormattedLib.inf
  }

  #
  # Fuse Support
  #
  Silicon/NVIDIA/Drivers/EFuseDxe/EFuseDxe.inf

  #
  # Tegra Platform Configuration
  #
  Silicon/NVIDIA/Drivers/TegraPlatformInit/TegraPlatformInitDxe.inf

  #
  # PCI Support
  #
  ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
  Silicon/NVIDIA/Server/TH500/Drivers/PcieControllerDxe/PcieControllerDxe.inf

  #
  # Memory Tests
  #
  MdeModulePkg/Universal/MemoryTest/GenericMemoryTestDxe/GenericMemoryTestDxe.inf

  #
  # Ramloaded OS
  #
  Silicon/NVIDIA/Drivers/RamDiskOS/RamDiskOS.inf

  #
  # APEI
  #
  Silicon/NVIDIA/Server/TH500/Drivers/ApeiDxe/Apei.inf {
    <LibraryClasses>
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }

  #
  # MM Communication
  #
  Silicon/NVIDIA/Drivers/MmCommunicateFfaDxe/MmCommunicationFfa.inf {
    <LibraryClasses>
      NULL|StandaloneMmPkg/Library/VariableMmDependency/VariableMmDependency.inf
      NULL|Silicon/NVIDIA/Library/StandaloneMmUnavailableLib/StandaloneMmUnavailableLib.inf
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }

  #
  # Report status code
  #
  Silicon/NVIDIA/Drivers/OemDescStatusCodeDxe/OemDescStatusCodeDxe.inf

  #
  # TPM
  #
  Silicon/NVIDIA/Drivers/Tpm2Dxe/Tpm2Dxe.inf
  SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf {
    <LibraryClasses>
      Tpm2DeviceLib|SecurityPkg/Library/Tpm2DeviceLibRouter/Tpm2DeviceLibRouterDxe.inf
      NULL|Silicon/NVIDIA/Library/Tpm2DeviceLib/Tpm2DeviceLib.inf
      Tpm2CommandLib|SecurityPkg/Library/Tpm2CommandLib/Tpm2CommandLib.inf
      HashLib|SecurityPkg/Library/HashLibTpm2/HashLibTpm2.inf
      Tcg2PhysicalPresenceLib|SecurityPkg/Library/DxeTcg2PhysicalPresenceLib/DxeTcg2PhysicalPresenceLib.inf
      Tcg2PpVendorLib|SecurityPkg/Library/Tcg2PpVendorLibNull/Tcg2PpVendorLibNull.inf
  }
  SecurityPkg/Tcg/Tcg2Config/Tcg2ConfigDxe.inf {
    <LibraryClasses>
      Tpm2DeviceLib|SecurityPkg/Library/Tpm2DeviceLibTcg2/Tpm2DeviceLibTcg2.inf
      Tpm2CommandLib|SecurityPkg/Library/Tpm2CommandLib/Tpm2CommandLib.inf
      Tcg2PhysicalPresenceLib|SecurityPkg/Library/DxeTcg2PhysicalPresenceLib/DxeTcg2PhysicalPresenceLib.inf
      Tcg2PpVendorLib|SecurityPkg/Library/Tcg2PpVendorLibNull/Tcg2PpVendorLibNull.inf
  }

!ifdef $(ENABLE_INTELUNDI)
  #
  # Intel 500 series NIC
  #
  IntelUndiPkg/XGigUndiDxe/XGigUndiDxe.inf {
    <LibraryClasses>
      StdLibC|IntelUndiPkg/LibC/LibC.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -Wno-unused-value -Wno-multistatement-macros
  }
!endif

  #
  # Host Interface test Application
  #
  Silicon/NVIDIA/Application/HostInterfaceUtil/HostInterfaceUtil.inf  {
    <LibraryClasses>
      RedfishPlatformCredentialLib|Silicon/NVIDIA/Library/RedfishPlatformCredentialLib/RedfishPlatformCredentialLib.inf
  }

  #
  # BMC Virtual NIC support
  #
  Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/UsbRndisDxe.inf

  #
  # Realtek USB NIC
  #
  Drivers/Realtek/Bus/Usb/UsbNetworking/RtkUsbUndiDxe.inf

  #
  # NS ERoT
  #
  Silicon/NVIDIA/Drivers/ErotQspiDxe/ErotQspiDxe.inf

  #
  # MM MCTP
  #
  Silicon/NVIDIA/Drivers/MctpMmDxe/MctpMmDxe.inf
