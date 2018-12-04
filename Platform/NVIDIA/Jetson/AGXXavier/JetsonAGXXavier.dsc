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
  PLATFORM_NAME                  = JetsonAGXXavier
  PLATFORM_GUID                  = 865873a1-b255-46c2-90d2-2e2578c00dbd
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/JetsonAGXXavier
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_BASE                     = 0x96000000
  FLASH_SIZE                     = 0x00150000
  FLASH_DEFINITION               = Platform/NVIDIA/Jetson/Jetson.fdf

!include Platform/NVIDIA/Jetson/Jetson.dsc.inc

[LibraryClasses.common]
  SerialPortLib|Silicon/NVIDIA/Library/TegraCombinedSerialPort/TegraCombinedSerialPortLib.inf
  SystemResourceLib|Silicon/NVIDIA/T194/Library/SystemResourceLib/SystemResourceLib.inf

[PcdsFixedAtBuild.common]

  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000
  #Size value is not directly used, must be non-zero and cover the firmware volume region at least
  gArmTokenSpaceGuid.PcdSystemMemorySize|0x30000000

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|4

  gArmTokenSpaceGuid.PcdVFPEnabled|1

  ## TCUart - Serial Terminal
  gNVIDIATokenSpaceGuid.PcdTegraCombinedUartRxMailbox|0x03C10000
  gNVIDIATokenSpaceGuid.PcdTegraCombinedUartTxMailbox|0x0C168000

  gNVIDIATokenSpaceGuid.PcdBootloaderInfoLocationAddress|0x0C3903F8

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


