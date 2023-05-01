/** @file
  A hook-in library for drivers.

  Plugging this library instance into a driver makes it conditional on running
  on a pre-silicon target

  Copyright (C) 2017, Red Hat, Inc.
  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>

RETURN_STATUS
EFIAPI
PlatformIsPresiliconInitialize (
  VOID
  )
{
  //
  // Do nothing, just imbue driver with a protocol dependency on
  // gNVIDIAIsPresiliconDeviceGuid.
  //
  return RETURN_SUCCESS;
}
