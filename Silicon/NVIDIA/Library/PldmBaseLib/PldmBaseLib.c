/** @file

  PLDM base protocol and helper functions

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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

EFI_STATUS
EFIAPI
PldmValidateResponse (
  IN CONST VOID    *ReqBuffer,
  IN CONST VOID    *RspBuffer,
  IN UINTN         RspLength,
  IN UINT8         ReqMsgTag,
  IN UINT8         RspMsgTag,
  IN CONST CHAR16  *DeviceName
  )
{
  CONST MCTP_PLDM_COMMON  *Req;
  CONST MCTP_PLDM_COMMON  *Rsp;
  EFI_STATUS              Status;

  Req = (MCTP_PLDM_COMMON *)ReqBuffer;
  Rsp = (MCTP_PLDM_COMMON *)RspBuffer;

  Status = MctpValidateResponse (Req, Rsp, ReqMsgTag, RspMsgTag, DeviceName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (RspLength < sizeof (MCTP_PLDM_RESPONSE_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: %s Cmd=0x%x bad rsplen=%u\n", __FUNCTION__, DeviceName, Req->Command, RspLength));
    return EFI_PROTOCOL_ERROR;
  }

  if (Req->Command != Rsp->Command) {
    DEBUG ((DEBUG_ERROR, "%a: %s cmd mismatch req/rsp=%u/%u\n", __FUNCTION__, DeviceName, Req->Command, Rsp->Command));
    return EFI_PROTOCOL_ERROR;
  }

  return EFI_SUCCESS;
}
