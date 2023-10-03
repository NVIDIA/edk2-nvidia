#
#  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2018 - 2022, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = TegraVirt
  PLATFORM_GUID                  = 5cf4bdba-e714-4a39-8d6d-440ba19e4b4e
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x0001001B
  OUTPUT_DIRECTORY               = Build/TegraVirt
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/NVIDIA/TegraVirt/TegraVirt.fdf

[Defines.AARCH64]
  DEFINE ACPIVIEW_ENABLE         = TRUE

!include ArmVirtPkg/ArmVirt.dsc.inc

!if $(ARCH) == AARCH64
!include DynamicTablesPkg/DynamicTables.dsc.inc
!endif

!include MdePkg/MdeLibs.dsc.inc

[BuildOptions.common.EDKII.DXE_CORE,BuildOptions.common.EDKII.DXE_DRIVER,BuildOptions.common.EDKII.UEFI_DRIVER,BuildOptions.common.EDKII.UEFI_APPLICATION]
  GCC:*_*_*_DLINK_FLAGS = -Wl,-z,common-page-size=0x1000

[BuildOptions.common.EDKII.DXE_RUNTIME_DRIVER]
  GCC:*_*_AARCH64_DLINK_FLAGS = -Wl,-z,common-page-size=0x10000

[BuildOptions.common]
  GCC:*_*_*_PLATFORM_FLAGS = -fstack-protector-strong

[LibraryClasses.common]
  ArmLib|ArmPkg/Library/ArmLib/ArmBaseLib.inf
  ArmMmuLib|ArmPkg/Library/ArmMmuLib/ArmMmuBaseLib.inf

  # Virtio Support
  VirtioLib|OvmfPkg/Library/VirtioLib/VirtioLib.inf
  VirtioMmioDeviceLib|OvmfPkg/Library/VirtioMmioDeviceLib/VirtioMmioDeviceLib.inf

  ArmPlatformLib|ArmPlatformPkg/Library/ArmPlatformLibNull/ArmPlatformLibNull.inf
  ArmVirtMemInfoLib|ArmVirtPkg/Library/KvmtoolVirtMemInfoLib/KvmtoolVirtMemInfoLib.inf

  TimerLib|ArmPkg/Library/ArmArchTimerLib/ArmArchTimerLib.inf
  NorFlashPlatformLib|ArmVirtPkg/Library/NorFlashKvmtoolLib/NorFlashKvmtoolLib.inf

  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibNull/DxeCapsuleLibNull.inf

  # BDS Libraries
  UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
  PlatformBootManagerLib|Silicon/NVIDIA/Library/PlatformBootManagerLib/PlatformBootManagerLib.inf
  PlatformBootOrderLib|Silicon/NVIDIA/Library/PlatformBootOrderLib/PlatformBootOrderLib.inf
  BootLogoLib|MdeModulePkg/Library/BootLogoLib/BootLogoLib.inf

  CustomizedDisplayLib|MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf
  FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf

  FileExplorerLib|MdeModulePkg/Library/FileExplorerLib/FileExplorerLib.inf

  PciPcdProducerLib|OvmfPkg/Fdt/FdtPciPcdProducerLib/FdtPciPcdProducerLib.inf
  PciSegmentLib|MdePkg/Library/BasePciSegmentLibPci/BasePciSegmentLibPci.inf
  PciHostBridgeLib|OvmfPkg/Fdt/FdtPciHostBridgeLib/FdtPciHostBridgeLib.inf
  PciHostBridgeUtilityLib|ArmVirtPkg/Library/ArmVirtPciHostBridgeUtilityLib/ArmVirtPciHostBridgeUtilityLib.inf

  TpmMeasurementLib|MdeModulePkg/Library/TpmMeasurementLibNull/TpmMeasurementLibNull.inf
  AuthVariableLib|MdeModulePkg/Library/AuthVariableLibNull/AuthVariableLibNull.inf

  PlatformPeiLib|ArmVirtPkg/Library/KvmtoolPlatformPeiLib/KvmtoolPlatformPeiLib.inf

  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
  PlatformHookLib|ArmVirtPkg/Library/Fdt16550SerialPortHookLib/Fdt16550SerialPortHookLib.inf
  SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf

  HwInfoParserLib|DynamicTablesPkg/Library/FdtHwInfoParserLib/FdtHwInfoParserLib.inf
  DynamicPlatRepoLib|DynamicTablesPkg/Library/Common/DynamicPlatRepoLib/DynamicPlatRepoLib.inf

  TpmPlatformHierarchyLib|SecurityPkg/Library/PeiDxeTpmPlatformHierarchyLibNull/PeiDxeTpmPlatformHierarchyLib.inf
  Tcg2PhysicalPresenceLib|Silicon/NVIDIA/Library/DxeTcg2PhysicalPresenceLibNull/DxeTcg2PhysicalPresenceLibNull.inf
  IpmiBaseLib|IpmiFeaturePkg/Library/IpmiBaseLib/IpmiBaseLib.inf
  FwVariableLib|Silicon/NVIDIA/Library/FwVariableLib/FwVariableLib.inf

  # AndroidBootDxe Libraries
  NvgLib|Silicon/NVIDIA/Library/NvgLib/NvgLib.inf
  MceAriLib|Silicon/NVIDIA/Library/MceAriLib/MceAriLib.inf
  GoldenRegisterLib|Silicon/NVIDIA/Library/GoldenRegisterLib/GoldenRegisterLib.inf
  PlatformResourceLib|Silicon/NVIDIA/Library/PlatformResourceLib/PlatformResourceLib.inf
  DtPlatformDtbLoaderLib|Silicon/NVIDIA/Library/DxeDtPlatformDtbLoaderLib/DxeDtPlatformDtbLoaderLib.inf
  DeviceTreeHelperLib|Silicon/NVIDIA/Library/DeviceTreeHelperLib/DeviceTreeHelperLib.inf
  BootChainInfoLib|Silicon/NVIDIA/Library/BootChainInfoLib/BootChainInfoLib.inf
  HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
  TegraPlatformInfoLib|Silicon/NVIDIA/Library/TegraPlatformInfoLib/TegraPlatformInfoLib.inf
  AndroidBcbLib|Silicon/NVIDIA/Library/AndroidBcbLib/AndroidBcbLib.inf

  # Override the ResetSystemLib used by ArmVirt with a Null implementation.
  # ArmVirtPsciResetSystemLib is not compatible with our DTB.  It expects
  # arm,psci-0.2 and we have arm,psci-1.0.  For now, we'll use the Null lib and
  # not support reset.
  ResetSystemLib|MdeModulePkg/Library/BaseResetSystemLibNull/BaseResetSystemLibNull.inf

  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf

[LibraryClasses.common.SEC, LibraryClasses.common.PEI_CORE, LibraryClasses.common.PEIM]
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
  PlatformHookLib|ArmVirtPkg/Library/Fdt16550SerialPortHookLib/EarlyFdt16550SerialPortHookLib.inf
  SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf

[LibraryClasses.common.UEFI_DRIVER]
  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf

[BuildOptions]
  *_*_*_CC_FLAGS = -D DISABLE_NEW_DEPRECATED_INTERFACES
  #
  # We need to avoid jump tables in SEC and BASE modules, so that the PE/COFF
  # self-relocation code itself is guaranteed to be position independent.
  #
  GCC:*_*_*_CC_XIPFLAGS = -fno-jump-tables

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsFeatureFlag.common]
  ## If TRUE, Graphics Output Protocol will be installed on virtual handle created by ConsplitterDxe.
  #  It could be set FALSE to save size.
  gEfiMdeModulePkgTokenSpaceGuid.PcdConOutGopSupport|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdConOutUgaSupport|FALSE

[PcdsFixedAtBuild.common]
  gNVIDIATokenSpaceGuid.PcdPlatformFamilyName|L"TegraVirt"
  gNVIDIATokenSpaceGuid.PcdUefiVersionString|L"$(BUILDID_STRING)"
  gNVIDIATokenSpaceGuid.PcdUefiDateTimeBuiltString|L"$(BUILD_DATE_TIME)"
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"$(BUILDID_STRING)"
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareReleaseDateString|L"$(BUILD_DATE_TIME)"

  #
  # Enable emulated variable NV mode in variable driver.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE

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

  gArmPlatformTokenSpaceGuid.PcdCoreCount|1

!if $(ARCH) == AARCH64
  gArmTokenSpaceGuid.PcdVFPEnabled|1
!endif

  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x22000
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0x2000
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxAuthVariableSize|0x2800

  #
  # TTY Terminal Type
  # 0-PCANSI, 1-VT100, 2-VT00+, 3-UTF8, 4-TTYTERM
  gEfiMdePkgTokenSpaceGuid.PcdDefaultTerminalType|4

  #
  # ARM Virtual Architectural Timer -- fetch frequency from KVM
  #
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|0

  # Use MMIO for accessing Serial port registers.
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialUseMmio|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialPciDeviceInfo|{0xFF}

  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ 0x21, 0xaa, 0x2c, 0x46, 0x14, 0x76, 0x03, 0x45, 0x83, 0x6e, 0x8a, 0xb6, 0xf4, 0x66, 0x23, 0x31 }

  #
  # The maximum physical I/O addressability of the processor, set with
  # BuildCpuHob().
  #
  gEmbeddedTokenSpaceGuid.PcdPrePiCpuIoSize|16

  gEfiMdeModulePkgTokenSpaceGuid.PcdStatusCodeUseSerial|TRUE

  #
  # UART 16550 parameters
  #
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultBaudRate|115200
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialClockRate|407347200

  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterStride|4
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialUseHardwareFlowControl|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialFifoControl|0x27
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialExtendedTxFifoSize|1

[PcdsPatchableInModule.common]
  #
  # These will be set at boot time.
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x0
  gArmTokenSpaceGuid.PcdSystemMemorySize|0x0
  gArmVirtTokenSpaceGuid.PcdDeviceTreeInitialBaseAddress|0x0

  gArmTokenSpaceGuid.PcdFvBaseAddress|0x0

  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterBase|0x0c280000

[PcdsDynamicHii]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|5

[PcdsDynamicDefault.common]
  gArmTokenSpaceGuid.PcdArmArchTimerSecIntrNum|0x0
  gArmTokenSpaceGuid.PcdArmArchTimerIntrNum|0x0
  gArmTokenSpaceGuid.PcdArmArchTimerVirtIntrNum|0x0
  gArmTokenSpaceGuid.PcdArmArchTimerHypIntrNum|0x0

  #
  # ARM General Interrupt Controller
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x0
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x0
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x0

  #
  # PCI settings
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdPciDisableBusEnumeration|TRUE

  # set PcdPciExpressBaseAddress to MAX_UINT64, which signifies that this
  # PCD and PcdPciDisableBusEnumeration above have not been assigned yet
  gEfiMdePkgTokenSpaceGuid.PcdPciExpressBaseAddress|0xFFFFFFFFFFFFFFFF

  gEfiMdePkgTokenSpaceGuid.PcdPciIoTranslation|0x0

  #
  # Set video resolution for boot options and for text setup.
  # PlatformDxe can set the former at runtime.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|800
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|600
  gEfiMdeModulePkgTokenSpaceGuid.PcdSetupVideoHorizontalResolution|640
  gEfiMdeModulePkgTokenSpaceGuid.PcdSetupVideoVerticalResolution|480

  # Setup Flash storage variables
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableBase|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize|0x40000
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingBase|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingSize|0x40000
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwSpareBase|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwSpareSize|0x40000

################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform
#
################################################################################
[Components.common]
  #
  # PEI Phase modules
  #
  Silicon/NVIDIA/TegraVirt/PrePi/TegraVirtPrePi.inf {
    <LibraryClasses>
      ExtractGuidedSectionLib|EmbeddedPkg/Library/PrePiExtractGuidedSectionLib/PrePiExtractGuidedSectionLib.inf
      LzmaDecompressLib|MdeModulePkg/Library/LzmaCustomDecompressLib/LzmaCustomDecompressLib.inf
      PrePiLib|EmbeddedPkg/Library/PrePiLib/PrePiLib.inf
      HobLib|EmbeddedPkg/Library/PrePiHobLib/PrePiHobLib.inf
      PrePiHobListPointerLib|ArmPlatformPkg/Library/PrePiHobListPointerLib/PrePiHobListPointerLib.inf
      MemoryAllocationLib|EmbeddedPkg/Library/PrePiMemoryAllocationLib/PrePiMemoryAllocationLib.inf
      DefaultExceptionHandlerLib|Silicon/NVIDIA/Library/PrePiDefaultExceptionHandlerLib/PrePiDefaultExceptionHandlerLib.inf
  }

  #
  # DXE
  #
  MdeModulePkg/Core/Dxe/DxeMain.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/DxeCrc32GuidedSectionExtractLib/DxeCrc32GuidedSectionExtractLib.inf
  }
  MdeModulePkg/Universal/PCD/Dxe/Pcd.inf {
    <LibraryClasses>
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  }

  #
  # Architectural Protocols
  #
  ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/VarCheckUefiLib/VarCheckUefiLib.inf
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }

  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
  MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf

  MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
  MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  MdeModulePkg/Universal/Metronome/Metronome.inf
  EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf

  MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
  MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
  MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

  MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf

  ArmPkg/Drivers/ArmGic/ArmGicDxe.inf
  ArmPkg/Drivers/TimerDxe/TimerDxe.inf {
    <LibraryClasses>
      NULL|ArmVirtPkg/Library/ArmVirtTimerFdtClientLib/ArmVirtTimerFdtClientLib.inf
  }

  MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf

  #
  # Platform Driver
  #
  EmbeddedPkg/Drivers/FdtClientDxe/FdtClientDxe.inf
  OvmfPkg/Fdt/HighMemDxe/HighMemDxe.inf

  #
  # FAT filesystem + GPT/MBR partitioning + UDF filesystem
  #
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  FatPkg/EnhancedFatDxe/Fat.inf
  MdeModulePkg/Universal/Disk/UdfDxe/UdfDxe.inf

  # Boot support for mkbootimg partitions
  Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf

  #
  # Bds
  #
  MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf {
    <LibraryClasses>
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  }
  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
  MdeModulePkg/Universal/DriverHealthManagerDxe/DriverHealthManagerDxe.inf
  MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
  MdeModulePkg/Logo/LogoDxe.inf
  MdeModulePkg/Application/UiApp/UiApp.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/DeviceManagerUiLib/DeviceManagerUiLib.inf
      NULL|MdeModulePkg/Library/BootManagerUiLib/BootManagerUiLib.inf
      NULL|MdeModulePkg/Library/BootMaintenanceManagerUiLib/BootMaintenanceManagerUiLib.inf
  }

  #
  # IPMI Null Driver
  #
  Silicon/NVIDIA/Drivers/IpmiNullDxe/IpmiNullDxe.inf

  #
  # BootManagerMenuApp
  #
  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf
