/** @file
  Capsule attempt record client library.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseCryptLib.h>
#include <Library/CapsuleAttemptRecordLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/CapsuleAttemptRecordProtocol.h>

BOOLEAN
EFIAPI
CapsuleAttemptRecordCalculateFmpImageDigest (
  IN CONST VOID  *FmpImage,
  IN UINTN       FmpImageSize,
  OUT UINT8      FmpImageDigest[CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE]
  )
{
  BOOLEAN  HashOk;

  if (!FeaturePcdGet (PcdCapsuleAttemptRecordSupported)) {
    return FALSE;
  }

  if ((FmpImage == NULL) || (FmpImageSize == 0) || (FmpImageDigest == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: invalid FMP image digest parameters\n", __FUNCTION__));
    return FALSE;
  }

  HashOk = Sha256HashAll (FmpImage, FmpImageSize, FmpImageDigest);
  if (HashOk) {
    DEBUG ((DEBUG_INFO, "%a: calculated SHA-256 over %Lu-byte FMP image\n", __FUNCTION__, (UINT64)FmpImageSize));
  } else {
    DEBUG ((DEBUG_INFO, "%a: SHA-256 failed for %Lu-byte FMP image; digest omitted\n", __FUNCTION__, (UINT64)FmpImageSize));
  }

  return HashOk;
}

STATIC
EFI_STATUS
LocateCapsuleAttemptRecordProtocol (
  OUT NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  **Protocol
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (
                  &gNVIDIACapsuleAttemptRecordProtocolGuid,
                  NULL,
                  (VOID **)Protocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: protocol locate failed: %r\n", __FUNCTION__, Status));
    return EFI_NOT_READY;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordBegin (
  IN CAPSULE_ATTEMPT_RECORD_SOURCE  Source
  )
{
  EFI_STATUS                              Status;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *Protocol;

  if (!FeaturePcdGet (PcdCapsuleAttemptRecordSupported)) {
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "%a: record begin requested for source %u\n", __FUNCTION__, Source));

  Status = LocateCapsuleAttemptRecordProtocol (&Protocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Protocol->Revision < NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_REVISION) ||
      (Protocol->Begin == NULL))
  {
    return EFI_UNSUPPORTED;
  }

  return Protocol->Begin (Protocol, Source);
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordFinalize (
  IN CAPSULE_ATTEMPT_RECORD_SOURCE  Source
  )
{
  EFI_STATUS                              Status;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *Protocol;

  if (!FeaturePcdGet (PcdCapsuleAttemptRecordSupported)) {
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "%a: record finalize requested for source %u\n", __FUNCTION__, Source));

  Status = LocateCapsuleAttemptRecordProtocol (&Protocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Protocol->Revision < NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_REVISION) ||
      (Protocol->Finalize == NULL))
  {
    return EFI_UNSUPPORTED;
  }

  return Protocol->Finalize (Protocol, Source);
}

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
  )
{
  EFI_STATUS                              Status;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *Protocol;

  if (!FeaturePcdGet (PcdCapsuleAttemptRecordSupported)) {
    return EFI_SUCCESS;
  }

  Status = LocateCapsuleAttemptRecordProtocol (&Protocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Protocol->Revision < NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_REVISION) ||
      (Protocol->UpdateFmpDiagnostics == NULL))
  {
    return EFI_UNSUPPORTED;
  }

  return Protocol->UpdateFmpDiagnostics (
                     Protocol,
                     CheckImage,
                     FailedImage,
                     RawStatus,
                     NvidiaStatus,
                     PlatformSpec,
                     TnSpec,
                     FmpImageDigest,
                     Completion,
                     TargetBootChain
                     );
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateFmpImageInfo (
  VOID
  )
{
  EFI_STATUS                              Status;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *Protocol;

  if (!FeaturePcdGet (PcdCapsuleAttemptRecordSupported)) {
    DEBUG ((DEBUG_INFO, "%a: client update skipped; feature disabled\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_ERROR, "%a: record update requested\n", __FUNCTION__));

  Status = LocateCapsuleAttemptRecordProtocol (&Protocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Protocol->UpdateFmpImageInfo (Protocol);
  return Status;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordDumpLatest (
  VOID
  )
{
  EFI_STATUS                              Status;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *Protocol;

  if (!FeaturePcdGet (PcdCapsuleAttemptRecordSupported)) {
    DEBUG ((DEBUG_INFO, "%a: client dump skipped; feature disabled\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  Status = LocateCapsuleAttemptRecordProtocol (&Protocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Protocol->DumpLatest (Protocol);
  return Status;
}
