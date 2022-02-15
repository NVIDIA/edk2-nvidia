//
//  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
//  SPDX-License-Identifier: BSD-2-Clause-Patent
//

#include <AsmMacroIoLibV8.h>
#include <Library/TegraPlatformInfoInternalLib.h>

ASM_FUNC(TegraGetBLCarveoutOffsetInternal)

  MOV64 (x0, 0)
  ret

ASM_FUNC(TegraGetGicDistributorBaseAddressInternal)

  MOV64 (x0, 0)
  ret

ASM_FUNC(TegraGetGicRedistributorBaseAddressInternal)

  MOV64 (x0, 0)
  ret
