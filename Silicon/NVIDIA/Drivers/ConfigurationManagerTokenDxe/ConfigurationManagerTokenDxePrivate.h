/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __CONFIGURATION_MANAGER_TOKEN_DXE_PRIVATE_H__
#define __CONFIGURATION_MANAGER_TOKEN_DXE_PRIVATE_H__

#include <Protocol/ConfigurationManagerTokenProtocol.h>

typedef struct {
  UINT32                                         Signature;
  UINT32                                         NextToken;
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL    ConfigurationManagerTokenProtocol;
} NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA;

#define NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_SIGNATURE  SIGNATURE_32('C','M','T','P')

#define NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA_FROM_PROTOCOL(a)  CR(a, NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA, ConfigurationManagerTokenProtocol, NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_SIGNATURE)

#endif /* __CONFIGURATION_MANAGER_TOKEN_DXE_PRIVATE_H__ */
