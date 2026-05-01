# SPDX-FileCopyrightText: Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

[Defines]
  PLATFORM_NAME                  = HostBasedTests
  PLATFORM_GUID                  = fb5efb94-61c8-42e0-9cb4-a33ba4e4aec8
  OUTPUT_DIRECTORY               = Build/HostBasedTests
  SKUID_IDENTIFIER               = ALL
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = IA32|X64|AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  FLASH_DEFINITION               = Platform/NVIDIA/HostBasedTests/HostBasedTests.fdf

!include Platform/NVIDIA/HostBasedTests/HostBasedTests.dsc.inc

[Components]

  #
  # BootChainDxe Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/BootChainDxe/UnitTest/BootChainDxeUnitTestsHost.inf {
    <LibraryClasses>
      UefiRuntimeServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiRuntimeServicesTableStubLib/UefiRuntimeServicesTableStubLib.inf
      UefiBootServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiBootServicesTableStubLib/UefiBootServicesTableStubLib.inf
      PlatformResourceLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PlatformResourceStubLib/PlatformResourceStubLib.inf
      StatusRegLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/StatusRegStubLib/StatusRegStubLib.inf
  }

  # BootConfigProtocolLib PMC RST_SOURCE -> boot reason decode
  Silicon/NVIDIA/Library/BootConfigProtocolLib/UnitTest/BootConfigProtocolLibUnitTests.inf

  #
  # FvbDxe Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/FvbDxe/UnitTest/FvbDxeUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Drivers/FvbDxe/UnitTest/FvbPcdStubLib/FvbPcdStubLib.inf
  }

  # PlatformResourceStubLib
  Silicon/NVIDIA/Library/HostBasedTestStubLib/PlatformResourceStubLib/UnitTest/PlatformResourceStubLibUnitTests.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Library/HostBasedTestStubLib/PlatformResourceStubLib/PlatformResourceStubLib.inf
  }

  # TegraPlatformInfoStubLib
  Silicon/NVIDIA/Library/HostBasedTestStubLib/TegraPlatformInfoStubLib/UnitTest/TegraPlatformInfoStubLibUnitTests.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Library/HostBasedTestStubLib/TegraPlatformInfoStubLib/TegraPlatformInfoStubLib.inf
  }

  # Nuvoton RTC library unit tests
  Silicon/NVIDIA/Library/NuvotonRealTimeClockLib/UnitTest/NuvotonRealTimeClockLibUnitTest.inf {
    <LibraryClasses>
      RealTimeClockLib|Silicon/NVIDIA/Library/NuvotonRealTimeClockLib/NuvotonRealTimeClockLib.inf
      TimerLib|MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
      TimeBaseLib|EmbeddedPkg/Library/TimeBaseLib/TimeBaseLib.inf
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
    <BuildOptions>
      GCC:*_*_*_DLINK_FLAGS = -Wl,--wrap=EfiGetVariable,--wrap=EfiSetVariable,--wrap=EfiCreateProtocolNotifyEvent,--wrap=GetPerformanceCounter,--wrap=GetTimeInNanoSecond,--wrap=EfiAtRuntime,--wrap=EfiGetSystemConfigurationTable
  }

  # IPMI Blob Transfer protocol unit tests
  Silicon/NVIDIA/Drivers/IpmiBlobTransferDxe/UnitTest/IpmiBlobTransferTestUnitTestsHost.inf {
    <LibraryClasses>
      IpmiBaseLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/IpmiStubLib/IpmiStubLib.inf
  }

  #
  # OEM Send Description
  #
  Silicon/NVIDIA/Drivers/OemDescStatusCodeDxe/UnitTest/OemDescStatusCodeDxeUnitTest.inf {
    <LibraryClasses>
      IpmiBaseLib|IpmiFeaturePkg/Library/IpmiBaseLibNull/IpmiBaseLibNull.inf
      DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
      DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
    <BuildOptions>
      GCC:*_*_*_DLINK_FLAGS = -Wl,--wrap=IpmiSubmitCommand,--wrap=GetDebugPrintErrorLevel,--wrap=EfiCreateProtocolNotifyEvent
  }

  #
  # Redfish CredentialBootstrap tests
  #
  Silicon/NVIDIA/Library/RedfishPlatformCredentialLib/UnitTest/CredentialBootstrapUnitTest.inf {
    <LibraryClasses>
      RedfishPlatformCredentialLib|Silicon/NVIDIA/Library/RedfishPlatformCredentialLib/RedfishPlatformCredentialLib.inf
      IpmiBaseLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/IpmiStubLib/IpmiStubLib.inf
      UefiRuntimeServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiRuntimeServicesTableStubLib/UefiRuntimeServicesTableStubLib.inf
  }

  # Redfish Host Interface unit tests
  Silicon/NVIDIA/Library/RedfishPlatformHostInterfaceOemLib/UnitTest/RedfishHostInterfaceUnitTest.inf {
    <LibraryClasses>
      NetLib|NetworkPkg/Library/DxeNetLib/DxeNetLib.inf
      IpmiBaseLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/IpmiStubLib/IpmiStubLib.inf
      UefiRuntimeServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiRuntimeServicesTableStubLib/UefiRuntimeServicesTableStubLib.inf
  }

  #
  # ErrorSerializationMmDxe Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/RAS/ErrorSerializationMmDxe/UnitTest/ErrorSerializationDxeUnitTestsHost.inf {
    <LibraryClasses>
      MmServicesTableLib|MdePkg/Library/StandaloneMmServicesTableLib/StandaloneMmServicesTableLib.inf
      StandaloneMmDriverEntryPoint|MdePkg/Library/StandaloneMmDriverEntryPoint/StandaloneMmDriverEntryPoint.inf
      TimerLib|MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
  }

  #
  # IPMI BootOrder tests
  #
  Silicon/NVIDIA/Library/PlatformBootOrderLib/UnitTest/IpmiBootOrderUnitTest.inf {
    <LibraryClasses>
      SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
      UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
      PlatformBootOrderLib|Silicon/NVIDIA/Library/PlatformBootOrderLib/PlatformBootOrderLib.inf
      IpmiCommandLib|ManageabilityPkg/Library/IpmiCommandLib/IpmiCommandLib.inf
      IpmiBaseLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/IpmiStubLib/IpmiStubLib.inf
      PeCoffGetEntryPointLib|EmulatorPkg/Library/PeiEmuPeCoffGetEntryPointLib/PeiEmuPeCoffGetEntryPointLib.inf
      DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
      DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
      UefiRuntimeServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiRuntimeServicesTableStubLib/UefiRuntimeServicesTableStubLib.inf
      UefiBootServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiBootServicesTableStubLib/UefiBootServicesTableStubLib.inf
      HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
      VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf
      UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
      PeiServicesTablePointerLib|EmulatorPkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf
      HobLib|MdeModulePkg/Library/BaseHobLibNull/BaseHobLibNull.inf
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
      FwVariableLib|Silicon/NVIDIA/Library/FwVariableLib/FwVariableLib.inf
  }

  #
  # AndroidBootDxe Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/AndroidBootDxe/UnitTest/AndroidBootDxeUnitTest.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
      HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
      ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
      ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
      BootConfigProtocolLib|Silicon/NVIDIA/Library/BootConfigProtocolLib/BootConfigProtocolLib.inf
      NctLib|Silicon/NVIDIA/Library/NctLib/NctLib.inf
      NetLib|NetworkPkg/Library/DxeNetLib/DxeNetLib.inf
      BaseCryptLib|CryptoPkg/Library/BaseCryptLibNull/BaseCryptLibNull.inf
      AvbLib|Silicon/NVIDIA/Library/AvbStubLib/AvbStubLib.inf
  }

  #
  # MmVarLib Host Base GoogleTest
  Silicon/NVIDIA/Library/MmVarLib/GoogleTest/MmVarLibGoogleTest.inf {
    <LibraryClasses>
      MmVarLib|Silicon/NVIDIA/Library/MmVarLib/MmVarLib.inf
      MmServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockMmStTableLib/MockMmStTableLib.inf
      SmmVarProtoLib|Silicon/NVIDIA/Test/Mock/Library/GoogleTest/MockSmmVarProto/MockSmmVarProto.inf
  }

  Silicon/NVIDIA/Library/NvVarIntLibrary/GoogleTest/NvVarIntLibGoogleTest.inf {
    <LibraryClasses>
      NvVarIntLib|Silicon/NVIDIA/Library/NvVarIntLibrary/NvVarIntLibrary.inf
      MmServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockMmStTableLib/MockMmStTableLib.inf
      MmVarLib|Silicon/NVIDIA/Test/Mock/Library/GoogleTest/MockMmVarLib/MockMmVarLib.inf
      HashApiLib|Silicon/NVIDIA/Test/Mock/Library/GoogleTest/MockHashApiLib/MockHashApiLib.inf
      SmmVarProtoLib|Silicon/NVIDIA/Test/Mock/Library/GoogleTest/MockSmmVarProto/MockSmmVarProto.inf
  }

  Silicon/NVIDIA/Library/MpCoreInfoLib/UnitTest/MpCoreInfoLibGoogleTest.inf {
    <LibraryClasses>
      MpCoreInfoLib|Silicon/NVIDIA/Library/MpCoreInfoLib/MpCoreInfoLib.inf
      HobLib|MdePkg/Test/Mock/Library/GoogleTest/MockHobLib/MockHobLib.inf
  }

  Silicon/NVIDIA/Library/DeviceTreeHelperLib/UnitTest/DeviceTreeHelperLibGoogleTest.inf {
    <LibraryClasses>
      DeviceTreeHelperLib|Silicon/NVIDIA/Library/DeviceTreeHelperLib/DeviceTreeHelperLib.inf
      FdtLib|MdePkg/Test/Mock/Library/GoogleTest/MockFdtLib/MockFdtLib.inf
      DtPlatformDtbLoaderLib|EmbeddedPkg/Test/Mock/Library/GoogleTest/MockDtPlatformDtbLoaderLib/MockDtPlatformDtbLoaderLib.inf
  }

  Silicon/NVIDIA/Library/Crc8Lib/GoogleTest/Crc8LibGoogleTest.inf {
    <LibraryClasses>
      Crc8Lib|Silicon/NVIDIA/Library/Crc8Lib/Crc8Lib.inf
  }

  Silicon/NVIDIA/Library/GptLib/GoogleTest/GptLibGoogleTest.inf {
    <LibraryClasses>
      GptLib|Silicon/NVIDIA/Library/GptLib/GptLib.inf
  }

  #
  # RamDiskOS driver Host Based GoogleTest
  #
  Silicon/NVIDIA/Drivers/RamDiskOS/GoogleTest/RamDiskOSGoogleTest.inf {
    <LibraryClasses>
     HobLib|MdePkg/Test/Mock/Library/GoogleTest/MockHobLib/MockHobLib.inf
     UefiBootServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
     RamDiskProtoLib|Silicon/NVIDIA/Test/Mock/Library/GoogleTest/MockRamDiskProto/MockRamDiskProto.inf
  }

  Silicon/NVIDIA/Library/WildcardStringLib/UnitTest/WildcardStringLibGoogleTest.inf

  Silicon/NVIDIA/Drivers/CapsuleAttemptRecordDxe/UnitTest/CapsuleAttemptRecordDxeInternalGoogleTest.inf

  #
  # Redfish HTTP Boot Config Driver GoogleTest
  #
  Silicon/NVIDIA/Drivers/RedfishHttpBootConfigDxe/GoogleTest/RedfishHttpBootConfigGoogleTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
  }

  #
  # CreateHttpBootOption GoogleTest
  #
  Silicon/NVIDIA/Drivers/RedfishHttpBootConfigDxe/GoogleTest/CreateHttpBootOptionGoogleTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  }

  #
  # CompareAndSyncBootOptions GoogleTest
  #
  Silicon/NVIDIA/Drivers/RedfishHttpBootConfigDxe/GoogleTest/CompareAndSyncBootOptionsGoogleTest.inf

  Silicon/NVIDIA/Drivers/FvbNorFlashDxe/UnitTest/VarIntUnitTestsHost.inf {
    <LibraryClasses>
     NvVarIntLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/NvVarIntStubLib/NvVarIntStubLib.inf
     StandaloneMmOpteeLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/StandaloneMmOpteeStubLib/StandaloneMmOpteeStubLib.inf
    <PcdsFixedAtBuild>
      gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x16
    <BuildOptions>
      GCC:*_*_*_DLINK_FLAGS = -Wl,--wrap=CpuDeadLoop
  }

  #
  # Boot helper library Host Based UnitTest Support
  #
  Silicon/NVIDIA/Library/BootChainInfoLib/UnitTest/BootChainInfoLibUnitTestsHost.inf {
    <LibraryClasses>
      HobLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/HobStubLib/HobStubLib.inf
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }
  Silicon/NVIDIA/Library/ProtocolHelperLib/UnitTest/ProtocolHelperLibUnitTestsHost.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiBootServicesTableStubLib/UefiBootServicesTableStubLib.inf
  }
  Silicon/NVIDIA/Library/PlatformIsSiliconLib/UnitTest/PlatformDepexLibUnitTestsHost.inf

  #
  # BrBctUpdateDeviceLib Host Based UnitTest Support
  #
  Silicon/NVIDIA/Library/BrBctUpdateDeviceLib/UnitTest/BrBctUpdateDeviceLibUnitTestsHost.inf

  #
  # QspiControllerLib Host Based UnitTest Support
  #
  Silicon/NVIDIA/Library/QspiControllerLib/UnitTest/QspiControllerLibUnitTestsHost.inf

  #
  # DeviceDiscoveryDriverLib Host Based UnitTest Support
  #
  Silicon/NVIDIA/Library/DeviceDiscoveryDriverLib/UnitTest/DeviceDiscoveryDriverLibUnitTestsHost.inf

  #
  # BootArchInfoParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/BootArchInfo/UnitTest/BootArchInfoParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # EthernetInfoParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/EthernetInfo/UnitTest/EthernetInfoParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # FixedFeatureFlagsParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/FixedFeatureFlags/UnitTest/FixedFeatureFlagsParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # GenericTimerParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/GenericTimer/UnitTest/GenericTimerParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # GenericWatchdogInfoParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/GenericWatchdog/UnitTest/GenericWatchdogInfoParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # PowerManagementProfileParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/PowerManagementProfile/UnitTest/PowerManagementProfileParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # Dbg2NetworkParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/Dbg2NetworkParser/UnitTest/Dbg2NetworkParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # ProtocolBasedObjectsParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/ProtocolBasedObjects/UnitTest/ProtocolBasedObjectsParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # SsdtTableGeneratorParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/SsdtTableGenerator/UnitTest/SsdtTableGeneratorParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # TpmInfoParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/TpmInfo/UnitTest/TpmInfoParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # FanInfoParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/FanInfo/UnitTest/FanInfoParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # HdaInfoParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/HdaInfo/UnitTest/HdaInfoParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }

  #
  # IpmiParser Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/IpmiParser/UnitTest/IpmiParserUnitTestsHost.inf {
    <LibraryClasses>
      PcdLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PcdStubLib/PcdStubLib.inf
  }


[PcdsDynamicDefault]
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize|0x00010000
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|FALSE

[PcdsFeatureFlag]
  gIpmiFeaturePkgTokenSpaceGuid.PcdIpmiFeatureEnable|TRUE

[PcdsFixedAtBuild.common]
  gNVIDIATokenSpaceGuid.PcdBuildEpoch|$(BUILD_EPOCH)
  gEfiCryptoPkgTokenSpaceGuid.PcdHashApiLibPolicy|0x00000002
