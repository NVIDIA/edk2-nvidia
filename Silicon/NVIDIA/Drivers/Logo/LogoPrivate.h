/** @file

  Logo Driver Private Data

  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2018-2019 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018-2019 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

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
