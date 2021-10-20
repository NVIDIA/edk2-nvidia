/** @file

  Tegra Platform Info Library.

  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include "TegraPlatformInfoLibPrivate.h"

STATIC
UINT32
TegraReadHidrevReg (
  VOID
  )
{
  UINT64 MiscRegBaseAddr = FixedPcdGet64(PcdMiscRegBaseAddress);
  if (MiscRegBaseAddr == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read HIDREV register\n", __FUNCTION__));
    return MAX_UINT32;
  }

  return (MmioRead32(MiscRegBaseAddr + HIDREV_OFFSET));
}

TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  )
{
  UINT32 Hidrev = TegraReadHidrevReg();
  UINT32 PlatType;
  PlatType = ((Hidrev >> HIDREV_PRE_SI_PLAT_SHIFT) & HIDREV_PRE_SI_PLAT_MASK);
  if (PlatType >= TEGRA_PLATFORM_UNKNOWN) {
    return TEGRA_PLATFORM_UNKNOWN;
  } else {
    return PlatType;
  }
}

UINT32
TegraGetMajorVersion (
  VOID
  )
{
  UINT32 Hidrev = TegraReadHidrevReg();

  return ((Hidrev >> HIDREV_MAJORVER_SHIFT) & HIDREV_MAJORVER_MASK);
}
