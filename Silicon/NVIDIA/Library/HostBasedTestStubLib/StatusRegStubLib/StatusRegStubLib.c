/** @file

  Status register library

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/StatusRegLib.h>
#include <Library/TegraPlatformInfoLib.h>

#define TH500_SCRATCH_STATUS_REGISTER  0x0c39040c

STATIC UINTN    mStatusRegAddr        = 0;
STATIC BOOLEAN  mStatusRegInitialized = FALSE;
STATIC UINT32   mStatusReg            = 0;

STATIC
VOID
EFIAPI
StatusRegInitialize (
  VOID
  )
{
  UINTN  ChipID;

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case TH500_CHIP_ID:
      mStatusRegAddr = TH500_SCRATCH_STATUS_REGISTER;
      break;
    default:
      break;
  }

  mStatusRegInitialized = TRUE;
}

VOID
EFIAPI
StatusRegSetPhase (
  UINT32  Phase,
  UINT32  Bits
  )
{
  UINT32  Status;
  UINT32  OldStatus;

  if (!mStatusRegInitialized) {
    StatusRegInitialize ();
  }

  if (mStatusRegAddr != 0) {
    OldStatus  = mStatusReg;
    Status     = OldStatus & ~(STATUS_REG_PHASE_MASK | STATUS_REG_STATUS_MASK);
    Status    |= Phase | Bits | STATUS_REG_VERSION_CURRENT;
    mStatusReg =  Status;

    DEBUG ((DEBUG_INFO, "%a: Updated status from 0x%x to 0x%x\n", __FUNCTION__, OldStatus, Status));
  }
}

VOID
EFIAPI
StatusRegSetBits (
  UINT32  Bits
  )
{
  UINT32  Status;
  UINT32  OldStatus;

  if (!mStatusRegInitialized) {
    StatusRegInitialize ();
  }

  if (mStatusRegAddr != 0) {
    OldStatus  = mStatusReg;
    Status     = OldStatus | Bits;
    mStatusReg =  Status;

    DEBUG ((DEBUG_INFO, "%a: Updated status from 0x%x to 0x%x\n", __FUNCTION__, OldStatus, Status));
  }
}

VOID
EFIAPI
StatusRegClearBits (
  UINT32  Bits
  )
{
  UINT32  Status;
  UINT32  OldStatus;

  if (!mStatusRegInitialized) {
    StatusRegInitialize ();
  }

  if (mStatusRegAddr != 0) {
    OldStatus  = mStatusReg;
    Status     = OldStatus & ~Bits;
    mStatusReg =  Status;

    DEBUG ((DEBUG_INFO, "%a: Updated status from 0x%x to 0x%x\n", __FUNCTION__, OldStatus, Status));
  }
}

UINT32
EFIAPI
StatusRegGet (
  VOID
  )
{
  UINT32  Status = 0;

  if (!mStatusRegInitialized) {
    StatusRegInitialize ();
  }

  if (mStatusRegAddr != 0) {
    Status = mStatusReg;
  }

  return Status;
}

VOID
EFIAPI
StatusRegReset (
  VOID
  )
{
  UINT32  OldStatus;

  if (!mStatusRegInitialized) {
    StatusRegInitialize ();
  }

  if (mStatusRegAddr != 0) {
    OldStatus  = mStatusReg;
    mStatusReg =  0;

    DEBUG ((DEBUG_INFO, "%a: Reset status 0x%x to 0\n", __FUNCTION__, OldStatus));
  }
}
