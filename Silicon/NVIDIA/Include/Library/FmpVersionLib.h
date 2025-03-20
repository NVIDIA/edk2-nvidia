/** @file

  FMP version library

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FMP_VERSION_LIB_H__
#define __FMP_VERSION_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Function called back when version is ready.  Note that the callback may be
  called during FmpVersionLibInit() or after FmpVersionLibInit() returns.

  @param[in]  Status            EFI_SUCCESS if version is ready, otherwise error.

  @retval None

**/
typedef
VOID
(EFIAPI *FMP_VERSION_READY_CALLBACK)(
  EFI_STATUS Status
  );

/**
  Get FMP version.

  @param[in]  Version           Pointer to return FMP version.
  @param[in]  VersionString     Pointer to return FMP version string.

  @retval EFI_SUCCESS           FMP version retrieved successfully.

**/
EFI_STATUS
EFIAPI
FmpVersionGet (
  OUT UINT32 *Version, OPTIONAL
  OUT CHAR16  **VersionString   OPTIONAL
  );

/**
  Initialize FMP version library.  Must be called before any other library
  API is used.

  @param[in]  ActiveBootChain   Active boot chain.
  @param[in]  Callback          Pointer to callback function.

  @retval EFI_SUCCESS           FMP version library initialized successfully.
  @retval others                An error was detected.

**/
EFI_STATUS
EFIAPI
FmpVersionLibInit (
  UINT32                      ActiveBootChain,
  FMP_VERSION_READY_CALLBACK  Callback
  );

#endif
