/** @file

  FwVariableLib - Firmware variable support library

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/FwVariableLib.h>

#include <OemStatusCodes.h>

#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

/**
  Delete all Firmware Variables

  @retval         EFI_SUCCESS          All variables deleted
  @retval         EFI_OUT_OF_RESOURCES Unable to allocate space for variable names
  @retval         others               Error deleting or getting next variable

**/
EFI_STATUS
EFIAPI
FwVariableDeleteAll (
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  VarGetStatus;
  EFI_STATUS  VarDeleteStatus;
  CHAR16      *CurrentName;
  CHAR16      *NextName;
  EFI_GUID    CurrentGuid;
  EFI_GUID    NextGuid;
  UINTN       NameSize;

  CurrentName = NULL;
  NextName    = NULL;

  CurrentName = AllocateZeroPool (MAX_VARIABLE_NAME);
  if (CurrentName == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  NextName = AllocateZeroPool (MAX_VARIABLE_NAME);
  if (NextName == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
    EFI_PROGRESS_CODE | EFI_OEM_PROGRESS_MAJOR,
    EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_DXE_BS_PC_CONFIG_RESET,
    OEM_PC_DESC_RESET_NS_VARIABLES,
    sizeof (OEM_PC_DESC_RESET_NS_VARIABLES)
    );

  NameSize     = MAX_VARIABLE_NAME;
  VarGetStatus = gRT->GetNextVariableName (&NameSize, NextName, &NextGuid);

  while (!EFI_ERROR (VarGetStatus)) {
    CopyMem (CurrentName, NextName, NameSize);
    CopyGuid (&CurrentGuid, &NextGuid);

    NameSize     = MAX_VARIABLE_NAME;
    VarGetStatus = gRT->GetNextVariableName (&NameSize, NextName, &NextGuid);

    // Delete Current Name variable
    VarDeleteStatus = gRT->SetVariable (
                             CurrentName,
                             &CurrentGuid,
                             0,
                             0,
                             NULL
                             );
    DEBUG ((DEBUG_ERROR, "Delete Variable %g:%s %r\r\n", &CurrentGuid, CurrentName, VarDeleteStatus));
  }

  if (EFI_ERROR (VarGetStatus) && (VarGetStatus != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "Get Next Variable %g:%s %r\r\n", &CurrentGuid, CurrentName, VarGetStatus));
    Status = VarGetStatus;
    goto CleanupAndReturn;
  }

  if (EFI_ERROR (VarDeleteStatus) && (VarDeleteStatus != EFI_ACCESS_DENIED)) {
    Status = VarDeleteStatus;
    goto CleanupAndReturn;
  }

  Status = EFI_SUCCESS;

CleanupAndReturn:
  FREE_NON_NULL (CurrentName);
  FREE_NON_NULL (NextName);

  return Status;
}
