/** @file

StandaloneMmOpteeStubLib definitions.

Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _STANDALONE_MM_OPTEE_STUB_LIB_H_
#define _STANDALONE_MM_OPTEE_STUB_LIB_H_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Protocol/NorFlash.h>

/**
 * Setup the CPU BL Params Address.
 *
 * @param[in] CpuBlAddr    Address for the CPU Bootloader Params.
 * @param[in] ReturnStatus Return status for the call to give
 *
**/
VOID
MockGetCpuBlParamsAddrStMm (
  IN EFI_PHYSICAL_ADDRESS  *CpuBlAddr,
  IN EFI_STATUS            ReturnStatus
  );

/**
 * Set the NorFlashProtocol for a given socket.
 *
 * @param[in] SocketNum         Socket Number for which the NOR Flash protocol
 *                              is to be installed.
 * @param[in] NorFlashProtocol  The NOR Flash protocol to install.
 *
 * @retval    EFI_SUCCESS           On Success.
 *            EFI_INVALID_PARAMETER Invalid Socket Number.
 **/
EFI_STATUS
EFIAPI
MockGetSocketNorFlashProtocol (
  IN UINT32                     SocketNum,
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol
  );

/**
  Initialize the StandaloneMmOpteeStubLib

  @retval EFI_SUCCESS           Initialization succeeded.
**/
EFI_STATUS
EFIAPI
StandaloneMmOpteeStubLibInitialize (
  );

/**
  Clean up the space used by the StandaloneMmOpteeStubLib stub if necessary.

  @retval EFI_SUCCESS Clean up was successful.
**/
EFI_STATUS
EFIAPI
StandaloneMmOpteeStubLibDestroy (
  );

#endif
