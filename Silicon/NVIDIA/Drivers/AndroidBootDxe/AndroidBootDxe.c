/** @file

  Android Boot Loader Driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2017, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AndroidBootDxe.h"
#include "AndroidBootConfig.h"
#include <libfdt.h>
#include <Library/PcdLib.h>
#include <PiDxe.h>
#include <Library/HandleParsingLib.h>
#include <Library/AndroidBcbLib.h>
#include <NVIDIAConfiguration.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/SiblingPartitionLib.h>
#include <Library/BootConfigProtocolLib.h>

#include <Library/AvbLib.h>

STATIC EFI_PHYSICAL_ADDRESS       mInitRdBaseAddress     = 0;
STATIC UINT64                     mInitRdSize            = 0;
STATIC SINGLE_VENHW_NODE_DEVPATH  mRcmLoadFileDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH) }
    },
    { 0 }
  },

  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL) }
  }
};

STATIC INITRD_DEVICE_PATH  mInitrdDevicePath = {
  {
    { MEDIA_DEVICE_PATH, MEDIA_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH) }
    },
    LINUX_EFI_INITRD_MEDIA_GUID
  },
  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL) }
  }
};

STATIC CHAR16  *pKernelPartitionDtbMapping[][2] = {
  {
    L"A_kernel", L"A_kernel-dtb"
  },
  {
    L"B_kernel", L"B_kernel-dtb"
  },
  {
    L"kernel", L"kernel-dtb"
  },
  {
    L"kernel_a", L"kernel-dtb_a"
  },
  {
    L"kernel_b", L"kernel-dtb_b"
  },
  {
    L"boot_a", L"kernel-dtb_a"
  },
  {
    L"boot_b", L"kernel-dtb_b"
  }
};

STATIC CHAR16  *pRecoveryKernelPartitionDtbMapping[][2] = {
  {
    L"recovery", L"recovery-dtb"
  },
  {
    L"SOS", L"kernel-dtb"
  }
};

/**
  Copy the new KernelArgs into the KernelArgsProtocol, freeing up the old string if necessary

  @param[in]  This       The KernelArgsProtocol
  @param[in]  NewArgs    The new KernelArgs string (may be NULL)

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_INVALID_PARAMETER  "This" is NULL.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate space for the new args.

**/
#ifndef EDKII_UNIT_TEST_FRAMEWORK_ENABLED
STATIC
#endif
EFI_STATUS
UpdateKernelArgs (
  IN NVIDIA_KERNEL_ARGS_PROTOCOL  *This,
  IN CONST CHAR16                 *NewArgs
  )
{
  UINT32  NewArgsSize;
  UINT32  OldArgsSize;
  CHAR16  *CopiedArgs;

  if (This == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Called with a NULL This pointer\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (NewArgs == NULL) {
    if (This->KernelArgs != NULL) {
      FreePool (This->KernelArgs);
    }

    This->KernelArgs = NULL;
  } else {
    NewArgsSize = StrSize (NewArgs);
    OldArgsSize = (This->KernelArgs) ? StrSize (This->KernelArgs) : 0;

    if (NewArgsSize > OldArgsSize) {
      CopiedArgs = AllocateCopyPool (NewArgsSize, NewArgs);
      if (CopiedArgs == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u bytes for NewKernelArgs\n", __FUNCTION__, NewArgsSize));
        return EFI_OUT_OF_RESOURCES;
      }

      if (This->KernelArgs != NULL) {
        FreePool (This->KernelArgs);
      }

      This->KernelArgs = CopiedArgs;
    } else {
      CopyMem (This->KernelArgs, NewArgs, NewArgsSize);
    }
  }

  return EFI_SUCCESS;
}

/**
  Check if loadfile2 protocol is already installed. If yes,
  uninstall it.

  @param[in]   Event                  Event structure
  @param[in]   Context                Notification context

**/
STATIC
VOID
EFIAPI
AndroidBootOnReadyToBootHandler (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;
  VOID        *Interface;

  gBS->CloseEvent (Event);

  Handle = (EFI_HANDLE)Context;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiLoadFile2ProtocolGuid,
                  (VOID **)&Interface
                  );
  if (!EFI_ERROR (Status)) {
    // If LoadFile2 protocol installed on handle, uninstall it.
    gBS->UninstallMultipleProtocolInterfaces (
           Handle,
           &gEfiLoadFile2ProtocolGuid,
           Interface,
           NULL
           );
  }

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&Interface
                  );
  if (!EFI_ERROR (Status)) {
    // If device path protocol installed on handle, uninstall it.
    gBS->UninstallMultipleProtocolInterfaces (
           Handle,
           &gEfiDevicePathProtocolGuid,
           Interface,
           NULL
           );
  }
}

/**
  Uninstall all protocols for the boot option

  @param[in]   Private       Private driver data for android kernel instance
                             being loaded

**/
STATIC
VOID
EFIAPI
AndroidBootUninstallProtocols (
  IN ANDROID_BOOT_PRIVATE_DATA  *Private
  )
{
  gBS->UninstallMultipleProtocolInterfaces (
         Private->AndroidBootHandle,
         &gEfiLoadFileProtocolGuid,
         &Private->LoadFile,
         &gNVIDIALoadfileKernelArgsProtocol,
         &Private->KernelArgsProtocol,
         &gEfiDevicePathProtocolGuid,
         Private->AndroidBootDevicePath,
         NULL
         );
}

/**
  Check if loadfile is installed for correct config. If not,
  uninstall it.

  @param[in]   Event                  Event structure
  @param[in]   Context                Notification context

**/
STATIC
VOID
EFIAPI
AndroidBootOnConnectCompleteHandler (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                 Status;
  ANDROID_BOOT_PRIVATE_DATA  *Private;
  EFI_HANDLE                 MscHandle;
  MiscCmdType                MiscCmd;
  CHAR16                     PartitionName[MAX_PARTITION_NAME_LEN];
  CHAR16                     *KernelName = NULL;
  UINTN                      Count;
  BOOLEAN                    RecoveryPartitonFound;

  gBS->CloseEvent (Event);

  Private = (ANDROID_BOOT_PRIVATE_DATA *)Context;

  // Check recovery mode
  MscHandle = GetSiblingPartitionHandle (
                Private->ControllerHandle,
                MISC_PARTITION_BASE_NAME
                );
  Status = GetCmdFromMiscPartition (MscHandle, &MiscCmd);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: no misc partition cmd: %r\n", __FUNCTION__, Status));
    return;
  }

  if ((MiscCmd == MISC_CMD_TYPE_RECOVERY) || (MiscCmd == MISC_CMD_TYPE_FASTBOOT_USERSPACE)) {
    Private->RecoveryMode = TRUE;
    DEBUG ((DEBUG_ERROR, "%a: misc command %d, doing recovery\n", __FUNCTION__, MiscCmd));
  }

  if (Private->RecoveryMode) {
    RecoveryPartitonFound = FALSE;
    for (Count = 0;
         Count < sizeof (pRecoveryKernelPartitionDtbMapping) / sizeof (pRecoveryKernelPartitionDtbMapping[0]);
         Count++)
    {
      if (StrCmp (Private->PartitionName, pRecoveryKernelPartitionDtbMapping[Count][0]) == 0) {
        RecoveryPartitonFound = TRUE;
        break;
      }
    }

    if (!RecoveryPartitonFound) {
      AndroidBootUninstallProtocols (Private);
    }
  } else {
    KernelName = PcdGetBool (PcdBootAndroidImage) ? L"boot" : L"kernel";
    Status     = GetActivePartitionName (KernelName, PartitionName);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: couldn't get active name: %r\n", __FUNCTION__, Status));
      return;
    }

    DEBUG ((DEBUG_INFO, "%a: active=%s, partition=%s\n", __FUNCTION__, PartitionName, Private->PartitionName));

    if (StrCmp (Private->PartitionName, PartitionName) != 0) {
      DEBUG ((DEBUG_INFO, "%a: uninstalling name=%s\n", __FUNCTION__, Private->PartitionName));

      AndroidBootUninstallProtocols (Private);
    }
  }
}

/*
  Set the kernel command line into DT.

  @param[in] CmdLine     Kernel command line for DTB.

  @retval EFI_SUCCESS    Command line set correctly.

  @retval Others         Failure.
*/
STATIC
EFI_STATUS
AndroidBootDxeUpdateDtbCmdLine (
  IN CHAR16  *CmdLine
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTreeBase;
  INT32       NodeOffset;
  INT32       Ret;
  UINT32      CommandLineLength;
  CHAR8       *CmdLineDtb;
  VOID        *AcpiBase;

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  DeviceTreeBase = NULL;
  Status         = EfiGetSystemConfigurationTable (&gFdtTableGuid, &DeviceTreeBase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CommandLineLength = StrLen (CmdLine);
  CommandLineLength++;
  CmdLineDtb = NULL;
  Status     = gBS->AllocatePool (
                      EfiBootServicesData,
                      CommandLineLength,
                      (VOID **)&CmdLineDtb
                      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->SetMem (CmdLineDtb, CommandLineLength, 0);

  UnicodeStrToAsciiStrS (CmdLine, CmdLineDtb, CommandLineLength);

  NodeOffset = fdt_path_offset (DeviceTreeBase, "/chosen");
  if (NodeOffset < 0) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  Ret = fdt_setprop (DeviceTreeBase, NodeOffset, "bootargs", CmdLineDtb, CommandLineLength);
  if (Ret < 0) {
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

Exit:
  if (CmdLineDtb != NULL) {
    gBS->FreePool (CmdLineDtb);
  }

  return EFI_SUCCESS;
}

/**
  Locate and install associated device tree

  @param[in]   Private       Private driver data for android kernel instance
                             being loaded

**/
STATIC
VOID
EFIAPI
AndroidBootDxeLoadDtb (
  IN ANDROID_BOOT_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                         Status;
  VOID                               *AcpiBase   = NULL;
  VOID                               *CurrentDtb = NULL;
  INT32                              UefiDtbNodeOffset;
  INT32                              KernelDtbNodeOffset;
  INT32                              PropOffset;
  CONST CHAR8                        *PropName;
  CONST CHAR8                        *PropStr;
  INT32                              PropLen;
  CHAR16                             DtbPartitionName[MAX_PARTITION_NAME_LEN];
  EFI_HANDLE                         DtbPartitionHandle;
  EFI_BLOCK_IO_PROTOCOL              *BlockIo;
  UINT64                             Size;
  VOID                               *KernelDtb = NULL;
  VOID                               *Dtb;
  VOID                               *DtbCopy;
  UINTN                              Count;
  BOOLEAN                            KernelDtbMappingFound;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigUpdate = NULL;
  CHAR8                              NewBootConfigStr[BOOTCONFIG_MAX_LEN];
  CHAR8                              *BootConfigEntry = NULL;
  INT32                              FdtErr;

  if (Private == NULL) {
    return;
  }

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &CurrentDtb);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: No DTB currently installed.\r\n", __FUNCTION__));
  }

  KernelDtbMappingFound = FALSE;

  if (!Private->RecoveryMode) {
    for (Count = 0;
         Count < sizeof (pKernelPartitionDtbMapping) / sizeof (pKernelPartitionDtbMapping[0]);
         Count++)
    {
      if (StrCmp (Private->PartitionName, pKernelPartitionDtbMapping[Count][0]) == 0) {
        StrCpyS (DtbPartitionName, MAX_PARTITION_NAME_LEN, pKernelPartitionDtbMapping[Count][1]);
        KernelDtbMappingFound = TRUE;
        break;
      }
    }
  } else {
    for (Count = 0;
         Count < sizeof (pRecoveryKernelPartitionDtbMapping) / sizeof (pRecoveryKernelPartitionDtbMapping[0]);
         Count++)
    {
      if (StrCmp (Private->PartitionName, pRecoveryKernelPartitionDtbMapping[Count][0]) == 0) {
        StrCpyS (DtbPartitionName, MAX_PARTITION_NAME_LEN, pRecoveryKernelPartitionDtbMapping[Count][1]);
        KernelDtbMappingFound = TRUE;
        break;
      }
    }
  }

  if (!KernelDtbMappingFound) {
    DEBUG ((DEBUG_ERROR, "%a: Using pre-installed DTB if any.\r\n", __FUNCTION__));
    return;
  }

  DtbPartitionHandle = GetSiblingPartitionHandle (
                         Private->ControllerHandle,
                         DtbPartitionName
                         );
  if (DtbPartitionHandle != NULL) {
    Status = gBS->HandleProtocol (
                    DtbPartitionHandle,
                    &gEfiBlockIoProtocolGuid,
                    (VOID **)&BlockIo
                    );
    if (EFI_ERROR (Status) || (BlockIo == NULL)) {
      goto Exit;
    }

    Size = MultU64x32 (BlockIo->Media->LastBlock+1, BlockIo->Media->BlockSize);

    KernelDtb = AllocatePool (Size);
    if (KernelDtb == NULL) {
      goto Exit;
    }

    Status = BlockIo->ReadBlocks (
                        BlockIo,
                        BlockIo->Media->MediaId,
                        0,
                        Size,
                        KernelDtb
                        );
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    Dtb = KernelDtb;
    if (fdt_check_header (Dtb) != 0) {
      Dtb += PcdGet32 (PcdSignedImageHeaderSize);
      if (fdt_check_header (Dtb) != 0) {
        DEBUG ((DEBUG_ERROR, "%a: DTB on partition was corrupted, attempt use to UEFI DTB\r\n", __FUNCTION__));
        goto Exit;
      }
    }

    DtbCopy = NULL;
    // Allowing space for overlays
    DtbCopy = AllocatePages (EFI_SIZE_TO_PAGES (4 * fdt_totalsize (Dtb)));
    if ((DtbCopy != NULL) &&
        (fdt_open_into (Dtb, DtbCopy, 4 * fdt_totalsize (Dtb)) == 0))
    {
      DEBUG ((DEBUG_ERROR, "%a: Installing Kernel DTB from %s\r\n", __FUNCTION__, DtbPartitionName));

      Status = gBS->InstallConfigurationTable (&gFdtTableGuid, DtbCopy);
      if (EFI_ERROR (Status)) {
        gBS->FreePages ((EFI_PHYSICAL_ADDRESS)DtbCopy, EFI_SIZE_TO_PAGES (fdt_totalsize (DtbCopy)));
        DtbCopy = NULL;
      } else {
        if (CurrentDtb != NULL) {
          KernelDtbNodeOffset = fdt_path_offset (DtbCopy, "/chosen");
          UefiDtbNodeOffset   = fdt_path_offset (CurrentDtb, "/chosen");
          if (fdt_get_property (CurrentDtb, UefiDtbNodeOffset, "nvidia,tegra-hypervisor-mode", NULL)) {
            fdt_for_each_property_offset (PropOffset, CurrentDtb, UefiDtbNodeOffset) {
              PropStr = fdt_getprop_by_offset (CurrentDtb, PropOffset, &PropName, &PropLen);
              fdt_setprop (DtbCopy, KernelDtbNodeOffset, PropName, PropStr, PropLen);
            }
          }

          // Modify /chosen/bootconfig
          Status = BootConfigAddSerialNumber (NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add serial number to BootConfigProtocol\n", __FUNCTION__, Status));
          }

          Status = GetBootConfigUpdateProtocol (&BootConfigUpdate);
          if (!EFI_ERROR (Status) && (BootConfigUpdate->BootConfigs != NULL)) {
            BootConfigEntry = (CHAR8 *)fdt_getprop (CurrentDtb, UefiDtbNodeOffset, "bootconfig", &PropLen);
            if (NULL == BootConfigEntry) {
              // Not fatal
              DEBUG ((DEBUG_ERROR, "%a: /chosen/bootconfig not found, creating..\n", __FUNCTION__));
              AsciiSPrint (
                NewBootConfigStr,
                sizeof (NewBootConfigStr),
                "%a",
                BootConfigUpdate->BootConfigs
                );
            } else {
              DEBUG ((DEBUG_ERROR, "%a: Updating /chosen/bootconfig\n", __FUNCTION__));
              AsciiSPrint (
                NewBootConfigStr,
                sizeof (NewBootConfigStr),
                "%a%a",
                BootConfigEntry,
                BootConfigUpdate->BootConfigs
                );
            }

            PropLen = AsciiStrLen (NewBootConfigStr) + 1;
            FdtErr  = fdt_setprop (DtbCopy, KernelDtbNodeOffset, "bootconfig", NewBootConfigStr, PropLen);
            if (FdtErr < 0) {
              // Not fatal
              DEBUG ((DEBUG_ERROR, "%a: Got err=%d trying to set bootconfig\n", __FUNCTION__, FdtErr));
            }
          } else {
            DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get bootconfig Handle, or BootConfigUpdate->BootConfigs is NULL\n", __FUNCTION__, Status));
          }

          gBS->FreePages ((EFI_PHYSICAL_ADDRESS)CurrentDtb, EFI_SIZE_TO_PAGES (fdt_totalsize (CurrentDtb)));
        }
      }
    }
  }

Exit:
  if (KernelDtb != NULL) {
    FreePool (KernelDtb);
    KernelDtb = NULL;
  }
}

/**
  Causes the driver to load a specified file.

  @param  This       Protocol instance pointer.
  @param  FilePath   The device specific path of the file to load.
  @param  BootPolicy Should always be FALSE.
  @param  BufferSize On input the size of Buffer in bytes. On output with a return
                     code of EFI_SUCCESS, the amount of data transferred to
                     Buffer. On output with a return code of EFI_BUFFER_TOO_SMALL,
                     the size of Buffer required to retrieve the requested file.
  @param  Buffer     The memory buffer to transfer the file to. IF Buffer is NULL,
                     then no the size of the requested file is returned in
                     BufferSize.

  @retval EFI_SUCCESS           The file was loaded.
  @retval EFI_UNSUPPORTED       BootPolicy is TRUE.
  @retval EFI_INVALID_PARAMETER FilePath is not a valid device path, or
                                BufferSize is NULL.
  @retval EFI_NO_MEDIA          No medium was present to load the file.
  @retval EFI_DEVICE_ERROR      The file was not loaded due to a device error.
  @retval EFI_NO_RESPONSE       The remote system did not respond.
  @retval EFI_NOT_FOUND         The file was not found
  @retval EFI_ABORTED           The file load process was manually canceled.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current
                                directory entry. BufferSize has been updated with
                                the size needed to complete the request.


**/
EFI_STATUS
EFIAPI
AndroidBootDxeLoadFile2 (
  IN EFI_LOAD_FILE2_PROTOCOL   *This,
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  IN BOOLEAN                   BootPolicy,
  IN OUT UINTN                 *BufferSize,
  IN VOID                      *Buffer OPTIONAL
  )

{
  // Verify if the valid parameters
  if ((This == NULL) || (BufferSize == NULL) || (FilePath == NULL) || !IsDevicePathValid (FilePath, 0)) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: buffer %09p in size %08x\n", __FUNCTION__, Buffer, *BufferSize));

  if (BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  // Check if the given buffer size is big enough
  // EFI_BUFFER_TOO_SMALL gets boot manager allocate a bigger buffer
  if (mInitRdBaseAddress == 0) {
    return EFI_NOT_FOUND;
  }

  if ((Buffer == NULL) || (*BufferSize < mInitRdSize)) {
    *BufferSize = mInitRdSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  // Copy InitRd
  gBS->CopyMem (Buffer, (VOID *)mInitRdBaseAddress, mInitRdSize);

  return EFI_SUCCESS;
}

///
/// Load File Protocol instance
///
GLOBAL_REMOVE_IF_UNREFERENCED
EFI_LOAD_FILE2_PROTOCOL  mAndroidBootDxeLoadFile2 = {
  AndroidBootDxeLoadFile2
};

/**
  Attempt to read data from an Android boot.img to destination buffer. If
  BlockIo and DiskIo are provided, the data will be read from there. Otherwise,
  we'll next try to read from the kernel address set in the DTB. Finally, we'll
  try to use the RCM kernel address.

  @param[in]  BlockIo             Optional. BlockIo protocol interface which is already located.
  @param[in]  DiskIo              Optional. DiskIo protocol interface which is already located.
  @param[in]  Offset              Data offset to read from.
  @param[in]  Buffer              The memory buffer to transfer the data to.
  @param[in]  BufferSize          Size of the memory buffer to transfer the data to.

  @retval EFI_SUCCESS             Operation successful.
  @retval others                  Error occurred
**/
EFI_STATUS
AndroidBootRead (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN EFI_DISK_IO_PROTOCOL   *DiskIo,
  IN UINT32                 Offset,
  OUT VOID                  *Buffer,
  IN UINTN                  BufferSize
  )
{
  VOID        *RcmKernelBase;
  UINT64      KernelStart;
  UINT64      KernelDtbStart;
  EFI_STATUS  Status;

  // Read from BlockIo and DiskIo, if provided.
  if ((BlockIo != NULL) && (DiskIo != NULL)) {
    return DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     Offset,
                     BufferSize,
                     Buffer
                     );
  }

  // Otherwise, try the address specified by the DTB.
  Status = GetKernelAddress (&KernelStart, &KernelDtbStart);
  if (Status == EFI_SUCCESS) {
    gBS->CopyMem (Buffer, (VOID *)((UINTN)KernelStart + Offset), BufferSize);
    return EFI_SUCCESS;
  }

  // Finally, fallback to an RCM boot
  RcmKernelBase = (VOID *)PcdGet64 (PcdRcmKernelBase);
  if (RcmKernelBase != NULL) {
    gBS->CopyMem (Buffer, (VOID *)((UINTN)RcmKernelBase + Offset), BufferSize);
    return EFI_SUCCESS;
  }

  return EFI_INVALID_PARAMETER;
}

/**
  Verify if there is the Android Vendor Boot image file by reading the magic word at the first
  block of the Android Vendor Boot image and save the important size information when a container
  is provided.

  @param[in]  BlockIo             BlockIo protocol interface which is already located.
  @param[in]  DiskIo              DiskIo protocol interface which is already located.
  @param[out] ImgData             A pointer to the internal data structure to retain
                                  the important size data of kernel and initrd images
                                  contained in the Android Boot image header.

  @retval EFI_SUCCESS             Operation successful.
  @retval others                  Error occurred
**/
EFI_STATUS
VendorBootGetVerify (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  OUT VENDOR_BOOT_DATA       *ImgData OPTIONAL,
  OUT CHAR16                 *KernelArgs OPTIONAL
  )
{
  EFI_STATUS                   Status;
  VENDOR_BOOTIMG_TYPE4_HEADER  *Header;
  UINT32                       Offset;
  UINTN                        PartitionSize;
  UINTN                        ImageSize;
  UINT32                       PageSize;
  UINT32                       VendorRamdiskSize;
  UINT32                       BootConfigSize;
  UINT32                       DtbSize;
  UINT32                       VendorRamdiskTableSize;
  CHAR8                        *HeaderKernelArgs;

  // Allocate a buffer large enough to hold any header type.
  Header = (VENDOR_BOOTIMG_TYPE4_HEADER *)AllocatePool (sizeof (VENDOR_BOOTIMG_TYPE4_HEADER));
  if (Header == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Read enough to get the header version.
  // - This is a minimal read.  We can assume it will fit in Header as it was
  // sized for a full header.
  Offset = 0;
  Status = AndroidBootRead (
             BlockIo,
             DiskIo,
             Offset,
             Header,
             sizeof (VENDOR_BOOTIMG_TYPE4_HEADER)
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Make sure it's an Android vendor boot image
  if (AsciiStrnCmp (
        (CONST CHAR8 *)Header->Magic,
        VENDOR_BOOT_MAGIC,
        VENDOR_BOOT_MAGIC_SIZE
        ) != 0)
  {
    goto Exit;
  }

  // We have an Android vendor boot image.
  // Vendor boot support version 4 for GKI
  switch (Header->HeaderVersion) {
    case 4:
      PageSize               = Header->PageSize;
      VendorRamdiskSize      = Header->VendorRamdiskSize;
      DtbSize                = Header->DtbSize;
      VendorRamdiskTableSize = Header->VendorRamdiskTableSize;
      BootConfigSize         = Header->BootConfigSize;
      HeaderKernelArgs       = Header->KernelArgs;

      break;

    default:
      // Unsupported header type
      Status = EFI_INCOMPATIBLE_VERSION;
      DEBUG ((DEBUG_ERROR, "%a: Unsupported header type %x\n", __FUNCTION__, Header->HeaderVersion));
      goto Exit;
  }

  // The page size is not specified, but it should be power of 2 at least
  if (!IS_VALID_ANDROID_PAGE_SIZE (PageSize)) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Make sure that the image fits in the partition
  if ((BlockIo != NULL) && (DiskIo != NULL)) {
    // Ignore RcmLoad case for now
    PartitionSize = (UINTN)(BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
    ImageSize     = Offset + PageSize
                    + ALIGN_VALUE (VendorRamdiskSize, PageSize);
    if (ImageSize > PartitionSize) {
      Status = EFI_NOT_FOUND;
      goto Exit;
    }
  }

  // Set up the internal data structure when ImgData is not NULL
  if (ImgData != NULL) {
    // Calculate a size of the kernel image, aligned in BlockSize
    // This size will be a reference when boot manger allocates a pool for LoadFile service
    // Kernel image to be loaded to a buffer allocated by boot manager
    // Ramdisk image to be loaded to a buffer allocated by this LoadFile service
    ImgData->Offset                 = Offset;
    ImgData->VendorRamdiskSize      = VendorRamdiskSize;
    ImgData->PageSize               = PageSize;
    ImgData->HeaderVersion          = Header->HeaderVersion;
    ImgData->DtbSize                = DtbSize;
    ImgData->VendorRamdiskTableSize = VendorRamdiskTableSize;
    ImgData->BootConfigSize         = BootConfigSize;
  }

  if (KernelArgs != NULL) {
    AsciiStrToUnicodeStrS (HeaderKernelArgs, KernelArgs, VENDOR_BOOT_ARGS_SIZE);
  }

  Status = EFI_SUCCESS;

Exit:
  FreePool (Header);

  return Status;
}

/**
  Verify if there is the Android Boot image file by reading the magic word at the first
  block of the Android Boot image and save the important size information when a container
  is provided.

  @param[in]  BlockIo             BlockIo protocol interface which is already located.
  @param[in]  DiskIo              DiskIo protocol interface which is already located.
  @param[out] ImgData             A pointer to the internal data structure to retain
                                  the important size data of kernel and initrd images
                                  contained in the Android Boot image header.
  @param[out] KernelArgs          A pointer to unicode array that stores kernel args
                                  contained in the Android Boot image header.

  @retval EFI_SUCCESS             Operation successful.
  @retval others                  Error occurred
**/
EFI_STATUS
AndroidBootGetVerify (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  OUT ANDROID_BOOT_DATA      *ImgData OPTIONAL,
  OUT CHAR16                 *KernelArgs OPTIONAL
  )
{
  EFI_STATUS                      Status;
  VOID                            *Header;
  ANDROID_BOOTIMG_VERSION_HEADER  *VersionHeader;
  ANDROID_BOOTIMG_TYPE0_HEADER    *Type0Header;
  ANDROID_BOOTIMG_TYPE1_HEADER    *Type1Header;
  ANDROID_BOOTIMG_TYPE2_HEADER    *Type2Header;
  ANDROID_BOOTIMG_TYPE3_HEADER    *Type3Header;
  ANDROID_BOOTIMG_TYPE4_HEADER    *Type4Header;
  UINT32                          Offset;
  UINT32                          SignatureHeaderSize;
  UINTN                           PartitionSize;
  UINTN                           ImageSize;
  UINT32                          PageSize;
  UINT32                          KernelSize;
  UINT32                          RamdiskSize;
  CHAR8                           *HeaderKernelArgs;
  UINT64                          KernelStart;
  UINT64                          KernelDtbStart;

  // Allocate a buffer large enough to hold any header type.
  // - This chain of MAX's is awkward looking, but it's safe and the compiler
  // will optimize this down to a single number anyway.
  Header = AllocatePool (
             MAX (
               MAX (
                 MAX (
                   MAX (
                     sizeof (ANDROID_BOOTIMG_TYPE0_HEADER),
                     sizeof (ANDROID_BOOTIMG_TYPE1_HEADER)
                     ),
                   sizeof (ANDROID_BOOTIMG_TYPE2_HEADER)
                   ),
                 sizeof (ANDROID_BOOTIMG_TYPE3_HEADER)
                 ),
               sizeof (ANDROID_BOOTIMG_TYPE4_HEADER)
               )
             );
  if (Header == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SignatureHeaderSize = PcdGet32 (PcdSignedImageHeaderSize);

  // Read enough to get the header version.
  // - This is a minimal read.  We can assume it will fit in Header as it was
  // sized for a full header.
  Offset = 0;
  Status = AndroidBootRead (
             BlockIo,
             DiskIo,
             Offset,
             Header,
             sizeof (ANDROID_BOOTIMG_VERSION_HEADER)
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  VersionHeader = (ANDROID_BOOTIMG_VERSION_HEADER *)Header;

  // Make sure it's an Android boot image
  if (AsciiStrnCmp (
        (CONST CHAR8 *)VersionHeader->BootMagic,
        ANDROID_BOOT_MAGIC,
        ANDROID_BOOT_MAGIC_LENGTH
        ) != 0)
  {
    // It's not an Android boot image.  We might need to skip
    // past our signature.
    Status = EFI_NOT_FOUND;
    if (SignatureHeaderSize != 0) {
      Offset = SignatureHeaderSize;
      Status = AndroidBootRead (
                 BlockIo,
                 DiskIo,
                 Offset,
                 Header,
                 sizeof (ANDROID_BOOTIMG_VERSION_HEADER)
                 );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      VersionHeader = (ANDROID_BOOTIMG_VERSION_HEADER *)Header;

      // Check for an Android boot image again
      if (AsciiStrnCmp (
            (CONST CHAR8 *)VersionHeader->BootMagic,
            ANDROID_BOOT_MAGIC,
            ANDROID_BOOT_MAGIC_LENGTH
            ) != 0)
      {
        Status = EFI_NOT_FOUND;
      }
    }

    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  // We have an Android boot image.

  // Handle each version
  switch (VersionHeader->HeaderVersion) {
    case 0:
      // Read the full Type0 header.
      Status = AndroidBootRead (
                 BlockIo,
                 DiskIo,
                 Offset,
                 Header,
                 sizeof (ANDROID_BOOTIMG_TYPE0_HEADER)
                 );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      Type0Header      = (ANDROID_BOOTIMG_TYPE0_HEADER *)Header;
      PageSize         = Type0Header->PageSize;
      KernelSize       = Type0Header->KernelSize;
      RamdiskSize      = Type0Header->RamdiskSize;
      HeaderKernelArgs = Type0Header->KernelArgs;

      break;

    case 1:
      // Read the full Type1 header.
      Status = AndroidBootRead (
                 BlockIo,
                 DiskIo,
                 Offset,
                 Header,
                 sizeof (ANDROID_BOOTIMG_TYPE1_HEADER)
                 );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      Type1Header      = (ANDROID_BOOTIMG_TYPE1_HEADER *)Header;
      PageSize         = Type1Header->PageSize;
      KernelSize       = Type1Header->KernelSize;
      RamdiskSize      = Type1Header->RamdiskSize;
      HeaderKernelArgs = Type1Header->KernelArgs;

      break;

    case 2:
      // Read the full Type2 header.
      Status = AndroidBootRead (
                 BlockIo,
                 DiskIo,
                 Offset,
                 Header,
                 sizeof (ANDROID_BOOTIMG_TYPE2_HEADER)
                 );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      Type2Header      = (ANDROID_BOOTIMG_TYPE2_HEADER *)Header;
      PageSize         = Type2Header->PageSize;
      KernelSize       = Type2Header->KernelSize;
      RamdiskSize      = Type2Header->RamdiskSize;
      HeaderKernelArgs = Type2Header->KernelArgs;

      break;

    case 3:
      // Read the full Type3 header.
      Status = AndroidBootRead (
                 BlockIo,
                 DiskIo,
                 Offset,
                 Header,
                 sizeof (ANDROID_BOOTIMG_TYPE3_HEADER)
                 );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      Type3Header      = (ANDROID_BOOTIMG_TYPE3_HEADER *)Header;
      PageSize         = SIZE_4KB;
      KernelSize       = Type3Header->KernelSize;
      RamdiskSize      = Type3Header->RamdiskSize;
      HeaderKernelArgs = Type3Header->KernelArgs;

      break;

    case 4:
      // Read the full Type4 header.
      Status = AndroidBootRead (
                 BlockIo,
                 DiskIo,
                 Offset,
                 Header,
                 sizeof (ANDROID_BOOTIMG_TYPE4_HEADER)
                 );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      Type4Header      = (ANDROID_BOOTIMG_TYPE4_HEADER *)Header;
      PageSize         = SIZE_4KB;
      KernelSize       = Type4Header->KernelSize;
      RamdiskSize      = Type4Header->RamdiskSize;
      HeaderKernelArgs = Type4Header->KernelArgs;

      break;

    default:
      // Unsupported header type
      Status = EFI_INCOMPATIBLE_VERSION;
      DEBUG ((DEBUG_ERROR, "%a: Unsupported header type %x\n", __FUNCTION__, VersionHeader->HeaderVersion));
      goto Exit;
  }

  // The page size is not specified, but it should be power of 2 at least
  if (!IS_VALID_ANDROID_PAGE_SIZE (PageSize)) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Make sure that the image fits in the partition.  If the image is bigger
  // than the space allocated, then we have to assume it has been truncated and
  // we don't want to use it.
  ImageSize = Offset + PageSize
              + ALIGN_VALUE (KernelSize, PageSize)
              + ALIGN_VALUE (RamdiskSize, PageSize);

  if ((BlockIo != NULL) && (DiskIo != NULL)) {
    // We're booting from a partition.  Get the size from the Media descriptor.
    PartitionSize = (UINTN)(BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  } else {
    // We're booting from memory.
    Status = GetKernelAddress (&KernelStart, &KernelDtbStart);
    if (Status == EFI_SUCCESS) {
      // When the kernel is handed off to us via the DTB, a size is not provided.
      // Just assume the partition is big enough.
      PartitionSize = ImageSize;
    } else {
      // Otherwise, assume this is an RCM load.
      PartitionSize = PcdGet64 (PcdRcmKernelSize);
    }
  }

  if (ImageSize > PartitionSize) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Set up the internal data structure when ImgData is not NULL
  if (ImgData != NULL) {
    // Calculate a size of the kernel image, aligned in BlockSize
    // This size will be a reference when boot manger allocates a pool for LoadFile service
    // Kernel image to be loaded to a buffer allocated by boot manager
    // Ramdisk image to be loaded to a buffer allocated by this LoadFile service
    ImgData->Offset        = Offset;
    ImgData->KernelSize    = KernelSize;
    ImgData->RamdiskSize   = RamdiskSize;
    ImgData->PageSize      = PageSize;
    ImgData->HeaderVersion = VersionHeader->HeaderVersion;
  }

  if (KernelArgs != NULL) {
    AsciiStrToUnicodeStrS (HeaderKernelArgs, KernelArgs, ANDROID_BOOTIMG_KERNEL_ARGS_SIZE);
  }

  Status = EFI_SUCCESS;

Exit:
  FreePool (Header);

  return Status;
}

/**
  Verify if there is the Android Init Boot image file by reading the magic word at the first
  block of the Android Init Boot image and save the important size information when a container
  is provided.

  @param[in]  BlockIo             BlockIo protocol interface which is already located.
  @param[in]  DiskIo              DiskIo protocol interface which is already located.
  @param[out] ImgData             A pointer to the internal data structure to retain
                                  the important size data of ramdisk images contained
                                  in the Android Init Boot image header.

  @retval EFI_SUCCESS             Operation successful.
  @retval others                  Error occurred
**/
STATIC
EFI_STATUS
InitBootGetVerify (
  IN  EFI_BLOCK_IO_PROTOCOL   *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL    *DiskIo,
  OUT ANDROID_INIT_BOOT_DATA  *ImgData
  )
{
  EFI_STATUS                    Status;
  ANDROID_BOOTIMG_TYPE4_HEADER  *Header;
  UINTN                         PartitionSize;
  UINTN                         ImageSize;
  UINT32                        PageSize;
  UINT32                        RamdiskSize;

  if ((BlockIo == NULL) || (DiskIo == NULL) || (ImgData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // Allocate a buffer to hold the type4 header.
  Header = (ANDROID_BOOTIMG_TYPE4_HEADER *)AllocatePool (sizeof (ANDROID_BOOTIMG_TYPE4_HEADER));
  if (Header == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Read the type4 header.
  Status = AndroidBootRead (
             BlockIo,
             DiskIo,
             0,
             Header,
             sizeof (ANDROID_BOOTIMG_TYPE4_HEADER)
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Make sure it's a kind of Android boot image
  if (AsciiStrnCmp (
        (CONST CHAR8 *)Header->BootMagic,
        ANDROID_BOOT_MAGIC,
        ANDROID_BOOT_MAGIC_LENGTH
        ) != 0)
  {
    goto Exit;
  }

  // Init boot only support version 4
  switch (Header->HeaderVersion) {
    case 4:
      PageSize    = SIZE_4KB;
      RamdiskSize = Header->RamdiskSize;
      break;

    default:
      // Unsupported header type
      Status = EFI_INCOMPATIBLE_VERSION;
      DEBUG ((DEBUG_ERROR, "%a: Unsupported header type %x\n", __FUNCTION__, Header->HeaderVersion));
      goto Exit;
  }

  // The page size is not specified, but it should be power of 2 at least
  if (!IS_VALID_ANDROID_PAGE_SIZE (PageSize)) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Make sure that the image fits in the partition
  PartitionSize = (UINTN)(BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  ImageSize     = PageSize
                  + ALIGN_VALUE (RamdiskSize, PageSize);
  if (ImageSize > PartitionSize) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Set up the internal data structure
  ImgData->RamdiskSize   = RamdiskSize;
  ImgData->PageSize      = PageSize;
  ImgData->HeaderVersion = Header->HeaderVersion;
  Status                 = EFI_SUCCESS;

Exit:
  FreePool (Header);

  return Status;
}

/**
  Attempt to setup access to the Android Init Boot image by acquiring
  the necessary protocol interfaces and validating the image header.

  @param[in]  ControllerHandle    The handle of the controller for init_boot. This handle
                                  must support a protocol interface that supplies
                                  an I/O abstraction to the driver.
  @param[out] BlockIo             BlockIo protocol interface which is to be located.
  @param[out] DiskIo              DiskIo protocol interface which is to be located.
  @param[out] ImgData             A pointer to the internal data structure to retain
                                  the important size data of ramdisk images contained
                                  in the Android Init Boot image header.

  @retval EFI_SUCCESS             Operation successful.
  @retval others                  Error occurred
**/
STATIC
EFI_STATUS
InitBootSetup (
  IN EFI_HANDLE               ControllerHandle,
  OUT EFI_BLOCK_IO_PROTOCOL   **BlockIo,
  OUT EFI_DISK_IO_PROTOCOL    **DiskIo,
  OUT ANDROID_INIT_BOOT_DATA  *ImgData
  )
{
  EFI_STATUS              Status;
  ANDROID_INIT_BOOT_DATA  InitImgData;
  EFI_DISK_IO_PROTOCOL    *InitDiskIo  = NULL;
  EFI_BLOCK_IO_PROTOCOL   *InitBlockIo = NULL;

  if (ControllerHandle == NULL) {
    return EFI_NOT_FOUND;
  }

  if ((BlockIo == NULL) || (DiskIo == NULL) || (ImgData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&InitBlockIo
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&InitDiskIo
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Examine if the Android Init Boot Image can be found
  Status = InitBootGetVerify (InitBlockIo, InitDiskIo, &InitImgData);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *DiskIo  = InitDiskIo;
  *BlockIo = InitBlockIo;
  *ImgData = InitImgData;

  return EFI_SUCCESS;
}

/**
  Attempt to load the kernel and initrd from the Android Boot image.
  Allocate pages reserved in BootService for the initrd image to persist until
  the completion of the kernel booting.

  @param[in]  BlockIo             BlockIo protocol interface which is already located.
  @param[in]  DiskIo              DiskIo protocol interface which is already located.
  @param[in]  ImgData             A pointer to the internal data structure to retain
                                  the important size data of kernel and initrd images
                                  contained in the Android Boot image header.
  @param[in]  VendorBlockIo       BlockIo protocol interface for vendor_boot partition.
  @param[in]  VendorDiskIo        DiskIo protocol interface for vendor_boot partition.
  @param[in]  VendorImgData       A pointer to the internal data structure to retain
                                  the important size data of vendor_boot img contained
                                  in the Android Vendor Boot header.
  @param[in]  InitBootBlockIo     BlockIo protocol interface for init_boot partition.
  @param[in]  InitBootDiskIo      DiskIo protocol interface for init_boot partition.
  @param[in]  InitBootImgData     A pointer to the internal data structure to retain
                                  the important size data of init_boot img contained
                                  in the Android Init Boot header.
  @param[in]  Buffer              The memory buffer to transfer the file to.

  @retval EFI_SUCCESS             Operation successful.
  @retval others                  Error occurred
**/
EFI_STATUS
AndroidBootLoadFile (
  IN EFI_BLOCK_IO_PROTOCOL   *BlockIo,
  IN EFI_DISK_IO_PROTOCOL    *DiskIo,
  IN ANDROID_BOOT_DATA       *ImgData,
  IN EFI_BLOCK_IO_PROTOCOL   *VendorBlockIo,
  IN EFI_DISK_IO_PROTOCOL    *VendorDiskIo,
  IN VENDOR_BOOT_DATA        *VendorImgData,
  IN EFI_BLOCK_IO_PROTOCOL   *InitBootBlockIo,
  IN EFI_DISK_IO_PROTOCOL    *InitBootDiskIo,
  IN ANDROID_INIT_BOOT_DATA  *InitBootImgData,
  IN VOID                    *Buffer
  )
{
  EFI_STATUS   Status;
  EFI_HANDLE   InitrdHandle;
  EFI_EVENT    InitrdEvent;
  UINTN        Addr;
  UINTN        BufSize;
  UINTN        BufBase;
  UINTN        BufBaseRamdisk;
  UINTN        BufSizeRamdisk         = 0;
  UINTN        BootConfigReservedSize = 0;
  MiscCmdType  MiscCmd;
  UINT32       BootConfigAppliedBytes = 0;
  UINT32       BootConfigSize;

  mInitRdBaseAddress = 0;
  mInitRdSize        = 0;

  // Android Boot image enabled in EFI stub feature consists of:
  // - Header info in PageSize that contains Android Boot image header
  // - Kernel image in EFI format as built in EFI stub feature
  // - Ramdisk image
  // - more as described in Android Boot image header
  // Note: Every image data is aligned in PageSize

  // Load the kernel
  if (ImgData->KernelSize == 0) {
    return EFI_NOT_FOUND;
  }

  Addr    = ImgData->PageSize + ImgData->Offset;
  BufSize = ImgData->KernelSize;
  BufBase = (UINTN)Buffer;
  Status  = AndroidBootRead (
              BlockIo,
              DiskIo,
              Addr,
              (VOID *)BufBase,
              BufSize
              );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Unable to read disk for kernel image: from offset %x" \
      " to %09p: %r\n",
      __FUNCTION__,
      Addr,
      BufBase,
      Status
      ));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Kernel image copied to %09p in size %08x\n", __FUNCTION__, BufBase, BufSize));

  // Load the initial ramdisk
  if ((!PcdGetBool (PcdBootAndroidImage) || (ImgData->HeaderVersion < 3)) && (ImgData->RamdiskSize == 0)) {
    mInitRdBaseAddress = 0;
    mInitRdSize        = 0;
    return Status;
  }

  // Check if init_boot exists as fallback
  if ((ImgData->RamdiskSize == 0) && (InitBootImgData != NULL)) {
    BufSize = InitBootImgData->RamdiskSize;
    DEBUG ((DEBUG_INFO, "%a: use init_boot to load ramdisk\n", __FUNCTION__));
  } else {
    BufSize = ImgData->RamdiskSize;
  }

  // Ramdisk buf size is generic_boot ramdisk + vendor_boot ramdisk
  // if kernel boot and vendor_boot ramdisk exists
  Status = GetCmdFromMiscPartition (NULL, &MiscCmd);
  if (  !EFI_ERROR (Status) && (MiscCmd != MISC_CMD_TYPE_RECOVERY) && (MiscCmd != MISC_CMD_TYPE_FASTBOOT_USERSPACE)
     && (VendorImgData != NULL))
  {
    BufSize += VendorImgData->VendorRamdiskSize;
  }

  BufSize               += BOOTCONFIG_RESERVED_SIZE;
  BootConfigReservedSize = BOOTCONFIG_RESERVED_SIZE;

  // Allocate a buffer reserved in EfiBootServicesData
  // to make this buffer persist until the completion of kernel booting
  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiBootServicesData,
                  EFI_SIZE_TO_PAGES (BufSize),
                  (EFI_PHYSICAL_ADDRESS *)&BufBase
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to get a buffer for ramdisk: %r\n", __FUNCTION__, Status));
    return Status;
  }

  BufBaseRamdisk = BufBase;
  BufSizeRamdisk = BufSize - BootConfigReservedSize;

  // recovery kernel has dedicated ramdisk in recovery.img
  Status = GetCmdFromMiscPartition (NULL, &MiscCmd);
  if (  !EFI_ERROR (Status) && (MiscCmd != MISC_CMD_TYPE_RECOVERY) && (MiscCmd != MISC_CMD_TYPE_FASTBOOT_USERSPACE)
     && (VendorImgData != NULL))
  {
    // ramdisk layout in memory
    // - vendor_boot ramdisk, followed by
    // - generic_boot ramdisk, then
    // - boot_config
    Addr    = ALIGN_VALUE (sizeof (VENDOR_BOOTIMG_TYPE4_HEADER), VendorImgData->PageSize);
    BufSize = VendorImgData->VendorRamdiskSize;
    Status  = AndroidBootRead (
                VendorBlockIo,
                VendorDiskIo,
                Addr,
                (VOID *)BufBase,
                BufSize
                );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unable to read disk for vendor ramdisk from offset %x" \
        " to %09p: %r\n",
        __FUNCTION__,
        Addr,
        BufBase,
        Status
        ));
      goto ErrorExit;
    }

    DEBUG ((DEBUG_INFO, "%a: Vendor RamDisk loaded to %09p in size %08x\n", __FUNCTION__, BufBase, BufSize));

    // Android Boot Ramdisk is following Vendor Boot Ramdisk
    BufBase += BufSize;
  }

  if ((ImgData->RamdiskSize == 0) && (InitBootImgData != NULL)) {
    Addr    = ALIGN_VALUE (sizeof (VENDOR_BOOTIMG_TYPE4_HEADER), InitBootImgData->PageSize);
    BufSize = InitBootImgData->RamdiskSize;
    Status  = AndroidBootRead (
                InitBootBlockIo,
                InitBootDiskIo,
                Addr,
                (VOID *)BufBase,
                BufSize
                );
  } else {
    Addr = ImgData->PageSize + ImgData->Offset + \
           ALIGN_VALUE (ImgData->KernelSize, ImgData->PageSize);
    BufSize = ImgData->RamdiskSize;
    Status  = AndroidBootRead (
                BlockIo,
                DiskIo,
                Addr,
                (VOID *)BufBase,
                BufSize
                );
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Unable to read disk for ramdisk from offset %x" \
      " to %09p: %r\n",
      __FUNCTION__,
      Addr,
      BufBase,
      Status
      ));
    goto ErrorExit;
  }

  DEBUG ((DEBUG_INFO, "%a: RamDisk loaded to %09p in size %08x\n", __FUNCTION__, BufBase, BufSize));

  if (ImgData->HeaderVersion >= 3) {
    Status = GetCmdFromMiscPartition (NULL, &MiscCmd);
    if (  !EFI_ERROR (Status) && (VendorImgData != NULL)) {
      // load BootConfig right behind the ramdisk memory
      BufBase += BufSize;

      Addr = ALIGN_VALUE (sizeof (VENDOR_BOOTIMG_TYPE4_HEADER), VendorImgData->PageSize) + \
             ALIGN_VALUE (VendorImgData->VendorRamdiskSize, VendorImgData->PageSize) + \
             ALIGN_VALUE (VendorImgData->DtbSize, VendorImgData->PageSize) + \
             ALIGN_VALUE (VendorImgData->VendorRamdiskTableSize, VendorImgData->PageSize);
      BufSize = VendorImgData->BootConfigSize;

      Status = AndroidBootRead (
                 VendorBlockIo,
                 VendorDiskIo,
                 Addr,
                 (VOID *)BufBase,
                 BufSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Unable to read disk for bootconfig from offset %x" \
          " to %09p: %r\n",
          __FUNCTION__,
          Addr,
          BufBase,
          Status
          ));
        goto ErrorExit;
      }

      Status = AddBootConfigTrailer ((UINT64)BufBase, VendorImgData->BootConfigSize, &BootConfigAppliedBytes);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: BootConfig trailer create failed\n", __FUNCTION__));
        goto ErrorExit;
      }

      BootConfigSize = VendorImgData->BootConfigSize + BootConfigAppliedBytes;

      // Append dtb's /chosen/bootconfig to BootConfig memory
      Status = AddBootConfigFromDtb (
                 (UINT64)BufBase,
                 BootConfigSize,
                 &BootConfigAppliedBytes
                 );
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      BootConfigSize += BootConfigAppliedBytes;
      BufSizeRamdisk += BootConfigSize;

      DEBUG ((DEBUG_ERROR, "%a: BootConfig loaded to %09p in size %08x\n", __FUNCTION__, BufBase, BufSize));
    }
  }

  mInitRdBaseAddress = BufBaseRamdisk;
  mInitRdSize        = BufSizeRamdisk;

  if (mInitRdSize != 0) {
    InitrdHandle = NULL;
    Status       = gBS->InstallMultipleProtocolInterfaces (
                          &InitrdHandle,
                          &gEfiLoadFile2ProtocolGuid,
                          &mAndroidBootDxeLoadFile2,
                          &gEfiDevicePathProtocolGuid,
                          &mInitrdDevicePath,
                          NULL
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to install initrd: %r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    InitrdEvent = NULL;
    Status      = gBS->CreateEventEx (
                         EVT_NOTIFY_SIGNAL,
                         TPL_CALLBACK,
                         AndroidBootOnReadyToBootHandler,
                         InitrdHandle,
                         &gEfiEventReadyToBootGuid,
                         &InitrdEvent
                         );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to create initrd callback: %r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  return Status;

ErrorExit:
  gBS->FreePages ((EFI_PHYSICAL_ADDRESS)BufBase, EFI_SIZE_TO_PAGES (BufSizeRamdisk));
  mInitRdBaseAddress = 0;
  mInitRdSize        = 0;
  return Status;
}

/**
  Causes the driver to load a specified file.

  @param  This                  Protocol instance pointer.
  @param  FilePath              The device specific path of the file to load.
  @param  BootPolicy            If TRUE, indicates that the request originates from the
                                boot manager is attempting to load FilePath as a boot
                                selection. If FALSE, then FilePath must match as exact file
                                to be loaded.
  @param  BufferSize            On input the size of Buffer in bytes. On output with a return
                                code of EFI_SUCCESS, the amount of data transferred to
                                Buffer. On output with a return code of EFI_BUFFER_TOO_SMALL,
                                the size of Buffer required to retrieve the requested file.
  @param  Buffer                The memory buffer to transfer the file to. IF Buffer is NULL,
                                then the size of the requested file is returned in
                                BufferSize.

  @retval EFI_SUCCESS           The file was loaded.
  @retval EFI_UNSUPPORTED       The device does not support the provided BootPolicy
  @retval EFI_INVALID_PARAMETER FilePath is not a valid device path, or
                                BufferSize is NULL.
  @retval EFI_NO_MEDIA          No medium was present to load the file.
  @retval EFI_DEVICE_ERROR      The file was not loaded due to a device error.
  @retval EFI_NO_RESPONSE       The remote system did not respond.
  @retval EFI_NOT_FOUND         The file was not found.
  @retval EFI_ABORTED           The file load process was manually cancelled.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current directory entry.
                                BufferSize has been updated with the size needed to complete
                                the request.

**/
EFI_STATUS
EFIAPI
AndroidBootDxeLoadFile (
  IN EFI_LOAD_FILE_PROTOCOL    *This,
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  IN BOOLEAN                   BootPolicy,
  IN OUT UINTN                 *BufferSize,
  IN VOID                      *Buffer OPTIONAL
  )

{
  EFI_STATUS                 Status;
  ANDROID_BOOT_PRIVATE_DATA  *Private;
  ANDROID_BOOT_DATA          ImgData;
  VENDOR_BOOT_DATA           VendorImgData;
  VENDOR_BOOT_DATA           *VendorImgDataPtr = NULL;
  EFI_BLOCK_IO_PROTOCOL      *VendorBlockIo    = NULL;
  EFI_DISK_IO_PROTOCOL       *VendorDiskIo     = NULL;
  EFI_HANDLE                 VendorBootHandle;
  CHAR16                     VendorBootPartitionName[MAX_PARTITION_NAME_LEN];
  ANDROID_BOOTIMG_PROTOCOL   *AndroidBootImgProtocol;
  CHAR8                      *AvbCmdline = NULL;
  CHAR16                     InitBootPartitionName[MAX_PARTITION_NAME_LEN];
  EFI_HANDLE                 InitBootHandle;
  ANDROID_INIT_BOOT_DATA     InitBootImgData;
  ANDROID_INIT_BOOT_DATA     *InitBootImgDataPtr = NULL;
  EFI_DISK_IO_PROTOCOL       *InitBootDiskIo     = NULL;
  EFI_BLOCK_IO_PROTOCOL      *InitBootBlockIo    = NULL;

  // Verify if the valid parameters
  if ((This == NULL) || (BufferSize == NULL) || (FilePath == NULL) || !IsDevicePathValid (FilePath, 0)) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: buffer %09p in size %08x\n", __FUNCTION__, Buffer, *BufferSize));

  if (!BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  // Retrieve Private data structure
  Private = ANDROID_BOOT_PRIVATE_DATA_FROM_LOADFILE (This);
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_ERROR, "%a: Attempting to boot kernel from %s\n", __FUNCTION__, Private->PartitionName));

  // Verify the image header and set the internal data structure ImgData
  Status = AndroidBootGetVerify (Private->BlockIo, Private->DiskIo, &ImgData, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Check if the given buffer size is big enough
  // EFI_BUFFER_TOO_SMALL gets boot manager allocate a bigger buffer
  if (ImgData.KernelSize == 0) {
    return EFI_NOT_FOUND;
  }

  if ((Buffer == NULL) || (*BufferSize < ImgData.KernelSize)) {
    *BufferSize = ImgData.KernelSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  // Vendor_boot requires boot_img header version to be at least 3
  if (ImgData.HeaderVersion >= 3) {
    if ((PcdGetBool (PcdBootAndroidImage))) {
      Status = AvbVerifyBoot (Private->RecoveryMode, Private->ControllerHandle, &AvbCmdline);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to AVB verify\n", __FUNCTION__, Status));
        return Status;
      }

      Status = CopyAndroidBootArgsToBootConfig (AvbCmdline);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add AVB cmdline to bootconfig protocol\n", __FUNCTION__, Status));
        return Status;
      }
    }

    Status = GetActivePartitionName (L"vendor_boot", VendorBootPartitionName);
    // Ignore vendor_boot ramdisk if vendor_boot partition not exist
    if (!EFI_ERROR (Status)) {
      // Get BlockIo/DiskIo for vendor_boot img
      VendorBootHandle = GetSiblingPartitionHandle (
                           Private->ControllerHandle,
                           VendorBootPartitionName
                           );

      if (VendorBootHandle != NULL) {
        Status = gBS->HandleProtocol (
                        VendorBootHandle,
                        &gEfiBlockIoProtocolGuid,
                        (VOID **)&VendorBlockIo
                        );
        if (EFI_ERROR (Status) || (VendorBlockIo == NULL)) {
          return Status;
        }

        Status = gBS->HandleProtocol (
                        VendorBootHandle,
                        &gEfiDiskIoProtocolGuid,
                        (VOID **)&VendorDiskIo
                        );
        if (EFI_ERROR (Status) || (VendorDiskIo == NULL)) {
          return Status;
        }

        // Examine if the Android Vendor Boot Image can be found
        Status = VendorBootGetVerify (VendorBlockIo, VendorDiskIo, &VendorImgData, NULL);
        if (EFI_ERROR (Status)) {
          return Status;
        }

        VendorImgDataPtr = &VendorImgData;
      }
    }

    Status = GetActivePartitionName (L"init_boot", InitBootPartitionName);
    if (!EFI_ERROR (Status)) {
      // Get BlockIo/DiskIo for init_boot img
      InitBootHandle = GetSiblingPartitionHandle (
                         Private->ControllerHandle,
                         InitBootPartitionName
                         );
      if (InitBootHandle != NULL) {
        Status = InitBootSetup (
                   InitBootHandle,
                   &InitBootBlockIo,
                   &InitBootDiskIo,
                   &InitBootImgData
                   );
        if (!EFI_ERROR (Status)) {
          InitBootImgDataPtr = &InitBootImgData;
        }
      }
    }
  }

  // Load kernel dtb
  AndroidBootDxeLoadDtb (Private);

  // Append Kernel Command Line Args
  Status = gBS->LocateProtocol (
                  &gAndroidBootImgProtocolGuid,
                  NULL,
                  (VOID **)&AndroidBootImgProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AndroidBootImgProtocol->AppendArgs (
                                     Private->KernelArgsProtocol.KernelArgs,
                                     (sizeof (CHAR16) * (ANDROID_BOOTIMG_KERNEL_ARGS_SIZE + PcdGet32 (PcdAndroidKernelCommandLineOverflow)))
                                     );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AndroidBootDxeUpdateDtbCmdLine (Private->KernelArgsProtocol.KernelArgs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Could not update kernel command line in DTB. Error Status: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Load Android Boot image
  Status = AndroidBootLoadFile (
             Private->BlockIo,
             Private->DiskIo,
             &ImgData,
             VendorBlockIo,
             VendorDiskIo,
             VendorImgDataPtr,
             InitBootBlockIo,
             InitBootDiskIo,
             InitBootImgDataPtr,
             Buffer
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_ERROR, "%a: KernelArgs: %s\n", __FUNCTION__, Private->KernelArgsProtocol.KernelArgs));

  return EFI_SUCCESS;
}

///
/// Load File Protocol instance
///
GLOBAL_REMOVE_IF_UNREFERENCED
EFI_LOAD_FILE_PROTOCOL  mAndroidBootDxeLoadFile = {
  AndroidBootDxeLoadFile
};

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Because ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
AndroidBootDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                   Status;
  UINT32                       *Id;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo       = NULL;
  EFI_DISK_IO_PROTOCOL         *DiskIo        = NULL;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo = NULL;
  EFI_HANDLE                   *ParentHandles = NULL;
  UINTN                        ParentCount;
  UINTN                        ParentIndex;
  EFI_HANDLE                   *ChildHandles = NULL;
  UINTN                        ChildCount;
  UINTN                        ChildIndex;
  VOID                         *Protocol;

  // This driver will be accessed while boot manager attempts to connect
  // all drivers to the controllers for each partition entry.
  // - BlockIo       to give a physical access to the flash device to obtain the image
  // - PartitionInfo to see the additional info like GPT type and the partition name, not must
  // - DevicePath    to get the device path and create a child node
  //                    MESSAGING_DEVICE_PATH and MSG_URI_DP required to be a valid boot option
  // Opening BY_DRIVER would not be successful so this opens GET_PROTOCOL
  // so CallerId will be used to avoid multiple attempts from attempting to manage the same controller.

  // Make sure BindingStart not done yet
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **)&Id,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status != EFI_UNSUPPORTED) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiPartitionInfoProtocolGuid,
                  (VOID **)&PartitionInfo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  if (PartitionInfo->Info.Gpt.StartingLBA > PartitionInfo->Info.Gpt.EndingLBA) {
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  // Check if there is an efi system partition on this disk
  // If so use that to boot the device
  Status = PARSE_HANDLE_DATABASE_PARENTS (ControllerHandle, &ParentCount, &ParentHandles);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find parents - %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  for (ParentIndex = 0; ParentIndex < ParentCount; ParentIndex++) {
    Status = ParseHandleDatabaseForChildControllers (ParentHandles[ParentIndex], &ChildCount, &ChildHandles);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find child controllers - %r\r\n", __FUNCTION__, Status));
      return Status;
    }

    for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++) {
      Status = gBS->HandleProtocol (ChildHandles[ChildIndex], &gEfiPartTypeSystemPartGuid, (VOID **)&Protocol);
      // Found ESP return unsupported
      if (!EFI_ERROR (Status)) {
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }
    }
  }

  // Examine if the Android Boot image can be found
  Status = AndroidBootGetVerify (BlockIo, DiskIo, NULL, NULL);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: AndroidBoot image found in %s\n", __FUNCTION__, PartitionInfo->Info.Gpt.PartitionName));
  }

ErrorExit:
  if (ParentHandles != NULL) {
    FreePool (ParentHandles);
    ParentHandles = NULL;
  }

  if (ChildHandles != NULL) {
    FreePool (ChildHandles);
    ChildHandles = NULL;
  }

  if (PartitionInfo != NULL) {
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiPartitionInfoProtocolGuid,
           This->DriverBindingHandle,
           ControllerHandle
           );
  }

  if (BlockIo != NULL) {
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiBlockIoProtocolGuid,
           This->DriverBindingHandle,
           ControllerHandle
           );
  }

  if (DiskIo != NULL) {
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiDiskIoProtocolGuid,
           This->DriverBindingHandle,
           ControllerHandle
           );
  }

  return Status;
}

/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed, or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.

**/
EFI_STATUS
EFIAPI
AndroidBootDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                   Status;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo = NULL;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo       = NULL;
  EFI_DISK_IO_PROTOCOL         *DiskIo        = NULL;
  EFI_DEVICE_PATH_PROTOCOL     *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL     *AndroidBootDevicePath;
  EFI_DEVICE_PATH_PROTOCOL     *Node;
  ANDROID_BOOT_PRIVATE_DATA    *Private;
  UINT32                       *Id;
  CHAR16                       *KernelArgs;

  // BindingSupported() filters out the unsupported attempts and the multiple attempts
  // from a successful ControllerHandle such that BindingStart() runs only once

  Private          = NULL;
  BlockIo          = NULL;
  ParentDevicePath = NULL;
  KernelArgs       = NULL;

  // Get Parent's device path to create a child node and append URI node
  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to get DevicePath: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Open PartitionInfo protocol to obtain the access to the flash partition
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiPartitionInfoProtocolGuid,
                  (VOID **)&PartitionInfo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to open PartitionInfo: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Open BlockIo protocol to obtain the access to the flash device
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to open BlockIo: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Open Disk Io protocol to obtain the access to the flash device
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a unable to open DiskIo protocol %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Allocate KernelArgs
  KernelArgs = AllocateZeroPool (sizeof (CHAR16) * (ANDROID_BOOTIMG_KERNEL_ARGS_SIZE + PcdGet32 (PcdAndroidKernelCommandLineOverflow)));
  if (KernelArgs == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  // Examine if the Android Boot Image can be found
  Status = AndroidBootGetVerify (BlockIo, DiskIo, NULL, KernelArgs);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Append URI device path node so this device can be used as boot option
  Node = CreateDeviceNode (MESSAGING_DEVICE_PATH, MSG_URI_DP, sizeof (EFI_DEVICE_PATH_PROTOCOL));
  if (Node == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  AndroidBootDevicePath = AppendDevicePathNode (ParentDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)Node);
  FreePool (Node);
  if (AndroidBootDevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  // Allocate Private Data and set up the initial data
  Private = AllocateZeroPool (sizeof (ANDROID_BOOT_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Private->Signature                           = ANDROID_BOOT_SIGNATURE;
  Private->BlockIo                             = BlockIo;
  Private->DiskIo                              = DiskIo;
  Private->ParentDevicePath                    = ParentDevicePath;
  Private->AndroidBootDevicePath               = AndroidBootDevicePath;
  Private->ControllerHandle                    = ControllerHandle;
  Private->ProtocolsInstalled                  = FALSE;
  Private->KernelArgsProtocol.KernelArgs       = KernelArgs;
  Private->KernelArgsProtocol.UpdateKernelArgs = UpdateKernelArgs;
  StrCpyS (Private->PartitionName, MAX_PARTITION_NAME_LEN, PartitionInfo->Info.Gpt.PartitionName);
  CopyMem (&Private->LoadFile, &mAndroidBootDxeLoadFile, sizeof (Private->LoadFile));

  // Install LoadFile and AndroidBootDevicePath protocols on child, AndroidBootHandle
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->AndroidBootHandle,
                  &gEfiLoadFileProtocolGuid,
                  &Private->LoadFile,
                  &gNVIDIALoadfileKernelArgsProtocol,
                  &Private->KernelArgsProtocol,
                  &gEfiDevicePathProtocolGuid,
                  Private->AndroidBootDevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to install the prot intf: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Private->ProtocolsInstalled = TRUE;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  AndroidBootOnConnectCompleteHandler,
                  Private,
                  &gNVIDIAConnectCompleteEventGuid,
                  &Private->ConnectCompleteEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to create end of dxe event callback: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  // Install and open CallerId to link the Private data structure
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ControllerHandle,
                  &gEfiCallerIdGuid,
                  &Private->Id,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to install CallerId: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **)&Id,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to open CallerId: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  // Setup a parent-child relationship between ControllerHandle and AndroidBootHandle
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **)&Id,
                  This->DriverBindingHandle,
                  Private->AndroidBootHandle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to set up parent-child: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      gBS->CloseProtocol (
             ControllerHandle,
             &gEfiCallerIdGuid,
             This->DriverBindingHandle,
             Private->AndroidBootHandle
             );
      gBS->UninstallMultipleProtocolInterfaces (
             ControllerHandle,
             &gEfiCallerIdGuid,
             &Private->Id,
             NULL
             );
      if (Private->ProtocolsInstalled) {
        gBS->UninstallMultipleProtocolInterfaces (
               Private->AndroidBootHandle,
               &gEfiLoadFileProtocolGuid,
               &Private->LoadFile,
               &gNVIDIALoadfileKernelArgsProtocol,
               &Private->KernelArgsProtocol,
               &gEfiDevicePathProtocolGuid,
               Private->AndroidBootDevicePath,
               NULL
               );
      }

      if (Private->ConnectCompleteEvent != NULL) {
        gBS->CloseEvent (Private->ConnectCompleteEvent);
      }

      FreePool (Private);
    }

    if (KernelArgs != NULL) {
      FreePool (KernelArgs);
    }

    if (AndroidBootDevicePath != NULL) {
      FreePool (AndroidBootDevicePath);
    }

    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiDiskIoProtocolGuid,
           This->DriverBindingHandle,
           &DiskIo
           );
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiBlockIoProtocolGuid,
           This->DriverBindingHandle,
           &BlockIo
           );
  } else {
    // BindingStart completed
    DEBUG ((DEBUG_INFO, "%a: done\n", __FUNCTION__));
  }

  return Status;
}

/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed, or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
EFI_STATUS
EFIAPI
AndroidBootDriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  EFI_STATUS                 Status;
  EFI_LOAD_FILE_PROTOCOL     *LoadFile;
  ANDROID_BOOT_PRIVATE_DATA  *Private;
  UINT32                     *Id;

  if (NumberOfChildren != 0) {
    return EFI_UNSUPPORTED;
  }

  // Attempt to open LoadFile protocol
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiLoadFileProtocolGuid,
                  (VOID **)&LoadFile,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiCallerIdGuid,
                    (VOID **)&Id,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Private = ANDROID_BOOT_PRIVATE_DATA_FROM_ID (Id);
  } else {
    Private = ANDROID_BOOT_PRIVATE_DATA_FROM_LOADFILE (LoadFile);
  }

  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiCallerIdGuid,
         This->DriverBindingHandle,
         &Private->Id
         );
  gBS->UninstallMultipleProtocolInterfaces (
         ControllerHandle,
         &gEfiCallerIdGuid,
         &Private->Id,
         NULL
         );
  gBS->UninstallMultipleProtocolInterfaces (
         Private->AndroidBootHandle,
         &gEfiLoadFileProtocolGuid,
         &Private->LoadFile,
         &gNVIDIALoadfileKernelArgsProtocol,
         &Private->KernelArgsProtocol,
         &gEfiDevicePathProtocolGuid,
         Private->AndroidBootDevicePath,
         NULL
         );
  if (Private->KernelArgsProtocol.KernelArgs != NULL) {
    FreePool (Private->KernelArgsProtocol.KernelArgs);
  }

  FreePool (Private->AndroidBootDevicePath);
  FreePool (Private);

  DEBUG ((DEBUG_INFO, "%a: done\n", __FUNCTION__));

  return EFI_SUCCESS;
}

/**
  Causes the driver to load a specified file.

  @param  This                  Protocol instance pointer.
  @param  FilePath              The device specific path of the file to load.
  @param  BootPolicy            If TRUE, indicates that the request originates from the
                                boot manager is attempting to load FilePath as a boot
                                selection. If FALSE, then FilePath must match as exact file
                                to be loaded.
  @param  BufferSize            On input the size of Buffer in bytes. On output with a return
                                code of EFI_SUCCESS, the amount of data transferred to
                                Buffer. On output with a return code of EFI_BUFFER_TOO_SMALL,
                                the size of Buffer required to retrieve the requested file.
  @param  Buffer                The memory buffer to transfer the file to. IF Buffer is NULL,
                                then the size of the requested file is returned in
                                BufferSize.

  @retval EFI_SUCCESS           The file was loaded.
  @retval EFI_UNSUPPORTED       The device does not support the provided BootPolicy
  @retval EFI_INVALID_PARAMETER FilePath is not a valid device path, or
                                BufferSize is NULL.
  @retval EFI_NO_MEDIA          No medium was present to load the file.
  @retval EFI_DEVICE_ERROR      The file was not loaded due to a device error.
  @retval EFI_NO_RESPONSE       The remote system did not respond.
  @retval EFI_NOT_FOUND         The file was not found.
  @retval EFI_ABORTED           The file load process was manually cancelled.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current directory entry.
                                BufferSize has been updated with the size needed to complete
                                the request.

**/
EFI_STATUS
EFIAPI
RcmLoadFile (
  IN EFI_LOAD_FILE_PROTOCOL    *This,
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  IN BOOLEAN                   BootPolicy,
  IN OUT UINTN                 *BufferSize,
  IN VOID                      *Buffer OPTIONAL
  )
{
  EFI_STATUS                   Status;
  ANDROID_BOOT_DATA            ImgData;
  UINTN                        NumOfHandles;
  EFI_HANDLE                   *HandleBuffer;
  UINTN                        Count;
  EFI_LOAD_FILE_PROTOCOL       *LoadFileProtocol;
  NVIDIA_KERNEL_ARGS_PROTOCOL  *NvidiaKernelArgsProtocol;
  ANDROID_BOOTIMG_PROTOCOL     *AndroidBootImgProtocol;

  // Verify if the valid parameters
  if ((This == NULL) || (BufferSize == NULL) || (FilePath == NULL) || !IsDevicePathValid (FilePath, 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  // Verify the image header and set the internal data structure ImgData
  Status = AndroidBootGetVerify (NULL, NULL, &ImgData, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Check if the given buffer size is big enough
  // EFI_BUFFER_TOO_SMALL gets boot manager allocate a bigger buffer
  if (ImgData.KernelSize == 0) {
    return EFI_NOT_FOUND;
  }

  if ((Buffer == NULL) || (*BufferSize < ImgData.KernelSize)) {
    *BufferSize = ImgData.KernelSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  HandleBuffer = NULL;
  NumOfHandles = 0;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiLoadFileProtocolGuid,
                        NULL,
                        &NumOfHandles,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status) || (NumOfHandles == 0) || (HandleBuffer == NULL)) {
    return EFI_UNSUPPORTED;
  }

  NvidiaKernelArgsProtocol = NULL;
  for (Count = 0; Count < NumOfHandles; Count++) {
    LoadFileProtocol = NULL;
    Status           = gBS->HandleProtocol (
                              HandleBuffer[Count],
                              &gEfiLoadFileProtocolGuid,
                              (VOID **)&LoadFileProtocol
                              );
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    if (This == LoadFileProtocol) {
      Status = gBS->HandleProtocol (
                      HandleBuffer[Count],
                      &gNVIDIALoadfileKernelArgsProtocol,
                      (VOID **)&NvidiaKernelArgsProtocol
                      );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      break;
    }
  }

  if (NvidiaKernelArgsProtocol == NULL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Append Kernel Command Line Args
  AndroidBootImgProtocol = NULL;
  Status                 = gBS->LocateProtocol (
                                  &gAndroidBootImgProtocolGuid,
                                  NULL,
                                  (VOID **)&AndroidBootImgProtocol
                                  );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AndroidBootImgProtocol->AppendArgs (
                                     NvidiaKernelArgsProtocol->KernelArgs,
                                     (sizeof (CHAR16) * (ANDROID_BOOTIMG_KERNEL_ARGS_SIZE + PcdGet32 (PcdAndroidKernelCommandLineOverflow)))
                                     );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AndroidBootDxeUpdateDtbCmdLine (NvidiaKernelArgsProtocol->KernelArgs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Could not update kernel command line in DTB. Error Status: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  // Load Android Boot image
  Status = AndroidBootLoadFile (NULL, NULL, &ImgData, NULL, NULL, NULL, NULL, NULL, NULL, Buffer);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  DEBUG ((DEBUG_ERROR, "%a: KernelArgs: %s\n", __FUNCTION__, NvidiaKernelArgsProtocol->KernelArgs));

Exit:
  if (HandleBuffer != NULL) {
    gBS->FreePool (HandleBuffer);
  }

  return Status;
}

///
/// Rcm LoadFile Protocol instance
///
GLOBAL_REMOVE_IF_UNREFERENCED
EFI_LOAD_FILE_PROTOCOL  mRcmLoadFile = {
  RcmLoadFile
};

GLOBAL_REMOVE_IF_UNREFERENCED
NVIDIA_KERNEL_ARGS_PROTOCOL  mRcmKernelArgsProtocol = {
  NULL,
  UpdateKernelArgs
};

///
/// Driver Binding Protocol instance
///
EFI_DRIVER_BINDING_PROTOCOL  mAndroidBootDriverBinding = {
  AndroidBootDriverBindingSupported,
  AndroidBootDriverBindingStart,
  AndroidBootDriverBindingStop,
  0x0,
  NULL,
  NULL
};

/**
  Setup to boot from an image in memory.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
STATIC
EFI_STATUS
AndroidBootPrepareBootFromMemory (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;
  CHAR16      *KernelArgs = NULL;

  // Allocate KernelArgs
  KernelArgs = AllocateZeroPool (sizeof (CHAR16) * (ANDROID_BOOTIMG_KERNEL_ARGS_SIZE + PcdGet32 (PcdAndroidKernelCommandLineOverflow)));
  if (KernelArgs == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Verify the image header
  Status = AndroidBootGetVerify (NULL, NULL, NULL, KernelArgs);
  if (EFI_ERROR (Status)) {
    FreePool (KernelArgs);
    return Status;
  }

  mRcmKernelArgsProtocol.KernelArgs = KernelArgs;

  // Copy NVIDIA RCM Kernel GUID to device path
  CopyMem (&mRcmLoadFileDevicePath.VenHwNode.Guid, &gNVIDIARcmKernelGuid, sizeof (EFI_GUID));

  // Install Rcm Loadfile protocol
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiLoadFileProtocolGuid,
                  &mRcmLoadFile,
                  &gNVIDIALoadfileKernelArgsProtocol,
                  &mRcmKernelArgsProtocol,
                  &gEfiDevicePathProtocolGuid,
                  &mRcmLoadFileDevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install Load File Protocol (%r)\r\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
AndroidBootDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT64      KernelStart;
  UINT64      KernelDtbStart;
  UINTN       NewKernelDtbPages;
  VOID        *NewKernelDtb = NULL;
  BOOLEAN     IsMemoryBoot  = FALSE;

  // Look for a kernel address in the DTB.  If found, then a boot.img has
  // already been loaded for us.  We'll boot it.
  Status = GetKernelAddress (&KernelStart, &KernelDtbStart);

  if (Status == EFI_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Booting image at %lx with DTB %lx\r\n",
      __FUNCTION__,
      KernelStart,
      KernelDtbStart
      ));

    Status = AndroidBootPrepareBootFromMemory (ImageHandle);
    if (EFI_ERROR (Status)) {
      goto Done;
    }

    // Copy the kernel DTB.  We need to provide a modified version.
    NewKernelDtbPages = EFI_SIZE_TO_PAGES (2 * fdt_totalsize ((VOID *)KernelDtbStart));
    NewKernelDtb      = AllocatePages (NewKernelDtbPages);
    if (NewKernelDtb == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: failed to allocate pages for expanded kernel DTB\r\n", __FUNCTION__));
      goto Done;
    }

    if (fdt_open_into ((UINT8 *)KernelDtbStart, NewKernelDtb, EFI_PAGES_TO_SIZE (NewKernelDtbPages)) != 0) {
      DEBUG ((DEBUG_ERROR, "%a: failed to relocate kernel DTB\r\n", __FUNCTION__));
      goto Done;
    }

    DEBUG ((DEBUG_INFO, "%a: Using DTB %p\r\n", __FUNCTION__, NewKernelDtb));

    // Install the modified DTB
    gBS->InstallConfigurationTable (&gFdtTableGuid, NewKernelDtb);

    IsMemoryBoot = TRUE;
  } else if ((PcdGet64 (PcdRcmKernelBase) != 0) &&
             (PcdGet64 (PcdRcmKernelSize) != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Booting image at %lx with RCM\r\n",
      __FUNCTION__,
      PcdGet64 (PcdRcmKernelBase)
      ));

    AndroidBootPrepareBootFromMemory (ImageHandle);
    IsMemoryBoot = TRUE;
  }

Done:
  if (IsMemoryBoot) {
    return EFI_SUCCESS;
  }

  // Install UEFI Driver Model protocol(s).
  Status = EfiLibInstallDriverBinding (
             ImageHandle,
             SystemTable,
             &mAndroidBootDriverBinding,
             ImageHandle
             );
  return Status;
}
