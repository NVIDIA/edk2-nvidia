/** @file
  UserAuthentication DXE password wrapper.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UserAuthenticationDxe.h"

/**
  Initialize the communicate buffer using DataSize and Function.

  @param[out]      DataPtr          Points to the data in the communicate buffer.
  @param[in]       DataSize         The data size to send to MM.
  @param[in]       Function         The function number to initialize the communicate header.

  @return Communicate buffer.
**/
VOID *
InitCommunicateBuffer (
  OUT     VOID   **DataPtr OPTIONAL,
  IN      UINTN  DataSize,
  IN      UINTN  Function
  )
{
  EFI_MM_COMMUNICATE_HEADER       *MmCommunicateHeader;
  MM_PASSWORD_COMMUNICATE_HEADER  *MmPasswordFunctionHeader;
  VOID                            *Buffer;

  Buffer = NULL;

  if (DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) +
      sizeof (MM_PASSWORD_COMMUNICATE_HEADER) > PASSWORD_COMM_BUFFER_SIZE)
  {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters\n", __FUNCTION__));
  }

  // Allocate the buffer for MM communication
  mMmCommBuffer = AllocateRuntimePool (PASSWORD_COMM_BUFFER_SIZE);
  if (mMmCommBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Buffer allocation for MM comm. failed\n", __FUNCTION__));
  }

  mMmCommBufferPhysical = mMmCommBuffer;
  Buffer                = mMmCommBufferPhysical;
  ASSERT (Buffer != NULL);

  // Initialize EFI_MM_COMMUNICATE_HEADER structure
  MmCommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)mMmCommBuffer;
  CopyGuid (&MmCommunicateHeader->HeaderGuid, &gUserAuthenticationGuid);
  MmCommunicateHeader->MessageLength = DataSize + sizeof (MM_PASSWORD_COMMUNICATE_HEADER);

  MmPasswordFunctionHeader = (MM_PASSWORD_COMMUNICATE_HEADER *)MmCommunicateHeader->Data;
  ZeroMem (MmPasswordFunctionHeader, DataSize + sizeof (MM_PASSWORD_COMMUNICATE_HEADER));
  MmPasswordFunctionHeader->Function = Function;
  if (DataPtr != NULL) {
    *DataPtr = MmPasswordFunctionHeader + 1;
  }

  return Buffer;
}

/**
  Send the data in communicate buffer to MM.

  @param[in]   Buffer                 Points to the data in the communicate buffer.
  @param[in]   DataSize               The data size to send to MM.

  @retval      EFI_SUCCESS            Success is returned from the function in MM.
  @retval      Others                 Failure is returned from the function in MM.

**/
EFI_STATUS
SendCommunicateBuffer (
  IN      VOID   *Buffer,
  IN      UINTN  DataSize
  )
{
  EFI_STATUS                      Status;
  UINTN                           CommSize;
  EFI_MM_COMMUNICATE_HEADER       *MmCommunicateHeader;
  MM_PASSWORD_COMMUNICATE_HEADER  *MmPasswordFunctionHeader;

  CommSize = DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) + sizeof (MM_PASSWORD_COMMUNICATE_HEADER);

  Status = mMmCommunication2->Communicate (mMmCommunication2, Buffer, Buffer, &CommSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Mm communicate failed!", __FUNCTION__));
    return Status;
  }

  MmCommunicateHeader      = (EFI_MM_COMMUNICATE_HEADER *)Buffer;
  MmPasswordFunctionHeader = (MM_PASSWORD_COMMUNICATE_HEADER *)MmCommunicateHeader->Data;
  return MmPasswordFunctionHeader->ReturnStatus;
}

/**
  Validate if the password is correct.

  @param[in] Password               The user input password.
  @param[in] PasswordSize           The size of Password in byte.

  @retval EFI_SUCCESS               The password is correct.
  @retval EFI_SECURITY_VIOLATION    The password is incorrect.
  @retval EFI_INVALID_PARAMETER     The password or size is invalid.
  @retval EFI_OUT_OF_RESOURCES      Insufficient resources to verify the password.
  @retval EFI_ACCESS_DENIED         Password retry count reach.
**/
EFI_STATUS
VerifyPassword (
  IN   CHAR16  *Password,
  IN   UINTN   PasswordSize
  )
{
  EFI_STATUS                               Status;
  VOID                                     *Buffer;
  MM_PASSWORD_COMMUNICATE_VERIFY_PASSWORD  *VerifyPassword;

  ASSERT (Password != NULL);

  if (PasswordSize > sizeof (VerifyPassword->Password) * sizeof (CHAR16)) {
    return EFI_INVALID_PARAMETER;
  }

  Buffer = InitCommunicateBuffer (
             (VOID **)&VerifyPassword,
             sizeof (*VerifyPassword),
             MM_PASSWORD_FUNCTION_VERIFY_PASSWORD
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = UnicodeStrToAsciiStrS (Password, VerifyPassword->Password, sizeof (VerifyPassword->Password));
  if (EFI_ERROR (Status)) {
    goto EXIT;
  }

  Status = SendCommunicateBuffer (Buffer, sizeof (*VerifyPassword));

EXIT:
  ZeroMem (VerifyPassword, sizeof (*VerifyPassword));
  return Status;
}

/**
  Set a new password.

  @param[in] NewPassword            The user input new password.
                                    NULL means clear password.
  @param[in] NewPasswordSize        The size of NewPassword in byte.
  @param[in] OldPassword            The user input old password.
                                    NULL means no old password.
  @param[in] OldPasswordSize        The size of OldPassword in byte.

  @retval EFI_SUCCESS               The NewPassword is set successfully.
  @retval EFI_SECURITY_VIOLATION    The OldPassword is incorrect.
  @retval EFI_INVALID_PARAMETER     The password or size is invalid.
  @retval EFI_OUT_OF_RESOURCES      Insufficient resources to set the password.
  @retval EFI_ACCESS_DENIED         Password retry count reach.
  @retval EFI_UNSUPPORTED           NewPassword is not strong enough.
  @retval EFI_ALREADY_STARTED       NewPassword is in history.
**/
EFI_STATUS
SetPassword (
  IN   CHAR16 *NewPassword, OPTIONAL
  IN   UINTN        NewPasswordSize,
  IN   CHAR16       *OldPassword, OPTIONAL
  IN   UINTN        OldPasswordSize
  )
{
  EFI_STATUS                            Status;
  VOID                                  *Buffer;
  MM_PASSWORD_COMMUNICATE_SET_PASSWORD  *SetPassword;

  if (NewPasswordSize > sizeof (SetPassword->NewPassword) * sizeof (CHAR16)) {
    return EFI_INVALID_PARAMETER;
  }

  if (OldPasswordSize > sizeof (SetPassword->OldPassword) * sizeof (CHAR16)) {
    return EFI_INVALID_PARAMETER;
  }

  Buffer = InitCommunicateBuffer (
             (VOID **)&SetPassword,
             sizeof (*SetPassword),
             MM_PASSWORD_FUNCTION_SET_PASSWORD
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (NewPassword != NULL) {
    Status = UnicodeStrToAsciiStrS (NewPassword, SetPassword->NewPassword, sizeof (SetPassword->NewPassword));
    if (EFI_ERROR (Status)) {
      goto EXIT;
    }
  } else {
    SetPassword->NewPassword[0] = 0;
  }

  if (OldPassword != NULL) {
    Status = UnicodeStrToAsciiStrS (OldPassword, SetPassword->OldPassword, sizeof (SetPassword->OldPassword));
    if (EFI_ERROR (Status)) {
      goto EXIT;
    }
  } else {
    SetPassword->OldPassword[0] = 0;
  }

  Status = SendCommunicateBuffer (Buffer, sizeof (*SetPassword));

EXIT:
  ZeroMem (SetPassword, sizeof (*SetPassword));
  return Status;
}

/**
  Return if the password is set.

  @retval TRUE      The password is set.
  @retval FALSE     The password is not set.
**/
BOOLEAN
IsPasswordInstalled (
  VOID
  )
{
  EFI_STATUS  Status;
  VOID        *Buffer;

  Buffer = InitCommunicateBuffer (
             NULL,
             0,
             MM_PASSWORD_FUNCTION_IS_PASSWORD_SET
             );
  if (Buffer == NULL) {
    return FALSE;
  }

  Status = SendCommunicateBuffer (Buffer, 0);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return TRUE;
}

/**
  Get password verification policy.

  @param[out] VerifyPolicy          Verification policy.

  @retval EFI_SUCCESS               Get verification policy successfully.
  @retval EFI_OUT_OF_RESOURCES      Insufficient resources to get verification policy.
**/
EFI_STATUS
GetPasswordVerificationPolicy (
  OUT MM_PASSWORD_COMMUNICATE_VERIFY_POLICY  *VerifyPolicy
  )
{
  EFI_STATUS                             Status;
  VOID                                   *Buffer;
  MM_PASSWORD_COMMUNICATE_VERIFY_POLICY  *GetVerifyPolicy;

  Buffer = InitCommunicateBuffer (
             (VOID **)&GetVerifyPolicy,
             sizeof (*GetVerifyPolicy),
             MM_PASSWORD_FUNCTION_GET_VERIFY_POLICY
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = SendCommunicateBuffer (Buffer, sizeof (*GetVerifyPolicy));
  if (!EFI_ERROR (Status)) {
    CopyMem (VerifyPolicy, GetVerifyPolicy, sizeof (MM_PASSWORD_COMMUNICATE_VERIFY_POLICY));
  }

  return Status;
}

/**
  Return if the password was verified.

  @retval TRUE      The password was verified.
  @retval FALSE     The password was not verified.
**/
BOOLEAN
WasPasswordVerified (
  VOID
  )
{
  EFI_STATUS  Status;
  VOID        *Buffer;

  Buffer = InitCommunicateBuffer (
             NULL,
             0,
             MM_PASSWORD_FUNCTION_WAS_PASSWORD_VERIFIED
             );
  if (Buffer == NULL) {
    return FALSE;
  }

  Status = SendCommunicateBuffer (Buffer, 0);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return TRUE;
}
