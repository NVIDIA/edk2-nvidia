/** @file

  NVIDIA Capsule Update Runtime DXE driver

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SavedCapsuleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/SavedCapsuleProtocol.h>

#define SAVED_CAPSULE_VARIABLE_NAME  L"SavedCapsuleHeader"

typedef struct {
  EFI_CAPSULE_HEADER    Header;
  UINT32                Checksum;
} SAVED_CAPSULE_INFO;

STATIC EFI_EVENT  mReadyToBootEvent   = NULL;
STATIC EFI_EVENT  mAddressChangeEvent = NULL;
STATIC EFI_EVENT  mEndOfDxeEvent      = NULL;

STATIC SAVED_CAPSULE_INFO             mSavedCapsuleInfo;
STATIC NVIDIA_SAVED_CAPSULE_PROTOCOL  mProtocol;

STATIC
VOID
EFIAPI
AddressChangeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
}

STATIC
EFI_STATUS
EFIAPI
DeleteCapsuleVariable (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = gRT->SetVariable (
                  SAVED_CAPSULE_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  0,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Error deleting variable: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
NvidiaUpdateCapsule (
  IN EFI_CAPSULE_HEADER    **CapsuleHeaderArray,
  IN UINTN                 CapsuleCount,
  IN EFI_PHYSICAL_ADDRESS  ScatterGatherList OPTIONAL
  )
{
  EFI_STATUS          Status;
  EFI_CAPSULE_HEADER  *Header;

  if (CapsuleCount != 1) {
    return EFI_UNSUPPORTED;
  }

  Header = CapsuleHeaderArray[0];

  DeleteCapsuleVariable ();

  CopyMem (&mSavedCapsuleInfo.Header, Header, sizeof (*Header));
  mSavedCapsuleInfo.Checksum = CalculateCrc32 ((UINT8 *)Header, Header->CapsuleImageSize);

  Status = CapsuleStore (Header, Header->CapsuleImageSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error saving capsule: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = gRT->SetVariable (
                  SAVED_CAPSULE_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  sizeof (mSavedCapsuleInfo),
                  &mSavedCapsuleInfo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error setting variable: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

STATIC
VOID
EFIAPI
EndOfDxeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  gBS->CloseEvent (Event);

  Status = SavedCapsuleLibInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: SavedCapsuleLib init failed: %r\n", __FUNCTION__, Status));

    gBS->CloseEvent (mReadyToBootEvent);
    mReadyToBootEvent = NULL;
  }
}

STATIC
VOID
EFIAPI
ReadyToBootNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;
  VOID        *CapsuleArchProtocol;
  UINT32      Crc32 = 0;

  gBS->CloseEvent (Event);

  Status = gBS->LocateProtocol (&gEfiCapsuleArchProtocolGuid, NULL, &CapsuleArchProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: no capsule arch protocol\n", __FUNCTION__));
    return;
  }

  DEBUG ((DEBUG_ERROR, "%a: installing NVIDIA RT UpdateCapsule function\n", __FUNCTION__));
  gRT->UpdateCapsule = NvidiaUpdateCapsule;

  gRT->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 ((UINT8 *)gRT, gRT->Hdr.HeaderSize, &Crc32);
  gRT->Hdr.CRC32 = Crc32;
}

STATIC
EFI_STATUS
EFIAPI
GetCapsule (
  IN  NVIDIA_SAVED_CAPSULE_PROTOCOL  *This,
  OUT EFI_CAPSULE_HEADER             **CapsuleHeader
  )
{
  VOID        *Capsule = NULL;
  EFI_STATUS  Status;
  UINTN       CapsuleSize;
  UINT32      Checksum;

  if ((This != &mProtocol) || (CapsuleHeader == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CapsuleSize = mSavedCapsuleInfo.Header.CapsuleImageSize;
  Capsule     = AllocatePool (CapsuleSize);
  if (Capsule == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: alloc of %u failed\n", __FUNCTION__, mSavedCapsuleInfo.Header.CapsuleImageSize));
    return EFI_OUT_OF_RESOURCES;
  }

  DeleteCapsuleVariable ();

  Status = CapsuleLoad (Capsule, CapsuleSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error loading capsule: %r\n", __FUNCTION__, Status));
  }

  Checksum = CalculateCrc32 ((UINT8 *)Capsule, CapsuleSize);
  if (Checksum != mSavedCapsuleInfo.Checksum) {
    DEBUG ((DEBUG_ERROR, "%a: checksum mismatch size=%u %u/%u\n", __FUNCTION__, CapsuleSize, Checksum, mSavedCapsuleInfo.Checksum));
  }

  if (EFI_ERROR (Status)) {
    if (Capsule != NULL) {
      FreePool (Capsule);
    }
  } else {
    *CapsuleHeader = (EFI_CAPSULE_HEADER *)Capsule;
  }

  return Status;
}

EFI_STATUS
EFIAPI
NvidiaCapsuleRuntimeDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;
  UINTN       VariableSize;
  UINT32      Attributes;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  ReadyToBootNotify,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &mReadyToBootEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error creating ReadyToBoot event: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  EndOfDxeNotify,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &mEndOfDxeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error creating EndOfDxe event: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  AddressChangeNotify,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mAddressChangeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error creating address change event: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  VariableSize = sizeof (mSavedCapsuleInfo);
  Status       = gRT->GetVariable (
                        SAVED_CAPSULE_VARIABLE_NAME,
                        &gNVIDIAPublicVariableGuid,
                        &Attributes,
                        &VariableSize,
                        &mSavedCapsuleInfo
                        );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((DEBUG_ERROR, "%a: Error reading variable: %r\n", __FUNCTION__, Status));
      goto Done;
    }

    Status = EFI_SUCCESS;
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "%a capsule size=%u guid=%g csum=0x%x installing protocol\n", __FUNCTION__, mSavedCapsuleInfo.Header.CapsuleImageSize, &mSavedCapsuleInfo.Header.CapsuleGuid, mSavedCapsuleInfo.Checksum));
  mProtocol.GetCapsule = GetCapsule;
  Handle               = NULL;
  Status               = gBS->InstallMultipleProtocolInterfaces (
                                &Handle,
                                &gNVIDIASavedCapsuleProtocolGuid,
                                &mProtocol,
                                NULL
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing protocol: %r\n", __FUNCTION__, Status));
    goto Done;
  }

Done:
  if (EFI_ERROR (Status)) {
    if (mReadyToBootEvent != NULL) {
      gBS->CloseEvent (mReadyToBootEvent);
      mReadyToBootEvent = NULL;
    }

    if (mEndOfDxeEvent != NULL) {
      gBS->CloseEvent (mEndOfDxeEvent);
      mEndOfDxeEvent = NULL;
    }

    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }
  }

  return Status;
}
