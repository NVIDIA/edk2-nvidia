/** @file
  Header file for IPMI password OEM command.

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __IPMI_PASSWORD_H__
#define __IPMI_PASSWORD_H__

#include <Uefi.h>
#include <IndustryStandard/Ipmi.h>
#include <Library/IpmiBaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#define IPMI_BIOS_PASSWORD_DEBUG  DEBUG_INFO

//
// Net function definition for OEM command
//
#define IPMI_NETFN_OEM  0x3C

//
// OEM commands for BIOS password
//
#define IPMI_OEM_SET_BIOS_PASSWORD  0x36
#define IPMI_OEM_GET_BIOS_PASSWORD  0x37

//
// Password ID selector
//
#define BIOS_PASSWORD_SELECTOR_ADMIN  0x01

//
// Password type
//
#define BIOS_PASSWORD_TYPE_NO_PASSWD      0x01
#define BIOS_PASSWORD_TYPE_PBKDF2_SHA256  0x02
#define BIOS_PASSWORD_TYPE_PBKDF2_SHA384  0x03

//
// Password action
//
#define BIOS_PASSWORD_ACTION_NO_CHANGE              0x00
#define BIOS_PASSWORD_ACTION_CLEAR_PASSWD           0x01
#define BIOS_PASSWORD_ACTION_CHANGED_PBKDF2_SHA256  0x02
#define BIOS_PASSWORD_ACTION_CHANGED_PBKDF2_SHA384  0x03

#define BIOS_PASSWORD_SALT_SIZE         32
#define BIOS_PASSWORD_HASH_SIZE         64
#define BIOS_PASSWORD_HASH_SHA256_SIZE  32
#define BIOS_PASSWORD_HASH_SHA384_SIZE  48

#pragma pack(1)

typedef struct {
  UINT8    PasswordIdSelector;
  UINT8    PasswordType;
  UINT8    PasswordSalt[BIOS_PASSWORD_SALT_SIZE];
  UINT8    PasswordHash[BIOS_PASSWORD_HASH_SIZE];
} IPMI_OEM_SET_BIOS_PASSWORD_REQUEST_DATA;

typedef struct {
  UINT8    CompletionCode;
} IPMI_OEM_SET_BIOS_PASSWORD_RESPONSE_DATA;

typedef struct {
  UINT8    PasswordIdSelector;
} IPMI_OEM_GET_BIOS_PASSWORD_REQUEST_DATA;

typedef struct {
  UINT8    CompletionCode;
  UINT8    PasswordAction;
  UINT8    PasswordSalt[BIOS_PASSWORD_SALT_SIZE];
  UINT8    PasswordHash[BIOS_PASSWORD_HASH_SIZE];
} IPMI_OEM_GET_BIOS_PASSWORD_RESPONSE_DATA;

#pragma pack()

/**
  Sync BIOS password between BIOS and BMC.

  @retval EFI_SUCCESS      Set BIOS password successfully.

**/
EFI_STATUS
BiosPasswordSynchronization (
  VOID
  );

#endif
