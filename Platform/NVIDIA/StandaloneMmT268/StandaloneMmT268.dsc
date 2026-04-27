#
#  Copyright (c) 2018, ARM Limited. All rights reserved.
#  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = StandaloneMmT268
  PLATFORM_GUID                  = a9eca421-f19b-41c4-b8c2-7b003d56d68f
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/StandaloneMmT268
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = ALL
  FLASH_DEFINITION               = Platform/NVIDIA/StandaloneMmJetson/StandaloneMmJetson.fdf

!include Platform/NVIDIA/StandaloneMmJetson/StandaloneMmJetson.dsc.inc

[PcdsFixedAtBuild]
  gNVIDIATokenSpaceGuid.PcdT26xCpublSocketCount|2
