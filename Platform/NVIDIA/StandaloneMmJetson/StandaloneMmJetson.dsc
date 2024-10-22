#
#  Copyright (c) 2018, ARM Limited. All rights reserved.
#  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = StandaloneMmJetson
  PLATFORM_GUID                  = c8223aa2-d291-4381-870a-8ac826dc0c94
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/StandaloneMmJetson
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = ALL
  FLASH_DEFINITION               = Platform/NVIDIA/StandaloneMmJetson/StandaloneMmJetson.fdf

!include Platform/NVIDIA/StandaloneMmJetson/StandaloneMmJetson.dsc.inc
