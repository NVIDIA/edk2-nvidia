#
#  SPDX-FileCopyrightText: Copyright (c) 2020-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
  StandaloneMmOpteeLib|Silicon/NVIDIA/Library/StandaloneMmOpteeLib/StandaloneMmOpteeLib.inf
  StandaloneMmCoreEntryPoint|Silicon/NVIDIA/Library/StandaloneMmCoreEntryPointOptee/StandaloneMmCoreEntryPointOptee.inf
  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLibNull/BaseCacheMaintenanceLibNull.inf
  PL011UartLib|ArmPlatformPkg/Library/PL011UartLib/PL011UartLib.inf
  TegraCombinedSerialPortLib|Silicon/NVIDIA/Library/TegraCombinedSerialPort/TegraStandaloneMmCombinedSerialPortLib.inf
  Tegra16550SerialPortLib|Silicon/NVIDIA/Library/Tegra16550SerialPortLib/Tegra16550SerialPortLib.inf
  TegraSbsaSerialPortLib|Silicon/NVIDIA/Library/TegraSbsaSerialPortLib/TegraSbsaSerialPortLib.inf
  SerialPortLib|Silicon/NVIDIA/Library/TegraSerialPortLib/TegraStandaloneMmSerialPortLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  QspiControllerLib|Silicon/NVIDIA/Library/QspiControllerLib/QspiControllerLib.inf
  VarSetCallbacksLib|Silicon/NVIDIA/Library/VarSetCallbacksLib/VarSetCallbacksLib.inf
  PlatformPasswordLibMm|Silicon/NVIDIA/Library/PlatformPasswordLibMm/PlatformPasswordLibMm.inf
  ArmSmcLib|ArmPkg/Library/ArmSmcLib/ArmSmcLib.inf
  HobPrintLib|MdeModulePkg/Library/HobPrintLib/HobPrintLib.inf

[LibraryClasses.common.MM_STANDALONE]

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################
[PcdsFixedAtBuild]
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0x2000
  gArmTokenSpaceGuid.PcdMmBufferSize|65536
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|FALSE
  gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence|TRUE
  gEfiCryptoPkgTokenSpaceGuid.PcdHashApiLibPolicy|0x00000002

[PcdsFeatureFlag]
  gArmTokenSpaceGuid.PcdFfaEnable|TRUE
  gNVIDIATokenSpaceGuid.PcdOpteePresent|TRUE
  gNVIDIATokenSpaceGuid.PcdVarStoreIntegritySupported|TRUE
  gNVIDIATokenSpaceGuid.PcdAssertOnVarStoreIntegrityCheckFail|TRUE

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
  # OP-TEE RPMB driver
  #
  Silicon/NVIDIA/Drivers/OpteeNvRpmb/OpTeeRpmbFvbNv.inf

  #
  # MM Fault Tolerant Write Driver
  #
  MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteStandaloneMm.inf {
    <LibraryClasses>
      GptLib|Silicon/NVIDIA/Library/GptLib/GptLib.inf
      NULL|Silicon/NVIDIA/Drivers/FvbNorFlashDxe/StandaloneMmFixupPcd.inf
      NULL|Silicon/NVIDIA/Drivers/OpteeNvRpmb/FixupPcdRpmb.inf
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
      NULL|Silicon/NVIDIA/Drivers/OpteeNvRpmb/FixupPcdRpmb.inf
  }

  #
  # MM FW Partition Driver
  #
  Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionStandaloneMm.inf

  #
  # MM Dice Driver
  #
  Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashStandaloneMmDice.inf

  #
  # MM User Authentication Driver
  #
  Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationMm.inf

[Components.AARCH64]
  #
  # MM CPU Driver
  #
  StandaloneMmPkg/Drivers/StandaloneMmCpu/StandaloneMmCpu.inf

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
  GCC:*_*_*_DLINK_FLAGS = -Wl,-z,common-page-size=0x1000 -march=armv8-a+nofp
  GCC:*_*_*_CC_FLAGS = -mstrict-align

[BuildOptions.AARCH64]
  # Avoid absolute symbols when linking, which will cause a failure in GenFw.
  # This happens in GCC toolchains not configured with --enable-default-pie,
  # such as ubuntu's "gcc-aarch64-linux-gnu" toolchain.  By adding the
  # following, we get the equivalent of "--enable-default-pie".
  GCC:*_*_*_DLINK_FLAGS = -Wl,-z,text,-Bsymbolic,-pie
  GCC:*_*_*_CC_FLAGS = -fPIE

[BuildOptions.ARM]
  GCC:*_*_*_DLINK_FLAGS = -Wl,-z,common-page-size=0x1000 -march=armv7-a
  GCC:*_*_*_CC_FLAGS = -fno-stack-protector
