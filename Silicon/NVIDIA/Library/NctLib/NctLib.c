/** @file
  EDK2 API for NctLib

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/BaseLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/HandleParsingLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/SiblingPartitionLib.h>
#include <Library/NctLib.h>

#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

#define NCT_PARTITION_BASE_NAME  L"NCT"

#define NCT_SPEC_ID_NAME   ("\"id\":\"")
#define NCT_SPEC_CFG_NAME  ("\"config\":\"")

STATIC BOOLEAN  IsNctInitialized = FALSE;
STATIC VOID     *NctPtr          = NULL;

/**
 * Get readable spec id/config from NCT
 *
 * @param[out] Id       Buffer to store spec/id
 * @param[out] Config   Buffer to store spec/config
 *
 * @retval EFI_SUCCESS            The operation completed successfully.
 * @retval EFI_INVALID_PARAMETER  "Id" or "Config" is NULL
 * @retval EFI_NOT_FOUND          Cfg or id is not found in spec
 */
EFI_STATUS
EFIAPI
NctGetSpec (
  OUT CHAR8  *Id,
  OUT CHAR8  *Config
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  NCT_ITEM    Item;
  CHAR8       *Ptr;

  if ((Id == NULL) || (Config == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  Status = NctReadItem (NCT_ID_SPEC, &Item);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get spec from NCT, err:%r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Ptr = (CHAR8 *)AsciiStrStr ((CHAR8 *)&Item.Spec, NCT_SPEC_CFG_NAME);
  if (Ptr == NULL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  Ptr += AsciiStrLen (NCT_SPEC_CFG_NAME);
  while (*Ptr != '\"') {
    *Config++ = *Ptr++;
  }

  *Config = '\0';

  Ptr = (CHAR8 *)AsciiStrStr ((CHAR8 *)&Item.Spec, NCT_SPEC_ID_NAME);
  if (Ptr == NULL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  Ptr += AsciiStrLen (NCT_SPEC_ID_NAME);
  while (*Ptr != '\"') {
    *Id++ = *Ptr++;
  }

  *Id = '\0';

Exit:
  return Status;
}

/**
 * Load Nct.bin from NCT partition and initialize & check header.
 *
 * @param Handle Handle that will be used to access partition.
 *
 * @retval EFI_SUCCESS           All process is successful
 * @retval EFI_NOT_FOUND         Cannot find Handle/PartitionInfo for NCT
 * @retval EFI_OUT_OF_RESOURCES  Not enough memory to allocate buffer for NCT
 * @retval EFI_INVALID_PARAMETER NCT sanity check failed
 */
STATIC
EFI_STATUS
EFIAPI
NctInit (
  EFI_HANDLE  Handle
  )
{
  EFI_STATUS                   Status = EFI_SUCCESS;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  EFI_HANDLE                   PartitionHandle;
  UINTN                        Index;
  UINTN                        NumOfHandles;
  EFI_HANDLE                   *HandleBuffer = NULL;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;
  EFI_DISK_IO_PROTOCOL         *DiskIo;
  NCT_PART_HEAD                *NctHead;
  UINTN                        NctSize;

  DEBUG ((DEBUG_INFO, "%a: Enter NCT init\n", __FUNCTION__));
  if (IsNctInitialized == TRUE) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  if (Handle == NULL) {
    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiPartitionInfoProtocolGuid,
                    NULL,
                    &NumOfHandles,
                    &HandleBuffer
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: Got %r trying to get PartitionInfo Handle\r\n", __FUNCTION__, Status));
      goto Exit;
    }

    for (Index = 0; Index < NumOfHandles; Index++) {
      // Get partition info protcol from handle and validate
      Status = gBS->HandleProtocol (
                      HandleBuffer[Index],
                      &gEfiPartitionInfoProtocolGuid,
                      (VOID **)&PartitionInfo
                      );

      if (EFI_ERROR (Status) || (PartitionInfo == NULL)) {
        Status = EFI_NOT_FOUND;
        DEBUG ((DEBUG_INFO, "%a: Unable to get PartitionInfo from Handle\r\n", __FUNCTION__));
        goto Exit;
      }

      // Found NCT PARTITION
      if (0 == StrCmp (
                 PartitionInfo->Info.Gpt.PartitionName,
                 NCT_PARTITION_BASE_NAME
                 )
          )
      {
        break;
      }
    }

    if (Index >= NumOfHandles) {
      Status = EFI_NOT_FOUND;
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate NCT partition\r\n", __FUNCTION__));
      goto Exit;
    }

    PartitionHandle = HandleBuffer[Index];
  } else {
    PartitionHandle = Handle;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to locate block io protocol on partition\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to locate disk io protocol on partition\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  NctSize = (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  NctPtr  = AllocateZeroPool (NctSize);
  if (NctPtr == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate buffer for NCT\r\n", __FUNCTION__));
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     0,
                     NctSize,
                     (VOID *)NctPtr
                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to read disk\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "%a: NctPtr = 0x%p\n", __FUNCTION__, NctPtr));

  /* Sanity check the NCT header */
  NctHead = (NCT_PART_HEAD *)NctPtr;

  DEBUG ((
    DEBUG_INFO,
    "%a: Magic(0x%x),vid(0x%x),pid(0x%x),ver(V%x.%x),rev(%u)\n",
    __FUNCTION__,
    NctHead->MagicId,
    NctHead->VendorId,
    NctHead->ProductId,
    (NctHead->Version >> 16) & 0xFFFF,
    (NctHead->Version & 0xFFFF),
    NctHead->Revision
    ));

  DEBUG ((
    DEBUG_INFO,
    "%a: tns(0x%x),tns offset(0x%x),tns len(%u)\n",
    __FUNCTION__,
    NctHead->TnsId,
    NctHead->TnsOff,
    NctHead->TnsLen
    ));

  if (CompareMem (&NctHead->MagicId, NCT_MAGIC_ID, NCT_MAGIC_ID_LEN) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: NCT error: magic ID error (0x%x/0x%p:%a)\n",
      __FUNCTION__,
      NctHead->MagicId,
      NCT_MAGIC_ID,
      NCT_MAGIC_ID
      ));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  IsNctInitialized = TRUE;

Exit:
  if (EFI_ERROR (Status) && (NctPtr != NULL)) {
    FreePool (NctPtr);
  }

  return Status;
}

/**
 * Read an Nct Item with a given ID
 *
 * @param[in]  Id       Nct item Id to read
 * @param[out] Buf      Output buffer to store Nct Item
 *
 * @retval EFI_SUCCESS            All process is successful
 * @retval EFI_NOT_READY          Nct is not initialized
 * @retval EFI_INVALID_PARAMETER  "Id" exceeds limit or output "Buf" is NULL, or integrity broken
 */
EFI_STATUS
EFIAPI
NctReadItem (
  IN  NCT_ID    Id,
  OUT NCT_ITEM  *Buf
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  NCT_ENTRY   *Entry;

  if (IsNctInitialized == FALSE) {
    DEBUG ((DEBUG_ERROR, "%a: Error: NCT has not been initialized\n", __FUNCTION__));
    Status = EFI_NOT_READY;
    goto Exit;
  }

  if (Id > NCT_ID_END) {
    DEBUG ((DEBUG_ERROR, "%a: Error: Invalid nct id: %u\n", __FUNCTION__, Id));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if (Buf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Error: Buffer is NULL\n", __FUNCTION__));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  Entry = (NCT_ENTRY *)((UINT8 *)NctPtr + NCT_ENTRY_OFFSET + (Id * sizeof (NCT_ENTRY)));

  /* check index integrity */
  if (Id != Entry->Index) {
    DEBUG ((DEBUG_ERROR, "%a: ID err(0x%x/0x%x)\n", __FUNCTION__, Id, Entry->Index));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  CopyMem (Buf, &Entry->Data, sizeof (NCT_ITEM));

Exit:
  return Status;
}

/**
 * Get a serial number from Nvidia Configrature Table.
 *
 * @param[out] SerialNumber  Output buffer to store SN
 *
 * @retval EFI_SUCCESS            The serial number was gotten successfully.
 * @retval EFI_INVALID_PARAMETER  "SerialNumber" buffer is NULL.
 */
EFI_STATUS
EFIAPI
NctGetSerialNumber (
  OUT CHAR8  *SerialNumber
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  NCT_ITEM    Item;
  UINT32      Len;

  if (IsNctInitialized == FALSE) {
    Status = NctInit (NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to initialize NCT\n", __FUNCTION__, Status));
      return Status;
    }
  }

  if (SerialNumber == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: SerialNumber buffer is NULL\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Status = NctReadItem (NCT_ID_SERIAL_NUMBER, &Item);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to read NCT item\n", __FUNCTION__, Status));
    return Status;
  }

  Len = AsciiStrLen (Item.SerialNumber.Sn);
  CopyMem (SerialNumber, Item.SerialNumber.Sn, Len);

  return Status;
}
