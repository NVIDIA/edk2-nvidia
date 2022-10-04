/** @file
*
*  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Library/PlatformToDriverConfiguration.h>

GUID_DEVICEFUNCPTR_MAPPING  InternalGuidDeviceFuncPtrMap[] = {
  { NULL, NULL, NULL }
};

/**
  Retrieve Internal Guid Device Funtion Map

**/
GUID_DEVICEFUNCPTR_MAPPING *
EFIAPI
GetInternalGuidMap (
  VOID
  )
{
  return InternalGuidDeviceFuncPtrMap;
}
