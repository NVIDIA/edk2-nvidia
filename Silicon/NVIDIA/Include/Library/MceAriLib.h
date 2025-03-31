/** @file

  MCE ARI library

  SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCE_ARI_LIB__
#define __MCE_ARI_LIB__

#include <Uefi/UefiBaseType.h>

#define MCE_ARI_APERTURE_SIZE  0x10000

// Gives the ARI aperture offset from PcdTegraMceAriApertureBaseAddress for
// a given Linear Core Id, both split and locked modes.
#define MCE_ARI_APERTURE_OFFSET(LinearCoreId)    \
  (MCE_ARI_APERTURE_SIZE * (LinearCoreId))

/**
  Fills in bit map of enabled cores

**/
EFI_STATUS
EFIAPI
MceAriGetEnabledCoresBitMap (
  IN  UINT64  *EnabledCoresBitMap,
  IN  UINT32  MaxPossibleCoresPerCluster
  );

#endif
