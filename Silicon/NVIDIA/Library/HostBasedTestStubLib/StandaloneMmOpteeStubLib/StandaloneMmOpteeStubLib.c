/** @file

Stub implementation of StandaloneMmOpteeLib.

SPDX-FileCopyrightText: Copyright (c) 2023-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Protocol/NorFlash.h>

#include <Library/DebugLib.h>
#include <HostBasedTestStubLib/StandaloneMmOpteeStubLib.h>

#define MAX_SOCKETS  4

STATIC NVIDIA_NOR_FLASH_PROTOCOL  *SocketNorFlashProtocols[MAX_SOCKETS];

/**
 * Check for OP-TEE presence.
 *
 * @retval TRUE  OP-TEE is present.
 * @retval FALSE OP-TEE is not present.
**/
EFIAPI
BOOLEAN
IsOpteePresent (
  VOID
  )
{
  BOOLEAN  ReturnStatus = mock_type (BOOLEAN);

  return ReturnStatus;
}

/**
 * Setup the check for OP-TEE return
 *
 * @param[in] ReturnStatus Return status for the call to give
 *
**/
VOID
MockIsOpteePresent (
  IN BOOLEAN  ReturnStatus
  )
{
  will_return (IsOpteePresent, ReturnStatus);
}

/**
 * Get the FFA TX/RX buffer addresses and sizes. This API only applies to Hafnium deployments.
 *
 * @param[out] FfaTxBufferAddr  FFA TX buffer address.
 * @param[out] FfaTxBufferSize  FFA TX buffer size.
 * @param[out] FfaRxBufferAddr  FFA RX buffer address.
 * @param[out] FfaRxBufferSize  FFA RX buffer size.
 */
EFI_STATUS
EFIAPI
FfaGetTxRxBuffer (
  UINT64  *FfaTxBufferAddr,
  UINT32  *FfaTxBufferSize,
  UINT64  *FfaRxBufferAddr,
  UINT32  *FfaRxBufferSize
  )
{
  EFI_STATUS  ReturnStatus = mock_type (EFI_STATUS);

  if (!EFI_ERROR (ReturnStatus)) {
    *FfaTxBufferAddr = mock_type (UINT64);
    *FfaTxBufferSize = mock_type (UINT32);
    *FfaRxBufferAddr = mock_type (UINT64);
    *FfaRxBufferSize = mock_type (UINT32);
  }

  return ReturnStatus;
}

/**
 * Setup the FfaGetTxRxBuffer return status.
 *
 * @param[in] ReturnStatus Return status for the call to give
 *
 **/
VOID
MockFfaGetTxRxBuffer (
  IN EFI_STATUS  ReturnStatus,
  IN UINT64      *FfaTxBufferAddr,
  IN UINT32      *FfaTxBufferSize,
  IN UINT64      *FfaRxBufferAddr,
  IN UINT32      *FfaRxBufferSize
  )
{
  will_return (FfaGetTxRxBuffer, ReturnStatus);
  will_return (FfaGetTxRxBuffer, FfaTxBufferAddr);
  will_return (FfaGetTxRxBuffer, FfaTxBufferSize);
  will_return (FfaGetTxRxBuffer, FfaRxBufferAddr);
  will_return (FfaGetTxRxBuffer, FfaRxBufferSize);
}

/*
 * GetOpteeVmId
 * Get the Optee VM ID from the SPMC.
 *
 * @param[out] OpteeVmId  Optee VM ID.
 *
 */
EFI_STATUS
EFIAPI
FfaGetOpteeVmId (
  OUT UINT16  *OpteeVmId
  )
{
  EFI_STATUS  ReturnStatus = mock_type (EFI_STATUS);

  if (!EFI_ERROR (ReturnStatus)) {
    *OpteeVmId = mock_type (UINT16);
  }

  return ReturnStatus;
}

/**
 * Setup the FfaGetOpteeVmId return status.
 *
 * @param[in] ReturnStatus Return status for the call to give
 *
 **/
VOID
MockFfaGetOpteeVmId (
  IN EFI_STATUS  ReturnStatus,
  IN UINT16      *OpteeVmId
  )
{
  will_return (FfaGetOpteeVmId, ReturnStatus);
  will_return (FfaGetOpteeVmId, OpteeVmId);
}

/*
* GetMmVmId
* Get the MM VM ID from the SPMC.
*
* @param[out] MmVmId  MM VM ID.
*
*/
EFI_STATUS
EFIAPI
FfaGetMmVmId (
  OUT UINT16  *MmVmId
  )
{
  EFI_STATUS  ReturnStatus = mock_type (EFI_STATUS);

  if (!EFI_ERROR (ReturnStatus)) {
    *MmVmId = mock_type (UINT16);
  }

  return ReturnStatus;
}

/**
 * Setup the FfaGetMmVmId return status.
 *
 * @param[in] ReturnStatus Return status for the call to give
 *
 **/
VOID
MockFfaGetMmVmId (
  IN EFI_STATUS  ReturnStatus,
  IN UINT16      *MmVmId
  )
{
  will_return (FfaGetMmVmId, ReturnStatus);
  will_return (FfaGetMmVmId, MmVmId);
}

/*
* PrepareFfaMemoryDescriptor
* Prepare the FfaMemoryDescriptor for the measurement.
*
* @param[in] Meas  Measurement buffer to be signed.
* @param[in] Size  Size of the measurement.
*
* @result EFI_SUCCESS Succesfully prepared the FfaMemoryDescriptor.
*/
EFI_STATUS
EFIAPI
PrepareFfaMemoryDescriptor (
  IN   UINT64  FfaTxBufferAddr,
  IN   UINT64  FfaTxBufferSize,
  IN   UINT8   *MeasurementBuffer,
  IN   UINT32  MeasurementBufferSize,
  IN   UINT16  SenderId,
  IN   UINT16  ReceiverId,
  OUT  UINT32  *TotalLength
  )
{
  EFI_STATUS  ReturnStatus = mock_type (EFI_STATUS);

  if (!EFI_ERROR (ReturnStatus)) {
    *TotalLength = mock_type (UINT32);
  }

  return ReturnStatus;
}

/**
 * Setup the PrepareFfaMemoryDescriptor return status.
 *
 * @param[in] ReturnStatus Return status for the call to give
 *
**/
VOID
MockPrepareFfaMemoryDescriptor (
  IN EFI_STATUS  ReturnStatus,
  IN UINT32      *TotalLength
  )
{
  will_return (PrepareFfaMemoryDescriptor, ReturnStatus);
  will_return (PrepareFfaMemoryDescriptor, TotalLength);
}

/*
* SendFfaShareCommand
* Send the FFA_SHARE_MEM_REQ_64/32 command.
*
* @param[in] TotalLength      Total length of the message.
* @param[in] FragmentLength   Fragment length of the message.
* @param[in] BufferAddr       Buffer address to share.
* @param[in] PageCount        Page count of the buffer.
* @param[out] Handle         Handle of the shared memory.
*/
EFI_STATUS
EFIAPI
FfaSendShareCommand (
  IN UINT32   TotalLength,
  IN UINT32   FragmentLength,
  IN UINT64   BufferAddr,
  IN UINT32   PageCount,
  OUT UINT64  *Handle
  )
{
  EFI_STATUS  ReturnStatus = mock_type (EFI_STATUS);

  if (!EFI_ERROR (ReturnStatus)) {
    *Handle = mock_type (UINT64);
  }

  return ReturnStatus;
}

/**
 * Setup the FfaSendShareCommand return status.
 *
 * @param[in] ReturnStatus Return status for the call to give
 *
 **/
VOID
MockFfaSendShareCommand (
  IN EFI_STATUS  ReturnStatus,
  IN UINT64      *Handle
  )
{
  will_return (FfaSendShareCommand, ReturnStatus);
  will_return (FfaSendShareCommand, Handle);
}

/**
 * Get the CPU BL Params Address.
 *
 * @param[out] CpuBlAddr   Address for the CPU Bootloader Params..
 *
 * @retval  EFI_SUCCESS    Succesfully looked up the value.
**/
EFIAPI
EFI_STATUS
GetCpuBlParamsAddrStMm (
  OUT EFI_PHYSICAL_ADDRESS  *CpuBlAddr
  )
{
  EFI_STATUS  ReturnStatus = mock_type (EFI_STATUS);

  if (!EFI_ERROR (ReturnStatus)) {
    *CpuBlAddr = mock_type (EFI_PHYSICAL_ADDRESS);
  }

  return ReturnStatus;
}

/**
 * Setup the CPU BL Params Address.
 *
 * @param[in] CpuBlAddr    Address for the CPU Bootloader Params.
 * @param[in] ReturnStatus Return status for the call to give
 *
**/
VOID
MockGetCpuBlParamsAddrStMm (
  IN EFI_PHYSICAL_ADDRESS  *CpuBlAddr,
  IN EFI_STATUS            ReturnStatus
  )
{
  will_return (GetCpuBlParamsAddrStMm, ReturnStatus);
  will_return (GetCpuBlParamsAddrStMm, CpuBlAddr);
}

/**
 * Get the NorFlashProtocol for a given socket.
 *
 * @param[in] SocketNum  Socket Number for which the NOR Flash protocol
 *                       is requested.
 * @retval    NorFlashProtocol    On Success.
 *            NULL                On Failure.
 **/
EFIAPI
NVIDIA_NOR_FLASH_PROTOCOL *
GetSocketNorFlashProtocol (
  UINT32  SocketNum
  )
{
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;

  NorFlashProtocol = NULL;

  if (SocketNum < MAX_SOCKETS) {
    NorFlashProtocol = SocketNorFlashProtocols[SocketNum];
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get NorFlash on Socket %u %r\n",
      __FUNCTION__,
      SocketNum,
      EFI_INVALID_PARAMETER
      ));
  }

  if (NorFlashProtocol == NULL) {
    DEBUG ((
      DEBUG_WARN,
      "%a:No NorFlashProtocol is installed on Socket %u\n",
      __FUNCTION__,
      SocketNum
      ));
  }

  return NorFlashProtocol;
}

/**
 * Setup the NorFlashProtocol for a given socket.
 *
 * @param[in] SocketNum         Socket Number for which the NOR Flash protocol
 *                              is to be installed.
 * @param[in] NorFlashProtocol  The NOR Flash protocol to install.
 *
 * @retval    EFI_SUCCESS           On Success.
 *            EFI_INVALID_PARAMETER Invalid Socket Number.
 **/
EFI_STATUS
EFIAPI
MockGetSocketNorFlashProtocol (
  IN UINT32                     SocketNum,
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;
  if (SocketNum < MAX_SOCKETS) {
    SocketNorFlashProtocols[SocketNum] = NorFlashProtocol;
  } else {
    Status = EFI_INVALID_PARAMETER;
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to set NorFlash on Socket %u %r\n",
      __FUNCTION__,
      SocketNum,
      Status
      ));
  }

  return Status;
}

/**
  Initialize the StandaloneMmOpteeStubLib

  @retval EFI_SUCCESS           Initialization succeeded.
**/
EFI_STATUS
EFIAPI
StandaloneMmOpteeStubLibInitialize (
  )
{
  UINTN  SocketIndex;

  for (SocketIndex = 0; SocketIndex < MAX_SOCKETS; SocketIndex++) {
    SocketNorFlashProtocols[SocketIndex] = NULL;
  }

  return EFI_SUCCESS;
}

/**
  Clean up the space used by the StandaloneMmOpteeStubLib stub if necessary.

  @retval EFI_SUCCESS Clean up was successful.
**/
EFI_STATUS
EFIAPI
StandaloneMmOpteeStubLibDestroy (
  )
{
  return EFI_SUCCESS;
}
