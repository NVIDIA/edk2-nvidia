/** @file

  Tegra Se RNG Driver private structures

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __SE_RNG_PRIVATE_H__
#define __SE_RNG_PRIVATE_H__

#include <Protocol/SeRngProtocol.h>
#include <PiDxe.h>

#define SE_RNG_SIGNATURE SIGNATURE_32('S','E','R','N')

typedef struct {
  UINT32                           Signature;
  UINT64                           BaseAddress;
  NVIDIA_SE_RNG_PROTOCOL           SeRngProtocol;
} SE_RNG_PRIVATE_DATA;

#define SE_MAX_POLL_COUNT          0x08000000U
#define RANDOM_BYTES               16U

#define SE_RNG_PRIVATE_DATA_FROM_THIS(a)   CR(a, SE_RNG_PRIVATE_DATA, SeRngProtocol, SE_RNG_SIGNATURE)

#define SE0_AES0_CONFIG_0                  0x1004

#define SE0_AES0_CONFIG_0_DST_SHIFT        2
#define SE0_AES0_CONFIG_0_DST_MEMORY       (0 << SE0_AES0_CONFIG_0_DST_SHIFT)

#define SE0_AES0_CONFIG_0_DEC_ALG_SHIFT    8
#define SE0_AES0_CONFIG_0_DEC_ALG_NOP      (0 << SE0_AES0_CONFIG_0_DEC_ALG_SHIFT)
#define SE0_AES0_CONFIG_0_ENC_ALG_SHIFT    12
#define SE0_AES0_CONFIG_0_ENC_ALG_RNG      (2 << SE0_AES0_CONFIG_0_ENC_ALG_SHIFT)


#define SE0_AES0_OUT_ADDR_0                0x1014
#define SE0_AES0_OUT_ADDR_HI_0             0x1018
#define SE0_AES0_OUT_ADDR_HI_0_SZ_SHIFT    0
#define SE0_AES0_OUT_ADDR_HI_0_SZ_MASK     0x00FFFFFF
#define SE0_AES0_OUT_ADDR_HI_0_MSB_SHIFT   24
#define SE0_AES0_OUT_ADDR_HI_0_MSB_MASK    0xFF000000

#define SE0_AES0_CRYPTO_LAST_BLOCK_0       0x102c

#define SE0_AES0_OPERATION_0               0x1038
#define SE0_AES0_OPERATION_0_LASTBUF_TRUE  BIT16
#define SE0_AES0_OPERATION_0_OP_START      1

#define SE0_AES0_STATUS_0                  0x10f4


#endif
