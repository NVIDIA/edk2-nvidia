#
#  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
#  Copyright (c) 2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = StandaloneMmOptee
  PLATFORM_GUID                  = 6f1dc7c3-1b41-4b4d-bf53-9023bafac4cc
  OUTPUT_DIRECTORY               = Build/StandaloneMmOptee
  FLASH_DEFINITION               = Platform/NVIDIA/StandaloneMmOptee/StandaloneMmOptee.fdf

!include Platform/NVIDIA/StandaloneMmOptee/StandaloneMmOptee.dsc.inc
