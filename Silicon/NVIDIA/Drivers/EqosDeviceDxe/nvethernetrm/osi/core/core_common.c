// SPDX-License-Identifier: MIT

/* SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "common.h"
#include "core_common.h"
#include "mgbe_core.h"
#include "eqos_core.h"
#include "xpcs.h"
#include "osi_macsec.h"

nve32_t
poll_check (
  struct osi_core_priv_data *const  osi_core,
  nveu8_t                           *addr,
  nveu32_t                          bit_check,
  nveu32_t                          *value
  )
{
  nveu32_t  retry = OSI_POLL_COUNT;
  nve32_t   cond  = COND_NOT_MET;
  nveu32_t  count;
  nve32_t   ret = 0;

  /* Poll Until Poll Condition */
  count = 0;
  while (cond == COND_NOT_MET) {
    if (count > retry) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "poll_check: timeout\n",
        0ULL
        );
      ret = -1;
      goto fail;
    }

    count++;

    *value = osi_readla (osi_core, addr);
    if ((*value & bit_check) == OSI_NONE) {
      cond = COND_MET;
    } else {
      osi_core->osd_ops.udelay (OSI_DELAY_1US);
    }
  }

fail:
  return ret;
}

nve32_t
hw_poll_for_swr (
  struct osi_core_priv_data *const  osi_core
  )
{
  nveu32_t        dma_mode_val                   = 0U;
  const nveu32_t  dma_mode[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_DMA_BMR,
    MGBE_DMA_MODE,
    MGBE_DMA_MODE
  };
  void            *addr = osi_core->base;

  return poll_check (
           osi_core,
           ((nveu8_t *)addr + dma_mode[osi_core->mac]),
           DMA_MODE_SWR,
           &dma_mode_val
           );
}

void
hw_start_mac (
  struct osi_core_priv_data *const  osi_core
  )
{
  void            *addr = osi_core->base;
  nveu32_t        value;
  const nveu32_t  mac_mcr_te_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_MCR,
    MGBE_MAC_TMCR,
    MGBE_MAC_TMCR
  };
  const nveu32_t  mac_mcr_re_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_MCR,
    MGBE_MAC_RMCR,
    MGBE_MAC_RMCR
  };
  const nveu32_t  set_bit_te[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MCR_TE,
    MGBE_MAC_TMCR_TE,
    MGBE_MAC_TMCR_TE
  };
  const nveu32_t  set_bit_re[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MCR_RE,
    MGBE_MAC_RMCR_RE,
    MGBE_MAC_RMCR_RE
  };

  value  = osi_readla (osi_core, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));
  value |= set_bit_te[osi_core->mac];
  osi_writela (osi_core, value, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));

  value  = osi_readla (osi_core, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
  value |= set_bit_re[osi_core->mac];
  osi_writela (osi_core, value, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
}

void
hw_stop_mac (
  struct osi_core_priv_data *const  osi_core
  )
{
  void            *addr = osi_core->base;
  nveu32_t        value;
  const nveu32_t  mac_mcr_te_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_MCR,
    MGBE_MAC_TMCR,
    MGBE_MAC_TMCR
  };
  const nveu32_t  mac_mcr_re_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_MCR,
    MGBE_MAC_RMCR,
    MGBE_MAC_RMCR
  };
  const nveu32_t  clear_bit_te[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MCR_TE,
    MGBE_MAC_TMCR_TE,
    MGBE_MAC_TMCR_TE
  };
  const nveu32_t  clear_bit_re[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MCR_RE,
    MGBE_MAC_RMCR_RE,
    MGBE_MAC_RMCR_RE
  };

  value  = osi_readla (osi_core, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));
  value &= ~clear_bit_te[osi_core->mac];
  osi_writela (osi_core, value, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));

  value  = osi_readla (osi_core, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
  value &= ~clear_bit_re[osi_core->mac];
  osi_writela (osi_core, value, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
}

nve32_t
hw_set_mode (
  struct osi_core_priv_data *const  osi_core,
  const nve32_t                     mode
  )
{
  void            *base = osi_core->base;
  nveu32_t        mcr_val;
  nve32_t         ret          = 0;
  const nveu32_t  bit_set[2]   = { EQOS_MCR_DO, EQOS_MCR_DM };
  const nveu32_t  clear_bit[2] = { EQOS_MCR_DM, EQOS_MCR_DO };

 #ifndef OSI_STRIPPED_LIB
  /* don't allow only if mode is other than 0 or 1 */
  if ((mode != OSI_FULL_DUPLEX) && (mode != OSI_HALF_DUPLEX)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid duplex mode\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

 #endif /* !OSI_STRIPPED_LIB */

  if ((osi_core->mac == OSI_MAC_HW_EQOS) &&
      ((mode == OSI_FULL_DUPLEX) || (mode == OSI_HALF_DUPLEX)))
  {
    mcr_val  = osi_readla (osi_core, (nveu8_t *)base + EQOS_MAC_MCR);
    mcr_val |= bit_set[mode];
    mcr_val &= ~clear_bit[mode];
    osi_writela (osi_core, mcr_val, ((nveu8_t *)base + EQOS_MAC_MCR));
  }

 #ifndef OSI_STRIPPED_LIB
fail:
 #endif /* !OSI_STRIPPED_LIB */
  return ret;
}

#if 0
static nve32_t
xpcs_init_start (
  struct osi_core_priv_data *const  osi_core
  )
{
  nve32_t   ret = 0;
  nveu32_t  value;

  if (osi_core->mac == OSI_MAC_HW_MGBE) {
    if (osi_core->xpcs_base == OSI_NULL) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "XPCS base is NULL",
        0ULL
        );
      ret = -1;
      goto fail;
    }

    ret = xpcs_init (osi_core);
    if (ret < 0) {
      goto fail;
    }

    ret = xpcs_start (osi_core);
    if (ret < 0) {
      goto fail;
    }

    value = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
    /* Enable Link Status interrupt only after lane bring up success */
    value |= MGBE_IMR_RGSMIIIE;
    osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
  }

fail:
  return ret;
}

#endif

nve32_t
hw_set_speed (
  struct osi_core_priv_data *const  osi_core,
  const nve32_t                     speed
  )
{
  struct core_local  *l_core = (struct core_local *)(void *)osi_core;
  nveu32_t           value;
  nve32_t            ret                           = 0;
  void               *base                         = osi_core->base;
  const nveu32_t     mac_mcr[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_MCR,
    MGBE_MAC_TMCR,
    MGBE_MAC_TMCR
  };

  l_core->lane_status = OSI_DISABLE;

  if (((osi_core->mac == OSI_MAC_HW_EQOS) && (speed > OSI_SPEED_2500)) ||
      (((osi_core->mac == OSI_MAC_HW_MGBE) ||
        (osi_core->mac == OSI_MAC_HW_MGBE_T26X)) &&
       ((speed < OSI_SPEED_2500) && (speed > OSI_SPEED_25000))))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "unsupported speed\n",
      (nveul64_t)speed
      );
    ret = -1;
    goto fail;
  }

 #ifdef MACSEC_SUPPORT
  if ((osi_core->macsec_initialized == OSI_ENABLE) &&
      ((speed == OSI_SPEED_10) || (speed == OSI_SPEED_100)) &&
      ((osi_core->mac_ver == OSI_EQOS_MAC_5_40) || (osi_core->mac_ver == OSI_MGBE_MAC_4_20)))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "unsupported speed when T264 MACSec is enabled\n",
      (nveul64_t)speed
      );
    ret = -1;
    goto fail;
  }

 #endif /* MACSEC_SUPPORT */
  value = osi_readla (osi_core, ((nveu8_t *)base + mac_mcr[osi_core->mac]));
  switch (speed) {
 #ifndef OSI_STRIPPED_LIB
    case OSI_SPEED_10:
      value |= EQOS_MCR_PS;
      value &= ~EQOS_MCR_FES;
      break;
    case OSI_SPEED_100:
      value |= EQOS_MCR_PS;
      value |= EQOS_MCR_FES;
      break;
    case OSI_SPEED_2500:
      if (osi_core->mac == OSI_MAC_HW_EQOS) {
        value &= ~EQOS_MCR_PS;
        value |= EQOS_MCR_FES;
      } else {
        value |= MGBE_MAC_TMCR_SS_2_5G;
      }

      break;
 #endif /* !OSI_STRIPPED_LIB */
    case OSI_SPEED_1000:
      value &= ~EQOS_MCR_PS;
      value &= ~EQOS_MCR_FES;
      break;
    case OSI_SPEED_5000:
      value |= MGBE_MAC_TMCR_SS_5G;
      break;
    case OSI_SPEED_10000:
      value &= ~MGBE_MAC_TMCR_SS_10G;
      break;
    case OSI_SPEED_25000:
      value &= ~MGBE_MAC_TMCR_SS_10G;
      value |= MGBE_MAC_TMCR_SS_SPEED_25G;
      break;
    default:
      ret = -1;
      break;
  }

  if (ret != -1) {
    osi_writela (osi_core, value, ((nveu8_t *)osi_core->base + mac_mcr[osi_core->mac]));
    if (osi_core->mac != OSI_MAC_HW_EQOS) {
      if (speed == OSI_SPEED_25000) {
        ret = xlgpcs_init (osi_core);
        if (ret < 0) {
          goto fail;
        }

        ret = xlgpcs_start (osi_core);
        if (ret < 0) {
          goto fail;
        }
      } else {
        ret = xpcs_init (osi_core);
        if (ret < 0) {
          goto fail;
        }

        ret = xpcs_start (osi_core);
        if (ret < 0) {
          goto fail;
        }
      }

      value = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
      /* Enable Link Status interrupt only after lane bring up success */
      value |= MGBE_IMR_RGSMIIIE;
      osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
    } else if (osi_core->mac_ver == OSI_EQOS_MAC_5_40) {
      ret = eqos_xpcs_init (osi_core);
      if (ret < 0) {
        goto fail;
      }
    }
  }

  l_core->lane_status = OSI_ENABLE;
  osi_core->speed     = speed;
fail:
  return ret;
}

nve32_t
hw_flush_mtl_tx_queue (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    q_inx
  )
{
  void            *addr          = osi_core->base;
  nveu32_t        tx_op_mode_val = 0U;
  nveu32_t        que_idx        = (q_inx & 0xFU);
  nveu32_t        value;
  const nveu32_t  tx_op_mode[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_CHX_TX_OP_MODE (que_idx),
    MGBE_MTL_CHX_TX_OP_MODE (que_idx),
    MGBE_MTL_CHX_TX_OP_MODE (que_idx)
  };

  /* Read Tx Q Operating Mode Register and flush TxQ */
  value  = osi_readla (osi_core, ((nveu8_t *)addr + tx_op_mode[osi_core->mac]));
  value |= MTL_QTOMR_FTQ;
  osi_writela (osi_core, value, ((nveu8_t *)addr + tx_op_mode[osi_core->mac]));

  /* Poll Until FTQ bit resets for Successful Tx Q flush */
  return poll_check (
           osi_core,
           ((nveu8_t *)addr + tx_op_mode[osi_core->mac]),
           MTL_QTOMR_FTQ,
           &tx_op_mode_val
           );
}

nve32_t
hw_config_fw_err_pkts (
  struct osi_core_priv_data  *osi_core,
  const nveu32_t             q_inx,
  const nveu32_t             enable_fw_err_pkts
  )
{
  nveu32_t        val;
  nve32_t         ret                              = 0;
  nveu32_t        que_idx                          = (q_inx & 0xFU);
  const nveu32_t  rx_op_mode[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_CHX_RX_OP_MODE (que_idx),
    MGBE_MTL_CHX_RX_OP_MODE (que_idx),
    MGBE_MTL_CHX_RX_OP_MODE (que_idx)
  };

 #ifndef OSI_STRIPPED_LIB
  const nveu32_t  max_q[OSI_MAX_MAC_IP_TYPES] = {
    OSI_EQOS_MAX_NUM_QUEUES,
    OSI_MGBE_MAX_NUM_QUEUES,
    OSI_MGBE_MAX_NUM_QUEUES
  };
  /* Check for valid enable_fw_err_pkts and que_idx values */
  if (((enable_fw_err_pkts != OSI_ENABLE) &&
       (enable_fw_err_pkts != OSI_DISABLE)) ||
      (que_idx >= max_q[osi_core->mac]))
  {
    ret = -1;
    goto fail;
  }

  /* Read MTL RXQ Operation_Mode Register */
  val = osi_readla (
          osi_core,
          ((nveu8_t *)osi_core->base +
           rx_op_mode[osi_core->mac])
          );

  /* enable_fw_err_pkts, 1 is for enable and 0 is for disable */
  if (enable_fw_err_pkts == OSI_ENABLE) {
    /* When enable_fw_err_pkts bit is set, all packets except
     * the runt error packets are forwarded to the application
     * or DMA.
     */
    val |= MTL_RXQ_OP_MODE_FEP;
  } else {
    /* When this bit is reset, the Rx queue drops packets with error
     * status (CRC error, GMII_ER, watchdog timeout, or overflow)
     */
    val &= ~MTL_RXQ_OP_MODE_FEP;
  }

  /* Write to FEP bit of MTL RXQ Operation Mode Register to enable or
   * disable the forwarding of error packets to DMA or application.
   */
  osi_writela (
    osi_core,
    val,
    ((nveu8_t *)osi_core->base +
     rx_op_mode[osi_core->mac])
    );
fail:
  return ret;
 #else
  /* using void to skip the misra error of unused variable */
  (void)enable_fw_err_pkts;
  /* Read MTL RXQ Operation_Mode Register */
  val = osi_readla (
          osi_core,
          ((nveu8_t *)osi_core->base +
           rx_op_mode[osi_core->mac])
          );
  val |= MTL_RXQ_OP_MODE_FEP;

  /* Write to FEP bit of MTL RXQ Operation Mode Register to enable or
   * disable the forwarding of error packets to DMA or application.
   */
  osi_writela (
    osi_core,
    val,
    ((nveu8_t *)osi_core->base +
     rx_op_mode[osi_core->mac])
    );

  return ret;
 #endif /* !OSI_STRIPPED_LIB */
}

nve32_t
hw_config_rxcsum_offload (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          enabled
  )
{
  void            *addr = osi_core->base;
  nveu32_t        value;
  nve32_t         ret                               = 0;
  const nveu32_t  rxcsum_mode[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_MCR,
    MGBE_MAC_RMCR,
    MGBE_MAC_RMCR
  };
  const nveu32_t  ipc_value[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MCR_IPC,
    MGBE_MAC_RMCR_IPC,
    MGBE_MAC_RMCR_IPC
  };

  if ((enabled != OSI_ENABLE) && (enabled != OSI_DISABLE)) {
    ret = -1;
    goto fail;
  }

  value = osi_readla (osi_core, ((nveu8_t *)addr + rxcsum_mode[osi_core->mac]));
  if (enabled == OSI_ENABLE) {
    value |= ipc_value[osi_core->mac];
  } else {
    value &= ~ipc_value[osi_core->mac];
  }

  osi_writela (osi_core, value, ((nveu8_t *)addr + rxcsum_mode[osi_core->mac]));
fail:
  return ret;
}

nve32_t
hw_set_systime_to_mac (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    sec,
  const nveu32_t                    nsec
  )
{
  void            *addr                          = osi_core->base;
  nveu32_t        mac_tcr                        = 0U;
  nve32_t         ret                            = 0;
  const nveu32_t  mac_tscr[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_TCR,
    MGBE_MAC_TCR,
    MGBE_MAC_TCR
  };
  const nveu32_t  mac_stsur[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_STSUR,
    MGBE_MAC_STSUR,
    MGBE_MAC_STSUR
  };
  const nveu32_t  mac_stnsur[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_STNSUR,
    MGBE_MAC_STNSUR,
    MGBE_MAC_STNSUR
  };

  ret = poll_check (
          osi_core,
          ((nveu8_t *)addr + mac_tscr[osi_core->mac]),
          MAC_TCR_TSINIT,
          &mac_tcr
          );
  if (ret == -1) {
    goto fail;
  }

  /* write seconds value to MAC_System_Time_Seconds_Update register */
  osi_writela (osi_core, sec, ((nveu8_t *)addr + mac_stsur[osi_core->mac]));

  /* write nano seconds value to MAC_System_Time_Nanoseconds_Update
   * register
   */
  osi_writela (osi_core, nsec, ((nveu8_t *)addr + mac_stnsur[osi_core->mac]));

  /* issue command to update the configured secs and nsecs values */
  mac_tcr |= MAC_TCR_TSINIT;
  osi_writela (osi_core, mac_tcr, ((nveu8_t *)addr + mac_tscr[osi_core->mac]));

  ret = poll_check (
          osi_core,
          ((nveu8_t *)addr + mac_tscr[osi_core->mac]),
          MAC_TCR_TSINIT,
          &mac_tcr
          );
fail:
  return ret;
}

nve32_t
hw_config_addend (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    addend
  )
{
  void            *addr                          = osi_core->base;
  nveu32_t        mac_tcr                        = 0U;
  nve32_t         ret                            = 0;
  const nveu32_t  mac_tscr[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_TCR,
    MGBE_MAC_TCR,
    MGBE_MAC_TCR
  };
  const nveu32_t  mac_tar[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_TAR,
    MGBE_MAC_TAR,
    MGBE_MAC_TAR
  };

  ret = poll_check (
          osi_core,
          ((nveu8_t *)addr + mac_tscr[osi_core->mac]),
          MAC_TCR_TSADDREG,
          &mac_tcr
          );
  if (ret == -1) {
    goto fail;
  }

  /* write addend value to MAC_Timestamp_Addend register */
  osi_writela (osi_core, addend, ((nveu8_t *)addr + mac_tar[osi_core->mac]));

  /* issue command to update the configured addend value */
  mac_tcr |= MAC_TCR_TSADDREG;
  osi_writela (osi_core, mac_tcr, ((nveu8_t *)addr + mac_tscr[osi_core->mac]));

  ret = poll_check (
          osi_core,
          ((nveu8_t *)addr + mac_tscr[osi_core->mac]),
          MAC_TCR_TSADDREG,
          &mac_tcr
          );
fail:
  return ret;
}

void
hw_config_pps (
  struct osi_core_priv_data *const  osi_core
  )
{
  const nveu32_t     mac_pps_tt_nsec[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_PPS_TT_NSEC,
    MGBE_MAC_PPS_TT_NSEC,
    MGBE_MAC_PPS_TT_NSEC
  };
  const nveu32_t     mac_pps_tt_sec[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_PPS_TT_SEC,
    MGBE_MAC_PPS_TT_SEC,
    MGBE_MAC_PPS_TT_SEC
  };
  const nveu32_t     mac_pps_interval[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_PPS_INTERVAL,
    MGBE_MAC_PPS_INTERVAL,
    MGBE_MAC_PPS_INTERVAL
  };
  const nveu32_t     mac_pps_width[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_PPS_WIDTH,
    MGBE_MAC_PPS_WIDTH,
    MGBE_MAC_PPS_WIDTH
  };
  const nveu32_t     mac_pps[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_PPS_CTL,
    MGBE_MAC_PPS_CTL,
    MGBE_MAC_PPS_CTL
  };
  void               *addr     = osi_core->base;
  struct core_local  *l_core   = (struct core_local *)(void *)osi_core;
  nveul64_t          temp      = 0U;
  nveu32_t           value     = 0x0U;
  nveu32_t           interval  = 0U;
  nveu32_t           width     = 0U;
  nveu32_t           sec       = 0U;
  nveu32_t           nsec      = 0U;
  nveu32_t           ssinc_val = OSI_PTP_SSINC_4;
  nve32_t            ret       = 0;

  if (l_core->pps_freq > OSI_ENABLE) {
    // PPS_CMD related code
    if (osi_core->mac_ver == OSI_EQOS_MAC_5_30) {
      ssinc_val = OSI_PTP_SSINC_6;
    }

    value  = osi_readla (osi_core, (nveu8_t *)addr + mac_pps[osi_core->mac]);
    value &= ~MAC_PPS_CTL_PPSCTRL0;
    value |= MAC_PPS_CTL_PPSEN0;             // set enable bit
    /* Set mode to 0b'10 for with interrupt, 0b'11 for non interrupt */
    value |= MAC_PPS_CTL_PPS_TRGTMODSEL0;

    /* If want to stop all ready running the pps train we need to write b'0101
     * in mac_pps[osi_core->mac])
     */
    value |= OSI_PPS_STOP_CMD;
    osi_writela (osi_core, value, ((nveu8_t *)addr + mac_pps[osi_core->mac]));

    /*
     * nvidia,pps_op_ctl  = 0  – 1Hz (pps fixed mode)
     * nvidia,pps_op_ctl  = 1  – 1Hz (pps fixed mode, 2 Edges)
     * nvidia,pps_op_ctl  = x – x Hz ( pps CMD by programming width and interval)
     */
    temp = OSI_NSEC_PER_SEC / ((nveul64_t)l_core->pps_freq * (nveul64_t)ssinc_val);
    if (temp <= UINT_MAX) {
      interval = (nveu32_t)temp;
      width    = (interval / 2U);
    }

    /* Target time programming */
    ret = poll_check (
            osi_core,
            ((nveu8_t *)addr + mac_pps_tt_nsec[osi_core->mac]),
            MAC_PPS_TT_NSEC_TRG_BUSY,
            &value
            );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Not able to program PPS trigger time\n",
        (nveul64_t)value
        );
      goto error;
    }

    core_get_systime_from_mac (osi_core->base, osi_core->mac, &sec, &nsec);

    if ((OSI_NSEC_PER_SEC_U - 100000000U) > nsec) {
      nsec += 100000000U;                   // Trigger PPS train after 100ms
    } else if (sec < UINT_MAX) {
      sec += 1U;
      nsec = nsec - OSI_NSEC_PER_SEC_U + OSI_PPS_TRIG_DELAY;
    } else {
      /* Do nothing */
    }

    osi_writela (osi_core, sec, ((nveu8_t *)addr + mac_pps_tt_sec[osi_core->mac]));
    osi_writela (osi_core, nsec, ((nveu8_t *)addr + mac_pps_tt_nsec[osi_core->mac]));

    /* interval programming */
    if (interval >= 1U) {
      osi_writela (
        osi_core,
        (interval - 1U),
        ((nveu8_t *)addr + mac_pps_interval[osi_core->mac])
        );
    }

    /* width programming */
    if (width >= 1U) {
      osi_writela (
        osi_core,
        (width - 1U),
        ((nveu8_t *)addr + mac_pps_width[osi_core->mac])
        );
    }
  }

error:
  value  = osi_readla (osi_core, (nveu8_t *)addr + mac_pps[osi_core->mac]);
  value &= ~MAC_PPS_CTL_PPSCTRL0;
  if (ret < 0) {
    value &= ~MAC_PPS_CTL_PPSEN0;
  } else if (l_core->pps_freq == OSI_ENABLE) {
    value &= ~MAC_PPS_CTL_PPSEN0;
    value |= OSI_ENABLE;            // Fixed PPS
  } else if (l_core->pps_freq > OSI_ENABLE) {
    value |= OSI_PPS_START_CMD;             // 0b'10 start after TT. PPS_CMD
  } else {
    value &= ~MAC_PPS_CTL_PPSEN0;
  }

  osi_writela (osi_core, value, ((nveu8_t *)addr + mac_pps[osi_core->mac]));

  return;
}

#ifndef OSI_STRIPPED_LIB
void
hw_config_tscr (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    ptp_filter
  )
#else
void
hw_config_tscr (
  struct osi_core_priv_data *const  osi_core,
  OSI_UNUSED const nveu32_t         ptp_filter
  )
#endif /* !OSI_STRIPPED_LIB */
{
  void      *addr   = osi_core->base;
  nveu32_t  mac_tcr = 0U;

 #ifndef OSI_STRIPPED_LIB
  nveu32_t  i = 0U, temp = 0U;
 #endif /* !OSI_STRIPPED_LIB */

  const nveu32_t  mac_tscr[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_TCR,
    MGBE_MAC_TCR,
    MGBE_MAC_TCR
  };

  (void)ptp_filter;       // unused

 #ifndef OSI_STRIPPED_LIB
  if (ptp_filter != OSI_DISABLE) {
    mac_tcr = (OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT | OSI_MAC_TCR_TSCTRLSSR);
    for (i = 0U; i < 32U; i++) {
      temp = ptp_filter & OSI_BIT (i);

      switch (temp) {
        case OSI_MAC_TCR_SNAPTYPSEL_1:
          mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_1;
          break;
        case OSI_MAC_TCR_SNAPTYPSEL_2:
          mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_2;
          break;
        case OSI_MAC_TCR_SNAPTYPSEL_3:
          mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_3;
          break;
        case OSI_MAC_TCR_TSIPV4ENA:
          mac_tcr |= OSI_MAC_TCR_TSIPV4ENA;
          break;
        case OSI_MAC_TCR_TSIPV6ENA:
          mac_tcr |= OSI_MAC_TCR_TSIPV6ENA;
          break;
        case OSI_MAC_TCR_TSEVENTENA:
          mac_tcr |= OSI_MAC_TCR_TSEVENTENA;
          break;
        case OSI_MAC_TCR_TSMASTERENA:
          mac_tcr |= OSI_MAC_TCR_TSMASTERENA;
          break;
        case OSI_MAC_TCR_TSVER2ENA:
          mac_tcr |= OSI_MAC_TCR_TSVER2ENA;
          break;
        case OSI_MAC_TCR_TSIPENA:
          mac_tcr |= OSI_MAC_TCR_TSIPENA;
          break;
        case OSI_MAC_TCR_AV8021ASMEN:
          mac_tcr |= OSI_MAC_TCR_AV8021ASMEN;
          break;
        case OSI_MAC_TCR_TSENALL:
          mac_tcr |= OSI_MAC_TCR_TSENALL;
          break;
        case OSI_MAC_TCR_CSC:
          mac_tcr |= OSI_MAC_TCR_CSC;
          break;
        default:
          /* misra */
          break;
      }
    }
  } else {
    /* Disabling the MAC time stamping */
    mac_tcr = OSI_DISABLE;
  }

 #else
  mac_tcr = (OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT | OSI_MAC_TCR_TSCTRLSSR
             | OSI_MAC_TCR_TSVER2ENA | OSI_MAC_TCR_TSIPENA | OSI_MAC_TCR_TSIPV6ENA |
             OSI_MAC_TCR_TSIPV4ENA | OSI_MAC_TCR_SNAPTYPSEL_1);
 #endif /* !OSI_STRIPPED_LIB */

  osi_writela (osi_core, mac_tcr, ((nveu8_t *)addr + mac_tscr[osi_core->mac]));

  return;
}

void
hw_config_ssir (
  struct osi_core_priv_data *const  osi_core
  )
{
  nveu32_t                 val                            = 0U;
  void                     *addr                          = osi_core->base;
  const struct core_local  *l_core                        = (struct core_local *)(void *)osi_core;
  const nveu32_t           mac_ssir[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_SSIR,
    MGBE_MAC_SSIR,
    MGBE_MAC_SSIR
  };
  const nveu32_t           ptp_ssinc[3] = { OSI_PTP_SSINC_4, OSI_PTP_SSINC_6, OSI_PTP_SSINC_4 };

  /* by default Fine method is enabled */
  val = ptp_ssinc[l_core->l_mac_ver];
  /* EQOS T234 SSINC is different from EOQS T264, Logic added for EQOS T264 */
  if (osi_core->mac_ver == OSI_EQOS_MAC_5_40) {
    val = OSI_PTP_SSINC_4;
  }

  val |= val << MAC_SSIR_SSINC_SHIFT;
  /* update Sub-second Increment Value */
  osi_writela (osi_core, val, ((nveu8_t *)addr + mac_ssir[osi_core->mac]));
}

nve32_t
hw_ptp_tsc_capture (
  struct osi_core_priv_data *const  osi_core,
  struct osi_core_ptp_tsc_data      *data
  )
{
 #ifndef OSI_STRIPPED_LIB
  const struct core_local  *l_core = (struct core_local *)osi_core;
 #endif /* !OSI_STRIPPED_LIB */
  void      *addr   = osi_core->base;
  nveu32_t  tsc_ptp = 0U;
  nve32_t   ret     = 0;

 #ifndef OSI_STRIPPED_LIB
  /* This code is NA for Orin use case */
  if (l_core->l_mac_ver < MAC_CORE_VER_TYPE_EQOS_5_30) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "ptp_tsc: older IP\n",
      0ULL
      );
    ret = -1;
    goto done;
  }

 #endif /* !OSI_STRIPPED_LIB */

  osi_writela (osi_core, OSI_ENABLE, (nveu8_t *)osi_core->base + WRAP_SYNC_TSC_PTP_CAPTURE);

  ret = poll_check (
          osi_core,
          ((nveu8_t *)addr + WRAP_SYNC_TSC_PTP_CAPTURE),
          OSI_ENABLE,
          &tsc_ptp
          );
  if (ret == -1) {
    goto done;
  }

  data->tsc_low_bits = osi_readla (
                         osi_core,
                         (nveu8_t *)osi_core->base +
                         WRAP_TSC_CAPTURE_LOW
                         );
  data->tsc_high_bits =  osi_readla (
                           osi_core,
                           (nveu8_t *)osi_core->base +
                           WRAP_TSC_CAPTURE_HIGH
                           );
  data->ptp_low_bits =  osi_readla (
                          osi_core,
                          (nveu8_t *)osi_core->base +
                          WRAP_PTP_CAPTURE_LOW
                          );
  data->ptp_high_bits =  osi_readla (
                           osi_core,
                           (nveu8_t *)osi_core->base +
                           WRAP_PTP_CAPTURE_HIGH
                           );
done:
  return ret;
}

#ifndef OSI_STRIPPED_LIB
static inline void
config_l2_da_perfect_inverse_match (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   perfect_inverse_match
  )
{
  nveu32_t  value = 0U;

  value  = osi_readla (osi_core, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));
  value &= ~MAC_PFR_DAIF;
  if (perfect_inverse_match == OSI_INV_MATCH) {
    /* Set DA Inverse Filtering */
    value |= MAC_PFR_DAIF;
  }

  osi_writela (osi_core, value, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));
}

#endif /* !OSI_STRIPPED_LIB */

nve32_t
hw_config_mac_pkt_filter_reg (
  struct osi_core_priv_data *const  osi_core,
  const struct osi_filter           *filter
  )
{
  nveu32_t  value = 0U;
  nve32_t   ret   = 0;

  value = osi_readla (osi_core, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));

  if ((filter->oper_mode & OSI_OPER_EN_PERFECT) != OSI_DISABLE) {
    value |= MAC_PFR_HPF;
  }

 #ifndef OSI_STRIPPED_LIB
  if ((filter->oper_mode & OSI_OPER_DIS_PERFECT) != OSI_DISABLE) {
    value &= ~MAC_PFR_HPF;
  }

  if ((filter->oper_mode & OSI_OPER_EN_PROMISC) != OSI_DISABLE) {
    value |= MAC_PFR_PR;
  }

  if ((filter->oper_mode & OSI_OPER_DIS_PROMISC) != OSI_DISABLE) {
    value &= ~MAC_PFR_PR;
  }

  if ((filter->oper_mode & OSI_OPER_EN_ALLMULTI) != OSI_DISABLE) {
    value |= MAC_PFR_PM;
  }

  if ((filter->oper_mode & OSI_OPER_DIS_ALLMULTI) != OSI_DISABLE) {
    value &= ~MAC_PFR_PM;
  }

 #endif /* !OSI_STRIPPED_LIB */

  osi_writela (
    osi_core,
    value,
    ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG)
    );

 #ifndef OSI_STRIPPED_LIB
  if ((filter->oper_mode & OSI_OPER_EN_L2_DA_INV) != OSI_DISABLE) {
    config_l2_da_perfect_inverse_match (osi_core, OSI_INV_MATCH);
  }

  if ((filter->oper_mode & OSI_OPER_DIS_L2_DA_INV) != OSI_DISABLE) {
    config_l2_da_perfect_inverse_match (osi_core, OSI_PFT_MATCH);
  }

 #else
  value  = osi_readla (osi_core, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));
  value &= ~MAC_PFR_DAIF;
  osi_writela (osi_core, value, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));

 #endif /* !OSI_STRIPPED_LIB */

  return ret;
}

#if !defined (L3L4_WILDCARD_FILTER)
void
hw_config_l3_l4_filter_enable (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    filter_enb_dis
  )
{
  nveu32_t  value = 0U;
  void      *base = osi_core->base;

  value  = osi_readla (osi_core, ((nveu8_t *)base + MAC_PKT_FILTER_REG));
  value &= ~(MAC_PFR_IPFE);
  value |= ((filter_enb_dis << MAC_PFR_IPFE_SHIFT) & MAC_PFR_IPFE);
  osi_writela (osi_core, value, ((nveu8_t *)base + MAC_PKT_FILTER_REG));
}

#endif /* !L3L4_WILDCARD_FILTER */

/**
 * @brief hw_est_read - indirect read the GCL to Software own list
 * (SWOL)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] addr_val: Address offset for indirect write.
 * @param[in] data: Data to be written at offset.
 * @param[in] gcla: Gate Control List Address, 0 for ETS register.
 *            1 for GCL memory.
 * @param[in] bunk: Memory bunk from which vlaues will be read. Possible
 *            value 0 or 1.
 * @param[in] mac: MAC index
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t
hw_est_read (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   addr_val,
  nveu32_t                   *data,
  OSI_UNUSED nveu32_t        gcla,
  nveu32_t                   bunk,
  nveu32_t                   mac
  )
{
  /* 1 busy wait, and the remaining retries are sleeps of granularity MIN_USLEEP_10US */
  nveu32_t        retry = (RETRY_COUNT / MIN_USLEEP_10US) + 1U;
  nveu32_t        once  = 0U;
  nveu32_t        val   = 0U;
  nve32_t         ret;
  const nveu32_t  MTL_EST_GCL_CONTROL[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_GCL_CONTROL,
    MGBE_MTL_EST_GCL_CONTROL,
    MGBE_MTL_EST_GCL_CONTROL
  };
  const nveu32_t  MTL_EST_DATA[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_DATA,
    MGBE_MTL_EST_DATA,
    MGBE_MTL_EST_DATA
  };

  (void)gcla;

  *data = 0U;
  val  &= ~MTL_EST_ADDR_MASK;
  val  |= MTL_EST_GCRR;
  val  |= MTL_EST_SRWO | MTL_EST_R1W0 | MTL_EST_DBGM | bunk | addr_val;
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->base +
    MTL_EST_GCL_CONTROL[mac]
    );

  while (retry > 0U) {
    retry--;
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MTL_EST_GCL_CONTROL[mac]
            );
    if ((val & MTL_EST_SRWO) == MTL_EST_SRWO) {
      if (once == 0U) {
        osi_core->osd_ops.udelay (OSI_DELAY_1US);

        /* udelay is a busy wait, so don't call it too frequently.
         * call it once to be optimistic, and then use usleep
         * with a longer timeout to yield to other CPU users.
         */
        once = 1U;
      } else {
        osi_core->osd_ops.usleep (MIN_USLEEP_10US);
      }

      continue;
    }

    break;
  }

  if (((val & MTL_EST_ERR0) == MTL_EST_ERR0) ||
      (retry <= 0U))
  {
    ret = -1;
    goto err;
  }

  *data = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MTL_EST_DATA[mac]
            );
  ret = 0;
err:
  return ret;
}

static nve32_t
validate_est_args (
  struct osi_core_priv_data *const  osi_core,
  struct osi_est_config *const      est
  )
{
  nve32_t                  ret     = 0;
  const struct core_local  *l_core = (struct core_local *)(void *)osi_core;

  if (est->en_dis > OSI_ENABLE) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "input argument en_dis value\n",
      (nveul64_t)est->en_dis
      );
    ret = -1;
    goto done;
  }

  if ((est->llr > l_core->gcl_dep) || (est->llr == OSI_NONE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "input argument more than GCL depth\n",
      (nveul64_t)est->llr
      );
    ret = -1;
    goto done;
  }

  /* 24 bit configure time in GCL + 7) */
  if (est->ter > 0x7FFFFFFFU) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "invalid TER value\n",
      (nveul64_t)est->ter
      );
    ret = -1;
    goto done;
  }

  /* nenosec register value can't be more than 10^9 nsec */
  if ((est->ctr[0] > OSI_NSEC_PER_SEC) ||
      (est->btr[0] > OSI_NSEC_PER_SEC) ||
      (est->ctr[1] > 0xFFU))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "input argument CTR/BTR nsec is invalid\n",
      0ULL
      );
    ret = -1;
    goto done;
  }

  /* if btr + offset is more than limit */
  if ((est->btr[0] > (OSI_NSEC_PER_SEC - est->btr_offset[0])) ||
      (est->btr[1] > (UINT_MAX - est->btr_offset[1])))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "input argument BTR offset is invalid\n",
      0ULL
      );
    ret = -1;
    goto done;
  }

done:
  return ret;
}

static nve32_t
validate_btr (
  struct osi_core_priv_data *const  osi_core,
  struct osi_est_config *const      est,
  const nveu32_t                    *btr,
  nveu32_t                          mac,
  nveu32_t                          bunk
  )
{
  nveu32_t               i;
  nve32_t                ret = 0;
  nveu32_t               val = 0U;
  nveu64_t               rem = 0U;
  nveu64_t               btr_new = 0U;
  nveu64_t               old_btr, old_ctr;
  nveu32_t               btr_l, btr_h, ctr_l, ctr_h;
  const nveu32_t         MTL_EST_CONTROL[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL,
    MGBE_MTL_EST_CONTROL,
    MGBE_MTL_EST_CONTROL
  };
  const nveu32_t         PTP_CYCLE_8[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_8PTP_CYCLE,
    MGBE_8PTP_CYCLE,
    MGBE_8PTP_CYCLE
  };
  const nveu32_t         MTL_EST_BTR_LOW[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_BTR_LOW,
    MGBE_MTL_EST_BTR_LOW,
    MGBE_MTL_EST_BTR_LOW
  };
  const nveu32_t         MTL_EST_BTR_HIGH[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_BTR_HIGH,
    MGBE_MTL_EST_BTR_HIGH,
    MGBE_MTL_EST_BTR_HIGH
  };
  const nveu32_t         MTL_EST_CTR_LOW[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CTR_LOW,
    MGBE_MTL_EST_CTR_LOW,
    MGBE_MTL_EST_CTR_LOW
  };
  const nveu32_t         MTL_EST_CTR_HIGH[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CTR_HIGH,
    MGBE_MTL_EST_CTR_HIGH,
    MGBE_MTL_EST_CTR_HIGH
  };
  const struct est_read  hw_read_arr[4] = {
    { &btr_l, MTL_EST_BTR_LOW[mac]  },
    { &btr_h, MTL_EST_BTR_HIGH[mac] },
    { &ctr_l, MTL_EST_CTR_LOW[mac]  },
    { &ctr_h, MTL_EST_CTR_HIGH[mac] }
  };

  btr_new = (((nveu64_t)btr[1] + est->btr_offset[1]) * OSI_NSEC_PER_SEC) +
            (btr[0] + est->btr_offset[0]);
  /* Check for BTR in case of new ETS while current GCL enabled */

  val = osi_readla (
          osi_core,
          (nveu8_t *)osi_core->base +
          MTL_EST_CONTROL[mac]
          );
  if ((val & MTL_EST_CONTROL_EEST) != MTL_EST_CONTROL_EEST) {
    ret = 0;
    goto done;
  }

  /* Read last BTR and CTR */
  for (i = 0U; i < (sizeof (hw_read_arr) / sizeof (hw_read_arr[0])); i++) {
    ret = hw_est_read (
            osi_core,
            hw_read_arr[i].addr,
            hw_read_arr[i].var,
            OSI_DISABLE,
            bunk,
            mac
            );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "Reading failed for index\n",
        (nveul64_t)i
        );
      goto done;
    }
  }

  old_btr = btr_l + ((nveu64_t)btr_h * OSI_NSEC_PER_SEC);
  old_ctr = ctr_l + ((nveu64_t)ctr_h * OSI_NSEC_PER_SEC);
  if (old_btr > btr_new) {
    rem = (old_btr - btr_new) % old_ctr;
    if ((rem != OSI_NONE) && (rem < PTP_CYCLE_8[mac])) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "invalid BTR",
        (nveul64_t)rem
        );
      ret = -1;
      goto done;
    }
  } else if (btr_new > old_btr) {
    rem = (btr_new - old_btr) % old_ctr;
    if ((rem != OSI_NONE) && (rem < PTP_CYCLE_8[mac])) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "invalid BTR",
        (nveul64_t)rem
        );
      ret = -1;
      goto done;
    }
  } else {
    // Nothing to do
  }

done:
  return ret;
}

/**
 * @brief eqos_gcl_validate - validate GCL from user
 *
 * Algorithm: validate GCL size and width of time interval value
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: Configuration input argument.
 * @param[in] btr: Base time register value.
 * @param[in] mac: MAC index
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
gcl_validate (
  struct osi_core_priv_data *const  osi_core,
  struct osi_est_config *const      est,
  const nveu32_t                    *btr,
  nveu32_t                          mac
  )
{
  const struct core_local  *l_core                           = (struct core_local *)(void *)osi_core;
  const nveu32_t           PTP_CYCLE_8[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_8PTP_CYCLE,
    MGBE_8PTP_CYCLE,
    MGBE_8PTP_CYCLE
  };
  const nveu32_t           MTL_EST_STATUS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_STATUS,
    MGBE_MTL_EST_STATUS,
    MGBE_MTL_EST_STATUS
  };
  nveu32_t                 i;
  nveu64_t                 sum_ti  = 0U;
  nveu64_t                 sum_tin = 0U;
  nveu64_t                 ctr     = 0U;
  nveu32_t                 bunk    = 0U;
  nveu32_t                 est_status;
  nve32_t                  ret = 0;

  if ((est->btr_offset[0] > OSI_NSEC_PER_SEC) ||
      (validate_est_args (osi_core, est) < 0))
  {
    ret = -1;
    goto done;
  }

  ctr = ((nveu64_t)est->ctr[1] * OSI_NSEC_PER_SEC)  + est->ctr[0];
  for (i = 0U; i < est->llr; i++) {
    if (est->gcl[i] <= l_core->gcl_width_val) {
      sum_ti += ((nveu64_t)est->gcl[i] & l_core->ti_mask);
      if ((sum_ti > ctr) &&
          ((ctr - sum_tin) >= PTP_CYCLE_8[mac]))
      {
        continue;
      } else if (((ctr - sum_ti) != 0U) &&
                 ((ctr - sum_ti) < PTP_CYCLE_8[mac]))
      {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_INVALID,
          "CTR issue due to trancate\n",
          (nveul64_t)i
          );
        ret = -1;
        goto done;
      } else {
        // do nothing
      }

      sum_tin = sum_ti;
      continue;
    }

    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "validation of GCL entry failed\n",
      (nveul64_t)i
      );
    ret = -1;
    goto done;
  }

  /* Read EST_STATUS for bunk */
  est_status = osi_readla (
                 osi_core,
                 (nveu8_t *)osi_core->base +
                 MTL_EST_STATUS[mac]
                 );
  if ((est_status & MTL_EST_STATUS_SWOL) == 0U) {
    bunk = MTL_EST_DBGB;
  }

  ret = validate_btr (osi_core, est, btr, mac, bunk);

done:
  return ret;
}

/**
 * @brief hw_est_write - indirect write the GCL to Software own list
 * (SWOL)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] addr_val: Address offset for indirect write.
 * @param[in] data: Data to be written at offset.
 * @param[in] gcla: Gate Control List Address, 0 for ETS register.
 *            1 for GCL memory.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
hw_est_write (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   addr_val,
  nveu32_t                   data,
  nveu32_t                   gcla
  )
{
  /* 1 busy wait, and the remaining retries are sleeps of granularity MIN_USLEEP_10US */
  nveu32_t        retry                              = (RETRY_COUNT / MIN_USLEEP_10US) + 1U;
  nveu32_t        once                               = 0U;
  nveu32_t        val                                = 0x0;
  nve32_t         ret                                = 0;
  const nveu32_t  MTL_EST_DATA[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_DATA,
    MGBE_MTL_EST_DATA,
    MGBE_MTL_EST_DATA
  };
  const nveu32_t  MTL_EST_GCL_CONTROL[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_GCL_CONTROL,
    MGBE_MTL_EST_GCL_CONTROL,
    MGBE_MTL_EST_GCL_CONTROL
  };

  osi_writela (
    osi_core,
    data,
    (nveu8_t *)osi_core->base +
    MTL_EST_DATA[osi_core->mac]
    );

  val &= ~MTL_EST_ADDR_MASK;
  val |= (gcla == 1U) ? 0x0U : MTL_EST_GCRR;
  val |= MTL_EST_SRWO;
  val |= addr_val;
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->base +
    MTL_EST_GCL_CONTROL[osi_core->mac]
    );

  while (retry > 0U) {
    retry--;
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MTL_EST_GCL_CONTROL[osi_core->mac]
            );
    if ((val & MTL_EST_SRWO) == MTL_EST_SRWO) {
      if (once == 0U) {
        osi_core->osd_ops.udelay (OSI_DELAY_1US);

        /* udelay is a busy wait, so don't call it too frequently.
         * call it once to be optimistic, and then use usleep
         * with a longer timeout to yield to other CPU users.
         */
        once = 1U;
      } else {
        osi_core->osd_ops.usleep (MIN_USLEEP_10US);
      }

      continue;
    }

    break;
  }

  if (((val & MTL_EST_ERR0) == MTL_EST_ERR0) ||
      (retry <= 0U))
  {
    ret = -1;
  }

  return ret;
}

static inline nve32_t
configure_est_params (
  struct osi_core_priv_data *const  osi_core,
  struct osi_est_config *const      est
  )
{
  nveu32_t        i;
  nve32_t         ret;
  nveu32_t        addr                                  = 0x0;
  const nveu32_t  MTL_EST_CTR_LOW[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CTR_LOW,
    MGBE_MTL_EST_CTR_LOW,
    MGBE_MTL_EST_CTR_LOW
  };
  const nveu32_t  MTL_EST_CTR_HIGH[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CTR_HIGH,
    MGBE_MTL_EST_CTR_HIGH,
    MGBE_MTL_EST_CTR_HIGH
  };
  const nveu32_t  MTL_EST_TER[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_TER,
    MGBE_MTL_EST_TER,
    MGBE_MTL_EST_TER
  };
  const nveu32_t  MTL_EST_LLR[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_LLR,
    MGBE_MTL_EST_LLR,
    MGBE_MTL_EST_LLR
  };

  ret = hw_est_write (osi_core, MTL_EST_CTR_LOW[osi_core->mac], est->ctr[0], 0);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "GCL CTR[0] failed\n",
      0LL
      );
    goto done;
  }

  /* check for est->ctr[i]  not more than FF, TODO as per hw config
   * parameter we can have max 0x3 as this value in sec */
  est->ctr[1] &= MTL_EST_CTR_HIGH_MAX;
  ret          = hw_est_write (osi_core, MTL_EST_CTR_HIGH[osi_core->mac], est->ctr[1], 0);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "GCL CTR[1] failed\n",
      0LL
      );
    goto done;
  }

  ret = hw_est_write (osi_core, MTL_EST_TER[osi_core->mac], est->ter, 0);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "GCL TER failed\n",
      0LL
      );
    goto done;
  }

  ret = hw_est_write (osi_core, MTL_EST_LLR[osi_core->mac], est->llr, 0);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "GCL LLR failed\n",
      0LL
      );
    goto done;
  }

  /* Write GCL table */
  for (i = 0U; i < est->llr; i++) {
    addr  = i;
    addr  = addr << MTL_EST_ADDR_SHIFT;
    addr &= MTL_EST_ADDR_MASK;
    ret   = hw_est_write (osi_core, addr, est->gcl[i], 1);
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "GCL enties write failed\n",
        (nveul64_t)i
        );
      goto done;
    }
  }

done:
  return ret;
}

/**
 * @brief hw_config_est - Read Setting for GCL from input and update
 * registers.
 *
 * Algorithm:
 * 1) Write  TER, LLR and EST control register
 * 2) Update GCL to sw own GCL (MTL_EST_Status bit SWOL will tell which is
 *    owned by SW) and store which GCL is in use currently in sw.
 * 3) TODO set DBGB and DBGM for debugging
 * 4) EST_data and GCRR to 1, update entry sno in ADDR and put data at
 *    est_gcl_data enable GCL MTL_EST_SSWL and wait for self clear or use
 *    SWLC in MTL_EST_Status. Please note new GCL will be pushed for each entry.
 * 5) Configure btr. Update btr based on current time (current time
 *    should be updated based on PTP by this time)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: EST configuration input argument.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
hw_config_est (
  struct osi_core_priv_data *const  osi_core,
  struct osi_est_config *const      est
  )
{
  nveu32_t        btr[2]                                = { 0 };
  nveu32_t        val                                   = 0x0;
  void            *base                                 = osi_core->base;
  nve32_t         ret                                   = 0;
  const nveu32_t  MTL_EST_CONTROL[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL,
    MGBE_MTL_EST_CONTROL,
    MGBE_MTL_EST_CONTROL
  };
  const nveu32_t  MTL_EST_BTR_LOW[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_BTR_LOW,
    MGBE_MTL_EST_BTR_LOW,
    MGBE_MTL_EST_BTR_LOW
  };
  const nveu32_t  MTL_EST_BTR_HIGH[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_BTR_HIGH,
    MGBE_MTL_EST_BTR_HIGH,
    MGBE_MTL_EST_BTR_HIGH
  };

  if (est->en_dis == OSI_DISABLE) {
    val = osi_readla (
            osi_core,
            (nveu8_t *)base +
            MTL_EST_CONTROL[osi_core->mac]
            );
    val &= ~MTL_EST_EEST;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)base +
      MTL_EST_CONTROL[osi_core->mac]
      );

    ret = 0;
  } else {
    btr[0] = est->btr[0];
    btr[1] = est->btr[1];
    if ((btr[0] == 0U) && (btr[1] == 0U)) {
      core_get_systime_from_mac (
        osi_core->base,
        osi_core->mac,
        &btr[1],
        &btr[0]
        );
    }

    if (gcl_validate (osi_core, est, btr, osi_core->mac) < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "GCL validation failed\n",
        0LL
        );
      ret = -1;
      goto done;
    }

    /* Configure ctr, ter, llr, gcl table */
    ret = configure_est_params (osi_core, est);
    if (ret < 0) {
      goto done;
    }

    /* Write parameters */
    ret = hw_est_write (
            osi_core,
            MTL_EST_BTR_LOW[osi_core->mac],
            btr[0] + est->btr_offset[0],
            OSI_DISABLE
            );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "GCL BTR[0] failed\n",
        (btr[0] + est->btr_offset[0])
        );
      goto done;
    }

    ret = hw_est_write (
            osi_core,
            MTL_EST_BTR_HIGH[osi_core->mac],
            btr[1] + est->btr_offset[1],
            OSI_DISABLE
            );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "GCL BTR[1] failed\n",
        (btr[1] + est->btr_offset[1])
        );
      goto done;
    }

    val = osi_readla (
            osi_core,
            (nveu8_t *)base +
            MTL_EST_CONTROL[osi_core->mac]
            );
    /* Store table */
    val |= MTL_EST_SSWL;
    val |= MTL_EST_EEST;
    val |= MTL_EST_QHLBF;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)base +
      MTL_EST_CONTROL[osi_core->mac]
      );
  }

done:
  return ret;
}

static nveu32_t
speed_index (
  nve32_t  speed
  )
{
  nveu32_t  ret;

  switch (speed) {
    case OSI_SPEED_10:
      ret = OSI_SPEED_10_INX;
      break;
    case OSI_SPEED_100:
      ret =  OSI_SPEED_100_INX;
      break;
    case OSI_SPEED_1000:
      ret = OSI_SPEED_1000_INX;
      break;
    case OSI_SPEED_2500:
      ret = OSI_SPEED_2500_INX;
      break;
    case OSI_SPEED_5000:
      ret = OSI_SPEED_5000_INX;
      break;
    case OSI_SPEED_10000:
      ret = OSI_SPEED_10000_INX;
      break;
    case OSI_SPEED_25000:
      ret = OSI_SPEED_25000_INX;
      break;
    default:
      ret = OSI_SPEED_10000_INX;
      break;
  }

  return ret;
}

static nve32_t
hw_config_fpe_pec_enable (
  struct osi_core_priv_data *const  osi_core,
  struct osi_fpe_config *const      fpe
  )
{
  nveu32_t  i = 0U;
  nveu32_t  index = 0;
  nveu32_t  val = 0U;
  nveu32_t  temp = 0U, temp1 = 0U;
  nveu32_t  temp_shift = 0U;
  nve32_t   ret        = 0;

  const nveu32_t  MTL_FPE_CTS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_FPE_CTS,
    MGBE_MTL_FPE_CTS,
    MGBE_MTL_FPE_CTS
  };
  const nveu32_t  MAC_FPE_CTS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_FPE_CTS,
    MGBE_MAC_FPE_CTS,
    MGBE_MAC_FPE_CTS
  };
  const nveu32_t  max_number_queue[OSI_MAX_MAC_IP_TYPES] = {
    OSI_EQOS_MAX_NUM_QUEUES,
    OSI_MGBE_MAX_NUM_QUEUES,
    OSI_MGBE_MAX_NUM_QUEUES
  };
  const nveu32_t  MAC_RQC1R[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_RQC1R,
    MGBE_MAC_RQC1R,
    MGBE_MAC_RQC1R
  };
  const nveu32_t  MAC_RQC1R_RQ[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_RQC1R_FPRQ,
    MGBE_MAC_RQC1R_RQ,
    MGBE_MAC_RQC1R_RQ
  };
  const nveu32_t  MAC_RQC1R_RQ_SHIFT[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_RQC1R_FPRQ_SHIFT,
    MGBE_MAC_RQC1R_RQ_SHIFT,
    MGBE_MAC_RQC1R_RQ_SHIFT
  };
  const nveu32_t  MTL_FPE_ADV[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_FPE_ADV,
    MGBE_MTL_FPE_ADV,
    MGBE_MTL_FPE_ADV
  };
  const nveu32_t  MTL_FPE_HADV_VAL[OSI_SPEED_MAX_INX] = {
    FPE_1G_HADV,  FPE_1G_HADV,
    FPE_1G_HADV,  FPE_10G_HADV,FPE_10G_HADV,
    FPE_10G_HADV, FPE_25G_HADV
  };

  val  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MTL_FPE_CTS[osi_core->mac]);
  val &= ~MTL_FPE_CTS_PEC;
  for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
    /* max 8 bit for this structure fot TC/TXQ. Set the TC for express or
     * preemption. Default is express for a TC. DWCXG_NUM_TC = 8 */
    temp = OSI_BIT (i);
    if ((fpe->tx_queue_preemption_enable & temp) == temp) {
      temp_shift  = i;
      temp_shift += MTL_FPE_CTS_PEC_SHIFT;
      /* set queue for preemtable */
      temp1 = OSI_ENABLE;
      temp1 = temp1 << temp_shift;
      val  |= temp1;
    }
  }

  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MTL_FPE_CTS[osi_core->mac]);
  if ((fpe->rq == 0x0U) || (fpe->rq >= max_number_queue[osi_core->mac])) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "FPE init failed due to wrong RQ\n",
      fpe->rq
      );
    ret = -1;
    goto done;
  }

  val                      = osi_readla (osi_core, (nveu8_t *)osi_core->base + (MAC_RQC1R[osi_core->mac]));
  val                     &= ~MAC_RQC1R_RQ[osi_core->mac];
  temp                     = fpe->rq;
  temp                     = temp << ((MAC_RQC1R_RQ_SHIFT[osi_core->mac]) & 0x1FU);
  temp                     = (temp & MAC_RQC1R_RQ[osi_core->mac]);
  val                     |= temp;
  osi_core->residual_queue = fpe->rq;
  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MAC_RQC1R[osi_core->mac]);

  if (osi_core->mac != OSI_MAC_HW_EQOS) {
    val  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_RQC4R);
    val &= ~MGBE_MAC_RQC4R_PMCBCQ;
    temp = fpe->rq;
    temp = temp << MGBE_MAC_RQC4R_PMCBCQ_SHIFT;
    temp = (temp & MGBE_MAC_RQC4R_PMCBCQ);
    val |= temp;
    osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MGBE_MAC_RQC4R);
  }

  /* initiate SVER for SMD-V and SMD-R */
  val  = osi_readla (osi_core, (nveu8_t *)osi_core->base + (MAC_FPE_CTS[osi_core->mac]));
  val |= MAC_FPE_CTS_SVER;
  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + (MAC_FPE_CTS[osi_core->mac]));

  val   = osi_readla (osi_core, (nveu8_t *)osi_core->base + (MTL_FPE_ADV[osi_core->mac]));
  val  &= ~MTL_FPE_ADV_HADV_MASK;
  index = speed_index (osi_core->speed);
  val  |= MTL_FPE_HADV_VAL[index];
  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + (MTL_FPE_ADV[osi_core->mac]));

  if (osi_core->mac == OSI_MAC_HW_MGBE) {
 #ifdef MACSEC_SUPPORT
    osi_core->is_fpe_enabled = OSI_ENABLE;
 #endif /*  MACSEC_SUPPORT */
  }

done:
  return ret;
}

/**
 * @brief hw_config_fpe - Read Setting for preemption and express for TC
 * and update registers.
 *
 * Algorithm:
 * 1) Check for TC enable and TC has masked for setting to preemptable.
 * 2) update FPE control status register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] fpe: FPE configuration input argument.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
hw_config_fpe (
  struct osi_core_priv_data *const  osi_core,
  struct osi_fpe_config *const      fpe
  )
{
  nveu32_t        val                               = 0U;
  nve32_t         ret                               = 0;
  const nveu32_t  MTL_FPE_CTS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_FPE_CTS,
    MGBE_MTL_FPE_CTS,
    MGBE_MTL_FPE_CTS
  };
  const nveu32_t  MAC_FPE_CTS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_FPE_CTS,
    MGBE_MAC_FPE_CTS,
    MGBE_MAC_FPE_CTS
  };

  /* Only 8 TC */
  if (fpe->tx_queue_preemption_enable > 0xFFU) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "FPE input tx_queue_preemption_enable is invalid\n",
      (nveul64_t)fpe->tx_queue_preemption_enable
      );
    ret = -1;
    goto error;
  }

  if (osi_core->mac_ver != OSI_EQOS_MAC_5_30) {
 #ifdef MACSEC_SUPPORT
    osi_lock_irq_enabled (&osi_core->macsec_fpe_lock);

    /* MACSEC and FPE cannot coexist on MGBE of T234 refer bug 3484034
     * Both EQOS and MGBE of T264 cannot have macsec and fpe enabled simultaneously */
    if (osi_core->is_macsec_enabled == OSI_ENABLE) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "FPE and MACSEC cannot co-exist\n",
        0ULL
        );
      ret = -1;
      goto done;
    }

 #endif /*  MACSEC_SUPPORT */
  }

  osi_core->fpe_ready = OSI_DISABLE;

  if (((fpe->tx_queue_preemption_enable << MTL_FPE_CTS_PEC_SHIFT) &
       MTL_FPE_CTS_PEC) == OSI_DISABLE)
  {
    val  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MTL_FPE_CTS[osi_core->mac]);
    val &= ~MTL_FPE_CTS_PEC;
    osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MTL_FPE_CTS[osi_core->mac]);

    val  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MAC_FPE_CTS[osi_core->mac]);
    val &= ~MAC_FPE_CTS_EFPE;
    osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MAC_FPE_CTS[osi_core->mac]);

    if (osi_core->mac != OSI_MAC_HW_EQOS) {
 #ifdef MACSEC_SUPPORT
      osi_core->is_fpe_enabled = OSI_DISABLE;
 #endif /*  MACSEC_SUPPORT */
    }

    ret = 0;
  } else {
    ret = hw_config_fpe_pec_enable (osi_core, fpe);
    if (ret < 0) {
      goto done;
    }
  }

done:

  if (osi_core->mac_ver != OSI_EQOS_MAC_5_30) {
 #ifdef MACSEC_SUPPORT
    osi_unlock_irq_enabled (&osi_core->macsec_fpe_lock);
 #endif /*  MACSEC_SUPPORT */
  }

error:
  return ret;
}

/**
 * @brief enable_mtl_interrupts - Enable MTL interrupts
 *
 * Algorithm: enable MTL interrupts for EST
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void
enable_mtl_interrupts (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t        mtl_est_ir                         = OSI_DISABLE;
  const nveu32_t  MTL_EST_ITRE[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_ITRE,
    MGBE_MTL_EST_ITRE,
    MGBE_MTL_EST_ITRE
  };

  mtl_est_ir = osi_readla (
                 osi_core,
                 (nveu8_t *)osi_core->base +
                 MTL_EST_ITRE[osi_core->mac]
                 );

  /* enable only MTL interrupt realted to
   * Constant Gate Control Error
   * Head-Of-Line Blocking due to Scheduling
   * Head-Of-Line Blocking due to Frame Size
   * BTR Error
   * Switch to S/W owned list Complete
   */
  mtl_est_ir |= (MTL_EST_ITRE_CGCE | MTL_EST_ITRE_IEHS |
                 MTL_EST_ITRE_IEHF | MTL_EST_ITRE_IEBE |
                 MTL_EST_ITRE_IECC);
  osi_writela (
    osi_core,
    mtl_est_ir,
    (nveu8_t *)osi_core->base +
    MTL_EST_ITRE[osi_core->mac]
    );
}

/**
 * @brief enable_fpe_interrupts - Enable MTL interrupts
 *
 * Algorithm: enable FPE interrupts
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void
enable_fpe_interrupts (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t        value                         = OSI_DISABLE;
  const nveu32_t  MAC_IER[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_IMR,
    MGBE_MAC_IER,
    MGBE_MAC_IER
  };
  const nveu32_t  IMR_FPEIE[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_IMR_FPEIE,
    MGBE_IMR_FPEIE,
    MGBE_IMR_FPEIE
  };

  /* Read MAC IER Register and enable Frame Preemption Interrupt
   * Enable */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MAC_IER[osi_core->mac]
            );
  value |= IMR_FPEIE[osi_core->mac];
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MAC_IER[osi_core->mac]
    );
}

/**
 * @brief save_gcl_params - save GCL configs in local core structure
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void
save_gcl_params (
  struct osi_core_priv_data  *osi_core
  )
{
  struct core_local  *l_core       = (struct core_local *)(void *)osi_core;
  const nveu32_t     gcl_widhth[4] = {
    0, OSI_MAX_24BITS, OSI_MAX_28BITS,
    OSI_MAX_32BITS
  };
  const nveu32_t     gcl_ti_mask[4] = {
    0, OSI_MASK_16BITS, OSI_MASK_20BITS,
    OSI_MASK_24BITS
  };
  const nveu32_t     gcl_depthth[6] = {
    0,                OSI_GCL_SIZE_64,  OSI_GCL_SIZE_128,
    OSI_GCL_SIZE_256, OSI_GCL_SIZE_512,
    OSI_GCL_SIZE_1024
  };

  l_core->gcl_width_val = gcl_widhth[l_core->hw_features.gcl_width];
  l_core->ti_mask       = gcl_ti_mask[l_core->hw_features.gcl_width];
  l_core->gcl_dep       = gcl_depthth[l_core->hw_features.gcl_depth];
}

/**
 * @brief hw_tsn_init - initialize TSN feature
 *
 * Algorithm:
 * 1) If hardware support EST,
 *   a) Set default EST configuration
 *   b) Set enable interrupts
 * 2) If hardware supports FPE
 *   a) Set default FPE configuration
 *   b) enable interrupts
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
void
hw_tsn_init (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t        val                                   = 0x0;
  nveu32_t        temp                                  = 0U;
  const nveu32_t  MTL_EST_CONTROL[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL,
    MGBE_MTL_EST_CONTROL,
    MGBE_MTL_EST_CONTROL
  };
  const nveu32_t  MTL_EST_CONTROL_PTOV[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL_PTOV,
    MGBE_MTL_EST_CONTROL_PTOV,
    MGBE_MTL_EST_CONTROL_PTOV
  };
  const nveu32_t  MTL_EST_PTOV_RECOMMEND[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_PTOV_RECOMMEND,
    MGBE_MTL_EST_PTOV_RECOMMEND,
    MGBE_MTL_EST_PTOV_RECOMMEND
  };
  const nveu32_t  MTL_EST_CONTROL_PTOV_SHIFT[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL_PTOV_SHIFT,
    MGBE_MTL_EST_CONTROL_PTOV_SHIFT,
    MGBE_MTL_EST_CONTROL_PTOV_SHIFT
  };
  const nveu32_t  MTL_EST_CONTROL_CTOV[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL_CTOV,
    MGBE_MTL_EST_CONTROL_CTOV,
    MGBE_MTL_EST_CONTROL_CTOV
  };
  const nveu32_t  MTL_EST_CTOV_RECOMMEND[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CTOV_RECOMMEND,
    MGBE_MTL_EST_CTOV_RECOMMEND,
    MGBE_MTL_EST_CTOV_RECOMMEND
  };
  const nveu32_t  MTL_EST_CONTROL_CTOV_SHIFT[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL_CTOV_SHIFT,
    MGBE_MTL_EST_CONTROL_CTOV_SHIFT,
    MGBE_MTL_EST_CONTROL_CTOV_SHIFT
  };
  const nveu32_t  MTL_EST_CONTROL_LCSE[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL_LCSE,
    MGBE_MTL_EST_CONTROL_LCSE,
    MGBE_MTL_EST_CONTROL_LCSE
  };
  const nveu32_t  MTL_EST_CONTROL_LCSE_VAL[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL_LCSE_VAL,
    MGBE_MTL_EST_CONTROL_LCSE_VAL,
    MGBE_MTL_EST_CONTROL_LCSE_VAL
  };
  const nveu32_t  MTL_EST_CONTROL_DDBF[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_CONTROL_DDBF,
    MGBE_MTL_EST_CONTROL_DDBF,
    MGBE_MTL_EST_CONTROL_DDBF
  };
  const nveu32_t  MTL_EST_OVERHEAD[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_OVERHEAD,
    MGBE_MTL_EST_OVERHEAD,
    MGBE_MTL_EST_OVERHEAD
  };
  const nveu32_t  MTL_EST_OVERHEAD_OVHD[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_OVERHEAD_OVHD,
    MGBE_MTL_EST_OVERHEAD_OVHD,
    MGBE_MTL_EST_OVERHEAD_OVHD
  };
  const nveu32_t  MTL_EST_OVERHEAD_RECOMMEND[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_EST_OVERHEAD_RECOMMEND,
    MGBE_MTL_EST_OVERHEAD_RECOMMEND,
    MGBE_MTL_EST_OVERHEAD_RECOMMEND
  };
  const nveu32_t  MAC_RQC1R[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_RQC1R,
    MGBE_MAC_RQC1R,
    MGBE_MAC_RQC1R
  };
  const nveu32_t  MAC_RQC1R_RQ[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_RQC1R_FPRQ,
    MGBE_MAC_RQC1R_RQ,
    MGBE_MAC_RQC1R_RQ
  };
  const nveu32_t  MAC_RQC1R_RQ_SHIFT[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_RQC1R_FPRQ_SHIFT,
    MGBE_MAC_RQC1R_RQ_SHIFT,
    MGBE_MAC_RQC1R_RQ_SHIFT
  };

  /* Configure EST paramenters */
  save_gcl_params (osi_core);
  val = osi_readla (osi_core, (nveu8_t *)osi_core->base + MTL_EST_CONTROL[osi_core->mac]);

  /*
   * PTOV PTP clock period * 6
   * dual-port RAM based asynchronous FIFO controllers or
   * Single-port RAM based synchronous FIFO controllers
   * CTOV 96 x Tx clock period
   * :
   * :
   * set other default value
   */
  val &= ~MTL_EST_CONTROL_PTOV[osi_core->mac];
  temp = MTL_EST_PTOV_RECOMMEND[osi_core->mac];
  temp = temp << ((MTL_EST_CONTROL_PTOV_SHIFT[osi_core->mac]) & 0x1FU);
  val |= temp;

  val &= ~MTL_EST_CONTROL_CTOV[osi_core->mac];
  temp = MTL_EST_CTOV_RECOMMEND[osi_core->mac];
  temp = temp << ((MTL_EST_CONTROL_CTOV_SHIFT[osi_core->mac]) & 0x1FU);
  val |= temp;

  /*Loop Count to report Scheduling Error*/
  val &= ~MTL_EST_CONTROL_LCSE[osi_core->mac];
  val |= MTL_EST_CONTROL_LCSE_VAL[osi_core->mac];

  if (osi_core->mac == OSI_MAC_HW_EQOS) {
    val &= ~EQOS_MTL_EST_CONTROL_DFBS;
  }

  val &= ~MTL_EST_CONTROL_DDBF[osi_core->mac];
  val |= MTL_EST_CONTROL_DDBF[osi_core->mac];
  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MTL_EST_CONTROL[osi_core->mac]);

  val  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MTL_EST_OVERHEAD[osi_core->mac]);
  val &= ~MTL_EST_OVERHEAD_OVHD[osi_core->mac];
  /* As per hardware programming info */
  val |= MTL_EST_OVERHEAD_RECOMMEND[osi_core->mac];
  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MTL_EST_OVERHEAD[osi_core->mac]);

  enable_mtl_interrupts (osi_core);

  /* Configure FPE parameters */
  val  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MAC_RQC1R[osi_core->mac]);
  val &= ~MAC_RQC1R_RQ[osi_core->mac];
  temp = osi_core->residual_queue;
  temp = temp << ((MAC_RQC1R_RQ_SHIFT[osi_core->mac]) & 0x1FU);
  temp = (temp & MAC_RQC1R_RQ[osi_core->mac]);
  val |= temp;
  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MAC_RQC1R[osi_core->mac]);

  if (osi_core->mac != OSI_MAC_HW_EQOS) {
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MAC_RQC4R
            );
    val &= ~MGBE_MAC_RQC4R_PMCBCQ;
    temp = osi_core->residual_queue;
    temp = temp << MGBE_MAC_RQC4R_PMCBCQ_SHIFT;
    temp = (temp & MGBE_MAC_RQC4R_PMCBCQ);
    val |= temp;
    osi_writela (osi_core, val, (nveu8_t *)osi_core->base + MGBE_MAC_RQC4R);
  }

  enable_fpe_interrupts (osi_core);

  /* CBS setting for TC or TXQ for default configuration
     user application should use IOCTL to set CBS as per requirement
   */
}

#ifdef HSI_SUPPORT
  #ifdef NV_VLTEST_BUILD

/**
 * @brief hsi_common_error_inject
 *
 * Algorithm:
 * - For macsec HSI: trigger interrupt using MACSEC_*_INTERRUPT_SET_0 register
 * - For mmc counter based: trigger interrupt by incrementing count by threshold value
 * - For rest: Directly set the error detected as there is no other mean to induce error
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] error_code: Ethernet HSI error code
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
hsi_common_error_inject (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   error_code
  )
{
  nve32_t  ret = 0;

 #ifdef MACSEC_SUPPORT
  const struct core_local  *l_core = (struct core_local *)(void *)osi_core;
 #endif

  switch (error_code) {
    case OSI_INBOUND_BUS_CRC_ERR:
      osi_core->hsi.inject_crc_err_count =
        osi_update_stats_counter (
          osi_core->hsi.inject_crc_err_count,
          osi_core->hsi.err_count_threshold
          );
      break;
    case OSI_RECEIVE_CHECKSUM_ERR:
      osi_core->hsi.inject_udp_err_count =
        osi_update_stats_counter (
          osi_core->hsi.inject_udp_err_count,
          osi_core->hsi.err_count_threshold
          );
      break;
 #ifdef MACSEC_SUPPORT
    case OSI_MACSEC_RX_CRC_ERR:
    case OSI_MACSEC_TX_CRC_ERR:
    case OSI_MACSEC_RX_ICV_ERR:
    case OSI_MACSEC_REG_VIOL_ERR:
      if (l_core->macsec_ops->hsi_macsec_error_inject != OSI_NULL) {
        l_core->macsec_ops->hsi_macsec_error_inject (osi_core, error_code);
      }

      break;
 #endif
    case OSI_PHY_WRITE_VERIFY_ERR:
      osi_core->hsi.err_code[PHY_WRITE_VERIFY_FAIL_IDX]         = OSI_PHY_WRITE_VERIFY_ERR;
      osi_core->hsi.report_err                                  = OSI_ENABLE;
      osi_core->hsi.report_count_err[PHY_WRITE_VERIFY_FAIL_IDX] = OSI_ENABLE;
      break;
    case OSI_TX_FRAME_ERR:
      osi_core->hsi.report_count_err[TX_FRAME_ERR_IDX] = OSI_ENABLE;
      osi_core->hsi.err_code[TX_FRAME_ERR_IDX]         = OSI_TX_FRAME_ERR;
      osi_core->hsi.report_err                         = OSI_ENABLE;
      break;
    case OSI_PCS_AUTONEG_ERR:
      osi_core->hsi.err_code[AUTONEG_ERR_IDX]         = OSI_PCS_AUTONEG_ERR;
      osi_core->hsi.report_err                        = OSI_ENABLE;
      osi_core->hsi.report_count_err[AUTONEG_ERR_IDX] = OSI_ENABLE;
      break;
    case OSI_PCS_LNK_ERR:
      osi_core->hsi.err_code[PCS_LNK_ERR_IDX]         = OSI_PCS_LNK_ERR;
      osi_core->hsi.report_err                        = OSI_ENABLE;
      osi_core->hsi.report_count_err[PCS_LNK_ERR_IDX] = OSI_ENABLE;
      break;
    case OSI_XPCS_WRITE_FAIL_ERR:
      osi_core->hsi.err_code[XPCS_WRITE_FAIL_IDX]         = OSI_XPCS_WRITE_FAIL_ERR;
      osi_core->hsi.report_err                            = OSI_ENABLE;
      osi_core->hsi.report_count_err[XPCS_WRITE_FAIL_IDX] = OSI_ENABLE;
      break;
    case OSI_MAC_CMN_INTR_ERR:
      osi_core->hsi.err_code[MAC_CMN_INTR_ERR_IDX]         = OSI_MAC_CMN_INTR_ERR;
      osi_core->hsi.report_err                             = OSI_ENABLE;
      osi_core->hsi.report_count_err[MAC_CMN_INTR_ERR_IDX] = OSI_ENABLE;
      break;
    case OSI_M2M_TSC_READ_ERR:
    case OSI_M2M_TIME_CAL_ERR:
    case OSI_M2M_ADJ_FREQ_ERR:
    case OSI_M2M_ADJ_TIME_ERR:
    case OSI_M2M_SET_TIME_ERR:
    case OSI_M2M_CONFIG_PTP_ERR:
      osi_core->hsi.report_err                = OSI_ENABLE;
      osi_core->hsi.err_code[MAC2MAC_ERR_IDX] = error_code;
      break;
    default:
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Invalid error code\n",
        (nveu32_t)error_code
        );
      ret = -1;
      break;
  }

  return ret;
}

  #endif

/**
 * @brief hsi_update_mmc_val - function to read register and return value to callee
 *
 * Algorithm: Read the registers, check for boundary, if more, reset
 *        counters else return same to caller.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] last_value: previous value of stats variable.
 * @param[in] offset: HW register offset
 *
 * @note
 *      1) MAC should be init and started. see osi_start_mac()
 *      2) osi_core->osd should be populated
 *
 * @retval 0 on MMC counters overflow
 * @retval value on current MMC counter value.
 */
static inline nveu64_t
hsi_update_mmc_val (
  struct osi_core_priv_data  *osi_core,
  nveu64_t                   last_value,
  nveu64_t                   offset
  )
{
  nveu64_t        temp                            = 0;
  nveu32_t        value                           = osi_readl ((nveu8_t *)osi_core->base + offset);
  const nveu32_t  MMC_CNTRL[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MMC_CNTRL,
    MGBE_MMC_CNTRL,
    MGBE_MMC_CNTRL
  };
  const nveu32_t  MMC_CNTRL_CNTRST[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MMC_CNTRL_CNTRST,
    MGBE_MMC_CNTRL_CNTRST,
    MGBE_MMC_CNTRL_CNTRST
  };

  temp = last_value + value;
  if (temp < last_value) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OUTOFBOUND,
      "Value overflow resetting  all counters\n",
      (nveul64_t)offset
      );
    value = osi_readl ((nveu8_t *)osi_core->base + MMC_CNTRL[osi_core->mac]);
    /* self-clear bit in one clock cycle */
    value |= MMC_CNTRL_CNTRST[osi_core->mac];
    osi_writel (value, (nveu8_t *)osi_core->base + MMC_CNTRL[osi_core->mac]);
    osi_memset (&osi_core->mmc, 0U, sizeof (struct osi_mmc_counters));
  }

  return temp;
}

/**
 * @brief hsi_read_err - To read MMC error registers and update
 *         ether_mmc_counter structure variable
 *
 * Algorithm: Pass register offset and old value to helper function and
 *         update structure.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 *      1) MAC should be init and started. see osi_start_mac()
 *      2) osi_core->osd should be populated
 */
void
hsi_read_err (
  struct osi_core_priv_data *const  osi_core
  )
{
  struct osi_mmc_counters  *mmc                             = &osi_core->mmc;
  const nveu32_t           RXCRCERROR[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MMC_RXCRCERROR,
    MGBE_MMC_RXCRCERROR_L,
    MGBE_MMC_RXCRCERROR_L
  };
  const nveu32_t           RXIPV4_HDRERR_PKTS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MMC_RXIPV4_HDRERR_PKTS,
    MGBE_MMC_RXIPV4_HDRERR_PKTS_L,
    MGBE_MMC_RXIPV4_HDRERR_PKTS_L
  };
  const nveu32_t           RXIPV6_HDRERR_PKTS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MMC_RXIPV6_HDRERR_PKTS,
    MGBE_MMC_RXIPV6_HDRERR_PKTS_L,
    MGBE_MMC_RXIPV6_HDRERR_PKTS_L
  };
  const nveu32_t           RXUDP_ERR_PKTS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MMC_RXUDP_ERR_PKTS,
    MGBE_MMC_RXUDP_ERR_PKTS_L,
    MGBE_MMC_RXUDP_ERR_PKTS_L
  };
  const nveu32_t           RXTCP_ERR_PKTS[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MMC_RXTCP_ERR_PKTS,
    MGBE_MMC_RXTCP_ERR_PKTS_L,
    MGBE_MMC_RXTCP_ERR_PKTS_L
  };

  mmc->mmc_rx_crc_error = hsi_update_mmc_val (
                            osi_core,
                            mmc->mmc_rx_crc_error,
                            RXCRCERROR[osi_core->mac]
                            );
  mmc->mmc_rx_ipv4_hderr = hsi_update_mmc_val (
                             osi_core,
                             mmc->mmc_rx_ipv4_hderr,
                             RXIPV4_HDRERR_PKTS[osi_core->mac]
                             );
  mmc->mmc_rx_ipv6_hderr = hsi_update_mmc_val (
                             osi_core,
                             mmc->mmc_rx_ipv6_hderr,
                             RXIPV6_HDRERR_PKTS[osi_core->mac]
                             );
  mmc->mmc_rx_udp_err = hsi_update_mmc_val (
                          osi_core,
                          mmc->mmc_rx_udp_err,
                          RXUDP_ERR_PKTS[osi_core->mac]
                          );
  mmc->mmc_rx_tcp_err = hsi_update_mmc_val (
                          osi_core,
                          mmc->mmc_rx_tcp_err,
                          RXTCP_ERR_PKTS[osi_core->mac]
                          );
}

#endif /* HSI_SUPPORT */

/**
 * @brief prepare_l3l4_ctr_reg - Prepare control register for L3L4 filters.
 *
 * @note
 * Algorithm:
 * - This sequence is used to prepare L3L4 control register for SA and DA Port Number matching.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (osi_l3_l4_filter)
 * @param[out] ctr_reg: Pointer to L3L4 CTR register value
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *
 * @retval L3L4 CTR register value
 */
static void
prepare_l3l4_ctr_reg (
  const struct osi_core_priv_data *const  osi_core,
  const struct osi_l3_l4_filter *const    l3_l4,
  nveu32_t                                *ctr_reg
  )
{
 #ifndef OSI_STRIPPED_LIB
  nveu32_t  dma_routing_enable = l3_l4->dma_routing_enable;
  nveu32_t  dst_addr_match     = l3_l4->data.dst.addr_match;
 #else
  nveu32_t  dma_routing_enable = OSI_BIT (0);
  nveu32_t  dst_addr_match     = OSI_BIT (0);
 #endif /* !OSI_STRIPPED_LIB */
  const nveu32_t  dma_chan_en_shift[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAC_L3L4_CTR_DMCHEN_SHIFT,
    MGBE_MAC_L3L4_CTR_DMCHEN_SHIFT,
    MGBE_MAC_L3L4_CTR_DMCHEN_SHIFT
  };
  nveu32_t        value = 0U;

  /* set routing dma channel */
  value |= dma_routing_enable << (dma_chan_en_shift[osi_core->mac] & 0x1FU);
  value |= l3_l4->dma_chan << MAC_L3L4_CTR_DMCHN_SHIFT;

  /* Enable L3 filters for IPv4 DESTINATION addr matching */
  value |= dst_addr_match << MAC_L3L4_CTR_L3DAM_SHIFT;

 #ifndef OSI_STRIPPED_LIB
  /* Enable L3 filters for IPv4 DESTINATION addr INV matching */
  value |= l3_l4->data.dst.addr_match_inv << MAC_L3L4_CTR_L3DAIM_SHIFT;

  /* Enable L3 filters for IPv4 SOURCE addr matching */
  value |= (l3_l4->data.src.addr_match << MAC_L3L4_CTR_L3SAM_SHIFT) |
           (l3_l4->data.src.addr_match_inv << MAC_L3L4_CTR_L3SAIM_SHIFT);

  /* Enable L4 filters for DESTINATION port No matching */
  value |= (l3_l4->data.dst.port_match << MAC_L3L4_CTR_L4DPM_SHIFT) |
           (l3_l4->data.dst.port_match_inv << MAC_L3L4_CTR_L4DPIM_SHIFT);

  /* Enable L4 filters for SOURCE Port No matching */
  value |= (l3_l4->data.src.port_match << MAC_L3L4_CTR_L4SPM_SHIFT) |
           (l3_l4->data.src.port_match_inv << MAC_L3L4_CTR_L4SPIM_SHIFT);
  if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
    /* Enable combined L3 and L4 filters */
    value |= l3_l4->data.is_l3l4_match_en << MAC_L3L4_CTR_L5TEN_SHIFT;
  }

  /* set udp / tcp port matching bit (for l4) */
  value |= l3_l4->data.is_udp << MAC_L3L4_CTR_L4PEN_SHIFT;

  /* set ipv4 / ipv6 protocol matching bit (for l3) */
  value |= l3_l4->data.is_ipv6 << MAC_L3L4_CTR_L3PEN_SHIFT;
 #endif /* !OSI_STRIPPED_LIB */

  *ctr_reg = value;
}

/**
 * @brief prepare_l3_addr_registers - prepare register data for IPv4/IPv6 address filtering
 *
 * @note
 * Algorithm:
 *  - Update IPv4/IPv6 source/destination address for L3 layer filtering.
 *  - For IPv4, both source/destination address can be configured but
 *    for IPv6, only one of the source/destination address can be configured.
 *
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (osi_l3_l4_filter)
 * @param[out] l3_addr1_reg: Pointer to L3 ADDR1 register value
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 */
static void
prepare_l3_addr_registers (
  const struct osi_l3_l4_filter *const  l3_l4,
#ifndef OSI_STRIPPED_LIB
  nveu32_t                              *l3_addr0_reg,
  nveu32_t                              *l3_addr2_reg,
  nveu32_t                              *l3_addr3_reg,
#endif /* !OSI_STRIPPED_LIB */
  nveu32_t                              *l3_addr1_reg
  )
{
 #ifndef OSI_STRIPPED_LIB
  if (l3_l4->data.is_ipv6 == OSI_L3L4_ENABLE) {
    const nveu16_t  *addr;

    /* For IPv6, either source address or destination
     * address only one of them can be enabled
     */
    if (l3_l4->data.src.addr_match == OSI_L3L4_ENABLE) {
      /* select src address only */
      addr = l3_l4->data.src.ip6_addr;
    } else {
      /* select dst address only */
      addr = l3_l4->data.dst.ip6_addr;
    }

    /* update Bits[31:0] of 128-bit IP addr */
    *l3_addr0_reg = addr[7] | ((nveu32_t)addr[6] << 16);

    /* update Bits[63:32] of 128-bit IP addr */
    *l3_addr1_reg = addr[5] | ((nveu32_t)addr[4] << 16);

    /* update Bits[95:64] of 128-bit IP addr */
    *l3_addr2_reg = addr[3] | ((nveu32_t)addr[2] << 16);

    /* update Bits[127:96] of 128-bit IP addr */
    *l3_addr3_reg = addr[1] | ((nveu32_t)addr[0] << 16);
  } else {
 #endif /* !OSI_STRIPPED_LIB */
  const nveu8_t *addr;
  nveu32_t value;

 #ifndef OSI_STRIPPED_LIB
  /* set source address */
  addr          = l3_l4->data.src.ip4_addr;
  value         = addr[3];
  value        |= (nveu32_t)addr[2] << 8;
  value        |= (nveu32_t)addr[1] << 16;
  value        |= (nveu32_t)addr[0] << 24;
  *l3_addr0_reg = value;
 #endif /* !OSI_STRIPPED_LIB */

  /* set destination address */
  addr          = l3_l4->data.dst.ip4_addr;
  value         = addr[3];
  value        |= (nveu32_t)addr[2] << 8;
  value        |= (nveu32_t)addr[1] << 16;
  value        |= (nveu32_t)addr[0] << 24;
  *l3_addr1_reg = value;
 #ifndef OSI_STRIPPED_LIB
}

 #endif /* !OSI_STRIPPED_LIB */
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief prepare_l4_port_register - program source and destination port number
 *
 * @note
 * Algorithm:
 *  - Program l4 address register with source and destination port numbers.
 *
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (osi_l3_l4_filter)
 * @param[out] l4_addr_reg: Pointer to L3 ADDR0 register value
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       3) DCS bits should be enabled in RXQ to DMA mapping register
 */
static void
prepare_l4_port_register (
  const struct osi_l3_l4_filter *const  l3_l4,
  nveu32_t                              *l4_addr_reg
  )
{
  nveu32_t  value = 0U;

  /* set source port */
  value |= ((nveu32_t)l3_l4->data.src.port_no
            & MGBE_MAC_L4_ADDR_SP_MASK);

  /* set destination port */
  value |= (((nveu32_t)l3_l4->data.dst.port_no <<
             MGBE_MAC_L4_ADDR_DP_SHIFT) & MGBE_MAC_L4_ADDR_DP_MASK);

  *l4_addr_reg = value;
}

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief prepare_l3l4_registers - function to prepare l3l4 registers
 *
 * @note
 * Algorithm:
 *  - If filter to be enabled,
 *        - Prepare l3 ip address registers using prepare_l3_addr_registers().
 *        - Prepare l4 port register using prepare_l4_port_register().
 *        - Prepare l3l4 control register using prepare_l3l4_ctr_reg().
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (osi_l3_l4_filter)
 * @param[out] l3_addr1_reg: Pointer to L3 ADDR1 register value
 * @param[out] ctr_reg: Pointer to L3L4 CTR register value
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->osd should be populated
 *       3) DCS bits should be enabled in RXQ to DMA mapping register
 */
void
prepare_l3l4_registers (
  const struct osi_core_priv_data *const  osi_core,
  const struct osi_l3_l4_filter *const    l3_l4,
#ifndef OSI_STRIPPED_LIB
  nveu32_t                                *l3_addr0_reg,
  nveu32_t                                *l3_addr2_reg,
  nveu32_t                                *l3_addr3_reg,
  nveu32_t                                *l4_addr_reg,
#endif /* !OSI_STRIPPED_LIB */
  nveu32_t                                *l3_addr1_reg,
  nveu32_t                                *ctr_reg
  )
{
  /* prepare regiser data if filter to be enabled */
  if (l3_l4->filter_enb_dis == OSI_L3L4_ENABLE) {
    /* prepare l3 filter ip address register data */
    prepare_l3_addr_registers (
      l3_l4,
 #ifndef OSI_STRIPPED_LIB
      l3_addr0_reg,
      l3_addr2_reg,
      l3_addr3_reg,
 #endif /* !OSI_STRIPPED_LIB */
      l3_addr1_reg
      );

 #ifndef OSI_STRIPPED_LIB
    /* prepare l4 filter port register data */
    prepare_l4_port_register (l3_l4, l4_addr_reg);
 #endif /* !OSI_STRIPPED_LIB */

    /* prepare control register data */
    prepare_l3l4_ctr_reg (osi_core, l3_l4, ctr_reg);
  }
}

/**
 * @brief hw_validate_avb_input- validate input arguments
 *
 * Algorithm:
 *      1) Check if idle slope is valid
 *      2) Check if send slope is valid
 *      3) Check if hi credit is valid
 *      4) Check if low credit is valid
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] avb: structure having configuration for avb algorithm
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
hw_validate_avb_input (
  struct osi_core_priv_data *const            osi_core,
  const struct osi_core_avb_algorithm *const  avb
  )
{
  nve32_t   ret                                     = 0;
  nveu32_t  ETS_QW_ISCQW_MASK[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_TXQ_ETS_QW_ISCQW_MASK,
    MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK,
    MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK
  };
  nveu32_t  ETS_SSCR_SSC_MASK[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MTL_TXQ_ETS_SSCR_SSC_MASK,
    MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK,
    MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK
  };
  nveu32_t  ETS_HC_BOUND = 0x8000000U;
  nveu32_t  ETS_LC_BOUND = 0xF8000000U;
  nveu32_t  mac          = osi_core->mac;

  if (avb->idle_slope > ETS_QW_ISCQW_MASK[mac]) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid idle_slope\n",
      (nveul64_t)avb->idle_slope
      );
    ret = -1;
    goto fail;
  }

  if (avb->send_slope > ETS_SSCR_SSC_MASK[mac]) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid send_slope\n",
      (nveul64_t)avb->send_slope
      );
    ret = -1;
    goto fail;
  }

  if (avb->hi_credit > ETS_HC_BOUND) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid hi credit\n",
      (nveul64_t)avb->hi_credit
      );
    ret = -1;
    goto fail;
  }

  if ((avb->low_credit < ETS_LC_BOUND) &&
      (avb->low_credit != 0U))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid low credit\n",
      (nveul64_t)avb->low_credit
      );
    ret = -1;
    goto fail;
  }

fail:
  return ret;
}

void
hw_config_flow_control (
  struct osi_core_priv_data *const  osi_core
  )
{
  nveu8_t   *addr = (nveu8_t *)osi_core->base;
  nveu32_t  val;

  /* Configure Tx flow control */
  val  = osi_readla (osi_core, addr + MAC_QX_TX_FLW_CTRL (0U));
  val |= MAC_QX_TX_FLW_CTRL_TFE;
  val &= ~MAC_PAUSE_TIME_MASK;
  val |= MAC_PAUSE_TIME & MAC_PAUSE_TIME_MASK;
  osi_writela (osi_core, val, addr + MAC_QX_TX_FLW_CTRL (0U));

  /* configure Rx flow control */
  val  = osi_readla (osi_core, addr + MAC_RX_FLW_CTRL);
  val |= MAC_RX_FLW_CTRL_RFE;
  osi_writela (osi_core, val, addr + MAC_RX_FLW_CTRL);
}
