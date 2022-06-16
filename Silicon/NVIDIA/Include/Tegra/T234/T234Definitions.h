/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T234_DEFINES_H__
#define __T234_DEFINES_H__

// GIC
#define T234_GIC_DISTRIBUTOR_BASE        0X0F400000
#define T234_GIC_REDISTRIBUTOR_BASE      0X0F440000
#define T234_GIC_REDISTRIBUTOR_INSTANCES 16

// PCIE
#define T234_PCIE_C1_CFG_BASE_ADDR       0x20B0000000
#define T234_PCIE_C4_CFG_BASE_ADDR       0x2430000000
#define T234_PCIE_C5_CFG_BASE_ADDR       0x2B30000000

#define T234_PCIE_BUS_MIN                0
#define T234_PCIE_BUS_MAX                255

// BL CARVEOUT OFFSET
#define T234_BL_CARVEOUT_OFFSET               0x588

#endif //__T234_DEFINES_H__
