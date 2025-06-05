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

#ifndef INCLUDED_OSI_COMMON_H
#define INCLUDED_OSI_COMMON_H

#include <nvethernet_type.h>

/**
 * @brief Maximum number of supported MAC IP types (EQOS, MGBE, MGBE_T26X)
 */
#define OSI_MAX_MAC_IP_TYPES  3U

/**
 * @addtogroup FC Flow Control Threshold Macros
 *
 * @brief These bits control the threshold (fill-level of Rx queue) at which
 * the flow control is asserted or de-asserted
 * @{
 */
#define FULL_MINUS_1_5K  ((nveu32_t)1)
#define FULL_MINUS_16_K  ((nveu32_t)30)
#define FULL_MINUS_32_K  ((nveu32_t)62)
/** @} */

/**
 * @addtogroup OSI-Helper OSI Helper MACROS
 * @{
 */
#define OSI_UNLOCKED  0x0U
#define OSI_LOCKED    0x1U
/** @brief Number of Nano seconds per second */
#define OSI_NSEC_PER_SEC    1000000000ULL
#define OSI_NSEC_PER_SEC_U  1000000000U

#define OSI_MGBE_MAX_RX_RIIT_NSEC  17500U
#define OSI_MGBE_MIN_RX_RIIT_NSEC  535U
#ifndef OSI_STRIPPED_LIB
#define OSI_MAX_RX_COALESCE_USEC       1020U
#define OSI_EQOS_MIN_RX_COALESCE_USEC  5U
#define OSI_MGBE_MIN_RX_COALESCE_USEC  6U
#define OSI_MIN_RX_COALESCE_FRAMES     1U
#define OSI_MAX_TX_COALESCE_USEC       1020U
#define OSI_MIN_TX_COALESCE_USEC       32U
#define OSI_MIN_TX_COALESCE_FRAMES     1U
#endif /* !OSI_STRIPPED_LIB */

/* Compiler hints for branch prediction */
#define osi_unlikely(x)  __builtin_expect(!!(x), 0)
/** @} */

/**
 * @addtogroup FC-Helper MACROS
 *
 * @brief FC-Helper Flow control enable/disable macros.
 * @{
 */
/** flag to disable pause frames */
#define OSI_PAUSE_FRAMES_DISABLE  0U
/** flag to enable pause frames */
#define OSI_PAUSE_FRAMES_ENABLE  1U
/** @} */

/**
 * @addtogroup helper MACROS
 *
 * @brief EQOS generic helper MACROS.
 * @{
 */
#define OSI_MAX_24BITS     0xFFFFFFU
#define OSI_MAX_28BITS     0xFFFFFFFU
#define OSI_MAX_32BITS     0xFFFFFFFFU
#define OSI_MASK_16BITS    0xFFFFU
#define OSI_MASK_20BITS    0xFFFFFU
#define OSI_MASK_24BITS    0xFFFFFFU
#define OSI_GCL_SIZE_64    64U
#define OSI_GCL_SIZE_128   128U
#define OSI_GCL_SIZE_512   512U
#define OSI_GCL_SIZE_1024  1024U
/** @} */

#ifndef OSI_STRIPPED_LIB

/**
 * @addtogroup Helper MACROS
 *
 * @brief EQOS generic helper MACROS.
 * @{
 */
#define OSI_FLOW_CTRL_DISABLE  0U
#define OSI_ADDRESS_32BIT      0
#define OSI_ADDRESS_40BIT      1
#define OSI_ADDRESS_48BIT      2
/** @ } */

/**
 * @addtogroup - LPI-Timers LPI configuration macros
 *
 * @brief LPI timers and config register field masks.
 * @{
 */
/* LPI LS timer - minimum time (in milliseconds) for which the link status from
 * PHY should be up before the LPI pattern can be transmitted to the PHY.
 * Default 1sec.
 */
#define OSI_DEFAULT_LPI_LS_TIMER  (nveu32_t)1000
#define OSI_LPI_LS_TIMER_MASK     0x3FFU
#define OSI_LPI_LS_TIMER_SHIFT    16U

/* LPI TW timer - minimum time (in microseconds) for which MAC wait after it
 * stops transmitting LPI pattern before resuming normal tx.
 * Default 21us
 */
#define OSI_DEFAULT_LPI_TW_TIMER  0x15U
#define OSI_LPI_TW_TIMER_MASK     0xFFFFU

/* LPI entry timer - Time in microseconds that MAC will wait to enter LPI mode
 * after all tx is complete.
 * Default 1sec.
 */
#define OSI_LPI_ENTRY_TIMER_MASK  0xFFFF8U

/* LPI entry timer - Time in microseconds that MAC will wait to enter LPI mode
 * after all tx is complete. Default 1sec.
 */
#define OSI_DEFAULT_TX_LPI_TIMER  0xF4240U

/* Max Tx LPI timer (in usecs) based on the timer value field length in HW
 * MAC_LPI_ENTRY_TIMER register */
#define OSI_MAX_TX_LPI_TIMER  0xFFFF8U

/* Min Tx LPI timer (in usecs) based on the timer value field length in HW
 * MAC_LPI_ENTRY_TIMER register */
#define OSI_MIN_TX_LPI_TIMER  0x8U

/* Time in 1 microseconds tic counter used as reference for all LPI timers.
 * It is clock rate of CSR slave port (APB clock[eqos_pclk] in eqos) minus 1
 * Current eqos_pclk is 204MHz
 */
#define OSI_LPI_1US_TIC_COUNTER_DEFAULT  0xCBU
#define OSI_LPI_1US_TIC_COUNTER_MASK     0xFFFU
/** @} */
#endif /* !OSI_STRIPPED_LIB */

#define OSI_PTP_REQ_CLK_FREQ  250000000U
#define OSI_POLL_COUNT        10000U
#ifndef UINT_MAX
/** Max value of uint */
#define UINT_MAX  (0xFFFFFFFFU)
#endif
#ifndef INT_MAX
#define INT_MAX  (0x7FFFFFFF)
#endif
#ifndef OSI_LLONG_MAX
#define OSI_LLONG_MAX  (0x7FFFFFFFFFFFFFFF)
#endif

/**
 * @addtogroup Generic helper MACROS
 *
 * @brief These are Generic helper macros used at various places.
 * @{
 */
#define OSI_UCHAR_MAX  (0xFFU)

/* Logging defines */
/* log levels */

#define OSI_LOG_INFO  1U
#ifndef OSI_STRIPPED_LIB
#define OSI_LOG_WARN  2U
#endif /* OSI_STRIPPED_LIB */
#define OSI_LOG_ERR  3U
/* Error types */
#define OSI_LOG_ARG_OUTOFBOUND  1U
#define OSI_LOG_ARG_INVALID     2U
#define OSI_LOG_ARG_HW_FAIL     4U
#define OSI_LOG_ARG_OPNOTSUPP   3U
/** Default maximum Giant Packet Size Limit is 16383 */
#define OSI_MAX_MTU_SIZE  16383U

/* MAC Tx/Rx Idle retry and delay count */
#define OSI_TXRX_IDLE_RETRY  5000U
#define OSI_DELAY_COUNT      10U

#define EQOS_DMA_CHX_STATUS(x)  ((0x0080U * (x)) + 0x1160U)
#define MGBE_DMA_CHX_STATUS(x)  ((0x0080U * (x)) + 0x3160U)
#define EQOS_DMA_CHX_IER(x)     ((0x0080U * (x)) + 0x1134U)

/* FIXME add logic based on HW version */

/**
 * @brief Maximum number of channels in EQOS
 */
#define OSI_EQOS_MAX_NUM_CHANS  8U
/** @brief Maximum number of queues in EQOS */
#define OSI_EQOS_MAX_NUM_QUEUES  8U
/** @brief Maximum number of L3L4 filters supported */
#define OSI_MGBE_MAX_L3_L4_FILTER  8U
/** @brief Maximum number of L3L4 filters supported for T264 */
#define OSI_MGBE_MAX_L3_L4_FILTER_T264  48U

/**
 * @brief Maximum number of channels in MGBE
 */
#ifdef FSI_EQOS_SUPPORT
#define OSI_MGBE_MAX_NUM_CHANS  10U
#else
#define OSI_MGBE_MAX_NUM_CHANS  48U
#endif

#define OSI_MGBE_T23X_MAX_NUM_CHANS  10U

/**
 * @brief Maximum number of PDMA channels in MGBE
 */
#define OSI_MGBE_MAX_NUM_PDMA_CHANS  10U
/** @brief Maximum number of queues in MGBE */
#define OSI_MGBE_MAX_NUM_QUEUES  10U
#define OSI_EQOS_XP_MAX_CHANS    4U
/* max riit DT configs for supported speeds */
#define OSI_MGBE_MAX_NUM_RIIT  4U

/**
 * @brief Maximum number of Secure Channels supported
 */
#define OSI_MACSEC_SC_INDEX_MAX  48

#ifndef OSI_STRIPPED_LIB
/* HW supports 8 Hash table regs, but eqos_validate_core_regs only checks 4 */
#define OSI_EQOS_MAX_HASH_REGS  4U
#endif /* OSI_STRIPPED_LIB */

#define MAC_VERSION             0x110
#define MAC_VERSION_SNVER_MASK  0x7FU

/** @brief flag indicating EQOS MAC */
#define OSI_MAC_HW_EQOS  0U
/** @brief flag indicating MGBE MAC */
#define OSI_MAC_HW_MGBE  1U
/** @brief flag indicating MGBE MAC on T26X */
#define OSI_MAC_HW_MGBE_T26X  2U

/** MAC version type for EQOS version previous to 5.30 */
#define MAC_CORE_VER_TYPE_EQOS  0U
/** MAC version type for EQOS version 5.30 */
#define MAC_CORE_VER_TYPE_EQOS_5_30  1U
/** MAC version type for MGBE IP */
#define MAC_CORE_VER_TYPE_MGBE  2U
/** MAC version type for EQOS version 5.40 */
#define MAC_CORE_VER_TYPE_EQOS_5_40  3U

#define OSI_NULL  ((void *)0)
/** Enable Flag */
#define OSI_ENABLE       1U
#define OSI_NONE         0U
#define OSI_NONE_SIGNED  0
/** Disable Flag */
#define OSI_DISABLE    0U
#define OSI_H_DISABLE  0x10101010U
#define OSI_H_ENABLE   (~OSI_H_DISABLE)

#define OSI_BIT(nr)     ((nveu32_t)1 << (((nveu32_t)nr) & 0x1FU))
#define OSI_BIT_64(nr)  ((nveu64_t)1 << (((nveu32_t)nr) & 0x3FU))

#ifndef OSI_STRIPPED_LIB
#define OSI_MGBE_MAC_3_00  0x30U
#define OSI_EQOS_MAC_4_10  0x41U
#define OSI_EQOS_MAC_5_10  0x51U
#define OSI_MGBE_MAC_4_00  0x40U
#endif /* OSI_STRIPPED_LIB */

/** @brief EQOS MAC version before Orin */
#define OSI_EQOS_MAC_5_00  0x50U
/** @brief EQOS MAC version Orin */
#define OSI_EQOS_MAC_5_30  0x53U
#define OSI_EQOS_MAC_5_40  0x54U
/** @brief MGBE MAC version Orin */
#define OSI_MGBE_MAC_3_10  0x31U
#define OSI_MGBE_MAC_3_20  0x32U
#define OSI_MGBE_MAC_4_20  0x42U

/**
 * @brief Maximum number of VM IRQs
 */
#define OSI_MAX_VM_IRQS  5U

#ifndef OSI_STRIPPED_LIB
#define OSI_HASH_FILTER_MODE     1U
#define OSI_L4_FILTER_TCP        0U
#define OSI_L4_FILTER_UDP        1U
#define OSI_PERFECT_FILTER_MODE  0U

#define OSI_INVALID_CHAN_NUM  0xFFU
#endif /* OSI_STRIPPED_LIB */
/** @} */

/**
 * @addtogroup OSI-DEBUG helper macros
 *
 * @brief OSI debug type macros
 * @{
 */
#ifdef OSI_DEBUG
#define OSI_DEBUG_TYPE_DESC     1U
#define OSI_DEBUG_TYPE_REG      2U
#define OSI_DEBUG_TYPE_STRUCTS  3U
#endif /* OSI_DEBUG */
/** @} */

/** \cond DO_NOT_DOCUMENT */

/**
 * @brief osi_memset - osi memset
 *
 * @param[out] s: source that need to be set
 * @param[in] c: value to fill in source
 * @param[in] count: first n bytes of source
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void
osi_memset (
  void      *s,
  nveu8_t   c,
  nveu64_t  count
  )
{
  nveu8_t   *xs = (nveu8_t *)s;
  nveu64_t  i   = 0UL;

  for (i = 0UL; i < count; i++) {
    xs[i] = c;
  }
}

/** \endcond */

/**
 * @brief unused function attribute
 */
#define OSI_UNUSED  __attribute__((__unused__))

/** @brief macro for 1 micro second delay */
#define OSI_DELAY_1US  1U

/**
 * @addtogroup MGBE PBL settings.
 *
 * @brief Values defined for PBL settings
 * @{
 */
/* Tx Queue size is 128KB */
#define MGBE_TXQ_SIZE  131072U
/* Rx Queue size is 192KB */
#define MGBE_RXQ_SIZE  196608U
/* uFPGA config Tx Queue size is 64KB */
#define MGBE_TXQ_SIZE_UFPGA  65536U

/* PBL values */
#define MGBE_DMA_CHX_MAX_PBL  32U
#define MGBE_DMA_CHX_PBL_16   16U
#define MGBE_DMA_CHX_PBL_8    8U
#define MGBE_DMA_CHX_PBL_4    4U
#define MGBE_DMA_CHX_PBL_1    1U
/* AXI Data width */
#define MGBE_AXI_DATAWIDTH  128U
/** @} */

/**
 * @brief MTL Q size depth helper macro
 */
#define Q_SZ_DEPTH(x)  (((x) * 1024U) / (MGBE_AXI_DATAWIDTH / 8U))

/**
 * @brief OSI PDMA to VDMA mapping data
 */
struct osi_pdma_vdma_data {
  /** PDMA channel */
  nveu32_t    pdma_chan;
  /** Number of VDMA channels */
  nveu32_t    num_vdma_chans;
  /** Array of VDMA channel list */
  nveu32_t    vdma_chans[OSI_MGBE_MAX_NUM_CHANS];
};

/** \cond DO_NOT_DOCUMENT */

/**
 * @brief osi_valid_pbl_value - returns the allowed pbl value.
 * @note
 * Algorithm:
 *  - Check the pbl range and return allowed pbl value
 *
 * @param[in] pbl_value: Calculated PBL value
 *
 * @note Input parameter should be only nveu32_t type
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval allowed pbl value
 */
static inline nveu32_t
osi_valid_pbl_value (
  nveu32_t  pbl_value
  )
{
  nveu32_t  allowed_pbl;
  nveu32_t  pbl;

  /* 8xPBL mode is set */
  pbl = pbl_value / 8U;

  if (pbl >= MGBE_DMA_CHX_MAX_PBL) {
    allowed_pbl = MGBE_DMA_CHX_MAX_PBL;
  } else if (pbl >= MGBE_DMA_CHX_PBL_16) {
    allowed_pbl = MGBE_DMA_CHX_PBL_16;
  } else if (pbl >= MGBE_DMA_CHX_PBL_8) {
    allowed_pbl = MGBE_DMA_CHX_PBL_8;
  } else if (pbl >= MGBE_DMA_CHX_PBL_4) {
    allowed_pbl = MGBE_DMA_CHX_PBL_4;
  } else {
    allowed_pbl = MGBE_DMA_CHX_PBL_1;
  }

  return allowed_pbl;
}

/** \endcond */

/**
 * @addtogroup PPS related information
 *
 * @brief PPS frequency configuration
 * @{
 */
/** Max PPS pulse supported */
#define OSI_MAX_PPS_HZ  8U
/** PPS_CMD Trigger delay 100 ms*/
#define OSI_PPS_TRIG_DELAY  100000000U
/** PPS train stop immediately */
#define OSI_PPS_START_CMD  2U
/** PPS train start after trigger time */
#define OSI_PPS_STOP_CMD  5U
/** @} */

#endif /* OSI_COMMON_H */
