/** @file
  The main process for L4TLauncher application.

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

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/HandleParsingLib.h>
#include <Library/PrintLib.h>
#include <Library/FileHandleLib.h>
#include <Library/DevicePathLib.h>
#include <Library/AndroidBootImgLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/AndroidBootImg.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

#define GRUB_PATH                      L"EFI\\BOOT\\grubaa64.efi"
#define GRUB_BOOTCONFIG_FILE           L"EFI\\BOOT\\boot.cfg"
#define MAX_BOOTCONFIG_CONTENT_SIZE    512
#define MAX_CBOOTARG_SIZE              256
#define GRUB_BOOTCONFIG_CONTENT_FORMAT "set cbootargs=\"%s\"\r\nset root_partition_number=%d\r\nset bootimg_present=%d\r\nset recovery_present=%d\r\n"

#define BOOTMODE_GRUB_STRING           L"bootmode=grub"
#define BOOTMODE_BOOTIMG_STRING        L"bootmode=bootimg"
#define BOOTMODE_RECOVERY_STRING       L"bootmode=recovery"

#define BOOTMODE_GRUB                  0
#define BOOTMODE_BOOTIMG               1
#define BOOTMODE_RECOVERY              2

#define BOOTCHAIN_OVERRIDE_STRING      L"bootchain="

#define MAX_PARTITION_NAME_SIZE        36 //From the UEFI spec for GPT partitions

#define BOOT_FW_VARIABLE_NAME          L"BootChainFwCurrent"
#define BOOT_OS_VARIABLE_NAME          L"BootChainOsCurrent"
#define BOOT_OS_OVERRIDE_VARIABLE_NAME L"BootChainOsOverride"

#define ROOTFS_BASE_NAME               L"APP"
#define BOOTIMG_BASE_NAME              L"kernel"
#define RECOVERY_BASE_NAME             L"recovery"

typedef struct {
  UINT8 BootMode;
  UINT8 BootChain;
} L4T_BOOT_PARAMS;

/**
  Find the index of the GPT on disk.

  @param[in]  DeviceHandle     The handle of partition.

  @retval Index of the partition.

**/
STATIC
UINT32
EFIAPI
LocatePartitionIndex (
  IN EFI_HANDLE  DeviceHandle
)
{
  EFI_DEVICE_PATH_PROTOCOL *DevicePath;
  HARDDRIVE_DEVICE_PATH    *HardDrivePath;

  if (DeviceHandle == 0) {
    return 0;
  }
  DevicePath = DevicePathFromHandle (DeviceHandle);
  if (DevicePath == NULL) {
    ErrorPrint (L"%a: Unable to find device path\r\n", __FUNCTION__);
    return 0;
  }

  while (!IsDevicePathEndType (DevicePath)) {
    if ((DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == MEDIA_HARDDRIVE_DP)) {
      HardDrivePath = (HARDDRIVE_DEVICE_PATH *)DevicePath;
      return HardDrivePath->PartitionNumber;
    }
    DevicePath = NextDevicePathNode (DevicePath);
  }

  ErrorPrint (L"%a: Unable to locate harddrive device path node\r\n", __FUNCTION__);
  return 0;
}


/**
  Find the partition on the same disk as the loaded image

  Will fall back to the other bootchain if needed

  @param[in]  DeviceHandle     The handle of partition where this file lives on.
  @param[out] PartitionIndex   The partition index on the disk
  @param[out] PartitionHandle  The partition handle

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval EFI_NOT_FOUND  The partition is not on the filesystem.

**/
STATIC
EFI_STATUS
EFIAPI
FindPartitionInfo (
  IN EFI_HANDLE   DeviceHandle,
  IN CONST CHAR16 *PartitionBasename,
  IN UINT8        BootChain,
  OUT UINT32      *PartitionIndex OPTIONAL,
  OUT EFI_HANDLE  *PartitionHandle OPTIONAL
)
{
  EFI_STATUS Status;
  EFI_HANDLE *ParentHandles;
  UINTN      ParentCount;
  UINTN      ParentIndex;
  EFI_HANDLE *ChildHandles;
  UINTN      ChildCount;
  UINTN      ChildIndex;
  UINT32     FoundIndex = 0;
  EFI_PARTITION_INFO_PROTOCOL *PartitionInfo;
  EFI_HANDLE FoundHandle        = 0;
  EFI_HANDLE FoundHandleGeneric = 0;
  EFI_HANDLE FoundHandleAlt     = 0;
  CHAR16     *SubString;
  UINTN      PartitionBasenameLen;

  if (BootChain > 1) {
    return EFI_UNSUPPORTED;
  }

  if (PartitionBasename == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PartitionBasenameLen = StrnLenS (PartitionBasename, MAX_PARTITION_NAME_SIZE);

  Status = PARSE_HANDLE_DATABASE_PARENTS (DeviceHandle, &ParentCount, &ParentHandles);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to find parents - %r\r\n", __FUNCTION__, Status);
    return Status;
  }
  for (ParentIndex = 0; ParentIndex < ParentCount; ParentIndex++) {
    Status = ParseHandleDatabaseForChildControllers (ParentHandles[ParentIndex], &ChildCount, &ChildHandles);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to find child controllers - %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++) {
      Status = gBS->HandleProtocol (ChildHandles[ChildIndex], &gEfiPartitionInfoProtocolGuid, (VOID **)&PartitionInfo);
      if (EFI_ERROR (Status)) {
        continue;
      }

      //Only GPT partitions are supported
      if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
        continue;
      }

      //Look for A/B Names
      if (StrCmp (PartitionInfo->Info.Gpt.PartitionName, PartitionBasename) == 0) {
        ASSERT (FoundHandleGeneric == 0);
        FoundHandleGeneric = ChildHandles[ChildIndex];
      } else if ((PartitionBasenameLen + 2) == StrLen (PartitionInfo->Info.Gpt.PartitionName)) {
        SubString = StrStr (PartitionInfo->Info.Gpt.PartitionName, PartitionBasename);
        if (SubString != NULL) {
          //See if it is a prefix
          if ((SubString == (PartitionInfo->Info.Gpt.PartitionName + 2)) &&
              (PartitionInfo->Info.Gpt.PartitionName[1] == L'_')) {
            if (PartitionInfo->Info.Gpt.PartitionName[0] == (L'A' + BootChain)) {
              ASSERT (FoundHandle == 0);
              FoundHandle = ChildHandles[ChildIndex];
            }
            if (PartitionInfo->Info.Gpt.PartitionName[0] == (L'B' - BootChain)) {
              ASSERT (FoundHandleAlt == 0);
              FoundHandleAlt = ChildHandles[ChildIndex];
            }
          //See if it is a postfix, these are lowercase
          } else if ((SubString == PartitionInfo->Info.Gpt.PartitionName) &&
                     (PartitionInfo->Info.Gpt.PartitionName[PartitionBasenameLen] == L'_')) {
            if (PartitionInfo->Info.Gpt.PartitionName[PartitionBasenameLen + 1] == (L'a' + BootChain)) {
              ASSERT (FoundHandle == 0);
              FoundHandle = ChildHandles[ChildIndex];
            } else if (PartitionInfo->Info.Gpt.PartitionName[PartitionBasenameLen + 1] == (L'b' - BootChain)) {
              ASSERT (FoundHandleAlt == 0);
              FoundHandleAlt = ChildHandles[ChildIndex];
            }
          }
        }
      }
    }
    FreePool (ChildHandles);
  }
  FreePool (ParentHandles);

  if ((FoundHandle == 0) && (FoundHandleGeneric == 0) && (FoundHandleAlt == 0)) {
    return EFI_NOT_FOUND;
  } else if (FoundHandle == 0) {
    if (FoundHandleGeneric != 0) {
      FoundHandle = FoundHandleGeneric;
    } else {
      FoundHandle = FoundHandleAlt;
      Print (L"Falling back to alternative boot path\r\n");
    }
  }

  FoundIndex = LocatePartitionIndex (FoundHandle);

  if (FoundIndex == 0) {
    ErrorPrint (L"%a: Failed to find both partitions index\r\n", __FUNCTION__);
    return EFI_DEVICE_ERROR;
  }

  if (PartitionIndex != NULL) {
    *PartitionIndex = FoundIndex;
  }
  if (PartitionHandle != NULL) {
    *PartitionHandle = FoundHandle;
  }
  return EFI_SUCCESS;
}

/**
  Update the grub boot configuration file

  @param[in]  DeviceHandle     The handle of partition where this file lives on.
  @param[in]  PartitionIndex   Partition number of the root file system
  @param[in]  BootImgPresent   BootImage is present on system
  @param[in]  RecoveryPresent  Recovery kernel partition is present on system

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
UpdateBootCfgFile (
  IN EFI_HANDLE DeviceHandle,
  IN UINT32     PartitionIndex,
  IN BOOLEAN    BootImgPresent,
  IN BOOLEAN    RecoveryPresent
)
{
  EFI_STATUS                Status;
  CHAR8                     CorrectPartitionContent[MAX_BOOTCONFIG_CONTENT_SIZE];
  CHAR8                     ReadPartitionContent[MAX_BOOTCONFIG_CONTENT_SIZE];
  CHAR16                    CpuBootArgs[MAX_CBOOTARG_SIZE/sizeof(CHAR16)];
  UINTN                     CorrectSize;
  UINT64                    FileSize;
  EFI_FILE_HANDLE           FileHandle;
  EFI_DEVICE_PATH          *FullDevicePath;
  ANDROID_BOOTIMG_PROTOCOL *AndroidBootProtocol;

  ZeroMem (CpuBootArgs, MAX_CBOOTARG_SIZE);
  Status = gBS->LocateProtocol (&gAndroidBootImgProtocolGuid, NULL, (VOID **)&AndroidBootProtocol);
  if (!EFI_ERROR (Status)) {
    if (AndroidBootProtocol->AppendArgs != NULL) {
      Status = AndroidBootProtocol->AppendArgs (CpuBootArgs, MAX_CBOOTARG_SIZE);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to get platform addition arguments\r\n", __FUNCTION__);
        return Status;
      }
    }
  }

  CorrectSize = AsciiSPrint (CorrectPartitionContent, MAX_BOOTCONFIG_CONTENT_SIZE, GRUB_BOOTCONFIG_CONTENT_FORMAT, CpuBootArgs, PartitionIndex, BootImgPresent, RecoveryPresent );

  FullDevicePath = FileDevicePath (DeviceHandle, GRUB_BOOTCONFIG_FILE);
  if (FullDevicePath == NULL) {
    ErrorPrint (L"%a: Failed to create file device path\r\n", __FUNCTION__);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = EfiOpenFileByDevicePath (&FullDevicePath, &FileHandle, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to open file: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  Status = FileHandleGetSize (FileHandle, &FileSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to get file size: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  if (FileSize == CorrectSize) {
    ASSERT (FileSize <= MAX_BOOTCONFIG_CONTENT_SIZE);
    Status = FileHandleRead (FileHandle, &FileSize, ReadPartitionContent);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to read current file content: %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    if (CompareMem (CorrectPartitionContent, ReadPartitionContent, CorrectSize) == 0) {
      return EFI_SUCCESS;
    }
  }

  Status = FileHandleSetSize (FileHandle, 0);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to set file size to 0\r\n", __FUNCTION__);
    return Status;
  }

  Status = FileHandleWrite (FileHandle, &CorrectSize, CorrectPartitionContent);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to write file content\r\n", __FUNCTION__);
    return Status;
  }

  FileHandleClose (FileHandle);
  return EFI_SUCCESS;
}

/**
  Update the grub partition configuration files

  @param[in]  DeviceHandle     The handle of partition where this file lives on.
  @param[in]  BootChain        Numeric version of the chain

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
UpdateBootConfig (
  IN EFI_HANDLE DeviceHandle,
  IN UINT8      BootChain
)
{
  UINT32     PartitionIndex;
  EFI_STATUS Status;
  BOOLEAN    BootImgPresent = FALSE;
  BOOLEAN    RecoveryPresent = FALSE;

  Status = FindPartitionInfo (DeviceHandle, ROOTFS_BASE_NAME, BootChain, &PartitionIndex, NULL);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to find rootfs partition info\r\n", __FUNCTION__);
    return Status;
  }

  Status = FindPartitionInfo (DeviceHandle, BOOTIMG_BASE_NAME, BootChain, NULL, NULL);
  if (Status == EFI_SUCCESS) {
    BootImgPresent = TRUE;
  } else if (Status == EFI_NOT_FOUND) {
    BootImgPresent = FALSE;
  } else if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to find bootimg partition info\r\n", __FUNCTION__);
    return Status;
  }

  Status = FindPartitionInfo (DeviceHandle, RECOVERY_BASE_NAME, BootChain, NULL, NULL);
  if (Status == EFI_SUCCESS) {
    RecoveryPresent = TRUE;
  } else if (Status == EFI_NOT_FOUND) {
    RecoveryPresent = FALSE;
  } else if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to find recovery partition info\r\n", __FUNCTION__);
    return Status;
  }

  Status = UpdateBootCfgFile (DeviceHandle, PartitionIndex, BootImgPresent, RecoveryPresent);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Unable to update boot configuration file\r\n");
    return Status;
  }

  return Status;
}

/**
  Process the boot mode selection from command line and variables

  @param[in]  LoadedImage     The LoadedImage protocol for this execution
  @param[out] BootParams      The current boot parameters

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
ProcessBootParams (
  IN  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  OUT L4T_BOOT_PARAMS           *BootParams
)
{
  CONST CHAR16 *CurrentBootOption;
  EFI_STATUS   Status;
  UINT8        BootChain;
  UINTN        DataSize;
  UINT64       StringValue;

  if ((LoadedImage == NULL) || (BootParams == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  BootParams->BootMode = BOOTMODE_GRUB;
  BootParams->BootChain = 0;

  DataSize = sizeof (BootChain);
  Status = gRT->GetVariable (BOOT_FW_VARIABLE_NAME, &gNVIDIATokenSpaceGuid, NULL, &DataSize, &BootChain);
  //If variable does not exist is not 1 byte or have a value larger than 1 boot partition A
  if (!EFI_ERROR (Status) && (BootChain <= 1)) {
    BootParams->BootChain = BootChain;
  }

  //Read override OS boot type
  DataSize = sizeof (BootChain);
  Status = gRT->GetVariable (BOOT_OS_OVERRIDE_VARIABLE_NAME, &gNVIDIATokenSpaceGuid, NULL, &DataSize, &BootChain);
  //If variable does not exist is not 1 byte or have a value larger than 1 boot partition A
  if (!EFI_ERROR (Status) && (BootChain <= 1)) {
    BootParams->BootChain = BootChain;
  }

  //Read current OS boot type to allow for chaining
  DataSize = sizeof (BootChain);
  Status = gRT->GetVariable (BOOT_OS_VARIABLE_NAME, &gNVIDIATokenSpaceGuid, NULL, &DataSize, &BootChain);
  //If variable does not exist is not 1 byte or have a value larger than 1 boot partition A
  if (!EFI_ERROR (Status) && (BootChain <= 1)) {
    BootParams->BootChain = BootChain;
  }

  if (LoadedImage->LoadOptionsSize) {
    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTMODE_GRUB_STRING);
    if (CurrentBootOption != NULL) {
      BootParams->BootMode = BOOTMODE_GRUB;
    }
    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTMODE_BOOTIMG_STRING);
    if (CurrentBootOption != NULL) {
      BootParams->BootMode = BOOTMODE_BOOTIMG;
    }
    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTMODE_RECOVERY_STRING);
    if (CurrentBootOption != NULL) {
      BootParams->BootMode = BOOTMODE_RECOVERY;
    }

    //See if boot option is passed in
    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTCHAIN_OVERRIDE_STRING);
    if (CurrentBootOption != NULL) {
      CurrentBootOption += StrLen (BOOTCHAIN_OVERRIDE_STRING);
      Status = StrDecimalToUint64S (CurrentBootOption, NULL, &StringValue);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"Failed to read boot chain override: %r\r\n", Status);
      } else if (StringValue <= 1) {
        BootParams->BootChain = (UINT8)StringValue;
      } else {
        ErrorPrint (L"Boot chain override value out of range, ignoring\r\n");
      }
    }
  }

  //Store the current boot chain in volatile variable to allow chain loading
  Status = gRT->SetVariable (BOOT_OS_VARIABLE_NAME, &gNVIDIATokenSpaceGuid, EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS, sizeof (BootParams->BootChain), &BootParams->BootChain);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to set OS variable: %r\r\n", Status);
  }

  return EFI_SUCCESS;
}

/**
  Boots an android style partition located with Partition base name and bootchain

  @param[in]  DeviceHandle      The handle of partition where this file lives on.
  @param[in]  PartitionBasename The base name of the partion where the image to boot is located.
  @param[in]  BootChain         Numeric version of the chain


  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
BootAndroidStylePartition (
  IN EFI_HANDLE   DeviceHandle,
  IN CONST CHAR16 *PartitionBasename,
  IN UINT8        BootChain
)
{
  EFI_STATUS             Status;
  EFI_HANDLE             PartitionHandle;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo = NULL;
  EFI_DISK_IO_PROTOCOL   *DiskIo = NULL;
  ANDROID_BOOTIMG_HEADER ImageHeader;
  UINTN                  ImageSize;
  VOID                   *Image;
  UINT32                 Offset = 0;

  Status = FindPartitionInfo (DeviceHandle, PartitionBasename, BootChain, NULL, &PartitionHandle);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to located partition\r\n", __FUNCTION__);
    return Status;
  }

  Status = gBS->HandleProtocol (PartitionHandle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate block io protocol on partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = gBS->HandleProtocol (PartitionHandle, &gEfiDiskIoProtocolGuid, (VOID **)&DiskIo);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate disk io protocol on partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     Offset,
                     sizeof (ANDROID_BOOTIMG_HEADER),
                     &ImageHeader
                    );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to read disk\r\n");
    goto Exit;
  }

  Status = AndroidBootImgGetImgSize (&ImageHeader, &ImageSize);
  if (EFI_ERROR (Status)) {
    Offset = FixedPcdGet32 (PcdBootImgSigningHeaderSize);
    Status = DiskIo->ReadDisk (
                      DiskIo,
                      BlockIo->Media->MediaId,
                      Offset,
                      sizeof (ANDROID_BOOTIMG_HEADER),
                      &ImageHeader
                      );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to read disk\r\n");
      goto Exit;
    }

    Status = AndroidBootImgGetImgSize (&ImageHeader, &ImageSize);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Header not seen at either offset 0 or offset 0x%x\r\n", Offset);
      goto Exit;
    }
  }

  Image = AllocatePool (ImageSize);
  if (Image == NULL) {
    ErrorPrint (L"Failed to allocate buffer for Image\r\n");
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                    DiskIo,
                    BlockIo->Media->MediaId,
                    Offset,
                    ImageSize,
                    Image
                  );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to read disk\r\n");
    goto Exit;
  }

  Status = AndroidBootImgBoot (Image, ImageSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to boot image: %r\r\n", Status);
    goto Exit;
  }

Exit:
  return Status;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers, including
  both device drivers and bus drivers.

  The entry point for StackCheck application that should casue an abort due to stack overwrite.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS    The operation completed successfully.

**/
EFI_STATUS
EFIAPI
L4TLauncher (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
  )
{
  EFI_DEVICE_PATH           *FullDevicePath;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  EFI_STATUS                Status;
  EFI_HANDLE                LoadedImageHandle = 0;
  L4T_BOOT_PARAMS           BootParams;

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate loaded image: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  Status = ProcessBootParams (LoadedImage, &BootParams);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to process boot parameters: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  if (BootParams.BootMode == BOOTMODE_GRUB) {
    do {
      Status = UpdateBootConfig (LoadedImage->DeviceHandle, BootParams.BootChain);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Unable to update partition files\r\n", __FUNCTION__);
        BootParams.BootMode = BOOTMODE_BOOTIMG;
        break;
      }

      FullDevicePath = FileDevicePath (LoadedImage->DeviceHandle, GRUB_PATH);
      if (FullDevicePath == NULL) {
        ErrorPrint (L"%a: Failed to create full device path\r\n", __FUNCTION__);
        BootParams.BootMode = BOOTMODE_BOOTIMG;
        break;
      }

      Status = gBS->LoadImage (FALSE, ImageHandle, FullDevicePath, NULL, 0, &LoadedImageHandle);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Unable to load image: %r\r\n", __FUNCTION__, Status);
        BootParams.BootMode = BOOTMODE_BOOTIMG;
        break;
      }

      Status = gBS->StartImage (LoadedImageHandle, NULL, NULL);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Unable to start image: %r\r\n", __FUNCTION__, Status);
        break;
      }
    } while (FALSE);
  }

  //Not in else to allow fallback
  if (BootParams.BootMode == BOOTMODE_BOOTIMG) {
    Status = BootAndroidStylePartition (LoadedImage->DeviceHandle, BOOTIMG_BASE_NAME, BootParams.BootChain);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to boot %s:%d partition\r\n", BOOTIMG_BASE_NAME, BootParams.BootChain);
    }
  }else if (BootParams.BootMode == BOOTMODE_RECOVERY) {
    Status = BootAndroidStylePartition (LoadedImage->DeviceHandle, RECOVERY_BASE_NAME, BootParams.BootChain);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to boot %s:%d partition\r\n", RECOVERY_BASE_NAME, BootParams.BootChain);
    }
  }
  return Status;
}
