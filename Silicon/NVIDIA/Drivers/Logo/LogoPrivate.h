/** @file
  Logo DXE Driver private header file

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __LOGO_PRIVATE_H__
#define __LOGO_PRIVATE_H__

#include <Uefi.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiImageEx.h>
#include <Protocol/HiiPackageList.h>

extern EFI_IMAGE_ID  mLogoImageId[];
extern UINTN         mLogoImageIdCount;

#endif
