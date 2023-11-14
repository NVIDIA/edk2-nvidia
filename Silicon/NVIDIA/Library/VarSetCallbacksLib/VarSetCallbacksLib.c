/** @file
  Implementation functions and structures for var check services.

SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/VarSetCallbacksLib.h>
#include <Library/BaseLib.h>
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

  /* Recompute/extend hash */
  Status = VarIntProto->ComputeNewMeasurement (
                          VarIntProto,
                          VariableName,
                          VendorGuid,
                          Attributes,
                          Data,
                          DataSize
                          );

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

  /* Recompute/extend hash */
  Status = VarIntProto->InvalidateLast (
                          VarIntProto,
                          VariableName,
                          VendorGuid,
                          SetVarStatus
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Invalidate Record %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}
