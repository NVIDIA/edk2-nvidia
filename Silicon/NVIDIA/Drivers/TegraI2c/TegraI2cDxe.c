/** @file

  Tegra I2c Driver

  Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/Crc8Lib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>
#include <Protocol/DeviceTreeNode.h>
#include <Protocol/PinControl.h>

#include <Library/DeviceDiscoveryDriverLib.h>
#include "TegraI2c.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra194-i2c", &gNVIDIANonDiscoverableI2cDeviceGuid },
  { "nvidia,tegra234-i2c", &gNVIDIANonDiscoverableI2cDeviceGuid },
  { NULL,                  NULL                                 }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA Tegra I2C controller driver",
  .UseDriverBinding                = FALSE,
  .AutoEnableClocks                = TRUE,
  .AutoResetModule                 = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .DirectEnumerationSupport        = TRUE,
};

STATIC
CONTROLLER_DEVICE_PATH  ControllerNode = {
  {
    HARDWARE_DEVICE_PATH, HW_CONTROLLER_DP,
    { (UINT8)(sizeof (CONTROLLER_DEVICE_PATH)), (UINT8)((sizeof (CONTROLLER_DEVICE_PATH)) >> 8) }
  },
  0
};

/**
  Transfers the register settings from shadow registers to actual controller registers.

  Config load register is used to transfer the SW programmed configuration in I2C registers to
  HW internal registers that would be used in actual logic. It has MSTR_CONFIG_LOAD bit field for
  I2C master and Bus clear logic.

  @param[in] Private            Pointer to an NVIDIA_TEGRA_I2C_PRIVATE_DATA structure

  @retval EFI_SUCCESS           The configuration was set successfully.
  @retval EFI_TIMEOUT           Timeout setting configuration.

**/
STATIC
EFI_STATUS
TegraI2cLoadConfiguration (
  IN NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private
  )
{
  UINT32  Data32;
  UINTN   Timeout = I2C_I2C_CONFIG_LOAD_0_TIMEOUT*1000;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!Private->ConfigurationChanged) {
    return EFI_SUCCESS;
  }

  Private->ConfigurationChanged = FALSE;

  Data32 = I2C_I2C_CONFIG_LOAD_0_MSTR_CONFIG_LOAD;
  MmioWrite32 (Private->BaseAddress + I2C_I2C_CONFIG_LOAD_0_OFFSET, Data32);

  do {
    MicroSecondDelay (1);
    Data32 = MmioRead32 (Private->BaseAddress + I2C_I2C_CONFIG_LOAD_0_OFFSET);
    Timeout--;
    if (Timeout == 0) {
      DEBUG ((DEBUG_ERROR, "%a: Configuration load timeout %x\r\n", __FUNCTION__, Data32));
      return EFI_TIMEOUT;
    }
  } while (Data32 != 0);

  return EFI_SUCCESS;
}

/**
  Set the frequency for the I2C clock line.

  This routine must be called at or below TPL_NOTIFY.

  The software and controller do a best case effort of using the specified
  frequency for the I2C bus.  If the frequency does not match exactly then
  the I2C master protocol selects the next lower frequency to avoid
  exceeding the operating conditions for any of the I2C devices on the bus.
  For example if 400 KHz was specified and the controller's divide network
  only supports 402 KHz or 398 KHz then the I2C master protocol selects 398
  KHz.  If there are not lower frequencies available, then return
  EFI_UNSUPPORTED.

  @param[in] This           Pointer to an EFI_I2C_MASTER_PROTOCOL structure
  @param[in] BusClockHertz  Pointer to the requested I2C bus clock frequency
                            in Hertz.  Upon return this value contains the
                            actual frequency in use by the I2C controller.

  @retval EFI_SUCCESS           The bus frequency was set successfully.
  @retval EFI_ALREADY_STARTED   The controller is busy with another transaction.
  @retval EFI_INVALID_PARAMETER BusClockHertz is NULL
  @retval EFI_UNSUPPORTED       The controller does not support this frequency.

**/
EFI_STATUS
TegraI2cSetBusFrequency (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This,
  IN OUT UINTN                      *BusClockHertz
  )
{
  NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private = NULL;
  UINT32                         Data32;
  EFI_STATUS                     Status;
  UINT8                          TLow;
  UINT8                          THigh;
  UINT32                         ClockDivisor;
  UINT32                         ClockMultiplier;

  if ((This == NULL) ||
      (BusClockHertz == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private = TEGRA_I2C_PRIVATE_DATA_FROM_MASTER (This);
  // Load relevent prod settings
  Status = DeviceDiscoverySetProd (Private->ControllerHandle, Private->DeviceTreeNode, "prod");
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set prod settings (%r)\r\n", __FUNCTION__, Status));
    return Status;
  }

  if (*BusClockHertz >= HS_SPEED) {
    Status = DeviceDiscoverySetProd (Private->ControllerHandle, Private->DeviceTreeNode, "prod_c_hs");
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set HS prod settings (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
  } else if (*BusClockHertz >= FM_PLUS_SPEED) {
    Status = DeviceDiscoverySetProd (Private->ControllerHandle, Private->DeviceTreeNode, "prod_c_fmplus");
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set FM+ prod settings (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
  } else if (*BusClockHertz >= FM_SPEED) {
    Status = DeviceDiscoverySetProd (Private->ControllerHandle, Private->DeviceTreeNode, "prod_c_fm");
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set FM prod settings (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
  } else {
    Status = DeviceDiscoverySetProd (Private->ControllerHandle, Private->DeviceTreeNode, "prod_c_sm");
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set SM prod settings (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  if (*BusClockHertz < HS_SPEED) {
    Private->HighSpeed = FALSE;
    Data32             = MmioRead32 (Private->BaseAddress + I2C_I2C_INTERFACE_TIMING_0_OFFSET);
    TLow               = (Data32 & I2C_I2C_INTERFACE_TIMING_0_TLOW_MASK) >> I2C_I2C_INTERFACE_TIMING_0_TLOW_SHIFT;
    THigh              = (Data32 & I2C_I2C_INTERFACE_TIMING_0_THIGH_MASK) >> I2C_I2C_INTERFACE_TIMING_0_THIGH_SHIFT;
    Data32             = MmioRead32 (Private->BaseAddress + I2C_I2C_CLK_DIVISOR_REGISTER_0_OFFSET);
    ClockDivisor       = (Data32 & I2C_CLK_DIVISOR_STD_FAST_MODE_MASK) >> I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT;
  } else {
    Private->HighSpeed = TRUE;
    Data32             = MmioRead32 (Private->BaseAddress + I2C_I2C_HS_INTERFACE_TIMING_0_OFFSET);
    TLow               = (Data32 & I2C_I2C_HS_INTERFACE_TIMING_0_TLOW_MASK) >> I2C_I2C_HS_INTERFACE_TIMING_0_TLOW_SHIFT;
    THigh              = (Data32 & I2C_I2C_HS_INTERFACE_TIMING_0_THIGH_MASK) >> I2C_I2C_HS_INTERFACE_TIMING_0_THIGH_SHIFT;
    Data32             = MmioRead32 (Private->BaseAddress + I2C_I2C_CLK_DIVISOR_REGISTER_0_OFFSET);
    ClockDivisor       = (Data32 & I2C_CLK_DIVISOR_HSMODE_MASK) >> I2C_CLK_DIVISOR_HSMODE_SHIFT;
  }

  ClockMultiplier = (TLow + THigh + 2) * (ClockDivisor + 1);

  Status = DeviceDiscoverySetClockFreq (Private->ControllerHandle, "div-clk", *BusClockHertz * ClockMultiplier);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, failed to set clock frequency to %lldHz (%r)\r\n", __FUNCTION__, *BusClockHertz * ClockMultiplier, Status));
    return Status;
  }

  Private->ConfigurationChanged = TRUE;
  Status                        = TegraI2cLoadConfiguration (Private);

  return Status;
}

/**
  Reset the I2C controller and configure it for use

  This routine must be called at or below TPL_NOTIFY.

  The I2C controller is reset.  The caller must call SetBusFrequench() after
  calling Reset().

  @param[in]     This       Pointer to an EFI_I2C_MASTER_PROTOCOL structure.

  @retval EFI_SUCCESS         The reset completed successfully.
  @retval EFI_ALREADY_STARTED The controller is busy with another transaction.
  @retval EFI_DEVICE_ERROR    The reset operation failed.

**/
EFI_STATUS
TegraI2cReset (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This
  )
{
  NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private = NULL;
  UINT32                         Data32;
  UINTN                          Timeout;
  EFI_STATUS                     Status;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = TEGRA_I2C_PRIVATE_DATA_FROM_MASTER (This);

  MmioWrite32 (Private->BaseAddress + I2C_I2C_MASTER_RESET_CNTRL_0_OFFSET, I2C_I2C_MASTER_RESET_CNTRL_0_SOFT_RESET);
  MicroSecondDelay (I2C_SOFT_RESET_DELAY);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_MASTER_RESET_CNTRL_0_OFFSET, 0);

  Timeout = I2C_TIMEOUT;
  Data32  = BC_TERMINATE_IMMEDIATE;
  MmioWrite32 (Private->BaseAddress + I2C_I2C_BUS_CLEAR_CONFIG_0_OFFSET, Data32);
  Status = TegraI2cLoadConfiguration (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to update configuration (%r)\r\n", __FUNCTION__, Status));
    return Status;
  }

  Data32 |= BC_ENABLE;
  MmioWrite32 (Private->BaseAddress + I2C_I2C_BUS_CLEAR_CONFIG_0_OFFSET, Data32);
  Data32 = MmioRead32 (Private->BaseAddress + I2C_I2C_BUS_CLEAR_CONFIG_0_OFFSET);
  while ((Data32 & BC_ENABLE) != 0) {
    MicroSecondDelay (1);
    Timeout--;
    if (Timeout == 0) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to clear bus\r\n", __FUNCTION__));
      return EFI_TIMEOUT;
    }

    Data32 = MmioRead32 (Private->BaseAddress + I2C_I2C_BUS_CLEAR_CONFIG_0_OFFSET);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
TegraI2cSendHeader (
  IN NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private,
  IN UINTN                          SlaveAddress,
  IN UINT32                         PayloadSize,
  IN BOOLEAN                        ReadOperation,
  IN BOOLEAN                        LastOperation,
  IN BOOLEAN                        ContinueTransfer
  )
{
  UINT32      PacketHeader[3];
  EFI_STATUS  Status;
  UINT32      Data32;
  UINT32      Timeout;

  if (PayloadSize > MAX_UINT16) {
    return EFI_INVALID_PARAMETER;
  }

  PacketHeader[0] = (0 << PACKET_HEADER0_HEADER_SIZE_SHIFT) |
                    PACKET_HEADER0_PROTOCOL_I2C |
                    (Private->ControllerId << PACKET_HEADER0_CONTROLLER_ID_SHIFT) |
                    (Private->PacketId << PACKET_HEADER0_PACKET_ID_SHIFT);
  Private->PacketId++;

  if (PayloadSize > 0) {
    PacketHeader[1] = PayloadSize - 1;
  } else {
    PacketHeader[1] = 0;
  }

  PacketHeader[2] = I2C_HEADER_IE_ENABLE;

  if (Private->HighSpeed) {
    PacketHeader[2] |= I2C_HEADER_HIGHSPEED_MODE;
  }

  if (ReadOperation) {
    PacketHeader[2] |= I2C_HEADER_READ;
    PacketHeader[2] |= BIT0;
  }

  if ((SlaveAddress & I2C_ADDRESSING_10_BIT) != 0) {
    PacketHeader[2] |= I2C_HEADER_10BIT_ADDR;
  }

  if (!LastOperation) {
    PacketHeader[2] |= I2C_HEADER_REPEAT_START;
  }

  if (ContinueTransfer) {
    PacketHeader[2] |= I2C_HEADER_CONTINUE_XFER;
  }

  PacketHeader[2] |= ((SlaveAddress << I2C_HEADER_SLAVE_ADDR_SHIFT) & I2C_HEADER_SLAVE_ADDR_MASK);
  MmioWrite32 (Private->BaseAddress + I2C_INTERRUPT_STATUS_REGISTER_0_OFFSET, MAX_UINT32);

  Timeout = I2C_TIMEOUT;
  do {
    Data32 = MmioRead32 (Private->BaseAddress + I2C_MST_FIFO_STATUS_0_OFFSET);
    Data32 = (Data32 & TX_FIFO_EMPTY_CNT_MASK) >> TX_FIFO_EMPTY_CNT_SHIFT;
    if (Data32 < 3) {
      MicroSecondDelay (1);
      Timeout--;
      if (Timeout == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Timeout waiting for to send packet header Free\r\n", __FUNCTION__));
        return EFI_TIMEOUT;
      }
    }
  } while (Data32 < 3);

  MmioWrite32 (Private->BaseAddress + I2C_I2C_TX_PACKET_FIFO_0_OFFSET, PacketHeader[0]);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_TX_PACKET_FIFO_0_OFFSET, PacketHeader[1]);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_TX_PACKET_FIFO_0_OFFSET, PacketHeader[2]);

  Status = TegraI2cLoadConfiguration (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to update configuration (%r)\r\n", __FUNCTION__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Start an I2C transaction on the host controller.

  This routine must be called at or below TPL_NOTIFY.  For synchronous
  requests this routine must be called at or below TPL_CALLBACK.

  This function initiates an I2C transaction on the controller.  To
  enable proper error handling by the I2C protocol stack, the I2C
  master protocol does not support queuing but instead only manages
  one I2C transaction at a time.  This API requires that the I2C bus
  is in the correct configuration for the I2C transaction.

  The transaction is performed by sending a start-bit and selecting the
  I2C device with the specified I2C slave address and then performing
  the specified I2C operations.  When multiple operations are requested
  they are separated with a repeated start bit and the slave address.
  The transaction is terminated with a stop bit.

  When Event is NULL, StartRequest operates synchronously and returns
  the I2C completion status as its return value.

  When Event is not NULL, StartRequest synchronously returns EFI_SUCCESS
  indicating that the I2C transaction was started asynchronously.  The
  transaction status value is returned in the buffer pointed to by
  I2cStatus upon the completion of the I2C transaction when I2cStatus
  is not NULL.  After the transaction status is returned the Event is
  signaled.

  Note: The typical consumer of this API is the I2C host protocol.
  Extreme care must be taken by other consumers of this API to prevent
  confusing the third party I2C drivers due to a state change at the
  I2C device which the third party I2C drivers did not initiate.  I2C
  platform specific code may use this API within these guidelines.

  @param[in] This           Pointer to an EFI_I2C_MASTER_PROTOCOL structure.
  @param[in] SlaveAddress   Address of the device on the I2C bus.  Set the
                            I2C_ADDRESSING_10_BIT when using 10-bit addresses,
                            clear this bit for 7-bit addressing.  Bits 0-6
                            are used for 7-bit I2C slave addresses and bits
                            0-9 are used for 10-bit I2C slave addresses.
  @param[in] RequestPacket  Pointer to an EFI_I2C_REQUEST_PACKET
                            structure describing the I2C transaction.
  @param[in] Event          Event to signal for asynchronous transactions,
                            NULL for asynchronous transactions
  @param[out] I2cStatus     Optional buffer to receive the I2C transaction
                            completion status

  @retval EFI_SUCCESS           The asynchronous transaction was successfully
                                started when Event is not NULL.
  @retval EFI_SUCCESS           The transaction completed successfully when
                                Event is NULL.
  @retval EFI_ALREADY_STARTED   The controller is busy with another transaction.
  @retval EFI_BAD_BUFFER_SIZE   The RequestPacket->LengthInBytes value is too
                                large.
  @retval EFI_DEVICE_ERROR      There was an I2C error (NACK) during the
                                transaction.
  @retval EFI_INVALID_PARAMETER RequestPacket is NULL
  @retval EFI_NOT_FOUND         Reserved bit set in the SlaveAddress parameter
  @retval EFI_NO_RESPONSE       The I2C device is not responding to the slave
                                address.  EFI_DEVICE_ERROR will be returned if
                                the controller cannot distinguish when the NACK
                                occurred.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory for I2C transaction
  @retval EFI_UNSUPPORTED       The controller does not support the requested
                                transaction.

**/
EFI_STATUS
TegraI2cStartRequest (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This,
  IN UINTN                          SlaveAddress,
  IN EFI_I2C_REQUEST_PACKET         *RequestPacket,
  IN EFI_EVENT                      Event      OPTIONAL,
  OUT EFI_STATUS                    *I2cStatus OPTIONAL
  )
{
  NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private = NULL;
  EFI_STATUS                     Status;
  UINTN                          PacketIndex   = 0;
  BOOLEAN                        BlockTransfer = FALSE;
  BOOLEAN                        LastOperation;
  BOOLEAN                        PecSupported = FALSE;
  UINT8                          Crc8         = 0;
  UINT8                          AddressCrc8  = 0;
  UINT8                          ReadCrc8     = 0;
  UINT32                         Data32;
  UINT32                         Timeout;
  BOOLEAN                        ReadOperation;
  NVIDIA_PIN_CONTROL_PROTOCOL    *PinControl;

  if ((This == NULL) ||
      (RequestPacket == NULL) ||
      (RequestPacket->OperationCount == 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private = TEGRA_I2C_PRIVATE_DATA_FROM_MASTER (This);

  if ((RequestPacket->Operation[0].Flags & I2C_FLAG_SMBUS_PEC) != 0) {
    if (RequestPacket->OperationCount > 2) {
      return EFI_INVALID_PARAMETER;
    } else if (RequestPacket->OperationCount == 2) {
      if (((RequestPacket->Operation[0].Flags & I2C_FLAG_READ) != 0) ||
          ((RequestPacket->Operation[1].Flags & I2C_FLAG_READ) == 0))
      {
        return EFI_INVALID_PARAMETER;
      }
    }

    PecSupported = TRUE;
  }

  if ((RequestPacket->Operation[0].Flags & I2C_FLAG_SMBUS_BLOCK) != 0) {
    BlockTransfer = TRUE;
  }

  Status = TegraI2cLoadConfiguration (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to update configuration (%r)\r\n", __FUNCTION__, Status));
    return Status;
  }

  if (!Private->PinControlConfigured) {
    if (Private->PinControlId != 0) {
      Status = gBS->LocateProtocol (&gNVIDIAPinControlProtocolGuid, NULL, (VOID **)&PinControl);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get pin control protocol when needed (%r)\r\n", __FUNCTION__, Status));
        return Status;
      }

      Status = PinControl->Enable (PinControl, Private->PinControlId);
      if (Status == EFI_NOT_FOUND) {
        DEBUG ((DEBUG_ERROR, "%a: Pinctl in device tree but not supported, ignoring.\r\n", __FUNCTION__));
        Status = EFI_SUCCESS;
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to configure pin control - %x (%r)\r\n", __FUNCTION__, Private->PinControlId, Status));
        return Status;
      }
    }

    Private->PinControlConfigured = TRUE;
  }

  for (PacketIndex = 0; PacketIndex < RequestPacket->OperationCount; PacketIndex++) {
    BOOLEAN  ContinueTransfer = FALSE;
    UINT32   LengthRemaining  = RequestPacket->Operation[PacketIndex].LengthInBytes;
    UINT32   BufferOffset     = 0;

    if ((RequestPacket->Operation[PacketIndex].Flags & I2C_FLAG_READ) != 0) {
      ReadOperation = TRUE;
    } else {
      ReadOperation = FALSE;
    }

    if (PecSupported) {
      AddressCrc8 = SlaveAddress << 1;
      if (ReadOperation) {
        AddressCrc8 |= 1;
      }

      Crc8 = CalculateCrc8 (&AddressCrc8, 1, Crc8, TYPE_CRC8);
      if (!ReadOperation) {
        if (RequestPacket->Operation[PacketIndex].LengthInBytes != 0) {
          Crc8 = CalculateCrc8 (RequestPacket->Operation[PacketIndex].Buffer, RequestPacket->Operation[PacketIndex].LengthInBytes, Crc8, TYPE_CRC8);
        }
      }
    }

    LastOperation = (PacketIndex == (RequestPacket->OperationCount - 1));

    do {
      UINT32  PayloadSize = 0;
      if (!ReadOperation) {
        if (PecSupported && (BufferOffset == 0) && LastOperation) {
          LengthRemaining++;
        }

        PayloadSize      = MIN (LengthRemaining, I2C_MAX_PACKET_SIZE - I2C_PACKET_HEADER_SIZE);
        ContinueTransfer = ((PayloadSize != LengthRemaining));
        Status           = TegraI2cSendHeader (Private, SlaveAddress, PayloadSize, ReadOperation, LastOperation, ContinueTransfer);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Header send failed (%r)\r\n", __FUNCTION__, Status));
          goto Exit;
        }

        while (PayloadSize != 0) {
          UINT32  WriteSize = MIN (sizeof (UINT32), PayloadSize);
          Timeout = I2C_TIMEOUT;
          do {
            Data32 = MmioRead32 (Private->BaseAddress + I2C_MST_FIFO_STATUS_0_OFFSET);
            Data32 = (Data32 & TX_FIFO_EMPTY_CNT_MASK) >> TX_FIFO_EMPTY_CNT_SHIFT;
            if (Data32 != 0) {
              MicroSecondDelay (1);
              Timeout--;
              if (Timeout == 0) {
                DEBUG ((DEBUG_ERROR, "%a: Timeout waiting for TX Free\r\n", __FUNCTION__));
                Status = EFI_TIMEOUT;
                goto Exit;
              }

              Data32 = MmioRead32 (Private->BaseAddress + I2C_PACKET_TRANSFER_STATUS_0_OFFSET);
              if ((Data32 & (PACKET_TRANSFER_NOACK_FOR_ADDR| PACKET_TRANSFER_NOACK_FOR_DATA)) != 0) {
                DEBUG ((DEBUG_ERROR, "%a: NAK for TX\r\n", __FUNCTION__));
                Status = EFI_DEVICE_ERROR;
                goto Exit;
              }

              Data32 = MmioRead32 (Private->BaseAddress + I2C_MST_FIFO_STATUS_0_OFFSET);
              Data32 = (Data32 & TX_FIFO_EMPTY_CNT_MASK) >> TX_FIFO_EMPTY_CNT_SHIFT;
            }
          } while (Data32 == 0);

          Data32 = 0;
          if (PecSupported && LastOperation && (WriteSize == LengthRemaining)) {
            CopyMem (
              (VOID *)&Data32,
              RequestPacket->Operation[PacketIndex].Buffer + BufferOffset,
              WriteSize-1
              );
            ((UINT8 *)&Data32)[WriteSize-1] = Crc8;
          } else {
            CopyMem (
              (VOID *)&Data32,
              RequestPacket->Operation[PacketIndex].Buffer + BufferOffset,
              WriteSize
              );
          }

          MmioWrite32 (Private->BaseAddress + I2C_I2C_TX_PACKET_FIFO_0_OFFSET, Data32);
          PayloadSize     -= WriteSize;
          LengthRemaining -= WriteSize;
          BufferOffset    += WriteSize;
        }
      } else {
        UINT32  ReadPacketSize;
        if (PecSupported && LastOperation && (BufferOffset == 0)) {
          LengthRemaining++;
        }

        if ((BufferOffset == 0) && BlockTransfer) {
          ReadPacketSize = 1;
        } else {
          ReadPacketSize = MIN (LengthRemaining, I2C_MAX_PACKET_SIZE);
        }

        ContinueTransfer = (LengthRemaining != ReadPacketSize);
        Status           = TegraI2cSendHeader (Private, SlaveAddress, ReadPacketSize, ReadOperation, LastOperation, ContinueTransfer);
        while (ReadPacketSize != 0) {
          UINT32  ReadSize = MIN (sizeof (UINT32), ReadPacketSize);
          Timeout = I2C_TIMEOUT;
          do {
            Data32 = MmioRead32 (Private->BaseAddress + I2C_MST_FIFO_STATUS_0_OFFSET);
            Data32 = (Data32 & RX_FIFO_FULL_CNT_MASK) >> RX_FIFO_FULL_CNT_SHIFT;
            if (Data32 == 0) {
              MicroSecondDelay (1);
              Timeout--;
              if (Timeout == 0) {
                DEBUG ((DEBUG_ERROR, "%a: Timeout waiting for RX Full\r\n", __FUNCTION__));
                Status = EFI_TIMEOUT;
                goto Exit;
              }

              Data32 = MmioRead32 (Private->BaseAddress + I2C_PACKET_TRANSFER_STATUS_0_OFFSET);
              if ((Data32 & (PACKET_TRANSFER_NOACK_FOR_ADDR| PACKET_TRANSFER_NOACK_FOR_DATA)) != 0) {
                DEBUG ((DEBUG_ERROR, "%a: NAK for RX\r\n", __FUNCTION__));
                Status = EFI_NO_RESPONSE;
                goto Exit;
              }

              Data32 = MmioRead32 (Private->BaseAddress + I2C_MST_FIFO_STATUS_0_OFFSET);
              Data32 = (Data32 & RX_FIFO_FULL_CNT_MASK) >> RX_FIFO_FULL_CNT_SHIFT;
            }
          } while (Data32 == 0);

          Data32 = MmioRead32 (Private->BaseAddress + I2C_I2C_RX_FIFO_0_OFFSET);
          if (PecSupported && LastOperation && (LengthRemaining == ReadSize)) {
            CopyMem (
              RequestPacket->Operation[PacketIndex].Buffer + BufferOffset,
              (VOID *)&Data32,
              ReadSize-1
              );
            ReadCrc8 = ((UINT8 *)&Data32)[ReadSize-1];
          } else {
            CopyMem (
              RequestPacket->Operation[PacketIndex].Buffer + BufferOffset,
              (VOID *)&Data32,
              ReadSize
              );
          }

          if ((BufferOffset == 0) && BlockTransfer) {
            if (RequestPacket->Operation[PacketIndex].LengthInBytes < (*RequestPacket->Operation[PacketIndex].Buffer + 1)) {
              Status = EFI_BUFFER_TOO_SMALL;
              goto Exit;
            }

            RequestPacket->Operation[PacketIndex].LengthInBytes = *RequestPacket->Operation[PacketIndex].Buffer + 1;
            LengthRemaining                                     = *RequestPacket->Operation[PacketIndex].Buffer;
            if (PecSupported && LastOperation) {
              LengthRemaining++;
            }
          } else {
            LengthRemaining -= ReadSize;
          }

          ReadPacketSize -= ReadSize;
          BufferOffset   += ReadSize;
        }
      }
    } while (LengthRemaining != 0);

    if (ReadOperation && PecSupported) {
      if (RequestPacket->Operation[PacketIndex].LengthInBytes != 0) {
        Crc8 = CalculateCrc8 (RequestPacket->Operation[PacketIndex].Buffer, RequestPacket->Operation[PacketIndex].LengthInBytes, Crc8, TYPE_CRC8);
      }
    }

    // Error Check
    Timeout = I2C_TIMEOUT;
    do {
      MicroSecondDelay (1);
      Timeout--;
      if (Timeout == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Timeout waiting for Packet Complete\r\n", __FUNCTION__));
        Status = EFI_TIMEOUT;
        break;
      }

      Data32 = MmioRead32 (Private->BaseAddress + I2C_INTERRUPT_STATUS_REGISTER_0_OFFSET);
      MmioWrite32 (Private->BaseAddress + I2C_INTERRUPT_STATUS_REGISTER_0_OFFSET, Data32);
      if ((Data32 & INTERRUPT_STATUS_NOACK) != 0) {
        DEBUG ((DEBUG_INFO, "%a: No ACK received\r\n", __FUNCTION__));
        Status = EFI_NO_RESPONSE;
        break;
      }

      if ((Data32 & INTERRUPT_STATUS_ARB_LOST) != 0) {
        DEBUG ((DEBUG_ERROR, "%a: ARB Lost\r\n", __FUNCTION__));
        Status = EFI_DEVICE_ERROR;
        break;
      }

      if ((Data32 & INTERRUPT_STATUS_PACKET_XFER_COMPLETE) != 0) {
        Status = EFI_SUCCESS;
        break;
      }
    } while (TRUE);

    if (EFI_ERROR (Status)) {
      break;
    }
  }

  if (!EFI_ERROR (Status)) {
    if (PecSupported &&
        ((RequestPacket->Operation[RequestPacket->OperationCount-1].Flags & I2C_FLAG_READ) != 0) &&
        (ReadCrc8 != Crc8))
    {
      DEBUG ((DEBUG_ERROR, "%a: PEC Mismatch, got: 0x%02x expected 0x%02x\r\n", __FUNCTION__, ReadCrc8, Crc8));
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }
  }

Exit:

  if (EFI_ERROR (Status)) {
    This->Reset (This);
  }

  if (I2cStatus != NULL) {
    *I2cStatus = Status;
    Status     = EFI_SUCCESS;
  }

  if (Event != NULL) {
    gBS->SignalEvent (Event);
  }

  return Status;
}

/**
  Enumerate the I2C devices

  This function enables the caller to traverse the set of I2C devices
  on an I2C bus.

  @param[in]  This              The platform data for the next device on
                                the I2C bus was returned successfully.
  @param[in, out] Device        Pointer to a buffer containing an
                                EFI_I2C_DEVICE structure.  Enumeration is
                                started by setting the initial EFI_I2C_DEVICE
                                structure pointer to NULL.  The buffer
                                receives an EFI_I2C_DEVICE structure pointer
                                to the next I2C device.

  @retval EFI_SUCCESS           The platform data for the next device on
                                the I2C bus was returned successfully.
  @retval EFI_INVALID_PARAMETER Device is NULL
  @retval EFI_NO_MAPPING        *Device does not point to a valid
                                EFI_I2C_DEVICE structure returned in a
                                previous call Enumerate().

**/
EFI_STATUS
TegraI2cEnumerate (
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL  *This,
  IN OUT CONST EFI_I2C_DEVICE          **Device
  )
{
  NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private;
  UINTN                          Index;

  if ((This == NULL) ||
      (Device == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private = TEGRA_I2C_PRIVATE_DATA_FROM_ENUMERATE (This);

  if (*Device == NULL) {
    Index = 0;
  } else {
    for (Index = 0; Index < Private->NumberOfI2cDevices; Index++) {
      if (&Private->I2cDevices[Index] == *Device) {
        break;
      }
    }

    if (Index == Private->NumberOfI2cDevices) {
      return EFI_NO_MAPPING;
    }

    Index++;
  }

  if (Index == Private->NumberOfI2cDevices) {
    *Device = NULL;
    return EFI_NOT_FOUND;
  }

  *Device = &Private->I2cDevices[Index];
  return EFI_SUCCESS;
}

/**
  Get the requested I2C bus frequency for a specified bus configuration.

  This function returns the requested I2C bus clock frequency for the
  I2cBusConfiguration.  This routine is provided for diagnostic purposes
  and is meant to be called after calling Enumerate to get the
  I2cBusConfiguration value.

  @param[in] This                 Pointer to an EFI_I2C_ENUMERATE_PROTOCOL
                                  structure.
  @param[in] I2cBusConfiguration  I2C bus configuration to access the I2C
                                  device
  @param[out] *BusClockHertz      Pointer to a buffer to receive the I2C
                                  bus clock frequency in Hertz

  @retval EFI_SUCCESS           The I2C bus frequency was returned
                                successfully.
  @retval EFI_INVALID_PARAMETER BusClockHertz was NULL
  @retval EFI_NO_MAPPING        Invalid I2cBusConfiguration value

**/
EFI_STATUS
TegraI2cGetBusFrequency (
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL  *This,
  IN UINTN                             I2cBusConfiguration,
  OUT UINTN                            *BusClockHertz
  )
{
  NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private = NULL;

  if ((This == NULL) ||
      (NULL == BusClockHertz))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private = TEGRA_I2C_PRIVATE_DATA_FROM_ENUMERATE (This);

  if (0 != I2cBusConfiguration) {
    return EFI_NO_MAPPING;
  }

  *BusClockHertz = Private->BusClockHertz;
  return EFI_SUCCESS;
}

/**
  Enable access to an I2C bus configuration.

  This routine must be called at or below TPL_NOTIFY.  For synchronous
  requests this routine must be called at or below TPL_CALLBACK.

  Reconfigure the switches and multiplexers in the I2C bus to enable
  access to a specific I2C bus configuration.  Also select the maximum
  clock frequency for this I2C bus configuration.

  This routine uses the I2C Master protocol to perform I2C transactions
  on the local bus.  This eliminates any recursion in the I2C stack for
  configuration transactions on the same I2C bus.  This works because the
  local I2C bus is idle while the I2C bus configuration is being enabled.

  If I2C transactions must be performed on other I2C busses, then the
  EFI_I2C_HOST_PROTOCOL, the EFI_I2C_IO_PROTCOL, or a third party I2C
  driver interface for a specific device must be used.  This requirement
  is because the I2C host protocol controls the flow of requests to the
  I2C controller.  Use the EFI_I2C_HOST_PROTOCOL when the I2C device is
  not enumerated by the EFI_I2C_ENUMERATE_PROTOCOL.  Use a protocol
  produced by a third party driver when it is available or the
  EFI_I2C_IO_PROTOCOL when the third party driver is not available but
  the device is enumerated with the EFI_I2C_ENUMERATE_PROTOCOL.

  When Event is NULL, EnableI2cBusConfiguration operates synchronously
  and returns the I2C completion status as its return value.

  @param[in]  This            Pointer to an EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL
                              structure.
  @param[in]  I2cBusConfiguration Index of an I2C bus configuration.  All
                                  values in the range of zero to N-1 are
                                  valid where N is the total number of I2C
                                  bus configurations for an I2C bus.
  @param[in]  Event           Event to signal when the transaction is complete
  @param[out] I2cStatus       Buffer to receive the transaction status.

  @return  When Event is NULL, EnableI2cBusConfiguration operates synchrouously
  and returns the I2C completion status as its return value.  In this case it is
  recommended to use NULL for I2cStatus.  The values returned from
  EnableI2cBusConfiguration are:776645563850

  @retval EFI_SUCCESS           The asynchronous bus configuration request
                                was successfully started when Event is not
                                NULL.BpmpIpcEvent
  @retval EFI_SUCCESS           The bus configuration request completed
                                successfully when Event is NULL.
  @retval EFI_DEVICE_ERROR      The bus configuration failed.
  @retval EFI_NO_MAPPING        Invalid I2cBusConfiguration value

**/
EFI_STATUS
TegraI2cEnableI2cBusConfiguration (
  IN CONST EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL  *This,
  IN UINTN                                                I2cBusConfiguration,
  IN EFI_EVENT                                            Event      OPTIONAL,
  IN EFI_STATUS                                           *I2cStatus OPTIONAL
  )
{
  if (I2cBusConfiguration != 0) {
    return EFI_NO_MAPPING;
  }

  if (NULL != Event) {
    if (NULL == I2cStatus) {
      return EFI_INVALID_PARAMETER;
    }

    *I2cStatus = EFI_SUCCESS;
    gBS->SignalEvent (Event);
  }

  return EFI_SUCCESS;
}

/**
  This routine is called to add an I2C device to the controller.

  @param[in] Private            Driver's private data.
  @param[in] I2cAddress         Address of the device.
  @param[in] DeviceGuid         GUID to identify the device type.
  @param[in] DeviceIndex        Index of the device derived from device tree.

  @retval EFI_SUCCESS           Device added.
  @retval other                 Some error occurs when device is being added.

**/
EFI_STATUS
TegraI2cAddDevice (
  IN NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private,
  IN UINT32                         I2cAddress,
  IN EFI_GUID                       *DeviceGuid,
  IN UINT32                         DeviceIndex
  )
{
  if (Private->NumberOfI2cDevices >= MAX_I2C_DEVICES) {
    DEBUG ((DEBUG_ERROR, "%a: Too many i2c devices detected, increase limit\r\n", __FUNCTION__));
    ASSERT (FALSE);
    return EFI_OUT_OF_RESOURCES;
  }

  Private->SlaveAddressArray[Private->NumberOfI2cDevices * MAX_SLAVES_PER_DEVICE] = I2cAddress;
  Private->I2cDevices[Private->NumberOfI2cDevices].DeviceGuid                     = DeviceGuid;
  Private->I2cDevices[Private->NumberOfI2cDevices].DeviceIndex                    = DeviceIndex;
  Private->I2cDevices[Private->NumberOfI2cDevices].HardwareRevision               = 1;
  Private->I2cDevices[Private->NumberOfI2cDevices].I2cBusConfiguration            = 0;
  Private->I2cDevices[Private->NumberOfI2cDevices].SlaveAddressCount              = 1;
  Private->I2cDevices[Private->NumberOfI2cDevices].SlaveAddressArray              = &Private->SlaveAddressArray[Private->NumberOfI2cDevices * MAX_SLAVES_PER_DEVICE];
  Private->NumberOfI2cDevices++;

  return EFI_SUCCESS;
}

/**
  This routine is called right after the .Supported() called and
  Start this driver on ControllerHandle.

  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

  @retval EFI_SUCCESS           This driver is added to this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
TegraI2CDriverBindingStart (
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                     Status;
  NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private = NULL;
  UINTN                          RegionSize;
  CONST UINT32                   *DtClockHertz;
  CONST UINT32                   *DtControllerId;
  UINT32                         Data32;
  UINTN                          Index;
  NON_DISCOVERABLE_DEVICE        *Device;
  INT32                          I2cNodeOffset = 0;
  UINT32                         I2cAddress;
  INT32                          EepromManagerNodeOffset;
  UINT32                         I2cNodeHandle;
  INT32                          EepromManagerBusNodeOffset;
  CONST UINT32                   *I2cBusProperty;
  INT32                          I2cBusHandleLength;
  UINT32                         I2cBusHandle;
  INT32                          EepromNodeOffset;
  CONST VOID                     *Property;
  INT32                          PropertyLen;
  EFI_GUID                       *DeviceGuid;
  EFI_DEVICE_PATH                *OldDevicePath;
  EFI_DEVICE_PATH                *NewDevicePath;
  EFI_DEVICE_PATH_PROTOCOL       *DevicePathNode;
  UINT32                         Count;

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Device
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get non discoverable protocol\r\n"));
    return Status;
  }

  Private = (NVIDIA_TEGRA_I2C_PRIVATE_DATA *)AllocateZeroPool (sizeof (NVIDIA_TEGRA_I2C_PRIVATE_DATA));
  if (NULL == Private) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate private data\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Private->Signature                                      = TEGRA_I2C_SIGNATURE;
  Private->I2cMaster.SetBusFrequency                      = TegraI2cSetBusFrequency;
  Private->I2cMaster.Reset                                = TegraI2cReset;
  Private->I2cMaster.StartRequest                         = TegraI2cStartRequest;
  Private->I2cMaster.I2cControllerCapabilities            = &Private->I2cControllerCapabilities;
  Private->I2cControllerCapabilities.MaximumReceiveBytes  = SIZE_64KB;
  Private->I2cControllerCapabilities.MaximumTransmitBytes = SIZE_64KB;
  Private->I2cControllerCapabilities.MaximumTotalBytes    = SIZE_64KB;
  Private->I2cControllerCapabilities.StructureSizeInBytes = sizeof (Private->I2cControllerCapabilities);
  Private->I2cEnumerate.Enumerate                         = TegraI2cEnumerate;
  Private->I2cEnumerate.GetBusFrequency                   = TegraI2cGetBusFrequency;
  Private->I2CConfiguration.EnableI2cBusConfiguration     = TegraI2cEnableI2cBusConfiguration;
  Private->ProtocolsInstalled                             = FALSE;
  Private->DeviceTreeBase                                 = DeviceTreeNode->DeviceTreeBase;
  Private->DeviceTreeNodeOffset                           = DeviceTreeNode->NodeOffset;
  Private->ConfigurationChanged                           = TRUE;
  Private->ControllerHandle                               = ControllerHandle;
  Private->DeviceTreeNode                                 = DeviceTreeNode;
  Private->PacketId                                       = 0;
  Private->HighSpeed                                      = FALSE;

  Private->PinControlConfigured = FALSE;
  Property                      = fdt_getprop (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "pinctrl-0", NULL);
  if (Property != NULL) {
    Private->PinControlId = SwapBytes32 (*(CONST UINT32 *)Property);
  } else {
    Private->PinControlId = 0;
  }

  DtControllerId = (CONST UINT32 *)fdt_getprop (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "nvidia,hw-instance-id", NULL);
  if (NULL != DtControllerId) {
    Private->ControllerId = SwapBytes32 (*DtControllerId);
    if (Private->ControllerId > 0xf) {
      DEBUG ((DEBUG_ERROR, "%a: Controller Id out of range (%x) setting to 0xf\r\n", __FUNCTION__, Private->ControllerId));
      Private->ControllerId = 0xf;
    }
  } else {
    CHAR8  I2cName[] = "i2cx";
    Private->ControllerId = 0xf;

    for (Index = 0; Index <= 9; Index++) {
      INT32        AliasOffset;
      CONST CHAR8  *AliasName;
      AsciiSPrint (I2cName, sizeof (I2cName), "i2c%u", Index);
      AliasName = fdt_get_alias (DeviceTreeNode->DeviceTreeBase, I2cName);
      if (AliasName == NULL) {
        break;
      }

      AliasOffset = fdt_path_offset (DeviceTreeNode->DeviceTreeBase, AliasName);
      if (AliasOffset == DeviceTreeNode->NodeOffset) {
        Private->ControllerId = Index;
        break;
      }
    }

    if (Private->ControllerId == 0xf) {
      DEBUG ((DEBUG_WARN, "%a: no nvidia,hw-instance-id in dt or alias, defaulting to %d\r\n", __FUNCTION__, Private->ControllerId));
    }
  }

  // Add controller device node
  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&OldDevicePath
                  );
  if (!EFI_ERROR (Status)) {
    DevicePathNode = OldDevicePath;
    // Check to make sure we haven't already added controller
    while (!IsDevicePathEnd (DevicePathNode)) {
      if ((DevicePathType (DevicePathNode) == HARDWARE_DEVICE_PATH) &&
          (DevicePathSubType (DevicePathNode) == HW_CONTROLLER_DP))
      {
        break;
      }

      DevicePathNode = NextDevicePathNode (DevicePathNode);
    }

    if (IsDevicePathEnd (DevicePathNode)) {
      ControllerNode.ControllerNumber = Private->ControllerId;
      NewDevicePath                   = AppendDevicePathNode (OldDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&ControllerNode);
      if (NewDevicePath == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to create new device path\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        goto ErrorExit;
      }

      Status = gBS->ReinstallProtocolInterface (
                      ControllerHandle,
                      &gEfiDevicePathProtocolGuid,
                      OldDevicePath,
                      NewDevicePath
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to update device path, %r\r\n", __FUNCTION__, Status));
        goto ErrorExit;
      }
    }
  }

  Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &Private->BaseAddress, &RegionSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TegraI2cDxe: Failed to get region location (%r)\r\n", Status));
    goto ErrorExit;
  }

  // Initialize controller
  Status = TegraI2cReset (&Private->I2cMaster);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, Failed to reset I2C (%r)\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  MmioWrite32 (Private->BaseAddress + I2C_I2C_TLOW_SEXT_0_OFFSET, 0);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_CMD_ADDR0_0_OFFSET, 0);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_CMD_ADDR1_0_OFFSET, 0);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_CMD_DATA1_0_OFFSET, 0);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_CMD_DATA2_0_OFFSET, 0);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_CLKEN_OVERRIDE_0_OFFSET, 0);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_DEBUG_CONTROL_0_OFFSET, 0);
  MmioWrite32 (Private->BaseAddress + I2C_I2C_INTERRUPT_SET_REGISTER_0_OFFSET, 0);

  DtClockHertz = (CONST UINT32 *)fdt_getprop (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "clock-frequency", NULL);
  if (NULL != DtClockHertz) {
    Private->BusClockHertz = SwapBytes32 (*DtClockHertz);
  } else {
    DEBUG ((DEBUG_WARN, "%a: no clock-frequency in dt, defaulting to %lu\r\n", __FUNCTION__, STD_SPEED));
    Private->BusClockHertz = STD_SPEED;
  }

  Status = TegraI2cSetBusFrequency (&Private->I2cMaster, &Private->BusClockHertz);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set clock frequency (%r)\r\n", __FUNCTION__, Status));
  }

  Data32 = I2C_I2C_CNFG_0_PACKET_MODE_EN | I2C_I2C_CNFG_0_NEW_MASTER_FSM;
  if (Private->BusClockHertz <= HS_SPEED) {
    Data32 |= (0x2 << I2C_I2C_CNFG_0_DEBOUNCE_CNT_SHIFT);
  }

  if (NULL != fdt_get_property (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "multi-master", NULL)) {
    Data32 |= I2C_I2C_CNFG_0_MULTI_MASTER_MODE;
  }

  MmioWrite32 (Private->BaseAddress + I2C_I2C_CNFG_0_OFFSET, Data32);

  Private->ConfigurationChanged = TRUE;
  Status                        = TegraI2cLoadConfiguration (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to load configuration (%r)\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  PropertyLen                 = 0;
  Private->NumberOfI2cDevices = 0;

  I2cNodeHandle = fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset);
  Count         = 0;
  fdt_for_each_subnode (I2cNodeOffset, DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset) {
    if (fdt_node_check_compatible (
          DeviceTreeNode->DeviceTreeBase,
          I2cNodeOffset,
          "atmel,24c02"
          ) == 0)
    {
      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset, "reg", &PropertyLen);
      if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
        gBS->CopyMem (&I2cAddress, (VOID *)Property, PropertyLen);
        I2cAddress = SwapBytes32 (I2cAddress);
        DEBUG ((DEBUG_INFO, "%a: Eeprom Found.\n", __FUNCTION__));
        DeviceGuid = &gNVIDIAEeprom;
        Status     = TegraI2cAddDevice (
                       Private,
                       I2cAddress,
                       DeviceGuid,
                       Count
                       );
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }

        Count++;
        DEBUG ((DEBUG_INFO, "%a: Eeprom Slave Address: 0x%lx on I2c Bus 0x%lx.\n", __FUNCTION__, I2cAddress, Private->ControllerId));
      }
    } else if (fdt_node_check_compatible (
                 DeviceTreeNode->DeviceTreeBase,
                 I2cNodeOffset,
                 "ti,tca9539"
                 ) == 0)
    {
      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset, "reg", &PropertyLen);
      if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
        gBS->CopyMem (&I2cAddress, (VOID *)Property, PropertyLen);
        I2cAddress = SwapBytes32 (I2cAddress);
        DEBUG ((DEBUG_INFO, "%a: TCA9539 Found.\n", __FUNCTION__));
        DeviceGuid = &gNVIDIAI2cTca9539;
        Status     = TegraI2cAddDevice (
                       Private,
                       I2cAddress,
                       DeviceGuid,
                       fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset)
                       );
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }

        DEBUG ((DEBUG_INFO, "%a: TCA9539 Slave Address: 0x%lx on I2c Bus 0x%lx.\n", __FUNCTION__, I2cAddress, Private->ControllerId));
      }
    } else if (fdt_node_check_compatible (
                 DeviceTreeNode->DeviceTreeBase,
                 I2cNodeOffset,
                 "nxp,pca9535"
                 ) == 0)
    {
      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset, "reg", &PropertyLen);
      if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
        gBS->CopyMem (&I2cAddress, (VOID *)Property, PropertyLen);
        I2cAddress = SwapBytes32 (I2cAddress);
        DEBUG ((DEBUG_INFO, "%a: PCA9535 Found.\n", __FUNCTION__));
        DeviceGuid = &gNVIDIAI2cPca9535;
        Status     = TegraI2cAddDevice (
                       Private,
                       I2cAddress,
                       DeviceGuid,
                       fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset)
                       );
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }

        DEBUG ((DEBUG_INFO, "%a: PCA9535 Slave Address: 0x%lx on I2c Bus 0x%lx.\n", __FUNCTION__, I2cAddress, Private->ControllerId));
      }
    } else if (fdt_node_check_compatible (
                 DeviceTreeNode->DeviceTreeBase,
                 I2cNodeOffset,
                 "nvidia,ncp81599"
                 ) == 0)
    {
      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset, "reg", &PropertyLen);
      if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
        gBS->CopyMem (&I2cAddress, (VOID *)Property, PropertyLen);
        I2cAddress = SwapBytes32 (I2cAddress);
        DEBUG ((DEBUG_INFO, "%a: NCP81599 Found.\n", __FUNCTION__));
        DeviceGuid = &gNVIDIAI2cNcp81599;
        Status     = TegraI2cAddDevice (
                       Private,
                       I2cAddress,
                       DeviceGuid,
                       fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset)
                       );
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }
      }
    } else if (fdt_node_check_compatible (
                 DeviceTreeNode->DeviceTreeBase,
                 I2cNodeOffset,
                 "nuvoton,nct3018y"
                 ) == 0)
    {
      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset, "reg", &PropertyLen);
      if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
        gBS->CopyMem (&I2cAddress, (VOID *)Property, PropertyLen);
        I2cAddress = SwapBytes32 (I2cAddress);
        DEBUG ((DEBUG_INFO, "%a: NCT3018Y Found.\n", __FUNCTION__));
        DeviceGuid = &gNVIDIAI2cNct3018y;
        Status     = TegraI2cAddDevice (
                       Private,
                       I2cAddress,
                       DeviceGuid,
                       fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset)
                       );
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }
      }
    } else if (fdt_node_check_compatible (
                 DeviceTreeNode->DeviceTreeBase,
                 I2cNodeOffset,
                 "ssif-bmc"
                 ) == 0)
    {
      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset, "reg", &PropertyLen);
      if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
        gBS->CopyMem (&I2cAddress, (VOID *)Property, PropertyLen);
        I2cAddress = SwapBytes32 (I2cAddress);
        DEBUG ((DEBUG_INFO, "%a: BMC-SSIF Slave Address: 0x%lx on I2c Bus 0x%lx.\n", __FUNCTION__, I2cAddress, Private->ControllerId));
        DeviceGuid = &gNVIDIAI2cBmcSSIF;
        Status     = TegraI2cAddDevice (
                       Private,
                       I2cAddress,
                       DeviceGuid,
                       fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset)
                       );
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }

        // Leave i2c for bmc activated on exit boot services
        Private->SkipOnExitDisabled = TRUE;
      }
    } else if (fdt_node_check_compatible (
                 DeviceTreeNode->DeviceTreeBase,
                 I2cNodeOffset,
                 "nvidia,fpga-cfr"
                 ) == 0)
    {
      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset, "reg", &PropertyLen);
      if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
        gBS->CopyMem (&I2cAddress, (VOID *)Property, PropertyLen);
        I2cAddress = SwapBytes32 (I2cAddress);
        DEBUG ((DEBUG_INFO, "%a: FPGA I2C Slave Address: 0x%lx on I2c Bus 0x%lx.\n", __FUNCTION__, I2cAddress, Private->ControllerId));
        DeviceGuid = &gNVIDIAI2cFpga;
        Status     = TegraI2cAddDevice (
                       Private,
                       I2cAddress,
                       DeviceGuid,
                       fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, I2cNodeOffset)
                       );
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }
      }
    }
  }

  Count                   = 0;
  EepromManagerNodeOffset = fdt_path_offset (DeviceTreeNode->DeviceTreeBase, "/eeprom-manager");
  if (EepromManagerNodeOffset >= 0) {
    fdt_for_each_subnode (EepromManagerBusNodeOffset, DeviceTreeNode->DeviceTreeBase, EepromManagerNodeOffset) {
      I2cBusProperty     = NULL;
      I2cBusHandleLength = 0;
      I2cBusProperty     = fdt_getprop (DeviceTreeNode->DeviceTreeBase, EepromManagerBusNodeOffset, "i2c-bus", &I2cBusHandleLength);
      if ((I2cBusProperty == NULL) || (I2cBusHandleLength != sizeof (UINT32))) {
        continue;
      }

      gBS->CopyMem (&I2cBusHandle, (VOID *)I2cBusProperty, I2cBusHandleLength);
      I2cBusHandle = SwapBytes32 (I2cBusHandle);
      if (I2cNodeHandle != I2cBusHandle) {
        continue;
      }

      fdt_for_each_subnode (EepromNodeOffset, DeviceTreeNode->DeviceTreeBase, EepromManagerBusNodeOffset) {
        Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, EepromNodeOffset, "slave-address", &PropertyLen);
        if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
          gBS->CopyMem (&I2cAddress, (VOID *)Property, PropertyLen);
          I2cAddress = SwapBytes32 (I2cAddress);
          DEBUG ((DEBUG_INFO, "%a: Eeprom Found.\n", __FUNCTION__));
          DeviceGuid = &gNVIDIAEeprom;
          Status     = TegraI2cAddDevice (
                         Private,
                         I2cAddress,
                         DeviceGuid,
                         Count
                         );
          if (EFI_ERROR (Status)) {
            goto ErrorExit;
          }

          Count++;
          DEBUG ((DEBUG_INFO, "%a: Eeprom Slave Address: 0x%lx on I2c Bus 0x%lx.\n", __FUNCTION__, I2cAddress, I2cBusHandle));
        }
      }
    }
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ControllerHandle,
                  &gEfiI2cMasterProtocolGuid,
                  &Private->I2cMaster,
                  &gEfiI2cEnumerateProtocolGuid,
                  &Private->I2cEnumerate,
                  &gEfiI2cBusConfigurationManagementProtocolGuid,
                  &Private->I2CConfiguration,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install i2c protocols:%r\r\n", __FUNCTION__, Status));
  } else {
    Private->ProtocolsInstalled = TRUE;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != Private) {
      if (Private->ProtocolsInstalled) {
        gBS->UninstallMultipleProtocolInterfaces (
               ControllerHandle,
               &gEfiI2cMasterProtocolGuid,
               &Private->I2cMaster,
               gEfiI2cEnumerateProtocolGuid,
               &Private->I2cEnumerate,
               &gEfiI2cBusConfigurationManagementProtocolGuid,
               &Private->I2CConfiguration,
               NULL
               );
      }

      FreePool (Private);
    }
  }

  return Status;
}

/**
  Stop this driver on ControllerHandle.

  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.

  @retval EFI_SUCCESS   This driver is removed from this device.
  @retval other         Some error occurs when removing this driver from this device.

**/
EFI_STATUS
EFIAPI
TegraI2CDriverBindingStop (
  IN  EFI_HANDLE  DriverHandle,
  IN  EFI_HANDLE  ControllerHandle
  )
{
  EFI_STATUS  Status;

  EFI_I2C_MASTER_PROTOCOL        *I2cMaster = NULL;
  NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private   = NULL;

  //
  // Attempt to open I2cMaster Protocol
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiI2cMasterProtocolGuid,
                  (VOID **)&I2cMaster,
                  DriverHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Private = TEGRA_I2C_PRIVATE_DATA_FROM_MASTER (I2cMaster);
  if (Private == NULL) {
    return EFI_DEVICE_ERROR;
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  ControllerHandle,
                  &gEfiI2cMasterProtocolGuid,
                  &Private->I2cMaster,
                  gEfiI2cEnumerateProtocolGuid,
                  &Private->I2cEnumerate,
                  &gEfiI2cBusConfigurationManagementProtocolGuid,
                  &Private->I2CConfiguration,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FreePool (Private);
  return EFI_SUCCESS;
}

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                     Status;
  NVIDIA_TEGRA_I2C_PRIVATE_DATA  *Private;
  EFI_I2C_MASTER_PROTOCOL        *I2cMaster;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      Status = TegraI2CDriverBindingStart (DriverHandle, ControllerHandle, DeviceTreeNode);
      break;

    case DeviceDiscoveryDriverBindingStop:
      Status = TegraI2CDriverBindingStop (DriverHandle, ControllerHandle);
      break;

    case DeviceDiscoveryOnExit:
      Status = gBS->HandleProtocol (ControllerHandle, &gEfiI2cMasterProtocolGuid, (VOID **)&I2cMaster);
      if (EFI_ERROR (Status)) {
        Status = EFI_SUCCESS;
        break;
      }

      Private = TEGRA_I2C_PRIVATE_DATA_FROM_MASTER (I2cMaster);
      if (Private->SkipOnExitDisabled) {
        Status = EFI_UNSUPPORTED;
      } else {
        Status = EFI_SUCCESS;
      }

      break;

    case DeviceDiscoveryEnumerationCompleted:
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &DriverHandle,
                      &gNVIDIATegraI2cInitCompleteProtocolGuid,
                      NULL,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to install Tegra I2C init complete protocol: %r\r\n",
          __FUNCTION__,
          Status
          ));
      }

      break;

    default:
      Status = EFI_SUCCESS;
      break;
  }

  return Status;
}
