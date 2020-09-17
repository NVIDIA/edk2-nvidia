/** @file
*  VPR Dxe
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __VPRDXE_H__
#define __VPRDXE_H__

#define VPR_CMDLINE_MAX_LEN 0x100

// VPR Offsets
#define MC_VIDEO_PROTECT_BOM_0        0x648
#define MC_VIDEO_PROTECT_SIZE_MB_0    0x64c
#define MC_VIDEO_PROTECT_REG_CTRL_0   0x650
#define MC_VIDEO_PROTECT_BOM_ADR_HI_0 0x978

// VPR MC_VIDEO_PROTECT_REG_CTRL_0 BMSK
#define VIDEO_PROTECT_ALLOW_TZ_WRITE_ACCESS_BMSK 0x2

#endif // __VPRDXE_H__
