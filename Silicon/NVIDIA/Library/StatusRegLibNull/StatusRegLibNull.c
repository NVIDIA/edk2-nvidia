/** @file

  Null Status register library

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/StatusRegLib.h>

VOID
EFIAPI
StatusRegSetPhase (
  UINT32  Phase,
  UINT32  Bits
  )
{
  return;
}

VOID
EFIAPI
StatusRegSetBits (
  UINT32  Bits
  )
{
  return;
}

VOID
EFIAPI
StatusRegClearBits (
  UINT32  Bits
  )
{
  return;
}

UINT32
EFIAPI
StatusRegGet (
  VOID
  )
{
  return 0;
}

VOID
EFIAPI
StatusRegReset (
  VOID
  )
{
  return;
}
