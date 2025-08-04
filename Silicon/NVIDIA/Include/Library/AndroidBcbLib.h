/** @file

  SPDX-FileCopyrightText: Copyright (c) 2023-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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

#pragma pack(1)

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

typedef struct slot_metadata {
  // Slot priority with 15 meaning highest priority, 1 lowest
  // priority and 0 the slot is unbootable.
  UINT8    Priority        : 4;
  // Number of times left attempting to boot this slot.
  UINT8    TriesRemaining  : 2;
  // 1 if this slot has booted successfully, 0 otherwise.
  UINT8    SuccessfulBoot  : 1;
  // 1 if this slot is corrupted from a dm-verity corruption, 0
  // otherwise.
  UINT8    VerityCorrupted : 1;
  // Reserved for further use.
  UINT8    Reserved        : 8;
} SlotMetadata;

/* Bootloader Control AB
 *
 * This struct can be used to manage A/B metadata. It is designed to
 * be put in the 'slot_suffix' field of the 'bootloader_message'
 * structure described above. It is encouraged to use the
 * 'bootloader_control' structure to store the A/B metadata, but not
 * mandatory.
 */
typedef struct bootloader_control {
  // NUL terminated active slot suffix.
  CHAR8           SlotSuffix[4];
  // Bootloader Control AB magic number (see BOOT_CTRL_MAGIC).
  UINT32          Magic;
  // Version of struct being used (see BOOT_CTRL_VERSION).
  UINT8           Version;
  // Number of slots being managed.
  UINT8           NbSlot                 : 3;
  // Number of times left attempting to boot recovery.
  UINT8           RecoveryTriesRemaining : 3;
  // Status of any pending snapshot merge of dynamic partitions.
  UINT8           MergeStatus            : 3;
  // Ensure 4-bytes alignment for slot_info field.
  UINT8           Reserved0[1];
  // Per-slot information.  Up to 4 slots.
  SlotMetadata    SlotInfo[4];
  // Reserved for further use.
  UINT8           Reserved1[8];
  // CRC32 of all 28 bytes preceding this field (little endian
  // format).
  UINT32          Crc32Le;
} BootloaderControl;

#pragma pack()

/**
 * The A/B-specific bootloader message structure (4-KiB).
 *
 * We separate A/B boot control metadata from the regular bootloader
 * message struct and keep it here. Everything that's A/B-specific
 * stays after struct bootloader_message, which belongs to the vendor
 * space of /misc partition. Also, the A/B-specific contents should be
 * managed by the A/B-bootloader or boot control HAL.
 *
 * The slot_suffix field is used for A/B implementations where the
 * bootloader does not set the androidboot.ro.boot.slot_suffix kernel
 * commandline parameter. This is used by fs_mgr to mount /system and
 * other partitions with the slotselect flag set in fstab. A/B
 * implementations are free to use all 32 bytes and may store private
 * data past the first NUL-byte in this field. It is encouraged, but
 * not mandatory, to use 'struct bootloader_control' described below.
 *
 * The update_channel field is used to store the Omaha update channel
 * if update_engine is compiled with Omaha support.
 */
typedef struct bootloader_message_ab {
  BootloaderMessage    Message;
  BootloaderControl    BootCtrl;

  // Round up the entire struct to 4096-byte.
  CHAR8                Reserved[2016];
} BootloaderMessageAb;

typedef enum {
  MISC_CMD_TYPE_RECOVERY            = 1,
  MISC_CMD_TYPE_FASTBOOT_USERSPACE  = 2,
  MISC_CMD_TYPE_FASTBOOT_BOOTLOADER = 3,
  MISC_CMD_TYPE_INVALID             = 4,
  MISC_CMD_TYPE_MAX,
} MiscCmdType;

/**
  Get BCB command type from BCB blob lodated in MISC partition

  @param[in]               Image Handle to access block device
  @param[out]              Type Pointer to BCB cmd type
  @param[in]               Clean bootonce cmd if True

  @retval EFI_SUCCESS      Operation successful.
  @retval others           Error occurred.
**/

EFI_STATUS
EFIAPI
GetCmdFromMiscPartition (
  IN  EFI_HANDLE   Handle,
  OUT MiscCmdType  *Type,
  IN  BOOLEAN      CleanBootOnceCmd
  );

/**
  Force Bcb active boot chain metadata to current boot chain if not in sync.

  @param[in]               Image Handle to access block device

  @retval EFI_SUCCESS      Operation successful.
  @retval others           Error occurred.
**/
EFI_STATUS
EFIAPI
AndroidBcbLockChain (
  EFI_HANDLE  Handle
  );

/**
  Update retry count if Bcb active boot chain is not boot_successful.

  @param[in]               Image Handle to access block device

  @retval EFI_SUCCESS      Operation successful.
  @retval others           Error occurred.
**/
EFI_STATUS
EFIAPI
AndroidBcbCheckAndUpdateRetryCount (
  EFI_HANDLE  Handle
  );

#endif /* __BOOTLOADER_MESSAGE_H_ */
