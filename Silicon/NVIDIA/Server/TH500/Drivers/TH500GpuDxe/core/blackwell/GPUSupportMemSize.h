/** @file

  NVIDIA GPU support structures and prototypes.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

///
/// GPUSupportMemSize.h: GPU Memory Size Support routine.
///

#ifndef __BLACKWELL_GPU_SUPPORT_MEM_SIZE_H__
#define __BLACKWELL_GPU_SUPPORT_MEM_SIZE_H__

#include <Uefi/UefiBaseType.h>

/** Returns the Memory Size for the GPU

  @param[in] ControllerHandle - Controller Handle to obtain the GPU Memory Information from

  @retval UINT64 containing the GPU MemSize
*/
UINT64
EFIAPI
GetGPUMemSizeSupport (
  IN EFI_HANDLE  ControllerHandle
  );

#endif // __BLACKWELL_GPU_SUPPORT_MEM_SIZE_H__
