/** @file

  Fvb Driver Private Data

  Copyright (c) 2018-2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FVB_PRIVATE_H__
#define __FVB_PRIVATE_H__

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Protocol/FirmwareVolumeBlock.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/FaultTolerantWrite.h>

#include <Guid/VariableFormat.h>
#include <Guid/RtPropertiesTable.h>

typedef struct {
  EFI_BLOCK_IO_PROTOCOL                  *BlockIo;
  UINT8                                  *VariablePartition;
  EFI_EVENT                              FvbVirtualAddrChangeEvent;
  EFI_LBA                                PartitionStartingLBA;
  EFI_LBA                                NumBlocks;
  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL    FvbInstance;
  EFI_FAULT_TOLERANT_WRITE_PROTOCOL      FtwInstance;
} NVIDIA_FVB_PRIVATE_DATA;

#endif
