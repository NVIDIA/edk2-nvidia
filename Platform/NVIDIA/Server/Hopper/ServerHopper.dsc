#
#  Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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
  PLATFORM_NAME                  = ServerHopper
  PLATFORM_GUID                  = 25cdda40-4cf9-44e9-97f1-b0a0f5fa7b9c
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
!if $(SIM)
  OUTPUT_DIRECTORY               = Build/ServerHopperSim
!else
  OUTPUT_DIRECTORY               = Build/ServerHopper
!endif
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/NVIDIA/Server/Server.fdf
  CHIPSET                        = TH500
  DEVICE_TREE_BUILD              = TRUE
  DEFINE VARIABLES               = "STORAGE"

  #
  # Define ESRT GUIDs for Firmware Management Protocol instances
  #
  DEFINE SYSTEM_FMP_ESRT_GUID   = fa4dedfc-7582-4fa2-ab5e-ac11ba1719ba

!include Platform/NVIDIA/Server/Server.dsc.inc

[LibraryClasses.common]

  UsbFirmwareLib|Silicon/NVIDIA/TH500/Library/UsbFirmwareLib/UsbFirmwareLib.inf

[PcdsFixedAtBuild.common]

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|4

  gArmTokenSpaceGuid.PcdVFPEnabled|1

  ## SBSA Watchdog Count
  gArmPlatformTokenSpaceGuid.PcdWatchdogCount|2

  #
  # ARM Architectural Timer Frequency
  #
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|31250000
  gEmbeddedTokenSpaceGuid.PcdMetronomeTickPeriod|1000

  # System FMP Capsule GUID bf0d4599-20d4-414e-b2c5-3595b1cda402
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID(bf0d4599-20d4-414e-b2c5-3595b1cda402)}
