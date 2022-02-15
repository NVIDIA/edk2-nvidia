# Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

!include UnitTestFrameworkPkg/UnitTestFrameworkPkgHost.dsc.inc

[LibraryClasses.common.HOST_APPLICATION]
  # standard libs
  BootChainInfoLib|Silicon/NVIDIA/Library/BootChainInfoLib/BootChainInfoLib.inf
  CmockaLib|UnitTestFrameworkPkg/Library/CmockaLib/CmockaLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  ResetSystemLib|MdeModulePkg/Library/BaseResetSystemLibNull/BaseResetSystemLibNull.inf

  # stub libs
  FlashStubLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/FlashStubLib/FlashStubLib.inf
  HobLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/HobStubLib/HobStubLib.inf
  PlatformResourceLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PlatformResourceStubLib/PlatformResourceStubLib.inf
  TegraPlatformInfoLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/TegraPlatformInfoStubLib/TegraPlatformInfoStubLib.inf
  UefiBootServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiBootServicesTableStubLib/UefiBootServicesTableStubLib.inf
  UefiRuntimeServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiRuntimeServicesTableStubLib/UefiRuntimeServicesTableStubLib.inf

[BuildOptions.common.EDKII.HOST_APPLICATION]
!ifdef $(HOSTAPP_STATIC_LINK)
  # Statically link the host-based executable.  This allows us to easily use
  # alternate toolchains at build time and not introduce a runtime dependency.
  GCC:*_*_*_DLINK2_FLAGS = -static -s
!endif

[BuildOptions]
!ifdef $(HOSTAPP_GCC_COVERAGE)
  # Generate coverage data when tests are run.
  GCC:*_*_*_DLINK2_FLAGS = -fprofile-arcs
  GCC:*_*_*_CC_FLAGS = -fprofile-arcs -ftest-coverage
!endif
