/** @file

  Fvb Driver Private Data

  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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

#include <Protocol/FirmwareVolumeBlock.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/FaultTolerantWrite.h>

#include <Guid/VariableFormat.h>

typedef struct {
  EFI_BLOCK_IO_PROTOCOL               *BlockIo;
  UINT8                               *VariablePartition;
  EFI_EVENT                           FvbVirtualAddrChangeEvent;
  EFI_LBA                             PartitionStartingLBA;
  EFI_LBA                             NumBlocks;
  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL FvbInstance;
  EFI_FAULT_TOLERANT_WRITE_PROTOCOL   FtwInstance;
} NVIDIA_FVB_PRIVATE_DATA;

#endif
