/** @file

  FW Image Library

  Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FwImageLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Uefi/UefiBaseType.h>

STATIC UINTN                     mNumImages  = 0;
STATIC NVIDIA_FW_IMAGE_PROTOCOL  **mFwImages = NULL;

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
  EFI_STATUS  Status;
  UINTN       Index;
  UINTN       NumHandles;
  EFI_HANDLE  *HandleBuffer;

  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gNVIDIAFwImageProtocolGuid,
                        NULL,
                        &NumHandles,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: LocateHandleBuffer failed for gNVIDIAFwImageProtocolGuid (%r)\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "%a: got %d FW image handles", __FUNCTION__, NumHandles));

  mFwImages = (NVIDIA_FW_IMAGE_PROTOCOL **)
              AllocateRuntimeZeroPool (NumHandles * sizeof (VOID *));
  if (mFwImages == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: mFwImages allocate failed\n", __FUNCTION__));
    goto Done;
  }

  mNumImages = 0;
  for (Index = 0; Index < NumHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gNVIDIAFwImageProtocolGuid,
                    (VOID **)&mFwImages[Index]
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get FW Image Protocol for index=%u: %r\n",
        __FUNCTION__,
        Index,
        Status
        ));
      goto Done;
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: Got FW Image protocol, Name=%s\n",
      __FUNCTION__,
      mFwImages[Index]->ImageName
      ));

    if (FwImageFindProtocol (mFwImages[Index]->ImageName) != NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: duplicate %s image index=%u\n",
        __FUNCTION__,
        mFwImages[Index]->ImageName,
        Index
        ));
      Status = EFI_UNSUPPORTED;
      goto Done;
    }

    mNumImages++;
  }

Done:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
    HandleBuffer = NULL;
  }

  if (EFI_ERROR (Status)) {
    if (mFwImages != NULL) {
      FreePool (mFwImages);
      mFwImages = NULL;
    }

    mNumImages = 0;
  }

  // If an error occurred above, library API reports no images.
  return EFI_SUCCESS;
}
