/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _RASFWHSPINTERFACE_H_
#define _RASFWHSPINTERFACE_H_

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/RASFwInterfaceLib.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

/**
 * @brief Request/response API over 128-bit HSP mailbox.
 */
typedef struct {
  union {
    UINT32    Request;
    UINT32    Result;
  };

  union {
    UINT32    Arg0;
    UINT32    Res0;
  };

  union {
    UINT32    Arg1;
    UINT32    Res1;
  };

  union {
    UINT32    Arg2;
    UINT32    Res2;
  };
} RAS_HSP_API_CMD_RESP;

/**
 * @brief Possible Request values to use in the HSP mailbox
 */
#define UEFI_CMD_GET_APEI_BUFFER  0x52550001
#define UEFI_CMD_MM_COMMUNICATE   0x52550002

/* If an error source has the following event ID, it is a BERT record */
#define HSP_BERT_EVENT_ID  0x4000

#endif /* _RASFWHSPINTERFACE_H_ */
