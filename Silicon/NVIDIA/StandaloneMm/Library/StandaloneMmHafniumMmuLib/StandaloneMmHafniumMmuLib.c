/** @file
*
*  StandaloneMmuLib to be used when StMM is in S-EL1.
*  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "Uefi/UefiBaseType.h"
#include "Uefi/UefiSpec.h"
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/StandaloneMmHafniumMmuLib.h>
#include <Library/MmServicesTableLib.h>

EFI_STATUS
ArmSetMemoryRegionNoExec (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length
  )
{
  if (gMmst != NULL) {
    return StMmSetMemoryAttributes (BaseAddress, Length, EFI_MEMORY_XP, EFI_MEMORY_XP);
  } else {
    DEBUG ((DEBUG_ERROR, "%a:Skip this in pre-mmcore\n", __FUNCTION__));
    return EFI_SUCCESS;
  }
}

EFI_STATUS
ArmClearMemoryRegionNoExec (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length
  )
{
  if (gMmst != NULL) {
    return StMmSetMemoryAttributes (BaseAddress, Length, 0, EFI_MEMORY_XP);
  } else {
    DEBUG ((DEBUG_ERROR, "%a:Skip this in pre-mmcore\n", __FUNCTION__));
    return EFI_SUCCESS;
  }
}

EFI_STATUS
ArmSetMemoryRegionReadOnly (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length
  )
{
  if (gMmst != NULL) {
    return StMmSetMemoryAttributes (BaseAddress, Length, EFI_MEMORY_RO, EFI_MEMORY_RO);
  } else {
    DEBUG ((DEBUG_ERROR, "%a:Skip this in pre-mmcore\n", __FUNCTION__));
    return EFI_SUCCESS;
  }
}

EFI_STATUS
ArmClearMemoryRegionReadOnly (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length
  )
{
  if (gMmst != NULL) {
    return StMmSetMemoryAttributes (BaseAddress, Length, 0, EFI_MEMORY_RO);
  } else {
    DEBUG ((DEBUG_ERROR, "%a:Skip this in pre-mmcore\n", __FUNCTION__));
    return EFI_SUCCESS;
  }
}
