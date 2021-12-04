//
//  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
//
//  This program and the accompanying materials
//  are licensed and made available under the terms and conditions of the BSD License
//  which accompanies this distribution.  The full text of the license may be found at
//  http://opensource.org/licenses/bsd-license.php
//
//  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
//  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
//
// Portions provided under the following terms:
// Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
// property and proprietary rights in and to this material, related
// documentation and any modifications thereto. Any use, reproduction,
// disclosure or distribution of this material and related documentation
// without an express license agreement from NVIDIA CORPORATION or
// its affiliates is strictly prohibited.
//
// SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES
// SPDX-License-Identifier: LicenseRef-NvidiaProprietary
//

#include <AsmMacroIoLibV8.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/TegraPlatformInfoInternalLib.h>
#include <T194/T194Definitions.h>
#include <T234/T234Definitions.h>
#include "TegraPlatformInfoLibPrivate.h"

ASM_FUNC(TegraGetChipID)
  MOV64 (x9, FixedPcdGet64(PcdMiscRegBaseAddress))
  add x9, x9, #HIDREV_OFFSET
  ldr w0, [x9]
  lsr w0, w0, #HIDREV_CHIPID_SHIFT
  and w0, w0, #HIDREV_CHIPID_MASK
  ret

ASM_FUNC(TegraGetSystemMemoryBaseAddress)
  MOV64(x0, TEGRA_SYSTEM_MEMORY_BASE)
  ret

ASM_FUNC(TegraGetBLCarveoutOffset)
  mov w1, w0
  mov x4, x30
  bl TegraGetBLCarveoutOffsetInternal
  cmp x0, #0
  beq _T194Carveout
  b _ReturnCarveout

_T194Carveout:
  cmp w1, #T194_CHIP_ID
  bne _T234Carveout
  MOV64 (x0, T194_BL_CARVEOUT_OFFSET)
  b _ReturnCarveout

_T234Carveout:
  cmp w1, #T234_CHIP_ID
  bne _DefaultCarveout
  MOV64 (x0, T234_BL_CARVEOUT_OFFSET)
  b _ReturnCarveout

_DefaultCarveout:
  MOV64 (x0, 0)

_ReturnCarveout:
  mov x30, x4
  ret

ASM_FUNC(TegraGetBLInfoLocationAddress)
  cmp w0, #T194_CHIP_ID
  bne _DefaultBLInfoAddr
  MOV64 (x0, T194_BLINFO_LOCATION_ADDRESS)
  ret

_DefaultBLInfoAddr:
  MOV64 (x0, DEFAULT_BLINFO_LOCATION_ADDRESS)

_ReturnBLInfoAddr:
  ret

ASM_FUNC(TegraGetBLCarveoutInfoLocationAddress)
  mov w1, w0
  mov x2, x30
  mov w0, w1
  bl TegraGetBLInfoLocationAddress
  mov x3, x0
  mov w0, w1
  bl TegraGetBLCarveoutOffset
  mov x4, x0
  cmp w1, #T234_CHIP_ID
  bne _DefaultCarveoutInfo
  add x5, x3, #4
  ldr w5, [x5]
  lsl x5, x5, #32
  ldr w3, [x3]
  orr x3, x3, x5
  b _BootloaderAddressGood

_DefaultCarveoutInfo:
  mov w0, w1
  bl TegraGetSystemMemoryBaseAddress
  mov x5, x0
  ldr w3, [x3]
  cmp x3, x5
  b.ge _BootloaderAddressGood
  lsl x3, x3, #16

_BootloaderAddressGood:
  add x0, x3, x4
  b _ReturnCarveoutInfo

_ReturnCarveoutInfo:
  mov x30, x2
  ret

ASM_FUNC(TegraGetGicDistributorBaseAddress)
  mov w1, w0
  mov x2, x30
  bl TegraGetGicDistributorBaseAddressInternal
  cmp x0, #0
  beq _T194GicDAddr
  b _ReturnGicDAddr

_T194GicDAddr:
  cmp w1, #T194_CHIP_ID
  bne _T234GicDAddr
  MOV64 (x0, T194_GIC_DISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_T234GicDAddr:
  cmp w1, #T234_CHIP_ID
  bne _DefaultGicDAddr
  MOV64 (x0, T234_GIC_DISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_DefaultGicDAddr:
  MOV64 (x0, 0)

_ReturnGicDAddr:
  mov x30, x2
  ret

ASM_FUNC(TegraGetGicRedistributorBaseAddress)
  mov w1, w0
  mov x2, x30
  bl TegraGetGicRedistributorBaseAddressInternal
  cmp x0, #0
  beq _T234GicRAddr
  b _ReturnGicRAddr

_T234GicRAddr:
  cmp w1, #T234_CHIP_ID
  bne _DefaultGicRAddr
  MOV64 (x0, T234_GIC_REDISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_DefaultGicRAddr:
  MOV64 (x0, 0)

_ReturnGicRAddr:
  mov x30, x2
  ret

ASM_FUNC(TegraGetGicInterruptInterfaceBaseAddress)
  cmp w0, #T194_CHIP_ID
  bne _DefaultGicIAddr
  MOV64 (x0, T194_GIC_INTERRUPT_INTERFACE_BASE)
  b _ReturnGicIAddr

_DefaultGicIAddr:
  MOV64 (x0, 0)

_ReturnGicIAddr:
  ret
