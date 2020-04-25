/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __T194_DEFINES_H__
#define __T194_DEFINES_H__

// UARTB
#define T194_UARTB_BASE_ADDR    0x03110000
#define T194_UARTB_CAR_SIZE     0x00010000
#define T194_UARTB_INTR         0x91

// UARTC
#define T194_UARTC_BASE_ADDR    0x0C280000
#define T194_UARTC_CAR_SIZE     0x00010000
#define T194_UARTC_INTR         0x92

// PWM4
#define T194_PWM4_BASE_ADDR     0x0C340000

// SDMMC1
#define T194_SDMMC1_BASE_ADDR   0x03400000
#define T194_SDMMC1_CAR_SIZE    0x00010000
#define T194_SDMMC1_INTR        0x5E

// SDMMC4
#define T194_SDMMC4_BASE_ADDR   0x03460000
#define T194_SDMMC4_CAR_SIZE    0x00010000
#define T194_SDMMC4_INTR        0x61

// ETHERNET
#define T194_ETHERNET_BASE_ADDR 0x02490000
#define T194_ETHERNET_CAR_SIZE  0x00010000
#define T194_ETHERNET_INTR      0xE2

// PCIE
#define T194_PCIE_BUS_MIN       0
#define T194_PCIE_BUS_MAX       31

#define T194_PCIE_C0_CFG_BASE_ADDR 0x38000000
#define T194_PCIE_C1_CFG_BASE_ADDR 0x30000000
#define T194_PCIE_C2_CFG_BASE_ADDR 0x32000000
#define T194_PCIE_C3_CFG_BASE_ADDR 0x34000000
#define T194_PCIE_C4_CFG_BASE_ADDR 0x36000000
#define T194_PCIE_C5_CFG_BASE_ADDR 0x3a000000

#endif //__T194_DEFINES_H__
