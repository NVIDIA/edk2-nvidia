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

#define MM_DICE_READ                     (1)
#define MM_DICE_WRITE                    (2)
#define MM_DICE_LOCK                     (3)
#define MM_DICE_CHECK_LOCK_STATUS        (4)
#define MM_COMMUNICATE_DICE_HEADER_SIZE  (OFFSET_OF (MM_COMMUNICATE_DICE_HEADER, Data))

typedef struct {
  UINTN         Function;
  EFI_STATUS    ReturnStatus;
  UINT8         Data[1];
} MM_COMMUNICATE_DICE_HEADER;

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

  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DiceHeader = (MM_COMMUNICATE_DICE_HEADER *)CommBuffer;
  if (*CommBufferSize < MM_COMMUNICATE_DICE_HEADER_SIZE) {
    DEBUG ((DEBUG_ERROR, "Communication buffer is too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  DiceHeader->ReturnStatus = EFI_SUCCESS;
  switch (DiceHeader->Function) {
    case MM_DICE_READ:
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

  Status = GetQspiDeviceRegion (&QspiBaseAddress, &QspiSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Qspi MMIO region not found (%r)\n", __FUNCTION__, Status));
    goto exit;
  }

  if (IsNorFlashDeviceSupported () == FALSE) {
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
  return Status;
}
