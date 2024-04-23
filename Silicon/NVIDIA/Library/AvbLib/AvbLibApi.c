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
#include <Library/HandleParsingLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/SiblingPartitionLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

#include "libavb/libavb/libavb.h"
#include "Library/AvbLib.h"

STATIC EFI_HANDLE  mControllerHandle;

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
  // Unlocked state will stay in tamper-resist storage
  // Always return "Locked" as WAR
  *IsUnlocked = false;
  return AVB_IO_RESULT_OK;
}

/**
  Get size of a given partition.

  @param[in]  Ops             A pointer to the AvbOps struct.
  @param[in]  Partition       Partition name string.
  @param[out] OutSizeNumBytes Output buffer to store partition size

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
GetSizeOfPartition (
  IN  AvbOps      *Ops,
  IN  const char  *Partition,
  OUT uint64_t    *OutSizeNumBytes
  )
{
  EFI_STATUS             Status = EFI_SUCCESS;
  EFI_HANDLE             PartitionHandle;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;
  UINTN                  PartitionSize;
  CHAR16                 PartitionName[MAX_PARTITION_NAME_LEN];
  CHAR16                 ActivePartitionName[MAX_PARTITION_NAME_LEN];
  AvbIOResult            AvbResult = AVB_IO_RESULT_OK;

  UnicodeSPrintAsciiFormat (PartitionName, sizeof (PartitionName), "%a", Partition);

  Status = GetActivePartitionName (PartitionName, ActivePartitionName);
  if (EFI_ERROR (Status)) {
    AvbResult = AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;
    goto Exit;
  }

  PartitionHandle = GetSiblingPartitionHandle (
                      mControllerHandle,
                      ActivePartitionName
                      );

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status) || (BlockIo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate block io protocol on partition\r\n", __FUNCTION__));
    AvbResult = AVB_IO_RESULT_ERROR_IO;
    goto Exit;
  }

  PartitionSize    = (UINTN)(BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  *OutSizeNumBytes = (uint64_t)PartitionSize;

Exit:
  return AvbResult;
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
  EFI_STATUS             Status = EFI_SUCCESS;
  EFI_HANDLE             PartitionHandle;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;
  EFI_DISK_IO_PROTOCOL   *DiskIo;
  CHAR16                 PartitionName[MAX_PARTITION_NAME_LEN];
  CHAR16                 ActivePartitionName[MAX_PARTITION_NAME_LEN];
  AvbIOResult            AvbResult = AVB_IO_RESULT_OK;

  if (AsciiStrCmp (Partition, "recovery") == 0) {
    UnicodeSPrintAsciiFormat (ActivePartitionName, sizeof (ActivePartitionName), "%a", Partition);
  } else {
    UnicodeSPrintAsciiFormat (PartitionName, sizeof (PartitionName), "%a", Partition);

    Status = GetActivePartitionName (PartitionName, ActivePartitionName);
    if (EFI_ERROR (Status)) {
      AvbResult = AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;
      goto Exit;
    }
  }

  PartitionHandle = GetSiblingPartitionHandle (
                      mControllerHandle,
                      ActivePartitionName
                      );
  if (PartitionHandle == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to get sibling partition handle: %s\n", __FUNCTION__, ActivePartitionName));
    AvbResult = AVB_IO_RESULT_ERROR_IO;
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status) || (BlockIo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r to locate block io protocol on partition\r\n", __FUNCTION__, Status));
    AvbResult = AVB_IO_RESULT_ERROR_IO;
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo
                  );
  if (EFI_ERROR (Status) || (DiskIo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r to locate disk io protocol on partition\r\n", __FUNCTION__, Status));
    AvbResult = AVB_IO_RESULT_ERROR_IO;
    goto Exit;
  }

  if (Offset < 0) {
    Offset += (UINTN)(BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  }

  // Make sure offset < PartitionSize
  if (Offset >= (UINTN)(BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid offset=%d, larger than partition size\n", __FUNCTION__, (INT32)Offset));
    AvbResult = AVB_IO_RESULT_ERROR_IO;
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     Offset,
                     NumBytes,
                     (VOID *)Buffer
                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read disk with %r\r\n", __FUNCTION__, Status));
    AvbResult = AVB_IO_RESULT_ERROR_IO;
    goto Exit;
  }

  *NumRead = NumBytes;

Exit:
  return AvbResult;
}

/**
  Read parition data from given offset.

  @param[in]  Ops         A pointer to the AvbOps struct.
  @param[in]  Partition   Partition name string.
  @param[out] GuidBuf     Output buffer for partition guid.
  @param[in]  GuidBufSize Size of partition guid buffer.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
GetUniqueGuidForPartition (
  IN  AvbOps      *Ops,
  IN  const char  *Partition,
  OUT char        *GuidBuf,
  IN  size_t      GuidBufSize
  )
{
  EFI_STATUS                   Status = EFI_SUCCESS;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  EFI_HANDLE                   PartitionHandle;
  CHAR16                       PartitionName[MAX_PARTITION_NAME_LEN];
  CHAR16                       ActivePartitionName[MAX_PARTITION_NAME_LEN];
  AvbIOResult                  AvbResult = AVB_IO_RESULT_OK;
  EFI_GUID                     *PartitionGuid;

  UnicodeSPrintAsciiFormat (PartitionName, sizeof (PartitionName), "%a", Partition);

  Status = GetActivePartitionName (PartitionName, ActivePartitionName);
  if (EFI_ERROR (Status)) {
    AvbResult = AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;
    goto Exit;
  }

  PartitionHandle = GetSiblingPartitionHandle (
                      mControllerHandle,
                      ActivePartitionName
                      );
  if (PartitionHandle == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to get sibling partition handle: %s\n", __FUNCTION__, ActivePartitionName));
    AvbResult = AVB_IO_RESULT_ERROR_IO;
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiPartitionInfoProtocolGuid,
                  (VOID **)&PartitionInfo
                  );
  if (EFI_ERROR (Status) || (PartitionInfo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r to get partition info, or PartitionInfo == NULL\n", __FUNCTION__, Status));
    AvbResult = AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;
    goto Exit;
  }

  PartitionGuid = &PartitionInfo->Info.Gpt.UniquePartitionGUID;
  AsciiSPrintUnicodeFormat (
    GuidBuf,
    GuidBufSize,
    L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    PartitionGuid->Data1,
    PartitionGuid->Data2,
    PartitionGuid->Data3,
    PartitionGuid->Data4[0],
    PartitionGuid->Data4[1],
    PartitionGuid->Data4[2],
    PartitionGuid->Data4[3],
    PartitionGuid->Data4[4],
    PartitionGuid->Data4[5],
    PartitionGuid->Data4[6],
    PartitionGuid->Data4[7]
    );

Exit:
  return AvbResult;
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
  Write rollback index to location in tamper-evident storage.

  @param[in]  Ops                    A pointer to the AvbOps struct.
  @param[in]  RollbackIndexLocation  Location for rollback index in tamper-evident storage.
  @param[in]  RollbackIndex          Rollback index to set.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
WriteRollbackIndex (
  IN AvbOps    *Ops,
  IN size_t    RollbackIndexLocation,
  IN uint64_t  RollbackIndex
  )
{
  // To implement with AVB TA
  return AVB_IO_RESULT_OK;
}

/**
  Read rollback index from location in tamper-evident storage.

  @param[in]  Ops                    A pointer to the AvbOps struct.
  @param[in]  RollbackIndexLocation  Location for rollback index in tamper-evident storage.
  @param[out] RollbackIndex          Buffer for rollback index to read.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
ReadRollbackIndex (
  IN  AvbOps    *Ops,
  IN  size_t    RollbackIndexLocation,
  OUT uint64_t  *OutRollbackIndex
  )
{
  // To implement with AVB TA
  if (OutRollbackIndex == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: OutRollbackIndex == NULL\n", __FUNCTION__));
    return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;
  }

  *OutRollbackIndex = 0;
  return AVB_IO_RESULT_OK;
}

/**
  Validate if vbmeta partition key is trusted key.

  @param[in]  Ops                       A pointer to the AvbOps struct.
  @param[in]  Partition                 Partition name string for the key.
  @param[in]  PubKeyData                Key0 public key buffer.
  @param[in]  PubKeyLength              Key0 public key length.
  @param[in]  PubKeyMetadata            Public key metadata buffer.
  @param[in]  PubKeyMetadataLen         Public key metadata length.
  @param[out] OutIsTrusted              Output buffer to store trusted state.
  @param[out] *OutRollbackIndexLocation Output buffer to store rollback location.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
ValidatePublicKeyForPartition (
  IN  AvbOps         *Ops,
  IN  const char     *Partition,
  IN  const uint8_t  *PubKeyData,
  IN  size_t         PubKeyLength,
  IN  const uint8_t  *PubKeyMetadata,
  IN  size_t         PubKeyMetadataLen,
  OUT bool           *OutIsTrusted,
  OUT uint32_t       *OutRollbackIndexLocation
  )
{
  if ((OutIsTrusted == NULL) || (OutRollbackIndexLocation == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: OutIsTrusted or OutRollbackIndexLocation == NULL\n", __FUNCTION__));
    return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;
  }

  *OutRollbackIndexLocation = 1;
  *OutIsTrusted             = TRUE;

  return AVB_IO_RESULT_OK;
}

/**
  Read persistent value in tamper-evident storage.

  @param[in]  Ops                    A pointer to the AvbOps struct.
  @param[in]  Name                   Persistent value name to read.
  @param[in]  BufferSize             Buffer size provided.
  @param[out] OutBuffer              Output buffer for persistent value.
  @param[out] OutNumBytesRead        Num of bytes read.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
ReadPersistentValue (
  IN  AvbOps              *Ops,
  IN  const         char  *Name,
  IN  size_t              BufferSize,
  OUT uint8_t             *OutBuffer,
  OUT size_t              *OutNumBytesRead
  )
{
  if ((OutBuffer == NULL) || (OutNumBytesRead == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: OutBuffer or OutNumBytesRead == NULL\n", __FUNCTION__));
    return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;
  }

  // To implement with AVB TA
  *OutNumBytesRead = 0;

  return AVB_IO_RESULT_OK;
}

/**
  Write persistent value in tamper-evident storage.

  @param[in]  Ops                    A pointer to the AvbOps struct.
  @param[in]  Name                   Persistent value name to read.
  @param[in]  BufferSize             Buffer size provided.
  @param[in]  Value                  Value to write to persistent value.

  @retval AVB_IO_RESULT_OK  The operation completed successfully.

**/
STATIC
AvbIOResult
WritePersistentValue (
  IN AvbOps         *Ops,
  IN const char     *Name,
  IN size_t         BufferSize,
  IN const uint8_t  *Value
  )
{
  // To implement with AVB TA
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
  // Use libavb API to verify boot chain
  AvbOps               Ops = {
    .read_from_partition               = ReadFromPartition,
    .read_is_device_unlocked           = ReadIsDeviceUnlocked,
    .validate_vbmeta_public_key        = ValidateVbmetaPublicKey,
    .validate_public_key_for_partition = ValidatePublicKeyForPartition,
    .get_unique_guid_for_partition     = GetUniqueGuidForPartition,
    .get_size_of_partition             = GetSizeOfPartition,
    .read_persistent_value             = ReadPersistentValue,
    .write_persistent_value            = WritePersistentValue,
    .read_rollback_index               = ReadRollbackIndex,
    .write_rollback_index              = WriteRollbackIndex,
  };
  AvbSlotVerifyResult  AvbRes                         = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
  BOOLEAN              DeviceUnlocked                 = FALSE;
  AvbSlotVerifyFlags   Flags                          = 0;
  const char           *NormalRequestedPartitions[]   = { "boot", "vendor_boot", NULL };
  const char           *RecoveryRequestedPartitions[] = { "recovery", NULL };
  const char *const    *RequestedPartitions;

  if (ReadIsDeviceUnlocked (&Ops, &DeviceUnlocked) != AVB_IO_RESULT_OK) {
    return EFI_UNSUPPORTED;
  }

  RequestedPartitions = IsRecovery ? RecoveryRequestedPartitions : NormalRequestedPartitions;
  Flags              |= DeviceUnlocked ? AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR : 0;
  Flags              |= IsRecovery ? AVB_SLOT_VERIFY_FLAGS_NO_VBMETA_PARTITION : 0;

  AvbRes = avb_slot_verify (
             &Ops,
             RequestedPartitions,
             "",
             Flags,
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

  mControllerHandle = ControllerHandle;

  Status = VerifiedBootGetBootState (IsRecovery, &BootState, &SlotData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:Avb Verify Boot failed with %r\n", __func__, Status));
  }

  if ((SlotData != NULL) && (AvbCmdline != NULL)) {
    DEBUG ((DEBUG_ERROR, "Avb cmdline: %a\n", SlotData->cmdline));
    *AvbCmdline = SlotData->cmdline;
  }

  return Status;
}
