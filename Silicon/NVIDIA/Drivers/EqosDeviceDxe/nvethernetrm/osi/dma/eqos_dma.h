/*
 * SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_EQOS_DMA_H
#define INCLUDED_EQOS_DMA_H

/**
 * @addtogroup EQOS AXI Clock defines
 *
 * @brief AXI Clock defines
 * @{
 */
#define EQOS_AXI_CLK_FREQ  125000000U
/** @} */

/**
 * @addtogroup EQOS1 DMA Channel Register offsets
 *
 * @brief EQOS DMA Channel register offsets
 * @{
 */
#define EQOS_DMA_CHX_CTRL(x)      ((0x0080U * (x)) + 0x1100U)
#define EQOS_DMA_CHX_TX_CTRL(x)   ((0x0080U * (x)) + 0x1104U)
#define EQOS_DMA_CHX_RX_CTRL(x)   ((0x0080U * (x)) + 0x1108U)
#define EQOS_DMA_CHX_INTR_ENA(x)  ((0x0080U * (x)) + 0x1134U)
#define EQOS_DMA_CHX_RX_WDT(x)    ((0x0080U * (x)) + 0x1138U)
#ifndef OSI_STRIPPED_LIB
#define EQOS_DMA_CHX_SLOT_CTRL(x)  ((0x0080U * (x)) + 0x113CU)
#endif /* !OSI_STRIPPED_LIB */

#define EQOS_DMA_CHX_RDTP(x)  ((0x0080U * (x)) + 0x1128U)
#define EQOS_DMA_CHX_RDLH(x)  ((0x0080U * (x)) + 0x1118U)
#define EQOS_DMA_CHX_RDLA(x)  ((0x0080U * (x)) + 0x111CU)
#define EQOS_DMA_CHX_RDRL(x)  ((0x0080U * (x)) + 0x1130U)
#define EQOS_DMA_CHX_TDTP(x)  ((0x0080U * (x)) + 0x1120U)
#define EQOS_DMA_CHX_TDLH(x)  ((0x0080U * (x)) + 0x1110U)
#define EQOS_DMA_CHX_TDLA(x)  ((0x0080U * (x)) + 0x1114U)
#define EQOS_DMA_CHX_TDRL(x)  ((0x0080U * (x)) + 0x112CU)
#define EQOS_DMA_CHX_CARD(x)  ((0x0080U * (x)) + 0x114CU)
#define EQOS_DMA_DEBUG_STATUS_0  0x100CU
#define EQOS_DMA_DEBUG_STATUS_1  0x1010U
#define EQOS_DMA_DEBUG_STATUS_2  0x1014U
/** @} */

/**
 * @addtogroup EQOS2 BIT fields for EQOS MAC HW DMA Channel Registers
 *
 * @brief Values defined for the DMA channel registers
 * @{
 */
#define EQOS_DMA_CHX_STATUS_TI   OSI_BIT(0)
#define EQOS_DMA_CHX_STATUS_RI   OSI_BIT(6)
#define EQOS_DMA_CHX_STATUS_NIS  OSI_BIT(15)
#define EQOS_DMA_CHX_STATUS_CLEAR_TX    \
        (EQOS_DMA_CHX_STATUS_TI | EQOS_DMA_CHX_STATUS_NIS)
#define EQOS_DMA_CHX_STATUS_CLEAR_RX    \
        (EQOS_DMA_CHX_STATUS_RI | EQOS_DMA_CHX_STATUS_NIS)

#ifdef OSI_DEBUG
#define EQOS_DMA_CHX_INTR_TBUE  OSI_BIT(2)
#define EQOS_DMA_CHX_INTR_RBUE  OSI_BIT(7)
#define EQOS_DMA_CHX_INTR_FBEE  OSI_BIT(12)
#define EQOS_DMA_CHX_INTR_AIE   OSI_BIT(14)
#define EQOS_DMA_CHX_INTR_NIE   OSI_BIT(15)
#endif
#define EQOS_DMA_CHX_TX_CTRL_TXPBL_RECOMMENDED  0x200000U
#define EQOS_DMA_CHX_RX_CTRL_RXPBL_RECOMMENDED  0xC0000U
#define EQOS_DMA_CHX_RX_WDT_RWT_MASK            0xFFU
#define EQOS_DMA_CHX_RX_WDT_RWTU_MASK           0x30000U
#define EQOS_DMA_CHX_RX_WDT_RWTU_512_CYCLE      0x10000U
#define EQOS_DMA_CHX_RX_WDT_RWTU                512U
#define EQOS_DMA_DEBUG_STATUS_0_RPS0_SHIFT      8U
#define EQOS_DMA_DEBUG_STATUS_0_RPS1_SHIFT      16U
#define EQOS_DMA_DEBUG_STATUS_0_RPS2_SHIFT      24U
#define EQOS_DMA_DEBUG_STATUS_1_RPS3_SHIFT      0U
#define EQOS_DMA_DEBUG_STATUS_1_RPS4_SHIFT      8U
#define EQOS_DMA_DEBUG_STATUS_1_RPS5_SHIFT      16U
#define EQOS_DMA_DEBUG_STATUS_1_RPS6_SHIFT      24U
#define EQOS_DMA_DEBUG_STATUS_2_RPS7_SHIFT      0U
#define EQOS_DMA_DEBUG_STATUS_RPSX_MASK         7U
#define EQOS_DMA_DEBUG_STATUS_RPSX_RUN_CRD      5U

/* Below macros are used for periodic reg validation for functional safety.
 * HW register mask - to mask out reserved and self-clearing bits
 */
#ifndef OSI_STRIPPED_LIB
#define EQOS_DMA_CHX_SLOT_SIV_MASK   0xFFFU
#define EQOS_DMA_CHX_SLOT_SIV_SHIFT  4U
#define EQOS_DMA_CHX_SLOT_ESC        0x1U
#endif /* !OSI_STRIPPED_LIB */
/** @} */

/**
 * @brief PTP Time read registers
 * @{
 */
#define EQOS_MAC_STSR             0x0B08
#define EQOS_MAC_STNSR            0x0B0C
#define EQOS_MAC_STNSR_TSSS_MASK  0x7FFFFFFFU
/** @} */

/**
 * @brief Common MAC MCR register and its bits
 * @{
 */
#define EQOS_MAC_MCR  0x0000
#define EQOS_MCR_TE   OSI_BIT(0)
#define EQOS_MCR_RE   OSI_BIT(1)
/** @} */

#endif /* INCLUDED_EQOS_DMA_H */
