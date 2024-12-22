/** @file

  DTB update library private header

  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#define DTB_UPDATE_UEFI_DTB    0x1
#define DTB_UPDATE_KERNEL_DTB  0x2
#define DTB_UPDATE_ALL         (DTB_UPDATE_UEFI_DTB | DTB_UPDATE_KERNEL_DTB)

#define DTB_UPDATE_REGISTER_FUNCTION(Function, Flags) \
  DtbUpdateRegisterFunction (Function, # Function, Flags)

typedef
VOID
(EFIAPI *DTB_UPDATE_FUNCTION)(
  VOID
  );

VOID
EFIAPI
DtbUpdateRegisterFunction (
  DTB_UPDATE_FUNCTION  Function,
  CONST CHAR8          *Name,
  UINT8                Flags
  );
