/** @file
  The header file of Platform Redfish boot order driver.

  (C) Copyright 2022 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_REDFISH_BOOT_DATA_H_
#define PLATFORM_REDFISH_BOOT_DATA_H_

#include <Uefi/UefiMultiPhase.h>
#include <Guid/HiiPlatformSetupFormset.h>

#define PLATFORM_REDFISH_BOOT_FORMSET_GUID \
{ \
  0x35aff689, 0x1c07, 0x4cac, { 0x90, 0xd5, 0xaa, 0x57, 0x20, 0xcb, 0x46, 0x6b } \
}

extern EFI_GUID  gPlatformRedfishBootFormsetGuid;

#define FORM_ID                   0x001
#define LABEL_BOOT_OPTION         0x200
#define LABEL_BOOT_OPTION_END     0x201
#define BOOT_ORDER_LIST           0x300
#define MAX_BOOT_OPTIONS          100
#define BOOT_OPTION_VAR_STORE_ID  0x800
//
// VarOffset that will be used to create question
// all these values are computed from the structure
// defined below
//
#define VAR_OFFSET(Field)  ((UINT16) ((UINTN) &(((PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA *) 0)->Field)))

#pragma pack(1)

//
// Definiton of PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA
//
typedef struct {
  UINT32    BootOptionOrder[MAX_BOOT_OPTIONS];
} PLATFORM_REDFISH_BOOT_OPTION_VARSTORE_DATA;

#pragma pack()

#endif
