/** @file

  FMP erot library private header file

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FMP_EROT_LIB_PRIVATE_H__
#define __FMP_EROT_LIB_PRIVATE_H__

#include <Uefi/UefiBaseType.h>

#define FMP_EROT_NVIDIA_IANA_ID  0x1647UL

/**
  Get system firmware version and/or version string.

  @param[out] Version               Pointer to return version number. OPTIONAL
  @param[out] VersionString         Pointer to return version string. OPTIONAL

  @retval EFI_SUCCESS               No errors found.
  @retval Others                    Error detected.

**/
EFI_STATUS
EFIAPI
FmpErotGetVersion (
  OUT UINT32 *Version, OPTIONAL
  OUT CHAR16  **VersionString       OPTIONAL
  );

#endif
