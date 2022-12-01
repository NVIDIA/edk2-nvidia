#Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

[Defines]
  PLATFORM_NAME                  = HostBasedTests
  PLATFORM_GUID                  = fb5efb94-61c8-42e0-9cb4-a33ba4e4aec8
  OUTPUT_DIRECTORY               = Build/HostBasedTests
  SKUID_IDENTIFIER               = ALL
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = IA32|X64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  FLASH_DEFINITION               = Platform/NVIDIA/HostBasedTests/HostBasedTests.fdf

!include Platform/NVIDIA/HostBasedTests/HostBasedTests.dsc.inc

[Components]
  #
  # FvbDxe Host Based UnitTest Support
  #
  Silicon/NVIDIA/Drivers/FvbDxe/UnitTest/FvbDxeUnitTestsHost.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
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
    <BuildOptions>
      GCC:*_*_*_DLINK_FLAGS = -Wl,--wrap=LibPcdGetBool,--wrap=EfiGetVariable,--wrap=EfiSetVariable,--wrap=EfiCreateProtocolNotifyEvent,--wrap=GetPerformanceCounter,--wrap=GetTimeInNanoSecond,--wrap=EfiAtRuntime,--wrap=EfiGetSystemConfigurationTable
  }

  # IPMI Blob Transfer protocol unit tests
  Silicon/NVIDIA/Drivers/IpmiBlobTransferDxe/UnitTest/IpmiBlobTransferTestUnitTestsHost.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Drivers/IpmiBlobTransferDxe/IpmiBlobTransferDxe.inf
      IpmiBaseLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/IpmiStubLib/IpmiStubLib.inf
  }

  #
  # OEM Send Description
  #
  Silicon/NVIDIA/Drivers/OemDescStatusCodeDxe/UnitTest/OemDescStatusCodeDxeUnitTest.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Drivers/OemDescStatusCodeDxe/OemDescStatusCodeDxe.inf
      IpmiBaseLib|IpmiFeaturePkg/Library/IpmiBaseLibNull/IpmiBaseLibNull.inf
      DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
      DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
    <BuildOptions>
      GCC:*_*_*_DLINK_FLAGS = -Wl,--wrap=IpmiSubmitCommand,--wrap=GetDebugPrintErrorLevel
  }

  #
  # Redfish CredentialBootstrap tests
  #
  Silicon/NVIDIA/Library/RedfishPlatformCredentialLib/UniTest/CredentialBootstrapUnitTest.inf {
    <LibraryClasses>
      RedfishPlatformCredentialLib|Silicon/NVIDIA/Library/RedfishPlatformCredentialLib/RedfishPlatformCredentialLib.inf
      IpmiBaseLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/IpmiStubLib/IpmiStubLib.inf
      UefiRuntimeServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiRuntimeServicesTableStubLib/UefiRuntimeServicesTableStubLib.inf
  }

[PcdsDynamicDefault]
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize|0x00010000
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|FALSE

[PcdsFeatureFlag]
  gIpmiFeaturePkgTokenSpaceGuid.PcdIpmiFeatureEnable|TRUE
