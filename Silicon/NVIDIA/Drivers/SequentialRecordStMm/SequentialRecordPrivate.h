/** @file

  Private Sequential record protocol/header definitions.

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SEQUENTIAL_RECORD_PVT_H
#define SEQUENTIAL_RECORD_PVT_H

#include <Library/MmServicesTableLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Protocol/SequentialRecord.h>
#include <TH500/TH500MB1Configuration.h>
#include <Guid/NVIDIAMmMb1Record.h>

#define READ_LAST_RECORD   (0)
#define WRITE_NEXT_RECORD  (1)
#define ERASE_PARTITION    (2)

typedef struct {
  /* Operation to perform */
  UINTN         Function;
  /* Return value in the EFI standard. Initialized as EFI_SUCCESS when making a request. */
  EFI_STATUS    ReturnStatus;
  /* Socket number [0-3] */
  UINTN         Socket;
  /* Extra data (ie data to write when RAS_FW requests a write, or read data from MM when returning a read request */
  UINT8         Data[]; /* Flexible array member */
} RAS_MM_COMMUNICATE_PAYLOAD;

#endif // SEQUENTIAL_RECORD_PVT_H
