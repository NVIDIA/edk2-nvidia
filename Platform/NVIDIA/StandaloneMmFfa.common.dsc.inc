#
#  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

[Defines]
  SKUID_IDENTIFIER               = ALL
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT

!include Platform/NVIDIA/StandaloneMm.common.dsc.inc
!include Platform/NVIDIA/StandaloneMm.legacy.dsc.inc

################################################################################
#
# Library Class section - list of all Library Classes needed by this Platform.
#
################################################################################
[LibraryClasses]
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  StandaloneMmCoreEntryPoint|Silicon/NVIDIA/StandaloneMm/Library/StandaloneMmHafniumEntryPoint/StandaloneMmHafniumCoreEntryPoint.inf
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
  DebugLib|Silicon/NVIDIA/Library/DebugLibHafnium/DebugLibHafnium.inf
  ArmSvcLib|Silicon/NVIDIA/Library/ArmSvcHvcLibHafnium/ArmSvcHvcLibHafnium.inf
  ArmGenericTimerCounterLib|ArmPkg/Library/ArmGenericTimerVirtCounterLib/ArmGenericTimerVirtCounterLib.inf
  SlabMemoryAllocationLib|Silicon/NVIDIA/StandaloneMm/Library/StandaloneMmHafniumSlabMemAllocLib/StandaloneMmHafniumSlabMemAllocLib.inf
  StandaloneMmMmuLib|Silicon/NVIDIA/StandaloneMm/Library/StandaloneMmHafniumMmuLib/StandaloneMmHafniumMmuLib.inf

[LibraryClasses.common.MM_STANDALONE]

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################
[PcdsFixedAtBuild]
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0xF000
  gArmTokenSpaceGuid.PcdMmBufferSize|65536
  gNVIDIATokenSpaceGuid.PcdSecureQspiUseFastRead|FALSE
  gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence|TRUE
  gNVIDIATokenSpaceGuid.PcdConfigMgrObjMax|0x400

  # DEBUG_ASSERT_ENABLED       0x01
  # DEBUG_PRINT_ENABLED        0x02
  # DEBUG_CODE_ENABLED         0x04
  # CLEAR_MEMORY_ENABLED       0x08
  # ASSERT_BREAKPOINT_ENABLED  0x10
  # ASSERT_DEADLOOP_ENABLED    0x20
  # ASSERT_RESET_ENABLED       0x40
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x43
  gNVIDIATokenSpaceGuid.PcdAssertResetTimeoutValue|5

[PcdsFeatureFlag]

[PcdsPatchableInModule]
  #
  # NV Storage PCDs.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwSpareBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingSize|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwSpareSize|0x0

###################################################################################################
#
# Components Section - list of the modules and components that will be processed by compilation
#                      tools and the EDK II tools to generate PE32/PE32+/Coff image files.
#
# Note: The EDK II DSC file is not used to specify how compiled binary images get placed
#       into firmware volume images. This section is just a list of modules to compile from
#       source into UEFI-compliant binaries.
#       It is the FDF file that contains information on combining binary files into firmware
#       volume images, whose concept is beyond UEFI and is described in PI specification.
#       Binary modules do not need to be listed in this section, as they should be
#       specified in the FDF file. For example: Shell binary (Shell_Full.efi), FAT binary (Fat.efi),
#       Logo (Logo.bmp), and etc.
#       There may also be modules listed in this section that are not required in the FDF file,
#       When a module listed here is excluded from FDF file, then UEFI-compliant binary will be
#       generated for it, but the binary will not be put into any firmware volume.
#
###################################################################################################
[Components.common]
  #
  # MM Core
  #
  Silicon/StandaloneMmPkgLegacy/Core/StandaloneMmCore.inf

  #
  # MM Qspi Driver
  #
  Silicon/NVIDIA/Drivers/QspiControllerDxe/QspiControllerStandaloneMm.inf

  #
  # MM NorFlash Driver
  #
  Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashStandaloneMm.inf

  #
  # MM NOR FvB driver
  #
  Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashStandaloneMm.inf {
    <LibraryClasses>
      GptLib|Silicon/NVIDIA/Library/GptLib/GptLib.inf
  }

  #
  # MM FW Partition Driver
  #
  Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionStandaloneMm.inf

  #
  # MM Fault Tolerant Write Driver
  #
  MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteStandaloneMm.inf {
    <LibraryClasses>
      GptLib|Silicon/NVIDIA/Library/GptLib/GptLib.inf
      NULL|Silicon/NVIDIA/Drivers/FvbNorFlashDxe/StandaloneMmFixupPcd.inf
  }

  #
  # MM Variable Driver
  #
  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableStandaloneMm.inf {
    <LibraryClasses>
      DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
      VarCheckLib|MdeModulePkg/Library/VarCheckLib/VarCheckLib.inf
      NULL|MdeModulePkg/Library/VarCheckUefiLib/VarCheckUefiLib.inf
      NULL|MdeModulePkg/Library/VarCheckPolicyLib/VarCheckPolicyLibStandaloneMm.inf
      GptLib|Silicon/NVIDIA/Library/GptLib/GptLib.inf
      NULL|Silicon/NVIDIA/Drivers/FvbNorFlashDxe/StandaloneMmFixupPcd.inf
  }

  #
  # GPIO Driver
  #
  Silicon/NVIDIA/Drivers/TegraGpio/TegraGpioStandaloneMm.inf

  #
  # MM User Authentication Driver
  #
  Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationMm.inf


[Components.AARCH64]
  #
  # MM CPU Driver
  #
  Silicon/NVIDIA/StandaloneMm/Drivers/NvMmStandaloneMmCpu/StandaloneMmCpu.inf

###################################################################################################
#
# BuildOptions Section - Define the module specific tool chain flags that should be used as
#                        the default flags for a module. These flags are appended to any
#                        standard flags that are defined by the build process. They can be
#                        applied for any modules or only those modules with the specific
#                        module style (EDK or EDKII) specified in [Components] section.
#
###################################################################################################
[BuildOptions.AARCH64]
  GCC:*_*_*_DLINK_FLAGS = -Wl,-z,common-page-size=0x1000 -Wl,-z,text,-Bsymbolic,-pie

[BuildOptions.AARCH64]
  # Avoid absolute symbols when linking, which will cause a failure in GenFw.
  # This happens in GCC toolchains not configured with --enable-default-pie,
  # such as ubuntu's "gcc-aarch64-linux-gnu" toolchain.  By adding the
  # following, we get the equivalent of "--enable-default-pie".
  GCC:*_*_*_DLINK_FLAGS = -Wl,-z,text,-Bsymbolic,-pie
  GCC:*_*_*_CC_FLAGS = -fPIE

[BuildOptions]
  RELEASE_*_*_CC_FLAGS  = -DMDEPKG_NDEBUG
  *_*_*_CC_FLAGS  = -DDISABLE_NEW_DEPRECATED_INTERFACES -fpie

  # Avoid absolute symbols when linking, which will cause a failure in GenFw.
  # This happens in GCC toolchains not configured with --enable-default-pie,
  # such as ubuntu's "gcc-aarch64-linux-gnu" toolchain.  By adding -fPIE here,
  # we get the equivalent of "--enable-default-pie".
  GCC:*_*_*_CC_FLAGS = -fPIE
