#
#  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
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
!if $(SIM)
  OUTPUT_DIRECTORY               = Build/JetsonAGXXavierSim
!else
  OUTPUT_DIRECTORY               = Build/JetsonAGXXavier
!endif
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/NVIDIA/Jetson/Jetson.fdf

  #
  # Define ESRT GUIDs for Firmware Management Protocol instances
  #
  DEFINE SYSTEM_FMP_ESRT_GUID   = be3f5d68-7654-4ed2-838c-2a2faf901a78

!include Platform/NVIDIA/Jetson/Jetson.dsc.inc

[LibraryClasses.common]
!if $(SIM)
  SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf
!else
  SerialPortLib|Silicon/NVIDIA/Library/TegraCombinedSerialPort/TegraCombinedSerialPortLib.inf
!endif
  SystemResourceLib|Silicon/NVIDIA/T194/Library/SystemResourceLib/SystemResourceLib.inf

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
  gNVIDIATokenSpaceGuid.PcdBootloaderCarveoutOffset|0x448

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

  # System FMP Capsule GUID be3f5d68-7654-4ed2-838c-2a2faf901a78
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID(be3f5d68-7654-4ed2-838c-2a2faf901a78)}

[PcdsFeatureFlag]
!if $(SIM)
  gEfiMdeModulePkgTokenSpaceGuid.PcdSdhciDisable26bitSupport|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSdhciDisableCidSupport|TRUE
!endif
