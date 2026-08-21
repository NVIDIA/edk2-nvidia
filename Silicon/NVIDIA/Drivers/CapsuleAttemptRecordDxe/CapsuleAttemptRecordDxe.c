/** @file
  Capsule attempt record DXE driver.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
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

STATIC
EFI_STATUS
EFIAPI
CapsuleAttemptRecordProtocolUpdateFmpDiagnostics (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This,
  IN BOOLEAN                                 CheckImage,
  IN CONST CHAR16                            *FailedImage OPTIONAL,
  IN EFI_STATUS                              RawStatus,
  IN UINT32                                  NvidiaStatus,
  IN CONST CHAR8                             *PlatformSpec OPTIONAL,
  IN CONST CHAR8                             *TnSpec OPTIONAL,
  IN CONST UINT8                             *FmpImageDigest OPTIONAL,
  IN CONST UINT32                            *Completion OPTIONAL,
  IN CONST UINT32                            *TargetBootChain OPTIONAL
  );

STATIC CAPSULE_ATTEMPT_RECORD_PRIVATE  mPrivate = {
  CAPSULE_ATTEMPT_RECORD_PRIVATE_SIGNATURE,
  {
    NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL_REVISION,
    CapsuleAttemptRecordProtocolUpdateFmpImageInfo,
    CapsuleAttemptRecordProtocolDumpLatest,
    CapsuleAttemptRecordProtocolBegin,
    CapsuleAttemptRecordProtocolFinalize,
    CapsuleAttemptRecordProtocolUpdateFmpDiagnostics
  },
  NULL,
  {
    FALSE,
    0,
    CapsuleAttemptRecordSourceUnknown,
    0,
    FALSE,
    { 0 },
    FALSE,
    0,
    FALSE,
    FALSE
  }
};

STATIC
BOOLEAN
CaptureTimestamp (
  OUT EFI_TIME  *Timestamp
  )
{
  EFI_STATUS  Status;

  ZeroMem (Timestamp, sizeof (*Timestamp));
  Status = gRT->GetTime (Timestamp, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: runtime GetTime failed: %r; timestamp omitted\n", __FUNCTION__, Status));
    return FALSE;
  }

  if (!CapsuleAttemptRecordIsTimeValid (Timestamp)) {
    DEBUG ((DEBUG_ERROR, "%a: runtime GetTime returned an invalid time; timestamp omitted\n", __FUNCTION__));
    ZeroMem (Timestamp, sizeof (*Timestamp));
    return FALSE;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: captured attempt time %04u-%02u-%02u %02u:%02u:%02u\n",
    __FUNCTION__,
    Timestamp->Year,
    Timestamp->Month,
    Timestamp->Day,
    Timestamp->Hour,
    Timestamp->Minute,
    Timestamp->Second
    ));
  return TRUE;
}

STATIC
CONST EFI_TIME *
GetActiveTimestamp (
  IN OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State
  )
{
  if (!State->AttemptTimestampValid) {
    State->AttemptTimestampValid = CaptureTimestamp (&State->AttemptTimestamp);
  }

  return State->AttemptTimestampValid ? &State->AttemptTimestamp : NULL;
}

STATIC
BOOLEAN
GetElapsedTimeNs (
  IN  UINT64  StartCounter,
  IN  UINT64  EndCounter,
  OUT UINT64  *ElapsedTimeNs
  )
{
  UINT64  CounterStartValue;
  UINT64  CounterEndValue;
  UINT64  Frequency;
  UINT64  ElapsedTicks;

  Frequency = GetPerformanceCounterProperties (
                &CounterStartValue,
                &CounterEndValue
                );
  if (Frequency == 0) {
    DEBUG ((DEBUG_ERROR, "%a: performance counter frequency is zero; elapsed time omitted\n", __FUNCTION__));
    return FALSE;
  }

  ElapsedTicks = CapsuleAttemptRecordGetElapsedTicks (
                   StartCounter,
                   EndCounter,
                   CounterStartValue,
                   CounterEndValue
                   );
  *ElapsedTimeNs = GetTimeInNanoSecond (ElapsedTicks);
  DEBUG ((
    DEBUG_INFO,
    "%a: capsule attempt elapsed time: %Lu ticks, %Lu ns\n",
    __FUNCTION__,
    ElapsedTicks,
    *ElapsedTimeNs
    ));
  return TRUE;
}

STATIC
BOOLEAN
CaptureActiveBootChain (
  OUT UINT32  *ActiveBootChain
  )
{
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob == NULL) ||
      (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DEBUG ((DEBUG_ERROR, "%a: platform resource HOB is unavailable or invalid\n", __FUNCTION__));
    return FALSE;
  }

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  if (PlatformResourceInfo->ActiveBootChain >= BOOT_CHAIN_COUNT) {
    DEBUG ((DEBUG_ERROR, "%a: active boot chain is invalid: %u\n", __FUNCTION__, PlatformResourceInfo->ActiveBootChain));
    return FALSE;
  }

  *ActiveBootChain = PlatformResourceInfo->ActiveBootChain;
  DEBUG ((DEBUG_INFO, "%a: captured active boot chain %u\n", __FUNCTION__, *ActiveBootChain));
  return TRUE;
}

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
  IN OUT CAPSULE_ATTEMPT_RECORD_PRIVATE            *Private,
  IN     CAPSULE_ATTEMPT_RECORD_SOURCE             Source,
  IN     UINT64                                    AttemptId,
  IN     BOOLEAN                                   AttemptIdValid,
  IN     CONST EFI_TIME                            *Timestamp OPTIONAL,
  IN     CONST UINT64                              *ElapsedTimeNs OPTIONAL,
  IN     CONST UINT32                              *ActiveBootChain OPTIONAL,
  IN     CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  *Diagnostics OPTIONAL
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
               Timestamp,
               ElapsedTimeNs,
               ActiveBootChain,
               &ImageTypeId,
               &ImageInfoSet,
               Diagnostics
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
  EFI_TIME  Timestamp;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return FinalizeRecord (
           CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL (This),
           CapsuleAttemptRecordSourceUnknown,
           0,
           FALSE,
           CaptureTimestamp (&Timestamp) ? &Timestamp : NULL,
           NULL,
           NULL,
           NULL
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
  UINT64                          StartCounter;
  EFI_TIME                        Timestamp;
  BOOLEAN                         TimestampValid;
  UINT32                          ActiveBootChain;
  BOOLEAN                         ActiveBootChainValid;

  if ((This == NULL) || (Source <= CapsuleAttemptRecordSourceUnknown) ||
      (Source >= CapsuleAttemptRecordSourceMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private              = CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL (This);
  StartCounter         = GetPerformanceCounter ();
  TimestampValid       = CaptureTimestamp (&Timestamp);
  ActiveBootChain      = 0;
  ActiveBootChainValid = CaptureActiveBootChain (&ActiveBootChain);
  Status               = InitializeRecordContext (Private);
  if (!EFI_ERROR (Status)) {
    Status = CapsuleAttemptRecordBegin (
               Private->RecordContext,
               Source,
               TimestampValid ? &Timestamp : NULL,
               ActiveBootChainValid ? &ActiveBootChain : NULL,
               &AttemptId
               );
  }

  if (!EFI_ERROR (Status)) {
    CapsuleAttemptRecordStateBegin (
      &Private->State,
      Source,
      AttemptId,
      TimestampValid ? &Timestamp : NULL,
      ActiveBootChainValid ? &ActiveBootChain : NULL,
      StartCounter
      );
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
  EFI_TIME                        Timestamp;
  CONST EFI_TIME                  *TimestampPointer;
  UINT64                          CompletionCounter;
  UINT64                          ElapsedTimeNs;
  CONST UINT64                    *ElapsedTimePointer;

  if ((This == NULL) || (Source <= CapsuleAttemptRecordSourceUnknown) ||
      (Source >= CapsuleAttemptRecordSourceMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private            = CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL (This);
  CompletionCounter  = GetPerformanceCounter ();
  ElapsedTimePointer = NULL;
  MatchingAttempt    = CapsuleAttemptRecordStateMatch (&Private->State, Source, &AttemptId);
  if (MatchingAttempt) {
    TimestampPointer = GetActiveTimestamp (&Private->State);
    if (Private->State.AttemptStartCounterValid &&
        GetElapsedTimeNs (
          Private->State.AttemptStartCounter,
          CompletionCounter,
          &ElapsedTimeNs
          ))
    {
      ElapsedTimePointer = &ElapsedTimeNs;
    }
  } else {
    TimestampPointer = CaptureTimestamp (&Timestamp) ? &Timestamp : NULL;
  }

  Status = FinalizeRecord (
             Private,
             Source,
             AttemptId,
             MatchingAttempt,
             TimestampPointer,
             ElapsedTimePointer,
             (MatchingAttempt && Private->State.ActiveBootChainValid) ? &Private->State.ActiveBootChain : NULL,
             MatchingAttempt ? CapsuleAttemptRecordStateGetDiagnostics (&Private->State) : NULL
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
CapsuleAttemptRecordProtocolUpdateFmpDiagnostics (
  IN NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL  *This,
  IN BOOLEAN                                 CheckImage,
  IN CONST CHAR16                            *FailedImage OPTIONAL,
  IN EFI_STATUS                              RawStatus,
  IN UINT32                                  NvidiaStatus,
  IN CONST CHAR8                             *PlatformSpec OPTIONAL,
  IN CONST CHAR8                             *TnSpec OPTIONAL,
  IN CONST UINT8                             *FmpImageDigest OPTIONAL,
  IN CONST UINT32                            *Completion OPTIONAL,
  IN CONST UINT32                            *TargetBootChain OPTIONAL
  )
{
  EFI_STATUS                      Status;
  CAPSULE_ATTEMPT_RECORD_PRIVATE  *Private;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CAPSULE_ATTEMPT_RECORD_PRIVATE_FROM_PROTOCOL (This);
  Status  = CapsuleAttemptRecordStateUpdateDiagnostics (
              &Private->State,
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
  if (Status == EFI_NOT_READY) {
    DEBUG ((DEBUG_ERROR, "%a: diagnostic update requested without an active attempt\n", __FUNCTION__));
  }

  if (!EFI_ERROR (Status)) {
    Status = InitializeRecordContext (Private);
  }

  if (!EFI_ERROR (Status) && (TargetBootChain != NULL)) {
    DEBUG ((DEBUG_INFO, "%a: captured target boot chain %u\n", __FUNCTION__, *TargetBootChain));
  }

  if (!EFI_ERROR (Status)) {
    Status = CapsuleAttemptRecordUpdateDiagnostics (
               Private->RecordContext,
               Private->State.AttemptSource,
               Private->State.AttemptId,
               GetActiveTimestamp (&Private->State),
               Private->State.ActiveBootChainValid ? &Private->State.ActiveBootChain : NULL,
               CapsuleAttemptRecordStateGetDiagnostics (&Private->State)
               );
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
