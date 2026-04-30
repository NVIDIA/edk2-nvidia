/** @file

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ANDROID_FASTBOOT_APP_H__
#define __ANDROID_FASTBOOT_APP_H__

#include <Library/AndroidBootImgLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#define BOOTIMG_KERNEL_ARGS_SIZE  512

#define ANDROID_FASTBOOT_VERSION  "0.4"

EFI_STATUS
BootAndroidBootImg (
  IN  UINTN  BufferSize,
  IN  VOID   *Buffer
  );

EFI_STATUS
ParseAndroidBootImg (
  IN  VOID   *BootImg,
  OUT VOID   **Kernel,
  OUT UINTN  *KernelSize,
  OUT VOID   **Ramdisk,
  OUT UINTN  *RamdiskSize,
  OUT CHAR8  *KernelArgs
  );

/**
  Wipe user data partitions (currently userdata, CAC, MDA). Missing
  partitions are treated as benign (board variants); fail-fast on
  the first hard error.

  Honours fac_rst_protection: when OEM unlock is blocked the wipe
  is refused with EFI_ACCESS_DENIED. Use FastbootLockBootloader to
  wipe as part of re-locking, which intentionally bypasses FRP.

  @retval EFI_SUCCESS         All partitions erased (or absent).
  @retval EFI_ACCESS_DENIED   Blocked by fac_rst_protection.
  @retval Other EFI_ERROR     Erase failed on some partition; see DEBUG.
**/
EFI_STATUS
FastbootFactoryReset (
  VOID
  );

/**
  Drive the bootloader to the "locked" state: wipe user data and
  flip the AVB locked flag. Unconditional -- caller is expected to
  AvbReadDeviceLockedState() first and skip if already locked.

  @retval EFI_SUCCESS       Device is now locked.
  @retval Other EFI_ERROR   Erase / write failed; see DEBUG.
**/
EFI_STATUS
FastbootLockBootloader (
  VOID
  );

/**
  Drive the bootloader to the "unlocked" state: validate
  fac_rst_protection allows it, wipe user data, then flip the AVB
  locked flag. Caller is expected to AvbReadDeviceLockedState()
  first and skip if already unlocked.

  @retval EFI_SUCCESS         Device is now unlocked.
  @retval EFI_ACCESS_DENIED   OEM unlock is not allowed (fac_rst_protection).
  @retval Other EFI_ERROR     Erase / write failed; see DEBUG.
**/
EFI_STATUS
FastbootUnlockBootloader (
  VOID
  );

#endif //ifdef __ANDROID_FASTBOOT_APP_H__
