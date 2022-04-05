/** @file

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2011-2018, ARM Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef T194_GRAPHICS_OUTPUT_DXE_H_
#define T194_GRAPHICS_OUTPUT_DXE_H_

#include <Base.h>

#include <Library/DebugLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>

#define WINBUF_START_ADDR_LO_OFFSET             0x2000
#define WINBUF_START_ADDR_HI_OFFSET             0x2034
#define WIN_COLOR_DEPTH_OFFSET                  0x2e0c
#define WIN_COLOR_DEPTH_B8G8R8A8                0xc
#define WIN_COLOR_DEPTH_R8G8B8A8                0xd
#define PCALC_WINDOW_SET_CROPPED_SIZE_IN_OFFSET 0x2e18
#define DC_A_WIN_AD_WIN_OPTIONS_OFFSET          0x2e00

typedef struct {
  UINT32                                  Signature;
  EFI_HANDLE                              Handle;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    ModeInfo;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE       Mode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL            Gop;
  FRAME_BUFFER_CONFIGURE                 *Configure;
} GOP_INSTANCE;

#define GOP_INSTANCE_SIGNATURE  SIGNATURE_32('g', 'o', 'p', '0')

#define GOP_INSTANCE_FROM_GOP_THIS(a)  CR (a, GOP_INSTANCE, Gop, GOP_INSTANCE_SIGNATURE)

#endif /* T194_GRAPHICS_OUTPUT_DXE_H_ */
