/** @file

  Mock Library for Computing Measurements of some variables.(NvVarIntLib)

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "Base.h"
#include <Library/MmServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <HostBasedTestStubLib/NvVarIntStubLib.h>

EFIAPI
EFI_STATUS
ComputeVarMeasurement (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  )
{
  EFI_STATUS  Status;
  UINT8       *MockMeas;
  UINTN       MeasSz;

  if (VarName != NULL) {
    check_expected_ptr (VarName);
  }

  MockMeas = (UINT8 *)mock ();
  MeasSz   = (UINTN)mock ();
  Status   = (EFI_STATUS)mock ();
  CopyMem (Meas, MockMeas, MeasSz);
  return Status;
}

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
  )
{
  if (VarName != NULL) {
    expect_memory (ComputeVarMeasurement, VarName, VarName, sizeof (VarName));
  }

  will_return (ComputeVarMeasurement, MockMeas);
  will_return (ComputeVarMeasurement, MeasSize);
  will_return (ComputeVarMeasurement, ReturnStatus);
}
