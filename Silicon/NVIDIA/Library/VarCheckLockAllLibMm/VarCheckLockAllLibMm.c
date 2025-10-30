/** @file
  Support option to lock all UEFI variables at runtime

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiMultiPhase.h>

#include <Library/VarCheckLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MmServicesTableLib.h>

#include <Guid/VarCheckLockAllMmi.h>
#include <Protocol/SmmVariable.h>

#define LIST_INCREMENT  10

STATIC BOOLEAN  mActivated = FALSE;

STATIC MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION  **mExceptionList       = NULL;
STATIC UINTN                                 mExceptionListMaxCount = 0;
STATIC UINTN                                 mExceptionListCount    = 0;
STATIC EFI_SMM_VARIABLE_PROTOCOL             *mSmmVariable          = NULL;

/**
  Adds a variable to the exception list

  @param[in]  Exception      Pointer to the variable to be added to the exception list
  @param[in]  ExceptionSize  Size of the variable to be added
**/
VOID
EFIAPI
AddException (
  IN MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION  *Exception,
  IN UINTN                                 ExceptionSize
  )
{
  MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION  **NewExceptionList;
  MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION  *NewException;

  //
  // Allocate more entries when the list is full
  //
  if (mExceptionListCount == mExceptionListMaxCount) {
    NewExceptionList = ReallocateRuntimePool (
                         mExceptionListMaxCount * sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION *),
                         (mExceptionListMaxCount + LIST_INCREMENT) * sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION *),
                         mExceptionList
                         );
    if (NewExceptionList == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Fail to allocate pool\n", __FUNCTION__));
      ASSERT (FALSE);
      return;
    }

    mExceptionList          = NewExceptionList;
    mExceptionListMaxCount += LIST_INCREMENT;
  }

  //
  // Add the variable to the exception list
  //
  NewException = AllocateRuntimeCopyPool (ExceptionSize, Exception);
  if (NewException == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to allocate pool\n", __FUNCTION__));
    ASSERT (FALSE);
    return;
  }

  mExceptionList[mExceptionListCount] = NewException;
  mExceptionListCount++;
}

/**
  Communication service MMI Handler entry.

  This MMI handler is used for checking when ReadyToBoot occurs

  @param[in]     DispatchHandle  The unique handle assigned to this handler by MmiHandlerRegister().
  @param[in]     RegisterContext Points to an optional handler context which was specified when the
                                 handler was registered.
  @param[in, out] CommBuffer     A pointer to a collection of data in memory that will
                                 be conveyed from a non-MM environment into an MM environment.
  @param[in, out] CommBufferSize The size of the CommBuffer.

  @retval EFI_SUCCESS                         The interrupt was handled and quiesced. No other handlers
                                              should still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_QUIESCED  The interrupt has been quiesced but other handlers should
                                              still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_PENDING   The interrupt is still pending and other handlers should still
                                              be called.
  @retval EFI_INTERRUPT_PENDING               The interrupt could not be quiesced.
**/
EFI_STATUS
EFIAPI
MmVarCheckHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  EFI_STATUS                         Status;
  MM_VAR_CHECK_LOCK_ALL_COMM_HEADER  *FuncHeader;
  UINTN                              PayloadSize;

  DEBUG ((DEBUG_INFO, "%a: *** MMI HANDLER CALLED ***\n", __FUNCTION__));

  //
  // If input is invalid, stop processing this SMI
  //
  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid buffer parameters\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  if (*CommBufferSize < sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: MM communication buffer size invalid!\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  FuncHeader = (MM_VAR_CHECK_LOCK_ALL_COMM_HEADER *)CommBuffer;
  switch (FuncHeader->Function) {
    case MM_VAR_CHECK_LOCK_ALL_ACTIVATE:
      mActivated = TRUE;
      Status     = EFI_SUCCESS;
      DEBUG ((DEBUG_INFO, "%a: *** VARIABLE LOCKING ACTIVATED ***\n", __FUNCTION__));
      break;

    case MM_VAR_CHECK_LOCK_ALL_ADD_EXCEPTION:
      if (mActivated) {
        Status = EFI_WRITE_PROTECTED;
        break;
      }

      PayloadSize = *CommBufferSize - sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_HEADER);
      if (PayloadSize <= sizeof (MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION)) {
        DEBUG ((DEBUG_ERROR, "%a: Invalid variable size - %u!\n", __FUNCTION__, PayloadSize));
        Status = EFI_INVALID_PARAMETER;
        break;
      }

      AddException ((MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION *)(FuncHeader + 1), PayloadSize);
      Status = EFI_SUCCESS;
      break;

    default:
      Status = EFI_UNSUPPORTED;
      break;
  }

  FuncHeader->ReturnStatus = Status;

  return EFI_SUCCESS;
}

/**
  SetVariable check handler that rejects variable modification at runtime

  @param[in] VariableName       Name of Variable to set.
  @param[in] VendorGuid         Variable vendor GUID.
  @param[in] Attributes         Attribute value of the variable.
  @param[in] DataSize           Size of Data to set.
  @param[in] Data               Data pointer.

  @retval EFI_SUCCESS           SetVariable are allowed
  @retval EFI_WRITE_PROTECTED   SetVariable are not allowed

**/
EFI_STATUS
EFIAPI
SetVariableCheckHandler (
  IN CHAR16    *VariableName,
  IN EFI_GUID  *VendorGuid,
  IN UINT32    Attributes,
  IN UINTN     DataSize,
  IN VOID      *Data
  )
{
  UINTN       Index;
  EFI_STATUS  Status;
  UINT32      ExistingAttributes;
  UINTN       ExistingDataSize;

  //
  // Debug: Log all SetVariable attempts for troubleshooting
  //
  DEBUG ((
    DEBUG_INFO,
    "%a: CALLED - Var=%s, DataSize=%u, Attr=0x%x, Activated=%d\n",
    __FUNCTION__,
    VariableName,
    DataSize,
    Attributes,
    mActivated
    ));

  //
  // Fast path: Not activated yet - allow everything
  //
  if (!mActivated) {
    return EFI_SUCCESS;
  }

  //
  // Lazy initialization: Locate SMM Variable Protocol if not already done
  //
  if (mSmmVariable == NULL) {
    Status = gMmst->MmLocateProtocol (&gEfiSmmVariableProtocolGuid, NULL, (VOID **)&mSmmVariable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: FAILED to locate SMM Variable Protocol - %r\n", __FUNCTION__, Status));
      // Can't check existing variables without the protocol
      // Conservative approach: block all operations when activated
      if (mActivated) {
        return EFI_WRITE_PROTECTED;
      }

      return EFI_SUCCESS;
    }

    DEBUG ((DEBUG_INFO, "%a: Successfully located SMM Variable Protocol\n", __FUNCTION__));
  }

  //
  // Fast path: Check exception list first
  //
  for (Index = 0; Index < mExceptionListCount; Index++) {
    if (CompareGuid (&mExceptionList[Index]->VendorGuid, VendorGuid) &&
        (StrCmp (mExceptionList[Index]->VariableName, VariableName) == 0))
    {
      return EFI_SUCCESS;
    }
  }

  //
  // Handle deletion operations (DataSize == 0)
  // Must query existing variable to determine if it's NV or volatile
  //
  if (DataSize == 0) {
    ExistingDataSize = 0;
    Status           = mSmmVariable->SmmGetVariable (
                                       VariableName,
                                       VendorGuid,
                                       &ExistingAttributes,
                                       &ExistingDataSize,
                                       NULL
                                       );

    //
    // Variable exists if we get SUCCESS (size=0) or BUFFER_TOO_SMALL (size>0)
    //
    if ((Status == EFI_SUCCESS) || (Status == EFI_BUFFER_TOO_SMALL)) {
      //
      // Variable exists - check if it's non-volatile
      //
      if ((ExistingAttributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
        DEBUG ((DEBUG_ERROR, "%a: BLOCKING non-volatile deletion: %s (Attr=0x%x)\n", __FUNCTION__, VariableName, ExistingAttributes));
        return EFI_WRITE_PROTECTED;
      }

      //
      // Volatile variable - allow deletion
      //
      DEBUG ((DEBUG_INFO, "%a: Allowing volatile deletion: %s (Attr=0x%x)\n", __FUNCTION__, VariableName, ExistingAttributes));
      return EFI_SUCCESS;
    }

    //
    // Variable doesn't exist (EFI_NOT_FOUND) - allow (no-op)
    //
    if (Status == EFI_NOT_FOUND) {
      return EFI_SUCCESS;
    }

    //
    // Unexpected error - be conservative and block
    //
    DEBUG ((DEBUG_INFO, "%a: BLOCKING deletion due to GetVariable error %r: %s\n", __FUNCTION__, Status, VariableName));
    return EFI_WRITE_PROTECTED;
  }

  //
  // Handle write operations (DataSize > 0)
  //

  //
  // Case 1: Explicit non-zero attributes specified
  //
  if (Attributes != 0) {
    //
    // Fast check: If NV bit is set, block immediately
    //
    if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
      DEBUG ((DEBUG_ERROR, "%a: BLOCKING non-volatile write: %s (Attr=0x%x)\n", __FUNCTION__, VariableName, Attributes));
      return EFI_WRITE_PROTECTED;
    }

    //
    // Volatile attributes only - allow
    //
    return EFI_SUCCESS;
  }

  //
  // Case 2: Attributes == 0 with DataSize > 0
  // Must check existing variable
  //
  ExistingDataSize = 0;
  Status           = mSmmVariable->SmmGetVariable (
                                     VariableName,
                                     VendorGuid,
                                     &ExistingAttributes,
                                     &ExistingDataSize,
                                     NULL
                                     );

  //
  // Variable exists if we get SUCCESS or BUFFER_TOO_SMALL
  //
  if ((Status == EFI_SUCCESS) || (Status == EFI_BUFFER_TOO_SMALL)) {
    //
    // Variable exists - check if it's non-volatile
    //
    if ((ExistingAttributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
      DEBUG ((DEBUG_ERROR, "%a: BLOCKING NV update (Attr=0): %s (ExistingAttr=0x%x)\n", __FUNCTION__, VariableName, ExistingAttributes));
      return EFI_WRITE_PROTECTED;
    }

    //
    // Existing variable is volatile - allow update
    //
    return EFI_SUCCESS;
  }

  //
  // Variable doesn't exist (EFI_NOT_FOUND)
  // Creating new variable with Attributes=0 is invalid per UEFI spec
  // Block it to prevent security bypass attempts
  //
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: BLOCKING invalid new variable (Attr=0): %s\n", __FUNCTION__, VariableName));
    return EFI_WRITE_PROTECTED;
  }

  //
  // Other error - be conservative and block
  //
  DEBUG ((DEBUG_ERROR, "%a: BLOCKING write due to GetVariable error %r: %s\n", __FUNCTION__, Status, VariableName));
  return EFI_WRITE_PROTECTED;
}

/**
  Register VarCheck handler that locks all variables at runtime

  @retval EFI_SUCCESS       The constructor executed correctly.

**/
EFI_STATUS
EFIAPI
VarCheckLockAllLibMmConstructor (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle = NULL;

  DEBUG ((DEBUG_INFO, "%a: *** CONSTRUCTOR CALLED ***\n", __FUNCTION__));

  Status = gMmst->MmiHandlerRegister (MmVarCheckHandler, &gVarCheckLockAllGuid, &Handle);
  DEBUG ((DEBUG_INFO, "%a: MmiHandlerRegister returned %r\n", __FUNCTION__, Status));
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FAILED to register MMI handler - %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = VarCheckLibRegisterSetVariableCheckHandler (SetVariableCheckHandler);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FAILED to register handler - %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: *** VARCHECK HANDLER REGISTERED ***\n", __FUNCTION__));

  return EFI_SUCCESS;
}
