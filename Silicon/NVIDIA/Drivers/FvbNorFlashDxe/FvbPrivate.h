/** @file

  Fvb Driver Private Data

  SPDX-FileCopyrightText: Copyright (c) 2018 - 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
#include <Library/StandaloneMmOpteeDeviceMem.h>

#include <Protocol/FirmwareVolumeBlock.h>
#include <Protocol/NorFlash.h>
#include <Uefi/UefiGpt.h>

#include <Guid/VariableFormat.h>
#include <Guid/RtPropertiesTable.h>
#include <Guid/SystemNvDataGuid.h>

#define UEFI_VARIABLE_PARTITION_NAME  L"uefi_variables"
#define FTW_PARTITION_NAME            L"uefi_ftw"
#define RESERVED_PARTITION_NAME       L"reserved_partition"
#define NVIDIA_FVB_SIGNATURE          SIGNATURE_32('N','F','V','B')
#define NVIDIA_FWB_SIGNATURE          SIGNATURE_32('N','F','W','B')
#define NVIDIA_FSB_SIGNATURE          SIGNATURE_32('N','F','S','B')
#define NVIDIA_INT_SIGNATURE          SIGNATURE_32('N','I','N','T')
#define NVIDIA_FVB_PRIVATE_DATA_FROM_FVB_PROTOCOL(a)  CR(a, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol, NVIDIA_FVB_SIGNATURE)

#define GPT_PARTITION_BLOCK_SIZE  512
#define FVB_TO_CREATE             3
#define FVB_VARIABLE_INDEX        0
#define FVB_FTW_SPARE_INDEX       1
#define FVB_FTW_WORK_INDEX        2
#define FVB_VAR_INT_INDEX         3

#define FVB_ERASED_BYTE  0xFF
#define VAR_INT_PENDING  0xFE
#define VAR_INT_VALID    0xFC
#define VAR_INT_INVALID  0xF8

typedef struct {
  UINT32                                 Signature;
  NVIDIA_NOR_FLASH_PROTOCOL              *NorFlashProtocol;
  EFI_EVENT                              FvbVirtualAddrChangeEvent;
  NOR_FLASH_ATTRIBUTES                   FlashAttributes;
  UINT8                                  *PartitionData;
  UINT32                                 PartitionOffset;
  UINT32                                 PartitionSize;
  EFI_PHYSICAL_ADDRESS                   PartitionAddress;
  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL    FvbProtocol;
  EFI_HANDLE                             Handle;
} NVIDIA_FVB_PRIVATE_DATA;

EFI_STATUS
EFIAPI
VarIntInit (
  IN UINTN                      PartitionStartOffset,
  IN UINTN                      PartitionSize,
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProto,
  IN NOR_FLASH_ATTRIBUTES       *NorFlashAttributes
  );

EFI_STATUS
EFIAPI
VarIntValidate (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  );

BOOLEAN
IsMeasurementPartitionErased (
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProto,
  UINT64                     PartitionStartOffset,
  UINT64                     PartitionSize
  );

#endif
