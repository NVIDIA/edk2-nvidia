/** @file
*
*  Provides empty versions of the IPMI functions to allow this library to work
*  without the IPMI libraries.
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
