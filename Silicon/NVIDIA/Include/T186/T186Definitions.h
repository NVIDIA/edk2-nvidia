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

#ifndef __T186_DEFINES_H__
#define __T186_DEFINES_H__

// UARTA
#define T186_UARTA_BASE_ADDR  0x03100000
#define T186_UARTA_CAR_SIZE   0x00010000
#define T186_UARTA_INTR       0x90

// SDMMC1
#define T186_SDMMC1_BASE_ADDR 0x03400000
#define T186_SDMMC1_CAR_SIZE  0x00010000
#define T186_SDMMC1_INTR      0x5E

// SDMMC4
#define T186_SDMMC4_BASE_ADDR 0x03460000
#define T186_SDMMC4_CAR_SIZE  0x00010000
#define T186_SDMMC4_INTR      0x61


#endif //__T186_DEFINES_H__
