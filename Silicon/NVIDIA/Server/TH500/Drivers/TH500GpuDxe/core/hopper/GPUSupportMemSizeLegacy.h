/** @file

  NVIDIA GPU support structures and prototypes.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

///
/// GPUSupportMemSizeLegacy.h: GPU Memory Size Support routine.
///

#ifndef __HOPPER_GPU_SUPPORT_MEM_SIZE_LEGACY_H__
#define __HOPPER_GPU_SUPPORT_MEM_SIZE_LEGACY_H__

#include <Uefi/UefiBaseType.h>

/** Returns the Memory Size for the GPU

  @param[in] ControllerHandle - Controller Handle to obtain the GPU Memory Information from

  @retval UINT64 containing the GPU MemSize
*/
UINT64
EFIAPI
GetGPUMemSizeSupportLegacy (
  IN EFI_HANDLE  ControllerHandle
  );

#endif // __HOPPER_GPU_SUPPORT_MEM_SIZE_LEGACY_H__
