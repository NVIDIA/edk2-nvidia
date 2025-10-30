/** @file

  Driver that locks all variables at runtime

  Copyright (c) 2025, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>

#include <Protocol/MmCommunication2.h>
#include <Protocol/FirmwareVolume2.h>

#include <Guid/EventGroup.h>
#include <Guid/VarCheckLockAllMmi.h>

#define  MM_COMM_BUFFER_SIZE    1024
#define  MAX_VARIABLE_NAME_LEN  256

typedef struct {
  CHAR16      *VariableName;
  EFI_GUID    *VendorGuid;
} VAR_LOCK_EXCEPTION_ENTRY;

STATIC EFI_MM_COMMUNICATION2_PROTOCOL  *mMmCommunication2       = NULL;
STATIC EFI_EVENT                       mReadyToBootEvent        = NULL;
STATIC UINT8                           *mVariableBuffer         = NULL;
STATIC UINT8                           *mVariableBufferPhysical = NULL;

STATIC VAR_LOCK_EXCEPTION_ENTRY  mVarLockExceptionList[] = {
  { L"RTC_OFFSET", &gNVIDIATokenSpaceGuid },
};

/**
  Initialize the communicate buffer using DataSize and Function.

  @param[out]      DataPtr          Points to the data in the communicate buffer.
  @param[in]       DataSize         The data size to send to MM.
  @param[in]       Function         The function number to initialize the communicate header.

  @retval EFI_INVALID_PARAMETER     The data size is too big.
  @retval EFI_SUCCESS               Find the specified variable.

**/
EFI_STATUS
InitCommunicateBuffer (
  OUT     VOID   **DataPtr OPTIONAL,
  IN      UINTN  DataSize,
  IN      UINTN  Function
  )
{
  EFI_MM_COMMUNICATE_HEADER          *MmCommunicateHeader;
  MM_VAR_CHECK_LOCK_ALL_COMM_HEADER  *FuncHeader;

  if (DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) +
      sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_HEADER) > MM_COMM_BUFFER_SIZE)
  {
    return EFI_INVALID_PARAMETER;
  }

  MmCommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)mVariableBuffer;
  CopyGuid (&MmCommunicateHeader->HeaderGuid, &gVarCheckLockAllGuid);
  MmCommunicateHeader->MessageLength = DataSize + sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_HEADER);

  FuncHeader           = (MM_VAR_CHECK_LOCK_ALL_COMM_HEADER *)MmCommunicateHeader->Data;
  FuncHeader->Function = Function;
  if (DataPtr != NULL) {
    *DataPtr = FuncHeader + 1;
  }

  return EFI_SUCCESS;
}

/**
  Send the data in communicate buffer to MM.

  @param[in]   DataSize               This size of the function header and the data.

  @retval      EFI_SUCCESS            Success is returned from the functin in MM.
  @retval      Others                 Failure is returned from the function in MM.

**/
EFI_STATUS
SendCommunicateBuffer (
  IN      UINTN  DataSize
  )
{
  EFI_STATUS                         Status;
  UINTN                              CommSize;
  EFI_MM_COMMUNICATE_HEADER          *MmCommunicateHeader;
  MM_VAR_CHECK_LOCK_ALL_COMM_HEADER  *FuncHeader;

  CommSize = DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) +
             sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_HEADER);
  Status = mMmCommunication2->Communicate (
                                mMmCommunication2,
                                mVariableBufferPhysical,
                                mVariableBuffer,
                                &CommSize
                                );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmCommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)mVariableBuffer;
  FuncHeader          = (MM_VAR_CHECK_LOCK_ALL_COMM_HEADER *)MmCommunicateHeader->Data;
  return FuncHeader->ReturnStatus;
}

/**
  Check if current BootCurrent variable is UiApp (Setup Menu).

  @retval  TRUE         BootCurrent is UiApp.
  @retval  FALSE        BootCurrent is not UiApp.
**/
STATIC
BOOLEAN
IsBootingToSetupMenu (
  VOID
  )
{
  UINTN                     VarSize;
  UINT16                    BootCurrent;
  CHAR16                    BootOptionName[16];
  UINT8                     *BootOption;
  UINT8                     *Ptr;
  BOOLEAN                   Result;
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *TempDevicePath;
  EFI_HANDLE                FvHandle;
  VOID                      *NameGuid;

  BootOption = NULL;
  Result     = FALSE;

  //
  // Get BootCurrent variable
  //
  VarSize = sizeof (UINT16);
  Status  = gRT->GetVariable (
                   L"BootCurrent",
                   &gEfiGlobalVariableGuid,
                   NULL,
                   &VarSize,
                   &BootCurrent
                   );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  //
  // Create boot option Bootxxxx from BootCurrent
  //
  UnicodeSPrint (BootOptionName, sizeof (BootOptionName), L"Boot%04x", BootCurrent);

  GetEfiGlobalVariable2 (BootOptionName, (VOID **)&BootOption, &VarSize);
  if ((BootOption == NULL) || (VarSize == 0)) {
    return FALSE;
  }

  //
  // Parse the boot option to get the device path
  // Boot option format: Attributes(4) + FilePathListLength(2) + Description(variable) + DevicePath(variable)
  //
  Ptr            = BootOption;
  Ptr           += sizeof (UINT32);           // Skip Attributes
  Ptr           += sizeof (UINT16);           // Skip FilePathListLength
  Ptr           += StrSize ((CHAR16 *)Ptr);   // Skip Description
  TempDevicePath = (EFI_DEVICE_PATH_PROTOCOL *)Ptr;

  //
  // Check if this device path points to UiApp
  //
  Status = gBS->LocateDevicePath (&gEfiFirmwareVolume2ProtocolGuid, &TempDevicePath, &FvHandle);
  if (!EFI_ERROR (Status)) {
    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode ((CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)TempDevicePath);
    if (NameGuid != NULL) {
      Result = CompareGuid (NameGuid, &gUiAppFileGuid);
    }
  }

  if (Result) {
    DEBUG ((DEBUG_INFO, "%a: Detected boot to UiApp (Setup Menu)\n", __FUNCTION__));
  }

  if (BootOption != NULL) {
    FreePool (BootOption);
  }

  return Result;
}

/**
  Start locking variables at ReadyToBoot

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
STATIC
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  //
  // Check if we're booting to Setup Menu - if so, don't lock variables
  //
  if (IsBootingToSetupMenu ()) {
    DEBUG ((DEBUG_INFO, "%a: Booting to Setup Menu - skipping variable lock\n", __FUNCTION__));
    return;
  }

  DEBUG ((DEBUG_ERROR, "%a: *** READY TO BOOT - ACTIVATING VARIABLE LOCK ***\n", __FUNCTION__));

  gBS->CloseEvent (Event);

  //
  // Start locking all variables after ReadyToBoot event
  //
  Status = InitCommunicateBuffer (NULL, 0, MM_VAR_CHECK_LOCK_ALL_ACTIVATE);
  DEBUG ((DEBUG_INFO, "%a: InitCommunicateBuffer returned %r\n", __FUNCTION__, Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to initialize MM communication buffer for variable lock activation: %r\n", __FUNCTION__, Status));
    ASSERT_EFI_ERROR (Status);
    return;
  }

  Status = SendCommunicateBuffer (0);
  DEBUG ((DEBUG_INFO, "%a: SendCommunicateBuffer returned %r\n", __FUNCTION__, Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send MM communication for variable lock activation: %r\n", __FUNCTION__, Status));
    ASSERT_EFI_ERROR (Status);
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
VarCheckLockAllDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                            Status;
  MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION  *Exception;
  UINTN                                 ExceptionSize;
  UINTN                                 Index;

  if (PcdGet8 (PcdLockAllVariables) == 0) {
    DEBUG ((DEBUG_ERROR, "%a: EXITING - Variable locking is DISABLED\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_ERROR, "%a: Variable locking is ENABLED - proceeding\n", __FUNCTION__));

  //
  // Initialize MM communication channel
  //
  mVariableBuffer         = AllocateRuntimePool (MM_COMM_BUFFER_SIZE);
  mVariableBufferPhysical = mVariableBuffer;
  ASSERT (mVariableBuffer != NULL);

  Status = gBS->LocateProtocol (
                  &gEfiMmCommunication2ProtocolGuid,
                  NULL,
                  (VOID **)&mMmCommunication2
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    FreePool (mVariableBuffer);
    mVariableBuffer         = NULL;
    mVariableBufferPhysical = NULL;
    return Status;
  }

  //
  // Transfer exception list to MM
  //
  for (Index = 0; Index < ARRAY_SIZE (mVarLockExceptionList); Index++) {
    //
    // Validate exception list entry for NULL pointers
    //
    if ((mVarLockExceptionList[Index].VariableName == NULL) ||
        (mVarLockExceptionList[Index].VendorGuid == NULL))
    {
      DEBUG ((DEBUG_ERROR, "VarCheckLockAllDxe: Exception entry %u has NULL pointer\n", Index));
      ASSERT (FALSE);
      continue;
    }

    if (StrnLenS (mVarLockExceptionList[Index].VariableName, MAX_VARIABLE_NAME_LEN) >= MAX_VARIABLE_NAME_LEN) {
      DEBUG ((DEBUG_ERROR, "VarCheckLockAllDxe: Variable name %u too long or not NULL-terminated\n", Index));
      ASSERT (FALSE);
      continue;
    }

    ExceptionSize = sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION) +
                    StrSize (mVarLockExceptionList[Index].VariableName);
    Status = InitCommunicateBuffer ((VOID **)&Exception, ExceptionSize, MM_VAR_CHECK_LOCK_ALL_ADD_EXCEPTION);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "VarCheckLockAllDxe: InitCommunicateBuffer failed for exception %u: %r\n", Index, Status));
      continue;
    }

    CopyGuid (&Exception->VendorGuid, mVarLockExceptionList[Index].VendorGuid);
    StrCpyS (Exception->VariableName, MAX_VARIABLE_NAME_LEN, mVarLockExceptionList[Index].VariableName);
    Status = SendCommunicateBuffer (ExceptionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "VarCheckLockAllDxe: SendCommunicateBuffer failed for exception %u: %r\n", Index, Status));
      continue;
    }
  }

  //
  // Register to handle ReadyToBoot event
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  OnReadyToBoot,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &mReadyToBootEvent
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    FreePool (mVariableBuffer);
    mVariableBuffer         = NULL;
    mVariableBufferPhysical = NULL;
    return Status;
  }

  return EFI_SUCCESS;
}
