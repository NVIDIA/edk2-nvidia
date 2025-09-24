/** @file

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ArmFfaLib.h>
#include <Library/ArmSmcLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/StMmDxeGetBuffersLib.h>
#include <Library/PcdLib.h>

/**
  Constructor for the Standalone MM FFA V1.2 Library
  This library will get the NS Buffer Details from the Standalone MM.


  @retval EFI_SUCCESS   Always return success,
**/
EFI_STATUS
EFIAPI
StandaloneMmFfav12LibConstructor (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT64      BufferBase;
  UINT32      BufferSize;

  Status = StMmDxeGetBuffer (GET_MM_NS_BUFFER_FID, &BufferBase, &BufferSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get MM Communication NS Buffer Details. %r\n", __FUNCTION__, Status));
    goto ExitStandaloneMmFfav12LibConstructor;
  }

  Status = PcdSet64S (PcdMmBufferBase, BufferBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set MM Communication NS Buffer Base. %r\n", __FUNCTION__, Status));
    goto ExitStandaloneMmFfav12LibConstructor;
  }

  Status = PcdSet64S (PcdMmBufferSize, BufferSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set MM Communication NS Buffer Size. %r\n", __FUNCTION__, Status));
    goto ExitStandaloneMmFfav12LibConstructor;
  }

ExitStandaloneMmFfav12LibConstructor:
  return EFI_SUCCESS;
}
