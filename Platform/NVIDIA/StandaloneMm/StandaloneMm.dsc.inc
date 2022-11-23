#
#  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
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

################################################################################
#
# Library Class section - list of all Library Classes needed by this Platform.
#
################################################################################
[LibraryClasses]
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  StandaloneMmCoreEntryPoint|Silicon/NVIDIA/StandaloneMm/Library/StandaloneMmHafniumEntryPoint/StandaloneMmHafniumCoreEntryPoint.inf
  FdtLib|EmbeddedPkg/Library/FdtLib/FdtLib.inf
  DebugLib|Silicon/NVIDIA/Library/DebugLibHafnium/DebugLibHafnium.inf
  ArmSvcLib|Silicon/NVIDIA/Library/ArmSvcHvcLibHafnium/ArmSvcHvcLibHafnium.inf
  QspiControllerLib|Silicon/NVIDIA/Library/QspiControllerLib/QspiControllerLib.inf
  ArmGenericTimerCounterLib|ArmPkg/Library/ArmGenericTimerVirtCounterLib/ArmGenericTimerVirtCounterLib.inf
  ErotQspiLib|Silicon/NVIDIA/Library/ErotQspiLib/ErotQspiLib.inf

[LibraryClasses.common.MM_STANDALONE]

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################
[PcdsFixedAtBuild]
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
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x8000000F
  gNVIDIATokenSpaceGuid.PcdFirmwareNickNameString|L"STMM:"
  gNVIDIATokenSpaceGuid.PcdFirmwareFullNameString|L"StandaloneMm"

  gNVIDIATokenSpaceGuid.PcdFirmwareNickNameString|L"STMM:"
  gNVIDIATokenSpaceGuid.PcdFirmwareFullNameString|L"StandaloneMm"
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0x2000
  gArmTokenSpaceGuid.PcdMmBufferSize|65536
  gNVIDIATokenSpaceGuid.PcdSecureQspiUseFastRead|FALSE

[PcdsFeatureFlag]
  gArmTokenSpaceGuid.PcdFfaEnable|TRUE

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
  # SP Manifest file
  # Note: it is not included in the FDF as it is not part of the FV
  #
  Silicon/NVIDIA/StandaloneMm/Manifest/Manifest.inf

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
  # MM MCTP/ERoT Drivers
  #
  Silicon/NVIDIA/Drivers/ErotQspiDxe/ErotQspiStmm.inf
  Silicon/NVIDIA/Drivers/MctpMmDxe/MctpStandaloneMm.inf

  #
  # Sequential Record Flash Driver
  #
  Silicon/NVIDIA/Drivers/SequentialRecordStMm/SequentialRecordStorage.inf

  #
  # Sequential Record client driver
  #
  Silicon/NVIDIA/Drivers/SequentialRecordStMm/SequentialRecordComm.inf

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
  GCC:*_*_*_DLINK_FLAGS = -z common-page-size=0x1000 -Wl,-z,text,-Bsymbolic,-pie

[BuildOptions.AARCH64]
  # Avoid absolute symbols when linking, which will cause a failure in GenFw.
  # This happens in GCC toolchains not configured with --enable-default-pie,
  # such as ubuntu's "gcc-aarch64-linux-gnu" toolchain.  By adding the
  # following, we get the equivalent of "--enable-default-pie".
  GCC:*_*_*_DLINK_FLAGS = -Wl,-z,text,-Bsymbolic,-pie
  GCC:*_*_*_CC_FLAGS = -fPIE

[BuildOptions]
  RELEASE_*_*_CC_FLAGS  = -DMDEPKG_NDEBUG
  *_*_*_CC_FLAGS  = -DDISABLE_NEW_DEPRECATED_INTERFACES -fpie -mstrict-align

  # Avoid absolute symbols when linking, which will cause a failure in GenFw.
  # This happens in GCC toolchains not configured with --enable-default-pie,
  # such as ubuntu's "gcc-aarch64-linux-gnu" toolchain.  By adding -fPIE here,
  # we get the equivalent of "--enable-default-pie".
  GCC:*_*_*_CC_FLAGS = -fPIE
