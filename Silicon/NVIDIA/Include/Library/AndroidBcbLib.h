/** @file

  SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BOOTLOADER_MESSAGE_H_
#define __BOOTLOADER_MESSAGE_H_

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>
#include <Uefi/UefiMultiPhase.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

#define MISC_PARTITION_BASE_NAME           L"MSC"
#define BOOTLOADER_MESSAGE_OFFSET_IN_MISC  0

#define BOOTLOADER_MESSAGE_COMMAND_BYTES   32
#define BOOTLOADER_MESSAGE_STATUS_BYTES    32
#define BOOTLOADER_MESSAGE_RECOVERY_BYTES  768
#define BOOTLOADER_MESSAGE_STAGE_BYTES     32
#define BOOTLOADER_MESSAGE_RESERVED_BYTES  1184

/**
 Standard definition for android bootloader message from
 https://android.googlesource.com/platform/bootable/recovery/+/master/bootloader_message/include/bootloader_message/bootloader_message.h
 Refer to the documentation in the header file for details.
 **/
typedef struct {
  CHAR8    command[BOOTLOADER_MESSAGE_COMMAND_BYTES];
  CHAR8    status[BOOTLOADER_MESSAGE_STATUS_BYTES];
  CHAR8    recovery[BOOTLOADER_MESSAGE_RECOVERY_BYTES];
  CHAR8    stage[BOOTLOADER_MESSAGE_STAGE_BYTES];
  CHAR8    reserved[BOOTLOADER_MESSAGE_RESERVED_BYTES];
} BootloaderMessage;

typedef enum {
  MISC_CMD_TYPE_RECOVERY           = 1,
  MISC_CMD_TYPE_FASTBOOT_USERSPACE = 2,
  MISC_CMD_TYPE_INVALID            = 3,
  MISC_CMD_TYPE_MAX,
} MiscCmdType;

/**
  Get BCB command type from BCB blob lodated in MISC partition

  @param[out]              Type Pointer to BCB cmd type

  @retval EFI_SUCCESS      Operation successful.
  @retval others           Error occurred.
**/

EFI_STATUS
EFIAPI
GetCmdFromMiscPartition (
  IN  EFI_HANDLE   Handle,
  OUT MiscCmdType  *Type
  );

#endif /* __BOOTLOADER_MESSAGE_H_ */
