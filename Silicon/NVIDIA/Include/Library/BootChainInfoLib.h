/** @file

  Boot Chain Information Library

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __BOOTCHAININFOLIB_H__
#define __BOOTCHAININFOLIB_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define MAX_PARTITION_NAME_LEN      36

/**
  Retrieve Active Boot Chain Partition Name

**/
EFI_STATUS
EFIAPI
GetActivePartitionName (
  IN  CONST CHAR16 *GeneralPartitionName,
  OUT CHAR16       *ActivePartitionName
);

#endif
