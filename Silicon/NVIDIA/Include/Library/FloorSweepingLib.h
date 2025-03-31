/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __FLOOR_SWEEPING_LIB_H__
#define __FLOOR_SWEEPING_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Check if given socket is enabled

**/
BOOLEAN
EFIAPI
IsSocketEnabled (
  IN UINT32  SocketIndex
  );

/**
  Get the first enabled socket

  @retval  First enabled socket
**/
UINT32
EFIAPI
GetFirstEnabledSocket (
  VOID
  );

/**
  Get the next enabled socket

  @param[in, out]  SocketId  Socket index. On input the last socket id, on output the next enabled socket id
                             if error is returned, SocketId is set to MAX_UINT32

  @retval  EFI_SUCCESS - Socket found
  @retval  EFI_NOT_FOUND - No more sockets
**/
EFI_STATUS
EFIAPI
GetNextEnabledSocket (
  IN OUT UINT32  *SocketId
  );

// Used to iterate over all enabled sockets, this looks for sockets that are enabled but might not have CPU cores
// Use MPCORE_FOR_EACH_ENABLED_SOCKET instead if looking for CPU cores
#define FOR_EACH_ENABLED_SOCKET(SocketId) \
  for (SocketId = GetFirstEnabledSocket(); \
       SocketId != MAX_UINT32; \
       GetNextEnabledSocket(&SocketId); \
       )

/**
  Floorsweep DTB

**/
EFI_STATUS
EFIAPI
FloorSweepDtb (
  IN VOID  *Dtb
  );

#endif //__FLOOR_SWEEPING_LIB_H__
