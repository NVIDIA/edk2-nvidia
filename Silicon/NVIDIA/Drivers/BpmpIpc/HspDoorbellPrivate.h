/** @file

  HspDoorbell private structures

  Copyright (c) 2018, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __HSP_DOORBELL_PRIVATE_H__
#define __HSP_DOORBELL_PRIVATE_H__

#include <Protocol/HspDoorbell.h>

#define HSP_DIMENSIONING    0x380

#define HSP_DB_REG_TRIGGER  0x0
#define HSP_DB_REG_ENABLE   0x4
#define HSP_DB_REG_RAW      0x8
#define HSP_DB_REG_PENDING  0xc

#define HSP_COMMON_REGION_SIZE   SIZE_64KB
#define HSP_DOORBELL_REGION_SIZE 0x100

#define HSP_MASTER_SECURE_CCPLEX 1
#define HSP_MASTER_CCPLEX        17
#define HSP_MASTER_DPMU          18
#define HSP_MASTER_BPMP          19
#define HSP_MASTER_SPE           20
#define HSP_MASTER_SCE           21
#define HSP_MASTER_APE           27

typedef UINT32 HSP_MASTER_ID;

typedef struct {
  union {
    UINT32 RawValue;
    struct {
      UINT8 SharedMailboxes:4;
      UINT8 SharedSemaphores:4;
      UINT8 ArbitratedSemaphores:4;
      UINT8 Reserved:4;
    };
  };
} HSP_DIMENSIONING_DATA;

#define HSP_MAILBOX_SHIFT_SIZE   15
#define HSP_SEMAPHORE_SHIFT_SIZE 16

//
// HspDoorbell driver private data structure
//

#define HSP_DOORBELL_SIGNATURE SIGNATURE_32('H','S','P','D')

typedef struct {
  //
  // Standard signature used to identify HspDoorbell private data
  //
  UINT32                            Signature;

  //
  // Protocol instance of NVIDIA_HSP_DOORBELL_PROTOCOL produced by this driver
  //
  NVIDIA_HSP_DOORBELL_PROTOCOL      DoorbellProtocol;

  //
  // Array of the doorbell locations
  EFI_PHYSICAL_ADDRESS              DoorbellLocation[HspDoorbellMax];

} NVIDIA_HSP_DOORBELL_PRIVATE_DATA;

#define HSP_DOORBELL_PRIVATE_DATA_FROM_THIS(a) CR(a, NVIDIA_HSP_DOORBELL_PRIVATE_DATA, DoorbellProtocol, HSP_DOORBELL_SIGNATURE)

#endif
