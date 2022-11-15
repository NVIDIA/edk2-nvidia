/** @file

  MCTP base protocol and helper functions

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/MctpBaseLib.h>

/**
  Return 32-bit value from big endian buffer.

  @param[in]  Buffer        Pointer to buffer containing 32-bit big endian value.

  @retval UINT32            Value extracted from buffer.

**/
UINT32
EFIAPI
MctpBEBufferToUint32 (
  IN CONST UINT8  *Buffer
  )
{
  return (Buffer[0] << 24) |
         (Buffer[1] << 16) |
         (Buffer[2] << 8) |
         (Buffer[3] << 0);
}

/**
  Return 16-bit value from big endian buffer.

  @param[in]  Buffer        Pointer to buffer containing 16-bit big endian value.

  @retval UINT32            Value extracted from buffer.

**/
UINT16
EFIAPI
MctpBEBufferToUint16 (
  IN CONST UINT8  *Buffer
  )
{
  return (Buffer[0] << 8) | (Buffer[1] << 0);
}

/**
  Put 32-bit value into big endian buffer.

  @param[in]  Buffer        Pointer to big endian buffer.
  @param[in]  Value         Value to put into buffer.

  @retval None

**/
VOID
EFIAPI
MctpUint32ToBEBuffer (
  OUT UINT8   *Buffer,
  IN  UINT32  Value
  )
{
  Buffer[0] = (UINT8)(Value >> 24);
  Buffer[1] = (UINT8)(Value >> 16);
  Buffer[2] = (UINT8)(Value >> 8);
  Buffer[3] = (UINT8)(Value);
}

/**
  Put 16-bit value into big endian buffer.

  @param[in]  Buffer        Pointer to big endian buffer.
  @param[in]  Value         Value to put into buffer.

  @retval None

**/
VOID
EFIAPI
MctpUint16ToBEBuffer (
  UINT8   *Buffer,
  UINT16  Value
  )
{
  Buffer[0] = (UINT8)(Value >> 8);
  Buffer[1] = (UINT8)(Value);
}

VOID
EFIAPI
MctpControlReqFillCommon (
  IN  MCTP_CONTROL_COMMON  *Common,
  IN  UINT8                Command
  )
{
  Common->Type       = MCTP_TYPE_CONTROL;
  Common->InstanceId = MCTP_RQ;
  Common->Command    = Command;
}
