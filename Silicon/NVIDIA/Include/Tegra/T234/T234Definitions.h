/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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
*  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __T234_DEFINES_H__
#define __T234_DEFINES_H__

// GIC
#define T234_GIC_DISTRIBUTOR_BASE        0X0F400000
#define T234_GIC_REDISTRIBUTOR_BASE      0X0F440000
#define T234_GIC_REDISTRIBUTOR_INSTANCES 12

// PCIE
#define T234_PCIE_C1_CFG_BASE_ADDR       0x20B0000000
#define T234_PCIE_BUS_MIN                0
#define T234_PCIE_BUS_MAX                255

// BL CARVEOUT OFFSET
#define T234_BL_CARVEOUT_OFFSET               0x548

#endif //__T234_DEFINES_H__
