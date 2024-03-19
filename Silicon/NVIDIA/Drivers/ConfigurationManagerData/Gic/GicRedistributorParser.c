/** @file
  GicRegistributor parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/NvCmObjectDescUtility.h>
#include "GicParser.h"
#include <Library/ArmGicLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/IoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/NVIDIADebugLib.h>

// In GICv3, there are 2 x 64KB frames:
// Redistributor control frame + SGI Control & Generation frame
#define GIC_V3_REDISTRIBUTOR_GRANULARITY  (ARM_GICR_CTLR_FRAME_SIZE           \
                                           + ARM_GICR_SGI_PPI_FRAME_SIZE)

// In GICv4, there are 2 additional 64KB frames:
// VLPI frame + Reserved page frame
#define GIC_V4_REDISTRIBUTOR_GRANULARITY  (GIC_V3_REDISTRIBUTOR_GRANULARITY   \
                                           + ARM_GICR_SGI_VLPI_FRAME_SIZE     \
                                           + ARM_GICR_SGI_RESERVED_FRAME_SIZE)

STATIC
UINTN
GicGetRedistributorSize (
  IN UINTN  GicRedistributorBase
  )
{
  UINTN   GicCpuRedistributorBase;
  UINT64  TypeRegister;

  GicCpuRedistributorBase = GicRedistributorBase;

  do {
    TypeRegister = MmioRead64 (GicCpuRedistributorBase + ARM_GICR_TYPER);

    // Move to the next GIC Redistributor frame.
    // The GIC specification does not forbid a mixture of redistributors
    // with or without support for virtual LPIs, so we test Virtual LPIs
    // Support (VLPIS) bit for each frame to decide the granularity.
    // Note: The assumption here is that the redistributors are adjacent
    // for all CPUs. However this may not be the case for NUMA systems.
    GicCpuRedistributorBase += (((ARM_GICR_TYPER_VLPIS & TypeRegister) != 0)
                                ? GIC_V4_REDISTRIBUTOR_GRANULARITY
                                : GIC_V3_REDISTRIBUTOR_GRANULARITY);
  } while ((TypeRegister & ARM_GICR_TYPER_LAST) == 0);

  return GicCpuRedistributorBase - GicRedistributorBase;
}

/** GicRedistributor parser function

  The following structure is populated:
  typedef struct CmArmGicRedistInfo {
    // The physical address of a page range
    // containing all GIC Redistributors.
    //
    UINT64    DiscoveryRangeBaseAddress;

    /// Length of the GIC Redistributor Discovery page range
    UINT32    DiscoveryRangeLength;
  } CM_ARM_GIC_REDIST_INFO;

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
GicRedistributorParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                        Status;
  UINT32                            NumberOfGicCtlrs;
  UINT32                            *GicHandles;
  TEGRA_GIC_INFO                    *GicInfo;
  CM_ARM_GIC_REDIST_INFO            *GicRedistInfo;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            RegisterSize;
  CM_OBJ_DESCRIPTOR                 Desc;
  UINT32                            GicRedistInfoSize;
  UINT32                            Index;

  GicHandles    = NULL;
  GicInfo       = NULL;
  GicRedistInfo = NULL;
  RegisterData  = NULL;

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

  // Redistributor is only relevant for GICv3 and following
  if (GicInfo->Version < 3) {
    Status = EFI_SUCCESS;
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

  GicRedistInfoSize = sizeof (CM_ARM_GIC_REDIST_INFO) * NumberOfGicCtlrs;
  GicRedistInfo     = (CM_ARM_GIC_REDIST_INFO *)AllocateZeroPool (GicRedistInfoSize);
  if (GicRedistInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // Get Register Info using the Gic Handles
  RegisterSize = 0;
  for (Index = 0; Index < NumberOfGicCtlrs; Index++) {
    Status = GetDeviceTreeRegisters (GicHandles[Index], RegisterData, &RegisterSize);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      FREE_NON_NULL (RegisterData);

      RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
      if (RegisterData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CleanupAndReturn;
      }

      Status = GetDeviceTreeRegisters (GicHandles[Index], RegisterData, &RegisterSize);
      if (EFI_ERROR (Status)) {
        goto CleanupAndReturn;
      }
    } else if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    if (RegisterSize < 2) {
      Status = EFI_NOT_FOUND;
      goto CleanupAndReturn;
    }

    GicRedistInfo[Index].DiscoveryRangeBaseAddress = RegisterData[1].BaseAddress;
    GicRedistInfo[Index].DiscoveryRangeLength      = GicGetRedistributorSize (RegisterData[1].BaseAddress);
  }

  // Add the CmObj to the Configuration Manager.
  Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicRedistributorInfo);
  Desc.Size     = GicRedistInfoSize;
  Desc.Count    = NumberOfGicCtlrs;
  Desc.Data     = GicRedistInfo;

  Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (GicHandles);
  FREE_NON_NULL (GicInfo);
  FREE_NON_NULL (RegisterData);
  FREE_NON_NULL (GicRedistInfo);
  return Status;
}
