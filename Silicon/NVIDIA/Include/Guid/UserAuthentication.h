/** @file
  GUID is for UserAuthentication MM communication.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __USER_AUTHENTICATION_GUID_H__
#define __USER_AUTHENTICATION_GUID_H__

#define PASSWORD_MIN_SIZE   15  // MIN number of chars of password, including NULL.
#define PASSWORD_MAX_SIZE   25  // MAX number of chars of password, including NULL.
#define PASSWORD_SALT_SIZE  32
#define PASSWORD_HASH_SIZE  32

#define PASSWORD_COMM_BUFFER_SIZE  1024

#define USER_AUTHENTICATION_GUID \
  { 0xf06e3ea7, 0x611c, 0x4b6b, { 0xb4, 0x10, 0xc2, 0xbf, 0x94, 0x3f, 0x38, 0xf2 } }

extern EFI_GUID  gUserAuthenticationGuid;

typedef struct {
  UINTN         Function;
  EFI_STATUS    ReturnStatus;
} MM_PASSWORD_COMMUNICATE_HEADER;

#define MM_PASSWORD_FUNCTION_IS_PASSWORD_SET        1
#define MM_PASSWORD_FUNCTION_SET_PASSWORD           2
#define MM_PASSWORD_FUNCTION_VERIFY_PASSWORD        3
#define MM_PASSWORD_FUNCTION_SET_VERIFY_POLICY      4
#define MM_PASSWORD_FUNCTION_GET_VERIFY_POLICY      5
#define MM_PASSWORD_FUNCTION_WAS_PASSWORD_VERIFIED  6
#define MM_PASSWORD_FUNCTION_GET_PASSWORD_HASH      7
#define MM_PASSWORD_FUNCTION_SET_PASSWORD_HASH      8

typedef struct {
  CHAR8    NewPassword[PASSWORD_MAX_SIZE];
  CHAR8    OldPassword[PASSWORD_MAX_SIZE];
} MM_PASSWORD_COMMUNICATE_SET_PASSWORD;

typedef struct {
  CHAR8    Password[PASSWORD_MAX_SIZE];
} MM_PASSWORD_COMMUNICATE_VERIFY_PASSWORD;

typedef struct {
  BOOLEAN    NeedReVerify;
} MM_PASSWORD_COMMUNICATE_VERIFY_POLICY;

typedef struct {
  BOOLEAN    ClearPassword;                     // Clear password when it is TRUE. This is not used in MM_PASSWORD_FUNCTION_GET_PASSWORD_HASH
  UINT8      PasswordSalt[PASSWORD_SALT_SIZE];  // Password salt
  UINT8      PasswordHash[PASSWORD_HASH_SIZE];  // Password hash
} MM_PASSWORD_COMMUNICATE_PASSWORD_HASH;

#endif
