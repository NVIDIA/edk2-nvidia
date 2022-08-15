/** @file
This module is a helper function to get the CPU Freq

Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <AsmMacroIoLibV8.h>
#include <Library/ArmLib.h>
#include <AsmMacroIoLibV8.h>

ASM_FUNC(GetT194PmCntr)
  mrs  x0, s3_0_c15_c0_5
  ret

ASM_FUNC(SetT194CpuNdiv)
  msr s3_0_c15_c0_4, x0
  ret

ASM_FUNC(GetT194CpuNdiv)
  mrs x0, s3_0_c15_c0_4
  ret

