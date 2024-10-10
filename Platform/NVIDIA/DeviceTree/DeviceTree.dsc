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
  PLATFORM_GUID               = $(BUILD_GUID)

!include Platform/NVIDIA/NVIDIA.global.dsc.inc

[Defines]
  PLATFORM_NAME               = DeviceTree
  PLATFORM_VERSION            = 0.1
  DSC_SPECIFICATION           = 0x0001001B
  OUTPUT_DIRECTORY            = Build/DeviceTree
  SUPPORTED_ARCHITECTURES     = AARCH64
  BUILD_TARGETS               = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER            = ALL

[Components.common]
  #
  # Default Variables Device Tree overlay builds
  #
  Silicon/NVIDIA/Tegra/DeviceTree/DeviceTree.inf
