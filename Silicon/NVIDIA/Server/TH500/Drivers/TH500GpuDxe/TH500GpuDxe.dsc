#/** @file
#  NVIDIA GPU driver binding
#
#  Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
#**/

[Defines]
    PLATFORM_NAME               = NVIDIA
    PLATFORM_GUID               = 9c4c1551-e2f9-4cb8-ba4b-cd017d99d5b6
    PLATFORM_VERSION            = 1.00
    DSC_SPECIFICATION           = 0x00010005
    OUTPUT_DIRECTORY            = buildefi
    SUPPORTED_ARCHITECTURES     = X64|AARCH64
    BUILD_TARGETS               = DEBUG|RELEASE
    SKUID_IDENTIFIER            = DEFAULT

[BuildOptions]
# Linker flags for link time optimization using gcc-4.6.  Must include -flto in the CC flags as well.  At the moment this increases code size.
#    RELEASE_GCC45_X64_DLINK_FLAGS           = -flto -Os -shared
#   *_GCC45_IA32_DLINK_FLAGS                = --script=$(WORKSPACE)/gpu/nv-gcc4.4-ld-ia32-script

    # DebugLib DEBUG() macros gated by MDEPKG_NDEBUG, need to disable for release to avoid expansion.
    MSFT:RELEASE_*_*_CC_FLAGS = /DMDEPKG_NDEBUG

    # Disable stack protector due to missing symbol errors
    #   advises recompile with -fPIC
#   GCC:*_*_AARCH64_CC_FLAGS    = -fno-stack-protector -nostdlib -fno-builtin-memcpy
    GCC:*_*_AARCH64_CC_FLAGS    = -fno-stack-protector -nostdlib

[SkuIds]
    0|DEFAULT              # The entry: 0|DEFAULT is reserved and always required.

# Does not work for X64
# !include DynamicTablesPkg/DynamicTables.dsc.inc

[LibraryClasses.common]
    UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
    UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
    UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
    BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
    BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
    PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
    PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
    MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
    DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
    UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
    PciLib|MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
    PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
    # DebugLib inclusion - missing StatusCode under developer options (Apple EG1 Legacy)
    DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
    DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
    IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
    PlatformHookLib|MdeModulePkg/Library/BasePlatformHookLibNull/BasePlatformHookLibNull.inf
    SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf
    DebugLib|MdePkg/Library/UefiDebugLibStdErr/UefiDebugLibStdErr.inf
    DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
    # Required for current edk2 master and edk2-stable202205 - not present for edk2-stable202102 - disable
    RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
    # New for parsing/processing AML
    AmlLib|DynamicTablesPkg/Library/Common/AmlLib/AmlLib.inf
    AcpiHelperLib|DynamicTablesPkg/Library/Common/AcpiHelperLib/AcpiHelperLib.inf
    # BaseSafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
    # Required to Test Dxe in standalone mode
    HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
[Components]
    TH500GpuDxe.inf
    DynamicTablesPkg/Library/Common/AmlLib/AmlLib.inf {
        <BuildOptions>
        MSFT:*_*_*_CC_FLAGS = /wd4244
    }
    DynamicTablesPkg/Library/Common/AcpiHelperLib/AcpiHelperLib.inf {
        <BuildOptions>
        MSFT:*_*_*_CC_FLAGS = /wd4244
    }
