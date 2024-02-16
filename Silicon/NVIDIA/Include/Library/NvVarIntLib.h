/** @file

  NvVarInt Library

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NV_VARINT_LIB__
#define __NV_VARINT_LIB__

#include <Guid/GlobalVariable.h>

EFIAPI
EFI_STATUS
ComputeVarMeasurement (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  );

EFIAPI
EFI_STATUS
MeasureBootVars (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL
  );

EFIAPI
EFI_STATUS
MeasureSecureDbVars (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL
  );

#endif // __NV_VARINT_LIB__
