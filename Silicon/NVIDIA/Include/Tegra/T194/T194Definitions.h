/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T194_DEFINES_H__
#define __T194_DEFINES_H__

#define T194_PMU_BASE_INTERRUPT  0x180
#define T194_VIRT_MAINT_INT      25

// SDMMC1
#define T194_SDMMC1_BASE_ADDR  0x03400000
#define T194_SDMMC1_CAR_SIZE   0x00010000
#define T194_SDMMC1_INTR       0x5E

// SDMMC4
#define T194_SDMMC4_BASE_ADDR  0x03460000
#define T194_SDMMC4_CAR_SIZE   0x00010000
#define T194_SDMMC4_INTR       0x61

// ETHERNET
#define T194_ETHERNET_BASE_ADDR  0x02490000
#define T194_ETHERNET_CAR_SIZE   0x00010000
#define T194_ETHERNET_INTR       0xE2

// PCIE
#define T194_PCIE_BUS_MIN  0
#define T194_PCIE_BUS_MAX  31

// GIC
#define T194_GIC_DISTRIBUTOR_BASE          0x03881000
#define T194_GIC_INTERRUPT_INTERFACE_BASE  0x03882000

// BL CARVEOUT OFFSET
#define T194_BL_CARVEOUT_OFFSET  0x448

// BPMP Info
#define BPMP_TX_MAILBOX    0x4004E000
#define BPMP_RX_MAILBOX    0x4004F000
#define BPMP_CHANNEL_SIZE  0x100
#define MRQ_THERMAL        27
#define ZONE_TEMP          1
#define CPU_TEMP_ZONE      2
#define TEMP_POLL_TIME     50                // 5s

#endif //__T194_DEFINES_H__
