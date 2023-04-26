/** @file
*  NVIDIA Secure Boot Provision
*
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Guid/ImageAuthentication.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <UefiSecureBoot.h>
#include <Library/SecureBootVariableLib.h>
#include <Library/SecureBootVariableProvisionLib.h>
#include <Library/BaseCryptLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

#define HASH_EXT  L"Hash"

/**
 * Utility function to determine if EnrollSecurityKeysApp should be run.
 * The function checks if the DT overlay file wants the App to be run and if
 * secure boot is currently enabled.
 *
 * @param[in]  None.
 *
 * @return TRUE   Run the EnrollSecurityKeysApp to Enroll keys.
 *         FALSE  Don't run the App.
 *
 **/
STATIC
BOOLEAN
OneTimeSecurityProvision (
  VOID
  )
{
  BOOLEAN     LaunchApp;
  EFI_STATUS  Status;
  UINT8       *EnrollDefaultKeys;
  UINT8       SetupMode;

  LaunchApp = FALSE;
  Status    = GetVariable2 (
                L"EnrollDefaultSecurityKeys",
                &gNVIDIAPublicVariableGuid,
                (VOID **)&EnrollDefaultKeys,
                NULL
                );
  if (!EFI_ERROR (Status)) {
    if (*EnrollDefaultKeys == 1) {
      Status = GetSetupMode (&SetupMode);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to get Setup Mode %r\n",
          __FUNCTION__,
          Status
          ));
      } else {
        if (SetupMode == USER_MODE) {
          DEBUG ((
            DEBUG_INFO,
            "%a: Skip EnrollDefaultKeys SetupMode %u\n",
            __FUNCTION__,
            SetupMode
            ));
        } else {
          DEBUG ((
            DEBUG_ERROR,
            "%a: EnrollDefaultKeys SetupMode %u\n",
            __FUNCTION__,
            SetupMode
            ));
          LaunchApp = TRUE;
        }
      }
    }
  }

  return LaunchApp;
}

/**
 * Run the EnrollKeysApp to enroll the secure boot keys from the default
 * variables.
 *
 * @param[in]  Context   Input from the EndOfDxe callback event.
 *
 * @return EFI_SUCCESS   Succesfully ran the EnrollKeysApp to enroll the secure
 *                       boot keys.
 *         Other         Failed to run the App.
 *
 **/
STATIC
EFI_STATUS
LaunchEnrollKeysApp (
  IN VOID  *Context
  )
{
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;
  EFI_HANDLE                         ImageHandle;
  EFI_HANDLE                         LoadedImageHandle;
  EFI_STATUS                         Status;

  ImageHandle = Context;
  Status      = gBS->HandleProtocol (
                       gImageHandle,
                       &gEfiLoadedImageProtocolGuid,
                       (VOID **)&LoadedImage
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to LoadedImageProtocol %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitLaunchEnrollKeysApp;
  }

  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  EfiInitializeFwVolDevicepathNode (&FileNode, &gEnrollFromDefaultKeysAppFileGuid);

  if (DevicePath == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to Init DevicePath\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitLaunchEnrollKeysApp;
  }

  DevicePath = AppendDevicePathNode (
                 DevicePath,
                 (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
                 );
  if (DevicePath == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to Append DevicePath\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitLaunchEnrollKeysApp;
  }

  Status = gBS->LoadImage (
                  FALSE,
                  ImageHandle,
                  DevicePath,
                  NULL,
                  0,
                  &LoadedImageHandle
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:Failed to Load %r\n", __FUNCTION__, Status));
    goto ExitLaunchEnrollKeysApp;
  }

  Status = gBS->StartImage (LoadedImageHandle, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:Failed to Start %r\n", __FUNCTION__, Status));
    goto ExitLaunchEnrollKeysApp;
  }

ExitLaunchEnrollKeysApp:
  return Status;
}

/**
 * Get Hash Variable Name.
 *
 * Get the name of the Variable that stores the Hash value of the Variable.
 *
 * @param[in]      InputVarName       Input Variable name to get updated key data.
 * @param[in,out]  ComputedHashValue  Hash var name
 *
 * @return TRUE  Update the Variable.
 *         FALSE Don't update the secure Variable.
 **/
STATIC
EFI_STATUS
GetHashVarName (
  IN  CHAR16  *InputVarName,
  OUT CHAR16  **HashVarName
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  CHAR16      *InputVarHashName;
  UINTN       InputVarHashNameSize;

  InputVarHashNameSize = StrSize (InputVarName) + StrSize (HASH_EXT) + 1;
  DEBUG ((
    DEBUG_ERROR,
    "%a: InputVarHashNameSize %u\n",
    __FUNCTION__,
    InputVarHashNameSize
    ));

  InputVarHashName = AllocateZeroPool (InputVarHashNameSize);
  if (InputVarHashName == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Allocate Memory for FileName\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitGetHashVarName;
  }

  UnicodeSPrint (InputVarHashName, InputVarHashNameSize, L"%s%s", InputVarName, HASH_EXT);
  *HashVarName = InputVarHashName;
ExitGetHashVarName:
  return Status;
}

/**
 * Check if a secure variable key should be updated.
 *
 * Compute the hash of the secure variable key and check if it matches the
 * hash stored for the secyre variable.
 *
 * @param[in]  InputVarName       Input Variable name to get updated key data.
 * @param[in]  InputHashVarName   Name of the Hash variable
 * @param[in]  SignedPayload      Secure Variable Payload
 * @param[in]  SignedPayloadSize  Payload Size
 * @param[in]  ComputedHashValue  Hash value computed for the secure payload.
 *
 * @return TRUE  Update the Variable.
 *         FALSE Don't update the secure Variable.
 **/
STATIC
BOOLEAN
UpdateSecVar (
  IN CHAR16  *InputVarName,
  IN CHAR16  *InputHashVarName,
  IN VOID    *SignedPayload,
  IN UINTN   SignedPayloadSize,
  IN UINT8   *ComputedHashValue
  )
{
  BOOLEAN     UpdateVar = TRUE;
  UINT8       *StoredHashValue;
  UINTN       StoredHashValueSize;
  EFI_STATUS  Status;

  Status = GetVariable2 (
             InputHashVarName,
             &gNVIDIATokenSpaceGuid,
             (VOID **)&StoredHashValue,
             &StoredHashValueSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:%d Failed to get %s %r\n", __FUNCTION__, __LINE__, InputVarName, Status));
    goto ExitUpdateSecVar;
  }

  if (StoredHashValueSize != SHA256_DIGEST_SIZE) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid Hash Size %u\n", __FUNCTION__, StoredHashValueSize));
    goto ExitUpdateSecVar;
  }

  if (CompareMem (StoredHashValue, ComputedHashValue, SHA256_DIGEST_SIZE) == 0) {
    DEBUG ((DEBUG_INFO, "%a:%s Same Keys , Hash values match\n", __FUNCTION__, InputVarName));
    UpdateVar = FALSE;
  }

ExitUpdateSecVar:
  return UpdateVar;
}

/**
 * Append to Secure Boot keys using signed payloads.
 * Even though the Security Libraries should be able to handle duplicates
 * avoid making these calls if we know the payload has already been processed
 * by keeping a hash of the last processed payload.
 *
 * @param[in]  InputVarName  Input Variable name to get updated key data.
 * @param[in]  InputVarGuid  Guid to use to lookup the Input key data.
 * @param[in]  SecDbToUpdate Security Key to update.
 * @param[in]  SecDbGuid     Guid of Security Key to update.
 *
 * @return EFI_SUCCESS  Variable Successfully updated.
 *         Other        Failed to update variable.
 **/
STATIC
EFI_STATUS
AppendKeys (
  IN CHAR16    *InputVarName,
  IN EFI_GUID  *InputVarGuid,
  IN CHAR16    *SecDbToUpdate,
  IN EFI_GUID  *SecDbGuid
  )
{
  EFI_STATUS  Status;
  VOID        *SignedPayload;
  UINTN       SignedPayloadSize;
  UINT8       ComputedHashValue[SHA256_DIGEST_SIZE];
  BOOLEAN     UpdateSecDb;
  CHAR16      *HashVarName = NULL;

  Status = GetVariable2 (
             InputVarName,
             InputVarGuid,
             (VOID **)&SignedPayload,
             &SignedPayloadSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to get %s %r\n",
      __FUNCTION__,
      InputVarName,
      Status
      ));
  } else {
    /* Check if this payload has been handled before */
    Status = GetHashVarName (InputVarName, &HashVarName);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get HashVarName %r\n",
        __FUNCTION__,
        Status
        ));
      goto ExitAppendKeys;
    }

    if (Sha256HashAll (
          (VOID *)SignedPayload,
          SignedPayloadSize,
          ComputedHashValue
          ) == FALSE)
    {
      DEBUG ((DEBUG_ERROR, "%a: Failed to compute SHA256 Hash\n", __FUNCTION__));
      goto ExitAppendKeys;
    }

    UpdateSecDb = UpdateSecVar (
                    InputVarName,
                    HashVarName,
                    SignedPayload,
                    SignedPayloadSize,
                    ComputedHashValue
                    );

    if (UpdateSecDb == TRUE) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Updating %s\n",
        __FUNCTION__,
        SecDbToUpdate
        ));

      Status = gRT->SetVariable (
                      SecDbToUpdate,
                      SecDbGuid,
                      (EFI_VARIABLE_NON_VOLATILE |
                       EFI_VARIABLE_BOOTSERVICE_ACCESS |
                       EFI_VARIABLE_RUNTIME_ACCESS |
                       EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS |
                       EFI_VARIABLE_APPEND_WRITE),
                      SignedPayloadSize,
                      SignedPayload
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to update %s %r\n", SecDbToUpdate, Status));
        goto ExitAppendKeys;
      }

      /* Store the hash of the payload that was used to update the secure key */
      Status = gRT->SetVariable (
                      HashVarName,
                      &gNVIDIATokenSpaceGuid,
                      (EFI_VARIABLE_NON_VOLATILE |
                       EFI_VARIABLE_BOOTSERVICE_ACCESS),
                      SHA256_DIGEST_SIZE,
                      ComputedHashValue
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Faile to update %s %r\n", HashVarName, Status));
      }
    }
  }

ExitAppendKeys:
  return Status;
}

/**
 * NvSecureBootUpdateSignedKeys
 *
 * Append keys to secure variable keys. If Secure Boot is
 * enabled then look for signed payloads obtained from the
 * Secure Keys overlay file to append to existing Secure Boot
 * keys.
 * Look for signed db/dbx payloads signed either by OEM Kek (dbSignedOem/
 * dbxSignedOem) OR Microsoft KEK (dbxSignedMsft/dbSignedMsft).
 * Look for a signed KEK payload as well (signed by OEM PK).
 *
 * @retval None
**/
STATIC
VOID
NvSecureBootUpdateSignedKeys (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT8       SetupMode;

  Status = GetSetupMode (&SetupMode);

  if (!EFI_ERROR (Status)) {
    if (SetupMode == USER_MODE) {
      AppendKeys (
        L"dbxSignedMsft",
        &gNVIDIAPublicVariableGuid,
        EFI_IMAGE_SECURITY_DATABASE1,
        &gEfiImageSecurityDatabaseGuid
        );

      AppendKeys (
        L"dbxSignedOem",
        &gNVIDIAPublicVariableGuid,
        EFI_IMAGE_SECURITY_DATABASE1,
        &gEfiImageSecurityDatabaseGuid
        );

      AppendKeys (
        L"dbSignedOem",
        &gNVIDIAPublicVariableGuid,
        EFI_IMAGE_SECURITY_DATABASE,
        &gEfiImageSecurityDatabaseGuid
        );

      AppendKeys (
        L"dbSignedMsft",
        &gNVIDIAPublicVariableGuid,
        EFI_IMAGE_SECURITY_DATABASE,
        &gEfiImageSecurityDatabaseGuid
        );

      AppendKeys (
        L"kekSignedOem",
        &gNVIDIAPublicVariableGuid,
        EFI_KEY_EXCHANGE_KEY_NAME,
        &gEfiGlobalVariableGuid
        );
    }
  }
}

/**
  EndOfDxe callback function.

  This function is the callback from EndOfDxe and tries to run the Enroll
  Security Keys and updates the keys if needed.

  @param[in]  Event     Callback Event pointer.
  @param[in]  Context   User Data passed as part of the callback.

  @retval None

**/
STATIC
VOID
EFIAPI
NvSecureBootProvisionEndOfDxe (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  gBS->CloseEvent (Event);

  if (OneTimeSecurityProvision () == TRUE) {
    Status = LaunchEnrollKeysApp (Context);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to Launch EnrollKeysApp %r\n",
        __FUNCTION__,
        Status
        ));
      goto ExitNvSecureBootProvisionEndOfDxe;
    }
  }

  NvSecureBootUpdateSignedKeys ();

ExitNvSecureBootProvisionEndOfDxe:
  return;
}

/**
  Entrypoint of this module.

  This function is the entrypoint of this module. It installs a EndOfDxe
  Callback function.

  @param[in]  ImageHandle       The firmware allocated handle for the EFI image.
  @param[in] SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
          Other             Failed to register EndOfDxe notification.

**/
EFI_STATUS
EFIAPI
NvSecureBootProvisionDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   EndOfDxeEvent;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  NvSecureBootProvisionEndOfDxe,
                  (VOID *)ImageHandle,
                  &gEfiEndOfDxeEventGroupGuid,
                  &EndOfDxeEvent
                  );

  return Status;
}
