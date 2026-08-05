/** @file
  Capsule attempt record client library.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/CapsuleAttemptRecordLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/CapsuleAttemptRecordProtocol.h>

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
