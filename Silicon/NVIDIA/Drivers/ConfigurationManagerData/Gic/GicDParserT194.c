/** @file
  GicD parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/NvCmObjectDescUtility.h>
#include "GicParser.h"
#include <Library/NVIDIADebugLib.h>

/** GicD parser function for T194.

  The following structure is populated:
  typedef struct CmArmGicDInfo {
    /// The Physical Base address for the GIC Distributor.
    UINT64    PhysicalBaseAddress;      // {Populated}

    // The global system interrupt
    //  number where this GIC Distributor's
    //  interrupt inputs start.
    UINT32    SystemVectorBase;         // 0

    // The GIC version as described
    // by the GICD structure in the
    // ACPI Specification.
    UINT8     GicVersion;               // 2
  } CM_ARM_GICD_INFO;

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
GicDParserT194 (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS        Status;
  CM_ARM_GICD_INFO  GicDInfo = {
    0,
    0,
    2
  };

  GicDInfo.PhysicalBaseAddress = PcdGet64 (PcdGicDistributorBase);

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  // Add the CmObj to the Configuration Manager.
  Status = NvAddSingleCmObj (
             ParserHandle,
             CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo),
             &GicDInfo,
             sizeof (GicDInfo),
             NULL
             );
  ASSERT_EFI_ERROR (Status);
  return Status;
}
