/** @file

SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef STMM_DXE_GET_BUFFERS_LIB_H
#define STMM_DXE_GET_BUFFERS_LIB_H

#include <Uefi.h>

/**
  Get the address and size of a shared buffer from StandaloneMm.

  @param[in] Fid            FID of the buffer to get
  @param[out] BufferBase    Base address of the buffer
  @param[out] BufferSize    Size of the buffer

  @retval EFI_SUCCESS   The constructor completed successfully
  @retval Other           Error.
**/
EFI_STATUS
EFIAPI
StMmDxeGetBuffer (
  IN UINT32   Fid,
  OUT UINT64  *BufferBase,
  OUT UINT32  *BufferSize
  );

#endif //STMM_DXE_GET_BUFFERS_LIB_H
