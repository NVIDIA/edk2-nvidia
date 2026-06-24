/** @file

  NvVarInt Library

  V0 computes the original boot/security variable measurement. V1 walks the
  current NV variable store and excludes variables listed by policy.

  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NV_VARINT_LIB__
#define __NV_VARINT_LIB__

#include <Guid/GlobalVariable.h>

EFI_STATUS
EFIAPI
ComputeVarMeasurement (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  );

EFI_STATUS
EFIAPI
ComputeVarMeasurementV0 (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  );

EFI_STATUS
EFIAPI
ComputeVarMeasurementV1 (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  );

BOOLEAN
EFIAPI
NvVarIntIsExcludedVar (
  IN CHAR16    *VarName OPTIONAL,
  IN EFI_GUID  *VarGuid OPTIONAL
  );

BOOLEAN
EFIAPI
NvVarIntCanUpdateMeasurement (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid,
  IN UINT32    Attributes,
  IN UINTN     DataSize
  );

BOOLEAN
EFIAPI
NvVarIntIsNoOpUpdate (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid,
  IN UINT32    Attributes,
  IN VOID      *Data,
  IN UINTN     DataSize
  );

VOID
EFIAPI
NvVarIntNotifyExitBootServices (
  VOID
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
