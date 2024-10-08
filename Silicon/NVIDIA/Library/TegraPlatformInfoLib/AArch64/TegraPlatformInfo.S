//
//  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
//  SPDX-License-Identifier: BSD-2-Clause-Patent
//

#include <AsmMacroIoLibV8.h>
#include <Library/TegraPlatformInfoLib.h>
#include <T194/T194Definitions.h>
#include <T234/T234Definitions.h>
#include <TH500/TH500Definitions.h>
#include "TegraPlatformInfoLibPrivate.h"

ASM_FUNC(TegraGetChipID)
  MOV64 (x0, SMCCC_ARCH_SOC_ID)
  MOV64 (x1, SMCCC_ARCH_SOC_ID_GET_SOC_VERSION)
  smc #0
  MOV64 (x1, T194_SOC_ID_VERSION_ALT)
  cmp w0, w1
  b.eq _T194ChipID
  lsr w0, w0, #SOC_ID_VERSION_CHIPID_SHIFT
  and w0, w0, #SOC_ID_VERSION_CHIPID_MASK
  ret
_T194ChipID:
  MOV64 (x0, T194_CHIP_ID)
  ret

ASM_FUNC(TegraGetSystemMemoryBaseAddress)
  MOV64(x0, TEGRA_SYSTEM_MEMORY_BASE)
  ret

ASM_FUNC(TegraGetBLCarveoutOffset)
  mov w1, w0
  mov x4, x30

_T194Carveout:
  cmp w1, #T194_CHIP_ID
  bne _T234Carveout
  MOV64 (x0, T194_BL_CARVEOUT_OFFSET)
  b _ReturnCarveout

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
  cmp w1, #T194_CHIP_ID
  beq _T194CarveoutInfo
  add x5, x3, #4
  ldr w5, [x5]
  lsl x5, x5, #32
  ldr w3, [x3]
  orr x3, x3, x5
  b _BootloaderAddressGood

_T194CarveoutInfo:
  mov w0, w1
  bl TegraGetSystemMemoryBaseAddress
  mov x5, x0
  ldr w3, [x3]
  cmp x3, x5
  b.ge _BootloaderAddressGood
  lsl x3, x3, #16

_BootloaderAddressGood:
  mov w0, w1
  bl TegraGetBLCarveoutOffset
  mov x4, x0
  add x0, x3, x4
  b _ReturnCarveoutInfo

_ReturnCarveoutInfo:
  mov x30, x2
  ret

ASM_FUNC(TegraGetGicDistributorBaseAddress)
  mov w1, w0
  mov x2, x30

_T194GicDAddr:
  cmp w1, #T194_CHIP_ID
  bne _T234GicDAddr
  MOV64 (x0, T194_GIC_DISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_T234GicDAddr:
  cmp w1, #T234_CHIP_ID
  bne _TH500GicDAddr
  MOV64 (x0, T234_GIC_DISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_TH500GicDAddr:
  cmp w1, #TH500_CHIP_ID
  bne _DefaultGicDAddr
  MOV64 (x0, TH500_GIC_DISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_DefaultGicDAddr:
  MOV64 (x0, 0)

_ReturnGicDAddr:
  mov x30, x2
  ret

ASM_FUNC(TegraGetGicRedistributorBaseAddress)
  mov w1, w0
  mov x2, x30

_T234GicRAddr:
  cmp w1, #T234_CHIP_ID
  bne _TH500GicRAddr
  MOV64 (x0, T234_GIC_REDISTRIBUTOR_BASE)
  b _ReturnGicDAddr

_TH500GicRAddr:
  cmp w1, #TH500_CHIP_ID
  bne _DefaultGicRAddr
  MOV64 (x0, TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_0)
  b _ReturnGicRAddr

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
