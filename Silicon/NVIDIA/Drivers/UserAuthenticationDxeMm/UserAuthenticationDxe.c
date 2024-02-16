/** @file
  This Driver mainly provides Setup Form to change password and
  does user authentication before entering Setup.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UserAuthenticationDxe.h"
#include "UserAuthenticationIpmi.h"
#include <Library/PlatformResourceLib.h>
#include <Library/StatusRegLib.h>

USER_AUTHENTICATION_PRIVATE_DATA  *mUserAuthenticationData = NULL;
EFI_MM_COMMUNICATION2_PROTOCOL    *mMmCommunication2       = NULL;

VOID  *mMmCommBuffer         = NULL;
VOID  *mMmCommBufferPhysical = NULL;

EFI_GUID                mUserAuthenticationVendorGuid = USER_AUTHENTICATION_FORMSET_GUID;
HII_VENDOR_DEVICE_PATH  mHiiVendorDevicePath          = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    USER_AUTHENTICATION_FORMSET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

/**
  Get a user input string.

  @param[in]       PopUpString      A popup string to inform user.
  @param[in, out]  UserInput        The user input string
  @param[in]       UserInputMaxLen  The max unicode count of the UserInput without NULL terminator.
**/
EFI_STATUS
GetUserInput (
  IN     CHAR16  *PopUpString,
  IN OUT CHAR16  *UserInput,
  IN     UINTN   UserInputMaxLen
  )
{
  EFI_INPUT_KEY  InputKey;
  UINTN          InputLength;
  CHAR16         *Mask;

  UserInput[0] = 0;
  Mask         = AllocateZeroPool ((UserInputMaxLen + 1) * sizeof (CHAR16));
  if (Mask == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  InputLength = 0;

  while (TRUE) {
    if (InputLength < UserInputMaxLen) {
      Mask[InputLength] = L'_';
    }

    CreatePopUp (
      EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
      &InputKey,
      PopUpString,
      L"--------------------------------",
      Mask,
      NULL
      );
    if (InputKey.ScanCode == SCAN_NULL) {
      //
      // Check whether finish inputing password.
      //
      if ((InputKey.UnicodeChar == CHAR_CARRIAGE_RETURN) && (InputLength > 0)) {
        //
        // Add the null terminator.
        //
        UserInput[InputLength] = 0;
        break;
      } else if ((InputKey.UnicodeChar == CHAR_NULL) ||
                 (InputKey.UnicodeChar == CHAR_LINEFEED) ||
                 (InputKey.UnicodeChar == CHAR_CARRIAGE_RETURN)
                 )
      {
        continue;
      } else {
        //
        // delete last key entered
        //
        if (InputKey.UnicodeChar == CHAR_BACKSPACE) {
          if (InputLength > 0) {
            UserInput[InputLength] = 0;
            Mask[InputLength]      = 0;
            InputLength--;
          }
        } else {
          if (InputLength == UserInputMaxLen) {
            Mask[InputLength] = 0;
            continue;
          }

          //
          // add Next key entry
          //
          UserInput[InputLength] = InputKey.UnicodeChar;
          Mask[InputLength]      = L'*';
          InputLength++;
        }
      }
    }
  }

  FreePool (Mask);
  return EFI_SUCCESS;
}

/**
  Display a message box to end user.

  @param[in] DisplayString   The string in message box.
**/
VOID
MessageBox (
  IN CHAR16  *DisplayString
  )
{
  EFI_INPUT_KEY  Key;

  do {
    CreatePopUp (
      EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
      &Key,
      L"",
      DisplayString,
      L"Press ENTER to continue ...",
      L"",
      NULL
      );
  } while (Key.UnicodeChar != CHAR_CARRIAGE_RETURN);
}

/**
  Force system reset.
**/
VOID
ForceSystemReset (
  VOID
  )
{
  MessageBox (L"Password retry count reach, reset system!");

  // Mark existing boot chain as good.
  ValidateActiveBootChain ();

  StatusRegReset ();
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
  CpuDeadLoop ();
}

/**
  Display message for set password.

  @param[in]  ReturnStatus   The return status for set password.
**/
VOID
PrintSetPasswordStatus (
  IN EFI_STATUS  ReturnStatus
  )
{
  CHAR16  *DisplayString;
  CHAR16  *DisplayString2;

  EFI_INPUT_KEY  Key;

  if (ReturnStatus == EFI_UNSUPPORTED) {
    DisplayString  = L"New password is not strong enough!";
    DisplayString2 = L"Check the help text for password requirements.";

    do {
      CreatePopUp (
        EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
        &Key,
        L"",
        DisplayString,
        DisplayString2,
        L"Press ENTER to continue ...",
        L"",
        NULL
        );
    } while (Key.UnicodeChar != CHAR_CARRIAGE_RETURN);
  } else {
    if (ReturnStatus == EFI_SUCCESS) {
      DisplayString = L"New password is updated successfully!";
    } else if (ReturnStatus == EFI_ALREADY_STARTED) {
      DisplayString = L"New password is found in the history passwords!";
    } else {
      DisplayString = L"New password update fails!";
    }

    do {
      CreatePopUp (
        EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
        &Key,
        L"",
        DisplayString,
        L"Press ENTER to continue ...",
        L"",
        NULL
        );
    } while (Key.UnicodeChar != CHAR_CARRIAGE_RETURN);
  }
}

/**
  Require user input password.

  @retval TRUE   User input correct password successfully.
  @retval FALSE  The password is not set.
**/
BOOLEAN
RequireUserPassword (
  VOID
  )
{
  EFI_STATUS                             Status;
  CHAR16                                 UserInputPw[PASSWORD_MAX_SIZE];
  CHAR16                                 *PopUpString;
  MM_PASSWORD_COMMUNICATE_VERIFY_POLICY  VerifyPolicy;

  Status = EFI_SUCCESS;
  ZeroMem (UserInputPw, sizeof (UserInputPw));
  if (!IsPasswordInstalled ()) {
    return FALSE;
  }

  Status = GetPasswordVerificationPolicy (&VerifyPolicy);
  if (!EFI_ERROR (Status)) {
    if (WasPasswordVerified () && (!VerifyPolicy.NeedReVerify)) {
      DEBUG ((DEBUG_INFO, "Password was verified and Re-verify is not needed\n"));
      return TRUE;
    }
  }

  PopUpString = L"Please input admin password";

  while (TRUE) {
    gST->ConOut->ClearScreen (gST->ConOut);
    GetUserInput (PopUpString, UserInputPw, PASSWORD_MAX_SIZE - 1);

    Status = VerifyPassword (UserInputPw, StrSize (UserInputPw));
    if (!EFI_ERROR (Status)) {
      break;
    }

    if (Status == EFI_ACCESS_DENIED) {
      //
      // Password retry count reach.
      //
      REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
        EFI_ERROR_CODE | EFI_ERROR_MAJOR,
        EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_DXE_BS_EC_INVALID_PASSWORD,
        OEM_EC_DESC_INVALID_PASSWORD_MAX,
        sizeof (OEM_EC_DESC_INVALID_PASSWORD_MAX)
        );

      ForceSystemReset ();
    } else {
      REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
        EFI_ERROR_CODE | EFI_ERROR_MINOR,
        EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_DXE_BS_EC_INVALID_PASSWORD,
        OEM_EC_DESC_INVALID_PASSWORD,
        sizeof (OEM_EC_DESC_INVALID_PASSWORD)
        );
    }

    MessageBox (L"Incorrect password!");
  }

  ZeroMem (UserInputPw, sizeof (UserInputPw));

  gST->ConOut->ClearScreen (gST->ConOut);

  return TRUE;
}

/**
  Set user password.

**/
VOID
SetUserPassword (
  VOID
  )
{
  EFI_STATUS  Status;
  CHAR16      UserInputPw[PASSWORD_MAX_SIZE];
  CHAR16      TmpPassword[PASSWORD_MAX_SIZE];
  CHAR16      *PopUpString;
  CHAR16      *PopUpString2;

  ZeroMem (UserInputPw, sizeof (UserInputPw));
  ZeroMem (TmpPassword, sizeof (TmpPassword));

  PopUpString = L"Please set admin password";

  while (TRUE) {
    gST->ConOut->ClearScreen (gST->ConOut);
    GetUserInput (PopUpString, UserInputPw, PASSWORD_MAX_SIZE - 1);

    PopUpString2 = L"Please confirm your new password";
    gST->ConOut->ClearScreen (gST->ConOut);
    GetUserInput (PopUpString2, TmpPassword, PASSWORD_MAX_SIZE - 1);
    if (StrCmp (TmpPassword, UserInputPw) != 0) {
      MessageBox (L"Password are not the same!");
      continue;
    }

    Status = SetPassword (UserInputPw, StrSize (UserInputPw), NULL, 0);
    PrintSetPasswordStatus (Status);
    if (!EFI_ERROR (Status)) {
      break;
    }
  }

  ZeroMem (UserInputPw, sizeof (UserInputPw));
  ZeroMem (TmpPassword, sizeof (TmpPassword));

  gST->ConOut->ClearScreen (gST->ConOut);
}

/**
  Prompt user to enter password and check if it is valid.

  @param  This         pointer to NVIDIA_USER_AUTH_PROTOCOL

  @retval EFI_SUCCESS              Valid password
  @retval EFI_SECURITY_VIOLATION   Invalid password
**/
EFI_STATUS
EFIAPI
CheckForPassword (
  IN     NVIDIA_USER_AUTH_PROTOCOL  *This
  )
{
  BOOLEAN  PasswordSet;

  //
  // Check whether enter setup page.
  //
  PasswordSet = RequireUserPassword ();
  if (PasswordSet) {
    DEBUG ((DEBUG_INFO, "Welcome Admin!\n"));
  } else {
    DEBUG ((DEBUG_INFO, "Admin password is not set!\n"));
    if (NeedEnrollPassword ()) {
      SetUserPassword ();
    }
  }

  return EFI_SUCCESS;
}

/**
  Protect user password variables from being changed or erased without authentication.

  @retval EFI_SUCCESS             Variables are locked successfully
  @retval EFI_SECURITY_VIOLATION  Fail to lock variables
**/
EFI_STATUS
EFIAPI
ProtectUserAuthenticationVariables (
  VOID
  )
{
  EFI_STATUS                      Status;
  EDKII_VARIABLE_POLICY_PROTOCOL  *PolicyProtocol;

  Status = gBS->LocateProtocol (
                  &gEdkiiVariablePolicyProtocolGuid,
                  NULL,
                  (VOID **)&PolicyProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to locate Variable policy protocol\r\n"));
    ASSERT (FALSE);
    return EFI_SECURITY_VIOLATION;
  }

  //
  // Lock all variables that are used for user authentication to make them
  // write protected for UEFI and only MM can change or delete them.
  //
  Status = RegisterBasicVariablePolicy (
             PolicyProtocol,
             &gUserAuthenticationGuid,
             NULL,
             VARIABLE_POLICY_NO_MIN_SIZE,
             VARIABLE_POLICY_NO_MAX_SIZE,
             VARIABLE_POLICY_NO_MUST_ATTR,
             VARIABLE_POLICY_NO_CANT_ATTR,
             VARIABLE_POLICY_TYPE_LOCK_NOW
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to lock Password variables - %r\r\n", Status));
    ASSERT (FALSE);
    return EFI_SECURITY_VIOLATION;
  }

  return EFI_SUCCESS;
}

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Request                A null-terminated Unicode string in
                                 <ConfigRequest> format.
  @param  Progress               On return, points to a character in the Request
                                 string. Points to the string's null terminator if
                                 request was successful. Points to the most recent
                                 '&' before the first failing name/value pair (or
                                 the beginning of the string if the failure is in
                                 the first name/value pair) if the request was not
                                 successful.
  @param  Results                A null-terminated Unicode string in
                                 <ConfigAltResp> format which has all values filled
                                 in for the names in the Request string. String to
                                 be allocated by the called function.

  @retval EFI_SUCCESS            The Results is filled with the requested values.
  @retval EFI_OUT_OF_RESOURCES   Not enough memory to store the results.
  @retval EFI_INVALID_PARAMETER  Request is illegal syntax, or unknown name.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
ExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Request,
  OUT EFI_STRING                            *Progress,
  OUT EFI_STRING                            *Results
  )
{
  if ((Progress == NULL) || (Results == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Request;
  return EFI_NOT_FOUND;
}

/**
  This function processes the results of changes in configuration.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Configuration          A null-terminated Unicode string in <ConfigResp>
                                 format.
  @param  Progress               A pointer to a string filled in with the offset of
                                 the most recent '&' before the first failing
                                 name/value pair (or the beginning of the string if
                                 the failure is in the first name/value pair) or
                                 the terminating NULL if all was successful.

  @retval EFI_SUCCESS            The Results is processed successfully.
  @retval EFI_INVALID_PARAMETER  Configuration is NULL.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
RouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Configuration,
  OUT EFI_STRING                            *Progress
  )
{
  if ((Configuration == NULL) || (Progress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Configuration;

  return EFI_NOT_FOUND;
}

/**
  HII update Admin Password status.

**/
VOID
HiiUpdateAdminPasswordStatus (
  VOID
  )
{
  if (IsPasswordInstalled ()) {
    HiiSetString (
      mUserAuthenticationData->HiiHandle,
      STRING_TOKEN (STR_ADMIN_PASSWORD_STS_CONTENT),
      L"Installed",
      NULL
      );
  } else {
    HiiSetString (
      mUserAuthenticationData->HiiHandle,
      STRING_TOKEN (STR_ADMIN_PASSWORD_STS_CONTENT),
      L"Not Installed",
      NULL
      );
  }
}

/**
  This function processes the results of changes in configuration.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Action                 Specifies the type of action taken by the browser.
  @param  QuestionId             A unique value which is sent to the original
                                 exporting driver so that it can identify the type
                                 of data to expect.
  @param  Type                   The type of value for the question.
  @param  Value                  A pointer to the data being sent to the original
                                 exporting driver.
  @param  ActionRequest          On return, points to the action requested by the
                                 callback function.

  @retval EFI_SUCCESS            The callback successfully handled the action.
  @retval EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the
                                 variable and its data.
  @retval EFI_DEVICE_ERROR       The variable could not be saved.
  @retval EFI_UNSUPPORTED        The specified Action is not supported by the
                                 callback.

**/
EFI_STATUS
EFIAPI
UserAuthenticationCallback (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  EFI_BROWSER_ACTION                    Action,
  IN  EFI_QUESTION_ID                       QuestionId,
  IN  UINT8                                 Type,
  IN  EFI_IFR_TYPE_VALUE                    *Value,
  OUT EFI_BROWSER_ACTION_REQUEST            *ActionRequest
  )
{
  EFI_STATUS  Status;
  CHAR16      *UserInputPassword;

  Status = EFI_SUCCESS;

  if (((Value == NULL) && (Action != EFI_BROWSER_ACTION_FORM_OPEN) && (Action != EFI_BROWSER_ACTION_FORM_CLOSE)) ||
      (ActionRequest == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  switch (Action) {
    case EFI_BROWSER_ACTION_FORM_OPEN:
    {
      switch (QuestionId) {
        case ADMIN_PASSWORD_KEY_ID:
          HiiUpdateAdminPasswordStatus ();
        default:
          break;
      }

      break;
    }
    case EFI_BROWSER_ACTION_CHANGING:
    {
      switch (QuestionId) {
        case ADMIN_PASSWORD_KEY_ID:
          if ((Type == EFI_IFR_TYPE_STRING) && (Value->string == 0) &&
              (mUserAuthenticationData->PasswordState == BROWSER_STATE_SET_PASSWORD))
          {
            mUserAuthenticationData->PasswordState = BROWSER_STATE_VALIDATE_PASSWORD;
            ZeroMem (mUserAuthenticationData->OldPassword, sizeof (mUserAuthenticationData->OldPassword));
            return EFI_INVALID_PARAMETER;
          }

          //
          // The Callback is responsible for validating old password input by user,
          // If Callback return EFI_SUCCESS, it indicates validation pass.
          //
          switch (mUserAuthenticationData->PasswordState) {
            case BROWSER_STATE_VALIDATE_PASSWORD:
              UserInputPassword = HiiGetString (mUserAuthenticationData->HiiHandle, Value->string, NULL);
              if ((StrLen (UserInputPassword) >= PASSWORD_MAX_SIZE)) {
                Status = EFI_NOT_READY;
                break;
              }

              if (UserInputPassword[0] == 0) {
                //
                // Setup will use a NULL password to check whether the old password is set,
                // If the validation is successful, means there is no old password, return
                // success to set the new password. Or need to return EFI_NOT_READY to
                // let user input the old password.
                //
                Status = VerifyPassword (UserInputPassword, StrSize (UserInputPassword));
                if (Status == EFI_SUCCESS) {
                  mUserAuthenticationData->PasswordState = BROWSER_STATE_SET_PASSWORD;
                } else {
                  Status = EFI_NOT_READY;
                }

                break;
              }

              Status = VerifyPassword (UserInputPassword, StrSize (UserInputPassword));
              if (Status == EFI_SUCCESS) {
                mUserAuthenticationData->PasswordState = BROWSER_STATE_SET_PASSWORD;
                StrCpyS (
                  mUserAuthenticationData->OldPassword,
                  sizeof (mUserAuthenticationData->OldPassword)/sizeof (CHAR16),
                  UserInputPassword
                  );
              } else {
                //
                // Old password mismatch, return EFI_NOT_READY to prompt for error message.
                //
                if (Status == EFI_ACCESS_DENIED) {
                  //
                  // Password retry count reach.
                  //
                  ForceSystemReset ();
                }

                Status = EFI_NOT_READY;
              }

              break;

            case BROWSER_STATE_SET_PASSWORD:
              UserInputPassword = HiiGetString (mUserAuthenticationData->HiiHandle, Value->string, NULL);
              if ((StrLen (UserInputPassword) >= PASSWORD_MAX_SIZE)) {
                Status = EFI_NOT_READY;
                break;
              }

              Status = SetPassword (UserInputPassword, StrSize (UserInputPassword), mUserAuthenticationData->OldPassword, StrSize (mUserAuthenticationData->OldPassword));
              PrintSetPasswordStatus (Status);
              ZeroMem (mUserAuthenticationData->OldPassword, sizeof (mUserAuthenticationData->OldPassword));
              mUserAuthenticationData->PasswordState = BROWSER_STATE_VALIDATE_PASSWORD;
              HiiUpdateAdminPasswordStatus ();
              break;

            default:
              break;
          }

        default:
          break;
      }

      break;
    }
    default:
      break;
  }

  return Status;
}

NVIDIA_USER_AUTH_PROTOCOL  mUserAuthenticationProtocol = {
  CheckForPassword
};

/**
  User Authentication entry point.

  @param ImageHandle     The image handle.
  @param SystemTable     The system table.

  @retval EFI_SUCCESS    The entry point is executed successfully.
  @return  other         Contain some other errors.

**/
EFI_STATUS
EFIAPI
UserAuthenticationEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS      Status;
  EFI_HANDLE      DriverHandle;
  EFI_HII_HANDLE  HiiHandle;

  DriverHandle = NULL;

  mUserAuthenticationData = AllocateZeroPool (sizeof (USER_AUTHENTICATION_PRIVATE_DATA));
  if (mUserAuthenticationData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  mUserAuthenticationData->ConfigAccess.ExtractConfig = ExtractConfig;
  mUserAuthenticationData->ConfigAccess.RouteConfig   = RouteConfig;
  mUserAuthenticationData->ConfigAccess.Callback      = UserAuthenticationCallback;
  mUserAuthenticationData->PasswordState              = BROWSER_STATE_VALIDATE_PASSWORD;

  //
  // Install Config Access protocol to driver handle.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &mUserAuthenticationData->ConfigAccess,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);
  mUserAuthenticationData->DriverHandle = DriverHandle;

  //
  // Add HII data to database.
  //
  HiiHandle = HiiAddPackages (
                &mUserAuthenticationVendorGuid,
                DriverHandle,
                UserAuthenticationDxeStrings,
                UserAuthenticationDxeVfrBin,
                NULL
                );
  if (HiiHandle == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  mUserAuthenticationData->HiiHandle = HiiHandle;

  //
  // Locates EFI MM Communication 2 protocol.
  //
  Status = gBS->LocateProtocol (&gEfiMmCommunication2ProtocolGuid, NULL, (VOID **)&mMmCommunication2);
  ASSERT_EFI_ERROR (Status);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIAUserAuthenticationProtocolGuid,
                  &mUserAuthenticationProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: install user authentication protocol failed: %r\n", __func__, Status));
    return Status;
  }

  //
  // Protect user password variables from being changed or erased without authentication.
  //
  Status = ProtectUserAuthenticationVariables ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fall through to allow driver to load.\n", __FUNCTION__));
  }

  //
  // BIOS password synchronization between BIOS and BMC.
  //
  Status = BiosPasswordSynchronization ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to sync BIOS password with BMC: %r\n", __func__, Status));
  }

  return EFI_SUCCESS;
}

/**
  Unloads the application and its installed protocol.

  @param[in]  ImageHandle       Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
**/
EFI_STATUS
EFIAPI
UserAuthenticationUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  ASSERT (mUserAuthenticationData != NULL);

  //
  // Uninstall Config Access Protocol.
  //
  if (mUserAuthenticationData->DriverHandle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           mUserAuthenticationData->DriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mHiiVendorDevicePath,
           &gEfiHiiConfigAccessProtocolGuid,
           &mUserAuthenticationData->ConfigAccess,
           NULL
           );
    mUserAuthenticationData->DriverHandle = NULL;
  }

  //
  // Remove Hii Data.
  //
  if (mUserAuthenticationData->HiiHandle != NULL) {
    HiiRemovePackages (mUserAuthenticationData->HiiHandle);
  }

  FreePool (mUserAuthenticationData);
  mUserAuthenticationData = NULL;

  return EFI_SUCCESS;
}
