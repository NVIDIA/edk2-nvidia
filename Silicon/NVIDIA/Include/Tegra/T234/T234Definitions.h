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
**/

#ifndef __T234_DEFINES_H__
#define __T234_DEFINES_H__

// UARTA
#define T234_UARTA_BASE_ADDR             0x03100000
#define T234_UARTA_CAR_SIZE              0x00010000
#define T234_UARTA_INTR                  0x90

// SDMMC1
#define T234_SDMMC1_BASE_ADDR            0x03400000
#define T234_SDMMC1_CAR_SIZE             0x00010000
#define T234_SDMMC1_INTR                 0x5E

// SDMMC4
#define T234_SDMMC4_BASE_ADDR            0x03460000
#define T234_SDMMC4_CAR_SIZE             0x00010000
#define T234_SDMMC4_INTR                 0x61

// SERIAL
#define T234_SERIAL_REGISTER_BASE        0X03100000

// GIC
#define T234_GIC_DISTRIBUTOR_BASE        0X0F400000
#define T234_GIC_REDISTRIBUTOR_BASE      0X0F440000
#define T234_GIC_REDISTRIBUTOR_INSTANCES 16

// PCIE
#define T234_PCIE_C1_CFG_BASE_ADDR       0x20B0000000
#define T234_PCIE_BUS_MIN                0
#define T234_PCIE_BUS_MAX                255

// BL CARVEOUT OFFSET
#define T234_BL_CARVEOUT_OFFSET               0x548

#endif //__T234_DEFINES_H__
