/** @file

Stub implementation of StandaloneMmOpteeLib.

Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Protocol/NorFlash.h>

#include <Library/DebugLib.h>
#include <HostBasedTestStubLib/StandaloneMmOpteeStubLib.h>

#define MAX_SOCKETS  4

STATIC NVIDIA_NOR_FLASH_PROTOCOL  *SocketNorFlashProtocols[MAX_SOCKETS];

/**
 * Get the CPU BL Params Address.
 *
 * @param[out] CpuBlAddr   Address for the CPU Bootloader Params..
 *
 * @retval  EFI_SUCCESS    Succesfully looked up the value.
**/
EFIAPI
EFI_STATUS
GetCpuBlParamsAddrStMm (
  OUT EFI_PHYSICAL_ADDRESS  *CpuBlAddr
  )
{
  EFI_STATUS  ReturnStatus = mock_type (EFI_STATUS);

  if (!EFI_ERROR (ReturnStatus)) {
    *CpuBlAddr = mock_type (EFI_PHYSICAL_ADDRESS);
  }

  return ReturnStatus;
}

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
  )
{
  will_return (GetCpuBlParamsAddrStMm, ReturnStatus);
  will_return (GetCpuBlParamsAddrStMm, CpuBlAddr);
}

/**
 * Get the NorFlashProtocol for a given socket.
 *
 * @param[in] SocketNum  Socket Number for which the NOR Flash protocol
 *                       is requested.
 * @retval    NorFlashProtocol    On Success.
 *            NULL                On Failure.
 **/
EFIAPI
NVIDIA_NOR_FLASH_PROTOCOL *
GetSocketNorFlashProtocol (
  UINT32  SocketNum
  )
{
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;

  NorFlashProtocol = NULL;

  if (SocketNum < MAX_SOCKETS) {
    NorFlashProtocol = SocketNorFlashProtocols[SocketNum];
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get NorFlash on Socket %u %r\n",
      __FUNCTION__,
      SocketNum,
      EFI_INVALID_PARAMETER
      ));
  }

  if (NorFlashProtocol == NULL) {
    DEBUG ((
      DEBUG_WARN,
      "%a:No NorFlashProtocol is installed on Socket %u\n",
      __FUNCTION__,
      SocketNum
      ));
  }

  return NorFlashProtocol;
}

/**
 * Setup the NorFlashProtocol for a given socket.
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
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;
  if (SocketNum < MAX_SOCKETS) {
    SocketNorFlashProtocols[SocketNum] = NorFlashProtocol;
  } else {
    Status = EFI_INVALID_PARAMETER;
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to set NorFlash on Socket %u %r\n",
      __FUNCTION__,
      SocketNum,
      Status
      ));
  }

  return Status;
}

/**
  Initialize the StandaloneMmOpteeStubLib

  @retval EFI_SUCCESS           Initialization succeeded.
**/
EFI_STATUS
EFIAPI
StandaloneMmOpteeStubLibInitialize (
  )
{
  UINTN  SocketIndex;

  for (SocketIndex = 0; SocketIndex < MAX_SOCKETS; SocketIndex++) {
    SocketNorFlashProtocols[SocketIndex] = NULL;
  }

  return EFI_SUCCESS;
}

/**
  Clean up the space used by the StandaloneMmOpteeStubLib stub if necessary.

  @retval EFI_SUCCESS Clean up was successful.
**/
EFI_STATUS
EFIAPI
StandaloneMmOpteeStubLibDestroy (
  )
{
  return EFI_SUCCESS;
}
