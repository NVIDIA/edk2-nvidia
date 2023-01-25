/** @file

  OEM Status code handler to log addtional data as string

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DebugPrintErrorLevelLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <IndustryStandard/Ipmi.h>
#include <Protocol/ReportStatusCodeHandler.h>

#include <OemStatusCodes.h>
#include "OemDescStatusCodeDxe.h"

STATIC EFI_RSC_HANDLER_PROTOCOL  *mRscHandler           = NULL;
STATIC EFI_EVENT                 mExitBootServicesEvent = NULL;
STATIC BOOLEAN                   mEnableOemDesc         = FALSE;

/**
  Checks if the data is a string and returns the length of it

  @param Data        - pointer to the data source
  @param DataSize    - data size in bytes
**/
STATIC
UINT16
OemDescLength (
  IN     CONST UINT8   *Data,
  IN           UINT16  DataSize
  )
{
  UINT16  NumChars = 0;

  while ((*Data >= ' ') && (*Data <= '~') && (NumChars <= DataSize)) {
    Data++;
    NumChars++;
  }

  return NumChars;
}

/**
  OEM handler of report status code that sends additional data to BMC as text.

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
EFIAPI
OemDescStatusCodeCallback (
  IN EFI_STATUS_CODE_TYPE   CodeType,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN UINT32                 Instance,
  IN EFI_GUID               *CallerId,
  IN EFI_STATUS_CODE_DATA   *Data
  )
{
  EFI_STATUS                   Status;
  IPMI_OEM_SEND_DESC_REQ_DATA  *RequestData = NULL;
  IPMI_OEM_SEND_DESC_RSP_DATA  ResponseData;
  UINT32                       RequestDataSize  = 0;
  UINT32                       ResponseDataSize = sizeof (ResponseData);
  UINT8                        *DataPtr         = (UINT8 *)(Data + 1);
  UINT16                       DataSize         = Data->Size;
  UINTN                        ErrorLevel       = 0;
  CHAR16                       *Str;
  CHAR8                        *DevicePathStr = NULL;
  INT32                        NumRetries;

  if (!mEnableOemDesc) {
    return EFI_UNSUPPORTED;
  }

  if (DataSize == 0) {
    return EFI_SUCCESS;
  }

  ASSERT (DataSize <= IPMI_OEM_DESC_MAX_LEN);
  if (DataSize > IPMI_OEM_DESC_MAX_LEN) {
    DataSize = IPMI_OEM_DESC_MAX_LEN;
  }

  //
  // Use PcdDebugPrintErrorLevel to select which description to log
  //
  if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_ERROR_CODE) {
    if ((CodeType & EFI_STATUS_CODE_SEVERITY_MASK) == EFI_ERROR_MINOR) {
      ErrorLevel = DEBUG_WARN;
    } else {
      ErrorLevel = DEBUG_ERROR;
    }
  } else if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_PROGRESS_CODE) {
    if ((CodeType & EFI_STATUS_CODE_SEVERITY_MASK) == EFI_OEM_PROGRESS_MINOR) {
      ErrorLevel = DEBUG_INFO;
    } else {
      // Escalate Progress Code logging level if it is important
      ErrorLevel = DEBUG_ERROR;
    }
  } else if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_DEBUG_CODE) {
    ErrorLevel = DEBUG_VERBOSE;
  }

  if ((ErrorLevel & GetDebugPrintErrorLevel ()) == 0) {
    return EFI_SUCCESS;
  }

  //
  // If the data is binary, check if it is device path and log it as text
  //
  if (IsDevicePathValid ((EFI_DEVICE_PATH_PROTOCOL *)DataPtr, DataSize)) {
    Str = ConvertDevicePathToText ((EFI_DEVICE_PATH_PROTOCOL *)DataPtr, TRUE, FALSE);
    if (Str == NULL) {
      ASSERT (FALSE);
      return EFI_OUT_OF_RESOURCES;
    }

    DevicePathStr = AllocatePool (IPMI_OEM_DESC_MAX_LEN);
    if (DevicePathStr == NULL) {
      ASSERT (FALSE);
      return EFI_OUT_OF_RESOURCES;
    }

    Status = UnicodeStrToAsciiStrS (Str, DevicePathStr, IPMI_OEM_DESC_MAX_LEN);
    if (EFI_ERROR (Status)) {
      ASSERT (FALSE);
      goto Exit;
    }

    DataPtr  = (UINT8 *)DevicePathStr;
    DataSize = IPMI_OEM_DESC_MAX_LEN;
  }

  //
  // Verify the data and only take printable characters
  //
  DataSize = OemDescLength (DataPtr, DataSize);
  if (DataSize == 0) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  //
  // Populate IPMI request packet
  //
  RequestDataSize = sizeof (IPMI_OEM_SEND_DESC_REQ_DATA) + DataSize;
  RequestData     = AllocateZeroPool (RequestDataSize);
  if (RequestData == NULL) {
    ASSERT (FALSE);
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  RequestData->EfiStatusCodeType  = CodeType;
  RequestData->EfiStatusCodeValue = Value;
  CopyMem (RequestData->Description, DataPtr, DataSize);

  //
  // Retry on errors for the important messages
  //
  if (ErrorLevel == DEBUG_ERROR) {
    NumRetries = 5;
  } else {
    NumRetries = 0;
  }

  //
  // Send IPMI packet to BMC
  //
  do {
    Status = IpmiSubmitCommand (
               IPMI_NETFN_OEM,
               IPMI_CMD_OEM_SEND_DESCRIPTION,
               (UINT8 *)RequestData,
               RequestDataSize,
               (UINT8 *)&ResponseData,
               &ResponseDataSize
               );
  } while (EFI_ERROR (Status) && (NumRetries-- > 0));

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send IPMI command - %r\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  if (ResponseDataSize != sizeof (ResponseData)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed unexpected response size, Got: %d, Expected: %d\r\n",
      __FUNCTION__,
      ResponseDataSize,
      sizeof (ResponseData)
      ));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  if (ResponseData.CompletionCode == IPMI_COMP_CODE_INVALID_COMMAND) {
    DEBUG ((DEBUG_ERROR, "%a: BMC does not support status codes, disabling\r\n", __FUNCTION__));
    mEnableOemDesc = FALSE;
    Status         = EFI_UNSUPPORTED;
    goto Exit;
  } else if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed unexpected command completion code, Got: %x, Expected: %x\r\n",
      __FUNCTION__,
      ResponseData.CompletionCode,
      IPMI_COMP_CODE_NORMAL
      ));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  Status = EFI_SUCCESS;

Exit:
  if (DevicePathStr != NULL) {
    FreePool (DevicePathStr);
  }

  if (RequestData != NULL) {
    FreePool (RequestData);
  }

  return Status;
}

/**
  Disable OEM status code callback

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
STATIC
VOID
EFIAPI
OemDescStatusCodeDisable (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  mEnableOemDesc = FALSE;
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
OemDescStatusCodeDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // IpmiBaseLib does not have constructor, need to initialize it manually
  //
  Status = InitializeIpmiBase ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Register to handle OEM status code handling
  //
  Status = gBS->LocateProtocol (&gEfiRscHandlerProtocolGuid, NULL, (VOID **)&mRscHandler);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = mRscHandler->Register (OemDescStatusCodeCallback, TPL_CALLBACK);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Register to disable OEM status code handling at ExitBootServices
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  OemDescStatusCodeDisable,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &mExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to create exit boot services event\r\n", __FUNCTION__));
    return Status;
  }

  //
  // TODO: Register to disable OEM status code handling when Redfish is online
  //

  mEnableOemDesc = TRUE;

  return EFI_SUCCESS;
}
