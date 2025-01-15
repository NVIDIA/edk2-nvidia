/** @file
This module is a helper function to get the CPU Freq

SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <AsmMacroLib.h>
#include <Library/ArmLib.h>
#include <AsmMacroLib.h>
#include <Library/TegraCpuFreqHelper.h>

ASM_FUNC(NvReadPmCntr)
  mrs  x0, s3_0_c15_c0_5
  ret

