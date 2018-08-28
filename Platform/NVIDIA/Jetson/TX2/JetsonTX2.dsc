#
#  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = JetsonTX2
  PLATFORM_GUID                  = 7eed17d6-f913-4726-be95-2936593a086b
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/JetsonTX2
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_BASE                     = 0x96000000
  FLASH_SIZE                     = 0x00120000
  FLASH_DEFINITION               = Platform/NVIDIA/Jetson/Jetson.fdf

!include Platform/NVIDIA/Jetson/Jetson.dsc.inc

[LibraryClasses.common]
  SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf

[PcdsFixedAtBuild.common]

  # System Memory (2GB - 16MB of Trusted DRAM at the top of the 32bit address space)
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  gArmTokenSpaceGuid.PcdSystemMemorySize|0x40000000

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|4

  gArmTokenSpaceGuid.PcdVFPEnabled|1

  ## UART 16550 parameters
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultBaudRate|115200
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialClockRate|407347200

  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialUseMmio|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterBase|0x03100000
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterStride|4
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialUseHardwareFlowControl|TRUE


  #
  # Tegra SDHCI controller base address
  #
  gNVIDIATokenSpaceGuid.PcdSdhciBaseAddress|0x03460000

  ## SBSA Watchdog Count
  gArmPlatformTokenSpaceGuid.PcdWatchdogCount|2

  #
  # ARM Generic Interrupt Controller
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x03881000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x03882000

  #
  # ARM Architectural Timer Frequency
  #
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|31250000
  gEmbeddedTokenSpaceGuid.PcdMetronomeTickPeriod|1000

