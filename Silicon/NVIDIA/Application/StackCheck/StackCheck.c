/** @file
  The main process for StackCheck application.

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#define STACK_DATA_SIZE 32
#define STACK_WRITE_SIZE 33

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers, including
  both device drivers and bus drivers.

  The entry point for StackCheck application that should casue an abort due to stack overwrite.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS    The operation completed successfully.

**/
EFI_STATUS
EFIAPI
InitializeStackCheck (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
  )
{
  UINT8 TestArray[STACK_DATA_SIZE];
  Print (L"Application should not return to shell as stack checker should abort\r\n");

  SetMem (TestArray, STACK_WRITE_SIZE, 0);
  return EFI_SUCCESS;
}
