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

//
// Define for forward reference.
//
typedef struct _NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL;

// Allocate a TokenMap with tokens for a new entry
typedef
  EFI_STATUS(*NVIDIA_CONFIGURATION_MANAGER_TOKEN_ALLOCATE)(
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  *This,
  UINT32                                       TokenCount,
  CM_OBJECT_TOKEN                              **TokenMap
  );

// NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL protocol structure.
struct _NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL {
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_ALLOCATE    AllocateTokens;
};

#endif /* __CONFIGURATION_MANAGER_TOKEN_PROTOCOL_H__ */
