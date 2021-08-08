/** @file

  Crc8 Provides CRC-8 features

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

#ifndef __CRC8LIB_H__
#define __CRC8LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define TYPE_CRC8       0 //x^8 + x^2 + x^1 + 1
#define TYPE_CRC8_MAXIM 1 //x^8 + x^5 + x^4 + 1


/**
  Calculates CRC-8 for input buffer.

  @param[in]  Buffer               A pointer to the data buffer.
  @param[in]  Size                 Size of buffer.
  @param[in]  Seed                 Seed of the CRC to use
  @param[in]  Type                 Type of the CRC to use

  @return the CRC-8 value.
**/
UINT8
EFIAPI
CalculateCrc8 (
  IN UINT8  *Buffer,
  IN UINT16 Size,
  IN UINT8  Seed,
  IN UINT8  Type
);

#endif
