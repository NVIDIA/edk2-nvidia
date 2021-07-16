/** @file

  PINMUX Driver private structures

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

#ifndef PINMUXDXE_PRIVATE_H_
#define PINMUXDXE_PRIVATE_H_

#include <PiDxe.h>
#include <Protocol/PinMux.h>

#define PINMUX_SIGNATURE SIGNATURE_32('P','M','U','X')
typedef struct {
  UINT32                     Signature;
  NVIDIA_PINMUX_PROTOCOL     PinMuxProtocol;
  EFI_PHYSICAL_ADDRESS       BaseAddress;
  UINTN                      RegionSize;
  EFI_HANDLE                 ImageHandle;
} PINMUX_DXE_PRIVATE;
#define PINMUX_PRIVATE_DATA_FROM_THIS(a)      CR(a, PINMUX_DXE_PRIVATE, PinMuxProtocol, PINMUX_SIGNATURE)
#define PINMUX_PRIVATE_DATA_FROM_PROTOCOL(a)  PINMUX_PRIVATE_DATA_FROM_THIS(a)

#endif /* PINMUXDXE_PRIVATE_H_ */
