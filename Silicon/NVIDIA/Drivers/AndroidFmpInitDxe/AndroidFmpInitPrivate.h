/** @file
  AndroidFmpInit Dxe private header.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __ANDROID_FMP_INIT_PRIVATE_H__
#define __ANDROID_FMP_INIT_PRIVATE_H__

#include <Uefi.h>
#include <Protocol/HiiImageEx.h>

//
// HII image IDs for OTA splash variants. Resolved from AndroidFmpInit.idf at
// build time via IMAGE_TOKEN().
//
extern EFI_IMAGE_ID  mOtaImageId4k;
extern EFI_IMAGE_ID  mOtaImageId1080;

#endif // __ANDROID_FMP_INIT_PRIVATE_H__
