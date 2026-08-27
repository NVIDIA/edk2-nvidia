/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _PLATFORM_BOOT_ORDER_IPMI_LIB_H_
#define _PLATFORM_BOOT_ORDER_IPMI_LIB_H_

VOID
EFIAPI
CheckIPMIForBootOrderUpdates (
  VOID
  );

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
