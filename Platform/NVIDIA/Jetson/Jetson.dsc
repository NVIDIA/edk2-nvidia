#
#  Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME               = Jetson
  PLATFORM_GUID               = b175f7b7-0cb0-446e-b338-0e0d0f688de8
  OUTPUT_DIRECTORY            = Build/Jetson
  FLASH_DEFINITION            = Platform/NVIDIA/Jetson/Jetson.fdf

[SkuIds]
  0|DEFAULT
  1|T194
  2|T234
  3|T234SLT|T234

!include Platform/NVIDIA/Jetson/Jetson.dsc.inc

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsFixedAtBuild]
  gNVIDIATokenSpaceGuid.PcdPlatformFamilyName|L"Jetson"
