/** @file

  Android Boot Loader Driver's private data structure and interfaces declaration

  SPDX-FileCopyrightText: Copyright (c) 2019-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2017, Linaro.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_ANDROID_BOOT_DXE_H__
#define __EFI_ANDROID_BOOT_DXE_H__

#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <AndroidBootImgHeader.h>
#include <Library/BootChainInfoLib.h>

#include <Guid/LinuxEfiInitrdMedia.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/LoadFile.h>
#include <Protocol/LoadFile2.h>
#include <Protocol/KernelArgsProtocol.h>
#include <Protocol/AndroidBootImg.h>

#define FDT_ADDITIONAL_ENTRIES_SIZE               0x400
#define ANDROID_BOOT_SIGNATURE                    SIGNATURE_64 ('A','N','D','R','O','I','D','!')
#define VENDOR_BOOT_MAGIC                         "VNDRBOOT"
#define VENDOR_BOOT_MAGIC_SIZE                    8
#define VENDOR_BOOT_ARGS_SIZE                     2048
#define VENDOR_BOOT_NAME_SIZE                     16
#define VENDOR_RAMDISK_TYPE_NONE                  0
#define VENDOR_RAMDISK_TYPE_PLATFORM              1
#define VENDOR_RAMDISK_TYPE_RECOVERY              2
#define VENDOR_RAMDISK_TYPE_DLKM                  3
#define VENDOR_RAMDISK_NAME_SIZE                  32
#define VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE  16

#define BOOTCONFIG_RESERVED_SIZE  1024

typedef struct {
  UINT8     Magic[VENDOR_BOOT_MAGIC_SIZE];
  UINT32    HeaderVersion;
  UINT32    PageSize;           /* flash page size we assume */

  UINT32    KernelAddr;         /* physical load addr */
  UINT32    RamdiskAddr;        /* physical load addr */

  UINT32    VendorRamdiskSize; /* size in bytes */

  CHAR8     KernelArgs[VENDOR_BOOT_ARGS_SIZE];

  UINT32    TagsAddr;           /* physical addr for kernel tags */

  CHAR8     Name[VENDOR_BOOT_NAME_SIZE]; /* asciiz product name */
  UINT32    HeaderSize;                  /* size of vendor boot image header in
                                   * bytes */
  UINT32    DtbSize;                     /* size of dtb image */
  UINT64    DtbAddr;                     /* physical load address */

  UINT32    VendorRamdiskTableSize;      /* size in bytes for the vendor ramdisk table */
  UINT32    VendorRamdiskTableEntryNum;  /* number of entries in the vendor ramdisk table */
  UINT32    VendorRamdiskTableEntrySize; /* size in bytes for a vendor ramdisk table entry */
  UINT32    BootConfigSize;              /* size in bytes for the bootconfig section */
} VENDOR_BOOTIMG_TYPE4_HEADER;

typedef struct {
  UINT32    RamdiskSize;   /* size in bytes for the ramdisk image */
  UINT32    RamdiskOffset; /* offset to the ramdisk image in vendor ramdisk section */
  UINT32    RamdiskType;   /* type of the ramdisk */

  UINT8     RamdiskName[VENDOR_RAMDISK_NAME_SIZE]; /* asciiz ramdisk name */

  // Hardware identifiers describing the board, soc or platform which this
  // ramdisk is intended to be loaded on.
  UINT32    BoardId[VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE];
} VENDOR_RAMDISK_TABLE_TYPE4_ENTRY;

// Android Vendor Boot Data structure
typedef struct {
  UINT32    Offset;
  UINT32    VendorRamdiskSize;
  UINT32    PageSize;
  UINT32    HeaderVersion;
  UINT32    DtbSize;
  UINT32    VendorRamdiskTableSize;
  UINT32    BootConfigSize;
} VENDOR_BOOT_DATA;

// Android Boot Data structure
typedef struct {
  UINT32    Offset;
  UINT32    KernelSize;
  UINT32    RamdiskSize;
  UINT32    PageSize;
  UINT32    HeaderVersion;
} ANDROID_BOOT_DATA;

// Android Init Boot Data structure
typedef struct {
  UINT32    RamdiskSize;
  UINT32    PageSize;
  UINT32    HeaderVersion;
} ANDROID_INIT_BOOT_DATA;

// Private data structure
typedef struct {
  UINT64                         Signature;

  CHAR16                         PartitionName[MAX_PARTITION_NAME_LEN];

  EFI_LOAD_FILE_PROTOCOL         LoadFile;
  EFI_PARTITION_INFO_PROTOCOL    *PartitionInfo;
  EFI_BLOCK_IO_PROTOCOL          *BlockIo;
  EFI_DISK_IO_PROTOCOL           *DiskIo;
  EFI_DEVICE_PATH_PROTOCOL       *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL       *AndroidBootDevicePath;
  EFI_EVENT                      ConnectCompleteEvent;
  NVIDIA_KERNEL_ARGS_PROTOCOL    KernelArgsProtocol;

  UINT32                         Id;
  BOOLEAN                        ProtocolsInstalled;

  EFI_HANDLE                     ControllerHandle;
  EFI_HANDLE                     DriverBindingHandle;
  EFI_HANDLE                     AndroidBootHandle;

  BOOLEAN                        RecoveryMode;
} ANDROID_BOOT_PRIVATE_DATA;

//
// Device path for the handle that incorporates our ramload and initrd load file
// instance.
//
#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH          VenHwNode;
  EFI_DEVICE_PATH_PROTOCOL    EndNode;
} SINGLE_VENHW_NODE_DEVPATH;

typedef struct {
  VENDOR_DEVICE_PATH          VenMediaNode;
  EFI_DEVICE_PATH_PROTOCOL    EndNode;
} INITRD_DEVICE_PATH;
#pragma pack()

#define ANDROID_BOOT_PRIVATE_DATA_FROM_ID(a)        CR (a, ANDROID_BOOT_PRIVATE_DATA, Id, ANDROID_BOOT_SIGNATURE)
#define ANDROID_BOOT_PRIVATE_DATA_FROM_LOADFILE(a)  CR (a, ANDROID_BOOT_PRIVATE_DATA, LoadFile, ANDROID_BOOT_SIGNATURE)

EFI_STATUS
AndroidBootGetVerify (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  OUT ANDROID_BOOT_DATA      *ImgData OPTIONAL,
  OUT CHAR16                 *KernelArgs OPTIONAL
  );

#endif // __EFI_ANDROID_BOOT_DXE_H__
