// SPDX-License-Identifier: MIT

/* SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <osi_core.h>
#include "nvxpcs.h"

/**
 * @brief eqos_xpcs_poll_for_an_complete - Polling for AN complete.
 *
 * Algorithm: This routine poll for AN completion status from
 *              EQOS XPCS IP.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[out] an_status: AN status from XPCS
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
#if 0  // FIXME: Not used for SLT EQOS bring up
static inline nve32_t
eqos_xpcs_poll_for_an_complete (
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

  /* Poll for AN complete */
  cond  = 1;
  count = 0;
  while (cond == 1) {
    if (count > retry) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "EQOS XPCS AN completion timed out\n",
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
    status = nv_xpcs_read (xpcs_base, XPCS_VR_MII_AN_INTR_STS);
    if ((status & XPCS_VR_MII_AN_INTR_STS_CL37_ANCMPLT_INTR) == 0U) {
      /* autoneg not completed - poll */
      osi_core->osd_ops.udelay (1000U);
    } else {
      /* Clear interrupt */
      status &= ~XPCS_VR_MII_AN_INTR_STS_CL37_ANCMPLT_INTR;
      ret     = nv_xpcs_write_safety (osi_core, XPCS_VR_MII_AN_INTR_STS, status);
      if (ret != 0) {
        goto fail;
      }

      cond = 0;
    }
  }

  if ((status & EQOS_XPCS_VR_MII_AN_INTR_STS_LINK_UP) == 0U) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "EQOS XPCS AN completed but link is down\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  *an_status = status;
fail:
  return ret;
}

#endif

/**
 * @brief eqos_xpcs_init - EQOS XPCS initialization
 *
 * Algorithm: This routine initialize XPCS in SGMII mode.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
eqos_xpcs_init (
  struct osi_core_priv_data  *osi_core
  )
{
  void  *xpcs_base = osi_core->xpcs_base;
  //    nveu32_t an_status = 0;
  nveu32_t  retry = 1000;
  nveu32_t  count;
  nveu32_t  ctrl = 0;
  nve32_t   cond = 1;
  nve32_t   ret  = 0;

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

  if (osi_core->pre_sil == 0x1U) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Pre-silicon, skipping lane bring up",
      0ULL
      );
  } else {
    if (xpcs_lane_bring_up (osi_core) < 0) {
      ret = -1;
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "XPCS bring up failed",
        0ULL
        );
      goto fail;
    }
  }

  /* Init XPCS controller based on
   * DWC XPCS programming guideline 7.1
   */

  /* 1. NA, Switch on power supply */
  /* 2. NA, Wait as per PHY requirements */
  /* 3. NA, De-assert reset */
  /* 4. NA, Configure multi-protocol */
  /* 5. Read SR_MII_CTRL register and wait for 15 bit read as 0 */
  cond  = 1;
  count = 0;
  while (cond == 1) {
    if (count > retry) {
      ret = -1;
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "XPCS timeout!!",
        0ULL
        );
      goto fail;
    }

    count++;
    ctrl = nv_xpcs_read (xpcs_base, NV_XPCS_SR_MII_CTRL);
    if ((ctrl & XPCS_SR_MII_CTRL_RST) == 0U) {
      cond = 0;
    } else {
      osi_core->osd_ops.usleep (100U);
    }
  }

 #if 0 // FIXME: Re-visit below steps, not required for SLT EQOS
  ctrl  = nv_xpcs_read (xpcs_base, XPCS_VS_MII_MMD_VR_MII_AN_CTRL_0);
  ctrl |= (XPCS_VS_MII_MMD_VR_MII_AN_CTRL_AN_INTR_EN |
           XPCS_VS_MII_MMD_VR_MII_AN_CTRL_PCS_MODE);
  ret = nv_xpcs_write_safety (osi_core, XPCS_VS_MII_MMD_VR_MII_AN_CTRL_0, ctrl);
  if (ret != 0) {
    goto fail;
  }

  ctrl  = nv_xpcs_read (xpcs_base, XPCS_SR_MII_CTRL);
  ctrl |= XPCS_SR_MII_CTRL_RESTART_AN;
  nv_xpcs_write_safety (osi_core, XPCS_SR_MII_CTRL, ctrl);
  osi_core->osd_ops.udelay (1000*100U);

  ret = eqos_xpcs_poll_for_an_complete (osi_core, &an_status);
  if (ret < 0) {
    goto fail;
  }

  OSI_CORE_INFO (
    osi_core->osd,
    OSI_LOG_ARG_HW_FAIL,
    "EQOS XPCS AN Status",
    an_status
    );
  ret = eqos_xpcs_set_speed (osi_core, an_status);
  if (ret != 0) {
    goto fail;
  }

 #endif

  /* 7. NA */
  /* 8. NA */
  /* 9. Wait for LINK_STS of SR_MII_STS Register bit to become 1 */
  cond  = 1;
  count = 0;
  while (cond == 1) {
    if (count > retry) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "XPCS LINK_STS timeout!!",
        0ULL
        );
 #ifdef HSI_SUPPORT

      /* T264-EQOS_HSIv2-59     Link Training Status Register
       * monitoring during Link Training in EQOS PCS
       */
      if (osi_core->hsi.enabled == OSI_ENABLE) {
        osi_core->hsi.err_code[PCS_LNK_ERR_IDX]         = OSI_PCS_LNK_ERR;
        osi_core->hsi.report_err                        = OSI_ENABLE;
        osi_core->hsi.report_count_err[PCS_LNK_ERR_IDX] = OSI_ENABLE;
      }

 #endif
      ret = -1;
      goto fail;
    }

    count++;
    ctrl = nv_xpcs_read (xpcs_base, XPCS_SR_MII_STS_0);
    if ((ctrl & XPCS_SR_MII_STS_0_LINK_STS) ==
        XPCS_SR_MII_STS_0_LINK_STS)
    {
      cond = 0;
    } else {
      osi_core->osd_ops.usleep (100U);
    }
  }

fail:
  return ret;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief xpcs_eee - XPCS enable/disable EEE
 *
 * Algorithm: This routine update register related to EEE
 * for XPCS.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[in] en_dis: enable - 1 or disable - 0
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
xpcs_eee (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   en_dis
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  val        = 0x0U;
  nve32_t   ret        = 0;

  if ((en_dis != OSI_ENABLE) && (en_dis != OSI_DISABLE)) {
    ret = -1;
    goto fail;
  }

  if (xpcs_base == OSI_NULL) {
    ret = -1;
    goto fail;
  }

  if (en_dis == OSI_DISABLE) {
    val  = nv_xpcs_read (xpcs_base, XPCS_VR_XS_PCS_EEE_MCTRL0);
    val &= ~XPCS_VR_XS_PCS_EEE_MCTRL0_LTX_EN;
    val &= ~XPCS_VR_XS_PCS_EEE_MCTRL0_LRX_EN;
    ret  = nv_xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_EEE_MCTRL0, val);
  } else {
    /* 1. Check if DWC_xpcs supports the EEE feature by
     * reading the SR_XS_PCS_EEE_ABL register
     * 1000BASEX-Only is different config then else so can (skip)
     */

    /* 2. Program various timers used in the EEE mode depending on the
     * clk_eee_i clock frequency. default times are same as IEEE std
     * clk_eee_i() is 102MHz. MULT_FACT_100NS = 9 because 9.8ns*10 = 98
     * which is between 80 and 120  this leads to default setting match
     */

    val = nv_xpcs_read (xpcs_base, XPCS_VR_XS_PCS_EEE_MCTRL0);
    /* 3. If FEC is enabled in the KR mode (skip in FPGA)*/
    /* 4. enable the EEE feature on the Tx path and Rx path */
    val |= (XPCS_VR_XS_PCS_EEE_MCTRL0_LTX_EN |
            XPCS_VR_XS_PCS_EEE_MCTRL0_LRX_EN);
    ret = nv_xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_EEE_MCTRL0, val);
    if (ret != 0) {
      goto fail;
    }

    /* Transparent Tx LPI Mode Enable */
    val  = nv_xpcs_read (xpcs_base, XPCS_VR_XS_PCS_EEE_MCTRL1);
    val |= XPCS_VR_XS_PCS_EEE_MCTRL1_TRN_LPI;
    ret  = nv_xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_EEE_MCTRL1, val);
  }

fail:
  return ret;
}

#endif

/**
 * @brief mixed_bank_reg_prog - programs the mixed bank registers in non-Tegra chips
 *
 * Algorithm: This routine update the mixed bank registers
 * for XPCS.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t
mixed_bank_reg_prog (
  struct osi_core_priv_data  *osi_core
  )
{
  (void)osi_core;

  return 0;
}
