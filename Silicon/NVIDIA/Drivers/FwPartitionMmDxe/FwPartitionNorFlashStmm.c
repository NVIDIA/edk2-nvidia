/** @file

  FW Partition Protocol NorFlash Dxe

  SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/FwPartitionDeviceLib.h>
#include <Library/GptLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/NorFlash.h>
#include <Library/FwPartitionDeviceLib.h>

#define MAX_NOR_FLASH_DEVICES                  16
#define FW_PARTITION_NOR_FLASH_INFO_SIGNATURE  SIGNATURE_32 ('F','W','N','S')

typedef enum {
  NOR_FLASH_TYPE_FW_AND_DATA,
  NOR_FLASH_TYPE_DATA_ONLY,
  NOR_FLASH_TYPE_FW_ONLY,
} NOR_FLASH_TYPE;

// private device data structure
typedef struct {
  UINT32                       Signature;
  UINT64                       Bytes;
  NOR_FLASH_ATTRIBUTES         Attributes;
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlash;
  FW_PARTITION_DEVICE_INFO     DeviceInfo;
  UINTN                        UnalignedGptStart;
  UINT32                       SocketId;
  NOR_FLASH_TYPE               FlashType;
} FW_PARTITION_NOR_FLASH_INFO;

STATIC FW_PARTITION_NOR_FLASH_INFO  *mNorFlashInfo   = NULL;
STATIC UINTN                        mNumDevices      = 0;
STATIC UINT32                       mActiveBootChain = 0;

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
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes
  )
{
  FW_PARTITION_NOR_FLASH_INFO  *NorFlashInfo;
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlash;
  EFI_STATUS                   Status;
  UINT32                       EraseBlockSize;
  UINT32                       OffsetLba;
  UINT32                       LbaCount;
  NOR_FLASH_ATTRIBUTES         *Attributes;

  NorFlashInfo = CR (
                   DeviceInfo,
                   FW_PARTITION_NOR_FLASH_INFO,
                   DeviceInfo,
                   FW_PARTITION_NOR_FLASH_INFO_SIGNATURE
                   );
  NorFlash       = NorFlashInfo->NorFlash;
  Attributes     = &NorFlashInfo->Attributes;
  EraseBlockSize = Attributes->BlockSize;

  Status = FwPartitionCheckOffsetAndBytes (NorFlashInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: erase offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  if ((Offset % EraseBlockSize) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: unaligned erase, block size=%u, Offset=%llu\n",
      __FUNCTION__,
      EraseBlockSize,
      Offset
      ));
    return EFI_INVALID_PARAMETER;
  }

  OffsetLba = Offset / EraseBlockSize;
  LbaCount  = ALIGN_VALUE (Bytes, EraseBlockSize) / EraseBlockSize;

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: erase OffsetLba=%u, LbaCount=%u\n",
    __FUNCTION__,
    OffsetLba,
    LbaCount
    ));

  return NorFlash->Erase (NorFlash, OffsetLba, LbaCount);
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
  IN  CONST CHAR16              *PartitionName,
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  OUT VOID                      *Buffer
  )
{
  FW_PARTITION_NOR_FLASH_INFO  *NorFlashInfo;
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlash;
  EFI_STATUS                   Status;

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: ENTRY  offset=%llu, bytes=%u\n",
    __FUNCTION__,
    Offset,
    Bytes
    ));

  NorFlashInfo = CR (
                   DeviceInfo,
                   FW_PARTITION_NOR_FLASH_INFO,
                   DeviceInfo,
                   FW_PARTITION_NOR_FLASH_INFO_SIGNATURE
                   );
  NorFlash = NorFlashInfo->NorFlash;

  Status = FwPartitionCheckOffsetAndBytes (NorFlashInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: read offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: read offset=%llu, bytes=%u\n",
    __FUNCTION__,
    Offset,
    Bytes
    ));

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
  IN  CONST CHAR16              *PartitionName,
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  IN  CONST VOID                *Buffer
  )
{
  VOID                         *NonConstBuffer;
  FW_PARTITION_NOR_FLASH_INFO  *NorFlashInfo;
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlash;
  EFI_STATUS                   Status;

  NorFlashInfo = CR (
                   DeviceInfo,
                   FW_PARTITION_NOR_FLASH_INFO,
                   DeviceInfo,
                   FW_PARTITION_NOR_FLASH_INFO_SIGNATURE
                   );
  NorFlash = NorFlashInfo->NorFlash;

  // NorFlash protocol Write prototype uses non-const buffer pointer
  NonConstBuffer = (VOID *)((UINTN)Buffer);

  Status = FwPartitionCheckOffsetAndBytes (NorFlashInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: write offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  if (Offset % NorFlashInfo->Attributes.BlockSize == 0 ) {
    Status = FPNorFlashErase (DeviceInfo, Offset, Bytes);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: erase offset=%llu, bytes=%u error: %r\n",
        __FUNCTION__,
        Offset,
        Bytes,
        Status
        ));
      return Status;
    }
  } else if (Offset == NorFlashInfo->UnalignedGptStart) {
    Status = FPNorFlashErase (
               DeviceInfo,
               Offset - (Offset % NorFlashInfo->Attributes.BlockSize),
               Bytes + (Offset % NorFlashInfo->Attributes.BlockSize)
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: GPT erase offset=%llu, bytes=%u error: %r\n",
        __FUNCTION__,
        Offset - (Offset % NorFlashInfo->Attributes.BlockSize),
        Bytes + (Offset % NorFlashInfo->Attributes.BlockSize),

        Status
        ));
      return Status;
    }
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: write offset=%llu, bytes=%u\n",
    __FUNCTION__,
    Offset,
    Bytes
    ));

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
  EFI_GUID        *ProtocolGuid,
  NOR_FLASH_TYPE  FlashType
  )
{
  EFI_STATUS            Status;
  UINTN                 HandleBufferSize;
  EFI_HANDLE            HandleBuffer[MAX_NOR_FLASH_DEVICES];
  UINTN                 NumHandles;
  UINTN                 Index;
  NOR_FLASH_ATTRIBUTES  Attributes;

  HandleBufferSize = sizeof (HandleBuffer);
  Status           = gMmst->MmLocateHandle (
                              ByProtocol,
                              ProtocolGuid,
                              NULL,
                              &HandleBufferSize,
                              HandleBuffer
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Error locating MM-NorFlash handles: %r\n", Status));
    return Status;
  }

  NumHandles = HandleBufferSize / sizeof (EFI_HANDLE);

  for (Index = 0; Index < NumHandles; Index++) {
    EFI_HANDLE                   Handle;
    NVIDIA_NOR_FLASH_PROTOCOL    *NorFlash;
    FW_PARTITION_NOR_FLASH_INFO  *NorFlashInfo;
    FW_PARTITION_DEVICE_INFO     *DeviceInfo;
    UINT32                       *SocketIdProtocol;
    UINT32                       SocketId;

    NorFlash = NULL;
    Handle   = HandleBuffer[Index];
    Status   = gMmst->MmHandleProtocol (
                        Handle,
                        ProtocolGuid,
                        (VOID **)&NorFlash
                        );
    if (EFI_ERROR (Status) || (NorFlash == NULL)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to get MM-NorFlash for handle index %u: %r\n",
        Index,
        Status
        ));
      continue;
    }

    Status = NorFlash->GetAttributes (NorFlash, &Attributes);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "MM-NorFlash attributes for handle %u failed: %r\n",
        Index,
        Status
        ));
      continue;
    }

    SocketIdProtocol = NULL;
    Status           = gMmst->MmHandleProtocol (
                                Handle,
                                &gNVIDIASocketIdProtocolGuid,
                                (VOID **)&SocketIdProtocol
                                );
    if (EFI_ERROR (Status) || (SocketIdProtocol == NULL)) {
      DEBUG ((DEBUG_ERROR, "SocketId protocol not found for handle %u\n", Index));
      continue;
    } else {
      SocketId = (UINT8)(*SocketIdProtocol);
    }

    DEBUG ((
      DEBUG_INFO,
      "Found MM-NorFlash Socket=%u BlockSize=%u, MemoryDensity=%llu\n",
      SocketId,
      Attributes.BlockSize,
      Attributes.MemoryDensity
      ));

    if (mNumDevices >= MAX_NOR_FLASH_DEVICES) {
      DEBUG ((DEBUG_ERROR, "%a: Max devices=%d exceeded\n", __FUNCTION__, MAX_NOR_FLASH_DEVICES));
      break;
    }

    NorFlashInfo                    = &mNorFlashInfo[mNumDevices];
    NorFlashInfo->Signature         = FW_PARTITION_NOR_FLASH_INFO_SIGNATURE;
    NorFlashInfo->Bytes             = Attributes.MemoryDensity;
    NorFlashInfo->Attributes        = Attributes;
    NorFlashInfo->NorFlash          = NorFlash;
    NorFlashInfo->UnalignedGptStart = GptGetGptDataOffset (OTHER_BOOT_CHAIN (mActiveBootChain), Attributes.MemoryDensity, Attributes.BlockSize);
    NorFlashInfo->SocketId          = SocketId;
    NorFlashInfo->FlashType         = FlashType;

    DeviceInfo              = &NorFlashInfo->DeviceInfo;
    DeviceInfo->DeviceName  = L"MM-NorFlash";
    DeviceInfo->DeviceRead  = FPNorFlashRead;
    DeviceInfo->DeviceWrite = FPNorFlashWrite;
    DeviceInfo->BlockSize   = Attributes.BlockSize;

    mNumDevices++;
  }

  return EFI_SUCCESS;
}

/**
  Fw Partition Nor Flash Driver initialization entry point.

  @param[in]  ActiveBootChain               Active boot chain
  @param[in]  OverwriteActiveFwPartition    Flag to allow overwriting

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionNorFlashStmmInitialize (
  UINTN    ActiveBootChain,
  BOOLEAN  OverwriteActiveFwPartition,
  UINTN    ChipId
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  Status = StmmGetActiveBootChain (&mActiveBootChain);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: error getting boot chain, using %u: %r\n", __FUNCTION__, ActiveBootChain, Status));
    mActiveBootChain = ActiveBootChain;
  }

  if (mActiveBootChain != ActiveBootChain) {
    DEBUG ((DEBUG_ERROR, "%a: boot chain mismatch %u != %u\n", __FUNCTION__, mActiveBootChain, ActiveBootChain));
    return EFI_INVALID_PARAMETER;
  }

  Status = FwPartitionDeviceLibInit (ActiveBootChain, MAX_FW_PARTITIONS, OverwriteActiveFwPartition, ChipId, StmmGetBootChainForGpt ());
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FwPartition lib init failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  mNorFlashInfo = (FW_PARTITION_NOR_FLASH_INFO *)AllocateZeroPool (
                                                   MAX_NOR_FLASH_DEVICES * sizeof (FW_PARTITION_NOR_FLASH_INFO)
                                                   );
  if (mNorFlashInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "mNorFlashInfo allocation failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  // when PcdDataOnlyFlashIsSupported, the FW flash is gNVIDIANorFlash2ProtocolGuid
  if (PcdGetBool (PcdDataOnlyFlashIsSupported) &&
      PcdGetBool (PcdFwBlobIsSupported))
  {
    Status = FPNorFlashInitDevices (&gNVIDIANorFlash2ProtocolGuid, NOR_FLASH_TYPE_FW_ONLY);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_INFO,
        "%a: Error initializing NorFlash devices: %r\n",
        __FUNCTION__,
        Status
        ));
      goto Done;
    }
  }

  Status = FPNorFlashInitDevices (
             &gNVIDIANorFlashProtocolGuid,
             (PcdGetBool (PcdDataOnlyFlashIsSupported)) ? NOR_FLASH_TYPE_DATA_ONLY : NOR_FLASH_TYPE_FW_AND_DATA
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: Error initializing NorFlash devices: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  for (Index = 0; Index < mNumDevices; Index++) {
    FW_PARTITION_NOR_FLASH_INFO  *NorFlashInfo = &mNorFlashInfo[Index];
    FW_PARTITION_DEVICE_INFO     *DeviceInfo   = &NorFlashInfo->DeviceInfo;

    if (NorFlashInfo->SocketId != 0) {
      continue;
    }

    if (NorFlashInfo->FlashType == NOR_FLASH_TYPE_FW_AND_DATA) {
      Status = FwPartitionAddFromDeviceGpt (DeviceInfo, NorFlashInfo->Bytes);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Error adding partitions from FW device=%s: %r\n",
          __FUNCTION__,
          DeviceInfo->DeviceName,
          Status
          ));
      }
    }

    if (NorFlashInfo->FlashType == NOR_FLASH_TYPE_DATA_ONLY) {
      Status = FwDeviceAddAsPartition (
                 L"MM-NorFlash",
                 DeviceInfo,
                 0,
                 NorFlashInfo->Bytes
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Error adding FW device %s as NorFlashpartition: %r\n",
          __FUNCTION__,
          DeviceInfo->DeviceName,
          Status
          ));
      }
    }

    if (PcdGetBool (PcdFwBlobIsSupported) &&
        ((NorFlashInfo->FlashType == NOR_FLASH_TYPE_FW_ONLY) ||
         (NorFlashInfo->FlashType == NOR_FLASH_TYPE_FW_AND_DATA)))
    {
      Status = FwDeviceAddAsPartition (
                 L"NorFlash-Blob",
                 DeviceInfo,
                 0,
                 NorFlashInfo->Bytes
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Error adding FW device %s as blob partition: %r\n",
          __FUNCTION__,
          DeviceInfo->DeviceName,
          Status
          ));
      }
    }
  }

Done:
  if (EFI_ERROR (Status)) {
    FwPartitionDeviceLibDeinit ();

    if (mNorFlashInfo != NULL) {
      FreePool (mNorFlashInfo);
      mNorFlashInfo = NULL;
    }

    mNumDevices = 0;
  }

  return Status;
}
