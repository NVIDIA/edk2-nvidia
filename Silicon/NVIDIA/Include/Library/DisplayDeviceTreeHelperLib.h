/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __DISPLAY_DEVICE_TREE_HELPER_LIB_H__
#define __DISPLAY_DEVICE_TREE_HELPER_LIB_H__

#include <Uefi/UefiBaseType.h>

#include <Protocol/GraphicsOutput.h>

/**
   Updates Device Tree simple-framebuffer node(s) with details about
   the given graphics output mode and framebuffer region.

   @param[in] DeviceTree       Base of the Device Tree to update.
   @param[in] ModeInfo         Pointer to the mode information to use.
   @param[in] FrameBufferBase  Base address of the framebuffer region.
   @param[in] FrameBufferSize  Size of the framebuffer region.

   @retval TRUE   Update successful.
   @retval FALSE  Update failed.
*/
BOOLEAN
UpdateDeviceTreeSimpleFramebufferInfo (
  IN VOID                                        *DeviceTree,
  IN CONST EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *ModeInfo,
  IN UINT64                                      FrameBufferBase,
  IN UINT64                                      FrameBufferSize
  );

#endif //__DISPLAY_DEVICE_TREE_HELPER_LIB_H__
