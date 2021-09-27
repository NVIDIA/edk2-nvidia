#
#  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
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
# Portions provided under the following terms:
# Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.
#
# SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary

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

  #
  # Define ESRT GUIDs for Firmware Management Protocol instances
  #
  DEFINE SYSTEM_FMP_ESRT_GUID = 7C374309-1649-4682-8BEE-04F3A8399414

!include Platform/NVIDIA/Jetson/Jetson.dsc.inc

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsFixedAtBuild]
  gNVIDIATokenSpaceGuid.PcdPlatformFamilyName|L"Jetson"
