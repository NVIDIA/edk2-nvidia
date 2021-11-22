/** @file

  FW Image Library

  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __FW_IMAGE_LIB_H__
#define __FW_IMAGE_LIB_H__

#include <Protocol/FwImageProtocol.h>

/**
  Find the NVIDIA_FW_IMAGE_PROTOCOL structure for the given image name.

  @param[in]  Name              Image name

  @retval NULL                  Image name not found
  @retval non-NULL              Pointer to the image protocol structure

**/
NVIDIA_FW_IMAGE_PROTOCOL *
EFIAPI
FwImageFindProtocol (
  CONST CHAR16                  *Name
  );

/**
  Get the number of NVIDIA_FW_IMAGE_PROTOCOL structures available.

  @retval UINTN                 Number of protocol structures

**/
UINTN
EFIAPI
FwImageGetCount (
  VOID
  );

/**
  Get a pointer to the first element of the NVIDIA_FW_IMAGE_PROTOCOL array.

  @retval NVIDIA_FW_IMAGE_PROTOCOL     Pointer to first protocol structure

**/
NVIDIA_FW_IMAGE_PROTOCOL **
EFIAPI
FwImageGetProtocolArray (
  VOID
  );

#endif
