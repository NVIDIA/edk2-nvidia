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

#ifndef __TH500_DEFINES_H__
#define __TH500_DEFINES_H__

// UARTA
#define TH500_UARTC_BASE_ADDR    0x0C280000
#define TH500_UARTC_CAR_SIZE     0x00010000
#define TH500_UARTC_INTR         0x92

// SDMMC4
#define TH500_SDMMC4_BASE_ADDR   0x03460000
#define TH500_SDMMC4_CAR_SIZE    0x00010000
#define TH500_SDMMC4_INTR        0x61

// ETHERNET
#define TH500_ETHERNET_BASE_ADDR 0x03B40000
#define TH500_ETHERNET_CAR_SIZE  0x00000100
#define TH500_ETHERNET_INTR      0xFA

// PCIE
#define TH500_PCIE_BUS_MIN       0
#define TH500_PCIE_BUS_MAX       1
#define TH500_PCIE_C0_64BIT_BASE 0x2680000000
#define TH500_PCIE_C0_64BIT_END  0x2A7FFFFFFF
#define TH500_PCIE_C0_64BIT_SIZE 0x400000000


#endif //__TH500_DEFINES_H__
