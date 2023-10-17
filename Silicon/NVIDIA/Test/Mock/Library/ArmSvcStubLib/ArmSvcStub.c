/** @file
  This file provides a mock for the SVC Library.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/ArmSvcLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#define MDE_CPU_AARCH64

VOID
ArmCallSvc (
  IN OUT ARM_SVC_ARGS  *Args
  )
{
  ZeroMem (Args, sizeof (ARM_SVC_ARGS));
}
