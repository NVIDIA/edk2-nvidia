# SPDX-FileCopyrightText: Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.


[Defines]
  PLATFORM_NAME                  = HostBasedTests
  PLATFORM_GUID                  = fb5efb94-61c8-42e0-9cb4-a33ba4e4aec8
  OUTPUT_DIRECTORY               = Build/HostBasedTests
  SKUID_IDENTIFIER               = ALL
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = IA32|X64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT

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

[PcdsDynamicDefault]
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize|0x00010000
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|FALSE
