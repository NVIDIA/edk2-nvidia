/** @file
  Mock SVC functions.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Library/ArmSvcLib.h>
#include <Library/BaseMemoryLib.h>

VOID
ArmCallSvc (
  IN OUT ARM_SVC_ARGS  *Args
  )
{
  ARM_SVC_ARGS  *MockArgs;

  MockArgs = (ARM_SVC_ARGS *)mock ();
  CopyMem (Args, MockArgs, sizeof (ARM_SVC_ARGS));
}

/*
 * MockArmCallSvc
 *
 * Set up mock parameters for ComputeVarMeasurement() stub
 *
 * @param[In]  *Args          Pointer to ARM SVC Args.
 *
 * @retval None
 */
VOID
MockArmCallSvc (
  IN OUT ARM_SVC_ARGS  *Args
  )
{
  will_return (ArmCallSvc, Args);
}
