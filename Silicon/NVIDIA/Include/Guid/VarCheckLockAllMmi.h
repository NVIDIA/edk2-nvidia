/** @file
  GUID is for VarCheck Lock All MM communication.

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __VAR_CHECK_LOCK_ALL_MMI_H__
#define __VAR_CHECK_LOCK_ALL_MMI_H__

#define VAR_CHECK_LOCK_ALL_GUID  \
    { 0x38ad93d2, 0x6ecb, 0x11ee, { 0xb6, 0x32, 0xaf, 0xcf, 0x23, 0x38, 0xc9, 0x90 } }

extern EFI_GUID  gVarCheckLockAllGuid;

#define MM_VAR_CHECK_LOCK_ALL_ACTIVATE       1
#define MM_VAR_CHECK_LOCK_ALL_ADD_EXCEPTION  2

typedef struct {
  UINTN         Function;
  EFI_STATUS    ReturnStatus;
} MM_VAR_CHECK_LOCK_ALL_COMM_HEADER;

typedef struct {
  EFI_GUID    VendorGuid;
  CHAR16      VariableName[1];
} MM_VAR_CHECK_LOCK_ALL_COMM_EXCEPTION;

#endif
