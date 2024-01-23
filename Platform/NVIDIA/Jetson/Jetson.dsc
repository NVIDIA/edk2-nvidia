#
#  SPDX-FileCopyrightText: Copyright (c) 2018-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME               = Jetson
  PLATFORM_GUID               = b175f7b7-0cb0-446e-b338-0e0d0f688de8
  FLASH_DEFINITION            = Platform/NVIDIA/NVIDIA.common.fdf

!include Platform/NVIDIA/NVIDIA.common.dsc.inc
