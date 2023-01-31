/** @file
*
*  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef NVIDIA_MM_MB1_RECORD_H__
#define NVIDIA_MM_MB1_RECORD_H__

#include <Uefi.h>
#include <TH500/TH500MB1Configuration.h>

#define NVIDIA_MM_MB1_RECORD_READ_CMD   0x0
#define NVIDIA_MM_MB1_RECORD_WRITE_CMD  0x1
#define NVIDIA_MM_MB1_ERASE_PARTITION   0x2

typedef struct {
  UINT32        Command;
  EFI_STATUS    Status;
  UINT8         Data[TEGRABL_EARLY_BOOT_VARS_MAX_SIZE-sizeof (TEGRABL_EARLY_BOOT_VARS_DATA_HEADER)];
} NVIDIA_MM_MB1_RECORD_PAYLOAD;

#define NVIDIA_MM_MB1_RECORD_GUID  \
    { 0x46213b77, 0xf89a, 0x43e8, { 0x9d, 0xf7, 0x67, 0xc6, 0x74, 0x07, 0x2d, 0xf4 } }

extern EFI_GUID  gNVIDIAMmMb1RecordGuid;

#endif //NVIDIA_MM_MB1_RECORD_H__
