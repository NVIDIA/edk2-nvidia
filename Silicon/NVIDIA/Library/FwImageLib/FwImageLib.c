/** @file

  FW Image Library

  Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FwImageLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Uefi/UefiBaseType.h>

STATIC UINTN                     mNumImages             = 0;
STATIC NVIDIA_FW_IMAGE_PROTOCOL  **mFwImages            = NULL;
STATIC FW_IMAGE_ADDED_CALLBACK   mImageAddedCallback    = NULL;
STATIC EFI_EVENT                 mNewImageEvent         = NULL;
STATIC VOID                      *mNewImageRegistration = NULL;

NVIDIA_FW_IMAGE_PROTOCOL *
EFIAPI
FwImageFindProtocol (
  CONST CHAR16  *Name
  )
{
  NVIDIA_FW_IMAGE_PROTOCOL  *Protocol;
  UINTN                     ProtocolIndex;

  Protocol = NULL;
  for (ProtocolIndex = 0; ProtocolIndex < mNumImages; ProtocolIndex++) {
    if (StrnCmp (
          mFwImages[ProtocolIndex]->ImageName,
          Name,
          FW_IMAGE_NAME_LENGTH
          ) == 0)
    {
      Protocol = mFwImages[ProtocolIndex];
      break;
    }
  }

  return Protocol;
}

UINTN
EFIAPI
FwImageGetCount (
  VOID
  )
{
  return mNumImages;
}

NVIDIA_FW_IMAGE_PROTOCOL **
EFIAPI
FwImageGetProtocolArray (
  VOID
  )
{
  return mFwImages;
}

VOID
EFIAPI
FwImageRegisterImageAddedCallback (
  FW_IMAGE_ADDED_CALLBACK  Callback
  )
{
  mImageAddedCallback = Callback;

  if ((Callback != NULL) && (mNumImages > 0)) {
    mImageAddedCallback ();
  }
}

/**
  Event notification that is fired when FwImage protocol instance is installed.

  @param  Event                 The Event that is being processed.
  @param  Context               Event Context.

**/
VOID
EFIAPI
FwImageLibProtocolCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS  Status;
  UINTN       HandleSize;
  EFI_HANDLE  Handle;

  while (TRUE) {
    HandleSize = sizeof (Handle);
    Status     = gBS->LocateHandle (
                        ByRegisterNotify,
                        &gNVIDIAFwImageProtocolGuid,
                        mNewImageRegistration,
                        &HandleSize,
                        &Handle
                        );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: No handles: %r\n", __FUNCTION__, Status));

      if (mImageAddedCallback != NULL) {
        mImageAddedCallback ();
      }

      return;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gNVIDIAFwImageProtocolGuid,
                    (VOID **)&mFwImages[mNumImages]
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get FW Image Protocol: %r\n", __FUNCTION__, Status));
      goto Done;
    }

    DEBUG ((DEBUG_INFO, "%a: Got FW Image protocol, Name=%s\n", __FUNCTION__, mFwImages[mNumImages]->ImageName));

    if (FwImageFindProtocol (mFwImages[mNumImages]->ImageName) != NULL) {
      DEBUG ((DEBUG_ERROR, "%a: duplicate %s image\n", __FUNCTION__, mFwImages[mNumImages]->ImageName));
      Status = EFI_UNSUPPORTED;
      goto Done;
    }

    mNumImages++;
  }

Done:
  if (EFI_ERROR (Status)) {
    if (mFwImages != NULL) {
      FreePool (mFwImages);
      mFwImages = NULL;
    }

    mNumImages = 0;

    gBS->CloseEvent (Event);
  }
}

/**
  Fw Image Lib constructor entry point.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwImageLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  mFwImages = (NVIDIA_FW_IMAGE_PROTOCOL **)
              AllocateRuntimePool (FW_IMAGE_MAX_IMAGES * sizeof (NVIDIA_FW_IMAGE_PROTOCOL *));
  if (mFwImages == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: mFwImages allocate failed\n", __FUNCTION__));
    goto Done;
  }

  mNewImageEvent = EfiCreateProtocolNotifyEvent (
                     &gNVIDIAFwImageProtocolGuid,
                     TPL_CALLBACK,
                     FwImageLibProtocolCallback,
                     NULL,
                     &mNewImageRegistration
                     );
  if (mNewImageEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: protocol notify failed\n", __FUNCTION__));
    goto Done;
  }

Done:
  // If an error occurred above, library API reports no images.
  return EFI_SUCCESS;
}
