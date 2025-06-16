/** @file
  AVB UI Image Token Array.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "AvbUiPrivate.h"

EFI_IMAGE_ID  mAvbUiImageId[AVB_UI_IMAGE_COUNT] = {
  IMAGE_TOKEN (IMG_AVB_ORANGE),
  IMAGE_TOKEN (IMG_AVB_YELLOW),
  IMAGE_TOKEN (IMG_AVB_RED_STOP),
  IMAGE_TOKEN (IMG_AVB_RED_EIO)
};
