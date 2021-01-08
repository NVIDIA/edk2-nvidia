/** @file

  EFUSE Driver private structures

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef EFUSEDXE_PRIVATE_H_
#define EFUSEDXE_PRIVATE_H_

#include <PiDxe.h>
#include <Protocol/EFuse.h>

#define EFUSE_SIGNATURE SIGNATURE_32('E','F','S','E')
typedef struct {
  UINT32                     Signature;
  NVIDIA_EFUSE_PROTOCOL      EFuseProtocol;
  EFI_PHYSICAL_ADDRESS       BaseAddress;
  UINTN                      RegionSize;
  EFI_HANDLE                 ImageHandle;
} EFUSE_DXE_PRIVATE;
#define EFUSE_PRIVATE_DATA_FROM_THIS(a)     CR(a, EFUSE_DXE_PRIVATE, EFuseProtocol, EFUSE_SIGNATURE)
#define EFUSE_PRIVATE_DATA_FROM_PROTOCOL(a) EFUSE_PRIVATE_DATA_FROM_THIS(a)

#endif /* EFUSEDXE_PRIVATE_H_ */
