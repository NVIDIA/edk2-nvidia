/** @file

  NOR Flash Driver

  Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiRuntimeLib.h>

#include <libfdt.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/BlockIo.h>
#include <Protocol/EraseBlock.h>
#include <Protocol/NorFlash.h>
#include <Protocol/QspiController.h>
#include <Protocol/DeviceTreeNode.h>


#define NOR_FLASH_SIGNATURE SIGNATURE_32('N','O','R','F')

#define TIMEOUT                       100

#define NOR_READ_ANY_REG              0x65
#define NOR_REG_OFFSET_SIZE           4
#define NOR_CR3V_REG_OFFSET           0x00800004
#define NOR_CR3V_PAGE_BUF_WRAP_BMSK   0x10
#define NOR_CR3V_BLK_SIZE_ERASE_BMSK  0x2

#define NOR_READ_SR1                  0x5
#define NOR_SR1_WEL_BMSK              0x2
#define NOR_SR1_WIP_BMSK              0x1
#define NOR_SR1_WEL_RETRY_CNT         2000
#define NOR_SR1_WIP_RETRY_CNT         2000

#define NOR_CMD_SIZE                  1
#define NOR_ADDR_SIZE                 4

#define NOR_WRITE_DATA_CMD            0x12
#define NOR_READ_DATA_CMD             0x13
#define NOR_WREN_DISABLE              0x4
#define NOR_WREN_ENABLE               0x6
#define NOR_ERASE_DATA_CMD            0xDC

#define NOR_READ_RDID_CMD             0x9f
#define NOR_READ_RDID_RESP_SIZE       3
#define NOR_RDID_MANU_ID_OFFSET       0
#define NOR_RDID_MEM_INTF_TYPE_OFFSET 1
#define NOR_RDID_MEM_DENSITY_OFFSET   2

#define NOR_PAGE_SIZE                 256
typedef struct {
  UINT32                           Signature;
  UINT32                           FlashInstance;
  EFI_HANDLE                       QspiControllerHandle;
  EFI_HANDLE                       NorFlashHandle;
  BOOLEAN                          ProtocolsInstalled;
  NVIDIA_NOR_FLASH_PROTOCOL        NorFlashProtocol;
  EFI_BLOCK_IO_PROTOCOL            BlockIoProtocol;
  EFI_ERASE_BLOCK_PROTOCOL         EraseBlockProtocol;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiController;
  EFI_DEVICE_PATH_PROTOCOL         *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL         *NorFlashDevicePath;
  EFI_EVENT                        VirtualAddrChangeEvent;
  UINT8                            CommandBuffer[NOR_CMD_SIZE + NOR_ADDR_SIZE + NOR_PAGE_SIZE];
} NOR_FLASH_PRIVATE_DATA;


#define NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(a)   CR(a, NOR_FLASH_PRIVATE_DATA, NorFlashProtocol, NOR_FLASH_SIGNATURE)
#define NOR_FLASH_PRIVATE_DATA_FROM_BLOCK_IO_PROTOCOL(a)    CR(a, NOR_FLASH_PRIVATE_DATA, BlockIoProtocol, NOR_FLASH_SIGNATURE)
#define NOR_FLASH_PRIVATE_DATA_FROM_ERASE_BLOCK_PROTOCOL(a) CR(a, NOR_FLASH_PRIVATE_DATA, EraseBlockProtocol, NOR_FLASH_SIGNATURE)


NOR_FLASH_ATTRIBUTES FlashAttributes[] = {
  {
    "s25fs512s",                   // Flash name
    SPANSION_MANUFACTURER_ID,      // Manufacturer ID
    SPANSION_SPI_NOR_INTERFACE_ID, // Memory Interface Type
    SPANSION_FLASH_DENSITY_512,    // Memory Size
    SIZE_256KB,                    // Sector Size
    256,                           // Number of Sectors
    SIZE_64KB,                     // Block Size
    NOR_PAGE_SIZE                  // page Size
  },
  {
    "s25fs256s",                   // Flash name
    SPANSION_MANUFACTURER_ID,      // Manufacturer ID
    SPANSION_SPI_NOR_INTERFACE_ID, // Memory Interface Type
    SPANSION_FLASH_DENSITY_256,    // Memory Size
    SIZE_256KB,                    // Sector Size
    128,                           // Number of Sectors
    SIZE_64KB,                     // Block Size
    NOR_PAGE_SIZE                  // page Size
  },
  {
  }
};


EFI_BLOCK_IO_MEDIA Media = {
  0,        // Media ID gets updated during Start
  FALSE,    // Non removable media
  TRUE,     // Media currently present
  0,        // First logical block
  FALSE,    // Not read only
  FALSE,    // Does not cache write data
  SIZE_4KB, // Block size gets updated during start
  4,        // Alignment required
  0         // Last logical block gets updated during start
};


VENDOR_DEVICE_PATH VendorDevicePath = {
  {
    HARDWARE_DEVICE_PATH,
    HW_VENDOR_DP,
    {
      (UINT8)( sizeof(VENDOR_DEVICE_PATH) ),
      (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8)
    }
  },
  { 0x8332de7f, 0x50c3, 0x47ca, { 0x82, 0x4e, 0x83, 0x3a, 0xac, 0x7c, 0xf1, 0x6d } }
};


/**
  Read a register in the NOR Flash

  @param[in]  Private               Driver's private data
  @param[in]  Cmd                   Register to be read.
  @param[in]  CmdSize               Length of command.
  @param[out] Resp                  Pointer for register data.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
ReadNorFlashRegister (
  IN  NOR_FLASH_PRIVATE_DATA *Private,
  IN  UINT8                  *Cmd,
  IN  UINT32                 CmdSize,
  OUT UINT8                  *Resp
)
{
  EFI_STATUS Status;
  QSPI_TRANSACTION_PACKET Packet;

  if ((Private == NULL) ||
      (Resp == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Packet.TxBuf = Cmd;
  Packet.RxBuf = Resp;
  Packet.TxLen = CmdSize;
  Packet.RxLen = sizeof (UINT8);
  Packet.WaitCycles = 0;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash register.\n", __FUNCTION__));
  }

  return Status;
}


/**
  Wait for Write Complete

  @param[in] Private               Driver's private data

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
WaitNorFlashWriteComplete (
  IN NOR_FLASH_PRIVATE_DATA *Private
)
{
  EFI_STATUS Status;
  UINT8 RegCmd;
  UINT8 Resp;
  UINT32 Count;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  RegCmd = NOR_READ_SR1;

  Count = 0;

  do {
    // Error out of retry count exceeds NOR_SR1_WEL_RETRY_CNT
    if (Count == NOR_SR1_WIP_RETRY_CNT) {
      return EFI_DEVICE_ERROR;
    }

    MicroSecondDelay (TIMEOUT);

    // Read WIP status
    Status = ReadNorFlashRegister (Private, &RegCmd, sizeof (RegCmd), &Resp);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash status 1 register.\n", __FUNCTION__));
      return Status;
    }

    Count++;
  } while ((Resp & NOR_SR1_WIP_BMSK) != 0);

  DEBUG ((EFI_D_INFO, "%a: NOR flash write complete.\n", __FUNCTION__));
  return Status;
}


/**
  Configure write enable latch

  @param[in] Private               Driver's private data
  @param[in] Enable                Enable or disable latching

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
ConfigureNorFlashWriteEnLatch (
  IN NOR_FLASH_PRIVATE_DATA *Private,
  IN BOOLEAN                Enable
)
{
  EFI_STATUS Status;
  UINT8 Cmd;
  UINT8 RegCmd;
  QSPI_TRANSACTION_PACKET Packet;
  UINT8 Resp;
  UINT8 Cmp;
  UINT32 Count;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Cmd = Enable ? NOR_WREN_ENABLE : NOR_WREN_DISABLE;
  Cmp = Enable ? NOR_SR1_WEL_BMSK : 0;

  Packet.TxBuf = &Cmd;
  Packet.RxBuf = NULL;
  Packet.TxLen = sizeof(Cmd);
  Packet.RxLen = 0;
  Packet.WaitCycles = 0;

  RegCmd = NOR_READ_SR1;

  Count = 0;

  do {
    // Error out of retry count exceeds NOR_SR1_WEL_RETRY_CNT
    if (Count == NOR_SR1_WEL_RETRY_CNT) {
      return EFI_DEVICE_ERROR;
    }

    // Configure WREN
    Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not program WREN latch.\n", __FUNCTION__));
      return Status;
    }

    MicroSecondDelay (TIMEOUT);

    // Read WREN status
    Status = ReadNorFlashRegister (Private, &RegCmd, sizeof (RegCmd), &Resp);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash status 1 register.\n", __FUNCTION__));
      return Status;
    }
    Count++;
  } while ((Resp & NOR_SR1_WEL_BMSK) != Cmp);

  DEBUG ((EFI_D_INFO, "%a: NOR flash WREN %s.\n", __FUNCTION__, Enable ? L"enabled" : L"disabled"));
  return Status;
}


/**
  Read NOR Flash's Device ID

  @param[in] Private               Driver's private data

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
ReadNorFlashDeviceID (
  IN NOR_FLASH_PRIVATE_DATA *Private
)
{
  EFI_STATUS Status;
  UINT8 Cmd;
  UINT8 DeviceID[NOR_READ_RDID_RESP_SIZE];
  QSPI_TRANSACTION_PACKET Packet;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Cmd = NOR_READ_RDID_CMD;
  ZeroMem (DeviceID, sizeof (DeviceID));

  Packet.TxBuf = &Cmd;
  Packet.RxBuf = DeviceID;
  Packet.TxLen = sizeof(Cmd);
  Packet.RxLen = sizeof(DeviceID);
  Packet.WaitCycles = 0;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash's device ID.\n", __FUNCTION__));
    return Status;
  }
  // Match the read Device ID with what is in Flash Attributes table.
  if ((FlashAttributes[Private->FlashInstance].ManufacturerId != DeviceID[NOR_RDID_MANU_ID_OFFSET]) ||
      (FlashAttributes[Private->FlashInstance].MemoryInterfaceType != DeviceID[NOR_RDID_MEM_INTF_TYPE_OFFSET]) ||
      (FlashAttributes[Private->FlashInstance].MemoryDensity != DeviceID[NOR_RDID_MEM_DENSITY_OFFSET])) {
    return EFI_DEVICE_ERROR;
  }
  DEBUG ((EFI_D_INFO, "%a: Device ID: 0x%x 0x%x 0x%x\n", __FUNCTION__, DeviceID[NOR_RDID_MANU_ID_OFFSET],
          DeviceID[NOR_RDID_MEM_INTF_TYPE_OFFSET], DeviceID[NOR_RDID_MEM_DENSITY_OFFSET]));

  return Status;
}


/**
  Update NOR Flash's parameters

  @param[in] Private               Driver's private data

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
UpdateNorFlashParameters (
  IN NOR_FLASH_PRIVATE_DATA *Private
)
{
  EFI_STATUS Status;
  UINT32     CmdSize;
  UINT32     Offset;
  UINT32     Count;
  UINT32     AddressShift;
  UINT8      Resp;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CmdSize = NOR_CMD_SIZE + NOR_REG_OFFSET_SIZE;
  ZeroMem (Private->CommandBuffer, CmdSize);

  // Create command for reading CR3V
  Offset = NOR_CR3V_REG_OFFSET;
  AddressShift = 0;
  for (Count = (CmdSize - 1); Count > 0; Count--) {
    Private->CommandBuffer[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
    AddressShift += 8;
  }
  Private->CommandBuffer[0] = NOR_READ_ANY_REG;

  // Read CR3V status
  Status = ReadNorFlashRegister (Private, Private->CommandBuffer, CmdSize, &Resp);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash CR3V register.\n", __FUNCTION__));
    goto ErrorExit;
  }

  if ((Resp & NOR_CR3V_PAGE_BUF_WRAP_BMSK) == NOR_CR3V_PAGE_BUF_WRAP_BMSK) {
    FlashAttributes[Private->FlashInstance].PageSize *= 2;
  }

  if ((Resp & NOR_CR3V_BLK_SIZE_ERASE_BMSK) == NOR_CR3V_BLK_SIZE_ERASE_BMSK) {
    FlashAttributes[Private->FlashInstance].BlockSize = SIZE_256KB;
  }

  DEBUG ((EFI_D_INFO, "%a: NOR flash parameters updated.\n", __FUNCTION__));

ErrorExit:

  return Status;
}


/**
  Get NOR Flash Attributes.

  @param[in]  This                  Instance to protocol
  @param[out] Attributes            Pointer to flash attributes

  @retval EFI_SUCCESS               Operation successful.
  @retval others                    Error occurred

**/
EFI_STATUS
NorFlashGetAttributes(
  IN  NVIDIA_NOR_FLASH_PROTOCOL *This,
  OUT NOR_FLASH_ATTRIBUTES      *Attributes
)
{
  NOR_FLASH_PRIVATE_DATA  *Private;

  if (This == NULL ||
      Attributes == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  CopyMem (Attributes, &FlashAttributes[Private->FlashInstance], sizeof (NOR_FLASH_ATTRIBUTES));

  return EFI_SUCCESS;
}


/**
  Read data from NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to read from
  @param[in] Size                  Number of bytes to be read
  @param[in] Buffer                Address to read data into

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashRead(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Offset,
  IN UINT32                    Size,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  UINT32                  CmdSize;
  UINT32                  Count;
  UINT32                  AddressShift;
  QSPI_TRANSACTION_PACKET Packet;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  FlashDensity;

  if (This == NULL ||
      Buffer == NULL ||
      Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  // Validate that read start and end offsets are within range.
  FlashDensity = FlashAttributes[Private->FlashInstance].NumSectors *
                 FlashAttributes[Private->FlashInstance].SectorSize;
  if ((Offset > (FlashDensity - 1)) ||
      ((Offset + Size) > (FlashDensity))) {
    return EFI_INVALID_PARAMETER;
  }

  CmdSize = NOR_CMD_SIZE + NOR_ADDR_SIZE;
  ZeroMem (Private->CommandBuffer, CmdSize);

  AddressShift = 0;
  for (Count = (CmdSize - 1); Count > 0; Count--) {
    Private->CommandBuffer[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
    AddressShift += 8;
  }
  Private->CommandBuffer[0] = NOR_READ_DATA_CMD;

  Packet.TxBuf = Private->CommandBuffer;
  Packet.TxLen = CmdSize;
  Packet.RxBuf = Buffer;
  Packet.RxLen = Size;
  Packet.WaitCycles = 0;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read data from NOR flash.\n", __FUNCTION__));
    goto ErrorExit;
  }

  DEBUG ((EFI_D_INFO, "%a: Successfully read data from NOR flash.\n", __FUNCTION__));

ErrorExit:

  return Status;
}


/**
  Read data from NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] MediaId               Media ID for the device
  @param[in] Lba                   Logical block to start reading from
  @param[in] BufferSize            Number of bytes to be read
  @param[in] Buffer                Address to read data into

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashReadBlock(
  IN EFI_BLOCK_IO_PROTOCOL     *This,
  IN UINT32                    MediaId,
  IN EFI_LBA                   Lba,
  IN UINTN                     BufferSize,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;

  if (This == NULL ||
      Buffer == NULL ||
      BufferSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_BLOCK_IO_PROTOCOL(This);

  if (MediaId != Private->FlashInstance) {
    return EFI_MEDIA_CHANGED;
  }

  Status = NorFlashRead (&Private->NorFlashProtocol,
                         (Lba * FlashAttributes[MediaId].BlockSize),
                         BufferSize,
                         Buffer);

  return Status;
}


/**
  Erase data from NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Logical block to start erasing from
  @param[in] NumLba                Number of block to be erased

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashErase(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Lba,
  IN UINT32                    NumLba
)
{
  EFI_STATUS              Status;
  UINT32                  CmdSize;
  UINT32                  Count;
  UINT32                  Block;
  UINT32                  AddressShift;
  QSPI_TRANSACTION_PACKET Packet;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  Offset;
  UINT32                  LastBlock;

  if (This == NULL ||
      NumLba == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  LastBlock = ((FlashAttributes[Private->FlashInstance].NumSectors * FlashAttributes[Private->FlashInstance].SectorSize) / FlashAttributes[Private->FlashInstance].BlockSize) - 1;

  if ((Lba > LastBlock) ||
      ((Lba + NumLba - 1) > LastBlock)) {
    return EFI_INVALID_PARAMETER;
  }

  CmdSize = NOR_CMD_SIZE + NOR_ADDR_SIZE;
  ZeroMem (Private->CommandBuffer, CmdSize);

  for (Block = Lba; Block < (Lba + NumLba); Block++) {
    Status = ConfigureNorFlashWriteEnLatch (Private, TRUE);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not enable NOR flash WREN.\n", __FUNCTION__));
      goto ErrorExit;
    }

    AddressShift = 0;
    Offset = Block * FlashAttributes[Private->FlashInstance].BlockSize;
    for (Count = (CmdSize - 1); Count > 0; Count--) {
      Private->CommandBuffer[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
      AddressShift += 8;
    }
    Private->CommandBuffer[0] = NOR_ERASE_DATA_CMD;

    Packet.TxBuf = Private->CommandBuffer;
    Packet.TxLen = CmdSize;
    Packet.RxBuf = NULL;
    Packet.RxLen = 0;
    Packet.WaitCycles = 0;

    Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not erase data from NOR flash.\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = WaitNorFlashWriteComplete (Private);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not complete NOR flash write.\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = ConfigureNorFlashWriteEnLatch (Private, FALSE);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not enable NOR flash WREN.\n", __FUNCTION__));
      goto ErrorExit;
    }
  }

  DEBUG ((EFI_D_INFO, "%a: Successfully erased data from NOR flash.\n", __FUNCTION__));

ErrorExit:

  return Status;
}


/**
  Erase data from NOR Flash.

  @param[in]       This            Instance to protocol
  @param[in]       MediaId         Media ID for the device
  @param[in]       LBA             Logical block to start erasing from
  @param[in, out]  Token           A pointer to the token associated with the
                                   transaction.
  @param[in]       Size            Number of bytes to be erased


  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashEraseBlock(
  IN     EFI_ERASE_BLOCK_PROTOCOL *This,
  IN     UINT32                   MediaId,
  IN     EFI_LBA                  LBA,
  IN OUT EFI_ERASE_BLOCK_TOKEN    *Token,
  IN     UINTN                    Size
)
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;

  if (This == NULL ||
      Token == NULL ||
      Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_ERASE_BLOCK_PROTOCOL(This);

  if (MediaId != Private->FlashInstance) {
    return EFI_MEDIA_CHANGED;
  }

  Status = NorFlashErase (&Private->NorFlashProtocol,
                          LBA,
                          Size / FlashAttributes[MediaId].BlockSize);

  if (Token->Event != NULL) {
    Token->TransactionStatus = Status;
    Status = EFI_SUCCESS;
    gBS->SignalEvent (Token->Event);
  }

  return Status;
}


/**
  Write single page data to NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to write to
  @param[in] Size                  Number of bytes to write
  @param[in] Buffer                Address to write data from

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashWriteSinglePage(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Offset,
  IN UINT32                    Size,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  UINT32                  CmdSize;
  UINT32                  Count;
  UINT32                  AddressShift;
  QSPI_TRANSACTION_PACKET Packet;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  FlashDensity;

  if (This == NULL ||
      Buffer == NULL ||
      Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  FlashDensity = FlashAttributes[Private->FlashInstance].NumSectors *
                 FlashAttributes[Private->FlashInstance].SectorSize;
  if ((Offset > (FlashDensity - 1)) ||
      ((Offset + Size) > (FlashDensity))) {
    return EFI_INVALID_PARAMETER;
  }

  CmdSize = NOR_CMD_SIZE + NOR_ADDR_SIZE;
  ZeroMem (Private->CommandBuffer, CmdSize + Size);
  Status = ConfigureNorFlashWriteEnLatch (Private, TRUE);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not enable NOR flash WREN.\n", __FUNCTION__));
    goto ErrorExit;
  }

  CopyMem (&Private->CommandBuffer[CmdSize], Buffer, Size);
  AddressShift = 0;
  for (Count = (CmdSize - 1); Count > 0; Count--) {
    Private->CommandBuffer[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
    AddressShift += 8;
  }
  Private->CommandBuffer[0] = NOR_WRITE_DATA_CMD;

  Packet.TxBuf = Private->CommandBuffer;
  Packet.TxLen = CmdSize + Size;
  Packet.RxBuf = NULL;
  Packet.RxLen = 0;
  Packet.WaitCycles = 0;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not write data to NOR flash.\n", __FUNCTION__));
    goto ErrorExit;
  }

  Status = WaitNorFlashWriteComplete (Private);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not complete NOR flash write.\n", __FUNCTION__));
    goto ErrorExit;
  }

  Status = ConfigureNorFlashWriteEnLatch (Private, FALSE);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not disable NOR flash WREN.\n", __FUNCTION__));
    goto ErrorExit;
  }

  DEBUG ((EFI_D_INFO, "%a: Successfully wrote data to NOR flash.\n", __FUNCTION__));

ErrorExit:

  return Status;
}


/**
  Write data to NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to write to
  @param[in] Size                  Number of bytes to write
  @param[in] Buffer                Address to write data from

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashWrite(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Offset,
  IN UINT32                    Size,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  FlashDensity;
  UINT32                  PageSize;
  UINT32                  BytesToWrite;

  if (This == NULL ||
      Buffer == NULL ||
      Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  FlashDensity = FlashAttributes[Private->FlashInstance].NumSectors *
                 FlashAttributes[Private->FlashInstance].SectorSize;
  if ((Offset > (FlashDensity - 1)) ||
      ((Offset + Size) > (FlashDensity))) {
    return EFI_INVALID_PARAMETER;
  }

  // Writes need to be confined in a page.
  PageSize = FlashAttributes[Private->FlashInstance].PageSize;
  while (Size > 0) {
    // Calculate offset and size within the page
    BytesToWrite = PageSize - (Offset & (PageSize - 1));
    if (BytesToWrite > Size) {
      BytesToWrite = Size;
    }
    Status = NorFlashWriteSinglePage (This, Offset, BytesToWrite, Buffer);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not write data to NOR flash.\n", __FUNCTION__));
      return Status;
    }
    Buffer = (UINT8 *)Buffer + BytesToWrite;
    Offset += BytesToWrite;
    Size -= BytesToWrite;
  }

  DEBUG ((EFI_D_INFO, "%a: Successfully wrote data to NOR flash.\n", __FUNCTION__));

  return Status;
}


/**
  Write data to NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] MediaId               Media ID for the device
  @param[in] Lba                   Logical block to start writing from
  @param[in] BufferSize            Number of bytes to be written
  @param[in] Buffer                Address to write data from

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashWriteBlock(
  IN EFI_BLOCK_IO_PROTOCOL     *This,
  IN UINT32                    MediaId,
  IN EFI_LBA                   Lba,
  IN UINTN                     BufferSize,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  StartPage;
  UINT32                  NumPages;
  UINT32                  PageSize;
  UINT32                  BlockSize;
  UINT8                   *Data;

  if (This == NULL ||
      Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_BLOCK_IO_PROTOCOL(This);

  if (MediaId != Private->FlashInstance) {
    return EFI_MEDIA_CHANGED;
  }

  Status = NorFlashErase (&Private->NorFlashProtocol,
                          Lba,
                          BufferSize / FlashAttributes[MediaId].BlockSize);

  BlockSize = FlashAttributes[MediaId].BlockSize;
  PageSize = FlashAttributes[MediaId].PageSize;
  StartPage = (BlockSize / PageSize) * Lba;
  NumPages = BufferSize / PageSize;

  Data = Buffer;
  while (NumPages > 0) {
    Status = NorFlashWriteSinglePage (&Private->NorFlashProtocol,
                                      StartPage *  BufferSize / FlashAttributes[MediaId].PageSize,
                                      PageSize,
                                      Data);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    StartPage++;
    NumPages--;
    Data += PageSize;
  }

  return Status;
}


/**
  Check for compatible flash part in device tree.

  Looks through all subnodes of the QSPI node to see if any of them has
  compatibility string that matches flash name.

  @param[in]   Controller          The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[out]  Instance            Pointer to return valid flash's instance.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
CheckNorFlashCompatibility(
  IN EFI_HANDLE                Controller,
  IN UINT32                    *Instance
)
{
  EFI_STATUS                       Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode;
  INT32                            NorFlashNodeOffset;
  BOOLEAN                          NorFlashFound;
  NOR_FLASH_ATTRIBUTES             *FlashAttributesPtr;
  UINT32                           FlashInstance;

  if (Instance == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Check whether device tree node protocol is available.
  DeviceTreeNode = NULL;
  Status = gBS->HandleProtocol (Controller,
                                &gNVIDIADeviceTreeNodeProtocolGuid,
                                (VOID **)&DeviceTreeNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FlashAttributesPtr = FlashAttributes;
  FlashInstance = 0;
  while (FlashAttributesPtr->FlashName != NULL) {
    // Check for SPI flash compatibility name in all QSPI subnodes.
    NorFlashFound = FALSE;
    fdt_for_each_subnode (NorFlashNodeOffset, DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset) {
      if (fdt_node_check_compatible (DeviceTreeNode->DeviceTreeBase,
                                     NorFlashNodeOffset,
                                     FlashAttributesPtr->FlashName) == 0) {
        DEBUG ((DEBUG_INFO, "%a: Supported NOR flash found.\n", __FUNCTION__));
        NorFlashFound = TRUE;
        break;
      }
    }
    if (NorFlashFound) {
      break;
    }
    FlashInstance++;
    FlashAttributesPtr++;
  }

  if (NorFlashFound) {
    *Instance = FlashInstance;
    return EFI_SUCCESS;
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
  Fixup internal data so that EFI can be call in virtual mode.
  Call the passed in Child Notify event and convert any pointers in
  lib to virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
NorVirtualNotifyEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  NOR_FLASH_PRIVATE_DATA *Private;

  Private = (NOR_FLASH_PRIVATE_DATA *)Context;
  EfiConvertPointer (0x0, (VOID**)&Private->QspiController->PerformTransaction);
  EfiConvertPointer (0x0, (VOID**)&Private->QspiController);
  return;
}

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Since ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN EFI_HANDLE                    Controller,
  IN EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath
)
{
  EFI_STATUS                       Status;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiInstance;
  UINT32                           Instance;

  // Check whether driver has already been started.
  QspiInstance = NULL;
  Status = gBS->OpenProtocol (Controller,
                              &gNVIDIAQspiControllerProtocolGuid,
                              (VOID**) &QspiInstance,
                              This->DriverBindingHandle,
                              Controller,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = gBS->CloseProtocol (Controller,
                               &gNVIDIAQspiControllerProtocolGuid,
                               This->DriverBindingHandle,
                               Controller);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return CheckNorFlashCompatibility (Controller, &Instance);
}


/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN EFI_HANDLE                    Controller,
  IN EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath
)
{
  EFI_STATUS                       Status;
  NOR_FLASH_PRIVATE_DATA           *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiInstance;
  UINT32                           Instance;
  EFI_DEVICE_PATH_PROTOCOL         *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL         *NorFlashDevicePath;
  VOID                             *Interface;

  // Open Qspi Controller Protocol
  QspiInstance = NULL;
  Status = gBS->OpenProtocol (Controller,
                              &gNVIDIAQspiControllerProtocolGuid,
                              (VOID**) &QspiInstance,
                              This->DriverBindingHandle,
                              Controller,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to open QSPI Protocol\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Allocate Private Data
  Private = AllocateRuntimeZeroPool (sizeof (NOR_FLASH_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Private->Signature = NOR_FLASH_SIGNATURE;
  Private->QspiControllerHandle = Controller;
  Private->QspiController = QspiInstance;

  Status = CheckNorFlashCompatibility (Controller, &Instance);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }
  Private->FlashInstance = Instance;

  // Read NOR flash's device ID
  Status = ReadNorFlashDeviceID (Private);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Update NOR flash's parameters
  Status = UpdateNorFlashParameters (Private);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Get Parent's device path.
  Status = gBS->HandleProtocol (Controller,
                                &gEfiDevicePathProtocolGuid,
                                (VOID **)&ParentDevicePath);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to get parent's device path\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Append Vendor device path to parent device path.
  NorFlashDevicePath = AppendDevicePathNode (ParentDevicePath,
                                             (EFI_DEVICE_PATH_PROTOCOL *)&VendorDevicePath);
  if (NorFlashDevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }
  Private->ParentDevicePath = ParentDevicePath;
  Private->NorFlashDevicePath = NorFlashDevicePath;

  // Install Protocols
  Private->NorFlashProtocol.FvbAttributes = EFI_FVB2_READ_ENABLED_CAP |
                                            EFI_FVB2_READ_STATUS |
                                            EFI_FVB2_STICKY_WRITE |
                                            EFI_FVB2_ERASE_POLARITY |
                                            EFI_FVB2_WRITE_STATUS |
                                            EFI_FVB2_WRITE_ENABLED_CAP;
  Private->NorFlashProtocol.GetAttributes = NorFlashGetAttributes;
  Private->NorFlashProtocol.Read = NorFlashRead;
  Private->NorFlashProtocol.Write = NorFlashWrite;
  Private->NorFlashProtocol.Erase = NorFlashErase;

  Media.MediaId = Private->FlashInstance;
  Media.BlockSize = FlashAttributes[Private->FlashInstance].BlockSize;
  Media.LastBlock = ((FlashAttributes[Private->FlashInstance].SectorSize *
                      FlashAttributes[Private->FlashInstance].NumSectors) /
                     FlashAttributes[Private->FlashInstance].BlockSize) - 1;
  Private->BlockIoProtocol.Reset = NULL;
  Private->BlockIoProtocol.ReadBlocks = NorFlashReadBlock;
  Private->BlockIoProtocol.WriteBlocks = NorFlashWriteBlock;
  Private->BlockIoProtocol.FlushBlocks = NULL;
  Private->BlockIoProtocol.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION;
  Private->BlockIoProtocol.Media = &Media;

  Private->EraseBlockProtocol.Revision = EFI_ERASE_BLOCK_PROTOCOL_REVISION;
  Private->EraseBlockProtocol.EraseLengthGranularity = 1;
  Private->EraseBlockProtocol.EraseBlocks = NorFlashEraseBlock;

  Status = gBS->InstallMultipleProtocolInterfaces (&Private->NorFlashHandle,
                                                   &gNVIDIANorFlashProtocolGuid,
                                                   &Private->NorFlashProtocol,
                                                   &gEfiDevicePathProtocolGuid,
                                                   Private->NorFlashDevicePath,
                                                   &gEfiBlockIoProtocolGuid,
                                                   &Private->BlockIoProtocol,
                                                   &gEfiEraseBlockProtocolGuid,
                                                   &Private->EraseBlockProtocol,
                                                   NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install NOR flash protocols\n", __FUNCTION__));
    goto ErrorExit;
  }
  Private->ProtocolsInstalled = TRUE;

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_NOTIFY,
                               NorVirtualNotifyEvent,
                               Private,
                               &gEfiEventVirtualAddressChangeGuid,
                               &Private->VirtualAddrChangeEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to create virtual address callback event\r\n",__FUNCTION__));
    goto ErrorExit;
  }
  // Open caller ID protocol for child
  Status = gBS->InstallMultipleProtocolInterfaces (&Controller,
                                                   &gEfiCallerIdGuid,
                                                   NULL,
                                                   NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to install callerid protocol\n",__FUNCTION__));
    goto ErrorExit;
  }
  Status = gBS->OpenProtocol (Controller,
                              &gEfiCallerIdGuid,
                              (VOID **)&Interface,
                              This->DriverBindingHandle,
                              Private->NorFlashHandle,
                              EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to open caller ID protocol\n", __FUNCTION__));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      gBS->CloseProtocol (Controller,
                          &gEfiCallerIdGuid,
                          This->DriverBindingHandle,
                          Private->NorFlashHandle);
      gBS->UninstallMultipleProtocolInterfaces (Controller,
                                                &gEfiCallerIdGuid,
                                                NULL,
                                                NULL);
      gBS->CloseEvent (Private->VirtualAddrChangeEvent);
      if (Private->ProtocolsInstalled) {
        gBS->UninstallMultipleProtocolInterfaces (Private->NorFlashHandle,
                                                  &gNVIDIANorFlashProtocolGuid,
                                                  &Private->NorFlashProtocol,
                                                  &gEfiDevicePathProtocolGuid,
                                                  Private->NorFlashDevicePath,
                                                  &gEfiBlockIoProtocolGuid,
                                                  &Private->BlockIoProtocol,
                                                  &gEfiEraseBlockProtocolGuid,
                                                  &Private->EraseBlockProtocol,
                                                  NULL);
      }
      if (Private->NorFlashDevicePath != NULL) {
        FreePool (Private->NorFlashDevicePath);
      }
      FreePool (Private);
    }
    gBS->CloseProtocol (Controller,
                        &gNVIDIAQspiControllerProtocolGuid,
                        This->DriverBindingHandle,
                        Controller);
  }
  return Status;
}


/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      Controller,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
)
{
  EFI_STATUS             Status;
  NOR_FLASH_PRIVATE_DATA *Private;
  UINT32                 Index;

  for (Index = 0; Index < NumberOfChildren; Index++) {
    Private = NULL;
    Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);
    if (Private == NULL) {
      return EFI_DEVICE_ERROR;
    }
    Status = gBS->CloseProtocol (Controller,
                                 &gEfiCallerIdGuid,
                                 This->DriverBindingHandle,
                                 ChildHandleBuffer[Index]);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
    gBS->CloseEvent (Private->VirtualAddrChangeEvent);
    if (Private->ProtocolsInstalled) {
      Status = gBS->UninstallMultipleProtocolInterfaces (ChildHandleBuffer[Index],
                                                         &gNVIDIANorFlashProtocolGuid,
                                                         &Private->NorFlashProtocol,
                                                         &gEfiDevicePathProtocolGuid,
                                                         Private->NorFlashDevicePath,
                                                         &gEfiBlockIoProtocolGuid,
                                                         &Private->BlockIoProtocol,
                                                         &gEfiEraseBlockProtocolGuid,
                                                         &Private->EraseBlockProtocol,
                                                         NULL);
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }
    }
    if (Private->NorFlashDevicePath != NULL) {
      FreePool (Private->NorFlashDevicePath);
    }
    FreePool (Private);
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (Controller,
                                                     &gEfiCallerIdGuid,
                                                     NULL,
                                                     NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = gBS->CloseProtocol (Controller,
                               &gNVIDIAQspiControllerProtocolGuid,
                               This->DriverBindingHandle,
                               Controller);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}


EFI_DRIVER_BINDING_PROTOCOL gNorFlashDxeDriverBinding = {
  NorFlashDxeDriverBindingSupported,
  NorFlashDxeDriverBindingStart,
  NorFlashDxeDriverBindingStop,
  0x1,
  NULL,
  NULL
};


/**
  The user Entry Point for module NorFlashDxe. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
InitializeNorFlashDxe (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
)
{
  // TODO: Add component name support.
  return EfiLibInstallDriverBinding (ImageHandle,
                                     SystemTable,
                                     &gNorFlashDxeDriverBinding,
                                     ImageHandle);
}
