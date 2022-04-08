/** @file

  Configuration Manager library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
