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
  PLATFORM_NAME                  = JetsonAGXXavier
  PLATFORM_GUID                  = 865873a1-b255-46c2-90d2-2e2578c00dbd
  OUTPUT_DIRECTORY               = Build/JetsonAGXXavier
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/NVIDIA/Jetson/Jetson.fdf
  CHIPSET                        = T194

!include Platform/NVIDIA/Jetson/Jetson.dsc.inc

[LibraryClasses.common]

  UsbFirmwareLib|Silicon/NVIDIA/T194/Library/UsbFirmwareLib/UsbFirmwareLib.inf

[PcdsFixedAtBuild.common]

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|4

  gArmTokenSpaceGuid.PcdVFPEnabled|1

  #
  # ARM Architectural Timer Frequency
  #
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|31250000
  gEmbeddedTokenSpaceGuid.PcdMetronomeTickPeriod|1000

  # System FMP Capsule GUID be3f5d68-7654-4ed2-838c-2a2faf901a78
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID(be3f5d68-7654-4ed2-838c-2a2faf901a78)}
