/** @file

  Addendum to NOR Flash Standalone MM Driver for DICE feature

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <NorFlashPrivate.h>
#include <Protocol/QspiController.h>
#include <Uefi/UefiGpt.h>
#include <Library/GptLib.h>

#define MM_DICE_READ                      (1)
#define MM_DICE_WRITE                     (2)
#define MM_DICE_LOCK                      (3)
#define MM_DICE_CHECK_LOCK_STATUS         (4)
#define GPT_PARTITION_BLOCK_SIZE          (512)
#define WORM_PARTITION_NAME               L"worm"
#define MM_DICE_CERT_MAGIC                "DICECERT"
#define MM_DICE_CERT_MAGIC_LEN            (8)
#define MM_DICE_CERT_NUM_MAX              (3)
#define MM_COMMUNICATE_DICE_HEADER_SIZE   (OFFSET_OF (MM_COMMUNICATE_DICE_HEADER, Data))
#define MM_DICE_CERT_CONTENT_HEADER_SIZE  (OFFSET_OF (MM_DICE_CERT_CONTENT, Value))

typedef struct {
  UINTN         Function;
  EFI_STATUS    ReturnStatus;
  UINT8         Data[1];
} MM_COMMUNICATE_DICE_HEADER;

typedef struct {
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlashProtocol;
  NOR_FLASH_ATTRIBUTES         NorFlashAttributes;
  UINT64                       WormOffset;
  UINT64                       WormSize;
} MM_DICE_WORM_INFO;

typedef struct {
  UINT32    Type;
  UINT32    Length;
  UINT8     Value[1];
} MM_DICE_CERT_CONTENT;

STATIC UINT64                 QspiBaseAddress;
STATIC UINTN                  QspiSize;
STATIC NOR_FLASH_DEVICE_INFO  SupportedDevices[] = {
  {
    .Name           = "Macronix 64MB\0",
    .ManufacturerId = 0xC2,
    .MemoryType     = 0x95,
    .Density        = 0x3A
  },
};
STATIC MM_DICE_WORM_INFO      *WormInfo = NULL;

/*
 * The helper function to get current active certificate slot
 *
 * @param[in]         NorFlashProtocol   The handler of NVIDIA nor flash protocol
 * @param[out]        CertIndex          The integer to save the current active certificate slot
 *                                       Possible values: [-1|0|1|2], -1 means no certificates in WORM partition
 *
 * @retval EFI_SUCCESS    Operation successful.
 * @retval others         Error occurred
 */
STATIC
EFI_STATUS
DiceGetActiveCertIndex (
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  OUT INTN                      *CertIndex
  )
{
  EFI_STATUS  Status     = EFI_SUCCESS;
  INTN        idx        = 0;
  UINT32      ReadOffset = 0;
  CHAR8       Magic[MM_DICE_CERT_MAGIC_LEN];

  if ((NorFlashProtocol == NULL) || (CertIndex == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *CertIndex = -1;
  for (idx = MM_DICE_CERT_NUM_MAX - 1; idx >= 0; idx--) {
    ZeroMem (Magic, MM_DICE_CERT_MAGIC_LEN);
    ReadOffset = WormInfo->WormOffset + (WormInfo->WormSize / MM_DICE_CERT_NUM_MAX) * idx;
    Status     = NorFlashProtocol->Read (
                                     NorFlashProtocol,
                                     ReadOffset,
                                     MM_DICE_CERT_MAGIC_LEN,
                                     Magic
                                     );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to read cert(%d) magic (%r)\n", idx, Status));
      break;
    }

    if (AsciiStrnCmp (Magic, MM_DICE_CERT_MAGIC, MM_DICE_CERT_MAGIC_LEN) != 0) {
      continue;
    }

    *CertIndex = idx;
    break;
  }

  return Status;
}

/*
 * The main function to handle the UEFI SMM DICE requests.
 *
 * @param[in]         DispatchHandle     The handler coming from UEFI SMM, unused for now
 * @param[in]         RegisterContext    A context that UEFI SMM users can register, unused for now
 * @param[in, out]    CommBuffer         The communication buffer between OP-TEE and UEFI SMM
 *                                       "MM_COMMUNICATE_DICE_HEADER" describes the format of this buffer
 * @param[in, out]    CommBufferSize     The size of the communication buffer
 *
 * @retval EFI_SUCCESS    Operation successful.
 * @retval others         Error occurred
 */
STATIC
EFI_STATUS
DiceProtocolMmHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  EFI_STATUS                  Status = EFI_SUCCESS;
  MM_COMMUNICATE_DICE_HEADER  *DiceHeader;
  NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocol;
  NOR_FLASH_ATTRIBUTES        *NorFlashAttributes;
  UINTN                       PayloadSize;
  MM_DICE_CERT_CONTENT        ReadCertHeader;
  UINT32                      ReadOffset = 0;
  INTN                        CertIndex;
  UINTN                       RespDataSize = 0;

  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DiceHeader         = (MM_COMMUNICATE_DICE_HEADER *)CommBuffer;
  NorFlashProtocol   = WormInfo->NorFlashProtocol;
  NorFlashAttributes = &WormInfo->NorFlashAttributes;
  if (*CommBufferSize < MM_COMMUNICATE_DICE_HEADER_SIZE) {
    DEBUG ((DEBUG_ERROR, "Communication buffer is too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  DiceHeader->ReturnStatus = EFI_SUCCESS;
  switch (DiceHeader->Function) {
    case MM_DICE_READ:
      PayloadSize = *CommBufferSize - MM_COMMUNICATE_DICE_HEADER_SIZE;
      if (PayloadSize <= MM_DICE_CERT_CONTENT_HEADER_SIZE) {
        // Type and Length are mandatory
        DEBUG ((DEBUG_ERROR, "Communication buffer is too small\n"));
        Status = EFI_BUFFER_TOO_SMALL;
        goto exit;
      }

      Status = DiceGetActiveCertIndex (NorFlashProtocol, &CertIndex);
      if (EFI_ERROR (Status)) {
        goto exit;
      }

      if (CertIndex == -1) {
        // No valid certificates
        DiceHeader->ReturnStatus = EFI_NO_MEDIA;
        goto exit;
      }

      ZeroMem (&ReadCertHeader, sizeof (MM_DICE_CERT_CONTENT));
      ReadOffset = WormInfo->WormOffset +
                   (WormInfo->WormSize / MM_DICE_CERT_NUM_MAX) * CertIndex +
                   MM_DICE_CERT_MAGIC_LEN;
      Status = NorFlashProtocol->Read (
                                   NorFlashProtocol,
                                   ReadOffset,
                                   MM_DICE_CERT_CONTENT_HEADER_SIZE,
                                   &ReadCertHeader
                                   );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to read cert(%d) type and length (%r)\n", CertIndex, Status));
        goto exit;
      }

      RespDataSize = MM_COMMUNICATE_DICE_HEADER_SIZE +
                     MM_DICE_CERT_CONTENT_HEADER_SIZE +
                     ReadCertHeader.Length;
      if (*CommBufferSize < RespDataSize) {
        DEBUG ((DEBUG_ERROR, "Communication buffer is too small\n"));
        DiceHeader->ReturnStatus = EFI_BUFFER_TOO_SMALL;
        goto exit;
      }

      Status = NorFlashProtocol->Read (
                                   NorFlashProtocol,
                                   ReadOffset,
                                   MM_DICE_CERT_CONTENT_HEADER_SIZE + ReadCertHeader.Length,
                                   DiceHeader->Data
                                   );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to read cert(%d) content (%r)\n", CertIndex, Status));
        goto exit;
      }

      *CommBufferSize = RespDataSize;
      break;

    case MM_DICE_WRITE:
      break;
    case MM_DICE_LOCK:
      break;
    case MM_DICE_CHECK_LOCK_STATUS:
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: Unknown request: %u\n", __FUNCTION__, DiceHeader->Function));
      Status = EFI_INVALID_PARAMETER;
      break;
  }

exit:
  if (EFI_ERROR (Status)) {
    DiceHeader->ReturnStatus = Status;
  }

  return Status;
}

/*
 * The function to get the WORM partition offset and size.
 * The offset and size will be saved in the global MM_DICE_WORM_INFO struct.
 *
 * @retval EFI_SUCCESS    Operation successful.
 * @retval others         Error occurred
 */
STATIC
EFI_STATUS
GetWormPartitionInfo (
  VOID
  )
{
  EFI_STATUS                  Status = EFI_SUCCESS;
  NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocol;
  NOR_FLASH_ATTRIBUTES        *NorFlashAttributes;
  EFI_PARTITION_TABLE_HEADER  PartitionHeader;
  VOID                        *PartitionEntryArray = NULL;
  CONST EFI_PARTITION_ENTRY   *PartitionEntry;

  NorFlashProtocol   = WormInfo->NorFlashProtocol;
  NorFlashAttributes = &WormInfo->NorFlashAttributes;
  Status             = NorFlashProtocol->GetAttributes (NorFlashProtocol, NorFlashAttributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get NOR Flash attributes (%r)\n", __FUNCTION__, Status));
    goto exit;
  }

  // Validate GPT and get table entries, always 512 bytes from the end
  Status = NorFlashProtocol->Read (
                               NorFlashProtocol,
                               NorFlashAttributes->MemoryDensity - GPT_PARTITION_BLOCK_SIZE,
                               sizeof (PartitionHeader),
                               &PartitionHeader
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read GPT partition table (%r)\n", __FUNCTION__, Status));
    goto exit;
  }

  Status = GptValidateHeader (&PartitionHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid efi partition table header\n", __FUNCTION__));
    goto exit;
  }

  // Read the partition Entries
  PartitionEntryArray = AllocateZeroPool (GptPartitionTableSizeInBytes (&PartitionHeader));
  if (PartitionEntryArray == NULL) {
    goto exit;
  }

  Status = NorFlashProtocol->Read (
                               NorFlashProtocol,
                               PartitionHeader.PartitionEntryLBA * GPT_PARTITION_BLOCK_SIZE,
                               GptPartitionTableSizeInBytes (&PartitionHeader),
                               PartitionEntryArray
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read GPT partition array (%r)\n", __FUNCTION__, Status));
    goto exit;
  }

  Status = GptValidatePartitionTable (&PartitionHeader, PartitionEntryArray);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Invalid PartitionEntryArray\n"));
    goto exit;
  }

  // Find WORM partition
  PartitionEntry = GptFindPartitionByName (
                     &PartitionHeader,
                     PartitionEntryArray,
                     WORM_PARTITION_NAME
                     );
  if (PartitionEntry == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Can't find WORM partition.\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto exit;
  }

  WormInfo->WormOffset = PartitionEntry->StartingLBA * GPT_PARTITION_BLOCK_SIZE;
  WormInfo->WormSize   = GptPartitionSizeInBlocks (PartitionEntry) * GPT_PARTITION_BLOCK_SIZE;
  if (WormInfo->WormOffset % NorFlashAttributes->BlockSize != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid offset of WORM partition: 0x%lx\n", __FUNCTION__, WormInfo->WormOffset));
    Status = EFI_DEVICE_ERROR;
    goto exit;
  }

  if (WormInfo->WormSize % NorFlashAttributes->BlockSize != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid size of WORM partition: 0x%lx\n", __FUNCTION__, WormInfo->WormSize));
    Status = EFI_DEVICE_ERROR;
    goto exit;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Got worm offset: 0x%lx, size: 0x%lx\n",
    __FUNCTION__,
    WormInfo->WormOffset,
    WormInfo->WormSize
    ));

exit:
  if (PartitionEntryArray != NULL) {
    FreePool (PartitionEntryArray);
  }

  return Status;
}

/*
 * A helper function to determine if the SPI-NOR supports DICE certificate functionalities or not.
 * This is done by checking a device whitelist we created.
 * Basically the most important feature the SPI-NOR needs to support is lock.
 * By locking the specified sector/block, the block of data can not be erased or written unless an
 * unlock is issued.
 *
 * @retval TRUE    Support
 * @retval FALSE   Not support
 */
STATIC
BOOLEAN
IsNorFlashDeviceSupported (
  VOID
  )
{
  UINT8                    Cmd;
  UINT8                    DeviceID[NOR_READ_RDID_RESP_SIZE];
  QSPI_TRANSACTION_PACKET  Packet;
  EFI_STATUS               Status;
  BOOLEAN                  SupportedDevice;
  UINTN                    idx;

  Cmd = NOR_READ_RDID_CMD;
  ZeroMem (DeviceID, sizeof (DeviceID));

  Packet.TxBuf = &Cmd;
  Packet.RxBuf = DeviceID;
  Packet.TxLen = sizeof (Cmd);
  Packet.RxLen = sizeof (DeviceID);

  Status = QspiPerformTransaction ((EFI_PHYSICAL_ADDRESS)QspiBaseAddress, &Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Could not read NOR flash's device ID (%r)\n", __FUNCTION__, Status));
    return FALSE;
  }

  // Match the read Device ID with what is in Flash Attributes table.
  DEBUG ((
    DEBUG_INFO,
    "%a: Device ID: 0x%02x 0x%02x 0x%02x\n",
    __FUNCTION__,
    DeviceID[NOR_RDID_MANU_ID_OFFSET],
    DeviceID[NOR_RDID_MEM_INTF_TYPE_OFFSET],
    DeviceID[NOR_RDID_MEM_DENSITY_OFFSET]
    ));

  SupportedDevice = FALSE;
  for (idx = 0; idx < (sizeof (SupportedDevices) / sizeof (SupportedDevices[0])); idx++) {
    if ((DeviceID[NOR_RDID_MANU_ID_OFFSET] == SupportedDevices[idx].ManufacturerId) &&
        (DeviceID[NOR_RDID_MEM_INTF_TYPE_OFFSET] == SupportedDevices[idx].MemoryType) &&
        (DeviceID[NOR_RDID_MEM_DENSITY_OFFSET] == SupportedDevices[idx].Density))
    {
      DEBUG ((DEBUG_INFO, "Found compatible device: %a\n", SupportedDevices[idx].Name));
      SupportedDevice = TRUE;
      break;
    }
  }

  if (SupportedDevice == FALSE) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Device with Manu 0x%02x MemType 0x%02x Density 0x%02x"
      " isn't supported\n",
      __FUNCTION__,
      DeviceID[NOR_RDID_MANU_ID_OFFSET],
      DeviceID[NOR_RDID_MEM_INTF_TYPE_OFFSET],
      DeviceID[NOR_RDID_MEM_DENSITY_OFFSET]
      ));
  }

  return SupportedDevice;
}

/*
 * The entry function of UEFI SMM DICE module
 *
 * @param[in]     ImageHandle      The handle of the DICE SMM image
 * @param[in]     MmSystemTable    The UEFI SMM system table
 *
 * @retval EFI_SUCCESS    Operation successful.
 * @retval others         Error occurred
 */
EFI_STATUS
EFIAPI
NorFlashDiceInitialise (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  EFI_HANDLE  Handle;

  if (PcdGetBool (PcdEmuVariableNvModeEnable)) {
    return EFI_SUCCESS;
  }

  if (!IsQspiPresent ()) {
    return EFI_SUCCESS;
  }

  WormInfo = AllocateZeroPool (sizeof (MM_DICE_WORM_INFO));
  if (WormInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gMmst->MmLocateProtocol (
                    &gNVIDIANorFlashProtocolGuid,
                    NULL,
                    (VOID **)&(WormInfo->NorFlashProtocol)
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get NOR Flash protocol (%r)\n", __FUNCTION__, Status));
    goto exit;
  }

  Status = GetQspiDeviceRegion (&QspiBaseAddress, &QspiSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Qspi MMIO region not found (%r)\n", __FUNCTION__, Status));
    goto exit;
  }

  if (IsNorFlashDeviceSupported () == FALSE) {
    FreePool (WormInfo);
    goto exit;
  }

  Status = GetWormPartitionInfo ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get WORM partition info (%r)\n", __FUNCTION__, Status));
    // Worm partition is not ready on all platforms.
    // Return OK to not break UEFI SMM
    Status = EFI_SUCCESS;
    FreePool (WormInfo);
    goto exit;
  }

  Handle = NULL;
  Status = gMmst->MmiHandlerRegister (
                    DiceProtocolMmHandler,
                    &gNVIDIANorFlashDiceProtocolGuid,
                    &Handle
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Register MMI handler failed (%r)\n", __FUNCTION__, Status));
    goto exit;
  }

exit:
  if (EFI_ERROR (Status) && (WormInfo != NULL)) {
    FreePool (WormInfo);
  }

  return Status;
}
