/** @file
  SDMMC Driver specific Query and Response functions

  Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SdMmcConfigurationData.h"

#include <libfdt.h>
#include <PlatformToDriverStructures.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

EFI_STATUS
EFIAPI
ResponseSdMmcParameters (
  IN CONST  VOID                               *ParameterBlock,
  IN CONST  EFI_PLATFORM_CONFIGURATION_ACTION  ConfigurationAction
  )
{
  if ( ParameterBlock == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  switch (ConfigurationAction) {
    case EfiPlatformConfigurationActionNone:
    case EfiPlatformConfigurationActionUnsupportedGuid:
      FreePool ((VOID *)ParameterBlock);
      return EFI_SUCCESS;

    case EfiPlatformConfigurationActionStopController:
    case EfiPlatformConfigurationActionRestartController:
    case EfiPlatformConfigurationActionRestartPlatform:
    case EfiPlatformConfigurationActionNvramFailed:
      DEBUG ((DEBUG_ERROR, "Handling not supported for ConfigurationAction %d\r\n", ConfigurationAction));
      return EFI_INVALID_PARAMETER;

    default:
      return EFI_SUCCESS;
  }
}

EFI_STATUS
EFIAPI
QuerySdMmcParameters (
  IN OUT   VOID                                    **ParameterBlock,
  IN OUT   UINTN                                   *ParameterBlockSize,
  IN       CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DtNode
  )
{
  SDMMC_PARAMETER_INFO  *SdmmcParameterInfo;
  CONST UINT32          *RegulatorPointer;

  if ((ParameterBlock == NULL) ||
      (ParameterBlockSize == NULL) ||
      (DtNode == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  *ParameterBlockSize = sizeof (SDMMC_PARAMETER_INFO);

  *ParameterBlock = (VOID *)AllocateZeroPool (*ParameterBlockSize);

  if (*ParameterBlock == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SdmmcParameterInfo = (SDMMC_PARAMETER_INFO *)*ParameterBlock;

  // Obtaining SDMMC parameters from the DT
  RegulatorPointer = (CONST UINT32 *)fdt_getprop (
                                       DtNode->DeviceTreeBase,
                                       DtNode->NodeOffset,
                                       "vmmc-supply",
                                       NULL
                                       );
  if (NULL != RegulatorPointer) {
    SdmmcParameterInfo->VmmcRegulatorIdPresent = TRUE;
    SdmmcParameterInfo->VmmcRegulatorId        = SwapBytes32 (*RegulatorPointer);
  }

  RegulatorPointer = (CONST UINT32 *)fdt_getprop (
                                       DtNode->DeviceTreeBase,
                                       DtNode->NodeOffset,
                                       "vqmmc-supply",
                                       NULL
                                       );
  if (NULL != RegulatorPointer) {
    SdmmcParameterInfo->VqmmcRegulatorIdPresent = TRUE;
    SdmmcParameterInfo->VqmmcRegulatorId        = SwapBytes32 (*RegulatorPointer);
  }

  if (fdt_get_property (DtNode->DeviceTreeBase, DtNode->NodeOffset, "non-removable", NULL) != NULL) {
    SdmmcParameterInfo->NonRemovable = TRUE;
  }

  if (fdt_getprop (DtNode->DeviceTreeBase, DtNode->NodeOffset, "only1v8", NULL) != NULL) {
    SdmmcParameterInfo->Only1v8 = TRUE;
  }

  return EFI_SUCCESS;
}
