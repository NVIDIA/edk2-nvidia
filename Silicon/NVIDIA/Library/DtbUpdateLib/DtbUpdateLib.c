/** @file

  DTB update library

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <libfdt.h>

#include "DtbUpdateLibPrivate.h"

typedef struct {
  DTB_UPDATE_FUNCTION    Function;
  CONST CHAR8            *Name;
  UINT8                  Flags;
} DTB_UPDATE_TABLE_ENTRY;

DTB_UPDATE_TABLE_ENTRY  mDtbUpdateTable[16]    = { 0 };
UINTN                   mDtbUpdateTableEntries = 0;

/**
  register sub-module update function in table

**/
VOID
EFIAPI
DtbUpdateRegisterFunction (
  DTB_UPDATE_FUNCTION  Function,
  CONST CHAR8          *Name,
  UINT8                Flags
  )
{
  DTB_UPDATE_TABLE_ENTRY  *Entry;

  DEBUG ((DEBUG_INFO, "%a: table[%u]=0x%p (%a) flags=0x%x\n", __FUNCTION__, mDtbUpdateTableEntries, Function, Name, Flags));

  if (mDtbUpdateTableEntries < ARRAY_SIZE (mDtbUpdateTable)) {
    Entry           = &mDtbUpdateTable[mDtbUpdateTableEntries];
    Entry->Function = Function;
    Entry->Name     = Name;
    Entry->Flags    = Flags;
    mDtbUpdateTableEntries++;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: can't register %a, entries=%u\n", __FUNCTION__, Name, mDtbUpdateTableEntries));
  }
}

/**
  execute all registered update functions with matching flags

**/
STATIC
VOID
EFIAPI
DtbUpdateExecuteAll (
  VOID   *Dtb,
  UINT8  Flags
  )
{
  CONST DTB_UPDATE_TABLE_ENTRY  *Entry;
  UINTN                         Index;
  BOOLEAN                       Executed;

  DEBUG ((DEBUG_INFO, "%a: flags=0x%x table entries=%u\n", __FUNCTION__, Flags, mDtbUpdateTableEntries));

  SetDeviceTreePointer (Dtb, fdt_totalsize (Dtb));

  Entry = mDtbUpdateTable;
  for (Index = 0; Index < mDtbUpdateTableEntries; Index++, Entry++) {
    if ((Entry->Flags & Flags) != 0) {
      (Entry->Function)();
      Executed = TRUE;
    } else {
      Executed = FALSE;
    }

    DEBUG ((DEBUG_INFO, "%a: %a %a\n", __FUNCTION__, (Executed) ? "executed" : "skipped", Entry->Name));
  }
}

/**
  update UEFI DTB

**/
VOID
EFIAPI
DtbUpdateForUefi (
  VOID  *Dtb
  )
{
  DtbUpdateExecuteAll (Dtb, DTB_UPDATE_UEFI_DTB);
}

/**
  update kernel DTB

**/
VOID
EFIAPI
DtbUpdateForKernel (
  VOID  *Dtb
  )
{
  DtbUpdateExecuteAll (Dtb, DTB_UPDATE_KERNEL_DTB);
}
