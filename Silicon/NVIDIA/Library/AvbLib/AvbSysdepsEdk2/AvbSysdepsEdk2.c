/** @file
  EDK2 API for AvbLib

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/NVIDIADebugLib.h>

#include "avb_sysdeps.h"

int
avb_memcmp (
  const void  *src1,
  const void  *src2,
  size_t      n
  )
{
  return (int)CompareMem ((VOID *)src1, (VOID *)src2, (UINTN)n);
}

void *
avb_memcpy (
  void        *dest,
  const void  *src,
  size_t      n
  )
{
  CopyMem ((VOID *)dest, (VOID *)src, (UINTN)n);
  return dest;
}

void *
avb_memset (
  void       *dest,
  const int  c,
  size_t     n
  )
{
  SetMem ((VOID *)dest, (UINTN)n, (UINT8)c);
  return dest;
}

int
avb_strcmp (
  const char  *s1,
  const char  *s2
  )
{
  return (int)AsciiStrCmp ((CHAR8 *)s1, (CHAR8 *)s2);
}

int
avb_strncmp (
  const char  *s1,
  const char  *s2,
  size_t      n
  )
{
  return (int)AsciiStrnCmp ((CHAR8 *)s1, (CHAR8 *)s2, (UINTN)n);
}

size_t
avb_strlen (
  const char  *str
  )
{
  return (size_t)AsciiStrLen ((CHAR8 *)str);
}

void
avb_abort (
  void
  )
{
  DEBUG ((DEBUG_ERROR, "AVB aborting\n"));
  ASSERT (false);
  __builtin_unreachable ();
}

void
avb_printf (
  const char  *fmt,
  ...
  )
{
  VA_LIST  ap;

  VA_START (ap, fmt);
  DebugVPrint (DEBUG_ERROR, fmt, ap);
  VA_END (ap);
}

void
avb_print (
  const char  *message
  )
{
  DEBUG ((DEBUG_ERROR, "%a", message));
}

void
avb_printv (
  const char  *message,
  ...
  )
{
  VA_LIST     ap;
  const char  *m;

  VA_START (ap, message);
  for (m = message; m != NULL; m = VA_ARG (ap, const char *)) {
    avb_print (m);
  }

  VA_END (ap);
}

void *
avb_malloc_ (
  size_t  size
  )
{
  return (void *)AllocatePool ((UINTN)size);
}

void
avb_free (
  void  *ptr
  )
{
  FreePool ((VOID *)ptr);
}

uint32_t
avb_div_by_10 (
  uint64_t  *dividend
  )
{
  uint32_t  rem = (uint32_t)(*dividend % 10);

  *dividend /= 10;
  return rem;
}
