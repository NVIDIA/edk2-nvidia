/** @file

  Mock SVC functions.

  SPDX-FileCopyrightText: Copyright (c) 2024-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NV_ARMSVC_STUB_LIB_H__
#define __NV_ARMSVC_STUB_LIB_H__

/*
 * ArmFfaSvc.h insists MDE_CPU_ARM or MDE_CPU_AARCH64 is defined, but neither
 * will be during host-based unittests.  To work-around this, we'll avoid
 * including it during tests and define what's needed.
 */
#define ARM_FFA_SVC_H_
#define ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH64   0xC400006F
#define ARM_FID_FFA_MSG_SEND_DIRECT_REQ           ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH64
#define ARM_FID_FFA_MSG_SEND_DIRECT_RESP_AARCH64  0xC4000070
#define ARM_FID_FFA_MSG_SEND_DIRECT_RESP          ARM_FID_FFA_MSG_SEND_DIRECT_RESP_AARCH64

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
