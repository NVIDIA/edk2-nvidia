/** @file
  Multiple Gray Logo DXE Driver logo images

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "LogoPrivate.h"

EFI_IMAGE_ID  mLogoImageId[] = {
  IMAGE_TOKEN (IMG_LOGO_0),
  IMAGE_TOKEN (IMG_LOGO_1),
  IMAGE_TOKEN (IMG_LOGO_2)
};

UINTN  mLogoImageIdCount = sizeof (mLogoImageId) / sizeof (mLogoImageId[0]);
