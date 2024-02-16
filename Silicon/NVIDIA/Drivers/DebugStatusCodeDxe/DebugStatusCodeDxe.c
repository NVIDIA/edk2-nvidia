/** @file

  Status code Driver via debug lib

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2006 - 2020, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Base.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/ReportStatusCodeLib.h>

#include <Protocol/ReportStatusCodeHandler.h>

#include <Guid/StatusCodeDataTypeId.h>
#include <Guid/StatusCodeDataTypeDebug.h>

STATIC BOOLEAN  mDisableDebugStatus = FALSE;

STATIC
EFI_STATUS
DebugStatusCodeCallback (
  IN EFI_STATUS_CODE_TYPE   CodeType,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN UINT32                 Instance,
  IN EFI_GUID               *CallerId,
  IN EFI_STATUS_CODE_DATA   *Data
  )
{
  CHAR8      *Filename;
  CHAR8      *Description;
  CHAR8      *Format;
  UINT32     ErrorLevel;
  UINT32     LineNumber;
  BASE_LIST  Marker;

  if (mDisableDebugStatus) {
    return EFI_UNSUPPORTED;
  }

  if ((Data != NULL) &&
      ReportStatusCodeExtractAssertInfo (CodeType, Value, Data, &Filename, &Description, &LineNumber))
  {
    //
    // Print ASSERT() information into output buffer.
    //
    DEBUG ((DEBUG_ERROR, "\r\nDXE_ASSERT!: %a (%u): %a\r\n", Filename, LineNumber, Description));
  } else if ((Data != NULL) &&
             ReportStatusCodeExtractDebugInfo (Data, &ErrorLevel, &Marker, &Format))
  {
    DebugBPrint (ErrorLevel, Format, Marker);
  } else {
    if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_ERROR_CODE) {
      ErrorLevel = DEBUG_ERROR;
      DEBUG ((ErrorLevel, "ERROR: C%08x:", CodeType));
    } else if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_PROGRESS_CODE) {
      if (Value == (EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_BS_PC_EXIT_BOOT_SERVICES)) {
        mDisableDebugStatus = TRUE;
        ErrorLevel          = DEBUG_ERROR;
      } else {
        ErrorLevel = DEBUG_INFO;
      }

      DEBUG ((ErrorLevel, "PROGRESS CODE: "));
    } else {
      ErrorLevel = DEBUG_ERROR;
      DEBUG ((ErrorLevel, "Undefined: C%08x:", CodeType));
    }

    DEBUG ((ErrorLevel, "V%08x I%x", Value, Instance));
    if (CallerId != NULL) {
      DEBUG ((ErrorLevel, " %g", CallerId));
    }

    if (Data != NULL) {
      DEBUG ((ErrorLevel, " %p", Data));
    }

    DEBUG ((ErrorLevel, "\r\n"));
  }

  return EFI_SUCCESS;
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
DebugStatusCodeDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_RSC_HANDLER_PROTOCOL  *RscHandler;

  RscHandler = NULL;

  Status = gBS->LocateProtocol (&gEfiRscHandlerProtocolGuid, NULL, (VOID **)&RscHandler);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = RscHandler->Register (DebugStatusCodeCallback, TPL_CALLBACK);
  return Status;
}
