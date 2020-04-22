//
//  Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
//
//  This program and the accompanying materials
//  are licensed and made available under the terms and conditions of the BSD License
//  which accompanies this distribution.  The full text of the license may be found at
//  http://opensource.org/licenses/bsd-license.php
//
//  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
//  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
//
//

#include <AsmMacroIoLibV8.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/TegraPlatformInfoInternalLib.h>
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

  cmp w0, #T186_CHIP_ID
  bne _DefaultCarveout
  MOV64 (x0, T186_BL_CARVEOUT_OFFSET)
  b _ReturnCarveout

_DefaultCarveout:
  MOV64 (x0, DEFAULT_BL_CARVEOUT_OFFSET)

_ReturnCarveout:
  ret

ASM_FUNC(TegraGetBLInfoLocationAddress)

  cmp w0, #T186_CHIP_ID
  bne _DefaultBLInfo
  MOV64 (x0, T186_BLINFO_LOCATION_ADDRESS)
  b _ReturnBLInfo

_DefaultBLInfo:
  MOV64 (x0, DEFAULT_BLINFO_LOCATION_ADDRESS)

_ReturnBLInfo:
  ret

ASM_FUNC(TegraGetGicDistributorBaseAddress)
  mov w1, w0
  mov x2, x30
  bl TegraGetGicDistributorBaseAddressInternal
  mov x30, x2
  cmp x0, #0
  beq _T194GicDAddr
  b _ReturnGicDAddr

_T194GicDAddr:
  cmp w1, #T194_CHIP_ID
  bne _T186GicDAddr
  MOV64 (x0, T194_GIC_DISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_T186GicDAddr:
  cmp w1, #T186_CHIP_ID
  bne _DefaultGicDAddr
  MOV64 (x0, T186_GIC_DISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_DefaultGicDAddr:
  MOV64 (x0, 0)

_ReturnGicDAddr:
  ret

ASM_FUNC(TegraGetGicRedistributorBaseAddress)
  mov x1, x30
  bl TegraGetGicRedistributorBaseAddressInternal
  mov x30, x1
  b _ReturnGicRAddr

_ReturnGicRAddr:
  ret

ASM_FUNC(TegraGetGicInterruptInterfaceBaseAddress)

  cmp w0, #T186_CHIP_ID
  bne _T194GicIAddr
  MOV64 (x0, T186_GIC_INTERRUPT_INTERFACE_BASE)
  b _ReturnGicIAddr

_T194GicIAddr:
  cmp w0, #T194_CHIP_ID
  bne _DefaultGicIAddr
  MOV64 (x0, T194_GIC_INTERRUPT_INTERFACE_BASE)
  b _ReturnGicIAddr

_DefaultGicIAddr:
  MOV64 (x0, 0)

_ReturnGicIAddr:
  ret
