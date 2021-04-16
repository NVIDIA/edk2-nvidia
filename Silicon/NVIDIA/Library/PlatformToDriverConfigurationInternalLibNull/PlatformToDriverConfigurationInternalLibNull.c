/** @file
*
*  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <PiDxe.h>

#include <Library/PlatformToDriverConfiguration.h>

GUID_DEVICEFUNCPTR_MAPPING InternalGuidDeviceFuncPtrMap[] = {
  { NULL, NULL, NULL }
};

/**
  Retrieve Internal Guid Device Funtion Map

**/
GUID_DEVICEFUNCPTR_MAPPING*
EFIAPI
GetInternalGuidMap (
  VOID
  )
{
  return InternalGuidDeviceFuncPtrMap;
}
