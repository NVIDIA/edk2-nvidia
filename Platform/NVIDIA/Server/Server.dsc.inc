#
#  Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

[Defines]
  SKUID_IDENTIFIER               = ALL
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  CONFIG_ARM_WATCHDOG            = y

  #
  # Define ESRT GUIDs for Firmware Management Protocol instances
  #
  DEFINE SYSTEM_FMP_ESRT_GUID    = 92597B71-0ABE-48F3-9F06-93D3E8CD7EDF

  #
  # Define Image Type ID GUIDs for Firmware Management Protocol
  #
  DEFINE FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500_NVIDIA = 8655e5cf-297b-4213-84d5-b6817203a432
  DEFINE FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500        = $(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500_NVIDIA)

[SkuIds]
  0|DEFAULT
  1|TH500
  255|TH500Presil|TH500

!include Platform/NVIDIA/NVIDIA.common.dsc.inc

[LibraryClasses.common]

  RealTimeClockLib|Silicon/NVIDIA/Library/NuvotonRealTimeClockLib/NuvotonRealTimeClockLib.inf

  SsdtPcieSupportLib|Silicon/NVIDIA/Server/TH500/Library/SsdtPcieSupportLib/SsdtPcieSupportLib.inf
  FmpDeviceLib|Silicon/NVIDIA/Library/FmpErotLib/FmpErotLib.inf

  ErotQspiLib|Silicon/NVIDIA/Library/ErotQspiLib/ErotQspiLib.inf
  PldmBaseLib|Silicon/NVIDIA/Library/PldmBaseLib/PldmBaseLib.inf
  PldmFwUpdateLib|Silicon/NVIDIA/Library/PldmFwUpdateLib/PldmFwUpdateLib.inf
  PldmFwUpdatePkgLib|Silicon/NVIDIA/Library/PldmFwUpdatePkgLib/PldmFwUpdatePkgLib.inf
  PldmFwUpdateTaskLib|Silicon/NVIDIA/Library/PldmFwUpdateTaskLib/PldmFwUpdateTaskLib.inf
  ErotLib|Silicon/NVIDIA/Library/ErotLib/ErotLib.inf

  # TPM
  Tpm2DeviceLib|SecurityPkg/Library/Tpm2DeviceLibTcg2/Tpm2DeviceLibTcg2.inf
  Tpm2CommandLib|SecurityPkg/Library/Tpm2CommandLib/Tpm2CommandLib.inf
  Tcg2PhysicalPresenceLib|Silicon/NVIDIA/Library/DxeTcg2PhysicalPresenceLib/DxeTcg2PhysicalPresenceLib.inf
  Tcg2PpVendorLib|SecurityPkg/Library/Tcg2PpVendorLibNull/Tcg2PpVendorLibNull.inf
  TpmPlatformHierarchyLib|SecurityPkg/Library/PeiDxeTpmPlatformHierarchyLib/PeiDxeTpmPlatformHierarchyLib.inf

  # User Authentication for BIOS Setup Menu
  PlatformPasswordLib|Silicon/NVIDIA/Library/PlatformPasswordLib/PlatformPasswordLib.inf

  # Configurable DebugLib
  DebugPrintErrorLevelLib|Silicon/NVIDIA/Library/DebugPrintErrorLevelLib/DebugPrintErrorLevelLib.inf

  #
  # Redfish support
  #
  RestExLib|RedfishPkg/Library/DxeRestExLib/DxeRestExLib.inf
  Ucs2Utf8Lib|RedfishPkg/Library/BaseUcs2Utf8Lib/BaseUcs2Utf8Lib.inf
  BaseSortLib|MdeModulePkg/Library/BaseSortLib/BaseSortLib.inf
  RedfishCrtLib|RedfishPkg/PrivateLibrary/RedfishCrtLib/RedfishCrtLib.inf
  JsonLib|RedfishPkg/Library/JsonLib/JsonLib.inf
  RedfishLib|RedfishPkg/PrivateLibrary/RedfishLib/RedfishLib.inf
  HiiUtilityLib|RedfishPkg/Library/HiiUtilityLib/HiiUtilityLib.inf
  RedfishPlatformConfigLib|RedfishPkg/Library/RedfishPlatformConfigLib/RedfishPlatformConfigLib.inf
  RedfishDebugLib|RedfishClientPkg/Library/RedfishDebugLib/RedfishDebugLib.inf
  RedfishHttpCacheLib|RedfishClientPkg/Library/RedfishHttpCacheLib/RedfishHttpCacheLib.inf
  RedfishAddendumLib|RedfishClientPkg/Library/RedfishAddendumLib/RedfishAddendumLib.inf

  RedfishLib|RedfishClientPkg/PrivateLibrary/RedfishLib/RedfishLib.inf
  RedfishFeatureUtilityLib|RedfishClientPkg/Library/RedfishFeatureUtilityLib/RedfishFeatureUtilityLib.inf
  RedfishContentCodingLib|RedfishPkg/Library/RedfishContentCodingLibNull/RedfishContentCodingLibNull.inf
  ConverterCommonLib|RedfishClientPkg/ConverterLib/edk2library/ConverterCommonLib/ConverterCommonLib.inf
  RedfishResourceIdentifyLib|RedfishClientPkg/Library/RedfishResourceIdentifyLibNull/RedfishResourceIdentifyLibNull.inf
  EdkIIRedfishResourceConfigLib|RedfishClientPkg/Library/EdkIIRedfishResourceConfigLib/EdkIIRedfishResourceConfigLib.inf
  RedfishEventLib|RedfishClientPkg/Library/RedfishEventLib/RedfishEventLib.inf
  RedfishVersionLib|RedfishClientPkg/Library/RedfishVersionLib/RedfishVersionLib.inf
  RedfishPlatformCredentialLib|Silicon/NVIDIA/Library/RedfishPlatformCredentialLib/RedfishPlatformCredentialLib.inf
  ComputerSystemV1_17_0Lib|RedfishClientPkg/ConverterLib/edk2library/ComputerSystem/v1_17_0/Lib.inf
  ComputerSystemCollectionLib|RedfishClientPkg/ConverterLib/edk2library/ComputerSystemCollection/Lib.inf
  BiosV1_2_0Lib|RedfishClientPkg/ConverterLib/edk2library/Bios/v1_2_0/Lib.inf
  SecureBootV1_1_0Lib|RedfishClientPkg/ConverterLib/edk2library/SecureBoot/v1_1_0/Lib.inf
  BootOptionCollectionLib|RedfishClientPkg/ConverterLib/edk2library/BootOptionCollection/Lib.inf
  BootOptionV1_0_4Lib|RedfishClientPkg/ConverterLib/edk2library/BootOption/v1_0_4/Lib.inf

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
  gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleOnDiskSupport|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleInRamSupport|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxSizeNonPopulateCapsule|0x1400000

[PcdsFixedAtBuild.common]
  gEfiMdeModulePkgTokenSpaceGuid.PcdStatusCodeUseSerial|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdReportStatusCodePropertyMask|0x07
  gNVIDIATokenSpaceGuid.PcdConfigMgrObjMax|0x400

[PcdsDynamicDefault]
  ## MM Communicate
  gArmTokenSpaceGuid.PcdMmBufferBase|0x8B5FE000
  gArmTokenSpaceGuid.PcdMmBufferSize|0x20000

[PcdsDynamicDefault.common.DEFAULT]
  #
  # Allow physical presence activities
  #
  gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence|TRUE

[PcdsDynamicDefault.common.TH500]
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TH500   '
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID("$(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500)")}
  gNVIDIATokenSpaceGuid.PcdNonUniqueSystemFmpCapsuleImageTypeIdGuid|{GUID("$(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500_NVIDIA)")}
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000
  gArmTokenSpaceGuid.PcdPciBusMin|0
  gArmTokenSpaceGuid.PcdPciBusMax|1
  gNVIDIATokenSpaceGuid.PcdPciConfigurationSpaceBaseAddress|0x2DA00000
  # SMBIOS for TH500
  ## Type 0 Table (BIOS Information)
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVendor|L"NVIDIA"
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
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2InitializationPolicy|0
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2AcpiTableRev|4
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2HashMask|0x00000006
  gEfiSecurityPkgTokenSpaceGuid.PcdTcg2HashAlgorithmBitmap|0x00000006
  gEfiSecurityPkgTokenSpaceGuid.PcdTcg2PhysicalPresenceFlags|0x00002
  gEfiSecurityPkgTokenSpaceGuid.PcdRandomizePlatformHierarchy|TRUE

  gNVIDIATokenSpaceGuid.PcdAffinityMpIdrSupported|TRUE
  gNVIDIATokenSpaceGuid.PcdNonSecureQspiAvailable|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"$(BUILDID_STRING)"
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareReleaseDateString|L"$(BUILD_DATE_TIME)"

[PcdsDynamicHii.common.DEFAULT]
  gNVIDIATokenSpaceGuid.PcdQuickBootEnabled|L"QuickBootEnabled"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV

[PcdsDynamicHii.common.TH500]
  gNVIDIATokenSpaceGuid.PcdEnabledPcieNicTopology|L"EnabledPcieNicTopology"|gNVIDIAPublicVariableGuid|0x0|{ 0x1, 0x8, 0xFF, 0xFF, 0xFF }|BS,NV

[PcdsDynamicHii.common.TH500Presil]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|2

[PcdsDynamicDefault.common.TH500Presil]
  gNVIDIATokenSpaceGuid.PcdVirtualRTC|TRUE

[PcdsFixedAtBuild.common]
  # HACK: Disable SR-IOV support to workaround a simulator bug
  # TODO: Remove once the simulator issue is fixed.
  gEfiMdeModulePkgTokenSpaceGuid.PcdSrIovSupport|FALSE

  gEdkiiDynamicTablesPkgTokenSpaceGuid.PcdForceTopLevelProcessorContainer|TRUE

  ## TPM
  gEfiSecurityPkgTokenSpaceGuid.PcdTpmPlatformClass|1

  #
  # Redfish support
  #
  # Use gNVIDIABeforeConsoleEventGuid to startup Redfish feature drivers.
  # { 0x974180a0, 0xa203, 0x43c8, { 0xb1, 0x12, 0x48, 0x70, 0xe1, 0xdb, 0xd6, 0x48 } }
  gEfiRedfishClientPkgTokenSpaceGuid.PcdEdkIIRedfishFeatureDriverStartupEventGuid|{0xA0, 0x80, 0x41, 0x97, 0x03, 0xA2, 0xC8, 0x43, 0xB1, 0x12, 0x48, 0x70, 0xE1, 0xDB, 0xD6, 0x48 }
  gEfiRedfishPkgTokenSpaceGuid.PcdRedfishRestExServiceAccessModeInBand|False
  gEfiRedfishPkgTokenSpaceGuid.PcdRedfishDiscoverAccessModeInBand|False
  gEfiRedfishClientPkgTokenSpaceGuid.PcdDefaultRedfishVersion|L"/v1/"

  # SBSA Watchdog
  gArmTokenSpaceGuid.PcdGenericWatchdogControlBase|0x000000000c6ab000
  gArmTokenSpaceGuid.PcdGenericWatchdogRefreshBase|0x000000000c6aa000
  gArmTokenSpaceGuid.PcdGenericWatchdogEl2IntrNum|39


[PcdsFeatureFlag]
  gArmTokenSpaceGuid.PcdFfaEnable|TRUE
  gNVIDIATokenSpaceGuid.PcdQuickBootSupported|FALSE
  gNVIDIATokenSpaceGuid.PcdDebugMenuSupport|TRUE
  gNVIDIATokenSpaceGuid.PcdUseSinglePassiveThermalZone|TRUE
  gNVIDIATokenSpaceGuid.PcdRedfishSupported|TRUE

  #
  # Redfish support
  #
  gIpmiFeaturePkgTokenSpaceGuid.PcdIpmiFeatureEnable|True

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
  # Regulator support
  #
  Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf
  Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf

  #
  # ACPI Support
  #
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  Silicon/NVIDIA/Drivers/AcpiDtbSsdtGenerator/AcpiDtbSsdtGenerator.inf

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
  # Tegra Platform Configuration
  #
  Silicon/NVIDIA/Drivers/TegraPlatformInit/TegraPlatformInitDxe.inf

  #
  # PCI Support
  #
  ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
  Silicon/NVIDIA/Drivers/PcieControllerDxe/PcieControllerDxe.inf

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
      HashLib|SecurityPkg/Library/HashLibBaseCryptoRouter/HashLibBaseCryptoRouterDxe.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha256/HashInstanceLibSha256.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha384/HashInstanceLibSha384.inf
  }
  Silicon/NVIDIA/Drivers/Tcg2Config/Tcg2ConfigDxe.inf

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
  #
  # Intel 700 series NIC
  #
  IntelUndiPkg/I40eUndiDxe/I40eUndiDxe.inf {
    <LibraryClasses>
      StdLibC|IntelUndiPkg/LibC/LibC.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -Wno-unused -Wno-multistatement-macros -Wno-incompatible-pointer-types
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

  #
  # IPMI test Application
  #
  Silicon/NVIDIA/Application/IpmiUtility/IpmiUtility.inf

  #
  # User Authentication
  #
  Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationDxe.inf

  #
  # GH100 GPU Driver
  #
  Silicon/NVIDIA/Server/TH500/Drivers/TH500GpuDxe/TH500GpuDxe.inf

  #
  # Redfish Host Interface Support
  #
  RedfishPkg/RedfishHostInterfaceDxe/RedfishHostInterfaceDxe.inf {
    <LibraryClasses>
      RedfishPlatformHostInterfaceLib|Silicon/NVIDIA/Library/RedfishPlatformHostInterfaceOemLib/RedfishPlatformHostInterfaceOemLib.inf
  }
  Silicon/NVIDIA/Drivers/RedfishHostInterfaceControlDxe/RedfishHostInterfaceControlDxe.inf {
    <LibraryClasses>
      RedfishEventLib|RedfishClientPkg/Library/RedfishEventLib/RedfishEventLib.inf
  }

  #
  # Default TLS Cipher Suites for Redfish HTTPS support
  #
  Silicon/NVIDIA/Drivers/RedfishCipherDxe/RedfishCipherDxe.inf {
    <LibraryClasses>
      RedfishEventLib|RedfishClientPkg/Library/RedfishEventLib/RedfishEventLib.inf
  }

  #
  # Redfish support
  #
  RedfishPkg/RestJsonStructureDxe/RestJsonStructureDxe.inf
  RedfishPkg/RedfishRestExDxe/RedfishRestExDxe.inf
  RedfishPkg/RedfishCredentialDxe/RedfishCredentialDxe.inf
  RedfishPkg/RedfishDiscoverDxe/RedfishDiscoverDxe.inf
  RedfishPkg/RedfishConfigHandler/RedfishConfigHandlerDriver.inf
  RedfishPkg/RedfishPlatformConfigDxe/RedfishPlatformConfigDxe.inf
  MdeModulePkg/Universal/RegularExpressionDxe/RegularExpressionDxe.inf

  RedfishClientPkg/RedfishFeatureCoreDxe/RedfishFeatureCoreDxe.inf
  RedfishClientPkg/RedfishETagDxe/RedfishETagDxe.inf
  RedfishClientPkg/RedfishConfigLangMapDxe/RedfishConfigLangMapDxe.inf
  RedfishClientPkg/Features/ComputerSystemCollectionDxe/ComputerSystemCollectionDxe.inf
  RedfishClientPkg/Features/ComputerSystem/v1_17_0/Dxe/ComputerSystemDxe.inf
  RedfishClientPkg/Features/Bios/v1_2_0/Dxe/BiosDxe.inf
  RedfishClientPkg/Features/SecureBoot/v1_1_0/Dxe/SecureBootDxe.inf
  RedfishClientPkg/Converter/ComputerSystem/v1_17_0/RedfishComputerSystem_V1_17_0_Dxe.inf
  RedfishClientPkg/Converter/ComputerSystemCollection/RedfishComputerSystemCollection_Dxe.inf
  RedfishClientPkg/Features/BootOptionCollection/BootOptionCollectionDxe.inf
  RedfishClientPkg/Features/BootOption/v1_0_4/Dxe/BootOptionDxe.inf
  RedfishClientPkg/Converter/Bios/v1_2_0/RedfishBios_V1_2_0_Dxe.inf
  RedfishClientPkg/Converter/BootOptionCollection/RedfishBootOptionCollection_Dxe.inf
  RedfishClientPkg/Converter/BootOption/v1_0_4/RedfishBootOption_V1_0_4_Dxe.inf
  RedfishClientPkg/Converter/SecureBoot/v1_1_0/RedfishSecureBoot_V1_1_0_Dxe.inf
  RedfishClientPkg/RedfishBootstrapAccountDxe/RedfishBootstrapAccountDxe.inf
  Silicon/NVIDIA/Drivers/PlatformRedfishBiosDxe/PlatformRedfishBiosDxe.inf
  Silicon/NVIDIA/Drivers/RedfishFirmwareInfoDxe/RedfishFirmwareInfoDxe.inf
  Silicon/NVIDIA/Drivers/PlatformRedfishBootDxe/PlatformRedfishBootDxe.inf
  Silicon/NVIDIA/Drivers/PlatformRedfishOverrideDxe/PlatformRedfishOverrideDxe.inf

  #
  # FPGA I2C Support
  #
  Silicon/NVIDIA/Drivers/FpgaI2c/FpgaI2c.inf
