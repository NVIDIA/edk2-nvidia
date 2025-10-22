/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/**
  Update Device Tree display node with maximum dispclk/hubclk rates.

  This function does not allocate any memory, hence it is safe to call
  during ExitBootServices.

  @param[in,opt] DeviceTree           Base of the Device Tree to update.
  @param[in]     DisplayNodePath      Path to the display node.
  @param[in]     MaxDispClkRateKhz    Maximum dispclk rates in kHz.
  @param[in]     MaxDispClkRateCount  Number of maximum dispclk rates.
  @param[in]     MaxHubClkRateKhz     Maximum hubclk rates in kHz.
  @param[in]     MaxHubClkRateCount   Number of maximum hubclk rates.

  @retval EFI_SUCCESS            Node successfully updated.
  @retval EFI_INVALID_PARAMETER  DisplayNodePath is NULL.
  @retval EFI_INVALID_PARAMETER  MaxDispClkRateCount is non-zero, but
                                 MaxDispClkRateKhz is NULL.
  @retval EFI_INVALID_PARAMETER  MaxHubClkRateCount is non-zero, but
                                 MaxHubClkRateKhz is NULL.
  @retval EFI_OUT_OF_RESOURCES   MaxDispClkRateCount is too large.
  @retval EFI_OUT_OF_RESOURCES   MaxHubClkRateCount is too large.
  @retval EFI_NOT_FOUND          Node specified by DevicePath not found.
  @retval EFI_DEVICE_ERROR       Other errors.
*/
EFI_STATUS
EFIAPI
DisplayDeviceTreeUpdateMaxClockRates (
  IN VOID          *DeviceTree  OPTIONAL,
  IN CONST CHAR8   *DisplayNodePath,
  IN CONST UINT32  *MaxDispClkRateKhz,
  IN UINTN         MaxDispClkRateCount,
  IN CONST UINT32  *MaxHubClkRateKhz,
  IN UINTN         MaxHubClkRateCount
  );

/**
  Update Device Tree display node with allocated ISO bandwidth and
  memory clock floor.

  This function does not allocate any memory, hence it is safe to call
  during ExitBootServices.

  @param[in,opt] DeviceTree                 Base of the Device Tree to update.
  @param[in]     DisplayNodePath            Path to the display node.
  @param[in]     IsoBandwidthKbytesPerSec   Requested ISO bandwidth.
  @param[in]     MemclockFloorKbytesPerSec  Requested memory clock floor.

  @retval EFI_SUCCESS            Node successfully updated.
  @retval EFI_INVALID_PARAMETER  DisplayNodePath is NULL.
  @retval EFI_NOT_FOUND          Node specified by DevicePath not found.
  @retval EFI_DEVICE_ERROR       Other errors.
*/
EFI_STATUS
EFIAPI
DisplayDeviceTreeUpdateIsoBandwidth (
  IN VOID         *DeviceTree  OPTIONAL,
  IN CONST CHAR8  *DisplayNodePath,
  IN UINT32       IsoBandwidthKbytesPerSec,
  IN UINT32       MemclockFloorKbytesPerSec
  );

#endif //__DISPLAY_DEVICE_TREE_HELPER_LIB_H__
