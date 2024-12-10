/** @file

  Mock SVC functions.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NV_ARMSVC_STUB_LIB_H__
#define __NV_ARMSVC_STUB_LIB_H__

#include <Uefi.h>
#include <Library/ArmSvcLib.h>

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
  );

#endif
