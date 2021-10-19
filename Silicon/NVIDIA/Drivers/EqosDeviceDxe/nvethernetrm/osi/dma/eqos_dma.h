/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
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
#define EQOS_AXI_CLK_FREQ	125000000U
/** @} */

/**
 * @addtogroup EQOS1 DMA Channel Register offsets
 *
 * @brief EQOS DMA Channel register offsets
 * @{
 */
#define EQOS_DMA_CHX_CTRL(x)			((0x0080U * (x)) + 0x1100U)
#define EQOS_DMA_CHX_TX_CTRL(x)			((0x0080U * (x)) + 0x1104U)
#define EQOS_DMA_CHX_RX_CTRL(x)			((0x0080U * (x)) + 0x1108U)
#define EQOS_DMA_CHX_INTR_ENA(x)		((0x0080U * (x)) + 0x1134U)
#define EQOS_DMA_CHX_RX_WDT(x)			((0x0080U * (x)) + 0x1138U)
#ifndef OSI_STRIPPED_LIB
#define EQOS_DMA_CHX_SLOT_CTRL(x)		((0x0080U * (x)) + 0x113CU)
#endif /* !OSI_STRIPPED_LIB */

#define EQOS_DMA_CHX_RDTP(x)			((0x0080U * (x)) + 0x1128U)
#define EQOS_DMA_CHX_RDLH(x)			((0x0080U * (x)) + 0x1118U)
#define EQOS_DMA_CHX_RDLA(x)			((0x0080U * (x)) + 0x111CU)
#define EQOS_DMA_CHX_RDRL(x)			((0x0080U * (x)) + 0x1130U)
#define EQOS_DMA_CHX_TDTP(x)			((0x0080U * (x)) + 0x1120U)
#define EQOS_DMA_CHX_TDLH(x)			((0x0080U * (x)) + 0x1110U)
#define EQOS_DMA_CHX_TDLA(x)			((0x0080U * (x)) + 0x1114U)
#define EQOS_DMA_CHX_TDRL(x)			((0x0080U * (x)) + 0x112CU)
#define EQOS_VIRT_INTR_APB_CHX_CNTRL(x)		(0x8200U + ((x) * 4U))
#define EQOS_VIRT_INTR_CHX_STATUS(x)		(0x8604U + ((x) * 8U))
#define EQOS_VIRT_INTR_CHX_CNTRL(x)		(0x8600U + ((x) * 8U))
#define EQOS_GLOBAL_DMA_STATUS			(0x8700U)
/** @} */

/**
 * @addtogroup EQOS2 BIT fields for EQOS MAC HW DMA Channel Registers
 *
 * @brief Values defined for the DMA channel registers
 * @{
 */
#define EQOS_VIRT_INTR_CHX_STATUS_TX		OSI_BIT(0)
#define EQOS_VIRT_INTR_CHX_STATUS_RX		OSI_BIT(1)
#define EQOS_DMA_CHX_STATUS_TI			OSI_BIT(0)
#define EQOS_DMA_CHX_STATUS_RI			OSI_BIT(6)
#define EQOS_DMA_CHX_STATUS_NIS			OSI_BIT(15)
#define EQOS_DMA_CHX_STATUS_CLEAR_TX	\
	(EQOS_DMA_CHX_STATUS_TI | EQOS_DMA_CHX_STATUS_NIS)
#define EQOS_DMA_CHX_STATUS_CLEAR_RX	\
	(EQOS_DMA_CHX_STATUS_RI | EQOS_DMA_CHX_STATUS_NIS)

#define EQOS_VIRT_INTR_CHX_CNTRL_TX		OSI_BIT(0)
#define EQOS_VIRT_INTR_CHX_CNTRL_RX		OSI_BIT(1)

#define EQOS_DMA_CHX_INTR_TIE			OSI_BIT(0)
#define EQOS_DMA_CHX_INTR_TBUE			OSI_BIT(2)
#define EQOS_DMA_CHX_INTR_RIE			OSI_BIT(6)
#define EQOS_DMA_CHX_INTR_RBUE			OSI_BIT(7)
#define EQOS_DMA_CHX_INTR_FBEE			OSI_BIT(12)
#define EQOS_DMA_CHX_INTR_AIE			OSI_BIT(14)
#define EQOS_DMA_CHX_INTR_NIE			OSI_BIT(15)
#define EQOS_DMA_CHX_TX_CTRL_OSF		OSI_BIT(4)
#define EQOS_DMA_CHX_TX_CTRL_TSE		OSI_BIT(12)
#define EQOS_DMA_CHX_CTRL_PBLX8			OSI_BIT(16)
#define EQOS_DMA_CHX_RBSZ_MASK			0x7FFEU
#define EQOS_DMA_CHX_RBSZ_SHIFT			1U
#define EQOS_DMA_CHX_TX_CTRL_TXPBL_RECOMMENDED	0x200000U
#define EQOS_DMA_CHX_RX_CTRL_RXPBL_RECOMMENDED	0xC0000U
#define EQOS_DMA_CHX_RX_WDT_RWT_MASK		0xFFU
#define EQOS_DMA_CHX_RX_WDT_RWTU_MASK		0x30000U
#define EQOS_DMA_CHX_RX_WDT_RWTU_512_CYCLE	0x10000U
#define EQOS_DMA_CHX_RX_WDT_RWTU		512U

/* Below macros are used for periodic reg validation for functional safety.
 * HW register mask - to mask out reserved and self-clearing bits
 */
#define EQOS_DMA_CHX_CTRL_MASK			0x11D3FFFU
#define EQOS_DMA_CHX_TX_CTRL_MASK		0xF3F9010U
#define EQOS_DMA_CHX_RX_CTRL_MASK		0x8F3F7FE0U
#define EQOS_DMA_CHX_TDRL_MASK			0x3FFU
#define EQOS_DMA_CHX_RDRL_MASK			0x3FFU
#define EQOS_DMA_CHX_INTR_ENA_MASK		0xFFC7U
#ifndef OSI_STRIPPED_LIB
#define EQOS_DMA_CHX_SLOT_SIV_MASK		0xFFFU
#define EQOS_DMA_CHX_SLOT_SIV_SHIFT		4U
#define EQOS_DMA_CHX_SLOT_ESC			0x1U
#endif /* !OSI_STRIPPED_LIB */
/* To add new registers to validate,append at end of below macro list and
 * increment EQOS_MAX_DMA_SAFETY_REGS.
 * Using macros instead of enum due to misra error.
 */
#define EQOS_DMA_CH0_CTRL_IDX			0U
#define EQOS_DMA_CH1_CTRL_IDX			1U
#define EQOS_DMA_CH2_CTRL_IDX			2U
#define EQOS_DMA_CH3_CTRL_IDX			3U
#define EQOS_DMA_CH4_CTRL_IDX			4U
#define EQOS_DMA_CH5_CTRL_IDX			5U
#define EQOS_DMA_CH6_CTRL_IDX			6U
#define EQOS_DMA_CH7_CTRL_IDX			7U
#define EQOS_DMA_CH0_TX_CTRL_IDX		8U
#define EQOS_DMA_CH1_TX_CTRL_IDX		9U
#define EQOS_DMA_CH2_TX_CTRL_IDX		10U
#define EQOS_DMA_CH3_TX_CTRL_IDX		11U
#define EQOS_DMA_CH4_TX_CTRL_IDX		12U
#define EQOS_DMA_CH5_TX_CTRL_IDX		13U
#define EQOS_DMA_CH6_TX_CTRL_IDX		14U
#define EQOS_DMA_CH7_TX_CTRL_IDX		15U
#define EQOS_DMA_CH0_RX_CTRL_IDX		16U
#define EQOS_DMA_CH1_RX_CTRL_IDX		17U
#define EQOS_DMA_CH2_RX_CTRL_IDX		18U
#define EQOS_DMA_CH3_RX_CTRL_IDX		19U
#define EQOS_DMA_CH4_RX_CTRL_IDX		20U
#define EQOS_DMA_CH5_RX_CTRL_IDX		21U
#define EQOS_DMA_CH6_RX_CTRL_IDX		22U
#define EQOS_DMA_CH7_RX_CTRL_IDX		23U
#define EQOS_DMA_CH0_TDRL_IDX			24U
#define EQOS_DMA_CH1_TDRL_IDX			25U
#define EQOS_DMA_CH2_TDRL_IDX			26U
#define EQOS_DMA_CH3_TDRL_IDX			27U
#define EQOS_DMA_CH4_TDRL_IDX			28U
#define EQOS_DMA_CH5_TDRL_IDX			29U
#define EQOS_DMA_CH6_TDRL_IDX			30U
#define EQOS_DMA_CH7_TDRL_IDX			31U
#define EQOS_DMA_CH0_RDRL_IDX			32U
#define EQOS_DMA_CH1_RDRL_IDX			33U
#define EQOS_DMA_CH2_RDRL_IDX			34U
#define EQOS_DMA_CH3_RDRL_IDX			35U
#define EQOS_DMA_CH4_RDRL_IDX			36U
#define EQOS_DMA_CH5_RDRL_IDX			37U
#define EQOS_DMA_CH6_RDRL_IDX			38U
#define EQOS_DMA_CH7_RDRL_IDX			39U
#define EQOS_DMA_CH0_INTR_ENA_IDX		40U
#define EQOS_DMA_CH1_INTR_ENA_IDX		41U
#define EQOS_DMA_CH2_INTR_ENA_IDX		42U
#define EQOS_DMA_CH3_INTR_ENA_IDX		43U
#define EQOS_DMA_CH4_INTR_ENA_IDX		44U
#define EQOS_DMA_CH5_INTR_ENA_IDX		45U
#define EQOS_DMA_CH6_INTR_ENA_IDX		46U
#define EQOS_DMA_CH7_INTR_ENA_IDX		47U
#define EQOS_MAX_DMA_SAFETY_REGS		48U
#define EQOS_AXI_BUS_WIDTH			0x10U
/** @} */

/**
 * @brief dma_func_safety - Struct used to store last written values of
 *	critical DMA HW registers.
 */
struct dma_func_safety {
	/** Array of reg MMIO addresses (base EQoS + offset of reg) */
	void *reg_addr[EQOS_MAX_DMA_SAFETY_REGS];
	/** Array of bit-mask value of each corresponding reg
	 * (used to ignore self-clearing/reserved bits in reg) */
	nveu32_t reg_mask[EQOS_MAX_DMA_SAFETY_REGS];
	/** Array of value stored in each corresponding register */
	nveu32_t reg_val[EQOS_MAX_DMA_SAFETY_REGS];
	/** OSI lock variable used to protect writes to reg
	 * while validation is in-progress */
	nveu32_t dma_safety_lock;
};

/**
 * @brief eqos_get_dma_safety_config - EQOS get DMA safety configuration
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @returns Pointer to DMA safety configuration
 */
void *eqos_get_dma_safety_config(void);
#endif /* INCLUDED_EQOS_DMA_H */
