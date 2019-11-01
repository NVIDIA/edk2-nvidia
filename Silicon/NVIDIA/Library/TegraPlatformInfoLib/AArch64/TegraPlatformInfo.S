//
//  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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
#include "TegraPlatformInfoLibPrivate.h"

ASM_FUNC(TegraGetChipID)

  MOV64 (x9, FixedPcdGet64(PcdMiscRegBaseAddress))
  add x9, x9, #HIDREV_OFFSET
  ldr w0, [x9]
  lsr w0, w0, #HIDREV_CHIPID_SHIFT
  and w0, w0, #HIDREV_CHIPID_MASK
  ret

ASM_FUNC(TegraGetBLCarveoutOffset)

  cmp w0, #TEGRA186
  bne _DefaultCarveout
  MOV64 (x0, T186_BL_CARVEOUT_OFFSET)
  b _ReturnCarveout

_DefaultCarveout:
  MOV64 (x0, DEFAULT_BL_CARVEOUT_OFFSET)

_ReturnCarveout:
  ret

ASM_FUNC(TegraGetBLInfoLocationAddress)

  cmp w0, #TEGRA186
  bne _DefaultBLInfo
  MOV64 (x0, T186_BLINFO_LOCATION_ADDRESS)
  b _ReturnBLInfo

_DefaultBLInfo:
  MOV64 (x0, DEFAULT_BLINFO_LOCATION_ADDRESS)

_ReturnBLInfo:
  ret
