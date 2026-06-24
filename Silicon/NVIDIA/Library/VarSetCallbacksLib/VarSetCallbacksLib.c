/** @file
  Implementation functions and structures for var check services.

SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/VarSetCallbacksLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/DebugLib.h>

STATIC NVIDIA_VAR_INT_PROTOCOL  *VarIntProto = NULL;

EFI_STATUS
EFIAPI
VarPreSetCallback (
  IN CHAR16                    *VariableName,
  IN EFI_GUID                  *VendorGuid,
  IN UINT32                    Attributes,
  IN UINTN                     DataSize,
  IN VOID                      *Data,
  IN VAR_CHECK_REQUEST_SOURCE  RequestSource
  )
{
  EFI_STATUS  Status;

  if (VarIntProto == NULL) {
    Status = gMmst->MmLocateProtocol (
                      &gNVIDIAVarIntGuid,
                      NULL,
                      (VOID **)&VarIntProto
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: Failed to get VarInt Proto%r\n", __FUNCTION__, Status));
      return EFI_SUCCESS;
    }
  }

  if ((VarIntProto->IsDeferred != NULL) &&
      (VarIntProto->IsDeferred (VarIntProto) == TRUE))
  {
    return EFI_SUCCESS;
  }

  /* Write the pending measurement before the variable flash update. */
  Status = VarIntProto->ComputeNewMeasurement (
                          VarIntProto,
                          VariableName,
                          VendorGuid,
                          Attributes,
                          Data,
                          DataSize
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to compute pending measurement %r\n", __FUNCTION__, Status));
    VarIntProto->MeasurementDirty = FALSE;
    return Status;
  }

  if (VarIntProto->MeasurementDirty == TRUE) {
    Status = VarIntProto->WriteNewMeasurement (VarIntProto);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to write pending measurement %r\n", __FUNCTION__, Status));
      VarIntProto->MeasurementDirty = FALSE;
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VarPostSetCallback (
  IN CHAR16                    *VariableName,
  IN EFI_GUID                  *VendorGuid,
  IN UINT32                    Attributes,
  IN UINTN                     DataSize,
  IN VOID                      *Data,
  IN VAR_CHECK_REQUEST_SOURCE  RequestSource,
  IN EFI_STATUS                SetVarStatus
  )
{
  EFI_STATUS  CleanupStatus;
  EFI_STATUS  Status;

  if (VarIntProto == NULL) {
    Status = gMmst->MmLocateProtocol (
                      &gNVIDIAVarIntGuid,
                      NULL,
                      (VOID **)&VarIntProto
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: Failed to get VarInt Proto%r\n", __FUNCTION__, Status));
      return EFI_SUCCESS;
    }
  }

  if ((VarIntProto->IsDeferred != NULL) &&
      (VarIntProto->IsDeferred (VarIntProto) == TRUE))
  {
    if (VarIntProto->MarkDirty != NULL) {
      Status = VarIntProto->MarkDirty (
                              VarIntProto,
                              VariableName,
                              VendorGuid,
                              SetVarStatus
                              );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to mark deferred measurement dirty %r\n", __FUNCTION__, Status));
      }
    }

    return EFI_SUCCESS;
  }

  CleanupStatus = SetVarStatus;

  if (VarIntProto->MeasurementDirty == TRUE) {
    if (!EFI_ERROR (SetVarStatus)) {
      Status = VarIntProto->ComputeNewMeasurement (
                              VarIntProto,
                              NULL,
                              NULL,
                              0,
                              NULL,
                              0
                              );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to compute committed measurement %r\n", __FUNCTION__, Status));
        CleanupStatus = EFI_ABORTED;
        goto CleanupPendingMeasurement;
      }

      Status = VarIntProto->WriteNewMeasurement (VarIntProto);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to prepare committed measurement %r\n", __FUNCTION__, Status));
        CleanupStatus = EFI_ABORTED;
        goto CleanupPendingMeasurement;
      }
    }
  }

CleanupPendingMeasurement:
  Status = VarIntProto->InvalidateLast (
                          VarIntProto,
                          VariableName,
                          VendorGuid,
                          CleanupStatus
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to Invalidate Record %r\n", __FUNCTION__, Status));
  }

  VarIntProto->MeasurementDirty = FALSE;

  return Status;
}
