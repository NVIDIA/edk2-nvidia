/** @file
*
*  Configuration Manager Token Protocol definition.
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __CONFIGURATION_MANAGER_TOKEN_PROTOCOL_H__
#define __CONFIGURATION_MANAGER_TOKEN_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>
#include <Include/StandardNameSpaceObjects.h>
#include <Library/ConfigurationManagerDataLib.h>

//
// Define for forward reference. Must match ConfigurationManagerDataLib.h
//
typedef struct _NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL;
typedef struct PlatformRepositoryInfo                        EDKII_PLATFORM_REPOSITORY_INFO;

// Allocate a TokenMap with tokens for a new entry
typedef
  EFI_STATUS(*NVIDIA_CONFIGURATION_MANAGER_TOKEN_ALLOCATE)(
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  *This,
  UINT32                                       TokenCount,
  CM_OBJECT_TOKEN                              **TokenMap
  );

// Sanity check the TokenMaps in the repository
typedef
  EFI_STATUS(*NVIDIA_CONFIGURATION_MANAGER_TOKEN_SANITY_CHECK)(
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  *This,
  EDKII_PLATFORM_REPOSITORY_INFO               *Repo
  );

// NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL protocol structure.
struct _NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL {
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_ALLOCATE        AllocateTokens;
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_SANITY_CHECK    SanityCheck;
};

#endif /* __CONFIGURATION_MANAGER_TOKEN_PROTOCOL_H__ */
