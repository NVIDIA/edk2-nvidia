/** @file
  Capsule attempt record DXE driver.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/CapsuleAttemptRecordProtocol.h>
#include <Protocol/FirmwareManagement.h>

#include "CapsuleAttemptRecordDxeInternal.h"
#include "CapsuleAttemptRecordDxeState.h"

#define CAPSULE_ATTEMPT_RECORD_PRIVATE_SIGNATURE  SIGNATURE_32 ('C', 'A', 'R', 'D')

typedef struct {
  UINT32                                    Signature;
  NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL    Protocol;
  CAPSULE_ATTEMPT_RECORD_CONTEXT            *RecordContext;
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE     State;
} CAPSULE_ATTEMPT_RECORD_PRIVATE;

#define CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL(ProtocolPointer)  \
  CR (ProtocolPointer, CAPSULE_ATTEMPT_RECORD_PRIVATE, Protocol,       \
      CAPSULE_ATTEMPT_RECORD_PRIVATE_SIGNATURE)

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolUpdateFmpImageInfo (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This
  );

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolDumpLatest (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This
  );

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolBegin (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This,
  IN CAPSULE_ATTEMPT_RECORD_SOURCE           Source
  );

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolFinalize (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This,
  IN CAPSULE_ATTEMPT_RECORD_SOURCE           Source
  );

STATIC CAPSULE_ATTEMPT_RECORD_PRIVATE  mPrivate = {
  CAPSULE_ATTEMPT_RECORD_PRIVATE_SIGNATURE,
  {
    NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_REVISION,
    CapsuleAttemptRecordProtocolUpdateFmpImageInfo,
    CapsuleAttemptRecordProtocolDumpLatest,
    CapsuleAttemptRecordProtocolBegin,
    CapsuleAttemptRecordProtocolFinalize
  },
  NULL,
  {
    FALSE,
    0,
    CapsuleAttemptRecordSourceUnknown
  }
};

STATIC
VOID
FreeFmpImageInfoSet (
  IN OUT CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  *ImageInfoSet
  )
{
  if (ImageInfoSet == NULL) {
    return;
  }

  if (ImageInfoSet->ImageInfo != NULL) {
    FreePool ((VOID *)ImageInfoSet->ImageInfo);
  }

  ZeroMem (ImageInfoSet, sizeof (*ImageInfoSet));
}

STATIC
BOOLEAN
FmpImageInfoResultIsValid (
  IN UINTN   ImageInfoSize,
  IN UINT32  DescriptorVersion,
  IN UINT8   DescriptorCount,
  IN UINTN   DescriptorSize
  )
{
  if ((DescriptorVersion == 0) || (DescriptorCount == 0) ||
      (DescriptorSize == 0) ||
      (DescriptorSize < OFFSET_OF (EFI_FIRMWARE_IMAGE_DESCRIPTOR, ImageTypeId) + sizeof (EFI_GUID)) ||
      (DescriptorCount > MAX_UINTN / DescriptorSize))
  {
    return FALSE;
  }

  return (BOOLEAN)(DescriptorCount * DescriptorSize <= ImageInfoSize);
}

STATIC
BOOLEAN
FmpImageInfoContainsImageTypeId (
  IN CONST CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  *ImageInfoSet,
  IN CONST EFI_GUID                                   *ImageTypeId
  )
{
  UINTN                                DescriptorIndex;
  CONST EFI_FIRMWARE_IMAGE_DESCRIPTOR  *Descriptor;

  for (DescriptorIndex = 0;
       DescriptorIndex < ImageInfoSet->DescriptorCount;
       DescriptorIndex++)
  {
    Descriptor = (CONST EFI_FIRMWARE_IMAGE_DESCRIPTOR *)(
                                                         (CONST UINT8 *)ImageInfoSet->ImageInfo +
                                                         DescriptorIndex * ImageInfoSet->DescriptorSize
                                                         );
    if (CompareGuid (&Descriptor->ImageTypeId, ImageTypeId)) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
EFI_STATUS
FindFmpImageInfo (
  IN  CONST EFI_GUID                             *ImageTypeId,
  OUT CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  *ImageInfoSet
  )
{
  EFI_STATUS                                 Status;
  EFI_STATUS                                 FmpStatus;
  EFI_HANDLE                                 *Handles;
  UINTN                                      HandleCount;
  UINTN                                      HandleIndex;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL           *Fmp;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR              *ImageInfo;
  UINTN                                      ImageInfoSize;
  UINT32                                     DescriptorVersion;
  UINT8                                      DescriptorCount;
  UINTN                                      DescriptorSize;
  UINT32                                     PackageVersion;
  CHAR16                                     *PackageVersionName;
  CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  CandidateSet;

  if ((ImageTypeId == NULL) || (ImageInfoSet == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (ImageInfoSet, sizeof (*ImageInfoSet));
  ImageInfo   = NULL;
  Handles     = NULL;
  HandleCount = 0;
  Status      = gBS->LocateHandleBuffer (
                       ByProtocol,
                       &gEfiFirmwareManagementProtocolGuid,
                       NULL,
                       &HandleCount,
                       &Handles
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FMP handle lookup failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: searching %u FMP handles for %g\n", __FUNCTION__, HandleCount, ImageTypeId));
  if (HandleCount == 0) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  ImageInfo = AllocateZeroPool (sizeof (*ImageInfo));
  if (ImageInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    Fmp       = NULL;
    FmpStatus = gBS->HandleProtocol (
                       Handles[HandleIndex],
                       &gEfiFirmwareManagementProtocolGuid,
                       (VOID **)&Fmp
                       );
    if (EFI_ERROR (FmpStatus) || (Fmp == NULL) || (Fmp->GetImageInfo == NULL)) {
      continue;
    }

    ZeroMem (ImageInfo, sizeof (*ImageInfo));
    ZeroMem (&CandidateSet, sizeof (CandidateSet));
    ImageInfoSize      = sizeof (*ImageInfo);
    DescriptorVersion  = 0;
    DescriptorCount    = 0;
    DescriptorSize     = 0;
    PackageVersion     = 0;
    PackageVersionName = NULL;
    FmpStatus          = Fmp->GetImageInfo (
                                Fmp,
                                &ImageInfoSize,
                                ImageInfo,
                                &DescriptorVersion,
                                &DescriptorCount,
                                &DescriptorSize,
                                &PackageVersion,
                                &PackageVersionName
                                );
    if (PackageVersionName != NULL) {
      FreePool (PackageVersionName);
      PackageVersionName = NULL;
    }

    if (EFI_ERROR (FmpStatus) ||
        !FmpImageInfoResultIsValid (
           ImageInfoSize,
           DescriptorVersion,
           DescriptorCount,
           DescriptorSize
           ))
    {
      continue;
    }

    CandidateSet.ImageInfo         = ImageInfo;
    CandidateSet.ImageInfoSize     = ImageInfoSize;
    CandidateSet.DescriptorVersion = DescriptorVersion;
    CandidateSet.DescriptorCount   = DescriptorCount;
    CandidateSet.DescriptorSize    = DescriptorSize;
    CandidateSet.PackageVersion    = PackageVersion;
    if (FmpImageInfoContainsImageTypeId (&CandidateSet, ImageTypeId)) {
      //
      // The NVIDIA system FMP is produced by FmpDevicePkg/FmpDxe, which
      // exposes exactly one firmware image descriptor.  Other FMP instances
      // may expose multiple descriptors, but they must not use the configured
      // system firmware image type GUID.
      //
      NV_ASSERT_RETURN (
        DescriptorCount == 1,
        { Status = EFI_UNSUPPORTED;
          goto Done;
        },
        "%a: unexpected descriptor count: %u",
        __FUNCTION__,
        DescriptorCount
        );

      *ImageInfoSet = CandidateSet;
      ImageInfo     = NULL;
      Status        = EFI_SUCCESS;
      goto Done;
    }
  }

  Status = EFI_NOT_FOUND;
  DEBUG ((DEBUG_ERROR, "%a: system FMP %g not found\n", __FUNCTION__, ImageTypeId));

Done:
  if (Handles != NULL) {
    FreePool (Handles);
  }

  if (ImageInfo != NULL) {
    FreePool (ImageInfo);
  }

  if (EFI_ERROR (Status)) {
    FreeFmpImageInfoSet (ImageInfoSet);
  }

  return Status;
}

STATIC
EFI_STATUS
InitializeRecordContext (
  IN OUT CAPSULE_ATTEMPT_RECORD_PRIVATE  *Private
  )
{
  EFI_STATUS  Status;
  VOID        *Dtb;
  UINTN       DtbSize;

  if (Private->RecordContext != NULL) {
    DEBUG ((DEBUG_INFO, "%a: reusing initialized storage context\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  Dtb     = NULL;
  DtbSize = 0;
  Status  = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: platform DT load failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = CapsuleAttemptRecordInitialize (
             Dtb,
             DtbSize,
             &Private->RecordContext
             );

  return Status;
}

STATIC
EFI_STATUS
FinalizeRecord (
  IN OUT CAPSULE_ATTEMPT_RECORD_PRIVATE  *Private,
  IN     CAPSULE_ATTEMPT_RECORD_SOURCE   Source,
  IN     UINT64                          AttemptId,
  IN     BOOLEAN                         AttemptIdValid
  )
{
  EFI_STATUS                                 Status;
  CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  ImageInfoSet;
  EFI_GUID                                   ImageTypeId;

  if (PcdGetSize (PcdSystemFmpCapsuleImageTypeIdGuid) != sizeof (ImageTypeId)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: system FMP GUID invalid size: %u\n",
      __FUNCTION__,
      PcdGetSize (PcdSystemFmpCapsuleImageTypeIdGuid)
      ));
    return EFI_UNSUPPORTED;
  }

  CopyGuid (&ImageTypeId, PcdGetPtr (PcdSystemFmpCapsuleImageTypeIdGuid));

  Status = FindFmpImageInfo (&ImageTypeId, &ImageInfoSet);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FMP lookup failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = InitializeRecordContext (Private);
  if (!EFI_ERROR (Status)) {
    Status = CapsuleAttemptRecordFinalize (
               Private->RecordContext,
               Source,
               AttemptId,
               AttemptIdValid,
               &ImageTypeId,
               &ImageInfoSet
               );
  }

  FreeFmpImageInfoSet (&ImageInfoSet);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolUpdateFmpImageInfo (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This
  )
{
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return FinalizeRecord (
           CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL (This),
           CapsuleAttemptRecordSourceUnknown,
           0,
           FALSE
           );
}

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolBegin (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This,
  IN CAPSULE_ATTEMPT_RECORD_SOURCE           Source
  )
{
  EFI_STATUS                      Status;
  CAPSULE_ATTEMPT_RECORD_PRIVATE  *Private;
  UINT64                          AttemptId;

  if ((This == NULL) || (Source <= CapsuleAttemptRecordSourceUnknown) ||
      (Source >= CapsuleAttemptRecordSourceMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private = CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL (This);
  Status  = InitializeRecordContext (Private);
  if (!EFI_ERROR (Status)) {
    Status = CapsuleAttemptRecordBegin (
               Private->RecordContext,
               Source,
               &AttemptId
               );
  }

  if (!EFI_ERROR (Status)) {
    CapsuleAttemptRecordStateBegin (&Private->State, Source, AttemptId);
    DEBUG ((DEBUG_INFO, "%a: attempt %Lu began for source %u\n", __FUNCTION__, AttemptId, Source));
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolFinalize (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This,
  IN CAPSULE_ATTEMPT_RECORD_SOURCE           Source
  )
{
  EFI_STATUS                      Status;
  CAPSULE_ATTEMPT_RECORD_PRIVATE  *Private;
  UINT64                          AttemptId;
  BOOLEAN                         MatchingAttempt;

  if ((This == NULL) || (Source <= CapsuleAttemptRecordSourceUnknown) ||
      (Source >= CapsuleAttemptRecordSourceMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private         = CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL (This);
  MatchingAttempt = CapsuleAttemptRecordStateMatch (&Private->State, Source, &AttemptId);

  Status = FinalizeRecord (
             Private,
             Source,
             AttemptId,
             MatchingAttempt
             );
  if (MatchingAttempt) {
    CapsuleAttemptRecordStateConsume (&Private->State);
  }

  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: attempt finalized for source %u\n", __FUNCTION__, Source));
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolDumpLatest (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This
  )
{
  EFI_STATUS                      Status;
  CAPSULE_ATTEMPT_RECORD_PRIVATE  *Private;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL (This);
  Status  = InitializeRecordContext (Private);
  if (!EFI_ERROR (Status)) {
    Status = CapsuleAttemptRecordDumpLatest (Private->RecordContext);
  }

  return Status;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  if (!FeaturePcdGet (PcdCapsuleAttemptRecordSupported)) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gNVIDIACapsuleAttemptRecordProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mPrivate.Protocol
                  );

  return Status;
}
