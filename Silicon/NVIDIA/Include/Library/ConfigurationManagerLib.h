/** @file

  Configuration Manager library

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_MANAGER_LIB_H_
#define __CONFIGURATION_MANAGER_LIB_H_

#include <Uefi/UefiBaseType.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

EFI_STATUS
EFIAPI
RegisterProtocolBasedObjects (
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo,
  EDKII_PLATFORM_REPOSITORY_INFO  **CurrentPlatformRepositoryInfo
  );

#endif /* __CONFIGURATION_MANAGER_LIB_H_ */
