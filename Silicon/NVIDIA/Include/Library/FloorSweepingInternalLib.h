/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __FLOOR_SWEEPING_INTERNAL_LIB_H__
#define __FLOOR_SWEEPING_INTERNAL_LIB_H__

/**
  Check if the given core is enabled or not

**/
BOOLEAN
EFIAPI
IsCoreEnabledInternal (
  IN  UINT32  CpuNum,
  OUT BOOLEAN *CoreEnabled
);

/**
  Retrieve number of CPUs for each platform

**/
BOOLEAN
EFIAPI
GetNumberOfEnabledCpuCoresInternal (
  OUT UINT32 *NumCpus
);

#endif //__FLOOR_SWEEPING_INTERNAL_LIB_H__
