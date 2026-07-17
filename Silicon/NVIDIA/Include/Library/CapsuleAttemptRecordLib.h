/** @file
  Capsule attempt record client library.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CAPSULE_ATTEMPT_RECORD_LIB_H__
#define __CAPSULE_ATTEMPT_RECORD_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Update the capsule attempt record with the current FMP image information.

  The library forwards the request to the capsule attempt record protocol. The
  protocol collects the scalar image information returned by FMP GetImageInfo()
  for the configured system firmware image type and persists a new record.

  @retval EFI_SUCCESS      The record was updated, or record support is disabled.
  @retval EFI_NOT_READY    The capsule attempt record protocol is unavailable.
  @retval Others           The protocol could not collect or persist the record.

**/
EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateFmpImageInfo (
  VOID
  );

/**
  Dump the newest valid capsule attempt record.

  The library forwards the request to the capsule attempt record protocol. The
  record is read and parsed for debug output without modifying persistent
  storage.

  @retval EFI_SUCCESS      The newest valid record was dumped, or record support
                           is disabled.
  @retval EFI_NOT_READY    The capsule attempt record protocol is unavailable.
  @retval EFI_NOT_FOUND    No valid capsule attempt record was found.
  @retval Others           The record could not be read or parsed.

**/
EFI_STATUS
EFIAPI
CapsuleAttemptRecordDumpLatest (
  VOID
  );

#endif
