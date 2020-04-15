//
//  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/TegraPlatformInfoPrivateLib.h>

ASM_FUNC(TegraGetGicDistributorBaseAddressPrivate)

  MOV64 (x0, 0)
  ret

ASM_FUNC(TegraGetGicRedistributorBaseAddressPrivate)

  MOV64 (x0, 0)
  ret
