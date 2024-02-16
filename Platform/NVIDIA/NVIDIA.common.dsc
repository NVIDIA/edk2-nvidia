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
  PLATFORM_NAME               = $(BUILD_NAME)
  PLATFORM_GUID               = $(BUILD_GUID)
  FLASH_DEFINITION            = Platform/NVIDIA/NVIDIA.common.fdf

!include Platform/NVIDIA/NVIDIA.common.dsc.inc
