/** @file
  Fixed feature flags parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "FixedFeatureFlagsParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>

/** Fixed feature flags parser function.

  The following structure is populated:
  typedef struct CmArmFixedFeatureFlags {
    /// The Fixed feature flags
    UINT32    Flags;                    // {Populated}
  } CM_ARCH_COMMON_FIXED_FEATURE_FLAGS;

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
FixedFeatureFlagsParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                          Status;
  CM_ARCH_COMMON_FIXED_FEATURE_FLAGS  FixedFeatureFlags;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  // The set of FADT Fixed Feature flag bits to publish is platform-defined
  // via PcdAcpiFadtFixedFeatureFlags. Each platform DSC selects the bits
  // that match its actual ACPI capabilities (e.g. PWR_BUTTON when the DSDT
  // declares a PNP0C0C device). The FADT generator OR's these bits into
  // EFI_ACPI_x_x_FIXED_ACPI_DESCRIPTION_TABLE.Flags, masked by the
  // hardware-reduced / non-hardware-reduced valid-flag mask.
  FixedFeatureFlags.Flags = FixedPcdGet32 (PcdAcpiFadtFixedFeatureFlags);

  // Skip publishing an empty CmObj when the platform did not select any
  // bits, preserving prior behaviour for SoCs that do not need this object.
  if (FixedFeatureFlags.Flags == 0) {
    return EFI_SUCCESS;
  }

  Status = NvAddSingleCmObj (
             ParserHandle,
             CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjFixedFeatureFlags),
             &FixedFeatureFlags,
             sizeof (FixedFeatureFlags),
             NULL
             );
  ASSERT_EFI_ERROR (Status);
  return Status;
}

REGISTER_PARSER_FUNCTION (FixedFeatureFlagsParser, NULL)
