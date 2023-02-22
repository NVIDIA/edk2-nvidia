/** @file

  Copyright (c) 2019 - 2021, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UserAuthenticationMm.h"

EFI_SMM_VARIABLE_PROTOCOL  *mSmmVariable;

UINTN  mAdminPasswordTryCount = 0;

BOOLEAN     mNeedReVerify     = TRUE;
BOOLEAN     mPasswordVerified = FALSE;
EFI_HANDLE  mHandle           = NULL;

/**
  Verify if the password is correct.

  @param[in]  Password               The user input password.
  @param[in]  PasswordSize           The size of Password in byte.
  @param[in]  UserPasswordVarStruct  The storage of password in variable.

  @retval EFI_SUCCESS              The password is correct.
  @retval EFI_SECURITY_VIOLATION   The password is incorrect.
**/
EFI_STATUS
VerifyPassword (
  IN CHAR8                     *Password,
  IN UINTN                     PasswordSize,
  IN USER_PASSWORD_VAR_STRUCT  *UserPasswordVarStruct
  )
{
  BOOLEAN  HashOk;
  UINT8    HashData[PASSWORD_HASH_SIZE];

  HashOk = KeyLibGeneratePBKDF2Hash (
             HASH_TYPE_SHA256,
             (UINT8 *)Password,
             PasswordSize,
             UserPasswordVarStruct->PasswordSalt,
             sizeof (UserPasswordVarStruct->PasswordSalt),
             HashData,
             sizeof (HashData)
             );
  if (!HashOk) {
    return EFI_DEVICE_ERROR;
  }

  if (KeyLibSlowCompareMem (UserPasswordVarStruct->PasswordHash, HashData, PASSWORD_HASH_SIZE) == 0) {
    return EFI_SUCCESS;
  } else {
    return EFI_SECURITY_VIOLATION;
  }
}

/**
  Get hash data of password from non-volatile variable region.

  @param[in]   UserGuid               The user GUID of the password variable.
  @param[in]   Index                  The index of the password.
                                      0 means current password.
                                      Non-0 means the password history.
  @param[out]  UserPasswordVarStruct  The storage of password in variable.

  @retval EFI_SUCCESS             The password hash is returned successfully.
  @retval EFI_NOT_FOUND           The password hash is not found.
**/
EFI_STATUS
GetPasswordHashFromVariable (
  IN  EFI_GUID                  *UserGuid,
  IN  UINTN                     Index,
  OUT USER_PASSWORD_VAR_STRUCT  *UserPasswordVarStruct
  )
{
  UINTN   DataSize;
  CHAR16  PasswordName[sizeof (USER_AUTHENTICATION_VAR_NAME)/sizeof (CHAR16) + 5];

  if (Index != 0) {
    UnicodeSPrint (PasswordName, sizeof (PasswordName), L"%s%04x", USER_AUTHENTICATION_VAR_NAME, Index);
  } else {
    UnicodeSPrint (PasswordName, sizeof (PasswordName), L"%s", USER_AUTHENTICATION_VAR_NAME);
  }

  DataSize = sizeof (*UserPasswordVarStruct);
  return mSmmVariable->SmmGetVariable (
                         PasswordName,
                         UserGuid,
                         NULL,
                         &DataSize,
                         UserPasswordVarStruct
                         );
}

/**
  Save password hash data to non-volatile variable region.

  @param[in]   UserGuid               The user GUID of the password variable.
  @param[in]   UserPasswordVarStruct  The storage of password in variable.

  @retval EFI_SUCCESS             The password hash is saved successfully.
  @retval EFI_OUT_OF_RESOURCES    Insufficient resources to save the password hash.
**/
EFI_STATUS
SavePasswordHashToVariable (
  IN EFI_GUID                  *UserGuid,
  IN USER_PASSWORD_VAR_STRUCT  *UserPasswordVarStruct
  )
{
  EFI_STATUS  Status;

  if (UserPasswordVarStruct == NULL) {
    Status = mSmmVariable->SmmSetVariable (
                             USER_AUTHENTICATION_VAR_NAME,
                             UserGuid,
                             EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                             0,
                             NULL
                             );
  } else {
    Status = mSmmVariable->SmmSetVariable (
                             USER_AUTHENTICATION_VAR_NAME,
                             UserGuid,
                             EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                             sizeof (*UserPasswordVarStruct),
                             UserPasswordVarStruct
                             );
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SavePasswordHashToVariable fails with %r\n", Status));
  }

  return Status;
}

/**
  Save old password hash data to non-volatile variable region as history.

  The number of password history variable is limited.
  If all the password history variables are used, the new password history
  will override the oldest one.

  @param[in]   UserGuid               The user GUID of the password variable.
  @param[in]   UserPasswordVarStruct  The storage of password in variable.

  @retval EFI_SUCCESS             The password hash is saved successfully.
  @retval EFI_OUT_OF_RESOURCES    Insufficient resources to save the password hash.
**/
EFI_STATUS
SaveOldPasswordToHistory (
  IN EFI_GUID                  *UserGuid,
  IN USER_PASSWORD_VAR_STRUCT  *UserPasswordVarStruct
  )
{
  EFI_STATUS  Status;
  UINTN       DataSize;
  UINT32      LastIndex;
  CHAR16      PasswordName[sizeof (USER_AUTHENTICATION_VAR_NAME)/sizeof (CHAR16) + 5];

  DEBUG ((DEBUG_INFO, "SaveOldPasswordToHistory\n"));

  DataSize = sizeof (LastIndex);
  Status   = mSmmVariable->SmmGetVariable (
                             USER_AUTHENTICATION_HISTORY_LAST_VAR_NAME,
                             UserGuid,
                             NULL,
                             &DataSize,
                             &LastIndex
                             );
  if (EFI_ERROR (Status)) {
    LastIndex = 0;
  }

  if (LastIndex >= PASSWORD_HISTORY_CHECK_COUNT) {
    LastIndex = 0;
  }

  LastIndex++;
  UnicodeSPrint (PasswordName, sizeof (PasswordName), L"%s%04x", USER_AUTHENTICATION_VAR_NAME, LastIndex);

  Status = mSmmVariable->SmmSetVariable (
                           PasswordName,
                           UserGuid,
                           EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                           sizeof (*UserPasswordVarStruct),
                           UserPasswordVarStruct
                           );
  DEBUG ((DEBUG_INFO, "  -- to %s, %r\n", PasswordName, Status));
  if (!EFI_ERROR (Status)) {
    Status = mSmmVariable->SmmSetVariable (
                             USER_AUTHENTICATION_HISTORY_LAST_VAR_NAME,
                             UserGuid,
                             EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                             sizeof (LastIndex),
                             &LastIndex
                             );
    DEBUG ((DEBUG_INFO, " LastIndex - 0x%04x, %r\n", LastIndex, Status));
  }

  return Status;
}

/**
  Calculate password hash data and save it to non-volatile variable region.

  @param[in]  UserGuid               The user GUID of the password variable.
  @param[in]  Password               The user input password.
                                     NULL means delete the password variable.
  @param[in]  PasswordSize           The size of Password in byte.

  @retval EFI_SUCCESS             The password hash is calculated and saved.
  @retval EFI_OUT_OF_RESOURCES    Insufficient resources to save the password hash.
**/
EFI_STATUS
SavePasswordToVariable (
  IN  EFI_GUID *UserGuid,
  IN  CHAR8 *Password, OPTIONAL
  IN  UINTN                         PasswordSize
  )
{
  EFI_STATUS                Status;
  USER_PASSWORD_VAR_STRUCT  UserPasswordVarStruct;
  BOOLEAN                   HashOk;

  //
  // If password is NULL, it means we want to clean password field saved in variable region.
  //
  if (Password != NULL) {
    KeyLibGenerateSalt (UserPasswordVarStruct.PasswordSalt, sizeof (UserPasswordVarStruct.PasswordSalt));
    HashOk = KeyLibGeneratePBKDF2Hash (
               HASH_TYPE_SHA256,
               (UINT8 *)Password,
               PasswordSize,
               UserPasswordVarStruct.PasswordSalt,
               sizeof (UserPasswordVarStruct.PasswordSalt),
               UserPasswordVarStruct.PasswordHash,
               sizeof (UserPasswordVarStruct.PasswordHash)
               );
    if (!HashOk) {
      return EFI_DEVICE_ERROR;
    }

    Status = SavePasswordHashToVariable (UserGuid, &UserPasswordVarStruct);
    //
    // Save Password data to history variable
    //
    if (!EFI_ERROR (Status)) {
      SaveOldPasswordToHistory (UserGuid, &UserPasswordVarStruct);
    }
  } else {
    Status = SavePasswordHashToVariable (UserGuid, NULL);
  }

  return Status;
}

/**
  Verify the password.
  If the password variable does not exist, it passes the verification.
  If the password variable exists, it does verification based upon password variable.

  @param[in]  UserGuid               The user GUID of the password variable.
  @param[in]  Password               The user input password.
  @param[in]  PasswordSize           The size of Password in byte.

  @retval TRUE    The verification passes.
  @retval FALSE   The verification fails.
**/
BOOLEAN
IsPasswordVerified (
  IN EFI_GUID  *UserGuid,
  IN CHAR8     *Password,
  IN UINTN     PasswordSize
  )
{
  USER_PASSWORD_VAR_STRUCT  UserPasswordVarStruct;
  EFI_STATUS                Status;
  UINTN                     *PasswordTryCount;

  PasswordTryCount = &mAdminPasswordTryCount;

  Status = GetPasswordHashFromVariable (UserGuid, 0, &UserPasswordVarStruct);
  if (EFI_ERROR (Status)) {
    return TRUE;
  }

  //
  // Old password exists
  //
  Status = VerifyPassword (Password, PasswordSize, &UserPasswordVarStruct);
  if (EFI_ERROR (Status)) {
    if (Password[0] != 0) {
      *PasswordTryCount = *PasswordTryCount + 1;
    }

    return FALSE;
  }

  return TRUE;
}

/**
  Return if the password is set.

  @param[in]  UserGuid               The user GUID of the password variable.

  @retval TRUE    The password is set.
  @retval FALSE   The password is not set.
**/
BOOLEAN
IsPasswordSet (
  IN EFI_GUID  *UserGuid
  )
{
  USER_PASSWORD_VAR_STRUCT  UserPasswordVarStruct;
  EFI_STATUS                Status;

  Status = GetPasswordHashFromVariable (UserGuid, 0, &UserPasswordVarStruct);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return TRUE;
}

/**
  Return if the password is strong.
  Criteria:
  1) length >= PASSWORD_MIN_SIZE
  2) include lower case, upper case, number, symbol.

  @param[in]  Password               The user input password.
  @param[in]  PasswordSize           The size of Password in byte.

  @retval TRUE    The password is strong.
  @retval FALSE   The password is weak.
**/
BOOLEAN
IsPasswordStrong (
  IN CHAR8  *Password,
  IN UINTN  PasswordSize
  )
{
  UINTN    Index;
  BOOLEAN  HasLowerCase;
  BOOLEAN  HasUpperCase;
  BOOLEAN  HasNumber;
  BOOLEAN  HasSymbol;

  if (PasswordSize < PASSWORD_MIN_SIZE) {
    return FALSE;
  }

  HasLowerCase = FALSE;
  HasUpperCase = FALSE;
  HasNumber    = FALSE;
  HasSymbol    = FALSE;
  for (Index = 0; Index < PasswordSize - 1; Index++) {
    if ((Password[Index] >= 'a') && (Password[Index] <= 'z')) {
      HasLowerCase = TRUE;
    } else if ((Password[Index] >= 'A') && (Password[Index] <= 'Z')) {
      HasUpperCase = TRUE;
    } else if ((Password[Index] >= '0') && (Password[Index] <= '9')) {
      HasNumber = TRUE;
    } else {
      HasSymbol = TRUE;
    }
  }

  if ((!HasLowerCase) || (!HasUpperCase) || (!HasNumber) || (!HasSymbol)) {
    return FALSE;
  }

  return TRUE;
}

/**
  Return if the password is set before in PASSWORD_HISTORY_CHECK_COUNT.

  @param[in]  UserGuid               The user GUID of the password variable.
  @param[in]  Password               The user input password.
  @param[in]  PasswordSize           The size of Password in byte.

  @retval TRUE    The password is set before.
  @retval FALSE   The password is not set before.
**/
BOOLEAN
IsPasswordInHistory (
  IN EFI_GUID  *UserGuid,
  IN CHAR8     *Password,
  IN UINTN     PasswordSize
  )
{
  EFI_STATUS                Status;
  USER_PASSWORD_VAR_STRUCT  UserPasswordVarStruct;
  UINTN                     Index;

  for (Index = 1; Index <= PASSWORD_HISTORY_CHECK_COUNT; Index++) {
    Status = GetPasswordHashFromVariable (UserGuid, Index, &UserPasswordVarStruct);
    if (!EFI_ERROR (Status)) {
      Status = VerifyPassword (Password, PasswordSize, &UserPasswordVarStruct);
      if (!EFI_ERROR (Status)) {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/**
  Communication service MMI Handler entry.

  This MMI handler provides services for password management.

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
MmPasswordHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  EFI_STATUS                               Status;
  MM_PASSWORD_COMMUNICATE_HEADER           *MmFunctionHeader;
  UINTN                                    CommBufferPayloadSize;
  UINTN                                    TempCommBufferSize;
  MM_PASSWORD_COMMUNICATE_SET_PASSWORD     MmCommunicateSetPassword;
  MM_PASSWORD_COMMUNICATE_VERIFY_PASSWORD  MmCommunicateVerifyPassword;
  MM_PASSWORD_COMMUNICATE_VERIFY_POLICY    MmCommunicateSetVerifyPolicy;
  MM_PASSWORD_COMMUNICATE_VERIFY_POLICY    *MmCommunicateGetVerifyPolicy;
  MM_PASSWORD_COMMUNICATE_PASSWORD_HASH    *MmCommunicatePasswordHash;
  USER_PASSWORD_VAR_STRUCT                 UserPasswordVarStruct;
  UINTN                                    PasswordLen;
  EFI_GUID                                 *UserGuid;
  UINTN                                    *PasswordTryCount;

  //
  // If input is invalid, stop processing this SMI
  //
  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Input is INVALID", __FUNCTION__));
    return EFI_SUCCESS;
  }

  TempCommBufferSize = *CommBufferSize;
  PasswordLen        = 0;

  if (TempCommBufferSize < sizeof (MM_PASSWORD_COMMUNICATE_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: MM communication buffer size invalid!\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  CommBufferPayloadSize = TempCommBufferSize - sizeof (MM_PASSWORD_COMMUNICATE_HEADER);

  Status           = EFI_SUCCESS;
  MmFunctionHeader = (MM_PASSWORD_COMMUNICATE_HEADER *)CommBuffer;

  UserGuid         = &gUserAuthenticationGuid;
  PasswordTryCount = &mAdminPasswordTryCount;

  switch (MmFunctionHeader->Function) {
    case MM_PASSWORD_FUNCTION_IS_PASSWORD_SET:
      PasswordTryCount = NULL;
      if (CommBufferPayloadSize != 0) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: IS_PASSWORD_SET payload buffer invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      if (IsPasswordSet (UserGuid)) {
        Status = EFI_SUCCESS;
      } else {
        Status = EFI_NOT_FOUND;
      }

      break;
    case MM_PASSWORD_FUNCTION_SET_PASSWORD:
      if (*PasswordTryCount >= PASSWORD_MAX_TRY_COUNT) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: SET_PASSWORD try count reach!\n"));
        PasswordTryCount = NULL;
        Status           = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      if (CommBufferPayloadSize != sizeof (MM_PASSWORD_COMMUNICATE_SET_PASSWORD)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: SET_PASSWORD payload buffer invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      CopyMem (&MmCommunicateSetPassword, MmFunctionHeader + 1, sizeof (MmCommunicateSetPassword));

      PasswordLen = AsciiStrnLenS (MmCommunicateSetPassword.OldPassword, sizeof (MmCommunicateSetPassword.OldPassword));
      if (PasswordLen == sizeof (MmCommunicateSetPassword.OldPassword)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: OldPassword invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      if (!IsPasswordVerified (UserGuid, MmCommunicateSetPassword.OldPassword, PasswordLen + 1)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: PasswordVerify - FAIL\n"));
        if (*PasswordTryCount >= PASSWORD_MAX_TRY_COUNT) {
          DEBUG ((DEBUG_ERROR, "MmPasswordHandler: SET_PASSWORD try count reach!\n"));
          Status = EFI_ACCESS_DENIED;
        } else {
          Status = EFI_SECURITY_VIOLATION;
        }

        goto EXIT;
      }

      PasswordLen = AsciiStrnLenS (MmCommunicateSetPassword.NewPassword, sizeof (MmCommunicateSetPassword.NewPassword));
      if (PasswordLen == sizeof (MmCommunicateSetPassword.NewPassword)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: NewPassword invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      if ((PasswordLen != 0) && !IsPasswordStrong (MmCommunicateSetPassword.NewPassword, PasswordLen + 1)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: NewPassword too weak!\n"));
        Status = EFI_UNSUPPORTED;
        goto EXIT;
      }

      if ((PasswordLen != 0) && IsPasswordInHistory (UserGuid, MmCommunicateSetPassword.NewPassword, PasswordLen + 1)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: NewPassword in history!\n"));
        Status = EFI_ALREADY_STARTED;
        goto EXIT;
      }

      if (PasswordLen == 0) {
        Status = SavePasswordToVariable (UserGuid, NULL, 0);
      } else {
        Status = SavePasswordToVariable (UserGuid, MmCommunicateSetPassword.NewPassword, PasswordLen + 1);
      }

      break;

    case MM_PASSWORD_FUNCTION_VERIFY_PASSWORD:
      if (*PasswordTryCount >= PASSWORD_MAX_TRY_COUNT) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: VERIFY_PASSWORD try count reach!\n"));
        PasswordTryCount = NULL;
        Status           = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      if (CommBufferPayloadSize != sizeof (MM_PASSWORD_COMMUNICATE_VERIFY_PASSWORD)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: VERIFY_PASSWORD payload buffer invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      CopyMem (&MmCommunicateVerifyPassword, MmFunctionHeader + 1, sizeof (MmCommunicateVerifyPassword));

      PasswordLen = AsciiStrnLenS (MmCommunicateVerifyPassword.Password, sizeof (MmCommunicateVerifyPassword.Password));
      if (PasswordLen == sizeof (MmCommunicateVerifyPassword.Password)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: Password invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      if (!IsPasswordVerified (UserGuid, MmCommunicateVerifyPassword.Password, PasswordLen + 1)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: PasswordVerify - FAIL\n"));
        if (*PasswordTryCount >= PASSWORD_MAX_TRY_COUNT) {
          DEBUG ((DEBUG_ERROR, "MmPasswordHandler: VERIFY_PASSWORD try count reach!\n"));
          Status = EFI_ACCESS_DENIED;
        } else {
          Status = EFI_SECURITY_VIOLATION;
        }

        goto EXIT;
      }

      mPasswordVerified = TRUE;
      Status            = EFI_SUCCESS;
      break;

    case MM_PASSWORD_FUNCTION_SET_VERIFY_POLICY:
      PasswordTryCount = NULL;
      if (CommBufferPayloadSize != sizeof (MM_PASSWORD_COMMUNICATE_VERIFY_POLICY)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: SET_VERIFY_POLICY payload buffer invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      CopyMem (&MmCommunicateSetVerifyPolicy, MmFunctionHeader + 1, sizeof (MmCommunicateSetVerifyPolicy));
      mNeedReVerify = MmCommunicateSetVerifyPolicy.NeedReVerify;
      break;

    case MM_PASSWORD_FUNCTION_GET_VERIFY_POLICY:
      PasswordTryCount = NULL;
      if (CommBufferPayloadSize != sizeof (MM_PASSWORD_COMMUNICATE_VERIFY_POLICY)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: GET_VERIFY_POLICY payload buffer invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      MmCommunicateGetVerifyPolicy               = (MM_PASSWORD_COMMUNICATE_VERIFY_POLICY *)(MmFunctionHeader + 1);
      MmCommunicateGetVerifyPolicy->NeedReVerify = mNeedReVerify;
      break;
    case MM_PASSWORD_FUNCTION_WAS_PASSWORD_VERIFIED:
      PasswordTryCount = NULL;
      if (CommBufferPayloadSize != 0) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: WAS_PASSWORD_VERIFIED payload buffer invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      if (mPasswordVerified) {
        Status = EFI_SUCCESS;
      } else {
        Status = EFI_NOT_STARTED;
      }

      break;

    case MM_PASSWORD_FUNCTION_GET_PASSWORD_HASH:

      if (CommBufferPayloadSize != sizeof (MM_PASSWORD_COMMUNICATE_PASSWORD_HASH)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: GET_PASSWORD_HASH payload buffer invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      MmCommunicatePasswordHash = (MM_PASSWORD_COMMUNICATE_PASSWORD_HASH *)(MmFunctionHeader + 1);

      Status = GetPasswordHashFromVariable (UserGuid, 0, &UserPasswordVarStruct);
      if (!EFI_ERROR (Status)) {
        CopyMem (MmCommunicatePasswordHash->PasswordSalt, UserPasswordVarStruct.PasswordSalt, sizeof (UserPasswordVarStruct.PasswordSalt));
        CopyMem (MmCommunicatePasswordHash->PasswordHash, UserPasswordVarStruct.PasswordHash, sizeof (UserPasswordVarStruct.PasswordHash));
      } else {
        ZeroMem (MmCommunicatePasswordHash->PasswordSalt, PASSWORD_SALT_SIZE);
        ZeroMem (MmCommunicatePasswordHash->PasswordHash, PASSWORD_HASH_SIZE);
        Status = EFI_NOT_FOUND;
      }

      break;

    case MM_PASSWORD_FUNCTION_SET_PASSWORD_HASH:

      if (CommBufferPayloadSize != sizeof (MM_PASSWORD_COMMUNICATE_PASSWORD_HASH)) {
        DEBUG ((DEBUG_ERROR, "MmPasswordHandler: SET_PASSWORD_HASH payload buffer invalid!\n"));
        Status = EFI_INVALID_PARAMETER;
        goto EXIT;
      }

      MmCommunicatePasswordHash = (MM_PASSWORD_COMMUNICATE_PASSWORD_HASH *)(MmFunctionHeader + 1);

      if (MmCommunicatePasswordHash->ClearPassword) {
        Status = SavePasswordHashToVariable (UserGuid, NULL);
      } else {
        ZeroMem (&UserPasswordVarStruct, sizeof (USER_PASSWORD_VAR_STRUCT));
        CopyMem (UserPasswordVarStruct.PasswordSalt, MmCommunicatePasswordHash->PasswordSalt, sizeof (UserPasswordVarStruct.PasswordSalt));
        CopyMem (UserPasswordVarStruct.PasswordHash, MmCommunicatePasswordHash->PasswordHash, sizeof (UserPasswordVarStruct.PasswordHash));
        Status = SavePasswordHashToVariable (UserGuid, &UserPasswordVarStruct);
        //
        // Save Password data to history variable
        //
        if (!EFI_ERROR (Status)) {
          SaveOldPasswordToHistory (UserGuid, &UserPasswordVarStruct);
        }
      }

      break;
    default:
      PasswordTryCount = NULL;
      Status           = EFI_UNSUPPORTED;
      break;
  }

EXIT:
  if (PasswordTryCount != NULL) {
    if (Status == EFI_SUCCESS) {
      *PasswordTryCount = 0;
    }
  }

  MmFunctionHeader->ReturnStatus = Status;

  return EFI_SUCCESS;
}

/**
  Main entry for this driver.

  @param ImageHandle     Image handle this driver.
  @param SystemTable     Pointer to SystemTable.

  @retval EFI_SUCESS     This function always complete successfully.

**/
EFI_STATUS
EFIAPI
PasswordMmInit (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS  Status;

  ASSERT (PASSWORD_HASH_SIZE == SHA256_DIGEST_SIZE);
  ASSERT (PASSWORD_HISTORY_CHECK_COUNT < 0xFFFF);

  Status = gMmst->MmLocateProtocol (&gEfiSmmVariableProtocolGuid, NULL, (VOID **)&mSmmVariable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: gEfiSmmVariableProtocolGuid: NOT LOCATED!\n", __FUNCTION__));
  }

  ASSERT_EFI_ERROR (Status);

  Status = gMmst->MmiHandlerRegister (MmPasswordHandler, &gUserAuthenticationGuid, &mHandle);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (IsPasswordCleared ()) {
    DEBUG ((DEBUG_INFO, "IsPasswordCleared\n"));
    SavePasswordToVariable (&gUserAuthenticationGuid, NULL, 0);
  }

  return EFI_SUCCESS;
}
