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

!include Platform/NVIDIA/StandaloneMmFfa.common.dsc.inc

################################################################################
#
# Library Class section - list of all Library Classes needed by this Platform.
#
################################################################################
[LibraryClasses]
  ErotQspiLib|Silicon/NVIDIA/Library/ErotQspiLib/ErotQspiLib.inf
  PldmBaseLib|Silicon/NVIDIA/Library/PldmBaseLib/PldmBaseLib.inf
  ResetSystemLib|Silicon/NVIDIA/StandaloneMm/Library/StandaloneMmHafniumResetLib/StandaloneMmHafniumResetLib.inf
  DebugLogScratchRegLib|Silicon/NVIDIA/Library/DebugLogScratchRegLib/DebugLogScratchRegLib.inf
  PlatformResourceLib|Silicon/NVIDIA/Library/PlatformResourceLib/T24xPlatformResourceLib.inf
[LibraryClasses.common.MM_STANDALONE]

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################
[PcdsFixedAtBuild]
  gNVIDIATokenSpaceGuid.PcdFirmwareNickNameString|L"STMM:"
  gNVIDIATokenSpaceGuid.PcdFirmwareFullNameString|L"StandaloneMm"

  ## SCRATCH_SCRATCH_<i> [i >= 21] is used to log crash related information.
  gNVIDIATokenSpaceGuid.PcdNvScratchRegBase|0x0000C3903DC
  ## Register 21(i = 21) is to log the FW image name.
  ## This is the starting offset of the debug data and must
  ## be used for FW image name.
  gNVIDIATokenSpaceGuid.PcdNvFwNameStartReg|21
  ## Register 21 to log file name.
  gNVIDIATokenSpaceGuid.PcdNvFileNameStartReg|22
  gNVIDIATokenSpaceGuid.PcdNvFileNameRegLimit|20
  ## Register 42 to log line name.
  gNVIDIATokenSpaceGuid.PcdNvLineNumStartReg|42
  ## Restrict the File Name to 2 32-bit values.
  gNVIDIATokenSpaceGuid.PcdNvLineNumRegLimit|2
  ## Register 44 to log Exception type.
  gNVIDIATokenSpaceGuid.PcdExceptionTypeStartReg|44
  ## Register 45 to log FAR Reg.
  gNVIDIATokenSpaceGuid.PcdFARStartReg|45
  ## Register 47 to log FP.
  gNVIDIATokenSpaceGuid.PcdFPStartReg|47
  ## Total number of registers for FP (across frames).
  gNVIDIATokenSpaceGuid.PcdFPRegLimit|53
  ## Limit of registers for each Frame.
  gNVIDIATokenSpaceGuid.PcdPerFpLimit|7

  gNVIDIATokenSpaceGuid.PcdNvLogToScratchRegs|TRUE
  ## Firmware string to log.
  gNVIDIATokenSpaceGuid.PcdNvFirmwareStr|"UEMM"


  # DEBUG_ASSERT_ENABLED       0x01
  # DEBUG_PRINT_ENABLED        0x02
  # DEBUG_CODE_ENABLED         0x04
  # CLEAR_MEMORY_ENABLED       0x08
  # ASSERT_BREAKPOINT_ENABLED  0x10
  # ASSERT_DEADLOOP_ENABLED    0x20
  # ASSERT_RESET_ENABLED       0x40
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x43
  gNVIDIATokenSpaceGuid.PcdAssertResetTimeoutValue|5
  gNVIDIATokenSpaceGuid.PcdDataOnlyFlashIsSupported|TRUE
  gNVIDIATokenSpaceGuid.PcdFwBlobIsSupported|TRUE

[PcdsFeatureFlag]

[PcdsPatchableInModule]

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
  # SP Manifest file
  # Note: it is not included in the FDF as it is not part of the FV
  #
  Silicon/NVIDIA/StandaloneMm/Manifest/Manifest.inf

  #
  # MM NorFlash Blob Driver for ERoT Less updates.
  #
  Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashStandaloneMmBlob.inf


  #
  # MM MCTP/ERoT Drivers
  #
  Silicon/NVIDIA/Drivers/ErotQspiDxe/ErotQspiStmm.inf
  Silicon/NVIDIA/Drivers/MctpMmDxe/MctpStandaloneMm.inf

  #
  # Sequential Record Flash Driver
  #
  Silicon/NVIDIA/Drivers/SequentialRecordStMm/SequentialRecordStorage.inf

  #
  # CMET Record Storage Driver
  #
  Silicon/NVIDIA/Drivers/SequentialRecordStMm/CmetRecordStorage.inf

  #
  # Sequential Record client driver
  #
  Silicon/NVIDIA/Drivers/SequentialRecordStMm/SequentialRecordComm.inf

  #
  # MM Error Serialization Driver
  #
  Silicon/NVIDIA/Server/TH500/Drivers/ErrorSerializationMmDxe/ErrorSerializationMm.inf

  #
  # MM Oem Partition Sample Driver.
  #
  Silicon/NVIDIA/Server/TH500/Drivers/OemPartitionDxeMm/OemPartitionMm.inf

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
