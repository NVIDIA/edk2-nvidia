/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _PLATFORM_BOOT_ORDER_LIB_H_
#define _PLATFORM_BOOT_ORDER_LIB_H_

VOID
EFIAPI
SetBootOrder (
  VOID
  );

VOID
EFIAPI
CheckIPMIForBootOrderUpdates (
  VOID
  );

VOID
EFIAPI
ProcessIPMIBootOrderUpdates (
  VOID
  );

VOID
EFIAPI
RestoreBootOrder (
  EFI_EVENT  Event,
  VOID       *Context
  );

#endif
