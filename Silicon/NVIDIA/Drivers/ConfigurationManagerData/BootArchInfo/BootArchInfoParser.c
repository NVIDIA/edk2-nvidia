/** @file
  Boot arch info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "BootArchInfoParser.h"
#include <Library/NVIDIADebugLib.h>

/** Boot arch info parser function.

  The following structure is populated:
  typedef struct CmArmBootArchInfo {
    // This is the ARM_BOOT_ARCH flags field of the FADT Table
    // described in the ACPI Table Specification.
    UINT16    BootArchFlags;              // {Populated}
  } CM_ARM_BOOT_ARCH_INFO;

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
BootArchInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS             Status;
  CM_ARM_BOOT_ARCH_INFO  BootArchInfo = {
    EFI_ACPI_6_4_ARM_PSCI_COMPLIANT
  };

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  // Add the CmObj to the Configuration Manager.
  Status = NvAddSingleCmObj (
             ParserHandle,
             CREATE_CM_ARM_OBJECT_ID (EArmObjBootArchInfo),
             &BootArchInfo,
             sizeof (BootArchInfo),
             NULL
             );
  ASSERT_EFI_ERROR (Status);
  return Status;
}
