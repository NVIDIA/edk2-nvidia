/** @file
  GOP Driver specific Query and Response functions

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <libfdt.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <PlatformToDriverStructures.h>

#include "GopDriverConfigurationData.h"

EFI_STATUS
EFIAPI
ResponseGopParameters (
  IN CONST  VOID                               *ParameterBlock,
  IN CONST  EFI_PLATFORM_CONFIGURATION_ACTION  ConfigurationAction
  )
{
  GOP_PARAMETER_INFO  *GopParameterBlock;

  if (ParameterBlock == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  GopParameterBlock = (GOP_PARAMETER_INFO *)ParameterBlock;

  switch (ConfigurationAction) {
    case EfiPlatformConfigurationActionNone:
    case EfiPlatformConfigurationActionUnsupportedGuid:
      if (GopParameterBlock->DcbImage != NULL) {
        FreePool (GopParameterBlock->DcbImage);
      }

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
QueryGopParameters (
  IN OUT   VOID                                    **ParameterBlock,
  IN OUT   UINTN                                   *ParameterBlockSize,
  IN       CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DtNode
  )
{
  GOP_PARAMETER_INFO  *GopParameterInfo;
  UINT8               *DcbImage;
  INT32               DcbImageLen;

  if ((ParameterBlock == NULL) ||
      (ParameterBlockSize == NULL) ||
      (DtNode == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  GopParameterInfo = (GOP_PARAMETER_INFO *)AllocateZeroPool (sizeof (GOP_PARAMETER_INFO));
  if (GopParameterInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Obtaining display info from the device tree
  DcbImage = (UINT8 *)fdt_getprop (
                        DtNode->DeviceTreeBase,
                        DtNode->NodeOffset,
                        "nvidia,dcb-image",
                        &DcbImageLen
                        );

  if ((DcbImage != NULL) &&
      (DcbImageLen > 0))
  {
    GopParameterInfo->DcbImage = AllocateZeroPool (DcbImageLen);
    if (GopParameterInfo->DcbImage == NULL) {
      FreePool (GopParameterInfo);
      return EFI_OUT_OF_RESOURCES;
    }

    // Copy contents of DCB image
    CopyMem (GopParameterInfo->DcbImage, DcbImage, DcbImageLen);
    GopParameterInfo->DcbImageLen = (UINT32)DcbImageLen;
  }

  *ParameterBlock     = (VOID *)GopParameterInfo;
  *ParameterBlockSize = sizeof (GOP_PARAMETER_INFO);

  return EFI_SUCCESS;
}
