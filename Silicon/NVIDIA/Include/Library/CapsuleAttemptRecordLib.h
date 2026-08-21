/** @file
  Capsule attempt record client library.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CAPSULE_ATTEMPT_RECORD_LIB_H__
#define __CAPSULE_ATTEMPT_RECORD_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/CapsuleAttemptRecordProtocol.h>

/**
  Persist a durable marker that a capsule attempt is starting.

  @param[in] Source  The producer or capsule-processing context.

  @retval EFI_SUCCESS            The marker was persisted, or support is disabled.
  @retval EFI_INVALID_PARAMETER  Source is not a supported producer.
  @retval EFI_NOT_READY          The driver protocol is unavailable.
  @retval Others                 The marker could not be persisted.

**/
EFI_STATUS
EFIAPI
CapsuleAttemptRecordBegin (
  IN CAPSULE_ATTEMPT_RECORD_SOURCE  Source
  );

/**
  Persist the completed capsule attempt and current FMP image information.

  @param[in] Source  The producer or capsule-processing context.

  @retval EFI_SUCCESS            The record was persisted, or support is disabled.
  @retval EFI_INVALID_PARAMETER  Source is not a supported producer.
  @retval EFI_NOT_READY          The driver protocol is unavailable.
  @retval Others                 The record could not be persisted.

**/
EFI_STATUS
EFIAPI
CapsuleAttemptRecordFinalize (
  IN CAPSULE_ATTEMPT_RECORD_SOURCE  Source
  );

/**
  Calculate the SHA-256 digest used to identify an FMP image.

  @param[in]  FmpImage        FMP image buffer.
  @param[in]  FmpImageSize    Size of FmpImage in bytes.
  @param[out] FmpImageDigest  SHA-256 digest buffer.

  @retval TRUE   The digest was calculated.
  @retval FALSE  Record support is disabled, a parameter is invalid, or the
                 digest could not be calculated.

**/
BOOLEAN
EFIAPI
CapsuleAttemptRecordCalculateFmpImageDigest (
  IN CONST VOID  *FmpImage,
  IN UINTN       FmpImageSize,
  OUT UINT8      FmpImageDigest[CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE]
  );

/**
  Persist the latest FMP diagnostic for the active attempt.

  @param[in] CheckImage    TRUE for FmpTegraCheckImage; FALSE for
                           FmpTegraSetImage.
  @param[in] FailedImage   Failed image name, or NULL when no image applies.
  @param[in] RawStatus     Raw status returned by the failing operation.
  @param[in] NvidiaStatus  Detailed NVIDIA last-attempt status.
  @param[in] PlatformSpec  Tegra platform specification, or NULL.
  @param[in] TnSpec        Compatible TnSpec, or NULL.
  @param[in] FmpImageDigest  SHA-256 digest of the FMP image, or NULL.
  @param[in] Completion    Current SetImage completion percentage, or NULL for
                           a producer that does not report completion.
  @param[in] TargetBootChain  Boot chain targeted by SetImage, or NULL when no
                              target chain was selected.

  @retval EFI_SUCCESS            The diagnostic was persisted, or support is disabled.
  @retval EFI_INVALID_PARAMETER  A string is too long, the completion
                                 percentage exceeds 100, or the target chain
                                 is invalid for the producer.
  @retval EFI_NOT_READY          The protocol or active attempt is unavailable.
  @retval Others                 The diagnostic could not be persisted.

**/
EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateFmpDiagnostics (
  IN BOOLEAN       CheckImage,
  IN CONST CHAR16  *FailedImage OPTIONAL,
  IN EFI_STATUS    RawStatus,
  IN UINT32        NvidiaStatus,
  IN CONST CHAR8   *PlatformSpec OPTIONAL,
  IN CONST CHAR8   *TnSpec OPTIONAL,
  IN CONST UINT8   *FmpImageDigest OPTIONAL,
  IN CONST UINT32  *Completion OPTIONAL,
  IN CONST UINT32  *TargetBootChain OPTIONAL
  );

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
