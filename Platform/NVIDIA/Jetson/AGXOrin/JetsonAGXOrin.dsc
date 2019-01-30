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
  PLATFORM_NAME                  = JetsonAGXOrin
  PLATFORM_GUID                  = c989d372-6b8c-40bc-85b2-5d6d71e0f671
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
!if $(SIM)
  OUTPUT_DIRECTORY               = Build/JetsonAGXOrinSim
!else
  OUTPUT_DIRECTORY               = Build/JetsonAGXOrin
!endif
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/NVIDIA/Jetson/Jetson.fdf

!include Platform/NVIDIA/Jetson/Jetson.dsc.inc

[LibraryClasses.common]
!if $(SIM)
  SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf
!else
  SerialPortLib|Silicon/NVIDIA/Library/TegraCombinedSerialPort/TegraCombinedSerialPortLib.inf
!endif
  SystemResourceLib|Silicon/NVIDIA/T234/Library/SystemResourceLib/SystemResourceLib.inf

[PcdsFixedAtBuild.common]

  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|4

  gArmTokenSpaceGuid.PcdVFPEnabled|1

  ## TCUart - Serial Terminal
  gNVIDIATokenSpaceGuid.PcdTegraCombinedUartRxMailbox|0x03C10000
  gNVIDIATokenSpaceGuid.PcdTegraCombinedUartTxMailbox|0x0C168000

  ## UART 16550 parameters
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultBaudRate|115200
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialClockRate|407347200

  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialUseMmio|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterBase|0x03100000
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterStride|4
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialUseHardwareFlowControl|TRUE

  gNVIDIATokenSpaceGuid.PcdBootloaderInfoLocationAddress|0x0C3903F8
  gNVIDIATokenSpaceGuid.PcdBootloaderCarveoutOffset|0x398

  ## SBSA Watchdog Count
  gArmPlatformTokenSpaceGuid.PcdWatchdogCount|2

  #
  # ARM Generic Interrupt Controller
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x0f400000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x0f440000

  #
  # ARM Architectural Timer Frequency
  #
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|31250000
  gEmbeddedTokenSpaceGuid.PcdMetronomeTickPeriod|1000

[PcdsDynamicHii.common.DEFAULT]
!if $(SIM)
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|2
!endif

