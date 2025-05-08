/** @file
*  Library to send messages to an SP using FF-A messages.
*
*  Copyright (c) 2025, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "ProcessorBind.h"
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Server/RASNSInterface.h>
#include <Protocol/MmCommunication2.h>
#include <Library/ArmSmcLib.h>
#include <Protocol/MmCommunication2.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/SecureFwCommLib.h>

/**
 * Allocate RX/TX buffers for FFA communication.
 * Note: the RX/TX buffers are shared for the entire NS world, so they must be
 * freed after use.
 *
 * @param[in]  pages           Number of pages for both buffers
 * @param[out] rx              Address of the RX buffer
 * @param[out] tx              Address of the TX buffer
 *
 * @return EFI_SUCCESS         Buffers allocated and mapped with Hafnium
 *
**/
STATIC
EFI_STATUS
FfaAllocateAndMapRxTxBuffers (
  IN  UINTN             pages,
  OUT PHYSICAL_ADDRESS  *rx,
  OUT PHYSICAL_ADDRESS  *tx
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData, pages, rx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RX buffer allocation failed\n", __FUNCTION__));
    goto out;
  }

  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData, pages, tx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TX buffer allocation failed\n", __FUNCTION__));
    goto out_rx;
  }

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RXTX_MAP;
  ArmSmcArgs.Arg1 = (UINTN)*tx;
  ArmSmcArgs.Arg2 = (UINTN)*rx;
  ArmSmcArgs.Arg3 = pages;

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RXTX_MAP failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
    goto out_tx;
  }

  goto out;

out_tx:
  gBS->FreePages (*tx, pages);
out_rx:
  gBS->FreePages (*rx, pages);
out:
  return Status;
}

/**
 * Free the RX buffer after reading data from it.
 *
 * @return EFI_SUCCESS         RX buffer released.
 *
**/
STATIC
EFI_STATUS
FfaReleaseRxBuffer (
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RX_RELEASE;
  ArmSmcArgs.Arg1 = 0; /* NS World */

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RX_RELEASE failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
  }

  return Status;
}

/**
 * Unmap RX/TX buffers and free them.
 *
 * @param[in]  pages           Number of pages for both buffers
 * @param[in]  rx              Address of the RX buffer
 * @param[in]  tx              Address of the TX buffer
 *
 * @return EFI_SUCCESS         Buffers unmapped and freed.
 *
**/
STATIC
EFI_STATUS
FfaFreeRxTxBuffers (
  IN  UINTN             pages,
  IN  PHYSICAL_ADDRESS  rx,
  IN  PHYSICAL_ADDRESS  tx
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RXTX_UNMAP;
  ArmSmcArgs.Arg1 = 0; /* NS World */

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RXTX_UNMAP failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
  }

  gBS->FreePages (tx, pages);
  gBS->FreePages (rx, pages);

  return Status;
}

/**
 * Get the SP's Partition Id by using its UUID and querying Hafnium via FFA.

 * @param[in]  Uuid0         UUID0
 * @param[in]  Uuid1         UUID1
 * @param[in]  Uuid2         UUID2
 * @param[in]  Uuid3         UUID3
 *
 * @return VmId            Return the VmId or 0 if not found
 *
**/
UINT16
FfaGetFwPartitionId (
  UINTN  Uuid0,
  UINTN  Uuid1,
  UINTN  Uuid2,
  UINTN  Uuid3
  )
{
  PHYSICAL_ADDRESS  rx, tx;
  UINT16            VmId;
  UINTN             pages  = 1;
  EFI_STATUS        Status = EFI_SUCCESS;

  Status = FfaAllocateAndMapRxTxBuffers (pages, &rx, &tx);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_PARTITION_INFO_GET;
  ArmSmcArgs.Arg1 = Uuid0;
  ArmSmcArgs.Arg2 = Uuid1;
  ArmSmcArgs.Arg3 = Uuid2;
  ArmSmcArgs.Arg4 = Uuid3;

  CallFfaSmc (&ArmSmcArgs);

  /* One SP should have been found */
  if (ArmSmcArgs.Arg2 != 1) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_PARTITION_INFO_GET failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    return 0;
  }

  VmId = *((UINT16 *)rx);
  DEBUG ((DEBUG_INFO, "%a: RAS_FW VmId=0x%x\n", __FUNCTION__, VmId));

  FfaReleaseRxBuffer ();
  Status = FfaFreeRxTxBuffers (pages, rx, tx);

  return VmId;
}

/**
 * Get the communication buffer from the SP we want to talk to VM.
 *
 * @param[out] CommBuffer      Address of the communication buffer
 * @param[out] CommBufferSize  Size of the communication buffer
 * @param[in]  PartitionId     Partition ID of the SP
 * @param[in]  GetCommFnId     Function ID to get the communication buffer.
*                              If left NULL, the default function Id used to get
*     this from StMM/RAS_FW is used.
 *
 * @return EFI_SUCCESS         Communication buffer retrieved successfully
 *         OTHERWISE           Error code
**/
EFI_STATUS
FfaGetCommunicationBuffer (
  OUT PHYSICAL_ADDRESS  *CommBuffer,
  OUT UINTN             *CommBufferSize,
  IN  UINT16            PartitionId,
  IN  UINTN             *GetCommFnId  OPTIONAL
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ;
  ArmSmcArgs.Arg1 = PartitionId;

  if (GetCommFnId == NULL) {
    ArmSmcArgs.Arg3 = RAS_FW_NS_BUFFER_REQ;
  } else {
    ArmSmcArgs.Arg3 = *GetCommFnId;
  }

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg0 != ARM_FID_FFA_MSG_SEND_DIRECT_RESP) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid FFA response: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg0
      ));
    return EFI_INVALID_PARAMETER;
  }

  *CommBuffer     = (PHYSICAL_ADDRESS)ArmSmcArgs.Arg4;
  *CommBufferSize = ArmSmcArgs.Arg5;

  DEBUG ((DEBUG_ERROR, "%a: CommBuffer=0x%lx, CommBufferSize=%u\n", __FUNCTION__, *CommBuffer, *CommBufferSize));
  return EFI_SUCCESS;
}

/**
 * Send a message to the SP using FFA. This follows the MM_COMMUNICATE defined protocol for setting
 * Arg3 onwards as the SMC Function ID etc.
 *
 * @param[in]  CommunicateHeader  Pointer to the communication header + the payload
 * @param[in]  CommBuffer         Pointer to the communication buffer
 * @param[in]  CommBufferSize     Size of the communication buffer
 * @param[in]  PartitionId        Partition ID of the SP
 * @param[in]  SmcFunctionId      SMC Function ID
 *
 * @return EFI_SUCCESS         Message sent successfully
 *         OTHERWISE           Error code
**/
EFI_STATUS
FfaGuidedCommunication (
  IN EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader,
  IN PHYSICAL_ADDRESS           CommBuffer,
  IN UINTN                      CommBufferSize,
  IN UINT16                     PartitionId,
  IN UINTN                      SmcFunctionId
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  UINTN         BufferSize;

  if ((CommBuffer == 0) || (CommBufferSize == 0) || (CommunicateHeader == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ;
  ArmSmcArgs.Arg1 = PartitionId;
  // Reserved for future use(MBZ)
  ArmSmcArgs.Arg2 = 0x0;
  ArmSmcArgs.Arg3 = SmcFunctionId;
  // Cookie
  ArmSmcArgs.Arg4 = 0x0;
  // comm_buffer_address (64-bit physical address)
  ArmSmcArgs.Arg5 = (UINTN)CommBuffer;
  // comm_size_address (not used, indicated by setting to zero)
  ArmSmcArgs.Arg6 = 0;

  BufferSize = OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) + CommunicateHeader->MessageLength;

  DEBUG ((DEBUG_ERROR, "%a: BufferSize=%u, CommBufferSize=%u\n", __FUNCTION__, BufferSize, CommBufferSize));
  if (BufferSize > CommBufferSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: buffer size too small: %u\n",
      __FUNCTION__,
      BufferSize
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  MmioWriteBuffer64 (CommBuffer, BufferSize, (UINT64 *)CommunicateHeader);

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg0 != ARM_FID_FFA_MSG_SEND_DIRECT_RESP) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid FFA response: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg0
      ));
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}
