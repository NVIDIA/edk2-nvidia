/** @file

  VerPartitionLib - VER partition library

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __VER_PARTITION_LIB_H__
#define __VER_PARTITION_LIB_H__

#include <Uefi/UefiBaseType.h>

#define VER_PARTITION_NAME  L"VER"

/**
  Return version info from VER partition data.

  @param[in]      Data              Pointer to VER partition data
  @param[in]      DataLen           Bytes of partition data buffer
  @param[out]     Version           UINT32 version number
  @param[out]     VersionString     Version string, caller must free with FreePool()

  @retval         EFI_SUCCESS       Operation successful
  @retval         Others            An error occurred

**/
EFI_STATUS
EFIAPI
VerPartitionGetVersion (
  IN  VOID    *Data,
  IN  UINTN   DataLen,
  OUT UINT32  *Version,
  OUT CHAR8   **VersionString
  );

#endif
