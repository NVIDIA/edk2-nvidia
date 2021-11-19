/** @file
This module is a helper function to get the CPU Freq

Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <AsmMacroIoLibV8.h>
#include <Library/ArmLib.h>
#include <AsmMacroIoLibV8.h>
#include <Library/TegraCpuFreqHelper.h>

ASM_FUNC(NvReadPmCntr)
  mrs  x0, s3_0_c15_c0_5
  ret

