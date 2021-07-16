/** @file
  Tegra P2U (PIPE to UPHY) Control Protocol

  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __TEGRAP2U_PROTOCOL_H__
#define __TEGRAP2U_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_TEGRAP2U_PROTOCOL_GUID \
  { \
  0x3f9c6949, 0x817a, 0x45f7, { 0xb9, 0xb5, 0x9c, 0x94, 0x83, 0xee, 0xa0, 0x9e } \
  }

typedef struct _NVIDIA_TEGRAP2U_PROTOCOL NVIDIA_TEGRAP2U_PROTOCOL;

/**
 * This function Initializes specified Tegra P2U instance.
 *
 * @param[in] This                   The instance of the NVIDIA_TEGRAP2U_PROTOCOL.
 * @param[in] P2UId                  Id of the P2U instance to be Initialized.
 *
 * @return EFI_SUCCESS               P2U instance initialized.
 * @return EFI_NOT_FOUND             P2U instance ID is not supported.
 * @return EFI_DEVICE_ERROR          Other error occurred.
 */
typedef
EFI_STATUS
(EFIAPI *TEGRAP2U_INIT) (
  IN NVIDIA_TEGRAP2U_PROTOCOL  *This,
  IN UINT32                    P2UId
  );

/// NVIDIA_REGULATOR_PROTOCOL protocol structure.
struct _NVIDIA_TEGRAP2U_PROTOCOL {

  TEGRAP2U_INIT            Init;
};

extern EFI_GUID gNVIDIATegraP2UProtocolGuid;

#endif
