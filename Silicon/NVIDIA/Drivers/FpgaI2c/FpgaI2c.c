/** @file

  FPGA I2C Driver

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/I2cIo.h>

#define NV_FPGA_I2C_POST_STATUS_REG  0x01
#define NV_FPGA_I2C_POST_END_STATUS  0x01

STATIC
VOID
EFIAPI
EndOfPostSignalToFpga (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS              Status;
  EFI_HANDLE              *Handles;
  UINTN                   NoHandles;
  EFI_I2C_IO_PROTOCOL     *I2cIo;
  UINTN                   Index;
  EFI_I2C_REQUEST_PACKET  *RequestPacket;
  UINT8                   Address;
  UINT8                   Data;
  UINT8                   WriteData[2];

  gBS->CloseEvent (Event);

  Handles       = NULL;
  RequestPacket = AllocateZeroPool (sizeof (EFI_I2C_REQUEST_PACKET) + sizeof (EFI_I2C_OPERATION));
  if (RequestPacket == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool failed\n", __FUNCTION__));
    return;
  }

  //
  // Try to locate all I2C IO protocol handles and find the FPGA I2C.
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiI2cIoProtocolGuid,
                  NULL,
                  &NoHandles,
                  &Handles
                  );
  if ((EFI_ERROR (Status)) || (Handles == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: LocateHandleBuffer failed\n", __FUNCTION__));
    FreePool (RequestPacket);
    return;
  }

  for (Index = 0; Index < NoHandles; Index++) {
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiI2cIoProtocolGuid,
                    (VOID **)&I2cIo
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (CompareGuid (&gNVIDIAI2cFpga, I2cIo->DeviceGuid)) {
      //
      // Set I2C Address 0x11 offset 0x01 to 0x01
      //
      WriteData[0]                              = NV_FPGA_I2C_POST_STATUS_REG;
      WriteData[1]                              = NV_FPGA_I2C_POST_END_STATUS;
      RequestPacket->OperationCount             = 1;
      RequestPacket->Operation[0].Buffer        = (VOID *)WriteData;
      RequestPacket->Operation[0].LengthInBytes = sizeof (WriteData);
      RequestPacket->Operation[0].Flags         = 0;
      Status                                    = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: I2c write data %r\n", __FUNCTION__, Status));
      } else {
        //
        // Read back and check the status.
        //
        Address                                   = NV_FPGA_I2C_POST_STATUS_REG;
        RequestPacket->OperationCount             = 2;
        RequestPacket->Operation[0].Buffer        = (VOID *)&Address;
        RequestPacket->Operation[0].LengthInBytes = sizeof (Address);
        RequestPacket->Operation[0].Flags         = 0;
        RequestPacket->Operation[1].Buffer        = (VOID *)&Data;
        RequestPacket->Operation[1].LengthInBytes = sizeof (Data);
        RequestPacket->Operation[1].Flags         = I2C_FLAG_READ;
        Status                                    = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: I2c read data %r\n", __FUNCTION__, Status));
        } else if (Data != NV_FPGA_I2C_POST_END_STATUS) {
          DEBUG ((DEBUG_ERROR, "%a: FPGA End of POST is not set\n", __FUNCTION__));
        }
      }

      break;
    }
  }

  FreePool (Handles);
  FreePool (RequestPacket);

  return;
}

/**
  The user Entry Point for module. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
FpgaI2cInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   Event;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  EndOfPostSignalToFpga,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &Event
                  );

  return Status;
}
