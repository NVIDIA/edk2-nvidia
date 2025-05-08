/** @file

  Library to send messages to an SP using FF-A messages.

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SECURE_FW_COMM_LIB_H__
#define __SECURE_FW_COMM_LIB_H__

#include <Protocol/MmCommunication2.h>
#include <IndustryStandard/ArmFfaSvc.h>

/*
 * FFA function IDs that are missing from ArmFfaSvc.h
 */
#define ARM_SVC_ID_FFA_PARTITION_INFO_GET  0x84000068
#define ARM_SVC_ID_FFA_RXTX_MAP            0xC4000066
#define ARM_SVC_ID_FFA_RXTX_UNMAP          0x84000067
#define ARM_SVC_ID_FFA_RX_RELEASE          0x84000065

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
  );

/**
 * Get the communication buffer from the SP we want to talk to VM.
 *
 * @param[out] CommBuffer      Address of the communication buffer
 * @param[out] CommBufferSize  Size of the communication buffer
 * @param[in]  PartitionId     Partition ID of the SP
 * @param[in]  GetCommFnId     Function ID to get the communication buffer
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
  );

/**
 * Send a message to the SP using FFA. This follows the MM_COMMUNICATE defined protocol for setting
 * Arg3 onwards as the SMC Function ID etc.
 *
 * @param[in]  CommunicateHeader  Pointer to the communication header
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
  );

/**
 * Call an SMC to send an FFA request. This function is similar to
 * ArmCallSmc except that it returns extra GP registers as needed for FFA.
 *
 * @param Args    GP registers to send with the SMC request
 */
VOID
CallFfaSmc (
  IN OUT ARM_SMC_ARGS  *Args
  );

#endif
