#
#  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
#  Copyright (c) 2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = StandaloneMmOptee
  PLATFORM_GUID                  = 6f1dc7c3-1b41-4b4d-bf53-9023bafac4cc
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/StandaloneMmOptee
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = ALL
  FLASH_DEFINITION               = Platform/NVIDIA/StandaloneMmOptee/StandaloneMmOptee.fdf

!include Platform/NVIDIA/StandaloneMm.common.dsc.inc
################################################################################
#
# Library Class section - list of all Library Classes needed by this Platform.
#
################################################################################
[LibraryClasses]
  StandaloneMmOpteeLib|Silicon/NVIDIA/Library/StandaloneMmOpteeLib/StandaloneMmOpteeLib.inf
  StandaloneMmCoreEntryPoint|Silicon/NVIDIA/Library/StandaloneMmCoreEntryPointOptee/StandaloneMmCoreEntryPointOptee.inf

[LibraryClasses.common.MM_STANDALONE]

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################
[PcdsFixedAtBuild]
  gArmTokenSpaceGuid.PcdMmBufferSize|65536
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0x4f90
  gArmTokenSpaceGuid.PcdMmBufferSize|65536

[PcdsFeatureFlag]
  gArmTokenSpaceGuid.PcdFfaEnable|TRUE
  gNVIDIATokenSpaceGuid.PcdOpteePresent|TRUE

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

[Components.AARCH64]

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
  GCC:*_*_*_DLINK_FLAGS = -z common-page-size=0x1000 -march=armv8-a+nofp
  GCC:*_*_*_CC_FLAGS = -mstrict-align

[BuildOptions.ARM]
  GCC:*_*_*_DLINK_FLAGS = -z common-page-size=0x1000 -march=armv7-a
  GCC:*_*_*_CC_FLAGS = -fno-stack-protector
