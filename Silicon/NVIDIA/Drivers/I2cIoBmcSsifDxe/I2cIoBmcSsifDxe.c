/** @file

  I2C IO IPMI driver

  Copyright (c) 2019-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright 1999 - 2021 Intel Corporation. <BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>

#include <IndustryStandard/Ipmi.h>
#include <Protocol/IpmiTransportProtocol.h>
#include <Protocol/I2cMaster.h>
#include <Protocol/I2cEnumerate.h>

#define BMC_SSIF_SIGNATURE  SIGNATURE_64 ('B','M','C','_','S','S','I','F')
#define BMC_RETRY_COUNT     10
#define BMC_RETRY_DELAY     100000

// Private data structure
typedef struct {
  UINT64                     Signature;

  IPMI_TRANSPORT             IpmiTransport;
  EFI_I2C_MASTER_PROTOCOL    *I2cMaster;
  UINT32                     SlaveAddress;

  VOID                       *ProtocolRegistration;
  EFI_EVENT                  ProtocolEvent;

  BMC_STATUS                 BmcStatus;
  UINT32                     SoftErrorCount;
} BMC_SSIF_PRIVATE_DATA;

#define BMC_SSIF_PRIVATE_DATA_FROM_IPMI(a)  CR (a, BMC_SSIF_PRIVATE_DATA, IpmiTransport, BMC_SSIF_SIGNATURE)

#define BMC_SSIF_SINGLE_PART_WRITE_CMD        0x2
#define BMC_SSIF_SINGLE_PART_READ_CMD         0x3
#define BMC_SSIF_MULTI_PART_WRITE_CMD_START   0x6
#define BMC_SSIF_MULTI_PART_WRITE_CMD_MIDDLE  0x7
#define BMC_SSIF_MULTI_PART_WRITE_CMD_END     0x8
// Arm Server Base Manageability Requirements 1.1
#define BMC_SSIF_MULTI_PART_READ_CMD_MIDDLE_END    0x9
#define BMC_SSIF_MULTI_PART_READ_CMD_MIDDLE_RETRY  0xA

#define SSIF_MAX_DATA            0x20
#define SSIF_HEADER_SIZE         2
#define SMBUS_WRITE_HEADER_SIZE  2
#define SMBUS_READ_HEADER_SIZE   1

#define BMC_SLAVE_ADDRESS  0x20
#define MAX_SOFT_COUNT     10

typedef struct {
  ///
  /// Number of elements in the operation array
  ///
  UINTN                OperationCount;

  ///
  /// Description of the I2C operation
  ///
  EFI_I2C_OPERATION    Operation[2];
} SSIF_REQUEST_PACKET;

/**
  This service enables submitting commands via Ipmi.

  @param[in]         This              This point for IPMI_PROTOCOL structure.
  @param[in]         NetFunction       Net function of the command.
  @param[in]         Command           IPMI Command.
  @param[in]         RequestData       Command Request Data.
  @param[in]         RequestDataSize   Size of Command Request Data.
  @param[out]        ResponseData      Command Response Data. The completion code is the first byte of response data.
  @param[in, out]    ResponseDataSize  Size of Command Response Data.

  @retval EFI_SUCCESS            The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_NOT_FOUND          The command was not successfully sent to the device or a response was not successfully received from the device.
  @retval EFI_NOT_READY          Ipmi Device is not ready for Ipmi command access.
  @retval EFI_DEVICE_ERROR       Ipmi Device hardware error.
  @retval EFI_TIMEOUT            The command time out.
  @retval EFI_UNSUPPORTED        The command was not successfully sent to the device.
  @retval EFI_OUT_OF_RESOURCES   The resource allcation is out of resource or data size error.
**/
EFI_STATUS
I2cIoBmcSsifIpmiSubmitCommand (
  IN     IPMI_TRANSPORT  *This,
  IN     UINT8           NetFunction,
  IN     UINT8           Lun,
  IN     UINT8           Command,
  IN     UINT8           *RequestData,
  IN     UINT32          RequestDataSize,
  OUT UINT8              *ResponseData,
  IN OUT UINT32          *ResponseDataSize
  )
{
  EFI_STATUS             Status;
  BMC_SSIF_PRIVATE_DATA  *BmcSsifPrivate;
  SSIF_REQUEST_PACKET    Packet;
  UINT8                  WriteData[SSIF_MAX_DATA + SMBUS_WRITE_HEADER_SIZE];
  UINT8                  ReadData[SSIF_MAX_DATA + SMBUS_READ_HEADER_SIZE];
  UINT32                 DataLeft;
  UINT32                 DataSize;
  UINT8                  ExpectedBlock;
  UINT32                 ResponseDataBufferSize;
  UINT32                 RetryCount;

  BmcSsifPrivate         = BMC_SSIF_PRIVATE_DATA_FROM_IPMI (This);
  ResponseDataBufferSize = *ResponseDataSize;

  // Transmit data
  if ((RequestDataSize + SSIF_HEADER_SIZE) <= SSIF_MAX_DATA) {
    // SinglePart
    WriteData[0]                           = BMC_SSIF_SINGLE_PART_WRITE_CMD;
    WriteData[1]                           = RequestDataSize + SSIF_HEADER_SIZE;
    WriteData[SMBUS_WRITE_HEADER_SIZE + 0] = NetFunction << 2 | (Lun & 0x3);
    WriteData[SMBUS_WRITE_HEADER_SIZE + 1] = Command;
    CopyMem (WriteData + SMBUS_WRITE_HEADER_SIZE + SSIF_HEADER_SIZE, RequestData, RequestDataSize);

    Packet.OperationCount             = 1;
    Packet.Operation[0].Flags         = I2C_FLAG_SMBUS_OPERATION | I2C_FLAG_SMBUS_BLOCK | I2C_FLAG_SMBUS_PEC;
    Packet.Operation[0].LengthInBytes = RequestDataSize + SSIF_HEADER_SIZE + SMBUS_WRITE_HEADER_SIZE;
    Packet.Operation[0].Buffer        = WriteData;

    Status = BmcSsifPrivate->I2cMaster->StartRequest (BmcSsifPrivate->I2cMaster, BmcSsifPrivate->SlaveAddress, (EFI_I2C_REQUEST_PACKET *)&Packet, NULL, NULL);
    if (EFI_ERROR (Status)) {
      BmcSsifPrivate->SoftErrorCount++;
      BmcSsifPrivate->BmcStatus = BMC_SOFTFAIL;
      DEBUG ((DEBUG_ERROR, "%a: Failed to send single part write - %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  } else {
    // Multi-part
    WriteData[0]                           = BMC_SSIF_MULTI_PART_WRITE_CMD_START;
    WriteData[1]                           = SSIF_MAX_DATA;
    WriteData[SMBUS_WRITE_HEADER_SIZE + 0] = NetFunction << 2;
    WriteData[SMBUS_WRITE_HEADER_SIZE + 1] = Command;
    CopyMem (WriteData + SMBUS_WRITE_HEADER_SIZE + SSIF_HEADER_SIZE, RequestData, SSIF_MAX_DATA - SSIF_HEADER_SIZE);

    Packet.OperationCount             = 1;
    Packet.Operation[0].Flags         = I2C_FLAG_SMBUS_OPERATION | I2C_FLAG_SMBUS_BLOCK | I2C_FLAG_SMBUS_PEC;
    Packet.Operation[0].LengthInBytes = SSIF_MAX_DATA + SMBUS_WRITE_HEADER_SIZE;
    Packet.Operation[0].Buffer        = WriteData;

    Status = BmcSsifPrivate->I2cMaster->StartRequest (BmcSsifPrivate->I2cMaster, BmcSsifPrivate->SlaveAddress, (EFI_I2C_REQUEST_PACKET *)&Packet, NULL, NULL);
    if (EFI_ERROR (Status)) {
      BmcSsifPrivate->SoftErrorCount++;
      BmcSsifPrivate->BmcStatus = BMC_SOFTFAIL;
      DEBUG ((DEBUG_ERROR, "%a: Failed to send multi part write start - %r\r\n", __FUNCTION__, Status));
      return Status;
    }

    DataLeft = RequestDataSize - (SSIF_MAX_DATA - SSIF_HEADER_SIZE);
    while (DataLeft != 0) {
      if (DataLeft <= SSIF_MAX_DATA) {
        WriteData[0] = BMC_SSIF_MULTI_PART_WRITE_CMD_END;
        DataSize     = DataLeft;
      } else {
        WriteData[0] = BMC_SSIF_MULTI_PART_WRITE_CMD_MIDDLE;
        DataSize     = SSIF_MAX_DATA;
      }

      WriteData[1] = DataSize;
      CopyMem (WriteData + SMBUS_WRITE_HEADER_SIZE, RequestData + (RequestDataSize - DataLeft), DataSize);

      Packet.OperationCount             = 1;
      Packet.Operation[0].Flags         = I2C_FLAG_SMBUS_OPERATION | I2C_FLAG_SMBUS_BLOCK | I2C_FLAG_SMBUS_PEC;
      Packet.Operation[0].LengthInBytes = DataSize + SMBUS_WRITE_HEADER_SIZE;
      Packet.Operation[0].Buffer        = WriteData;

      Status = BmcSsifPrivate->I2cMaster->StartRequest (BmcSsifPrivate->I2cMaster, BmcSsifPrivate->SlaveAddress, (EFI_I2C_REQUEST_PACKET *)&Packet, NULL, NULL);
      if (EFI_ERROR (Status)) {
        BmcSsifPrivate->SoftErrorCount++;
        BmcSsifPrivate->BmcStatus = BMC_SOFTFAIL;
        DEBUG ((DEBUG_ERROR, "%a: Failed to send multi part write continue/end - %r\r\n", __FUNCTION__, Status));
        return Status;
      }

      DataLeft -= DataSize;
    }
  }

  if (ResponseData != NULL) { \
    gBS->Stall (BMC_RETRY_DELAY);
    for (RetryCount = 0; RetryCount < BMC_RETRY_COUNT; RetryCount++) {
      // Get response data
      WriteData[0] = BMC_SSIF_SINGLE_PART_READ_CMD;

      Packet.OperationCount             = 2;
      Packet.Operation[0].Flags         = I2C_FLAG_SMBUS_OPERATION | I2C_FLAG_SMBUS_BLOCK | I2C_FLAG_SMBUS_PEC;
      Packet.Operation[0].LengthInBytes = 1;
      Packet.Operation[0].Buffer        = WriteData;
      Packet.Operation[1].Flags         = I2C_FLAG_READ;
      Packet.Operation[1].LengthInBytes = SSIF_MAX_DATA + SMBUS_READ_HEADER_SIZE;
      Packet.Operation[1].Buffer        = ReadData;

      Status = BmcSsifPrivate->I2cMaster->StartRequest (BmcSsifPrivate->I2cMaster, BmcSsifPrivate->SlaveAddress, (EFI_I2C_REQUEST_PACKET *)&Packet, NULL, NULL);
      if (EFI_ERROR (Status)) {
        BmcSsifPrivate->SoftErrorCount++;
        BmcSsifPrivate->BmcStatus = BMC_SOFTFAIL;
        DEBUG ((DEBUG_ERROR, "%a: Failed to send read command - %r\r\n", __FUNCTION__, Status));
        if (Status == EFI_NO_RESPONSE) {
          gBS->Stall (BMC_RETRY_DELAY);
          continue;
        } else {
          break;
        }
      }

      // Sanity check size
      if (ReadData[0] < SSIF_HEADER_SIZE) {
        BmcSsifPrivate->SoftErrorCount++;
        BmcSsifPrivate->BmcStatus = BMC_SOFTFAIL;
        DEBUG ((DEBUG_ERROR, "%a: Read size less then expected 0x%x\r\n", __FUNCTION__, ReadData[0]));
        return EFI_NOT_FOUND;
      }

      if ((ReadData[1] == 0x00) && (ReadData[2] == 0x01)) {
        // Multi-part read
        if (ReadData[0] < (SSIF_HEADER_SIZE + 2)) {
          BmcSsifPrivate->SoftErrorCount++;
          DEBUG ((DEBUG_ERROR, "%a: Read size less then expected 0x%x\r\n", __FUNCTION__, ReadData[0]));
          return EFI_NOT_FOUND;
        }

        if (((ReadData[3] >> 2) != (NetFunction + 1)) ||
            (ReadData[4] != Command))
        {
          BmcSsifPrivate->SoftErrorCount++;
          DEBUG ((DEBUG_ERROR, "%a: Unexpected NetFn:Command! Expected: %x:%x. Got: %x:%x\r\n", __FUNCTION__, NetFunction, Command, ReadData[3]>>2, ReadData[4]));
          return EFI_NOT_FOUND;
        }

        if (ResponseDataBufferSize < (ReadData[0] - SSIF_HEADER_SIZE - 2)) {
          BmcSsifPrivate->SoftErrorCount++;
          DEBUG ((DEBUG_ERROR, "%a: Read size returned is larger than buffer\r\n", __FUNCTION__));
          return EFI_OUT_OF_RESOURCES;
        }

        *ResponseDataSize = ReadData[0] - SSIF_HEADER_SIZE - 2;
        CopyMem (ResponseData, &ReadData[SSIF_HEADER_SIZE + 2 + 1], *ResponseDataSize);

        // Need to get the rest of the data
        ExpectedBlock = 0;
        do {
          WriteData[0]                      = BMC_SSIF_MULTI_PART_READ_CMD_MIDDLE_END;
          Packet.OperationCount             = 2;
          Packet.Operation[0].Flags         = I2C_FLAG_SMBUS_OPERATION | I2C_FLAG_SMBUS_BLOCK | I2C_FLAG_SMBUS_PEC;
          Packet.Operation[0].LengthInBytes = 1;
          Packet.Operation[0].Buffer        = WriteData;
          Packet.Operation[1].Flags         = I2C_FLAG_READ;
          Packet.Operation[1].LengthInBytes = SSIF_MAX_DATA + SMBUS_READ_HEADER_SIZE;
          Packet.Operation[1].Buffer        = ReadData;

          Status = BmcSsifPrivate->I2cMaster->StartRequest (BmcSsifPrivate->I2cMaster, BmcSsifPrivate->SlaveAddress, (EFI_I2C_REQUEST_PACKET *)&Packet, NULL, NULL);
          if (EFI_ERROR (Status)) {
            BmcSsifPrivate->SoftErrorCount++;
            DEBUG ((DEBUG_ERROR, "%a: Failed to send multi part read middle/end - %r\r\n", __FUNCTION__, Status));
            return Status;
          }

          if (ReadData[0] < 2) {
            BmcSsifPrivate->SoftErrorCount++;
            DEBUG ((DEBUG_ERROR, "%a: Read size less then expected 0x%x\r\n", __FUNCTION__, ReadData[0]));
            return EFI_NOT_FOUND;
          }

          if ((ReadData[1] == ExpectedBlock) || (ReadData[1] == 0xFF)) {
            if (ResponseDataBufferSize < (*ResponseDataSize + (ReadData[0] - 1))) {
              BmcSsifPrivate->SoftErrorCount++;
              DEBUG ((DEBUG_ERROR, "%a: Read size returned is larger than buffer\r\n", __FUNCTION__));
              return EFI_OUT_OF_RESOURCES;
            }

            CopyMem (ResponseData + *ResponseDataSize, &ReadData[2], ReadData[0]-1);
            *ResponseDataSize += (ReadData[0] - 1);
            if (ReadData[1] == 0xFF) {
              ExpectedBlock = 0xFF;
            } else {
              ExpectedBlock++;
            }
          } else {
            // Out of order block, request retry
            WriteData[0]                      = BMC_SSIF_MULTI_PART_READ_CMD_MIDDLE_RETRY;
            WriteData[1]                      = 1;
            WriteData[2]                      = ExpectedBlock;
            Packet.OperationCount             = 1;
            Packet.Operation[0].Flags         = I2C_FLAG_SMBUS_OPERATION | I2C_FLAG_SMBUS_BLOCK | I2C_FLAG_SMBUS_PEC;
            Packet.Operation[0].LengthInBytes = 3;
            Packet.Operation[0].Buffer        = WriteData;

            Status = BmcSsifPrivate->I2cMaster->StartRequest (BmcSsifPrivate->I2cMaster, BmcSsifPrivate->SlaveAddress, (EFI_I2C_REQUEST_PACKET *)&Packet, NULL, NULL);
            if (EFI_ERROR (Status)) {
              BmcSsifPrivate->SoftErrorCount++;
              DEBUG ((DEBUG_ERROR, "%a: Failed to send multi part read retry - %r\r\n", __FUNCTION__, Status));
              return Status;
            }
          }
        } while (ExpectedBlock != 0xFF);
      } else {
        // Check netfn and command
        if (((ReadData[1] >> 2) != (NetFunction + 1)) ||
            (ReadData[2] != Command))
        {
          BmcSsifPrivate->SoftErrorCount++;
          DEBUG ((DEBUG_ERROR, "%a: Unexpected NetFn:Command! Expected: %x:%x. Got: %x:%x\r\n", __FUNCTION__, NetFunction+1, Command, ReadData[1]>>2, ReadData[2]));
          return EFI_NOT_FOUND;
        }

        if (ResponseDataBufferSize < (ReadData[0] - SSIF_HEADER_SIZE)) {
          DEBUG ((DEBUG_ERROR, "%a: Read size returned is larger than buffer\r\n", __FUNCTION__));
          return EFI_OUT_OF_RESOURCES;
        }

        *ResponseDataSize = ReadData[0] - SSIF_HEADER_SIZE;
        CopyMem (ResponseData, &ReadData[SSIF_HEADER_SIZE + 1], *ResponseDataSize);
      }

      break;
    }
  }

  return Status;
}

/**
  Routine Description:
  Updates the BMC status and returns the Com Address

  @param[in] This        - Pointer to IPMI protocol instance
  @param[out] BmcStatus   - BMC status
  @param[out] ComAddress  - Com Address

  @retval EFI_SUCCESS - Success
**/
STATIC
EFI_STATUS
EFIAPI
I2cIoBmcSsifGetBmcStatus (
  IN  IPMI_TRANSPORT  *This,
  OUT BMC_STATUS      *BmcStatus,
  OUT SM_COM_ADDRESS  *ComAddress
  )
{
  BMC_SSIF_PRIVATE_DATA  *BmcSsifPrivate;

  BmcSsifPrivate = BMC_SSIF_PRIVATE_DATA_FROM_IPMI (This);

  if ((BmcSsifPrivate->BmcStatus == BMC_OK) && (BmcSsifPrivate->SoftErrorCount >= MAX_SOFT_COUNT)) {
    BmcSsifPrivate->BmcStatus = BMC_HARDFAIL;
  } else if (BmcSsifPrivate->SoftErrorCount != 0) {
    BmcSsifPrivate->BmcStatus = BMC_SOFTFAIL;
  }

  *BmcStatus                                    = BmcSsifPrivate->BmcStatus;
  ComAddress->ChannelType                       = SmBmc;
  ComAddress->Address.BmcAddress.LunAddress     = 0x0;
  ComAddress->Address.BmcAddress.SlaveAddress   = BMC_SLAVE_ADDRESS;
  ComAddress->Address.BmcAddress.ChannelAddress = 0x0;

  return EFI_SUCCESS;
}

/**
  Callback when new i2c master protocol is installed

  @param[in]  Event                Event that caused this notification.
  @param[in]  Context              Context pointer for this notification

**/
STATIC
VOID
EFIAPI
I2cIoBmcMasterNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                      Status;
  EFI_HANDLE                      Handle;
  UINTN                           HandleSize;
  EFI_I2C_MASTER_PROTOCOL         *I2cMasterProtocol;
  EFI_I2C_ENUMERATE_PROTOCOL      *I2cEnumerateProtocol;
  CONST EFI_I2C_DEVICE            *I2cDevice;
  BMC_SSIF_PRIVATE_DATA           *BmcSsifPrivate;
  IPMI_SELF_TEST_RESULT_RESPONSE  SelfTestResult;
  UINT32                          ResultSize;

  I2cMasterProtocol = NULL;
  BmcSsifPrivate    = (BMC_SSIF_PRIVATE_DATA *)Context;

  do {
    HandleSize = sizeof (Handle);
    Status     = gBS->LocateHandle (ByRegisterNotify, NULL, BmcSsifPrivate->ProtocolRegistration, &HandleSize, &Handle);
    if (EFI_ERROR (Status)) {
      return;
    }

    Status = gBS->HandleProtocol (Handle, &gEfiI2cEnumerateProtocolGuid, (VOID **)&I2cEnumerateProtocol);
    if (EFI_ERROR (Status)) {
      continue;
    }

    I2cDevice = NULL;
    do {
      Status = I2cEnumerateProtocol->Enumerate (I2cEnumerateProtocol, &I2cDevice);
      if (!EFI_ERROR (Status)) {
        if (CompareGuid (I2cDevice->DeviceGuid, &gNVIDIAI2cBmcSSIF)) {
          break;
        }
      }
    } while (!EFI_ERROR (Status));

    if (EFI_ERROR (Status)) {
      continue;
    }

    if (I2cDevice->SlaveAddressCount != 1) {
      DEBUG ((DEBUG_ERROR, "%a: BMC node with more than 1 slave address found\r\n", __FUNCTION__));
      continue;
    }

    BmcSsifPrivate->SlaveAddress = I2cDevice->SlaveAddressArray[0];

    Status = gBS->HandleProtocol (Handle, &gEfiI2cMasterProtocolGuid, (VOID **)&I2cMasterProtocol);
    if (EFI_ERROR (Status)) {
      I2cMasterProtocol = NULL;
    }
  } while (I2cMasterProtocol == NULL);

  BmcSsifPrivate->Signature                       = BMC_SSIF_SIGNATURE;
  BmcSsifPrivate->I2cMaster                       = I2cMasterProtocol;
  BmcSsifPrivate->IpmiTransport.IpmiSubmitCommand = I2cIoBmcSsifIpmiSubmitCommand;
  BmcSsifPrivate->IpmiTransport.GetBmcStatus      = I2cIoBmcSsifGetBmcStatus;

  gBS->CloseEvent (Event);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gIpmiTransportProtocolGuid,
                  &BmcSsifPrivate->IpmiTransport,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install Ipmi protocol - %r\r\n", __FUNCTION__, Status));
    return;
  }

  ResultSize = sizeof (SelfTestResult);
  Status     = BmcSsifPrivate->IpmiTransport.IpmiSubmitCommand (
                                               &BmcSsifPrivate->IpmiTransport,
                                               IPMI_NETFN_APP,
                                               0,
                                               IPMI_APP_GET_SELFTEST_RESULTS,
                                               NULL,
                                               0,
                                               (UINT8 *)&SelfTestResult,
                                               &ResultSize
                                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get BMC SelfTest - %r\r\n", __FUNCTION__, Status));
    BmcSsifPrivate->BmcStatus = BMC_HARDFAIL;
    Status                    = EFI_SUCCESS;
  } else {
    if ((SelfTestResult.Result != IPMI_APP_SELFTEST_NO_ERROR) &&
        (SelfTestResult.Result != IPMI_APP_SELFTEST_NOT_IMPLEMENTED))
    {
      DEBUG ((DEBUG_ERROR, "%a: BMC Self test failed - 0x%02x\r\n", __FUNCTION__, SelfTestResult.Result));
      BmcSsifPrivate->BmcStatus = BMC_HARDFAIL;
    }
  }
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
I2cIoBmcSsifDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  BMC_SSIF_PRIVATE_DATA  *BmcSsifPrivate;

  BmcSsifPrivate = AllocateZeroPool (sizeof (BMC_SSIF_PRIVATE_DATA));
  if (BmcSsifPrivate == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  BmcSsifPrivate->ProtocolEvent = EfiCreateProtocolNotifyEvent (
                                    &gEfiI2cMasterProtocolGuid,
                                    TPL_CALLBACK,
                                    I2cIoBmcMasterNotify,
                                    BmcSsifPrivate,
                                    &BmcSsifPrivate->ProtocolRegistration
                                    );
  if (BmcSsifPrivate->ProtocolEvent == NULL) {
    FreePool (BmcSsifPrivate);
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}
