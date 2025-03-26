/** @file

  Send IPMI command to notify BMC upon reset

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Base.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiRuntimeLib.h>

#include <Protocol/I2cMaster.h>
#include <Protocol/I2cEnumerate.h>
#include <Protocol/ReportStatusCodeHandler.h>

#include <IndustryStandard/Ipmi.h>

#define BMC_SSIF_SINGLE_PART_WRITE_CMD  0x2

#define ARM_IPMI_GROUP_EXTENSION              0xAE
#define ARM_SBMR_SEND_PROGRESS_CODE_CMD       0x2
#define ARM_SBMR_SEND_PROGRESS_CODE_REQ_SIZE  10

#define SSIF_MAX_DATA            0x20
#define SSIF_HEADER_SIZE         2
#define SMBUS_WRITE_HEADER_SIZE  2
#define IPMI_DATA_OFFSET         (SMBUS_WRITE_HEADER_SIZE + SSIF_HEADER_SIZE)

typedef struct {
  UINTN                OperationCount;
  EFI_I2C_OPERATION    Operation[2];
} SSIF_REQUEST_PACKET;

STATIC VOID                     *mI2cMasterSearchToken     = NULL;
STATIC EFI_I2C_MASTER_PROTOCOL  *mI2cMaster                = NULL;
STATIC UINT16                   mSlaveAddr                 = 0;
STATIC EFI_EVENT                mVirtualAddressChangeEvent = NULL;
STATIC BOOLEAN                  mIsRuntime                 = FALSE;

/**
  Fixup pointers for runtime.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
VirtualAddressChangeCallBack (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EfiConvertPointer (0x0, (VOID **)&mI2cMaster);
}

/**
  Handler of report status code to notify BMC upon reset

  @param[in]  CodeType     Indicates the type of status code being reported.
  @param[in]  Value        Describes the current status of a hardware or software entity.
                           This included information about the class and subclass that is used to
                           classify the entity as well as an operation.
  @param[in]  Instance     The enumeration of a hardware or software entity within
                           the system. Valid instance numbers start with 1.
  @param[in]  CallerId     This optional parameter may be used to identify the caller.
                           This parameter allows the status code driver to apply different rules to
                           different callers.
  @param[in]  Data         This optional parameter may be used to pass additional data.

  @retval EFI_SUCCESS      Status code is what we expected.
  @retval EFI_UNSUPPORTED  Status code not supported.
**/
STATIC
EFI_STATUS
ResetNotifyStatusCodeCallback (
  IN EFI_STATUS_CODE_TYPE   CodeType,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN UINT32                 Instance,
  IN EFI_GUID               *CallerId,
  IN EFI_STATUS_CODE_DATA   *Data
  )
{
  EFI_STATUS           Status;
  UINT8                NetFunction = IPMI_NETFN_GROUP_EXT;
  UINT8                Command     = ARM_SBMR_SEND_PROGRESS_CODE_CMD;
  UINT8                Lun         = 0;
  UINT8                WriteData[SSIF_MAX_DATA + SMBUS_WRITE_HEADER_SIZE];
  SSIF_REQUEST_PACKET  Packet;
  UINT8                DataSize = ARM_SBMR_SEND_PROGRESS_CODE_REQ_SIZE;

  if (((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_PROGRESS_CODE) &&
      (Value == (EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_BS_PC_EXIT_BOOT_SERVICES)))
  {
    mIsRuntime = TRUE;
  }

  if (!mIsRuntime ||
      ((CodeType & EFI_STATUS_CODE_TYPE_MASK) != EFI_PROGRESS_CODE) ||
      (Value != (EFI_SOFTWARE_EFI_RUNTIME_SERVICE | EFI_SW_RS_PC_RESET_SYSTEM)))
  {
    return EFI_NOT_READY;
  }

  if (mI2cMaster == NULL) {
    return EFI_DEVICE_ERROR;
  }

  //
  // Send SBMR status code EFI_SW_RS_PC_RESET_SYSTEM when OS requests system reset
  //
  WriteData[0]                           = BMC_SSIF_SINGLE_PART_WRITE_CMD;
  WriteData[1]                           = DataSize + SSIF_HEADER_SIZE;
  WriteData[SMBUS_WRITE_HEADER_SIZE + 0] = NetFunction << 2 | (Lun & 0x3);
  WriteData[SMBUS_WRITE_HEADER_SIZE + 1] = Command;

  WriteData[IPMI_DATA_OFFSET] = ARM_IPMI_GROUP_EXTENSION;
  CopyMem (&WriteData[IPMI_DATA_OFFSET + 1], &CodeType, sizeof (CodeType));
  CopyMem (&WriteData[IPMI_DATA_OFFSET + 5], &Value, sizeof (Value));
  WriteData[IPMI_DATA_OFFSET + 9] = 0x00;

  Packet.OperationCount             = 1;
  Packet.Operation[0].Flags         = I2C_FLAG_SMBUS_OPERATION | I2C_FLAG_SMBUS_BLOCK | I2C_FLAG_SMBUS_PEC;
  Packet.Operation[0].LengthInBytes = DataSize + SSIF_HEADER_SIZE + SMBUS_WRITE_HEADER_SIZE;
  Packet.Operation[0].Buffer        = WriteData;

  Status = mI2cMaster->StartRequest (mI2cMaster, mSlaveAddr, (EFI_I2C_REQUEST_PACKET *)&Packet, NULL, NULL);

  return Status;
}

/**
  Get I2C protocol for this bus

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
STATIC
VOID
EFIAPI
I2cMasterRegistrationEvent (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS                  Status;
  EFI_HANDLE                  Handle;
  UINTN                       HandleSize;
  EFI_I2C_MASTER_PROTOCOL     *I2cMasterProtocol = NULL;
  EFI_I2C_ENUMERATE_PROTOCOL  *I2cEnumerateProtocol;
  CONST EFI_I2C_DEVICE        *I2cDevice;

  //
  // Try to connect the newly registered driver to our handle.
  //
  do {
    HandleSize = sizeof (Handle);
    Status     = gBS->LocateHandle (ByRegisterNotify, NULL, mI2cMasterSearchToken, &HandleSize, &Handle);
    if (EFI_ERROR (Status)) {
      return;
    }

    Status = gBS->HandleProtocol (Handle, &gEfiI2cEnumerateProtocolGuid, (VOID **)&I2cEnumerateProtocol);
    if (EFI_ERROR (Status)) {
      return;
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
      return;
    }

    if (I2cDevice->SlaveAddressCount != 1) {
      DEBUG ((DEBUG_ERROR, "%a: BMC node with more than 1 slave address found\r\n", __FUNCTION__));
      return;
    }

    mSlaveAddr = I2cDevice->SlaveAddressArray[0];

    Status = gBS->HandleProtocol (Handle, &gEfiI2cMasterProtocolGuid, (VOID **)&I2cMasterProtocol);
    if (EFI_ERROR (Status)) {
      I2cMasterProtocol = NULL;
    }
  } while (I2cMasterProtocol == NULL);

  gBS->CloseEvent (Event);

  mI2cMaster = I2cMasterProtocol;
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
ResetNotifyRuntimeDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_EVENT                 Event;
  EFI_STATUS                Status;
  EFI_RSC_HANDLER_PROTOCOL  *RscHandler;

  RscHandler = NULL;

  //
  // Register a protocol registration notification callback on the I2C Io
  // protocol. This will notify us even if the protocol instance we are looking
  // for has already been installed.
  //
  Event = EfiCreateProtocolNotifyEvent (
            &gEfiI2cMasterProtocolGuid,
            TPL_CALLBACK,
            I2cMasterRegistrationEvent,
            NULL,
            &mI2cMasterSearchToken
            );
  if (Event == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create protocol notify event\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  VirtualAddressChangeCallBack,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mVirtualAddressChangeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to create virtual address change event - %r\n",
      __FUNCTION__,
      Status
      ));
    goto ErrorExit;
  }

  Status = gBS->LocateProtocol (&gEfiRscHandlerProtocolGuid, NULL, (VOID **)&RscHandler);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to locate ReportStatusCodeHandler protocol - %r\n",
      __FUNCTION__,
      Status
      ));
    goto ErrorExit;
  }

  Status = RscHandler->Register (ResetNotifyStatusCodeCallback, TPL_CALLBACK);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to register ResetNotifyStatusCodeCallback - %r\n",
      __FUNCTION__,
      Status
      ));
    goto ErrorExit;
  }

  return EFI_SUCCESS;

ErrorExit:
  if (mVirtualAddressChangeEvent != NULL) {
    gBS->CloseEvent (mVirtualAddressChangeEvent);
    mVirtualAddressChangeEvent = NULL;
  }

  if (Event != NULL) {
    gBS->CloseEvent (Event);
  }

  return Status;
}
