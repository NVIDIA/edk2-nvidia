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
// Image indices
//
typedef enum {
  AVB_UI_IMAGE_ORANGE,
  AVB_UI_IMAGE_YELLOW,
  AVB_UI_IMAGE_RED_STOP,
  AVB_UI_IMAGE_RED_EIO,
  AVB_UI_IMAGE_COUNT
} AVB_UI_IMAGE_INDEX;

extern EFI_IMAGE_ID  mAvbUiImageId[AVB_UI_IMAGE_COUNT];

#endif // __AVB_UI_PRIVATE_H__
