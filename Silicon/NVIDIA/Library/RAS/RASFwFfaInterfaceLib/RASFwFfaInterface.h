/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _RASFWFFAINTERFACE_H_
#define _RASFWFFAINTERFACE_H_

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/RASFwInterfaceLib.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <IndustryStandard/ArmFfaSvc.h>

/*
 * Operation Ids for RAS_FW, to be passed as Arg3 via FFA direct messaging
 */
#define RAS_FW_NS_BUFFER_REQ       0xC0270001
#define RAS_FW_GUID_COMMUNICATION  0xC0270002

/*
 * Unique ID (UUID) that identifies the RAS_FW secure partition
 */
#define RAS_FW_UUID_0  0x3c99b242
#define RAS_FW_UUID_1  0xc93d11eb
#define RAS_FW_UUID_2  0x91012fbd
#define RAS_FW_UUID_3  0xec0769ff

/*
 * FFA function IDs that are missing from ArmFfaSvc.h
 */
#define ARM_SVC_ID_FFA_PARTITION_INFO_GET  0x84000068
#define ARM_SVC_ID_FFA_RXTX_MAP            0xC4000066
#define ARM_SVC_ID_FFA_RXTX_UNMAP          0x84000067
#define ARM_SVC_ID_FFA_RX_RELEASE          0x84000065

/* If an error source has the following event ID, it is a BERT record */
#define FFA_BERT_EVENT_ID  0xFFBE

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

#endif /* _RASFWFFAINTERFACE_H_ */
