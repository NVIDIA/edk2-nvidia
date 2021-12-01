/** @file

  FW Partition Protocol NorFlash Dxe

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

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FwPartitionDeviceLib.h>
#include <Library/GptLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/NorFlash.h>

#define MAX_NOR_FLASH_DEVICES                   1
#define FW_PARTITION_NOR_FLASH_INFO_SIGNATURE   SIGNATURE_32 ('F','W','N','F')

// private device data structure
typedef struct {
  UINT32                            Signature;
  UINT64                            Bytes;
  UINT32                            EraseBlockSize;
  NVIDIA_NOR_FLASH_PROTOCOL         *NorFlash;
  FW_PARTITION_DEVICE_INFO          DeviceInfo;
} FW_PARTITION_NOR_FLASH_INFO;

STATIC FW_PARTITION_NOR_FLASH_INFO  *mNorFlashInfo      = NULL;
STATIC UINTN                        mNumDevices         = 0;
STATIC EFI_EVENT                    mAddressChangeEvent = NULL;

/**
  Erase data from device.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to begin erase
  @param[in]  Bytes             Number of bytes to erase

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPNorFlashErase (
  IN  FW_PARTITION_DEVICE_INFO          *DeviceInfo,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes
  )
{
  FW_PARTITION_NOR_FLASH_INFO           *NorFlashInfo;
  NVIDIA_NOR_FLASH_PROTOCOL             *NorFlash;
  EFI_STATUS                            Status;
  UINT32                                EraseBlockSize;
  UINT32                                OffsetLba;
  UINT32                                LbaCount;

  NorFlashInfo  = CR (DeviceInfo,
                      FW_PARTITION_NOR_FLASH_INFO,
                      DeviceInfo,
                      FW_PARTITION_NOR_FLASH_INFO_SIGNATURE);
  NorFlash      = NorFlashInfo->NorFlash;
  EraseBlockSize= NorFlashInfo->EraseBlockSize;

  Status = FwPartitionCheckOffsetAndBytes (NorFlashInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read offset=%llu, bytes=%u error: %r\n",
            __FUNCTION__, Offset, Bytes, Status));
    return Status;
  }

  if (((Offset % EraseBlockSize) != 0)  || ((Bytes % EraseBlockSize) != 0)) {
    DEBUG ((DEBUG_ERROR, "%a: unaligned erase, block size=%u, Offset=%u, Bytes=%u\n",
            __FUNCTION__, EraseBlockSize, Offset, Bytes));
    return EFI_INVALID_PARAMETER;
  }

  OffsetLba = Offset / EraseBlockSize;
  LbaCount  = Bytes / EraseBlockSize;

  DEBUG ((DEBUG_VERBOSE, "%a: erase OffsetLba=%u, LbaCount=%u\n",
          __FUNCTION__, OffsetLba, LbaCount));

  return NorFlash->Erase (NorFlash, OffsetLba, LbaCount, FALSE);
}

/**
  Read data from device.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to read from
  @param[in]  Bytes             Number of bytes to read
  @param[out] Buffer            Address to read data into

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPNorFlashRead (
  IN  FW_PARTITION_DEVICE_INFO          *DeviceInfo,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  OUT VOID                              *Buffer
  )
{
  FW_PARTITION_NOR_FLASH_INFO           *NorFlashInfo;
  NVIDIA_NOR_FLASH_PROTOCOL             *NorFlash;
  EFI_STATUS                            Status;

  NorFlashInfo  = CR (DeviceInfo,
                      FW_PARTITION_NOR_FLASH_INFO,
                      DeviceInfo,
                      FW_PARTITION_NOR_FLASH_INFO_SIGNATURE);
  NorFlash      = NorFlashInfo->NorFlash;

  Status = FwPartitionCheckOffsetAndBytes (NorFlashInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read offset=%llu, bytes=%u error: %r\n",
            __FUNCTION__, Offset, Bytes, Status));
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: read offset=%llu, bytes=%u\n",
          __FUNCTION__, Offset, Bytes));

  return NorFlash->Read (NorFlash, Offset, Bytes, Buffer);
}

/**
  Write data to device.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to write
  @param[in]  Bytes             Number of bytes to write
  @param[in]  Buffer            Address of write data

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPNorFlashWrite (
  IN  FW_PARTITION_DEVICE_INFO          *DeviceInfo,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  )
{
  VOID                                  *NonConstBuffer;
  FW_PARTITION_NOR_FLASH_INFO           *NorFlashInfo;
  NVIDIA_NOR_FLASH_PROTOCOL             *NorFlash;
  EFI_STATUS                            Status;

  NorFlashInfo  = CR (DeviceInfo,
                      FW_PARTITION_NOR_FLASH_INFO,
                      DeviceInfo,
                      FW_PARTITION_NOR_FLASH_INFO_SIGNATURE);
  NorFlash      = NorFlashInfo->NorFlash;

  // NorFlash protocol Write prototype uses non-const buffer pointer
  NonConstBuffer = (VOID *) ((UINTN) Buffer);

  Status = FwPartitionCheckOffsetAndBytes (NorFlashInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: write offset=%llu, bytes=%u error: %r\n",
            __FUNCTION__, Offset, Bytes, Status));
    return Status;
  }

  Status = FPNorFlashErase (DeviceInfo,
                            Offset,
                            ALIGN_VALUE (Bytes, NorFlashInfo->EraseBlockSize));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: erase offset=%llu, bytes=%u error: %r\n",
            __FUNCTION__, Offset, Bytes, Status));
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: write offset=%llu, bytes=%u\n",
          __FUNCTION__, Offset, Bytes));

  return NorFlash->Write (NorFlash, Offset, Bytes, NonConstBuffer);
}

/**
  Find NorFlash devices and initialize private data structures.

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPNorFlashInitDevices (
  VOID
  )
{
  EFI_STATUS                Status;
  UINTN                     NumHandles;
  EFI_HANDLE                *HandleBuffer;
  UINTN                     Index;
  CHAR16                    *DeviceName;
  NOR_FLASH_ATTRIBUTES      Attributes;

  DEBUG ((DEBUG_INFO, "%a: Entry\n", __FUNCTION__));

  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gNVIDIANorFlashProtocolGuid,
                                    NULL,
                                    &NumHandles,
                                    &HandleBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Error locating NorFlash handles: %r\n", Status));
    return Status;
  }

  for (Index = 0; Index < NumHandles; Index++) {
    EFI_HANDLE                  Handle;
    EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
    NVIDIA_NOR_FLASH_PROTOCOL   *NorFlash;
    FW_PARTITION_NOR_FLASH_INFO *NorFlashInfo;
    FW_PARTITION_DEVICE_INFO    *DeviceInfo;

    Handle = HandleBuffer[Index];
    Status = gBS->HandleProtocol (Handle,
                                  &gEfiDevicePathProtocolGuid,
                                  (VOID **) &DevicePath);
    if (EFI_ERROR (Status) || (DevicePath == NULL)) {
      DEBUG ((DEBUG_ERROR, "Failed to get DevicePath for handle index %u: %r\n",
              Index, Status));
      continue;
    }

    Status = gBS->HandleProtocol (Handle,
                                  &gNVIDIANorFlashProtocolGuid,
                                  (VOID **) &NorFlash);
    if (EFI_ERROR (Status) || (NorFlash == NULL)) {
      DEBUG ((DEBUG_ERROR, "Failed to get NorFlash for handle index %u: %r\n",
              Index, Status));
      continue;
    }

    Status = NorFlash->GetAttributes (NorFlash, &Attributes);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "NorFlash attributes for handle %u failed: %r\n",
              Index, Status));
      continue;
    }

    DeviceName = ConvertDevicePathToText (DevicePath, TRUE, TRUE);
    DEBUG ((DEBUG_INFO, "Found NorFlash FW device=%s, BlockSize=%u, MemoryDensity=%llu\n",
            DeviceName, Attributes.UniformBlockSize, Attributes.UniformMemoryDensity));

    if (mNumDevices >= MAX_NOR_FLASH_DEVICES) {
      DEBUG ((DEBUG_ERROR, "%a: Max devices=%u exceeded\n",
              __FUNCTION__, MAX_NOR_FLASH_DEVICES));
      break;
    }

    NorFlashInfo                    = &mNorFlashInfo[mNumDevices];
    NorFlashInfo->Signature         = FW_PARTITION_NOR_FLASH_INFO_SIGNATURE;
    mNorFlashInfo->Bytes            = Attributes.UniformMemoryDensity;
    mNorFlashInfo->EraseBlockSize   = Attributes.UniformBlockSize;
    mNorFlashInfo->NorFlash         = NorFlash;

    DeviceInfo                      = &NorFlashInfo->DeviceInfo;
    DeviceInfo->DeviceName          = DeviceName;
    DeviceInfo->DeviceRead          = FPNorFlashRead;
    DeviceInfo->DeviceWrite         = FPNorFlashWrite;

    mNumDevices++;
  }

  FreePool (HandleBuffer);

  return EFI_SUCCESS;
}

/**
  Convert given pointer to support runtime execution.

  @param[in]  Pointer       Address of pointer to convert

  @retval None

**/
STATIC
VOID
EFIAPI
FPNorFlashAddressConvert (
  IN  VOID **Pointer
  )
{
  EfiConvertPointer (0x0, Pointer);
}

/**
  Handle address change notification to support runtime execution.

  @param[in]  Event         Event being handled
  @param[in]  Context       Event context

  @retval None

**/
STATIC
VOID
EFIAPI
FPNorFlashAddressChangeNotify (
  IN EFI_EVENT                  Event,
  IN VOID                       *Context
  )
{
  UINTN                         Index;
  FW_PARTITION_NOR_FLASH_INFO   *NorFlashInfo;

  NorFlashInfo = mNorFlashInfo;
  for (Index = 0; Index < mNumDevices; Index++, NorFlashInfo++) {
    FW_PARTITION_DEVICE_INFO    *DeviceInfo;

    EfiConvertPointer (0x0, (VOID **) &NorFlashInfo->NorFlash);

    DeviceInfo = &NorFlashInfo->DeviceInfo;
    EfiConvertPointer (0x0, (VOID **) &DeviceInfo->DeviceName);
    EfiConvertPointer (0x0, (VOID **) &DeviceInfo->DeviceRead);
    EfiConvertPointer (0x0, (VOID **) &DeviceInfo->DeviceWrite);
  }
  EfiConvertPointer (0x0, (VOID **) &mNorFlashInfo);

  BrBctUpdateAddressChangeHandler (FPNorFlashAddressConvert);
  FwPartitionAddressChangeHandler (FPNorFlashAddressConvert);
}

/**
  Fw Partition Nor Flash Driver initialization entry point.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionNorFlashDxeInitialize (
  IN EFI_HANDLE                 ImageHandle,
  IN EFI_SYSTEM_TABLE           *SystemTable
  )
{
  EFI_STATUS                    Status;
  UINTN                         Index;
  UINT32                        ActiveBootChain;
  BR_BCT_UPDATE_PRIVATE_DATA    *BrBctUpdatePrivate;
  FW_PARTITION_PRIVATE_DATA     *FwPartitionPrivate;

  BrBctUpdatePrivate = NULL;

  Status = GetActiveBootChain (&ActiveBootChain);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting active boot chain: %r\n",
            __FUNCTION__, Status));
    return Status;
  }

  Status = FwPartitionDeviceLibInit (ActiveBootChain, MAX_FW_PARTITIONS);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FwPartition lib init failed: %r\n", Status));
    return Status;
  }

  mNorFlashInfo = (FW_PARTITION_NOR_FLASH_INFO *) AllocateRuntimeZeroPool (
    MAX_NOR_FLASH_DEVICES * sizeof (FW_PARTITION_NOR_FLASH_INFO));
  if (mNorFlashInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "mNorFlashInfo allocation failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = FPNorFlashInitDevices ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Error initializing NorFlash devices: %r\n",
            __FUNCTION__, Status));
    goto Done;
  }

  // add FwPartition structs for all partitions in GPT on each device
  for (Index = 0; Index < mNumDevices; Index++) {
    FW_PARTITION_NOR_FLASH_INFO *NorFlashInfo   = &mNorFlashInfo[Index];
    FW_PARTITION_DEVICE_INFO    *DeviceInfo     = &NorFlashInfo->DeviceInfo;

    Status = FwPartitionAddFromDeviceGpt (DeviceInfo, NorFlashInfo->Bytes);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error adding partitions from FW device=%s: %r\n",
              __FUNCTION__, DeviceInfo->DeviceName, Status));
    }
  }

  // install FwPartition protocols for all partitions
  FwPartitionPrivate = FwPartitionGetPrivateArray ();
  for (Index = 0; Index < FwPartitionGetCount (); Index++, FwPartitionPrivate++) {
    Status = gBS->InstallMultipleProtocolInterfaces (&FwPartitionPrivate->Handle,
                                                     &gNVIDIAFwPartitionProtocolGuid,
                                                     &FwPartitionPrivate->Protocol,
                                                     NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Couldn't install protocol for Index=%u, partition=%s: %r\n",
              __FUNCTION__, Index, FwPartitionPrivate->PartitionInfo.Name, Status));
      goto Done;
    }
  }

  // Only one is device supported, use its device erase size for BR-BCT
  ASSERT (mNumDevices == 1);
  Status = BrBctUpdateDeviceLibInit (ActiveBootChain,
                                     FPNorFlashErase,
                                     mNorFlashInfo[0].EraseBlockSize);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((DEBUG_ERROR, "%a: Error initializing BrBct lib: %r\n",
              __FUNCTION__, Status));
    }
    goto Done;
  }

  BrBctUpdatePrivate = BrBctUpdateGetPrivate ();
  Status = gBS->InstallMultipleProtocolInterfaces (&BrBctUpdatePrivate->Handle,
                                                   &gNVIDIABrBctUpdateProtocolGuid,
                                                   &BrBctUpdatePrivate->Protocol,
                                                   NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Couldn't install BR-BCT update protocol: %r\n",
            __FUNCTION__, Status));
    goto Done;
  }

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_NOTIFY,
                               FPNorFlashAddressChangeNotify,
                               NULL,
                               &gEfiEventVirtualAddressChangeGuid,
                               &mAddressChangeEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error creating address change event Status = %r\n",
            __FUNCTION__, Status));
    goto Done;
  }

Done:
  if (EFI_ERROR (Status)) {
    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }

    if ((BrBctUpdatePrivate != NULL) && (BrBctUpdatePrivate->Handle != NULL)) {
      gBS->UninstallMultipleProtocolInterfaces (BrBctUpdatePrivate->Handle,
                                                &gNVIDIABrBctUpdateProtocolGuid,
                                                &BrBctUpdatePrivate->Protocol,
                                                NULL);
      BrBctUpdatePrivate->Handle = NULL;
    }

    FwPartitionPrivate = FwPartitionGetPrivateArray ();
    for (Index = 0; Index < FwPartitionGetCount (); Index++, FwPartitionPrivate++) {
      if (FwPartitionPrivate->Handle != NULL) {
        Status = gBS->UninstallMultipleProtocolInterfaces (FwPartitionPrivate->Handle,
                                                           &gNVIDIAFwPartitionProtocolGuid,
                                                           &FwPartitionPrivate->Protocol,
                                                           NULL);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Error uninstalling protocol for partition=%s: %r\n",
                  __FUNCTION__, FwPartitionPrivate->PartitionInfo.Name, Status));
        }
        FwPartitionPrivate->Handle = NULL;
      }
    }

    BrBctUpdateDeviceLibDeinit ();
    FwPartitionDeviceLibDeinit ();

    if (mNorFlashInfo != NULL) {
      FreePool (mNorFlashInfo);
      mNorFlashInfo = NULL;
    }
    mNumDevices = 0;
  }

  return Status;
}
