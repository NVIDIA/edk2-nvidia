/** @file

  TegraP2U Driver private structures

  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TEGRAP2U_DXE_PRIVATE_H__
#define __TEGRAP2U_DXE_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/TegraP2U.h>

#define TEGRAP2U_LIST_SIGNATURE  SIGNATURE_32('P','2','U','L')
typedef struct {
  UINT32        Signature;
  LIST_ENTRY    Link;
  UINT32        P2UId;
  UINT64        BaseAddr;
  BOOLEAN       SkipSizeProtectionEn;
  LIST_ENTRY    NotifyList;
} TEGRAP2U_LIST_ENTRY;
#define TEGRAP2U_LIST_FROM_LINK(a)  CR(a, TEGRAP2U_LIST_ENTRY, Link, TEGRAP2U_LIST_SIGNATURE)

#define TEGRAP2U_SIGNATURE  SIGNATURE_32('P','2','U','D')
typedef struct {
  //
  // Standard signature used to identify Tegra P2U private data
  //
  UINT32                      Signature;

  NVIDIA_TEGRAP2U_PROTOCOL    TegraP2UProtocol;

  EFI_HANDLE                  ImageHandle;

  VOID                        *DeviceTreeBase;
  UINTN                       DeviceTreeSize;

  LIST_ENTRY                  TegraP2UList;
  UINTN                       TegraP2Us;
} TEGRAP2U_DXE_PRIVATE;
#define TEGRAP2U_PRIVATE_DATA_FROM_THIS(a)  CR(a, TEGRAP2U_DXE_PRIVATE, TegraP2UProtocol, TEGRAP2U_SIGNATURE)

#endif
