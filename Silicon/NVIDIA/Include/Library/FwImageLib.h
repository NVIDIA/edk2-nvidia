/** @file

  FW Image Library

  Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FW_IMAGE_LIB_H__
#define __FW_IMAGE_LIB_H__

#include <Protocol/FwImageProtocol.h>

/**
  Function called back when FwImage array is updated.

  @retval None

**/
typedef
VOID
(EFIAPI *FW_IMAGE_ADDED_CALLBACK)(
  VOID
  );

/**
  Find the NVIDIA_FW_IMAGE_PROTOCOL structure for the given image name.

  @param[in]  Name              Image name

  @retval NULL                  Image name not found
  @retval non-NULL              Pointer to the image protocol structure

**/
NVIDIA_FW_IMAGE_PROTOCOL *
EFIAPI
FwImageFindProtocol (
  CONST CHAR16  *Name
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

/**
  Register function to call back when FwImage is added.

  @param[in]  Callback              Pointer to callback function or NULL to
                                    unregister previous callback function.

  @retval None

**/
VOID
EFIAPI
FwImageRegisterImageAddedCallback (
  FW_IMAGE_ADDED_CALLBACK  Callback
  );

#endif
