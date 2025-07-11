/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_HW_COMMON_H
#define INCLUDED_HW_COMMON_H

/**
 * @addtogroup COMMON HW specific offset macros
 *
 * @brief Register offset values common for EQOS and MGBE
 * @{
 */
#define HW_GLOBAL_DMA_STATUS  0x8700U
#define VIRT_INTR_CHX_CNTRL(x)   (0x8600U + ((x) * 8U))
#define VIRT_INTR_CHX_STATUS(x)  (0x8604U + ((x) * 8U))
#define AXI_BUS_WIDTH            0x10U
#define DMA_CHX_INTR_TIE         OSI_BIT(0)
#define DMA_CHX_INTR_RIE         OSI_BIT(6)
#define DMA_CHX_CTRL_PBLX8       OSI_BIT(16)
#define DMA_CHX_TX_CTRL_OSP      OSI_BIT(4)
#define DMA_CHX_TX_CTRL_TSE      OSI_BIT(12)
#define DMA_CHX_RBSZ_MASK        0x7FFEU
#define DMA_CHX_RBSZ_SHIFT       1U
#define DMA_CHX_RX_WDT_RWT_MASK  0xFFU
/** @} */

#endif /* INCLUDED_HW_COMMON_H */
