//
//  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
//  SPDX-License-Identifier: BSD-2-Clause-Patent
//

#include <AsmMacroIoLibV8.h>
#include <Library/TegraPlatformInfoLib.h>
#include <T234/T234Definitions.h>
#include <TH500/TH500Definitions.h>
#include "TegraPlatformInfoLibPrivate.h"

ASM_FUNC(TegraGetChipID)
  MOV64 (x0, SMCCC_ARCH_SOC_ID)
  MOV64 (x1, SMCCC_ARCH_SOC_ID_GET_SOC_VERSION)
  smc #0
  lsr w0, w0, #SOC_ID_VERSION_CHIPID_SHIFT
  and w0, w0, #SOC_ID_VERSION_CHIPID_MASK
  ret

ASM_FUNC(TegraGetSystemMemoryBaseAddress)
  MOV64(x0, TEGRA_SYSTEM_MEMORY_BASE)
  ret

ASM_FUNC(TegraGetBLCarveoutOffset)
  mov w1, w0
  mov x4, x30

_T234Carveout:
  cmp w1, #T234_CHIP_ID
  bne _TH500Carveout
  MOV64 (x0, T234_BL_VERSION_OFFSET)
  add x0, x0, x3
  ldr w0, [x0]
  cmp w0, 0
  beq _T234Carveout_V0
  cmp w0, 1
  beq _T234Carveout_V1
  cmp w0, 2
  bne _DefaultCarveout
  MOV64 (x0, T234_BL_CARVEOUT_OFFSET_V2)
  b _ReturnCarveout

_T234Carveout_V1:
  MOV64 (x0, T234_BL_CARVEOUT_OFFSET_V1)
  b _ReturnCarveout

_T234Carveout_V0:
  MOV64 (x0, T234_BL_CARVEOUT_OFFSET_V0)
  b _ReturnCarveout

_TH500Carveout:
  cmp w1, #TH500_CHIP_ID
  bne _DefaultCarveout
  MOV64 (x0, TH500_BL_CARVEOUT_OFFSET)
  b _ReturnCarveout

_DefaultCarveout:
  MOV64 (x0, 0)

_ReturnCarveout:
  mov x30, x4
  ret

ASM_FUNC(TegraGetBLInfoLocationAddress)
  MOV64 (x0, DEFAULT_BLINFO_LOCATION_ADDRESS)
  ret

ASM_FUNC(TegraGetBLCarveoutInfoLocationAddress)
  mov w1, w0
  mov x2, x30
  mov w0, w1
  bl TegraGetBLInfoLocationAddress
  mov x3, x0
  add x5, x3, #4
  ldr w5, [x5]
  lsl x5, x5, #32
  ldr w3, [x3]
  orr x3, x3, x5
  mov w0, w1
  bl TegraGetBLCarveoutOffset
  mov x4, x0
  add x0, x3, x4
  b _ReturnCarveoutInfo

_ReturnCarveoutInfo:
  mov x30, x2
  ret
