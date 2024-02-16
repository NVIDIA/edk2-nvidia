/** @file

NorFlashStubLib private definitions.

Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NOR_FLASH_STUB_LIB_PRIVATE_H_
#define _NOR_FLASH_STUB_LIB_PRIVATE_H_

#include <Uefi.h>
#include <Protocol/NorFlash.h>

// Note: These values DO NOT need to be kept in sync with the real values used by the real driver
// They simply need to be reasonably valid values
#define NOR_SFDP_WRITE_DEF_PAGE                        256
#define NOR_SFDP_PROGRAM_FIRST_BYTE_TIME_DEFAULT       15
#define NOR_SFDP_PROGRAM_ADDITIONAL_BYTE_TIME_DEFAULT  1
#define NOR_SFDP_PROGRAM_PAGE_TIME_DEFAULT             120
#define NOR_SFDP_PROGRAM_MAX_TIME_MULTIPLIER_DEFAULT   24

typedef struct {
  UINT32                       Signature;
  UINT8                        *Memory;
  NOR_FLASH_ATTRIBUTES         Attributes;
  NVIDIA_NOR_FLASH_PROTOCOL    Protocol;
} VIRTUAL_NOR_FLASH_DEVICE;

#define VIRTUAL_NOR_FLASH_SIGNATURE  SIGNATURE_32('v','N','O','R')

#define NOR_FLASH_DEVICE_FROM_NOR_FLASH_PROTOCOL(a)  CR(a, VIRTUAL_NOR_FLASH_DEVICE, Protocol, VIRTUAL_NOR_FLASH_SIGNATURE)

#endif
