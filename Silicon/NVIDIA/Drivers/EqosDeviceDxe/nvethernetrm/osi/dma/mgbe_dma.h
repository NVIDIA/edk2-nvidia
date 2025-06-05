/*
 * SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef INCLUDED_MGBE_DMA_H
#define INCLUDED_MGBE_DMA_H

/**
 * @addtogroup MGBE_AXI_Clock defines
 *
 * @brief AXI Clock defines
 * @{
 */
#define MGBE_AXI_CLK_FREQ  480000000U
/** @} */

/**
 * @addtogroup MGBE_DMA DMA Channel Register offsets
 *
 * @brief MGBE DMA Channel register offsets
 * @{
 */
#define MGBE_T26X_GLOBAL_DMA_STATUS  0x8800U

#define MGBE_DMA_CHX_TX_CTRL(x)  ((0x0080U * (x)) + 0x3104U)
#define MGBE_DMA_CHX_RX_CTRL(x)  ((0x0080U * (x)) + 0x3108U)
#ifndef OSI_STRIPPED_LIB
#define MGBE_DMA_CHX_SLOT_CTRL(x)  ((0x0080U * (x)) + 0x310CU)
#endif /* !OSI_STRIPPED_LIB */
#define MGBE_DMA_CHX_INTR_ENA(x)               ((0x0080U * (x)) + 0x3138U)
#define MGBE_DMA_CHX_CTRL(x)                   ((0x0080U * (x)) + 0x3100U)
#define MGBE_DMA_CHX_RX_WDT(x)                 ((0x0080U * (x)) + 0x313CU)
#define MGBE_DMA_CHX_TX_CNTRL2(x)              ((0x0080U * (x)) + 0x3130U)
#define MGBE_DMA_CHX_RX_CNTRL2(x)              ((0x0080U * (x)) + 0x3134U)
#define MGBE_DMA_CHX_TDLH(x)                   ((0x0080U * (x)) + 0x3110U)
#define MGBE_DMA_CHX_TDLA(x)                   ((0x0080U * (x)) + 0x3114U)
#define MGBE_DMA_CHX_TDTLP(x)                  ((0x0080U * (x)) + 0x3124U)
#define MGBE_DMA_CHX_RDLH(x)                   ((0x0080U * (x)) + 0x3118U)
#define MGBE_DMA_CHX_RDLA(x)                   ((0x0080U * (x)) + 0x311CU)
#define MGBE_DMA_CHX_RDTLP(x)                  ((0x0080U * (x)) + 0x312CU)
#define MGBE_DMA_CHX_RX_DESC_WR_RNG_OFFSET(x)  ((0x0080U * (x)) + 0x317CU)

#define MAX_REG_OFFSET  0xFFFFU
/** @} */

/** @} */

/**
 * @addtogroup MGBE_BIT BIT fields for MGBE channel registers
 *
 * @brief Values defined for the MGBE registers
 * @{
 */
#define MGBE_DMA_CHX_RX_WDT_RWT_MASK         0xFFU
#define MGBE_DMA_CHX_RX_WDT_RWTU             2048U
#define MGBE_DMA_CHX_RX_WDT_RWTU_2048_CYCLE  0x3000U
#define MGBE_DMA_CHX_RX_WDT_RWTU_MASK        0x3000U
#define MGBE_DMA_CHX_RX_WDT_ITW_MASK         0x7C000000U
#define MGBE_DMA_CHX_RX_WDT_ITW_SHIFT        26U
#define MGBE_DMA_CHX_RX_WDT_ITW_MAX          0x1FU
#define MGBE_DMA_CHX_RX_WDT_ITW_DEFAULT      1100U
#define MGBE_DMA_CHX_RX_WDT_ITCU             256U

#ifdef OSI_DEBUG
#define MGBE_DMA_CHX_INTR_TBUE  OSI_BIT(2)
#define MGBE_DMA_CHX_INTR_RBUE  OSI_BIT(7)
#define MGBE_DMA_CHX_INTR_FBEE  OSI_BIT(12)
#define MGBE_DMA_CHX_INTR_AIE   OSI_BIT(14)
#define MGBE_DMA_CHX_INTR_NIE   OSI_BIT(15)
#endif
#ifndef OSI_STRIPPED_LIB
#define MGBE_DMA_CHX_SLOT_ESC  OSI_BIT(0)
#endif /* !OSI_STRIPPED_LIB */
#define MGBE_DMA_CHX_TX_CNTRL2_ORRQ_RECOMMENDED  64U
#define MGBE_DMA_CHX_TX_CNTRL2_ORRQ_SHIFT        24U
#define MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SCHAN        32U
#define MGBE_DMA_CHX_RX_CNTRL2_OWRQ_MCHAN        64U
#define MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SHIFT        24U
#define MGBE_DMA_CHX_TX_CTRL_TXPBL_RECOMMENDED   0x100000U
#define MGBE_DMA_CHX_CTRL_PBL_SHIFT              16U
/* MGBE VDMA to TC mask */
#define MGBE_TX_VDMA_TC_MASK            (OSI_BIT(4) | OSI_BIT(5) | OSI_BIT(6))
#define MGBE_TX_VDMA_TC_SHIFT           4
#define MGBE_RX_VDMA_TC_MASK            (OSI_BIT(28) | OSI_BIT(29) | OSI_BIT(30))
#define MGBE_RX_VDMA_TC_SHIFT           28
#define MGBE_RX_DESC_WR_RNG_RWDC_SHIFT  16

/** @} */

/**
 * @addtogroup MGBE-MAC MAC register offsets
 *
 * @{
 */
#define MGBE_MAC_STSR             0x0D08
#define MGBE_MAC_STNSR            0x0D0C
#define MGBE_MAC_STNSR_TSSS_MASK  0x7FFFFFFFU

#define MGBE_MAC_TX  0x0000
#define MGBE_MCR_TE  OSI_BIT(0)
/** @} */

#endif
