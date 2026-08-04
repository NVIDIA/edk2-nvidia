/** @file
  AVB UI DXE Driver private header file.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __AVB_UI_PRIVATE_H__
#define __AVB_UI_PRIVATE_H__

#include <Uefi.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiImageEx.h>

//
// Asset resolution tiers. Only 720 and 1080 art exists; 4K/8K reuse 1080.
// Values match the 720/1080 offset within each AVB_UI_IMAGE_* pair.
//
typedef enum {
  AVB_UI_ASSET_720  = 0,
  AVB_UI_ASSET_1080 = 1
} AVB_UI_ASSET_TIER;

//
// Image indices
//
typedef enum {
  AVB_UI_IMAGE_ORANGE_720,
  AVB_UI_IMAGE_ORANGE_1080,
  AVB_UI_IMAGE_RED_STOP_720,
  AVB_UI_IMAGE_RED_STOP_1080,
  AVB_UI_IMAGE_RED_EIO_720,
  AVB_UI_IMAGE_RED_EIO_1080,
  AVB_UI_IMAGE_COUNT
} AVB_UI_IMAGE_INDEX;

extern EFI_IMAGE_ID  mAvbUiImageId[AVB_UI_IMAGE_COUNT];

#endif // __AVB_UI_PRIVATE_H__
