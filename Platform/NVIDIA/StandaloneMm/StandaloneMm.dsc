#
#  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
#  Copyright (c) 2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = StandaloneMm
  PLATFORM_GUID                  = c8223aa2-d291-4381-870a-8ac826dc0c94
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/StandaloneMm
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = ALL
  FLASH_DEFINITION               = Platform/NVIDIA/StandaloneMm/StandaloneMm.fdf

!include Platform/NVIDIA/StandaloneMm/StandaloneMm.dsc.inc
