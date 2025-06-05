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

#ifndef INCLUDED_CORE_COMMON_H
#define INCLUDED_CORE_COMMON_H

#include "core_local.h"

#ifndef OSI_STRIPPED_LIB
#define MAC_PFR_PR         OSI_BIT(0)
#define MAC_TCR_TSCFUPDT   OSI_BIT(1)
#define MAC_TCR_TSCTRLSSR  OSI_BIT(9)
#define MAC_PFR_PM         OSI_BIT(4)
#endif /* !OSI_STRIPPED_LIB */

#define MTL_EST_ADDR_SHIFT    8
#define MTL_EST_ADDR_MASK     (OSI_BIT(8) | OSI_BIT(9) |           \
                                         OSI_BIT(10) | OSI_BIT(11) | \
                                         OSI_BIT(12) | OSI_BIT(13) | \
                                         OSI_BIT(14) | OSI_BIT(15) | \
                                         OSI_BIT(16) | (17U) | \
                                         OSI_BIT(18) | OSI_BIT(19))
#define MTL_EST_SRWO          OSI_BIT(0)
#define MTL_EST_R1W0          OSI_BIT(1)
#define MTL_EST_GCRR          OSI_BIT(2)
#define MTL_EST_DBGM          OSI_BIT(4)
#define MTL_EST_DBGB          OSI_BIT(5)
#define MTL_EST_ERR0          OSI_BIT(20)
#define MTL_EST_CONTROL_EEST  OSI_BIT(0)
#define MTL_EST_STATUS_SWOL   OSI_BIT(7)
/* EST control OSI_BIT map */
#define MTL_EST_EEST          OSI_BIT(0)
#define MTL_EST_SSWL          OSI_BIT(1)
#define MTL_EST_QHLBF         OSI_BIT(3)
#define MTL_EST_CTR_HIGH_MAX  0xFFU
#define MTL_EST_ITRE_CGCE     OSI_BIT(4)
#define MTL_EST_ITRE_IEHS     OSI_BIT(3)
#define MTL_EST_ITRE_IEHF     OSI_BIT(2)
#define MTL_EST_ITRE_IEBE     OSI_BIT(1)
#define MTL_EST_ITRE_IECC     OSI_BIT(0)
/* MTL_FPE_CTRL_STS */
#define MTL_FPE_CTS_PEC        (OSI_BIT(8) | OSI_BIT(9) |          \
                                         OSI_BIT(10) | OSI_BIT(11) | \
                                         OSI_BIT(12) | OSI_BIT(13) | \
                                         OSI_BIT(14) | OSI_BIT(15))
#define MTL_FPE_CTS_PEC_SHIFT  8U
#define MAC_FPE_CTS_EFPE       OSI_BIT(0)
#define MAC_FPE_CTS_SVER       OSI_BIT(1)
/* MTL FPE adv registers */
#define MTL_FPE_ADV_HADV_MASK        (0xFFFFU)
#define MTL_FPE_ADV_HADV_VAL         100U
#define DMA_MODE_SWR                 OSI_BIT(0)
#define MTL_QTOMR_FTQ                OSI_BIT(0)
#define MTL_RXQ_OP_MODE_FEP          OSI_BIT(4)
#define MAC_TCR_TSINIT               OSI_BIT(2)
#define MAC_TCR_TSADDREG             OSI_BIT(5)
#define MAC_PPS_CTL_PPSCTRL0         (OSI_BIT(3) | OSI_BIT(2) |   \
                                        OSI_BIT(1) | OSI_BIT(0))
#define MAC_PPS_CTL_PPSEN0           OSI_BIT(4)
#define MAC_PPS_CTL_PPS_TRGTMODSEL0  (OSI_BIT(6) | OSI_BIT(5))
#define MAC_PPS_TT_NSEC_TRG_BUSY     OSI_BIT(31)
#define MAC_SSIR_SSINC_SHIFT         16U
#define MAC_PFR_DAIF                 OSI_BIT(3)
#define MAC_PFR_DBF                  OSI_BIT(5)
#define MAC_PFR_PCF                  (OSI_BIT(6) | OSI_BIT(7))
#define MAC_PFR_SAIF                 OSI_BIT(8)
#define MAC_PFR_SAF                  OSI_BIT(9)
#define MAC_PFR_HPF                  OSI_BIT(10)
#define MAC_PFR_VTFE                 OSI_BIT(16)
#define MAC_PFR_IPFE                 OSI_BIT(20)
#if !defined (L3L4_WILDCARD_FILTER)
#define MAC_PFR_IPFE_SHIFT  20U
#endif /* !L3L4_WILDCARD_FILTER */
#define MAC_PFR_DNTU  OSI_BIT(21)
#define MAC_PFR_RA    OSI_BIT(31)

#define WRAP_SYNC_TSC_PTP_CAPTURE  0x800CU
#define WRAP_TSC_CAPTURE_LOW       0x8010U
#define WRAP_TSC_CAPTURE_HIGH      0x8014U
#define WRAP_PTP_CAPTURE_LOW       0x8018U
#define WRAP_PTP_CAPTURE_HIGH      0x801CU
#define MAC_PKT_FILTER_REG         0x0008
#define HW_MAC_IER                 0x00B4U
#define WRAP_COMMON_INTR_ENABLE    0x8704U

/* common l3 l4 register bit fields for eqos and mgbe */
#ifndef OSI_STRIPPED_LIB
#define MAC_L3L4_CTR_L3PEN_SHIFT   0
#define MAC_L3L4_CTR_L3SAM_SHIFT   2
#define MAC_L3L4_CTR_L3SAIM_SHIFT  3
#endif /* !OSI_STRIPPED_LIB */
#define MAC_L3L4_CTR_L3DAM_SHIFT  4
#ifndef OSI_STRIPPED_LIB
#define MAC_L3L4_CTR_L3DAIM_SHIFT  5
#define MAC_L3L4_CTR_L4PEN_SHIFT   16
#define MAC_L3L4_CTR_L5TEN_SHIFT   17
#define MAC_L3L4_CTR_L4SPM_SHIFT   18
#define MAC_L3L4_CTR_L4SPIM_SHIFT  19
#define MAC_L3L4_CTR_L4DPM_SHIFT   20
#define MAC_L3L4_CTR_L4DPIM_SHIFT  21
#endif /* !OSI_STRIPPED_LIB */
#define MAC_L3L4_CTR_DMCHN_SHIFT        24
#define EQOS_MAC_L3L4_CTR_DMCHEN_SHIFT  28
#define MGBE_MAC_L3L4_CTR_DMCHEN_SHIFT  31
#define MAC_QX_TX_FLW_CTRL(x)  ((0x0004U * (x)) + 0x0070U)
#define MAC_QX_TX_FLW_CTRL_TFE  OSI_BIT(1)
#define MAC_PAUSE_TIME_MASK     0xFFFF0000U
#define MAC_PAUSE_TIME          0xFFFF0000U
#define MAC_RX_FLW_CTRL         0x0090
#define MAC_RX_FLW_CTRL_RFE     OSI_BIT(0)

/**
 * @addtogroup FPE HADV register value
 *
 * @brief Defines the supported speeds
 * and the corresponding HADV value for
 * hardware according to the ASIC recommendations.
 * The HADV value is a speed-dependent parameter.
 * The speed index is used to map the HADV
 * for hardware during initialization.
 *
 * @{
 */
#define OSI_SPEED_10_INX     0U
#define OSI_SPEED_100_INX    1U
#define OSI_SPEED_1000_INX   2U
#define OSI_SPEED_2500_INX   3U
#define OSI_SPEED_5000_INX   4U
#define OSI_SPEED_10000_INX  5U
#define OSI_SPEED_25000_INX  6U
#define OSI_SPEED_MAX_INX    7U

#define FPE_1G_HADV   0x380U
#define FPE_10G_HADV  0x59U
#define FPE_25G_HADV  0x23U
/** @} */

#ifdef HSI_SUPPORT

/**
 * @addtogroup MMC HW register offsets
 *
 * @brief MMC HW register offsets
 * @{
 */
#define EQOS_MMC_RXCRCERROR          0x00794
#define EQOS_MMC_RXIPV4_HDRERR_PKTS  0x00814
#define EQOS_MMC_RXIPV6_HDRERR_PKTS  0x00828
#define EQOS_MMC_RXUDP_ERR_PKTS      0x00834
#define EQOS_MMC_RXTCP_ERR_PKTS      0x0083c

#define MGBE_MMC_RXCRCERROR_L          0x00928
#define MGBE_MMC_RXIPV4_HDRERR_PKTS_L  0x00A6C
#define MGBE_MMC_RXIPV6_HDRERR_PKTS_L  0x00A94
#define MGBE_MMC_RXUDP_ERR_PKTS_L      0x00AAC
#define MGBE_MMC_RXTCP_ERR_PKTS_L      0x00ABC
/** @} */
#endif /* HSI_SUPPORT */

/**
 * @addtogroup typedef related info
 *
 * @brief typedefs that indeicates variable address and memory addr
 * @{
 */

struct est_read {
  /** variable pointer */
  nveu32_t    *var;
  /** memory register/address offset */
  nveu32_t    addr;
};

/** @} */
nve32_t
poll_check (
  struct osi_core_priv_data *const  osi_core,
  nveu8_t                           *addr,
  nveu32_t                          bit_check,
  nveu32_t                          *value
  );

nve32_t
hw_poll_for_swr (
  struct osi_core_priv_data *const  osi_core
  );

void
hw_start_mac (
  struct osi_core_priv_data *const  osi_core
  );

void
hw_stop_mac (
  struct osi_core_priv_data *const  osi_core
  );

nve32_t
hw_set_mode (
  struct osi_core_priv_data *const  osi_core,
  const nve32_t                     mode
  );

nve32_t
hw_set_speed (
  struct osi_core_priv_data *const  osi_core,
  const nve32_t                     speed
  );

nve32_t
hw_flush_mtl_tx_queue (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    q_inx
  );

nve32_t
hw_config_fw_err_pkts (
  struct osi_core_priv_data  *osi_core,
  const nveu32_t             q_inx,
  const nveu32_t             enable_fw_err_pkts
  );

nve32_t
hw_config_rxcsum_offload (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          enabled
  );

nve32_t
hw_set_systime_to_mac (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    sec,
  const nveu32_t                    nsec
  );

nve32_t
hw_config_addend (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    addend
  );

void
hw_config_tscr (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    ptp_filter
  );

void
hw_config_pps (
  struct osi_core_priv_data *const  osi_core
  );

void
hw_config_ssir (
  struct osi_core_priv_data *const  osi_core
  );

nve32_t
hw_ptp_tsc_capture (
  struct osi_core_priv_data *const  osi_core,
  struct osi_core_ptp_tsc_data      *data
  );

nve32_t
hw_config_mac_pkt_filter_reg (
  struct osi_core_priv_data *const  osi_core,
  const struct osi_filter           *filter
  );

#if !defined (L3L4_WILDCARD_FILTER)
void
hw_config_l3_l4_filter_enable (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    filter_enb_dis
  );

#endif /* !L3L4_WILDCARD_FILTER */
nve32_t
hw_config_est (
  struct osi_core_priv_data *const  osi_core,
  struct osi_est_config *const      est
  );

nve32_t
hw_config_fpe (
  struct osi_core_priv_data *const  osi_core,
  struct osi_fpe_config *const      fpe
  );

void
hw_tsn_init (
  struct osi_core_priv_data  *osi_core
  );

void  prepare_l3l4_registers (const struct osi_core_priv_data *const osi_core,
                              const struct osi_l3_l4_filter *const l3_l4,
#ifndef OSI_STRIPPED_LIB
                              nveu32_t *l3_addr0_reg,
                              nveu32_t *l3_addr2_reg,
                              nveu32_t *l3_addr3_reg,
                              nveu32_t *l4_addr_reg,
#endif /* !OSI_STRIPPED_LIB */
                              nveu32_t *l3_addr1_reg,
                              nveu32_t *ctr_reg);
#ifdef HSI_SUPPORT
  #ifdef NV_VLTEST_BUILD
nve32_t
hsi_common_error_inject (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   error_code
  );

  #endif
void
hsi_read_err (
  struct osi_core_priv_data *const  osi_core
  );

#endif
nve32_t
hw_validate_avb_input (
  struct osi_core_priv_data *const            osi_core,
  const struct osi_core_avb_algorithm *const  avb
  );

void
hw_config_flow_control (
  struct osi_core_priv_data *const  osi_core
  );

void
core_get_systime_from_mac (
  void      *addr,
  nveu32_t  mac,
  nveu32_t  *sec,
  nveu32_t  *nsec
  );

#endif /* INCLUDED_CORE_COMMON_H */
