/** @file
*  VPR Dxe
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
*  Portions provided under the following terms:
*  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
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
