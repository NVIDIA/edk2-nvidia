/** @file
  IPMI password OEM commands.

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UserAuthenticationIpmi.h"
#include "UserAuthenticationDxe.h"

/**
  Debug dump binary hash data.

  @param[in]      Message     Additional debug message.
  @param[in]      Data        Pointer to hash data.
  @param[in]      DataSize    Size of Data in byte.

  @retval EFI_SUCCESS           Dump Data successfully.
  @retval EFI_INVALID_PARAMETER Data is NULL or DataSize is 0.

**/
EFI_STATUS
DumpBiosPasswordHash (
  IN CONST CHAR8  *Message  OPTIONAL,
  IN UINT8        *Data,
  IN UINTN        DataSize
  )
{
  UINTN  Index;

  if ((Data == NULL) || (DataSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Message != NULL) && (Message[0] != '\0')) {
    DEBUG ((DEBUG_ERROR, "%a\n", Message));
  }

  for (Index = 0; Index < DataSize; Index++) {
    DEBUG ((DEBUG_ERROR, " 0x%02X", Data[Index]));
  }

  DEBUG ((DEBUG_ERROR, "\n"));

  return EFI_SUCCESS;
}

/**
  Debug dump IPMI_OEM_GET_BIOS_PASSWORD_RESPONSE_DATA structure.

  @param[in]      Message         Additional debug message.
  @param[in]      PasswdResponse  Pointer to password data.

  @retval EFI_SUCCESS           Dump PasswdResponse successfully.
  @retval EFI_INVALID_PARAMETER PasswdResponse is NULL.

**/
EFI_STATUS
DumpIpmiBiosPasswordResponse (
  IN CONST CHAR8                               *Message  OPTIONAL,
  IN IPMI_OEM_GET_BIOS_PASSWORD_RESPONSE_DATA  *PasswdResponse
  )
{
  if (PasswdResponse == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Message != NULL) && (Message[0] != '\0')) {
    DEBUG ((DEBUG_ERROR, "%a\n", Message));
  }

  DEBUG ((DEBUG_ERROR, " CompletionCode: 0x%x\n", PasswdResponse->CompletionCode));
  DEBUG ((DEBUG_ERROR, " Action: 0x%x\n", PasswdResponse->PasswordAction));
  DumpBiosPasswordHash ("Salt:", PasswdResponse->PasswordSalt, BIOS_PASSWORD_SALT_SIZE);
  DumpBiosPasswordHash ("Hash:", PasswdResponse->PasswordHash, BIOS_PASSWORD_HASH_SIZE);

  return EFI_SUCCESS;
}

/**
  Debug dump IPMI_OEM_SET_BIOS_PASSWORD_REQUEST_DATA structure.

  @param[in]      Message       Additional debug message.
  @param[in]      PasswdRequest Pointer to password data.

  @retval EFI_SUCCESS           Dump PasswdRequest successfully.
  @retval EFI_INVALID_PARAMETER PasswdRequest is NULL.

**/
EFI_STATUS
DumpIpmiBiosPasswordRequest (
  IN CONST CHAR8                              *Message  OPTIONAL,
  IN IPMI_OEM_SET_BIOS_PASSWORD_REQUEST_DATA  *PasswdRequest
  )
{
  if (PasswdRequest == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Message != NULL) && (Message[0] != '\0')) {
    DEBUG ((DEBUG_ERROR, "%a\n", Message));
  }

  DEBUG ((DEBUG_ERROR, " ID selector: 0x%x\n", PasswdRequest->PasswordIdSelector));
  DEBUG ((DEBUG_ERROR, " Type: 0x%x\n", PasswdRequest->PasswordType));
  DumpBiosPasswordHash ("Salt:", PasswdRequest->PasswordSalt, BIOS_PASSWORD_SALT_SIZE);
  DumpBiosPasswordHash ("Hash:", PasswdRequest->PasswordHash, BIOS_PASSWORD_HASH_SIZE);

  return EFI_SUCCESS;
}

/**
  Set BIOS password to BMC via IPMI OEM command.

  @param[in]   IdSelector       Password ID selector.
  @param[in]   PasswordType     Pointer to password type.
  @param[in]   PasswordSalt     Pointer to password salt.
  @param[in]   PasswordSaltSize The size of PasswordSalt buffer.
  @param[in]   PasswordHash     Pointer to password hash.
  @param[in]   PasswordHashSize The size of PasswordHash buffer.

  @retval EFI_SUCCESS     Set BIOS password successfully.
  @retval EFI_UNSUPPORTED The ID selector is not supported.

**/
EFI_STATUS
IpmiOemSetBiosPassword (
  IN  UINT8 IdSelector,
  IN  UINT8 PasswordType,
  IN  UINT8 *PasswordSalt, OPTIONAL
  IN  UINTN PasswordSaltSize,
  IN  UINT8 *PasswordHash, OPTIONAL
  IN  UINTN PasswordHashSize
  )
{
  IPMI_OEM_SET_BIOS_PASSWORD_REQUEST_DATA   RequestData;
  IPMI_OEM_SET_BIOS_PASSWORD_RESPONSE_DATA  ResponseData;
  UINT32                                    ResponseSize;
  EFI_STATUS                                Status;

  if (IdSelector != BIOS_PASSWORD_SELECTOR_ADMIN) {
    return EFI_UNSUPPORTED;
  }

  if (((PasswordType == BIOS_PASSWORD_TYPE_PBKDF2_SHA256) || (PasswordType == BIOS_PASSWORD_TYPE_PBKDF2_SHA384)) &&
      ((PasswordSalt == NULL) || (PasswordHash == NULL) || (PasswordSaltSize == 0) || (PasswordHashSize == 0)))
  {
    return EFI_INVALID_PARAMETER;
  }

  if ((PasswordSaltSize > BIOS_PASSWORD_SALT_SIZE) || (PasswordHashSize > BIOS_PASSWORD_HASH_SIZE)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&RequestData, sizeof (IPMI_OEM_SET_BIOS_PASSWORD_REQUEST_DATA));
  ZeroMem (&ResponseData, sizeof (IPMI_OEM_SET_BIOS_PASSWORD_RESPONSE_DATA));

  RequestData.PasswordIdSelector = IdSelector;
  RequestData.PasswordType       = PasswordType;

  if ((PasswordType == BIOS_PASSWORD_TYPE_PBKDF2_SHA256) || (PasswordType == BIOS_PASSWORD_TYPE_PBKDF2_SHA384)) {
    CopyMem (RequestData.PasswordSalt, PasswordSalt, PasswordSaltSize);
    CopyMem (RequestData.PasswordHash, PasswordHash, PasswordHashSize);
  }

  DEBUG_CODE_BEGIN ();
  DumpIpmiBiosPasswordRequest (__func__, &RequestData);
  DEBUG_CODE_END ();

  ResponseSize = sizeof (IPMI_OEM_SET_BIOS_PASSWORD_RESPONSE_DATA);
  Status       = IpmiSubmitCommand (
                   IPMI_NETFN_OEM,
                   IPMI_OEM_SET_BIOS_PASSWORD,
                   (UINT8 *)&RequestData,
                   sizeof (RequestData),
                   (UINT8 *)&ResponseData,
                   &ResponseSize
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: IPMI_OEM_SET_BIOS_PASSWORD error: %r\n", __func__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __func__, ResponseData.CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Get BIOS password to BMC via IPMI OEM command. It is the
  caller's responsibility to free "PasswordSalt" and "PasswordHash"
  with EFI_BOOT_SERVICES.FreePool().

  @param[in]    IdSelector       Password ID selector.
  @param[out]   PasswordAction   Pointer to password action.
  @param[out]   PasswordSalt     Pointer to password salt.
  @param[out]   PasswordHash     Pointer to password hash.

  @retval EFI_SUCCESS     Set BIOS password successfully.
  @retval EFI_UNSUPPORTED The ID selector is not supported.

**/
EFI_STATUS
IpmiOemGetBiosPassword (
  IN  UINT8  IdSelector,
  OUT UINT8  *PasswordAction,
  OUT UINT8  **PasswordSalt,
  OUT UINT8  **PasswordHash
  )
{
  IPMI_OEM_GET_BIOS_PASSWORD_REQUEST_DATA   RequestData;
  IPMI_OEM_GET_BIOS_PASSWORD_RESPONSE_DATA  ResponseData;
  UINT32                                    ResponseSize;
  EFI_STATUS                                Status;

  if ((PasswordAction == NULL) || (PasswordSalt == NULL) || (PasswordHash == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (IdSelector != BIOS_PASSWORD_SELECTOR_ADMIN) {
    return EFI_UNSUPPORTED;
  }

  *PasswordAction = BIOS_PASSWORD_ACTION_NO_CHANGE;
  *PasswordSalt   = NULL;
  *PasswordHash   = NULL;

  ZeroMem (&ResponseData, sizeof (IPMI_OEM_GET_BIOS_PASSWORD_RESPONSE_DATA));
  ResponseSize                   = sizeof (IPMI_OEM_GET_BIOS_PASSWORD_RESPONSE_DATA);
  RequestData.PasswordIdSelector = IdSelector;
  Status                         = IpmiSubmitCommand (
                                     IPMI_NETFN_OEM,
                                     IPMI_OEM_GET_BIOS_PASSWORD,
                                     (UINT8 *)&RequestData,
                                     sizeof (RequestData),
                                     (UINT8 *)&ResponseData,
                                     &ResponseSize
                                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: IPMI_OEM_GET_BIOS_PASSWORD error: %r\n", __func__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __func__, ResponseData.CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  *PasswordAction = ResponseData.PasswordAction;
  if ((ResponseData.PasswordAction == BIOS_PASSWORD_ACTION_CHANGED_PBKDF2_SHA256) ||
      (ResponseData.PasswordAction == BIOS_PASSWORD_ACTION_CHANGED_PBKDF2_SHA384))
  {
    *PasswordSalt = AllocateCopyPool (BIOS_PASSWORD_SALT_SIZE, ResponseData.PasswordSalt);
    if (*PasswordSalt == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    *PasswordHash = AllocateCopyPool (BIOS_PASSWORD_SALT_SIZE, ResponseData.PasswordHash);
    if (*PasswordHash == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  DEBUG_CODE_BEGIN ();
  DumpIpmiBiosPasswordResponse (__func__, &ResponseData);
  DEBUG_CODE_END ();

  return EFI_SUCCESS;
}

/**
  Sync BIOS password between BIOS and BMC.

  @retval EFI_SUCCESS     Set BIOS password successfully.

**/
EFI_STATUS
BiosPasswordSynchronization (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT8       PasswordAction;
  UINT8       *PasswordSalt;
  UINT8       *PasswordHash;
  UINTN       SaltSize;
  UINTN       HashSize;

  PasswordSalt = NULL;
  PasswordHash = NULL;
  SaltSize     = 0;
  HashSize     = 0;

  DEBUG ((IPMI_BIOS_PASSWORD_DEBUG, "%a: perform BIOS password synchronization with BMC\n", __func__));

  //
  // Get BIOS password from BMC
  //
  Status = IpmiOemGetBiosPassword (
             BIOS_PASSWORD_SELECTOR_ADMIN,
             &PasswordAction,
             &PasswordSalt,
             &PasswordHash
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: can not get BIOS password from BMC: %r\n", __func__, Status));
    return Status;
  }

  //
  // Apply BIOS password change if requested.
  //
  DEBUG ((IPMI_BIOS_PASSWORD_DEBUG, "%a: apply BIOS password action: 0x%x from BMC\n", __func__, PasswordAction));
  switch (PasswordAction) {
    case BIOS_PASSWORD_ACTION_CLEAR_PASSWD:
      Status = SetPasswordHash (NULL, 0x00, NULL, 0x00);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: failed to clear password: %r\n", __func__, Status));
      }

      break;
    case BIOS_PASSWORD_ACTION_CHANGED_PBKDF2_SHA256:
      Status = SetPasswordHash (PasswordSalt, BIOS_PASSWORD_SALT_SIZE, PasswordHash, BIOS_PASSWORD_HASH_SHA256_SIZE);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: failed to set SHA256 password hash: %r\n", __func__, Status));
      }

      break;
    case BIOS_PASSWORD_ACTION_CHANGED_PBKDF2_SHA384:
      Status = SetPasswordHash (PasswordSalt, BIOS_PASSWORD_SALT_SIZE, PasswordHash, BIOS_PASSWORD_HASH_SHA384_SIZE);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: failed to set SHA384 password hash: %r\n", __func__, Status));
      }

      break;
    case BIOS_PASSWORD_ACTION_NO_CHANGE:
    default:
      DEBUG ((IPMI_BIOS_PASSWORD_DEBUG, "%a: no BIOS password change requested\n", __func__));
      break;
  }

  DEBUG ((IPMI_BIOS_PASSWORD_DEBUG, "%a: apply BIOS password from BMC successfully\n", __func__));

  if (PasswordSalt != NULL) {
    FreePool (PasswordSalt);
    PasswordSalt = NULL;
  }

  if (PasswordHash != NULL) {
    FreePool (PasswordHash);
    PasswordHash = NULL;
  }

  //
  // Populate BIOS password to BMC
  //
  DEBUG ((IPMI_BIOS_PASSWORD_DEBUG, "%a: populate BIOS password to BMC\n", __func__));

  SaltSize     = BIOS_PASSWORD_SALT_SIZE;
  PasswordSalt = AllocateZeroPool (BIOS_PASSWORD_SALT_SIZE);
  if (PasswordSalt == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  HashSize     = BIOS_PASSWORD_HASH_SHA256_SIZE;
  PasswordHash = AllocateZeroPool (BIOS_PASSWORD_HASH_SHA256_SIZE);
  if (PasswordHash == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RELEASE;
  }

  //
  // Get BIOS password from MM
  //
  Status = GetPasswordHash (PasswordSalt, &SaltSize, PasswordHash, &HashSize);
  if (EFI_ERROR (Status)) {
    //
    // Failed to get password. Check to see if password is set or not.
    //
    if (IsPasswordInstalled ()) {
      DEBUG ((DEBUG_ERROR, "%a: can not get BIOS password hash: %r\n", __func__, Status));
      goto RELEASE;
    }

    //
    // There is no BIOS password set in system.
    //
    DEBUG ((IPMI_BIOS_PASSWORD_DEBUG, "%a: tell BMC there is no BIOS password\n", __func__));
    Status = IpmiOemSetBiosPassword (BIOS_PASSWORD_SELECTOR_ADMIN, BIOS_PASSWORD_TYPE_NO_PASSWD, NULL, 0, NULL, 0);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: can not set BIOS password to BMC: %r\n", __func__, Status));
      goto RELEASE;
    }
  } else {
    DEBUG ((IPMI_BIOS_PASSWORD_DEBUG, "%a: tell BMC that BIOS password is set.\n", __func__));
    Status = IpmiOemSetBiosPassword (BIOS_PASSWORD_SELECTOR_ADMIN, BIOS_PASSWORD_TYPE_PBKDF2_SHA256, PasswordSalt, SaltSize, PasswordHash, HashSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: can not set BIOS password to BMC: %r\n", __func__, Status));
      goto RELEASE;
    }
  }

  DEBUG ((IPMI_BIOS_PASSWORD_DEBUG, "%a: populate BIOS password to BMC successfully\n", __func__));

RELEASE:

  if (PasswordSalt != NULL) {
    FreePool (PasswordSalt);
    PasswordSalt = NULL;
  }

  if (PasswordHash != NULL) {
    FreePool (PasswordHash);
    PasswordHash = NULL;
  }

  return Status;
}
