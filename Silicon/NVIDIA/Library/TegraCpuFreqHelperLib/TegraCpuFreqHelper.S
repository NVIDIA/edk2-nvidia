/** @file
This module is a helper function to get the CPU Freq

Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <AsmMacroIoLibV8.h>
#include <Library/ArmLib.h>
#include <AsmMacroIoLibV8.h>
#include <Library/TegraCpuFreqHelper.h>

ASM_FUNC(NvReadPmCntr)
  mrs  x0, s3_0_c15_c0_5
  ret

