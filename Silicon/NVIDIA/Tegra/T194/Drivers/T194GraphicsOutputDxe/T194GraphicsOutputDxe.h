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

#define CORE_HEAD_SET_CONTROL_OUTPUT_LUT_SIZE(x)  BitFieldRead32 ((x), 1, 2)
#define CORE_HEAD_SET_CONTROL_OUTPUT_LUT_SIZE_257       0x0
#define CORE_HEAD_SET_CONTROL_OUTPUT_LUT_SIZE_1025      0x2
#define CORE_HEAD_SET_CONTROL_OUTPUT_LUT_OFFSET         0x10c4
#define COREPVT_HEAD_SET_OUTPUT_LUT_BASE_LO_OFFSET      0x10c8
#define COREPVT_HEAD_SET_OUTPUT_LUT_BASE_HI_OFFSET      0x10cc
#define DC_DISP_DISP_ACTIVE_OFFSET                      0x1024
#define DC_DISP_DISP_ACTIVE_RESET_VAL                   0x00040008
#define DC_PER_HEAD_OFFSET                              0x00010000
#define DC_HEAD_0_BASE_ADDR                             0x15200000
#define DC_HEAD_1_BASE_ADDR                             0x15210000
#define DC_HEAD_2_BASE_ADDR                             0x15220000
#define WIN_COLOR_DEPTH_OFFSET                          0x2e0c
#define WIN_COLOR_DEPTH_B8G8R8A8                        0xc
#define WIN_COLOR_DEPTH_R8G8B8A8                        0xd
#define DC_WIN_A_PCALC_WINDOW_SET_CROPPED_SIZE_IN_0     0x1c18
#define DC_A_WIN_AD_PCALC_WINDOW_SET_CROPPED_SIZE_IN_0  0x2e18
#define DC_A_WIN_AD_WIN_OPTIONS_OFFSET                  0x2e00
#define DC_A_WIN_AD_WIN_OPTIONS_AD_WIN_ENABLE_ENABLE    BIT30
#define DC_A_WINBUF_AD_START_ADDR_OFFSET                0x2f00
#define DC_A_WINBUF_AD_START_ADDR_HI_OFFSET             0x2f34

#define DC_PER_WINDOW_OFFSET  0xc00
#define WINDOW_INDEX_A        0x0
#define WINDOW_INDEX_B        0x1
#define WINDOW_INDEX_C        0x2
#define WINDOW_INDEX_D        0x3
#define WINDOW_INDEX_E        0x4
#define WINDOW_INDEX_F        0x5
#define WINDOW_INDEX_MAX      0x5

#define WIN_CROPPED_SIZE_IN_MIN_HEIGHT  600
#define WIN_CROPPED_SIZE_IN_MIN_WIDTH   800

typedef struct {
  UINT32                                  Signature;
  EFI_HANDLE                              Handle;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    ModeInfo;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE       Mode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL            Gop;
  FRAME_BUFFER_CONFIGURE                  *Configure;
  EFI_PHYSICAL_ADDRESS                    DcAddr;
} GOP_INSTANCE;

#define GOP_INSTANCE_SIGNATURE  SIGNATURE_32('g', 'o', 'p', '0')

#define GOP_INSTANCE_FROM_GOP_THIS(a)  CR (a, GOP_INSTANCE, Gop, GOP_INSTANCE_SIGNATURE)

#endif /* T194_GRAPHICS_OUTPUT_DXE_H_ */
