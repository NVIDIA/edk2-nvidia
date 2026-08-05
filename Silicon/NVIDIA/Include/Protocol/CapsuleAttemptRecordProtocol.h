/** @file
  NVIDIA capsule attempt record protocol.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_H__
#define __NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

#define NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_GUID  \
  { 0xada0d916, 0xc07e, 0x489f,                     \
    { 0x93, 0x33, 0x7f, 0x44, 0x8e, 0x82, 0x2c, 0x17 } }

#define NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_REVISION  0x00020000

typedef enum {
  CapsuleAttemptRecordSourceUnknown = 0,
  CapsuleAttemptRecordSourceAndroidFastboot,
  CapsuleAttemptRecordSourceCapsuleOnDisk,
  CapsuleAttemptRecordSourceSavedCapsule,
  CapsuleAttemptRecordSourceMax
} CAPSULE_ATTEMPT_RECORD_SOURCE;

typedef struct _NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL
  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL;

/**
  Update the capsule attempt record with the current FMP image information.

  The protocol collects the scalar image information returned by FMP
  GetImageInfo() for the configured system firmware image type and persists a
  new record. Pointer fields returned in an FMP image descriptor are not
  persisted.

  This function must be called at TPL_APPLICATION.

  @param[in] This  A pointer to the protocol instance.

  @retval EFI_SUCCESS            The record was updated.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_UNSUPPORTED        Capsule attempt record support is disabled.
  @retval Others                 The record could not be collected or persisted.

**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_CAPSULE_ATTEMPT_RECORD_UPDATE_FMP_IMAGE_INFO)(
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This
  );

/**
  Dump the newest valid capsule attempt record.

  The record is read and parsed for debug output without modifying persistent
  storage.

  This function must be called at TPL_APPLICATION.

  @param[in] This  A pointer to the protocol instance.

  @retval EFI_SUCCESS            The newest valid record was dumped.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_UNSUPPORTED        Capsule attempt record support is disabled.
  @retval EFI_NOT_FOUND          No valid capsule attempt record was found.
  @retval Others                 The record could not be read or parsed.

**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_CAPSULE_ATTEMPT_RECORD_DUMP_LATEST)(
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This
  );

/**
  Persist an in-progress capsule attempt record.

  @param[in] This    A pointer to the protocol instance.
  @param[in] Source  The producer or capsule-processing context.

  @retval EFI_SUCCESS            The marker was persisted.
  @retval EFI_INVALID_PARAMETER  This is NULL or Source is invalid.
  @retval Others                 The marker could not be persisted.

**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_CAPSULE_ATTEMPT_RECORD_BEGIN)(
  IN  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE           Source
  );

/**
  Persist a completed capsule attempt record with current FMP information.

  @param[in] This    A pointer to the protocol instance.
  @param[in] Source  The producer or capsule-processing context.

  @retval EFI_SUCCESS            The record was persisted.
  @retval EFI_INVALID_PARAMETER  This is NULL or Source is invalid.
  @retval Others                 The record could not be persisted.

**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_CAPSULE_ATTEMPT_RECORD_FINALIZE)(
  IN  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE           Source
  );

///
/// Protocol used by executable modules to access the singleton capsule attempt
/// record service.
///
struct _NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL {
  UINT32                                                 Revision;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_UPDATE_FMP_IMAGE_INFO    UpdateFmpImageInfo;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_DUMP_LATEST              DumpLatest;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_BEGIN                    Begin;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_FINALIZE                 Finalize;
};

extern EFI_GUID  gNVIDIACapsuleAttemptRecordProtocolGuid;

#endif
