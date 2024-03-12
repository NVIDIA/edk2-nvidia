/** @file
  EDK2 API for LibAvb

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/BaseLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/NVIDIADebugLib.h>

#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

#include "libavb/libavb/libavb.h"
#include "Library/AvbLib.h"

/**
  Read tamper-evident storage, parse device unlocked state.

  @param[in]  Ops         A pointer to the AvbOps struct.
  @param[out] IsUnlocked  True if device is unlocked.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
ReadIsDeviceUnlocked (
  IN  AvbOps  *Ops,
  OUT bool    *IsUnlocked
  )
{
  *IsUnlocked = false;
  return AVB_IO_RESULT_OK;
}

/**
  Read parition data from given offset.

  @param[in]  Ops         A pointer to the AvbOps struct.
  @param[in]  Partition   Partition name string.
  @param[in]  Offset      Read from this offset (negative means read from bottom).
  @param[in]  NumBytes    Num of bytes to read.
  @param[out] Buffer      Buffer address to read into.
  @param[out] NumRead     Actual bytes read from storage.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
ReadFromPartition (
  IN  AvbOps      *Ops,
  IN  const char  *Partition,
  IN  int64_t     Offset,
  IN  size_t      NumBytes,
  OUT VOID        *Buffer,
  OUT size_t      *NumRead
  )
{
  return AVB_IO_RESULT_OK;
}

/**
  Validate if vbmeta key0 is trusted key.

  @param[in]  Ops               A pointer to the AvbOps struct.
  @param[in]  PubKey            Key0 public key buffer.
  @param[in]  PubKeyLen         Key0 public key length.
  @param[in]  PubKeyMetadata    Public key metadata buffer.
  @param[in]  PubKeyMetadataLen Public key metadata length.
  @param[out] OutIsTrusted      Output buffer to store trusted state.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
ValidateVbmetaPublicKey (
  IN  AvbOps         *Ops,
  IN  const uint8_t  *PubKey,
  IN  size_t         PubKeyLen,
  IN  const uint8_t  *PubKeyMetadata,
  IN  size_t         PubKeyMetadataLen,
  OUT bool           *OutIsTrusted
  )
{
  *OutIsTrusted = true;
  return AVB_IO_RESULT_OK;
}

/**
  Verify avb_slot_verify and get boot state based on result.

  @param[in]  IsRecovery  If boot is recovery boot.
  @param[out] BootState   Output buffer to store boot state.
  @param[out] SlotData    Output buffer to store AvbSlotVerifyData.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
EFI_STATUS
VerifiedBootGetBootState (
  IN  BOOLEAN            IsRecovery,
  OUT AVB_BOOT_STATE     *BootState,
  OUT AvbSlotVerifyData  **SlotData
  )
{
  AvbSlotVerifyResult  AvbRes = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
  // Use libavb API to verify boot chain
  AvbOps  Ops = {
    .read_from_partition        = ReadFromPartition,
    .read_is_device_unlocked    = ReadIsDeviceUnlocked,
    .validate_vbmeta_public_key = ValidateVbmetaPublicKey,
  };

  AvbRes = avb_slot_verify (
             &Ops,
             NULL,
             "",
             0,
             AVB_HASHTREE_ERROR_MODE_MANAGED_RESTART_AND_EIO,
             SlotData
             );

  return AvbRes == AVB_SLOT_VERIFY_RESULT_OK ? EFI_SUCCESS : EFI_SECURITY_VIOLATION;
}

EFI_STATUS
AvbVerifyBoot (
  IN BOOLEAN     IsRecovery,
  IN EFI_HANDLE  ControllerHandle,
  OUT CHAR8      **AvbCmdline
  )
{
  AVB_BOOT_STATE     BootState = VERIFIED_BOOT_UNKNOWN_STATE;
  EFI_STATUS         Status;
  AvbSlotVerifyData  *SlotData = NULL;

  Status = VerifiedBootGetBootState (IsRecovery, &BootState, &SlotData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:Avb Verify Boot failed with %r\n", __func__, Status));
  }

  return Status;
}
