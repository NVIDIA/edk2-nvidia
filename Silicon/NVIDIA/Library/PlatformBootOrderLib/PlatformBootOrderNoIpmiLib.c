/** @file
*
*  Provides empty versions of the IPMI functions to allow this library to work
*  without the IPMI libraries.
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Library/PlatformBootOrderIpmiLib.h>

VOID
EFIAPI
CheckIPMIForBootOrderUpdates (
  VOID
  )
{
}

/**
  Reports whether the cached IPMI Boot Flags request is persistent.

  CheckIPMIForBootOrderUpdates () must be called before this function. The
  cached request must not have been consumed by ProcessIPMIBootOrderUpdates ().

  @retval TRUE   A valid cached request has PersistentOptions set.
  @retval FALSE  The cached request is absent, invalid, or non-persistent.
**/
BOOLEAN
EFIAPI
IsIPMIBootOrderUpdatePersistent (
  VOID
  )
{
  return FALSE;
}

VOID
EFIAPI
RestoreBootOrder (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
}

VOID
EFIAPI
ProcessIPMIBootOrderUpdates (
  VOID
  )
{
}
