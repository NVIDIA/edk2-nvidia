/** @file

  NVG Library

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVG_LIB__
#define __NVG_LIB__

#include <Uefi/UefiBaseType.h>

/**
  Returns number of CPU cores enabled on the system

  @return       UINT32          Number of CPU cores

**/
UINT32
EFIAPI
NvgGetNumberOfEnabledCpuCores (
  VOID
  );

/**
  Returns the Mpidr for a specified logical CPU

  @param[in]    LogicalCore     Logical CPU core ID
  @param[out]   Mpidr           Mpidr of the logical CPU

  @return       EFI_SUCCESS     Logical CPU enabled and Mpidr returned
  @return       EFI_NOT_FOUND   Logical CPU not enabled

**/
EFI_STATUS
EFIAPI
NvgConvertCpuLogicalToMpidr (
  IN  UINT32  LogicalCore,
  OUT UINT64  *Mpidr
  );

/**
  Fills in bit map of enabled cores

**/
EFI_STATUS
EFIAPI
NvgGetEnabledCoresBitMap (
  IN  UINT64  *EnabledCoresBitMap
  );

#endif
