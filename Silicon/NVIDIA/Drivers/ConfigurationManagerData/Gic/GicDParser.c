/** @file
  GicD parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "GicParser.h"
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/NVIDIADebugLib.h>

/** GicD parser function

  The following structure is populated:
  typedef struct CmArmGicDInfo {
    /// The Physical Base address for the GIC Distributor.
    UINT64    PhysicalBaseAddress;

    // The global system interrupt
    //  number where this GIC Distributor's
    //  interrupt inputs start.
    UINT32    SystemVectorBase;

    // The GIC version as described
    // by the GICD structure in the
    // ACPI Specification.
    UINT8     GicVersion;
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
GicDParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                        Status;
  UINT32                            NumberOfGicCtlrs;
  UINT32                            *GicHandles;
  TEGRA_GIC_INFO                    *GicInfo;
  CM_ARM_GICD_INFO                  GicDInfo;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            RegisterSize;

  GicHandles   = NULL;
  GicInfo      = NULL;
  RegisterData = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  // Get GIC Info
  GicInfo = (TEGRA_GIC_INFO *)AllocatePool (sizeof (TEGRA_GIC_INFO));
  if (!GetGicInfo (GicInfo)) {
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  // Get GIC Handles
  NumberOfGicCtlrs = 0;
  Status           = GetMatchingEnabledDeviceTreeNodes (GicInfo->GicCompatString, NULL, &NumberOfGicCtlrs);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto CleanupAndReturn;
  }

  GicHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfGicCtlrs);
  if (GicHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (GicInfo->GicCompatString, GicHandles, &NumberOfGicCtlrs);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Get Register Info using the Gic Handle
  RegisterSize = 0;
  Status       = GetDeviceTreeRegisters (GicHandles[0], RegisterData, &RegisterSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
    if (RegisterData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    Status = GetDeviceTreeRegisters (GicHandles[0], RegisterData, &RegisterSize);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }
  } else if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  if (RegisterSize < 1) {
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  // GICD structure entry
  GicDInfo.PhysicalBaseAddress = RegisterData[0].BaseAddress;
  GicDInfo.SystemVectorBase    = 0;
  GicDInfo.GicVersion          = GicInfo->Version;

  // Add the CmObj to the Configuration Manager.
  Status = NvAddSingleCmObj (
             ParserHandle,
             CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo),
             &GicDInfo,
             sizeof (GicDInfo),
             NULL
             );
  ASSERT_EFI_ERROR (Status);

CleanupAndReturn:
  FREE_NON_NULL (GicHandles);
  FREE_NON_NULL (GicInfo);
  FREE_NON_NULL (RegisterData);
  return Status;
}
