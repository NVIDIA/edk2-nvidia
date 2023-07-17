/** @file

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/PcdLib.h>

/**
  Return the base address of the PrePi image.

  @param  FaultAddress         Ignored.
  @param  ImageBase            PcdFvBaseAddress.
**/
VOID
GetImageBase (
  OUT UINTN  *ImageBase
  )
{
  *ImageBase = PatchPcdGet64 (PcdFvBaseAddress);
}
