/** @file

  TegraP2U Driver private structures

  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __TEGRAP2U_DXE_PRIVATE_H__
#define __TEGRAP2U_DXE_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/TegraP2U.h>

#define TEGRAP2U_LIST_SIGNATURE SIGNATURE_32('P','2','U','L')
typedef struct {
  UINT32                     Signature;
  LIST_ENTRY                 Link;
  UINT32                     P2UId;
  UINT32                     BaseAddr;
  LIST_ENTRY                 NotifyList;
} TEGRAP2U_LIST_ENTRY;
#define TEGRAP2U_LIST_FROM_LINK(a) CR(a, TEGRAP2U_LIST_ENTRY, Link, TEGRAP2U_LIST_SIGNATURE)


#define TEGRAP2U_SIGNATURE SIGNATURE_32('P','2','U','D')
typedef struct {
  //
  // Standard signature used to identify Tegra P2U private data
  //
  UINT32                     Signature;

  NVIDIA_TEGRAP2U_PROTOCOL   TegraP2UProtocol;

  EFI_HANDLE                 ImageHandle;

  VOID                       *DeviceTreeBase;
  UINTN                      DeviceTreeSize;

  LIST_ENTRY                 TegraP2UList;
  UINTN                      TegraP2Us;

} TEGRAP2U_DXE_PRIVATE;
#define TEGRAP2U_PRIVATE_DATA_FROM_THIS(a) CR(a, TEGRAP2U_DXE_PRIVATE, TegraP2UProtocol, TEGRAP2U_SIGNATURE)


#endif
