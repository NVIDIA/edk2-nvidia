/** @file

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ArmFfaLib.h>
#include <Library/ArmSmcLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Protocol/MmCommunication2.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/PcdLib.h>

/**
  Convert EFI_GUID to UUID format.

  @param[in] Guid            EFI_GUID
  @param[out] Uuid           Uuid

  @retval EFI_SUCCESS   Successfully converted EFI_GUID to UUID format
  @retval Other           Error.
**/
STATIC
VOID
EFIAPI
ConvertEfiGuidToUuid (
  IN   EFI_GUID  *Guid,
  OUT  UINT64    *Uuid
  )
{
  UINT32  *Data32;
  UINT16  *Data16;

  CopyGuid ((EFI_GUID *)Uuid, Guid);
  Data32    = (UINT32 *)Uuid;
  Data32[0] = SwapBytes32 (Data32[0]);
  Data16    = (UINT16 *)&Data32[1];
  Data16[0] = SwapBytes16 (Data16[0]);
  Data16[1] = SwapBytes16 (Data16[1]);
}

/**
  Get the Standalone MM Partition ID.

  Ideally the call to the ARM_FID_FFA_PARTITION_INFO_GET_REGS should be in the ArmFfaLib,
  but we're using only a small portion of this message type.
  This will need more work to be added to the ArmFfaLib.

  @param[out] StmmPartId      Standalone MM Partition ID

  @retval EFI_SUCCESS   Successfully got the Standalone MM Partition ID
  @retval Other           Error.
**/
STATIC
EFI_STATUS
GetStmmPartId (
  UINT16  *StmmPartId
  )
{
  EFI_STATUS    Status;
  ARM_FFA_ARGS  FfaArgs;
  UINT64        Uuid[2];

  if (StmmPartId == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&FfaArgs, sizeof (ARM_FFA_ARGS));
  ConvertEfiGuidToUuid (&gNVIDIAStandaloneMmBuffersGuid, Uuid);

  FfaArgs.Arg0 = ARM_FID_FFA_PARTITION_INFO_GET_REGS;
  FfaArgs.Arg1 = Uuid[0];
  FfaArgs.Arg2 = Uuid[1];

  ArmCallSmc ((ARM_SMC_ARGS *)&FfaArgs);

  Status = IS_FID_FFA_ERROR (FfaArgs.Arg0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Standalone MM Partition ID %x\n", __FUNCTION__, FfaArgs.Arg3));

  *StmmPartId = (UINT16)(FfaArgs.Arg3 & 0xFFFF);
  return EFI_SUCCESS;
}

/**
  Get the address and size of a shared buffer from StandaloneMm.

  @param[in] Fid            FID of the buffer to get
  @param[out] BufferBase    Base address of the buffer
  @param[out] BufferSize    Size of the buffer

  @retval EFI_SUCCESS       Got the NS Comm Buffer for the desired secure
                            service.
          Other             Failed to get the buffer/size.
**/
EFI_STATUS
EFIAPI
StMmDxeGetBuffer (
  IN UINT32   Fid,
  OUT UINT64  *BufferBase,
  OUT UINT32  *BufferSize
  )
{
  EFI_STATUS       Status;
  UINT16           RcvPartId;
  DIRECT_MSG_ARGS  DirectMsgArgs;

  if ((BufferBase == NULL) || (BufferSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetStmmPartId (&RcvPartId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get Standalone MM Partition ID %r\n", __FUNCTION__, Status));
    goto ExitStandaloneMmGetBuffer;
  }

  ZeroMem (&DirectMsgArgs, sizeof (DIRECT_MSG_ARGS));
  DirectMsgArgs.Arg0 = Fid;

  Status = ArmFfaLibMsgSendDirectReq2 (RcvPartId, &gNVIDIAStandaloneMmBuffersGuid, &DirectMsgArgs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send FFA Direct Msg Request 2 %r\n", __FUNCTION__, Status));
    goto ExitStandaloneMmGetBuffer;
  }

  if (DirectMsgArgs.Arg0 != EFI_SUCCESS) {
    DEBUG ((DEBUG_INFO, "%a: Failed to get NS Buffer Details. %r\n", __FUNCTION__, DirectMsgArgs.Arg0));
    Status = DirectMsgArgs.Arg0;
  } else {
    DEBUG ((
      DEBUG_INFO,
      "%a: Successfully got NS Buffer Details %llx %lx\n",
      __FUNCTION__,
      DirectMsgArgs.Arg1,
      DirectMsgArgs.Arg2
      ));
    if (DirectMsgArgs.Arg1 == 0) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get NS Buffer Base. %r\n", __FUNCTION__, EFI_INVALID_PARAMETER));
      Status = EFI_DEVICE_ERROR;
      goto ExitStandaloneMmGetBuffer;
    }

    if ((DirectMsgArgs.Arg2 == 0) || (DirectMsgArgs.Arg2 < sizeof (EFI_MM_COMMUNICATE_HEADER))) {
      DEBUG ((DEBUG_ERROR, "%a: Invalid NS Buffer Size. %r\n", __FUNCTION__, EFI_DEVICE_ERROR));
      Status = EFI_DEVICE_ERROR;
      goto ExitStandaloneMmGetBuffer;
    }

    *BufferBase = DirectMsgArgs.Arg1;
    *BufferSize = DirectMsgArgs.Arg2;
    Status      = EFI_SUCCESS;
  }

ExitStandaloneMmGetBuffer:
  return Status;
}
