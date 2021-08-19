/** @file

  Configuration Manager library

  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
**/

#ifndef __CONFIGURATION_MANAGER_LIB_H_
#define __CONFIGURATION_MANAGER_LIB_H_

#include <Uefi/UefiBaseType.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

// GIC declarations

extern CM_ARM_GICC_INFO *GicCInfo;

CM_OBJECT_TOKEN
EFIAPI
GetGicCToken (
  UINTN
);

EFI_STATUS
EFIAPI
UpdateGicInfo (
  EDKII_PLATFORM_REPOSITORY_INFO **PlatformRepositoryInfo
);

#endif /* __CONFIGURATION_MANAGER_LIB_H_ */
