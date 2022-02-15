/** @file

  PINMUX Driver private structures

  Copyright (c) 2019-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
