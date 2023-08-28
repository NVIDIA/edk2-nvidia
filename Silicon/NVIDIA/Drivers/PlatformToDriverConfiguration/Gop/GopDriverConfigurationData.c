/** @file
  GOP Driver specific Query and Response functions

  SPDX-FileCopyrightText: Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
  if (ParameterBlock == NULL) {
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
      DEBUG ((
        DEBUG_ERROR,
        "%a: unhandled configuration action: %lu\r\n",
        __FUNCTION__,
        (UINT64)ConfigurationAction
        ));
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

  // Obtaining display info from the device tree
  DcbImage = (UINT8 *)fdt_getprop (
                        DtNode->DeviceTreeBase,
                        DtNode->NodeOffset,
                        "nvidia,dcb-image",
                        &DcbImageLen
                        );
  if (DcbImage == NULL) {
    if (DcbImageLen != -FDT_ERR_NOTFOUND) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to retrieve property 'nvidia,dcb-image': %a\r\n",
        __FUNCTION__,
        fdt_strerror (DcbImageLen)
        ));
    }

    DcbImageLen = 0;
  }

  GopParameterInfo = (GOP_PARAMETER_INFO *)AllocateZeroPool (sizeof (*GopParameterInfo) + DcbImageLen);
  if (GopParameterInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (DcbImageLen > 0) {
    GopParameterInfo->DcbImage    = (UINT8 *)(GopParameterInfo + 1);
    GopParameterInfo->DcbImageLen = DcbImageLen;

    // Copy contents of DCB image
    CopyMem (GopParameterInfo->DcbImage, DcbImage, DcbImageLen);
  }

  *ParameterBlock     = (VOID *)GopParameterInfo;
  *ParameterBlockSize = sizeof (*GopParameterInfo);

  return EFI_SUCCESS;
}
