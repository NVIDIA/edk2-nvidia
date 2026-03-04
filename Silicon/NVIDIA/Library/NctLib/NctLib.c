/** @file
  EDK2 API for NctLib

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/UefiLib.h>
#include <Library/SiblingPartitionLib.h>
#include <Library/NctLib.h>
#include <Library/FdtLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PlatformResourceLib.h>

#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

#define NCT_PARTITION_BASE_NAME  L"NCT"

#define NCT_SPEC_ID_NAME   ("\"id\":\"")
#define NCT_SPEC_CFG_NAME  ("\"config\":\"")

STATIC BOOLEAN        IsNctInitialized = FALSE;
STATIC NCT_PART_HEAD  *NctHead;
STATIC VOID           *NctPtr = NULL;

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
 * @param[in]  BufferSize    Output buffer size to store SN
 *
 * @retval EFI_SUCCESS            The serial number was gotten successfully.
 * @retval EFI_INVALID_PARAMETER  "SerialNumber" buffer is NULL.
 */
EFI_STATUS
EFIAPI
NctGetSerialNumber (
  OUT CHAR8   *SerialNumber,
  IN  UINT32  BufferSize
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  NCT_ITEM    Item;

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

  if (AsciiStrLen (Item.SerialNumber.Sn) == 0) {
    DEBUG ((DEBUG_ERROR, "%a: NCT SerialNumber is empty\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  AsciiStrCpyS (SerialNumber, BufferSize, Item.SerialNumber.Sn);

  return EFI_SUCCESS;
}

/**
 * Get tnspec from Nvidia Configrature Table.
 *
 * @param[out] Tnspec  Output buffer to store Tnspec string
 *
 * @retval EFI_SUCCESS            The serial number was gotten successfully.
 */
STATIC
EFI_STATUS
EFIAPI
NctGetTnspec (
  OUT CHAR8  **Tnspec
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  CHAR8       *NctTnspecPtr;
  UINT32      Len;

  if (IsNctInitialized == FALSE) {
    Status = NctInit (NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to initialize NCT\n", __FUNCTION__, Status));
      return Status;
    }
  }

  // Check if TNS field present is correct
  if (CompareMem (&NctHead->TnsId, TNS_MAGIC_ID, TNS_MAGIC_ID_LEN)) {
    DEBUG ((DEBUG_ERROR, "%a: tns ID error (0x%x/0x%p:%s)\n", __FUNCTION__, NctHead->TnsId, TNS_MAGIC_ID, TNS_MAGIC_ID_LEN));
  }

  // Lenggh of tnspec in NCT
  Len = NctHead->TnsLen;
  if ((Len == 0) || (Len > MAX_TNSPEC_LEN)) {
    DEBUG ((DEBUG_ERROR, "%a: tnspec length is %d, should be between 0 and %d\n", __FUNCTION__, Len, MAX_TNSPEC_LEN));
    return EFI_BAD_BUFFER_SIZE;
  }

  NctTnspecPtr = (CHAR8 *)((CHAR8 *)NctPtr + NctHead->TnsOff);
  if (NctTnspecPtr[Len] != '\0') {
    DEBUG ((DEBUG_ERROR, "%a: No NULL termination for tnspec\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  *Tnspec = NctTnspecPtr;

  return EFI_SUCCESS;
}

/**
 * Dump tnspec from Nvidia Configrature Table.
 *
 * @retval EFI_SUCCESS            The tnspec was dumpped successfully.
 */
EFI_STATUS
EFIAPI
NctDumpTnspecToDtb (
  VOID
  )
{
  VOID        *DeviceTree;
  EFI_STATUS  Status = EFI_SUCCESS;
  INT32       Node;
  INT32       TnspecNode;
  INT32       Ret;
  CHAR8       *Tnspec;

  Status = NctGetTnspec (&Tnspec);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get tnspec from NCT\n", __FUNCTION__, Status));
    return Status;
  }

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &DeviceTree);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get dtb ptr\n", __FUNCTION__, Status));
    return Status;
  }

  Node = FdtPathOffset (DeviceTree, "/chosen");

  // Find or create /chosen/tnspec subnode
  TnspecNode = FdtPathOffset (DeviceTree, "/chosen/tnspec");
  if (TnspecNode < 0) {
    // /chosen/tnspec subnode doesn't exist, create it
    DEBUG ((DEBUG_INFO, "%a: /chosen/tnspec subnode not found, creating it\n", __FUNCTION__));
    TnspecNode = FdtAddSubnode (DeviceTree, Node, "tnspec");
    if (TnspecNode < 0) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to create /chosen/tnspec subnode: %d\n", __FUNCTION__, TnspecNode));
      return EFI_NOT_FOUND;
    }
  }

  // Set the tnspec property in /chosen/tnspec subnode
  Ret = FdtSetProp (DeviceTree, TnspecNode, "tnspec", Tnspec, AsciiStrLen (Tnspec) + 1);
  if (Ret) {
    DEBUG ((DEBUG_ERROR, "%a: Could not set tnspec property in /chosen/tnspec: %d\n", __FUNCTION__, Ret));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
 * Set a MAC address property on a DTB node.
 *
 * @param[in,out] Dtb       Pointer to FDT blob.
 * @param[in]     NodePath  Absolute DTB node path.
 * @param[in]     PropName  Property name to set.
 * @param[in]     Addr      6-byte MAC address.
 * @param[in]     Label     Human-readable label for log messages.
 */
STATIC
EFI_STATUS
SetMacAddrInDtb (
  IN OUT VOID         *Dtb,
  IN     CONST CHAR8  *NodePath,
  IN     CONST CHAR8  *PropName,
  IN     UINT8        *Addr,
  IN     CONST CHAR8  *Label
  )
{
  INT32  NodeOffset;
  INT32  FdtErr;

  NodeOffset = FdtPathOffset (Dtb, NodePath);
  if (NodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: DTB node %a not found (fdt err=%d)\n", __FUNCTION__, NodePath, NodeOffset));
    return EFI_NOT_FOUND;
  }

  FdtErr = FdtSetProp (Dtb, NodeOffset, PropName, Addr, 6);
  if (FdtErr < 0) {
    DEBUG ((DEBUG_ERROR, "%a: FdtSetProp failed for %a/%a (fdt err=%d)\n", __FUNCTION__, NodePath, PropName, FdtErr));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: %a = %02x:%02x:%02x:%02x:%02x:%02x\n",
    __FUNCTION__,
    Label,
    Addr[0],
    Addr[1],
    Addr[2],
    Addr[3],
    Addr[4],
    Addr[5]
    ));

  return EFI_SUCCESS;
}

/**
 * Create a top-level DTB node if it does not already exist.
 *
 * @param[in,out] Dtb       Pointer to FDT blob.
 * @param[in]     NodePath  Absolute path starting with '/'.
 *
 * @return  Node offset (>= 0) on success, or negative fdt error code.
 */
STATIC
INT32
NctCreateNode (
  IN OUT VOID         *Dtb,
  IN     CONST CHAR8  *NodePath
  )
{
  INT32  Node;

  Node = FdtPathOffset (Dtb, NodePath);
  if (Node < 0) {
    DEBUG ((DEBUG_ERROR, "%a: node %a not found, creating it\n", __FUNCTION__, NodePath));
    Node = FdtAddSubnode (Dtb, 0, NodePath + 1);
    if (Node < 0) {
      DEBUG ((DEBUG_ERROR, "%a: failed to create node %a: %d\n", __FUNCTION__, NodePath, Node));
    }
  }

  return Node;
}

/**
 * Dump NCT items (spec, factory-mode) into a /nct node of the given DTB.
 */
EFI_STATUS
EFIAPI
NctDumpNctToDtb (
  IN OUT VOID  *Dtb
  )
{
  EFI_STATUS  Status;
  NCT_ITEM    Item;
  INT32       NctNode;
  INT32       FdtErr;
  UINT32      CellValue;

  if (Dtb == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsNctInitialized == FALSE) {
    Status = NctInit (NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: NCT init failed: %r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  NctNode = NctCreateNode (Dtb, "/nct");
  if (NctNode < 0) {
    return EFI_NOT_FOUND;
  }

  /* /nct/spec (string) from NCT_ID_SPEC */
  Status = NctReadItem (NCT_ID_SPEC, &Item);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read NCT_ID_SPEC: %r\n", __FUNCTION__, Status));
    return Status;
  }

  FdtErr = FdtSetProp (Dtb, NctNode, "spec", Item.Spec.Data, AsciiStrLen ((CHAR8 *)Item.Spec.Data) + 1);
  if (FdtErr < 0) {
    DEBUG ((DEBUG_ERROR, "%a: fdt_setprop spec failed: %d\n", __FUNCTION__, FdtErr));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DEBUG_ERROR, "%a: /nct/spec = %a\n", __FUNCTION__, (CHAR8 *)Item.Spec.Data));

  /* /nct/factory-mode (uint32 cell) from NCT_ID_FACTORY_MODE */
  Status = NctReadItem (NCT_ID_FACTORY_MODE, &Item);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read NCT_ID_FACTORY_MODE: %r\n", __FUNCTION__, Status));
    return Status;
  }

  CellValue = CpuToFdt32 (Item.FactoryMode.Flag);
  FdtErr    = FdtSetProp (Dtb, NctNode, "factory-mode", &CellValue, sizeof (CellValue));
  if (FdtErr < 0) {
    DEBUG ((DEBUG_ERROR, "%a: fdt_setprop factory-mode failed: %d\n", __FUNCTION__, FdtErr));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DEBUG_ERROR, "%a: /nct/factory-mode = %u\n", __FUNCTION__, Item.FactoryMode.Flag));

  return EFI_SUCCESS;
}

/*
 * Populate WiFi, Ethernet, and Bluetooth MAC addresses from NCT into a DTB.
 */
EFI_STATUS
EFIAPI
NctPopulateMacAddrs (
  IN OUT VOID  *Dtb
  )
{
  EFI_STATUS  Status;
  NCT_ITEM    Item;

  if (Dtb == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsNctInitialized == FALSE) {
    Status = NctInit (NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: NCT init failed: %r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  /* WiFi: /wifi/mac-address */
  Status = NctReadItem (NCT_ID_WIFI_ADDR, &Item);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read WiFi MAC from NCT: %r\n", __FUNCTION__, Status));
  }

  Status = SetMacAddrInDtb (Dtb, "/wifi", "mac-address", Item.WifiAddr.Addr, "WiFi MAC");
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set WiFi MAC from NCT: %r\n", __FUNCTION__, Status));
  }

  /* Ethernet: /ethernet/mac-address */
  Status = NctReadItem (NCT_ID_ETH_ADDR, &Item);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read ETH MAC from NCT: %r\n", __FUNCTION__, Status));
  }

  Status = SetMacAddrInDtb (Dtb, "/ethernet", "mac-address", Item.EthAddr.Addr, "ETH MAC");
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set ETH MAC from NCT: %r\n", __FUNCTION__, Status));
  }

  /* Bluetooth: /serial@3130000/bluetooth/local-bd-address */
  Status = NctReadItem (NCT_ID_BT_ADDR, &Item);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read BT MAC from NCT: %r\n", __FUNCTION__, Status));
  }

  Status = SetMacAddrInDtb (Dtb, "/serial@3130000/bluetooth", "local-bd-address", Item.BtAddr.Addr, "BT MAC");
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set BT MAC from NCT: %r\n", __FUNCTION__, Status));
  }

  /* Thread network: /serial@c6f0000/threadnetwork/mac-address */
  Status = NctReadItem (NCT_ID_THREAD_ADDR, &Item);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read THREAD MAC from NCT: %r\n", __FUNCTION__, Status));
  }

  Status = SetMacAddrInDtb (Dtb, "/serial@c6f0000/threadnetwork", "mac-address", Item.BtAddr.Addr, "THREAD MAC");
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set THREAD MAC from NCT: %r\n", __FUNCTION__, Status));
  }

  return Status;
}
