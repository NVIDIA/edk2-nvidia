/** @file

  Mock Library for Computing Measurements of some variables.(NvVarIntLib)

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NV_VARINT_STUB_LIB_H__
#define __NV_VARINT_STUB_LIB_H__

#include <Uefi.h>
#include <Library/NvVarIntLib.h>

/**
  Set up mock parameters for ComputeVarMeasurement() stub

  @param[In]  *VarName          Pointer to Variable Name.
  @param[In]  *MockMeas         Pointer to Variable Measurement.
  @param[In]  ReturnStatus      Status to return.

  @retval None

**/
VOID
MockComputeVarMeasurement (
  IN  CHAR16      *VarName,
  OUT UINT8       *MockMeas,
  IN  UINTN       MeasSize,
  IN  EFI_STATUS  ReturnStatus
  );

#endif
