/** @file

  FW Image Library

  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FwImageLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Uefi/UefiBaseType.h>

STATIC UINTN                        mNumImages                      = 0;
STATIC NVIDIA_FW_IMAGE_PROTOCOL     **mFwImages                     = NULL;

NVIDIA_FW_IMAGE_PROTOCOL *
EFIAPI
FwImageFindProtocol (
  CONST CHAR16                  *Name
  )
{
  NVIDIA_FW_IMAGE_PROTOCOL      *Protocol;
  UINTN                         ProtocolIndex;

  Protocol = NULL;
  for (ProtocolIndex = 0; ProtocolIndex < mNumImages; ProtocolIndex++) {
    if (StrnCmp (mFwImages[ProtocolIndex]->ImageName,
                 Name,
                 FW_IMAGE_NAME_LENGTH) == 0) {
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
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS            Status;
  UINTN                 Index;
  UINTN                 NumHandles;
  EFI_HANDLE            *HandleBuffer;

  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gNVIDIAFwImageProtocolGuid,
                                    NULL,
                                    &NumHandles,
                                    &HandleBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: LocateHandleBuffer failed for gNVIDIAFwImageProtocolGuid (%r)\n",
            __FUNCTION__, Status));
    return Status;
  }
  DEBUG ((DEBUG_INFO, "%a: got %d FW image handles", __FUNCTION__, NumHandles));

  mFwImages = (NVIDIA_FW_IMAGE_PROTOCOL **)
    AllocateRuntimeZeroPool (NumHandles * sizeof (VOID *));
  if (mFwImages == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  mNumImages = 0;
  for (Index = 0; Index < NumHandles; Index++) {
    Status = gBS->HandleProtocol( HandleBuffer[Index],
                                  &gNVIDIAFwImageProtocolGuid,
                                  (VOID **) &mFwImages[Index]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get FW Image Protocol for index=%u: %r\n",
              __FUNCTION__, Index, Status));
      Status = EFI_NOT_FOUND;
      goto Done;
    }

    DEBUG ((DEBUG_INFO, "%a: Got FW Image protocol, Name=%s\n",
            __FUNCTION__, mFwImages[Index]->ImageName));

    mNumImages++;
  }

Done:
  FreePool (HandleBuffer);

  if (EFI_ERROR (Status)) {
    if (mFwImages != NULL) {
      FreePool (mFwImages);
      mFwImages = NULL;
    }
    mNumImages = 0;
  }

  return Status;
}
