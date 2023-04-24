# Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
  IoLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/IoStubLib/IoStubLib.inf
  NorFlashStubLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/NorFlashStubLib/NorFlashStubLib.inf
  PlatformResourceLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/PlatformResourceStubLib/PlatformResourceStubLib.inf
  TegraPlatformInfoLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/TegraPlatformInfoStubLib/TegraPlatformInfoStubLib.inf
  UefiBootServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiBootServicesTableStubLib/UefiBootServicesTableStubLib.inf
  UefiRuntimeServicesTableLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/UefiRuntimeServicesTableStubLib/UefiRuntimeServicesTableStubLib.inf
  StandaloneMmOpteeLib|Silicon/NVIDIA/Library/HostBasedTestStubLib/StandaloneMmOpteeStubLib/StandaloneMmOpteeStubLib.inf

[BuildOptions.common.EDKII.HOST_APPLICATION]
!ifdef $(HOSTAPP_STATIC_LINK)
  # Statically link the host-based executable.  This allows us to easily use
  # alternate toolchains at build time and not introduce a runtime dependency.
  GCC:*_*_*_DLINK2_FLAGS = -static
!endif

[BuildOptions]
!ifdef $(HOSTAPP_GCC_COVERAGE)
  # Generate coverage data when tests are run.
  GCC:*_*_*_DLINK2_FLAGS = -fprofile-arcs
  GCC:*_*_*_CC_FLAGS = -fprofile-arcs -ftest-coverage
!endif

[PcdsFixedAtBuild.common]
  #  DEBUG_INIT      0x00000001  // Initialization
  #  DEBUG_WARN      0x00000002  // Warnings
  #  DEBUG_LOAD      0x00000004  // Load events
  #  DEBUG_FS        0x00000008  // EFI File system
  #  DEBUG_POOL      0x00000010  // Alloc & Free (pool)
  #  DEBUG_PAGE      0x00000020  // Alloc & Free (page)
  #  DEBUG_INFO      0x00000040  // Informational debug messages
  #  DEBUG_DISPATCH  0x00000080  // PEI/DXE/SMM Dispatchers
  #  DEBUG_VARIABLE  0x00000100  // Variable
  #  DEBUG_BM        0x00000400  // Boot Manager
  #  DEBUG_BLKIO     0x00001000  // BlkIo Driver
  #  DEBUG_NET       0x00004000  // SNP Driver
  #  DEBUG_UNDI      0x00010000  // UNDI Driver
  #  DEBUG_LOADFILE  0x00020000  // LoadFile
  #  DEBUG_EVENT     0x00080000  // Event messages
  #  DEBUG_GCD       0x00100000  // Global Coherency Database changes
  #  DEBUG_CACHE     0x00200000  // Memory range cachability changes
  #  DEBUG_VERBOSE   0x00400000  // Detailed debug messages that may
  #                              // significantly impact boot performance
  #  DEBUG_ERROR     0x80000000  // Error
  gEfiMdePkgTokenSpaceGuid.PcdFixedDebugPrintErrorLevel|0x8000000F
