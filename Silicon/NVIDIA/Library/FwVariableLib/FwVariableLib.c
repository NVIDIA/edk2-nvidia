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
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/FwVariableLib.h>
#include <Protocol/MmCommunication2.h>
#include <Guid/NVIDIAMmMb1Record.h>

#include <OemStatusCodes.h>

#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

STATIC EFI_MM_COMMUNICATION2_PROTOCOL  *mMmCommunicate2        = NULL;
STATIC VOID                            *mMmCommunicationBuffer = NULL;

/**
  Erase the Mb1 Variables partition.

  @retval         EFI_SUCCESS         Partition Erased.
  @retval         others              Error Erasing Partition.

**/
EFI_STATUS
EFIAPI
EraseMb1VariablePartition (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_MM_COMMUNICATE_HEADER     *Header;
  NVIDIA_MM_MB1_RECORD_PAYLOAD  *Payload;
  UINTN                         MmBufferSize;

  MmBufferSize = sizeof (EFI_MM_COMMUNICATE_HEADER) + sizeof (NVIDIA_MM_MB1_RECORD_PAYLOAD) - 1;

  if (mMmCommunicate2 == NULL) {
    Status = gBS->LocateProtocol (&gEfiMmCommunication2ProtocolGuid, NULL, (VOID **)&mMmCommunicate2);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (mMmCommunicationBuffer == NULL) {
      mMmCommunicationBuffer = AllocateZeroPool (MmBufferSize);
      if (mMmCommunicationBuffer == NULL) {
        mMmCommunicate2 = NULL;
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate buffer \r\n", __FUNCTION__));
        return EFI_OUT_OF_RESOURCES;
      }
    }

    Header = (EFI_MM_COMMUNICATE_HEADER *)mMmCommunicationBuffer;
    CopyGuid (&Header->HeaderGuid, &gNVIDIAMmMb1RecordGuid);
    Header->MessageLength = sizeof (NVIDIA_MM_MB1_RECORD_PAYLOAD);
  }

  Header  = (EFI_MM_COMMUNICATE_HEADER *)mMmCommunicationBuffer;
  Payload = (NVIDIA_MM_MB1_RECORD_PAYLOAD *)&Header->Data;

  Payload->Command = NVIDIA_MM_MB1_ERASE_PARTITION;

  Status = mMmCommunicate2->Communicate (
                              mMmCommunicate2,
                              mMmCommunicationBuffer,
                              mMmCommunicationBuffer,
                              &MmBufferSize
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to dispatch Mb1 MM command %r \r\n", __FUNCTION__, Status));
    return Status;
  }

  if (EFI_ERROR (Payload->Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error in Mb1 MM command %r \r\n", __FUNCTION__, Payload->Status));
    return Payload->Status;
  }

  return Status;
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

  Status = EraseMb1VariablePartition ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Erase Mb1 Var Partition %r\n",
      __FUNCTION__,
      Status
      ));
    goto CleanupAndReturn;
  }

  Status = EFI_SUCCESS;

CleanupAndReturn:
  FREE_NON_NULL (CurrentName);
  FREE_NON_NULL (NextName);

  return Status;
}
