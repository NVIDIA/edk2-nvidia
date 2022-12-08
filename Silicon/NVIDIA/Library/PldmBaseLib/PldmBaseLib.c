/** @file

  PLDM base protocol and helper functions

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/MctpBaseLib.h>
#include <Library/PldmBaseLib.h>

VOID
EFIAPI
PldmFillCommon (
  IN  MCTP_PLDM_COMMON  *Common,
  IN  BOOLEAN           IsRequest,
  IN  UINT8             InstanceId,
  IN  UINT8             PldmType,
  IN  UINT8             Command
  )
{
  UINT8  Control;

  Control = (IsRequest) ? PLDM_RQ : 0;

  Common->MctpType   = MCTP_TYPE_PLDM;
  Common->InstanceId = (InstanceId & PLDM_INSTANCE_ID_MASK) | Control;
  Common->PldmType   = (PldmType & PLDM_TYPE_MASK) | PLDM_HDR_VER;
  Common->Command    = Command;
}
