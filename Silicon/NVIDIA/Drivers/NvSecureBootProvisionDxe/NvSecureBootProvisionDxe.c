/** @file
*  NVIDIA Secure Boot Provision
*
* SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <Library/AuthVariableLib.h>

#define NUM_DBS          (3)
#define MAX_DB_STRLEN    (10)
#define MAX_SIGNED_KEYS  (50)
#define MAX_KEY_STRLEN   (14)
#define HASH_EXT         L"Hash"

typedef struct {
  CHAR16      SignedCertName[MAX_KEY_STRLEN];
  CHAR16      CertName[MAX_KEY_STRLEN];
  EFI_GUID    *CertGuid;
  CHAR16      DefaultName[MAX_KEY_STRLEN];
  EFI_GUID    *DefaultGuid;
} SignedKeysType;

STATIC VOID  *Registration = NULL;

STATIC SignedKeysType  SupportedKeys[NUM_DBS] = {
  [0] = {
    .SignedCertName = L"dbxSigned",
    .CertName       = L"dbx",
    .CertGuid       = &gEfiImageSecurityDatabaseGuid,
    .DefaultName    = L"dbxDefault",
    .DefaultGuid    = &gEfiGlobalVariableGuid
  },
  [1] = {
    .SignedCertName = L"dbSigned",
    .CertName       = L"db",
    .CertGuid       = &gEfiImageSecurityDatabaseGuid,
    .DefaultName    = L"dbDefault",
    .DefaultGuid    = &gEfiGlobalVariableGuid
  },
  [2] = {
    .SignedCertName = L"kekSigned",
    .CertName       = L"KEK",
    .CertGuid       = &gEfiGlobalVariableGuid,
    .DefaultName    = L"KEKDefault",
    .DefaultGuid    = &gEfiGlobalVariableGuid
  },
};

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
    DEBUG ((DEBUG_INFO, "%a:%d Failed to get %s %r\n", __FUNCTION__, __LINE__, InputHashVarName, Status));
    goto ExitUpdateSecVar;
  }

  if (StoredHashValueSize != SHA256_DIGEST_SIZE) {
    DEBUG ((DEBUG_INFO, "%a: Invalid Hash Size %u\n", __FUNCTION__, StoredHashValueSize));
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
 * GetPayloadFromSigned
 *
 * Util functions to get the signed payload (get past the auth header).
 *
 * @param[in]   SignedPayload      Input Signed Pointer.
 * @param[in]   SignedPayloadSize  Input Signed Payload Size.
 * @param[out]  PayloadPtr         Output Payload Pointer.
 * @param[out]  PayloadSize        Size of Signed payload.
 **/
STATIC
VOID
GetPayloadFromSigned (
  IN  VOID   *SignedPayload,
  IN  UINTN  SignedPayloadSize,
  OUT UINT8  **PayloadPtr,
  OUT UINTN  *PayloadSize
  )
{
  EFI_VARIABLE_AUTHENTICATION_2  *CertData;
  UINT8                          *SigData;
  UINT32                         SigDataSize;

  CertData = (EFI_VARIABLE_AUTHENTICATION_2 *)SignedPayload;

  if ((CertData->AuthInfo.Hdr.wCertificateType != WIN_CERT_TYPE_EFI_GUID) ||
      !CompareGuid (&CertData->AuthInfo.CertType, &gEfiCertPkcs7Guid))
  {
    DEBUG ((DEBUG_ERROR, "No Valid Signature Data Found\n"));
    *PayloadPtr  = SignedPayload;
    *PayloadSize = SignedPayloadSize;
  } else {
    SigData      = CertData->AuthInfo.CertData;
    SigDataSize  = CertData->AuthInfo.Hdr.dwLength - (UINT32)(OFFSET_OF (WIN_CERTIFICATE_UEFI_GUID, CertData));
    *PayloadPtr  = SigData + SigDataSize;
    *PayloadSize = SignedPayloadSize - OFFSET_OF_AUTHINFO2_CERT_DATA - (UINTN)SigDataSize;
  }

  DEBUG ((DEBUG_INFO, "%a:PayloadSize %u\n", __FUNCTION__, *PayloadSize));
}

/**
 Append to the default secure boot keys.
 Util function to take a signed key variable, strip out the
 header and append to existing default variables to enable
 secure boot, this function is called when secure boot hasn't been
 enabled.

 @param[in] DefaultName        Name of the Default Variable (e.g dbdefault).
 @param[in] DefaultGuid        Guid that the Default var belongs to.
 @param[in] SignedPayload      Pointer to the signed payload.
 @param[in] SignedPayloadSize  Size of the signed payload.

 @return EFI_SUCCESS  On Success.
         other        Failure.
 **/
STATIC
EFI_STATUS
AppendToDefault (
  IN CHAR16    *DefaultName,
  IN EFI_GUID  *DefaultGuid,
  IN VOID      *SignedPayload,
  IN UINTN     SignedPayloadSize
  )
{
  EFI_STATUS  Status;
  UINT8       *PayloadPtr;
  UINTN       PayloadSize;
  UINT32      Attributes;
  UINTN       DataSize = 0;

  GetPayloadFromSigned (
    SignedPayload,
    SignedPayloadSize,
    &PayloadPtr,
    &PayloadSize
    );

  Status = gRT->GetVariable (
                  DefaultName,
                  DefaultGuid,
                  &Attributes,
                  &DataSize,
                  NULL
                  );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Attributes |= EFI_VARIABLE_APPEND_WRITE;
  } else {
    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS;
  }

  Status = gRT->SetVariable (
                  DefaultName,
                  DefaultGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  PayloadSize,
                  PayloadPtr
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to update %s %r\n", __FUNCTION__, DefaultName, Status));
  }

  return Status;
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
  IN EFI_GUID  *SecDbGuid,
  IN CHAR16    *DefaultVarName OPTIONAL,
  IN EFI_GUID  *DefaultVarGuid OPTIONAL,
  IN UINT8     SetupMode
  )
{
  EFI_STATUS  Status;
  VOID        *SignedPayload;
  UINTN       SignedPayloadSize;
  UINT8       ComputedHashValue[SHA256_DIGEST_SIZE];
  BOOLEAN     UpdateSecDb;
  CHAR16      *HashVarName = NULL;
  UINT8       *PayloadPtr;
  UINTN       PayloadSize;
  UINT32      Attributes;
  UINTN       VarSize;

  PayloadPtr  = NULL;
  PayloadSize = 0;
  Status      = GetVariable2 (
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
      VarSize = 0;

      /*
       * If we're here then we haven't processed the signed payload and we
       * need to append it to the secure keys.
       * If secure boot has been enabled then append to the key db variable
       * else append/create the default key variable.
       */
      if (SetupMode == USER_MODE) {
        Attributes = (EFI_VARIABLE_NON_VOLATILE |
                      EFI_VARIABLE_BOOTSERVICE_ACCESS |
                      EFI_VARIABLE_RUNTIME_ACCESS |
                      EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS);
        GetPayloadFromSigned (
          SignedPayload,
          SignedPayloadSize,
          &PayloadPtr,
          &PayloadSize
          );
        /* If the payload size is 0, then don't add the Append_write attribute */
        if (PayloadSize != 0) {
          Attributes |= EFI_VARIABLE_APPEND_WRITE;
        }

        Status = gRT->SetVariable (
                        SecDbToUpdate,
                        SecDbGuid,
                        Attributes,
                        SignedPayloadSize,
                        SignedPayload
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to update %s %r\n", SecDbToUpdate, Status));
          goto ExitAppendKeys;
        }
      } else {
        Status = AppendToDefault (
                   DefaultVarName,
                   DefaultVarGuid,
                   SignedPayload,
                   SignedPayloadSize
                   );
      }

      /* Store the hash of the payload that was used to update the secure key */
      if (!EFI_ERROR (Status)) {
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
  CHAR16      KeyName[MAX_KEY_STRLEN];
  UINTN       Index;
  UINTN       KeyIdx;

  Status = GetSetupMode (&SetupMode);

  if (!EFI_ERROR (Status)) {
    for (Index = 0; Index < NUM_DBS; Index++) {
      for (KeyIdx = 0; KeyIdx < MAX_SIGNED_KEYS; KeyIdx++) {
        ZeroMem (KeyName, (sizeof (CHAR16) * MAX_KEY_STRLEN));
        UnicodeSPrint (
          KeyName,
          (sizeof (CHAR16) * MAX_KEY_STRLEN),
          L"%s_%u",
          SupportedKeys[Index].SignedCertName,
          KeyIdx
          );

        Status = AppendKeys (
                   KeyName,
                   &gNVIDIAPublicVariableGuid,
                   SupportedKeys[Index].CertName,
                   SupportedKeys[Index].CertGuid,
                   SupportedKeys[Index].DefaultName,
                   SupportedKeys[Index].DefaultGuid,
                   SetupMode
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_INFO,
            "%a:%d Key %s Status %r\n",
            __FUNCTION__,
            __LINE__,
            KeyName,
            Status
            ));
          if (Status == EFI_NOT_FOUND) {
            break;
          }
        }
      }
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
  Default Variable Driver Ready Callback

  This function is the callback after the Default Variable Driver has
  run.
  Security Keys and updates the keys if needed.

  @param[in]  Event     Callback Event pointer.
  @param[in]  Context   User Data passed as part of the callback.

  @retval None

**/
STATIC
VOID
DefaultVarDriverReady (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  NvSecureBootUpdateSignedKeys ();
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
  EFI_EVENT   NotifyEvent;

  /**
   * Notification for when the Default Variable Driver is done.
   * Need that driver to parse the dtbo first.
   **/
  NotifyEvent = EfiCreateProtocolNotifyEvent (
                  &gNVIDIADefaultVarDoneGuid,
                  TPL_CALLBACK,
                  DefaultVarDriverReady,
                  NULL,
                  &Registration
                  );
  if (NotifyEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create notify event\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitNvSecureBootProvisionDxeInitialize;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  NvSecureBootProvisionEndOfDxe,
                  (VOID *)ImageHandle,
                  &gEfiEndOfDxeEventGroupGuid,
                  &EndOfDxeEvent
                  );

ExitNvSecureBootProvisionDxeInitialize:
  return Status;
}
