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

#define T194_PMU_BASE_INTERRUPT 0x180
#define T194_VIRT_MAINT_INT     25

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

#endif //__T194_DEFINES_H__
