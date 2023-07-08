/** @file

  MM FW partition protocol driver

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>

#include "FwPartitionMmDxe.h"

#define FW_PARTITION_MM_INFO_SIGNATURE  SIGNATURE_32 ('F','W','M','M')
#define FW_PARTITION_MM_TRANSFER_SIZE   (32 * 1024)

// private MM info structure, one per partition
typedef struct {
  UINT32                      Signature;
  UINT64                      Bytes;
  CHAR16                      PartitionName[FW_PARTITION_NAME_LENGTH];
  FW_PARTITION_DEVICE_INFO    DeviceInfo;
} FW_PARTITION_MM_INFO;

STATIC FW_PARTITION_MM_INFO  *mMmInfo            = NULL;
STATIC UINTN                 mNumPartitions      = 0;
STATIC EFI_EVENT             mAddressChangeEvent = NULL;

EFI_MM_COMMUNICATION2_PROTOCOL  *mMmCommProtocol       = NULL;
VOID                            *mMmCommBuffer         = NULL;
VOID                            *mMmCommBufferPhysical = NULL;

STATIC
EFI_STATUS
EFIAPI
FPMmRead (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  OUT VOID                      *Buffer
  )
{
  FW_PARTITION_MM_INFO  *MmInfo;
  EFI_STATUS            Status;
  UINTN                 ReadBytes;

  MmInfo = CR (
             DeviceInfo,
             FW_PARTITION_MM_INFO,
             DeviceInfo,
             FW_PARTITION_MM_INFO_SIGNATURE
             );

  Status = FwPartitionCheckOffsetAndBytes (MmInfo->Bytes, Offset, Bytes);
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

  while (Bytes > 0) {
    ReadBytes = MIN (FW_PARTITION_MM_TRANSFER_SIZE, Bytes);

    Status = MmSendReadData (MmInfo->PartitionName, Offset, ReadBytes, Buffer);
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: read %s Offset=%lu, Bytes=%u\n",
      __FUNCTION__,
      MmInfo->PartitionName,
      Offset,
      ReadBytes
      ));
    if (EFI_ERROR (Status)) {
      break;
    }

    Bytes  -= ReadBytes;
    Offset += ReadBytes;
    Buffer  = ((UINT8 *)Buffer + ReadBytes);
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
FPMmWrite (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  IN  CONST VOID                *Buffer
  )
{
  FW_PARTITION_MM_INFO  *MmInfo;
  EFI_STATUS            Status;

  MmInfo = CR (
             DeviceInfo,
             FW_PARTITION_MM_INFO,
             DeviceInfo,
             FW_PARTITION_MM_INFO_SIGNATURE
             );

  Status = FwPartitionCheckOffsetAndBytes (MmInfo->Bytes, Offset, Bytes);
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

  Status = MmSendWriteData (MmInfo->PartitionName, Offset, Bytes, Buffer);
  DEBUG ((
    DEBUG_VERBOSE,
    "%a: write %s Offset=%lu, Bytes=%u\n",
    __FUNCTION__,
    MmInfo->PartitionName,
    Offset,
    Bytes
    ));

  return Status;
}

/**
  Find MM partitions and add private data structures for them

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPMmAddPartitions (
  OUT UINTN  *BrBctEraseBlockSize
  )
{
  EFI_STATUS                      Status;
  UINTN                           Index;
  UINTN                           Count;
  FW_PARTITION_MM_PARTITION_INFO  *PartitionInfoBuffer;

  PartitionInfoBuffer = (FW_PARTITION_MM_PARTITION_INFO *)
                        AllocateZeroPool (MAX_FW_PARTITIONS * sizeof (FW_PARTITION_MM_PARTITION_INFO));
  if (PartitionInfoBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = MmSendGetPartitions (
             MAX_FW_PARTITIONS,
             PartitionInfoBuffer,
             &Count,
             BrBctEraseBlockSize
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (Count == 0) {
    DEBUG ((DEBUG_INFO, "%a: No MM images found\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "%a: Got %u image names from MM\n", __FUNCTION__, Count));
  for (Index = 0; Index < Count; Index++) {
    FW_PARTITION_MM_PARTITION_INFO  *PartitionInfo = &PartitionInfoBuffer[Index];
    FW_PARTITION_MM_INFO            *MmInfo        = &mMmInfo[Index];
    FW_PARTITION_DEVICE_INFO        *DeviceInfo    = &MmInfo->DeviceInfo;

    DEBUG ((DEBUG_INFO, "Found MM Image name=%s\n", PartitionInfo->Name));

    MmInfo->Signature = FW_PARTITION_MM_INFO_SIGNATURE;
    MmInfo->Bytes     = PartitionInfo->Bytes;
    StrnCpyS (
      MmInfo->PartitionName,
      FW_PARTITION_NAME_LENGTH,
      PartitionInfo->Name,
      StrLen (PartitionInfo->Name)
      );

    DeviceInfo->DeviceName  = MmInfo->PartitionName;
    DeviceInfo->DeviceRead  = FPMmRead;
    DeviceInfo->DeviceWrite = FPMmWrite;
    DeviceInfo->BlockSize   = 1;

    Status = FwPartitionAdd (
               PartitionInfo->Name,
               DeviceInfo,
               0,
               PartitionInfo->Bytes
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: error adding %s\n",
        __FUNCTION__,
        PartitionInfo->Name
        ));
    }

    mNumPartitions++;
  }

  Status = EFI_SUCCESS;

Done:
  if (PartitionInfoBuffer != NULL) {
    FreePool (PartitionInfoBuffer);
  }

  return Status;
}

/**
  Convert given pointer to support runtime execution.

  @param[in]  Pointer       Address of pointer to convert

  @retval None

**/
STATIC
VOID
EFIAPI
FPMmAddressConvert (
  IN  VOID  **Pointer
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
FPMmAddressChangeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN                 Index;
  FW_PARTITION_MM_INFO  *MmInfo;

  MmInfo = mMmInfo;
  for (Index = 0; Index < mNumPartitions; Index++, MmInfo++) {
    FW_PARTITION_DEVICE_INFO  *DeviceInfo;

    DeviceInfo = &MmInfo->DeviceInfo;
    EfiConvertPointer (0x0, (VOID **)&DeviceInfo->DeviceName);
    EfiConvertPointer (0x0, (VOID **)&DeviceInfo->DeviceRead);
    EfiConvertPointer (0x0, (VOID **)&DeviceInfo->DeviceWrite);
  }

  EfiConvertPointer (0x0, (VOID **)&mMmInfo);
  EfiConvertPointer (0x0, (VOID **)&mMmCommProtocol);
  EfiConvertPointer (0x0, (VOID **)&mMmCommBuffer);

  BrBctUpdateAddressChangeHandler (FPMmAddressConvert);
  FwPartitionAddressChangeHandler (FPMmAddressConvert);
}

/**
  Fw Partition Mm Driver initialization entry point.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionMmDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  UINTN                       Index;
  UINT32                      ActiveBootChain;
  BR_BCT_UPDATE_PRIVATE_DATA  *BrBctUpdatePrivate;
  FW_PARTITION_PRIVATE_DATA   *FwPartitionPrivate;
  VOID                        *Hob;
  UINTN                       BrBctEraseBlockSize;
  BOOLEAN                     PcdOverwriteActiveFwPartition;
  UINTN                       ChipId;

  ChipId                        = TegraGetChipID ();
  PcdOverwriteActiveFwPartition = PcdGetBool (PcdOverwriteActiveFwPartition);
  BrBctUpdatePrivate            = NULL;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    ActiveBootChain = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ActiveBootChain;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting active boot chain\n",
      __FUNCTION__
      ));
    return EFI_UNSUPPORTED;
  }

  Status = gBS->LocateProtocol (
                  &gEfiMmCommunication2ProtocolGuid,
                  NULL,
                  (VOID **)&mMmCommProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a - Failed to locate MmCommunication protocol! %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  mMmCommBuffer = AllocateRuntimePool (FW_PARTITION_COMM_BUFFER_SIZE);
  if (mMmCommBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "mMmCommBuffer allocation failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  mMmCommBufferPhysical = mMmCommBuffer;

  Status = FwPartitionDeviceLibInit (ActiveBootChain, MAX_FW_PARTITIONS, PcdOverwriteActiveFwPartition, ChipId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FwPartition lib init failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  mMmInfo = (FW_PARTITION_MM_INFO *)AllocateRuntimeZeroPool (
                                      MAX_FW_PARTITIONS * sizeof (FW_PARTITION_MM_INFO)
                                      );
  if (mMmInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "mMmInfo allocation failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = MmSendInitialize (
             ActiveBootChain,
             PcdOverwriteActiveFwPartition,
             ChipId
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: Error initializing MM interface: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  Status = FPMmAddPartitions (&BrBctEraseBlockSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: Error initializing MM devices: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  // install FwPartition protocols for all partitions
  FwPartitionPrivate = FwPartitionGetPrivateArray ();
  for (Index = 0; Index < FwPartitionGetCount (); Index++, FwPartitionPrivate++) {
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &FwPartitionPrivate->Handle,
                    &gNVIDIAFwPartitionProtocolGuid,
                    &FwPartitionPrivate->Protocol,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Couldn't install protocol for Index=%u, partition=%s: %r\n",
        __FUNCTION__,
        Index,
        FwPartitionPrivate->PartitionInfo.Name,
        Status
        ));
      goto Done;
    }
  }

  Status = BrBctUpdateDeviceLibInit (
             ActiveBootChain,
             BrBctEraseBlockSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error initializing BrBct lib: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  BrBctUpdatePrivate = BrBctUpdateGetPrivate ();
  Status             = gBS->InstallMultipleProtocolInterfaces (
                              &BrBctUpdatePrivate->Handle,
                              &gNVIDIABrBctUpdateProtocolGuid,
                              &BrBctUpdatePrivate->Protocol,
                              NULL
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Couldn't install BR-BCT update protocol: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  FPMmAddressChangeNotify,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mAddressChangeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error creating address change event Status = %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

Done:
  if (EFI_ERROR (Status)) {
    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }

    if ((BrBctUpdatePrivate != NULL) && (BrBctUpdatePrivate->Handle != NULL)) {
      gBS->UninstallMultipleProtocolInterfaces (
             BrBctUpdatePrivate->Handle,
             &gNVIDIABrBctUpdateProtocolGuid,
             &BrBctUpdatePrivate->Protocol,
             NULL
             );
      BrBctUpdatePrivate->Handle = NULL;
    }

    FwPartitionPrivate = FwPartitionGetPrivateArray ();
    for (Index = 0; Index < FwPartitionGetCount (); Index++, FwPartitionPrivate++) {
      if (FwPartitionPrivate->Handle != NULL) {
        EFI_STATUS  LocalStatus;

        LocalStatus = gBS->UninstallMultipleProtocolInterfaces (
                             FwPartitionPrivate->Handle,
                             &gNVIDIAFwPartitionProtocolGuid,
                             &FwPartitionPrivate->Protocol,
                             NULL
                             );
        if (EFI_ERROR (LocalStatus)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Error uninstalling protocol for partition=%s: %r\n",
            __FUNCTION__,
            FwPartitionPrivate->PartitionInfo.Name,
            LocalStatus
            ));
        }

        FwPartitionPrivate->Handle = NULL;
      }
    }

    BrBctUpdateDeviceLibDeinit ();
    FwPartitionDeviceLibDeinit ();

    if (mMmInfo != NULL) {
      FreePool (mMmInfo);
      mMmInfo = NULL;
    }
  }

  return Status;
}
