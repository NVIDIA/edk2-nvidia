/** @file

  Fvb Driver Private Data

  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/DevicePathLib.h>
#include <Library/GptLib.h>

#include <Protocol/FirmwareVolumeBlock.h>
#include <Protocol/NorFlash.h>
#include <Uefi/UefiGpt.h>

#include <Guid/VariableFormat.h>
#include <Guid/RtPropertiesTable.h>

#define UEFI_VARIABLE_PARTITION_NAME L"uefi_variables"
#define FTW_PARTITION_NAME L"uefi_ftw"

typedef struct {
  UINT32                              Signature;
  NVIDIA_NOR_FLASH_PROTOCOL           *NorFlashProtocol;
  EFI_EVENT                           FvbVirtualAddrChangeEvent;
  NOR_FLASH_ATTRIBUTES                FlashAttributes;
  UINT8                               *PartitionData;
  UINT32                              PartitionOffset;
  UINT32                              PartitionSize;
  EFI_PHYSICAL_ADDRESS                PartitionAddress;
  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL FvbProtocol;
  EFI_HANDLE                          Handle;
} NVIDIA_FVB_PRIVATE_DATA;

#define NVIDIA_FVB_SIGNATURE SIGNATURE_32('N','F','V','B')
#define NVIDIA_FVB_PRIVATE_DATA_FROM_FVB_PROTOCOL(a)   CR(a, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol, NVIDIA_FVB_SIGNATURE)

#define GPT_PARTITION_BLOCK_SIZE 512
#define FVB_TO_CREATE 3
#define FVB_VARIABLE_INDEX  0
#define FVB_FTW_SPARE_INDEX 1
#define FVB_FTW_WORK_INDEX  2

#define FVB_ERASED_BYTE 0xFF

#endif
