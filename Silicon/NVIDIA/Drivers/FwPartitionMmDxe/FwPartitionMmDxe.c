/** @file

  MM FW partition protocol driver

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/BootChainInfoLib.h>
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

#define FW_PARTITION_MM_DEVICE_INDEX_NORMAL  0
#define FW_PARTITION_MM_DEVICE_INDEX_PSEUDO  1
#define FW_PARTITION_MM_DEVICE_MAX           2

// private MM info structure
typedef struct {
  UINT32                      Signature;
  BOOLEAN                     IsPseudoPartition;
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
FPMmInstallProtocols (
  VOID
  )
{
  EFI_STATUS                 Status;
  UINTN                      Index;
  FW_PARTITION_PRIVATE_DATA  *FwPartitionPrivate;

  Status             = EFI_SUCCESS;
  FwPartitionPrivate = FwPartitionGetPrivateArray ();
  for (Index = 0; Index < FwPartitionGetCount (); Index++, FwPartitionPrivate++) {
    if (FwPartitionPrivate->Handle != NULL) {
      DEBUG ((DEBUG_INFO, "%a: %s protocol already installed\n", __FUNCTION__, FwPartitionPrivate->PartitionInfo.Name));
      continue;
    }

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
    }
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
FPMmRead (
  IN  CONST CHAR16              *PartitionName,
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  OUT VOID                      *Buffer
  )
{
  EFI_STATUS  Status;
  UINTN       ReadBytes;

  Status = EFI_SUCCESS;
  while (Bytes > 0) {
    ReadBytes = MIN (FW_PARTITION_MM_TRANSFER_SIZE, Bytes);

    Status = MmSendReadData (PartitionName, Offset, ReadBytes, Buffer);
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: read %s Offset=%lu, Bytes=%u\n",
      __FUNCTION__,
      PartitionName,
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
  IN  CONST CHAR16              *PartitionName,
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

  Status = MmSendWriteData (PartitionName, Offset, Bytes, Buffer);
  DEBUG ((
    DEBUG_VERBOSE,
    "%a: write %s Offset=%lu, Bytes=%u\n",
    __FUNCTION__,
    PartitionName,
    Offset,
    Bytes
    ));

  if (!EFI_ERROR (Status) && MmInfo->IsPseudoPartition) {
    Status = FPMmInstallProtocols ();
  }

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
  FW_PARTITION_DEVICE_INFO        *DeviceInfo;

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
  DeviceInfo = &mMmInfo[FW_PARTITION_MM_DEVICE_INDEX_NORMAL].DeviceInfo;
  for (Index = 0; Index < Count; Index++) {
    FW_PARTITION_MM_PARTITION_INFO  *PartitionInfo = &PartitionInfoBuffer[Index];

    DEBUG ((DEBUG_INFO, "Found MM Image name=%s\n", PartitionInfo->Name));

    if (StrCmp (PartitionInfo->Name, FW_PARTITION_UPDATE_INACTIVE_PARTITIONS) == 0) {
      Status = FwPartitionAddPseudoPartition (&mMmInfo[FW_PARTITION_MM_DEVICE_INDEX_PSEUDO].DeviceInfo);
    } else {
      Status = FwPartitionAdd (
                 PartitionInfo->Name,
                 DeviceInfo,
                 0,
                 PartitionInfo->Bytes
                 );
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: error adding %s\n", __FUNCTION__, PartitionInfo->Name));
    }

    mNumPartitions++;
  }

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
  for (Index = 0; Index < FW_PARTITION_MM_DEVICE_MAX; Index++, MmInfo++) {
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
  FW_PARTITION_MM_INFO        *MmInfo;

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

  Status = FwPartitionDeviceLibInit (ActiveBootChain, MAX_FW_PARTITIONS, PcdOverwriteActiveFwPartition, ChipId, GetBootChainForGpt ());
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FwPartition lib init failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  mMmInfo = (FW_PARTITION_MM_INFO *)AllocateRuntimeZeroPool (
                                      FW_PARTITION_MM_DEVICE_MAX * sizeof (FW_PARTITION_MM_INFO)
                                      );
  if (mMmInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "mMmInfo allocation failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  MmInfo = mMmInfo;
  for (Index = 0; Index < FW_PARTITION_MM_DEVICE_MAX; Index++, MmInfo++) {
    MmInfo->Signature = FW_PARTITION_MM_INFO_SIGNATURE;
    if (Index == FW_PARTITION_MM_DEVICE_INDEX_PSEUDO) {
      MmInfo->IsPseudoPartition     = TRUE;
      MmInfo->DeviceInfo.DeviceName = L"MMPseudoDevice";
    } else {
      MmInfo->IsPseudoPartition     = FALSE;
      MmInfo->DeviceInfo.DeviceName = L"MMDevice";
    }

    MmInfo->DeviceInfo.DeviceRead  = FPMmRead;
    MmInfo->DeviceInfo.DeviceWrite = FPMmWrite;
    MmInfo->DeviceInfo.BlockSize   = 1;
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
  Status = FPMmInstallProtocols ();
  if (EFI_ERROR (Status)) {
    goto Done;
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
