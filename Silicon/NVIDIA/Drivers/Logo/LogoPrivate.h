/** @file

  Logo Driver Private Data

  Copyright (c) 2018-2019, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef __LOGO_PRIVATE_H__
#define __LOGO_PRIVATE_H__

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PlatformLogo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/BmpSupportLib.h>

#define MAX_SUPPORTED_LOGO 10

typedef struct {
  VOID* Base;
  UINTN Size;
} NVIDIA_LOGO_INFO;

typedef struct {
  UINT32                              Signature;
  UINT32                              NumLogos;
  UINT32                              SupportedLogo;
  NVIDIA_LOGO_INFO                    LogoInfo[MAX_SUPPORTED_LOGO];
  EDKII_PLATFORM_LOGO_PROTOCOL        PlatformLogo;
} NVIDIA_LOGO_PRIVATE_DATA;

#define NVIDIA_LOGO_SIGNATURE                 SIGNATURE_32('L','O','G','O')
#define NVIDIA_LOGO_PRIVATE_DATA_FROM_THIS(a) CR(a, NVIDIA_LOGO_PRIVATE_DATA, PlatformLogo, NVIDIA_LOGO_SIGNATURE)

#endif
