// SPDX-License-Identifier: MIT

/* SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "xpcs.h"
#include "core_local.h"

/**
 * @brief xpcs_poll_for_an_complete - Polling for AN complete.
 *
 * Algorithm: This routine poll for AN completion status from
 *              XPCS IP.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[out] an_status: AN status from XPCS
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t
xpcs_poll_for_an_complete (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   *an_status
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  status     = 0;
  nveu32_t  retry      = 1000;
  nveu32_t  count;
  nve32_t   cond = 1;
  nve32_t   ret  = 0;

  if (osi_core->pre_sil == 0x1U) {
    // TBD: T264 increase retry for uFPGA
    retry = 10000;
  }

  /* 14. Poll for AN complete */
  cond  = 1;
  count = 0;
  while (cond == 1) {
    if (count > retry) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "XPCS AN completion timed out\n",
        0ULL
        );
 #ifdef HSI_SUPPORT
      if (osi_core->hsi.enabled == OSI_ENABLE) {
        osi_core->hsi.err_code[AUTONEG_ERR_IDX] =
          OSI_PCS_AUTONEG_ERR;
        osi_core->hsi.report_err                        = OSI_ENABLE;
        osi_core->hsi.report_count_err[AUTONEG_ERR_IDX] = OSI_ENABLE;
      }

 #endif
      ret = -1;
      goto fail;
    }

    count++;

    status = xpcs_read (xpcs_base, XPCS_VR_MII_AN_INTR_STS);
    if ((status & XPCS_VR_MII_AN_INTR_STS_CL37_ANCMPLT_INTR) == 0U) {
      /* autoneg not completed - poll */
      osi_core->osd_ops.usleep (OSI_DELAY_1000US);
    } else {
      /* 15. clear interrupt */
      status &= ~XPCS_VR_MII_AN_INTR_STS_CL37_ANCMPLT_INTR;
      ret     = xpcs_write_safety (osi_core, XPCS_VR_MII_AN_INTR_STS, status);
      if (ret != 0) {
        goto fail;
      }

      cond = 0;
    }
  }

  if ((status & XPCS_USXG_AN_STS_SPEED_MASK) == 0U) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "XPCS AN completed with zero speed\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  *an_status = status;
fail:
  return ret;
}

/**
 * @brief xpcs_set_speed - Set speed at XPCS
 *
 * Algorithm: This routine program XPCS speed based on AN status.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[in] status: Autonegotation Status.
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static inline nve32_t
xpcs_set_speed (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   status
  )
{
  nveu32_t  speed      = status & XPCS_USXG_AN_STS_SPEED_MASK;
  nveu32_t  ctrl       = 0;
  void      *xpcs_base = osi_core->xpcs_base;

  ctrl = xpcs_read (xpcs_base, XPCS_SR_MII_CTRL);

  switch (speed) {
    case XPCS_USXG_AN_STS_SPEED_2500:
      /* 2.5Gbps */
      ctrl |= XPCS_SR_MII_CTRL_SS5;
      ctrl &= ~(XPCS_SR_MII_CTRL_SS6 | XPCS_SR_MII_CTRL_SS13);
      break;
    case XPCS_USXG_AN_STS_SPEED_5000:
      /* 5Gbps */
      ctrl |= (XPCS_SR_MII_CTRL_SS5 | XPCS_SR_MII_CTRL_SS13);
      ctrl &= ~XPCS_SR_MII_CTRL_SS6;
      break;
    case XPCS_USXG_AN_STS_SPEED_10000:
    default:
      /* 10Gbps */
      ctrl |= (XPCS_SR_MII_CTRL_SS6 | XPCS_SR_MII_CTRL_SS13);
      ctrl &= ~XPCS_SR_MII_CTRL_SS5;
      break;
  }

  return xpcs_write_safety (osi_core, XPCS_SR_MII_CTRL, ctrl);
}

static nve32_t
xpcs_poll_flt_rx_link (
  struct osi_core_priv_data  *osi_core
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nve32_t   cond       = COND_NOT_MET;
  nveu32_t  retry      = RETRY_ONCE;
  nveu32_t  count      = 0;
  nveu32_t  once       = 0;
  nve32_t   ret        = 0;
  nveu32_t  ctrl       = 0;

  /* poll for Rx link up */
  while (cond == COND_NOT_MET) {
    ctrl = xpcs_read (xpcs_base, XPCS_SR_XS_PCS_STS1);
    if ((ctrl & XPCS_SR_XS_PCS_STS1_RLU) == XPCS_SR_XS_PCS_STS1_RLU) {
      cond = COND_MET;
    } else {
      /* Maximum wait delay as per HW team is 1msec */
      if (count > retry) {
        ret = -1;
        goto fail;
      }

      count++;
      if (once == 0U) {
        osi_core->osd_ops.udelay (OSI_DELAY_1US);
        once = 1U;
      } else {
        osi_core->osd_ops.usleep (OSI_DELAY_1000US);
      }
    }
  }

  // FIXME: Causes lane bringup failure for SLT MGBE
  if (osi_core->mac != OSI_MAC_HW_MGBE_T26X) {
    /* poll for FLT bit to 0 */
    cond  = COND_NOT_MET;
    count = 0;
    retry = RETRY_COUNT;
    while (cond == COND_NOT_MET) {
      if (count > retry) {
        ret = -1;
        goto fail;
      }

      count++;

      ctrl = xpcs_read (xpcs_base, XPCS_SR_XS_PCS_STS1);
      if ((ctrl & XPCS_SR_XS_PCS_STS1_FLT) == 0U) {
        cond = COND_MET;
      } else {
        /* Maximum wait delay as 1ms */
        osi_core->osd_ops.usleep (OSI_DELAY_1000US);
      }
    }
  }

  /* delay 10ms to wait the staus propagate to MAC block */
  osi_core->osd_ops.usleep (OSI_DELAY_10000US);

fail:
  return ret;
}

#if 0 // FIXME: Not used for SLT EQOS bringup

/**
 * @brief eqos_xpcs_set_speed - Set speed at XPCS
 *
 * Algorithm: This routine program XPCS speed based on AN status.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[in] status: Autonegotation Status.
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static inline nve32_t
eqos_xpcs_set_speed (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   status
  )
{
  nveu32_t  speed      = status & EQOS_XPCS_VR_MII_AN_STS_SPEED_MASK;
  nveu32_t  ctrl       = 0;
  void      *xpcs_base = osi_core->xpcs_base;
  nve32_t   ret        = 0;

  ctrl = xpcs_read (xpcs_base, XPCS_SR_MII_CTRL);
  switch (speed) {
    case XPCS_USXG_AN_STS_SPEED_10:
      /* 10Mbps */
      ctrl &= ~(XPCS_SR_MII_CTRL_SS6 | XPCS_SR_MII_CTRL_SS13);
      break;
    case XPCS_USXG_AN_STS_SPEED_100:
      /* 100Mbps */
      ctrl |= XPCS_SR_MII_CTRL_SS13;
      ctrl &= ~XPCS_SR_MII_CTRL_SS6;
      break;
    case XPCS_USXG_AN_STS_SPEED_1000:
    default:
      /* 1000Mbps */
      ctrl |= XPCS_SR_MII_CTRL_SS6;
      ctrl &= ~XPCS_SR_MII_CTRL_SS13;
      break;
  }

  ret = xpcs_write_safety (osi_core, XPCS_SR_MII_CTRL, ctrl);
  return ret;
}

#endif

/**
 * @brief update_an_status - update AN status.
 *
 * Algorithm: This routine initialize AN status based on
 *              the USXGMII mode selected. Later this value
 *              will be overwritten if AN is enabled.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[out] an_status: Predefined AN status
 *
 */
static inline void
update_an_status (
  const struct osi_core_priv_data *const  osi_core,
  nveu32_t                                *an_status
  )
{
  /* initialize an_status based on DT, later overwite if AN is enabled */
  if (osi_core->phy_iface_mode == OSI_USXGMII_MODE_10G) {
    *an_status = XPCS_USXG_AN_STS_SPEED_10000;
  } else if (osi_core->phy_iface_mode == OSI_USXGMII_MODE_5G) {
    *an_status = XPCS_USXG_AN_STS_SPEED_5000;
  } else {
    /* do nothing */
  }
}

/**
 * @brief xpcs_start - Start XPCS
 *
 * Algorithm: This routine enables AN and set speed based on AN status
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
xpcs_start (
  struct osi_core_priv_data  *osi_core
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  an_status  = 0;
  nveu32_t  retry      = RETRY_COUNT;
  nveu32_t  count      = 0;
  nveu32_t  ctrl       = 0;
  nve32_t   ret        = 0;
  nve32_t   cond       = COND_NOT_MET;

  if ((osi_core->phy_iface_mode == OSI_USXGMII_MODE_10G) ||
      (osi_core->phy_iface_mode == OSI_USXGMII_MODE_5G))
  {
    /* initialize an_status based on DT, later overwite if AN is enabled */
    update_an_status (osi_core, &an_status);
    /* Skip AN in USXGMII mode if skip_usxgmii_an is configured in DT */
    if (osi_core->skip_usxgmii_an == OSI_DISABLE) {
      ctrl  = xpcs_read (xpcs_base, XPCS_SR_MII_CTRL);
      ctrl |= XPCS_SR_MII_CTRL_AN_ENABLE;
      ret   = xpcs_write_safety (osi_core, XPCS_SR_MII_CTRL, ctrl);
      if (ret != 0) {
        goto fail;
      }

      ret = xpcs_poll_for_an_complete (osi_core, &an_status);
      if (ret < 0) {
        goto fail;
      }
    }

    ret = xpcs_set_speed (osi_core, an_status);
    if (ret != 0) {
      goto fail;
    }

    /* USXGMII Rate Adaptor Reset before data transfer */
    ctrl  = xpcs_read (xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
    ctrl |= XPCS_VR_XS_PCS_DIG_CTRL1_USRA_RST;
    xpcs_write (xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1, ctrl);
    while (cond == COND_NOT_MET) {
      if (count > retry) {
        ret = -1;
        goto fail;
      }

      count++;

      ctrl = xpcs_read (xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
      if ((ctrl & XPCS_VR_XS_PCS_DIG_CTRL1_USRA_RST) == 0U) {
        cond = COND_MET;
      } else {
        osi_core->osd_ops.usleep (OSI_DELAY_1000US);
      }
    }
  }

  /* poll for FLT and Rx link up */
  ret = xpcs_poll_flt_rx_link (osi_core);

fail:
  return ret;
}

/**
 * @brief xlgpcs_start - Start XLGPCS
 *
 * Algorithm: This routine enables AN and set speed based on AN status
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
xlgpcs_start (
  struct osi_core_priv_data  *osi_core
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  retry      = RETRY_COUNT;
  nveu32_t  count      = 0;
  nveu32_t  ctrl       = 0;
  nve32_t   ret        = 0;
  nve32_t   cond       = COND_NOT_MET;

  if (xpcs_base == OSI_NULL) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "XLGPCS base is NULL",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  /* * XLGPCS programming guideline IAS section 7.1.3.2.2.2
   */
  if (osi_core->pcs_base_r_fec_en != OSI_ENABLE) {
    /* 4 Poll SR_PCS_CTRL1 reg RST bit */
    ctrl  = xpcs_read (xpcs_base, XLGPCS_SR_PCS_CTRL1);
    ctrl |= XLGPCS_SR_PCS_CTRL1_RST;
    xpcs_write (xpcs_base, XLGPCS_SR_PCS_CTRL1, ctrl);

    count = 0;
    while (cond == 1) {
      if (count > retry) {
        ret = -1;
        goto fail;
      }

      count++;
      ctrl = xpcs_read (xpcs_base, XLGPCS_SR_PCS_CTRL1);
      if ((ctrl & XLGPCS_SR_PCS_CTRL1_RST) == 0U) {
        cond = 0;
      } else {
        /* Maximum wait delay as per HW team is 10msec.
         * So add a loop for 1000 iterations with 1usec delay,
         * so that if check get satisfies before 1msec will come
         * out of loop and it can save some boot time
         */
        osi_core->osd_ops.usleep (10U);
      }
    }
  }

  /* 5 Program SR_AN_CTRL reg AN_EN bit to disable auto-neg */
  ctrl  = xpcs_read (xpcs_base, XLGPCS_SR_AN_CTRL);
  ctrl &= ~XLGPCS_SR_AN_CTRL_AN_EN;
  ret   = xpcs_write_safety (osi_core, XLGPCS_SR_AN_CTRL, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* 6 Wait for SR_PCS_STS1 reg RLU bit to set */
  cond  = COND_NOT_MET;
  count = 0;
  while (cond == COND_NOT_MET) {
    if (count > retry) {
      ret = -1;
      break;
    }

    count++;
    ctrl = xpcs_read (xpcs_base, XLGPCS_SR_PCS_STS1);
    if ((ctrl & XLGPCS_SR_PCS_STS1_RLU) ==
        XLGPCS_SR_PCS_STS1_RLU)
    {
      cond = COND_MET;
    } else {
      /* Maximum wait delay as per HW team is 10msec.
       * So add a loop for 1000 iterations with 1usec delay,
       * so that if check get satisfies before 1msec will come
       * out of loop and it can save some boot time
       */
      osi_core->osd_ops.usleep (10U);
    }
  }

fail:
  return ret;
}

/**
 * @brief xpcs_uphy_lane_bring_up - Bring up UPHY Tx/Rx lanes
 *
 * Algorithm: This routine bring up the UPHY Tx/Rx lanes
 * through XPCS FSM wrapper.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[in] lane_init_en: Tx/Rx lane init value.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
xpcs_uphy_lane_bring_up (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   lane_init_en
  )
{
  void            *xpcs_base = osi_core->xpcs_base;
  nveu32_t        retry      = RETRY_ONCE;
  nve32_t         cond       = COND_NOT_MET;
  nveu32_t        val        = 0;
  nveu32_t        count;
  nve32_t         ret                                   = 0;
  nveu64_t        retry_delay                           = OSI_DELAY_1US;
  const nveu32_t  uphy_status_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_XPCS_WRAP_UPHY_STATUS,
    XPCS_WRAP_UPHY_STATUS,
    T26X_XPCS_WRAP_UPHY_STATUS
  };
  const nveu32_t  uphy_init_ctrl_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_XPCS_WRAP_UPHY_HW_INIT_CTRL,
    XPCS_WRAP_UPHY_HW_INIT_CTRL,
    T26X_XPCS_WRAP_UPHY_HW_INIT_CTRL
  };

  if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
    /* Delay added as per HW team suggestion which is
     * of 100msec if equalizer is enabled for every
     * iteration of a lane bring sequence. So 100 * 1000
     * gives us a delay of 100msec for each retry of lane
     * bringup */
    retry       = 1000U;
    retry_delay = 100U;
  } else if (osi_core->mac_ver == OSI_EQOS_MAC_5_40) {
    /* Delay added as per HW team suggestion which is
     * of 10msec for eqos lane bring up. So 10 * 1000
     * gives us a delay of 10msec for each retry of lane
     * bringup */
    retry       = 1000U;
    retry_delay = 10U;
  } else {
    /* do nothing */
  }

  val = osi_readla (
          osi_core,
          (nveu8_t *)xpcs_base + uphy_status_reg[osi_core->mac]
          );
  if ((lane_init_en == XPCS_WRAP_UPHY_HW_INIT_CTRL_TX_EN) &&
      ((val & XPCS_WRAP_UPHY_STATUS_TX_P_UP_STATUS) ==
       XPCS_WRAP_UPHY_STATUS_TX_P_UP_STATUS))
  {
    goto done;
  } else {
    val = osi_readla (
            osi_core,
            (nveu8_t *)xpcs_base +
            uphy_init_ctrl_reg[osi_core->mac]
            );
    val |= lane_init_en;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)xpcs_base +
      uphy_init_ctrl_reg[osi_core->mac]
      );

    count = 0;
    while (cond == COND_NOT_MET) {
      val = osi_readla (
              osi_core,
              (nveu8_t *)xpcs_base +
              uphy_init_ctrl_reg[osi_core->mac]
              );
      if ((val & lane_init_en) == OSI_NONE) {
        /* exit loop */
        cond = COND_MET;
      } else {
        if (count > retry) {
          ret = -1;
          goto done;
        }

        count++;
        /* Apply delay based on retry_delay value */
        if (retry_delay == OSI_DELAY_1US) {
          osi_core->osd_ops.udelay (retry_delay);
        } else {
          osi_core->osd_ops.usleep (retry_delay);
        }
      }
    }
  }

done:
  return ret;
}

/**
 * @brief xpcs_check_pcs_lock_status - Checks whether PCS lock happened or not.
 *
 * Algorithm: This routine helps to check whether PCS lock happened or not.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
xpcs_check_pcs_lock_status (
  struct osi_core_priv_data  *osi_core
  )
{
  void            *xpcs_base = osi_core->xpcs_base;
  nveu32_t        retry      = RETRY_ONCE;
  nve32_t         cond       = COND_NOT_MET;
  nveu32_t        val        = 0;
  nveu32_t        count;
  nve32_t         ret                                    = 0;
  const nveu32_t  uphy_irq_sts_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_XPCS_WRAP_UPHY_INTERRUPT_STATUS,
    XPCS_WRAP_INTERRUPT_STATUS,
    T26X_XPCS_WRAP_INTERRUPT_STATUS
  };

  count = 0;
  while (cond == COND_NOT_MET) {
    val = osi_readla (
            osi_core,
            (nveu8_t *)xpcs_base +
            uphy_irq_sts_reg[osi_core->mac]
            );
    if ((val & XPCS_WRAP_IRQ_STATUS_PCS_LINK_STS) ==
        XPCS_WRAP_IRQ_STATUS_PCS_LINK_STS)
    {
      /* exit loop */
      cond = COND_MET;
    } else {
      if (count >= retry) {
        ret = -1;
        goto fail;
      }

      count++;

      /* Maximum wait delay as per HW team is 1msec. */
      osi_core->osd_ops.usleep (OSI_DELAY_1000US);
    }
  }

  /* Clear the status */
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)xpcs_base +
    uphy_irq_sts_reg[osi_core->mac]
    );
fail:
  return ret;
}

/**
 * @brief xpcs_lane_bring_up - Bring up UPHY Tx/Rx lanes
 *
 * Algorithm: This routine bring up the UPHY Tx/Rx lanes
 * through XPCS FSM wrapper.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
xpcs_lane_bring_up (
  struct osi_core_priv_data  *osi_core
  )
{
  struct core_local  *l_core = (struct core_local *)(void *)osi_core;
  nveu32_t           retry   = 7U;
  nveu32_t           count;
  nveu32_t           val = 0;
  nve32_t            cond;
  nve32_t            ret = 0;

  if (xpcs_uphy_lane_bring_up (
        osi_core,
        XPCS_WRAP_UPHY_HW_INIT_CTRL_TX_EN
        ) < 0)
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "UPHY TX lane bring-up failed\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  if (osi_core->mac != OSI_MAC_HW_MGBE) {
    if (xpcs_uphy_lane_bring_up (
          osi_core,
          XPCS_WRAP_UPHY_HW_INIT_CTRL_RX_EN
          ) < 0)
    {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "UPHY RX lane bring-up failed\n",
        0ULL
        );

      /* Peform UPHY Rx lane power down on Lane bring up failure path.
       * FIXME: Discuss with HW team further whether this is necessary step or not
       */
      if (xpcs_uphy_lane_bring_up (
            osi_core,
            XPCS_WRAP_UPHY_HW_INIT_CTRL_RX_P_DN
            ) < 0)
      {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "UPHY Rx lane power down failed\n",
          0ULL
          );
      }

      ret = -1;
      goto fail;
    }
  } else {
    if (l_core->lane_powered_up == OSI_ENABLE) {
      goto step10;
    }

    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );
    /* Step1 RX_SW_OVRD */
    val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_SW_OVRD;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );

    /* Step2 RX_IDDQ */
    val &= ~(XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_IDDQ);
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );

    /* Step2 AUX_RX_IDDQ */
    val &= ~(XPCS_WRAP_UPHY_RX_CONTROL_0_0_AUX_RX_IDDQ);
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    /* Step3: wait for 1usec, HW recommended value is 50nsec minimum */
    osi_core->osd_ops.udelay (1U);

    /* Step4 RX_SLEEP */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );
    val &= ~(XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_SLEEP);
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    /* Step5: wait for 1usec, HW recommended value is 500nsec minimum */
    osi_core->osd_ops.udelay (1U);

    /* Step6 RX_CAL_EN */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );
    val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CAL_EN;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    /* Step7 poll for Rx cal enable */
    cond  = COND_NOT_MET;
    count = 0;
    while (cond == COND_NOT_MET) {
      val = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->xpcs_base +
              XPCS_WRAP_UPHY_RX_CONTROL_0_0
              );
      if ((val & XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CAL_EN) == 0U) {
        cond = COND_MET;
      } else {
        if (count > retry) {
          ret = -1;
          goto fail;
        }

        count++;

        /* Maximum wait delay as per HW team is 100 usec.
         * But most of the time as per experiments it takes
         * around 14usec to satisy the condition.
         * Use 200US to yield CPU for other users.
         */
        osi_core->osd_ops.usleep (OSI_DELAY_200US);
      }
    }

    /* Step8: wait for 1usec, HW recommended value is 50nsec minimum */
    osi_core->osd_ops.udelay (1U);

    /* Step9 RX_DATA_EN */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );
    val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_DATA_EN;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    /* set lane_powered_up to OSI_ENABLE */
    l_core->lane_powered_up = OSI_ENABLE;

step10:

    /* Step10 reset RX_PCS_PHY_RDY */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );
    val &= ~XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_PCS_PHY_RDY;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    /* Step11: wait for 1usec, HW recommended value is 50nsec minimum */
    osi_core->osd_ops.udelay (1U);

    /* Step12 RX_CDR_RESET */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );
    val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CDR_RESET;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    /* Step13: wait for 1usec, HW recommended value is 50nsec minimum */
    osi_core->osd_ops.udelay (1U);

    /* Step14 RX_PCS_PHY_RDY */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );
    val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_PCS_PHY_RDY;
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );
    /* Step14: wait for 30ms */
    osi_core->osd_ops.usleep (OSI_DELAY_30000US);

    /* Step15 RX_CDR_RESET */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->xpcs_base +
            XPCS_WRAP_UPHY_RX_CONTROL_0_0
            );
    val &= ~(XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CDR_RESET);
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->xpcs_base +
      XPCS_WRAP_UPHY_RX_CONTROL_0_0
      );

    /* Step16: wait for 30ms */
    osi_core->osd_ops.usleep (OSI_DELAY_30000US);
  }

  if (xpcs_check_pcs_lock_status (osi_core) < 0) {
    if (l_core->lane_status == OSI_ENABLE) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Failed to get PCS block lock\n",
        0ULL
        );
    }

    ret = -1;
    goto fail;
  } else {
    OSI_CORE_INFO (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "PCS block lock SUCCESS\n",
      0ULL
      );
  }

fail:
  return ret;
}

static nve32_t
vendor_specifc_sw_rst_usxgmii_an_en (
  struct osi_core_priv_data  *osi_core
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  retry      = 1000;
  nveu32_t  count;
  nveu32_t  ctrl = 0;
  nve32_t   cond = 1;
  nve32_t   ret  = 0;

  ctrl  = xpcs_read (xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
  ctrl |= XPCS_VR_XS_PCS_DIG_CTRL1_USXG_EN;
  ret   = xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_DIG_CTRL1, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* XPCS_VR_XS_PCS_DIG_CTRL1_VR_RST bit is self clearing
   * value readback varification is not needed
   */
  ctrl |= XPCS_VR_XS_PCS_DIG_CTRL1_VR_RST;
  xpcs_write (xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1, ctrl);

  /* 6. Programming for Synopsys PHY - NA */

  /* 7. poll until vendor specific software reset */
  cond  = 1;
  count = 0;
  while (cond == 1) {
    if (count > retry) {
      ret = -1;
      goto fail;
    }

    count++;

    ctrl = xpcs_read (xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
    if ((ctrl & XPCS_VR_XS_PCS_DIG_CTRL1_VR_RST) == 0U) {
      cond = 0;
    } else {
      osi_core->osd_ops.usleep (OSI_DELAY_1000US);
    }
  }

  /* 8. Backplane Ethernet PCS configurations
   * clear AN_EN in SR_AN_CTRL
   * set CL37_BP in VR_XS_PCS_DIG_CTRL1
   */
  if ((osi_core->phy_iface_mode == OSI_USXGMII_MODE_10G) ||
      (osi_core->phy_iface_mode == OSI_USXGMII_MODE_5G))
  {
    ctrl  = xpcs_read (xpcs_base, XPCS_SR_AN_CTRL);
    ctrl &= ~XPCS_SR_AN_CTRL_AN_EN;
    ret   = xpcs_write_safety (osi_core, XPCS_SR_AN_CTRL, ctrl);
    if (ret != 0) {
      goto fail;
    }

    ctrl  = xpcs_read (xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
    ctrl |= XPCS_VR_XS_PCS_DIG_CTRL1_CL37_BP;
    ret   = xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_DIG_CTRL1, ctrl);
    if (ret != 0) {
      goto fail;
    }
  }

fail:
  return ret;
}

/**
 * @brief xpcs_base_r_fec - Enable BASE-R FEC based on sysfs setting
 *
 * Algorithm: This routine enable or disable the PCS BASE-R FEC.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
xpcs_base_r_fec (
  struct osi_core_priv_data  *osi_core
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  ctrl       = 0;
  nve32_t   ret        = 0;

  if ((osi_core->pcs_base_r_fec_en == OSI_ENABLE) &&
      (osi_core->uphy_gbe_mode == OSI_GBE_MODE_25G))
  {
    /* Program SR_AN_CTRL reg AN_EN bit to disable auto-neg */
    ctrl  = xpcs_read (xpcs_base, XLGPCS_SR_AN_CTRL);
    ctrl &= ~XLGPCS_SR_AN_CTRL_AN_EN;
    ret   = xpcs_write_safety (osi_core, XLGPCS_SR_AN_CTRL, ctrl);
    if (ret != 0) {
      goto fail;
    }

    osi_writela (
      osi_core,
      XPCS_WRAP_UPHY_TIMEOUT_CONTROL_0_0_VALUE,
      (nveu8_t *)osi_core->xpcs_base +
      T26X_XPCS_WRAP_UPHY_TIMEOUT_CONTROL_0_0
      );
  }

  /* Enable/Disable BASE-R FEC */
  ctrl = xpcs_read (xpcs_base, XPCS_SR_PMA_KR_FEC_CTRL);
  if (osi_core->pcs_base_r_fec_en == OSI_ENABLE) {
    ctrl |= (XPCS_SR_PMA_KR_FEC_CTRL_FEC_EN |
             XPCS_SR_PMA_KR_FEC_CTRL_EN_ERR_IND);
  } else {
    ctrl &= ~(XPCS_SR_PMA_KR_FEC_CTRL_FEC_EN |
              XPCS_SR_PMA_KR_FEC_CTRL_EN_ERR_IND);
  }

  ret = xpcs_write_safety (osi_core, XPCS_SR_PMA_KR_FEC_CTRL, ctrl);
  if (ret != 0) {
    goto fail;
  }

fail:
  return ret;
}

/**
 * @brief xpcs_init - XPCS initialization
 *
 * Algorithm: This routine initialize XPCS in USXMII mode.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
xpcs_init (
  struct osi_core_priv_data  *osi_core
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  value      = 0;
  nveu32_t  ctrl       = 0;
  nve32_t   ret        = 0;

  if (osi_core->pre_sil == 0x1U) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Pre-silicon, skipping lane bring up",
      0ULL
      );
  } else {
    /* Enable/Disable BASE-R FEC before lane bring up */
    ret = xpcs_base_r_fec (osi_core);
    if (ret != 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "xpcs_base_r_fec failed",
        0ULL
        );
      goto fail;
    }

    if ((osi_core->mac == OSI_MAC_HW_MGBE_T26X) &&
        (osi_core->uphy_gbe_mode == OSI_GBE_MODE_10G))
    {
      /* Added below programming sequence from hw scripts */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->xpcs_base +
                T26X_XPCS_WRAP_CONFIG_0
                );
      value &= ~OSI_BIT(0);
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_CONFIG_0
        );
      osi_writela (
        osi_core,
        XPCS_10G_WRAP_UPHY_RX_CTRL_2_SLEEP_CAL_EN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_2
        );
      osi_writela (
        osi_core,
        XPCS_10G_WRAP_UPHY_RX_CTRL_2_SLEEP_CAL_EN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_2
        );
      osi_writela (
        osi_core,
        XPCS_10G_WRAP_UPHY_TX_CTRL_3_DATAREADY_DATAEN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_3
        );
      osi_writela (
        osi_core,
        XPCS_10G_WRAP_UPHY_RX_CTRL_3_CAL_DONE_DATA_EN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_3
        );
      osi_writela (
        osi_core,
        XLGPCS_WRAP_UPHY_TO_CTRL2_EQ_DONE_TOV,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_T0_CTRL_2_0
        );
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->xpcs_base +
                T26X_XPCS_WRAP_UPHY_RX_CTRL_5_0
                );
      value |= XLGPCS_WRAP_UPHY_RX_CTRL5_RX_EQ_ENABLE;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_5_0
        );
    }

    if (xpcs_lane_bring_up (osi_core) < 0) {
      ret = -1;
      goto fail;
    }
  }

  /* Switching to USXGMII Mode based on
   * XPCS programming guideline 7.6
   */

  /* 1. switch DWC_xpcs to BASE-R mode */
  ctrl  = xpcs_read (xpcs_base, XPCS_SR_XS_PCS_CTRL2);
  ctrl |= XPCS_SR_XS_PCS_CTRL2_PCS_TYPE_SEL_BASE_R;
  ret   = xpcs_write_safety (osi_core, XPCS_SR_XS_PCS_CTRL2, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* 2. enable USXGMII Mode inside DWC_xpcs */

  /* 3.  USXG_MODE = 10G - default it will be 10G mode */
  if ((osi_core->phy_iface_mode == OSI_USXGMII_MODE_10G) ||
      (osi_core->phy_iface_mode == OSI_USXGMII_MODE_5G))
  {
    ctrl  = xpcs_read (xpcs_base, XPCS_VR_XS_PCS_KR_CTRL);
    ctrl &= ~(XPCS_VR_XS_PCS_KR_CTRL_USXG_MODE_MASK);

    if (osi_core->uphy_gbe_mode == OSI_GBE_MODE_5G) {
      ctrl |= XPCS_VR_XS_PCS_KR_CTRL_USXG_MODE_5G;
    }
  }

  ret = xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_KR_CTRL, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* 4. Program PHY to operate at 10Gbps/5Gbps/2Gbps
   * this step not required since PHY speed programming
   * already done as part of phy INIT
   */
  /* 5. Vendor specific software reset */
  ret = vendor_specifc_sw_rst_usxgmii_an_en (osi_core);

fail:
  return ret;
}

/**
 * @brief xlgpcs_init - XLGPCS initialization
 *
 * Algorithm: This routine initialize XLGPCS in USXMII mode.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
xlgpcs_init (
  struct osi_core_priv_data  *osi_core
  )
{
 #if 0 // FIXME: matching with HW script sequence for 25G
  nveu32_t  retry = 1000;
  nveu32_t  count;
  nve32_t   cond = COND_NOT_MET;
 #endif
  nve32_t   ret   = 0;
  nveu32_t  value = 0;

  if (osi_core->xpcs_base == OSI_NULL) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "XLGPCS base is NULL",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  if (osi_core->pre_sil == 0x1U) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Pre-silicon, skipping lane bring up",
      0ULL
      );
  } else {
    /* Select XLGPCS in wrapper register */
    if ((osi_core->mac == OSI_MAC_HW_MGBE_T26X) &&
        (osi_core->uphy_gbe_mode == OSI_GBE_MODE_25G))
    {
      /* Added below programming sequence from hw scripts */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->xpcs_base +
                T26X_XPCS_WRAP_CONFIG_0
                );
      value |= OSI_BIT (0);
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_CONFIG_0
        );

      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_RX_CTRL_4_CDR_RESET_WIDTH,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_4
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_1_IDDQ_SLEEP_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_1
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_1_IDDQ_SLEEP_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_1
        );
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->xpcs_base +
                T26X_XPCS_WRAP_UPHY_RX_CTRL_P_DN_0
                );
      value = value & XPCS_WRAP_UPHY_RX_CTRL_P_DN_0_RXEN_SLEEP_DLY;
      value = value | XPCS_WRAP_UPHY_RX_CTRL_P_DN_0_SLEEP_IDDQ_DLY;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_P_DN_0
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_P_DN_0_DATARDY_SLEEP_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_0
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_P_DN_1_DATAEN_RDY_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_1
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_P_DN_2_SLEEP_IDDQ_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_2
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_P_DN_3_IDDQ_P_DN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_3
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_P_DN_4_CAL_DONE_SLP_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_4
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_4_CAL_EN_HIGH_LOW_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_4
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_5_CAL_EN_LOW_DTRDY_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_5
        );
      osi_writela (
        osi_core,
        XLGPCS_WRAP_UPHY_TO_CTRL2_EQ_DONE_TOV,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_T0_CTRL_2_0
        );
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->xpcs_base +
                T26X_XPCS_WRAP_UPHY_RX_CTRL_5_0
                );
      value |= XLGPCS_WRAP_UPHY_RX_CTRL5_RX_EQ_ENABLE;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_5_0
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_RX_CTRL_7_EQ_RESET_WIDTH,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_7
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_RX_CTRL_8_EQ_TRAIN_EN_DELAY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_8
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_RX_CTRL_9_EQ_TRAIN_EN_HILO_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_9
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_TX_CTRL_3_DATAREADY_DATAEN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_3
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_RX_CTRL_2_SLEEP_CAL_EN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_TX_CTRL_2
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_RX_CTRL_2_SLEEP_CAL_EN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_2
        );
      osi_writela (
        osi_core,
        XPCS_WRAP_UPHY_RX_CTRL_3_CAL_DONE_DATA_EN_DLY,
        (nveu8_t *)osi_core->xpcs_base +
        T26X_XPCS_WRAP_UPHY_RX_CTRL_3
        );
    }

    /* Enable/Disable BASE-R FEC before lane bring up */
    ret = xpcs_base_r_fec (osi_core);
    if (ret != 0) {
      goto fail;
    }

    if (xpcs_lane_bring_up (osi_core) < 0) {
      ret = -1;
      goto fail;
    }
  }

  /* As a part of bringup debug, below programming is leading to the failures in block lock
   * in XFI mode. This programming is not done in the HW scripts, but mentioned as a part of IAS.
   * For now commenting it and need to be checked with HW team and enable it when required
  */

 #if 0 // FIXME: matching with HW script sequence for 25G

  /* Switching to USXGMII Mode to 25G based on
   * XLGPCS programming guideline IAS section 7.1.3.2.2.1
   */
  /* 1.Program SR_PCS_CTRL1 reg SS_5_2 bits */
  ctrl  = xpcs_read (xpcs_base, XLGPCS_SR_PCS_CTRL1);
  ctrl &= ~XLGPCS_SR_PCS_CTRL1_SS5_2_MASK;
  ctrl |= XLGPCS_SR_PCS_CTRL1_SS5_2;
  ret   = xpcs_write_safety (osi_core, XLGPCS_SR_PCS_CTRL1, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* 2.Program SR_PCS_CTRL2 reg PCS_TYPE_SEL bits */
  ctrl  = xpcs_read (xpcs_base, XLGPCS_SR_PCS_CTRL2);
  ctrl &= ~XLGPCS_SR_PCS_CTRL2_PCS_TYPE_SEL_MASK;
  ctrl |= XLGPCS_SR_PCS_CTRL2_PCS_TYPE_SEL;
  ret   = xpcs_write_safety (osi_core, XLGPCS_SR_PCS_CTRL2, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* 3.Program SR_PMA_CTRL2 reg PMA_TYPE bits */
  ctrl  = xpcs_read (xpcs_base, XLGPCS_SR_PMA_CTRL2);
  ctrl &= ~XLGPCS_SR_PMA_CTRL2_PMA_TYPE_MASK;
  ctrl |= XLGPCS_SR_PMA_CTRL2_PMA_TYPE;
  ret   = xpcs_write_safety (osi_core, XLGPCS_SR_PMA_CTRL2, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* 4.NA [Program VR_PCS_MMD Digital Control3 reg EN_50G bit
   * to disable 50G] 25G mode selected for T264 */
  /* 5.Program VR_PCS_MMD Digital Control3 reg CNS_EN bit to 1 to
   * enable 25G as per manual */
  ctrl  = xpcs_read (xpcs_base, XLGPCS_VR_PCS_DIG_CTRL3);
  ctrl |= XLGPCS_VR_PCS_DIG_CTRL3_CNS_EN;
  ret   = xpcs_write_safety (osi_core, XLGPCS_VR_PCS_DIG_CTRL3, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* 6.NA. Enable RS FEC */
  /* 7. Enable BASE-R FEC */
  ctrl  = xpcs_read (xpcs_base, XLGPCS_SR_PMA_KR_FEC_CTRL);
  ctrl |= XLGPCS_SR_PMA_KR_FEC_CTRL_FEC_EN;
  ret   = xpcs_write_safety (osi_core, XLGPCS_SR_PMA_KR_FEC_CTRL, ctrl);
  if (ret != 0) {
    goto fail;
  }

  /* 8.NA, Configure PHY to 25G rate */
  /* 9.Program VR_PCS_DIG_CTRL1 reg VR_RST bit */
  ctrl  = xpcs_read (xpcs_base, XLGPCS_VR_PCS_DIG_CTRL1);
  ctrl |= XLGPCS_VR_PCS_DIG_CTRL1_VR_RST;
  xpcs_write (xpcs_base, XLGPCS_VR_PCS_DIG_CTRL1, ctrl);
  /* 10.Wait for VR_PCS_DIG_CTRL1 reg VR_RST bit to self clear */
  count = 0;
  while (cond == COND_NOT_MET) {
    if (count > retry) {
      ret = -1;
      goto fail;
    }

    count++;
    ctrl = xpcs_read (xpcs_base, XLGPCS_VR_PCS_DIG_CTRL1);
    if ((ctrl & XLGPCS_VR_PCS_DIG_CTRL1_VR_RST) == 0U) {
      cond = 0;
    } else {
      /* Maximum wait delay as per HW team is 10msec.
       * So add a loop for 1000 iterations with 1usec delay,
       * so that if check get satisfies before 1msec will come
       * out of loop and it can save some boot time
       */
      osi_core->osd_ops.udelay (10U);
    }
  }

 #endif
fail:
  return ret;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief xlgpcs_eee - XLGPCS enable/disable EEE
 *
 * Algorithm: This routine update register related to EEE
 * for XLGPCS.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[in] en_dis: enable - 1 or disable - 0
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
xlgpcs_eee (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   en_dis
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  val        = 0x0U;
  nve32_t   ret        = 0;
  nveu32_t  retry      = 1000U;
  nveu32_t  count      = 0;
  nve32_t   cond       = COND_NOT_MET;

  if ((en_dis != OSI_ENABLE) && (en_dis != OSI_DISABLE)) {
    ret = -1;
    goto fail;
  }

  if (xpcs_base == OSI_NULL) {
    ret = -1;
    goto fail;
  }

  if (en_dis == OSI_DISABLE) {
    val  = xpcs_read (xpcs_base, XLGPCS_VR_PCS_EEE_MCTRL);
    val &= ~XPCS_VR_XS_PCS_EEE_MCTRL0_LTX_EN;
    val &= ~XPCS_VR_XS_PCS_EEE_MCTRL0_LRX_EN;
    ret  = xpcs_write_safety (osi_core, XLGPCS_VR_PCS_EEE_MCTRL, val);

    /* To disable EEE on TX side, the software must wait for
     * TX LPI to enter TX_ACTIVE state by reading
     * VR_PCS_DIG_STS Register
     */
    while (cond == COND_NOT_MET) {
      if (count > retry) {
        ret = -1;
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "EEE active state timeout!",
          0ULL
          );
        goto fail;
      }

      count++;
      val = xpcs_read (xpcs_base, XLGPCS_VR_PCS_DIG_STS);
      if ((val & XLGPCS_VR_PCS_DIG_STSLTXRX_STATE) == 0U) {
        cond = 0;
      } else {
        osi_core->osd_ops.usleep (100U);
      }
    }
  } else {
    /* 1. Check if DWC_xlgpcs supports the EEE feature
     * by reading the SR_PCS_EEE_ABL reg. For 25G always enabled
     * by default
     */

    /* 2. Program various timers used in the EEE mode depending on
     * the clk_eee_i clock frequency. default timers are same as
     * IEEE std clk_eee_i() is 108MHz. MULT_FACT_100NS = 9
     * because 9.2ns*10 = 92 which is between 80 and 120 this
     * leads to default setting match.
     */

    /* 3. NA. [If FEC is enabled in the KR mode] */
    /* 4. NA. [Enable fast_sim mode] */

    /* 5. NA [If RS FEC is enabled, program AM interval and RS FEC]
     */
    /* 6. NA [Fast wake is not enabled default] */
    /* 7. Enable the EEE feature on Tx and Rx path */
    val  = xpcs_read (xpcs_base, XLGPCS_VR_PCS_EEE_MCTRL);
    val |= (XPCS_VR_XS_PCS_EEE_MCTRL0_LTX_EN |
            XPCS_VR_XS_PCS_EEE_MCTRL0_LRX_EN);
    ret = xpcs_write_safety (osi_core, XLGPCS_VR_PCS_EEE_MCTRL, val);
    if (ret != 0) {
      goto fail;
    }

    /* 8. NA [If PMA service interface is XLAUI or CAUI] */
  }

fail:
  return ret;
}

#endif /* !OSI_STRIPPED_LIB */
