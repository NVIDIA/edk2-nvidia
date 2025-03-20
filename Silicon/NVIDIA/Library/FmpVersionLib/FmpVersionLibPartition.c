/** @file

  FMP version library using version partition

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Base.h"
#include <Library/BaseLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/DebugLib.h>
#include <Library/FmpVersionLib.h>
#include <Library/FwImageLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/VerPartitionLib.h>
#include <Protocol/FwImageProtocol.h>

STATIC FMP_VERSION_READY_CALLBACK  mFmpVersionCallback = NULL;
STATIC UINT32                      mActiveBootChain    = MAX_UINT32;

extern EFI_STATUS  mFmpVersionStatus;
extern UINT32      mFmpVersion;
extern CHAR16      *mFmpVersionString;

/**
  Set system FW version UEFI variable.

  @param[in]  ActiveVersion         Version of active FW.
  @param[in]  InactiveVersion       Version of inactive FW.

  @retval     None

**/
STATIC
VOID
EFIAPI
SetFwVersionVariable (
  UINT32  ActiveVersion,
  UINT32  InactiveVersion
  )
{
  EFI_STATUS  Status;
  UINT32      VersionArray[BOOT_CHAIN_COUNT];

  VersionArray[BOOT_CHAIN_A] = (mActiveBootChain == BOOT_CHAIN_A) ? ActiveVersion : InactiveVersion;
  VersionArray[BOOT_CHAIN_B] = (mActiveBootChain == BOOT_CHAIN_B) ? ActiveVersion : InactiveVersion;

  Status = gRT->SetVariable (
                  L"SystemFwVersions",
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (VersionArray),
                  VersionArray
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error setting fw versions: %r\n", __FUNCTION__, Status));
  }
}

/**
  Get version info.

  @retval EFI_SUCCESS                   Operation completed successfully
  @retval EFI_NOT_FOUND                 The VER partition wasn't found
  @retval Others                        An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FmpVersionPartitionGetInfo (
  VOID
  )
{
  EFI_STATUS                Status;
  CHAR8                     *VerStr;
  UINTN                     VerStrSize;
  NVIDIA_FW_IMAGE_PROTOCOL  *Image;
  FW_IMAGE_ATTRIBUTES       Attributes;
  UINTN                     BufferSize;
  EFI_STATUS                InactiveStatus  = EFI_NOT_FOUND;
  UINT32                    InactiveVersion = MAX_UINT32;
  VOID                      *DataBuffer     = NULL;
  UINTN                     DataBufferSize  = SIZE_8KB;

  VerStr = NULL;
  Image  = FwImageFindProtocol (VER_PARTITION_NAME);
  if (Image == NULL) {
    return EFI_NOT_FOUND;
  }

  Status = Image->GetAttributes (Image, &Attributes);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  DataBuffer = AllocateRuntimeZeroPool (DataBufferSize);
  if (DataBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate data buffer\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  BufferSize = MIN (Attributes.ReadBytes, DataBufferSize);
  Status     = Image->Read (
                        Image,
                        0,
                        BufferSize,
                        DataBuffer,
                        FW_IMAGE_RW_FLAG_NONE
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: VER read failed: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  ((CHAR8 *)DataBuffer)[BufferSize - 1] = '\0';

  Status = VerPartitionGetVersion (DataBuffer, BufferSize, &mFmpVersion, &VerStr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to parse version info: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  VerStrSize        = AsciiStrSize (VerStr);
  mFmpVersionString = (CHAR16 *)
                      AllocateRuntimeZeroPool (VerStrSize * sizeof (CHAR16));
  if (mFmpVersionString == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = AsciiStrToUnicodeStrS (
             VerStr,
             mFmpVersionString,
             VerStrSize
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  mFmpVersionStatus = Status;

  // read inactive version number
  if (VerStr != NULL) {
    FreePool (VerStr);
    VerStr = NULL;
  }

  InactiveStatus = Image->Read (
                            Image,
                            0,
                            BufferSize,
                            DataBuffer,
                            FW_IMAGE_RW_FLAG_READ_INACTIVE_IMAGE
                            );
  if (EFI_ERROR (InactiveStatus)) {
    DEBUG ((DEBUG_ERROR, "%a: inactive VER read failed: %r\n", __FUNCTION__, InactiveStatus));
    goto Done;
  }

  ((CHAR8 *)DataBuffer)[BufferSize - 1] = '\0';

  InactiveStatus = VerPartitionGetVersion (DataBuffer, BufferSize, &InactiveVersion, &VerStr);
  if (EFI_ERROR (InactiveStatus)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to parse inactive version info: %r\n", __FUNCTION__, InactiveStatus));
    goto Done;
  }

Done:
  if (VerStr != NULL) {
    FreePool (VerStr);
  }

  if (DataBuffer != NULL) {
    FreePool (DataBuffer);
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Version=0x%x, Str=(%s), Status=%r, InactiveVersion=0x%x\n",
    __FUNCTION__,
    mFmpVersion,
    mFmpVersionString,
    Status,
    InactiveVersion
    ));

  if (EFI_ERROR (Status)) {
    if (mFmpVersionString != NULL) {
      FreePool (mFmpVersionString);
      mFmpVersionString = NULL;
    }

    mFmpVersion       = 0;
    mFmpVersionStatus = EFI_UNSUPPORTED;
  }

  SetFwVersionVariable (
    (mFmpVersionStatus == EFI_SUCCESS) ? mFmpVersion : MAX_UINT32,
    InactiveVersion
    );

  return mFmpVersionStatus;
}

/**
  Function to handle new FwImage found.

  @return None

**/
VOID
EFIAPI
FmpVersionFwImageCallback (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mFmpVersionStatus == EFI_UNSUPPORTED) {
    Status = FmpVersionPartitionGetInfo ();
    if (Status == EFI_NOT_FOUND) {
      return;
    }
  }

  mFmpVersionCallback (mFmpVersionStatus);
  FwImageRegisterImageAddedCallback (NULL);
}

EFI_STATUS
EFIAPI
FmpVersionLibInit (
  UINT32                      ActiveBootChain,
  FMP_VERSION_READY_CALLBACK  Callback
  )
{
  if (Callback == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  mActiveBootChain    = ActiveBootChain;
  mFmpVersionCallback = Callback;
  FwImageRegisterImageAddedCallback (FmpVersionFwImageCallback);

  return EFI_SUCCESS;
}
