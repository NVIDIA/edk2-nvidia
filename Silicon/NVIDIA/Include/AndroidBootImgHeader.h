/** @file

  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2017, Linaro.
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ABOOTIMG_H__
#define __ABOOTIMG_H__

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define ANDROID_BOOTIMG_KERNEL_ARGS_SIZE        512
#define ANDROID_BOOTIMG_KERNEL_EXTRA_ARGS_SIZE  1024
#define ANDROID_BOOTIMG_NAME_SIZE               16

#define ANDROID_BOOT_MAGIC         "ANDROID!"
#define ANDROID_BOOT_MAGIC_LENGTH  (sizeof (ANDROID_BOOT_MAGIC) - 1)

#pragma pack(1)

/**
  Minimal Android boot.img header with magic and version.

  This should only be used as a bootstrap.  BootMagic can be used to verify
  it's a boot.img and HeaderVersion to determine which type.  Once the version
  is determined, the appropriate type header should be used instead.

  https://source.android.com/docs/core/architecture/bootloader/boot-image-header
*/
typedef struct {
  UINT8     BootMagic[ANDROID_BOOT_MAGIC_LENGTH];
  UINT32    Reserved[8];
  UINT32    HeaderVersion;
} ANDROID_BOOTIMG_VERSION_HEADER;

/**
  Type0 Android boot.img header.

  https://source.android.com/docs/core/architecture/bootloader/boot-image-header
*/
typedef struct {
  UINT8     BootMagic[ANDROID_BOOT_MAGIC_LENGTH];
  UINT32    KernelSize;
  UINT32    KernelAddress;
  UINT32    RamdiskSize;
  UINT32    RamdiskAddress;
  UINT32    SecondStageBootloaderSize;
  UINT32    SecondStageBootloaderAddress;
  UINT32    KernelTagsAddress;
  UINT32    PageSize;
  UINT32    Reserved;
  UINT32    OsVersion;
  CHAR8     ProductName[ANDROID_BOOTIMG_NAME_SIZE];
  CHAR8     KernelArgs[ANDROID_BOOTIMG_KERNEL_ARGS_SIZE];
  UINT32    Id[8];
  CHAR8     KernelExtraArgs[ANDROID_BOOTIMG_KERNEL_EXTRA_ARGS_SIZE];
} ANDROID_BOOTIMG_TYPE0_HEADER;

/**
  Type1 Android boot.img header.

  https://source.android.com/docs/core/architecture/bootloader/boot-image-header
*/
typedef struct {
  UINT8     BootMagic[ANDROID_BOOT_MAGIC_LENGTH];
  UINT32    KernelSize;
  UINT32    KernelAddress;
  UINT32    RamdiskSize;
  UINT32    RamdiskAddress;
  UINT32    SecondStageBootloaderSize;
  UINT32    SecondStageBootloaderAddress;
  UINT32    KernelTagsAddress;
  UINT32    PageSize;
  UINT32    HeaderVersion;
  UINT32    OsVersion;
  CHAR8     ProductName[ANDROID_BOOTIMG_NAME_SIZE];
  CHAR8     KernelArgs[ANDROID_BOOTIMG_KERNEL_ARGS_SIZE];
  UINT32    Id[8];
  CHAR8     KernelExtraArgs[ANDROID_BOOTIMG_KERNEL_EXTRA_ARGS_SIZE];
  UINT32    RecoveryOverlaySize;
  UINT64    RecoveryOverlayOffset;
  UINT32    HeaderSize;
} ANDROID_BOOTIMG_TYPE1_HEADER;

/**
  Type2 Android boot.img header.

  https://source.android.com/docs/core/architecture/bootloader/boot-image-header
*/
typedef struct {
  UINT8     BootMagic[ANDROID_BOOT_MAGIC_LENGTH];
  UINT32    KernelSize;
  UINT32    KernelAddress;
  UINT32    RamdiskSize;
  UINT32    RamdiskAddress;
  UINT32    SecondStageBootloaderSize;
  UINT32    SecondStageBootloaderAddress;
  UINT32    KernelTagsAddress;
  UINT32    PageSize;
  UINT32    HeaderVersion;
  UINT32    OsVersion;
  CHAR8     ProductName[ANDROID_BOOTIMG_NAME_SIZE];
  CHAR8     KernelArgs[ANDROID_BOOTIMG_KERNEL_ARGS_SIZE];
  UINT32    Id[8];
  CHAR8     KernelExtraArgs[ANDROID_BOOTIMG_KERNEL_EXTRA_ARGS_SIZE];
  UINT32    RecoveryOverlaySize;
  UINT64    RecoveryOverlayOffset;
  UINT32    HeaderSize;
  UINT32    DtbSize;
  UINT32    DtbAddr;
} ANDROID_BOOTIMG_TYPE2_HEADER;

/**
  Type3 Android boot.img header.

  https://source.android.com/docs/core/architecture/bootloader/boot-image-header
*/
typedef struct {
  UINT8     BootMagic[ANDROID_BOOT_MAGIC_LENGTH];
  UINT32    KernelSize;
  UINT32    RamdiskSize;
  UINT32    OsVersion;
  UINT32    HeaderSize;
  UINT32    Reserved[4];
  UINT32    HeaderVersion;
  CHAR8     KernelArgs[ANDROID_BOOTIMG_KERNEL_ARGS_SIZE + ANDROID_BOOTIMG_KERNEL_EXTRA_ARGS_SIZE];
} ANDROID_BOOTIMG_TYPE3_HEADER;

#pragma pack ()

/* Check Val (unsigned) is a power of 2 (has only one bit set) */
#define IS_POWER_OF_2(Val)  ((Val) != 0 && (((Val) & ((Val) - 1)) == 0))

/* Android boot image page size is not specified, but it should be power of 2
 * and larger than boot header */
#define IS_VALID_ANDROID_PAGE_SIZE(Val)   \
             (IS_POWER_OF_2(Val) && (Val > sizeof(ANDROID_BOOTIMG_VERSION_HEADER)))

EFI_STATUS
AndroidBootImgGetImgSize (
  IN  VOID   *BootImg,
  OUT UINTN  *ImgSize
  );

EFI_STATUS
AndroidBootImgBoot (
  IN VOID   *Buffer,
  IN UINTN  BufferSize
  );

#endif /* __ABOOTIMG_H__ */
