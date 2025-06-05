// SPDX-License-Identifier: MIT

/* SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <osi_core.h>
#include "eqos_core.h"
#include "eqos_mmc.h"
#include "core_local.h"
#include "core_common.h"
#include "xpcs.h"

/*
 * Forward declarations of local functions.
 */
static nve32_t
eqos_post_pad_calibrate (
  struct osi_core_priv_data *const  osi_core
  );

static nve32_t
eqos_pre_pad_calibrate (
  struct osi_core_priv_data *const  osi_core
  );

#ifndef OSI_STRIPPED_LIB

/**
 * @brief eqos_config_flow_control - Configure MAC flow control settings
 *
 * @note
 * Algorithm:
 *  - Validate flw_ctrl for validity and return -1 if fails.
 *  - Configure Tx and(or) Rx flow control registers based on flw_ctrl.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] flw_ctrl: flw_ctrl settings.
 *  -  If OSI_FLOW_CTRL_TX set, enable TX flow control, else disable.
 *  -  If OSI_FLOW_CTRL_RX set, enable RX flow control, else disable.
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_flow_control (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    flw_ctrl
  )
{
  nveu32_t  val;
  void      *addr = osi_core->base;

  /* return on invalid argument */
  if (flw_ctrl > (OSI_FLOW_CTRL_RX | OSI_FLOW_CTRL_TX)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "flw_ctr: invalid input\n",
      0ULL
      );
    return -1;
  }

  /* Configure MAC Tx Flow control */
  /* Read MAC Tx Flow control Register of Q0 */
  val = osi_readla (
          osi_core,
          (nveu8_t *)addr + EQOS_MAC_QX_TX_FLW_CTRL (0U)
          );

  /* flw_ctrl BIT0: 1 is for tx flow ctrl enable
   * flw_ctrl BIT0: 0 is for tx flow ctrl disable
   */
  if ((flw_ctrl & OSI_FLOW_CTRL_TX) == OSI_FLOW_CTRL_TX) {
    /* Enable Tx Flow Control */
    val |= EQOS_MAC_QX_TX_FLW_CTRL_TFE;
    /* Mask and set Pause Time */
    val &= ~EQOS_MAC_PAUSE_TIME_MASK;
    val |= EQOS_MAC_PAUSE_TIME & EQOS_MAC_PAUSE_TIME_MASK;
  } else {
    /* Disable Tx Flow Control */
    val &= ~EQOS_MAC_QX_TX_FLW_CTRL_TFE;
  }

  /* Write to MAC Tx Flow control Register of Q0 */
  osi_writela (osi_core, val, (nveu8_t *)addr + EQOS_MAC_QX_TX_FLW_CTRL (0U));

  /* Configure MAC Rx Flow control*/
  /* Read MAC Rx Flow control Register */
  val = osi_readla (
          osi_core,
          (nveu8_t *)addr + EQOS_MAC_RX_FLW_CTRL
          );

  /* flw_ctrl BIT1: 1 is for rx flow ctrl enable
   * flw_ctrl BIT1: 0 is for rx flow ctrl disable
   */
  if ((flw_ctrl & OSI_FLOW_CTRL_RX) == OSI_FLOW_CTRL_RX) {
    /* Enable Rx Flow Control */
    val |= EQOS_MAC_RX_FLW_CTRL_RFE;
  } else {
    /* Disable Rx Flow Control */
    val &= ~EQOS_MAC_RX_FLW_CTRL_RFE;
  }

  /* Write to MAC Rx Flow control Register */
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)addr + EQOS_MAC_RX_FLW_CTRL
    );

  return 0;
}

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief eqos_pad_calibrate - performs PAD calibration
 *
 * @note
 * Algorithm:
 *  - Set field PAD_E_INPUT_OR_E_PWRD in reg ETHER_QOS_SDMEMCOMPPADCTRL_0
 *  - Delay for 1 usec.
 *  - Set AUTO_CAL_ENABLE and AUTO_CAL_START in reg
 *    ETHER_QOS_AUTO_CAL_CONFIG_0
 *  - Wait on AUTO_CAL_ACTIVE until it is 0 for a loop of 1000 with a sleep of 10 microsecond
 *    between itertions.
 *  - Re-program the value PAD_E_INPUT_OR_E_PWRD in
 *    ETHER_QOS_SDMEMCOMPPADCTRL_0 to save power
 *  - return 0 if wait for AUTO_CAL_ACTIVE is success else -1.
 *  - Refer to EQOS column of <<RM_13, (sequence diagram)>> for API details.
 *  - TraceID:ETHERNET_NVETHERNETRM_013
 *
 * @param[in] osi_core: OSI core private data structure. Used param is base, osd_ops.usleep.
 *
 * @pre
 *  - MAC should out of reset and clocks enabled.
 *  - RGMII and MDIO interface needs to be IDLE before performing PAD
 *    calibration.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_pad_calibrate (
  struct osi_core_priv_data *const  osi_core
  )
{
  void      *ioaddr = osi_core->base;
  nveu32_t  retry   = RETRY_COUNT;
  nveu32_t  count;
  nve32_t   cond = COND_NOT_MET, ret = 0;
  nveu32_t  value;

  if (osi_core->mac_ver == OSI_EQOS_MAC_5_40) {
    return 0;
  }

  (void)__sync_val_compare_and_swap (
          &osi_core->padctrl.is_pad_cal_in_progress,
          OSI_DISABLE,
          OSI_ENABLE
          );
  ret = eqos_pre_pad_calibrate (osi_core);
  if (ret < 0) {
    ret = -1;
    goto error;
  }

  /* 1. Set field PAD_E_INPUT_OR_E_PWRD in
   * reg ETHER_QOS_SDMEMCOMPPADCTRL_0
   */
  value  = osi_readla (osi_core, (nveu8_t *)ioaddr + EQOS_PAD_CRTL);
  value |= EQOS_PAD_CRTL_E_INPUT_OR_E_PWRD;
  osi_writela (osi_core, value, (nveu8_t *)ioaddr + EQOS_PAD_CRTL);

  /* 2. delay for 1 usec */
  osi_core->osd_ops.udelay (OSI_DELAY_1US);

  /* 3. Set AUTO_CAL_ENABLE and AUTO_CAL_START in
   * reg ETHER_QOS_AUTO_CAL_CONFIG_0.
   * Set pad_auto_cal pd/pu offset values
   */
  value = osi_readla (
            osi_core,
            (nveu8_t *)ioaddr + EQOS_PAD_AUTO_CAL_CFG
            );
  value &= ~EQOS_PAD_CRTL_PU_OFFSET_MASK;
  value &= ~EQOS_PAD_CRTL_PD_OFFSET_MASK;
  value |= osi_core->padctrl.pad_auto_cal_pu_offset;
  value |= (osi_core->padctrl.pad_auto_cal_pd_offset << 8U);
  value |= EQOS_PAD_AUTO_CAL_CFG_START |
           EQOS_PAD_AUTO_CAL_CFG_ENABLE;
  osi_writela (osi_core, value, (nveu8_t *)ioaddr + EQOS_PAD_AUTO_CAL_CFG);

  /* 4. Wait on 10 to 12 us before start checking for calibration done.
   *    This delay is consumed in delay inside while loop.
   */

  /* 5. Wait on AUTO_CAL_ACTIVE until it is 0. 10ms is the timeout */
  count = 0;
  while (cond == COND_NOT_MET) {
    if (count > retry) {
      goto calibration_failed;
    }

    count++;
    osi_core->osd_ops.usleep (OSI_DELAY_10US);
    value = osi_readla (
              osi_core,
              (nveu8_t *)ioaddr +
              EQOS_PAD_AUTO_CAL_STAT
              );
    /* calibration done when CAL_STAT_ACTIVE is zero */
    if ((value & EQOS_PAD_AUTO_CAL_STAT_ACTIVE) == 0U) {
      cond = COND_MET;
    }
  }

calibration_failed:

  /* 6. Re-program the value PAD_E_INPUT_OR_E_PWRD in
   * ETHER_QOS_SDMEMCOMPPADCTRL_0 to save power
   */
  value  = osi_readla (osi_core, (nveu8_t *)ioaddr + EQOS_PAD_CRTL);
  value &=  ~EQOS_PAD_CRTL_E_INPUT_OR_E_PWRD;
  osi_writela (osi_core, value, (nveu8_t *)ioaddr + EQOS_PAD_CRTL);
  ret = (eqos_post_pad_calibrate (osi_core) < 0) ? -1 : ret;
error:
  (void)__sync_val_compare_and_swap (
          &osi_core->padctrl.is_pad_cal_in_progress,
          OSI_ENABLE,
          OSI_DISABLE
          );

  return ret;
}

/** \cond DO_NOT_DOCUMENT */

/**
 * @brief eqos_configure_mtl_queue - Configure MTL Queue
 *
 * @note
 * Algorithm:
 *  - This takes care of configuring the  below
 *    parameters for the MTL Queue
 *    - Mapping MTL Rx queue and DMA Rx channel
 *    - Flush TxQ
 *    - Enable Store and Forward mode for Tx, Rx
 *    - Configure Tx and Rx MTL Queue sizes
 *    - Configure TxQ weight
 *    - Enable Rx Queues
 *
 * @param[in] qinx:     Queue number that need to be configured.
 * @param[in] osi_core: OSI core private data.
 * @param[in] tx_fifo: MTL TX queue size for a MTL queue.
 * @param[in] rx_fifo: MTL RX queue size for a MTL queue.
 *
 * @pre MAC has to be out of reset.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_configure_mtl_queue (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          q_inx
  )
{
  const struct core_local  *l_core                                 = (struct core_local *)(void *)osi_core;
  const nveu32_t           rx_fifo_sz[2U][OSI_EQOS_MAX_NUM_QUEUES] = {
    { FIFO_SZ (9U),  FIFO_SZ (9U), FIFO_SZ (9U), FIFO_SZ (9U),
      FIFO_SZ (1U), FIFO_SZ (1U), FIFO_SZ (1U), FIFO_SZ (1U) },
    { FIFO_SZ (36U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U),
      FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (16U) },
  };
  const nveu32_t           tx_fifo_sz[2U][OSI_EQOS_MAX_NUM_QUEUES] = {
    { FIFO_SZ (9U), FIFO_SZ (9U), FIFO_SZ (9U), FIFO_SZ (9U),
      FIFO_SZ (1U), FIFO_SZ (1U), FIFO_SZ (1U), FIFO_SZ (1U) },
    { FIFO_SZ (8U), FIFO_SZ (8U), FIFO_SZ (8U), FIFO_SZ (8U),
      FIFO_SZ (8U), FIFO_SZ (8U), FIFO_SZ (8U), FIFO_SZ (8U) },
  };
  const nveu32_t           rfd_rfa[OSI_EQOS_MAX_NUM_QUEUES] = {
    FULL_MINUS_16_K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
  };
  nveu32_t                 l_macv       = (l_core->l_mac_ver & 0x1U);
  nveu32_t                 que_idx      = (q_inx & 0x7U);
  nveu32_t                 rx_fifo_sz_t = 0U;
  nveu32_t                 tx_fifo_sz_t = 0U;
  nveu32_t                 value        = 0;
  nve32_t                  ret          = 0;

  tx_fifo_sz_t = tx_fifo_sz[l_macv][que_idx];

  ret = hw_flush_mtl_tx_queue (osi_core, que_idx);
  if (ret < 0) {
    goto fail;
  }

  value = (tx_fifo_sz_t << EQOS_MTL_TXQ_SIZE_SHIFT);
  /* Enable Store and Forward mode */
  value |= EQOS_MTL_TSF;
  /* Enable TxQ */
  value |= EQOS_MTL_TXQEN;
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MTL_CHX_TX_OP_MODE (que_idx));

  /* read RX Q0 Operating Mode Register */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_CHX_RX_OP_MODE (que_idx)
            );

  rx_fifo_sz_t = rx_fifo_sz[l_macv][que_idx];
  value       |= (rx_fifo_sz_t << EQOS_MTL_RXQ_SIZE_SHIFT);
  /* Enable Store and Forward mode */
  value |= EQOS_MTL_RSF;

  /* Update EHFL, RFA and RFD
   * EHFL: Enable HW Flow Control
   * RFA: Threshold for Activating Flow Control
   * RFD: Threshold for Deactivating Flow Control
   */
  value &= ~EQOS_MTL_RXQ_OP_MODE_RFD_MASK;
  value &= ~EQOS_MTL_RXQ_OP_MODE_RFA_MASK;
  value |= EQOS_MTL_RXQ_OP_MODE_EHFC;
  value |= (rfd_rfa[que_idx] << EQOS_MTL_RXQ_OP_MODE_RFD_SHIFT) &
           EQOS_MTL_RXQ_OP_MODE_RFD_MASK;
  value |= (rfd_rfa[que_idx] << EQOS_MTL_RXQ_OP_MODE_RFA_SHIFT) &
           EQOS_MTL_RXQ_OP_MODE_RFA_MASK;
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MTL_CHX_RX_OP_MODE (que_idx));

  /* Transmit Queue weight */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_TXQ_QW (que_idx)
            );
  value |= EQOS_MTL_TXQ_QW_ISCQW;
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MTL_TXQ_QW (que_idx));

  /* Enable Rx Queue Control */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MAC_RQC0R
            );
  value |= ((osi_core->rxq_ctrl[que_idx] & EQOS_RXQ_EN_MASK) << (que_idx * 2U));
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_RQC0R);

fail:
  return ret;
}

/** \endcond */

/**
 * @brief eqos_config_frp - Enable/Disale RX Flexible Receive Parser in HW
 *
 * Algorithm:
 *      1) Read the MTL OP Mode configuration register.
 *      2) Enable/Disable FRPE bit based on the input.
 *      3) Write the MTL OP Mode configuration register.
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] enabled: Flag to indicate feature is to be enabled/disabled.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_frp (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enabled
  )
{
  nveu8_t   *base = osi_core->base;
  nveu32_t  op_mode = 0U, val = 0U;
  nve32_t   ret = 0;

  /* Disable RE */
  val  = osi_readl (base + EQOS_MAC_MCR);
  val &= ~EQOS_MCR_RE;
  osi_writel (val, base + EQOS_MAC_MCR);

  op_mode = osi_readl (base + EQOS_MTL_OP_MODE);
  if (enabled == OSI_ENABLE) {
    /* Set FRPE bit of MTL_Operation_Mode register */
    op_mode |= EQOS_MTL_OP_MODE_FRPE;
  } else {
    /* Reset FRPE bit of MTL_Operation_Mode register */
    op_mode &= ~EQOS_MTL_OP_MODE_FRPE;
  }

  osi_writel (op_mode, base + EQOS_MTL_OP_MODE);

  /* Verify RXPI bit set in MTL_RXP_Control_Status */
  ret = osi_readl_poll_timeout (
          (base + EQOS_MTL_RXP_CS),
          osi_core,
          EQOS_MTL_RXP_CS_RXPI,
          EQOS_MTL_RXP_CS_RXPI,
          (EQOS_MTL_FRP_READ_UDELAY),
          (EQOS_MTL_FRP_READ_RETRY)
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to enable FRP\n",
      val
      );
    goto frp_enable_re;
  }

  val = osi_readl (base + EQOS_MTL_RXP_INTR_CS);
  if (enabled == OSI_ENABLE) {
    /* Enable FRP Interrupt MTL_RXP_Interrupt_Control_Status */
    val |= (EQOS_MTL_RXP_INTR_CS_NVEOVIE |
            EQOS_MTL_RXP_INTR_CS_NPEOVIE |
            EQOS_MTL_RXP_INTR_CS_FOOVIE |
            EQOS_MTL_RXP_INTR_CS_PDRFIE);
  } else {
    /* Disable FRP Interrupt MTL_RXP_Interrupt_Control_Status */
    val &= ~(EQOS_MTL_RXP_INTR_CS_NVEOVIE |
             EQOS_MTL_RXP_INTR_CS_NPEOVIE |
             EQOS_MTL_RXP_INTR_CS_FOOVIE |
             EQOS_MTL_RXP_INTR_CS_PDRFIE);
  }

  osi_writel (val, base + EQOS_MTL_RXP_INTR_CS);

frp_enable_re:
  /* Enable RE */
  val  = osi_readla (osi_core, base + EQOS_MAC_MCR);
  val |= EQOS_MCR_RE;
  osi_writela (osi_core, val, base + EQOS_MAC_MCR);

  return ret;
}

/**
 * @brief eqos_update_frp_nve - Update FRP NVE into HW
 *
 * Algorithm:
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] nve: Number of Valid Entries.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static void
eqos_update_frp_nve (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    nve
  )
{
  nveu32_t  val;
  nveu8_t   *base = osi_core->base;

  /* Update NVE and NPE in MTL_RXP_Control_Status register */
  val = osi_readla (osi_core, base + EQOS_MTL_RXP_CS);
  /* Clear old NVE and NPE */
  val &= ~(EQOS_MTL_RXP_CS_NVE | EQOS_MTL_RXP_CS_NPE);
  /* Add new NVE and NVE */
  val |= (nve & EQOS_MTL_RXP_CS_NVE);
  val |= ((nve << EQOS_MTL_RXP_CS_NPE_SHIFT) & EQOS_MTL_RXP_CS_NPE);
  osi_writela (osi_core, val, base + EQOS_MTL_RXP_CS);

  return;
}

/**
 * @brief eqos_frp_write - Write FRP entry into HW
 *
 * Algorithm: This function will write FRP entry registers into HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] addr: FRP register address.
 * @param[in] data: FRP register data.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_frp_write (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   addr,
  nveu32_t                   data
  )
{
  nve32_t   ret   = 0;
  nveu8_t   *base = osi_core->base;
  nveu32_t  val   = 0U;

  /* Wait for ready */
  ret = osi_readl_poll_timeout (
          (base + EQOS_MTL_RXP_IND_CS),
          osi_core,
          EQOS_MTL_RXP_IND_CS_BUSY,
          OSI_NONE,
          (EQOS_MTL_FRP_READ_UDELAY),
          (EQOS_MTL_FRP_READ_RETRY)
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to write\n",
      val
      );
    ret = -1;
    goto done;
  }

  /* Write data into MTL_RXP_Indirect_Acc_Data */
  osi_writel (data, base + EQOS_MTL_RXP_IND_DATA);

  /* Program MTL_RXP_Indirect_Acc_Control_Status */
  val = osi_readl (base + EQOS_MTL_RXP_IND_CS);
  /* Set WRRDN for write */
  val |= EQOS_MTL_RXP_IND_CS_WRRDN;
  /* Clear and add ADDR */
  val &= ~EQOS_MTL_RXP_IND_CS_ADDR;
  val |= (addr & EQOS_MTL_RXP_IND_CS_ADDR);
  /* Start write */
  val |= EQOS_MTL_RXP_IND_CS_BUSY;
  osi_writel (val, base + EQOS_MTL_RXP_IND_CS);

  /* Wait for complete */
  ret = osi_readl_poll_timeout (
          (base + EQOS_MTL_RXP_IND_CS),
          osi_core,
          EQOS_MTL_RXP_IND_CS_BUSY,
          OSI_NONE,
          (EQOS_MTL_FRP_READ_UDELAY),
          (EQOS_MTL_FRP_READ_RETRY)
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to write\n",
      val
      );
    ret = -1;
  }

done:
  return ret;
}

/**
 * @brief eqos_update_frp_entry - Update FRP Instruction Table entry in HW
 *
 * Algorithm:
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] pos_val: FRP Instruction Table entry location.
 * @param[in] data: FRP entry data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_update_frp_entry (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    pos_val,
  struct osi_core_frp_data *const   data
  )
{
  nveu32_t  val = 0U, tmp = 0U;
  nve32_t   ret = -1;
  nveu32_t  pos = (pos_val & 0xFFU);

  /** Write Match Data into IE0 **/
  val = data->match_data;
  ret = eqos_frp_write (osi_core, EQOS_MTL_FRP_IE0 (pos), val);
  if (ret < 0) {
    /* Match Data Write fail */
    ret = -1;
    goto done;
  }

  /** Write Match Enable into IE1 **/
  val = data->match_en;
  ret = eqos_frp_write (osi_core, EQOS_MTL_FRP_IE1 (pos), val);
  if (ret < 0) {
    /* Match Enable Write fail */
    ret = -1;
    goto done;
  }

  /** Write AF, RF, IM, NIC, FO and OKI into IE2 **/
  val = 0;
  if (data->accept_frame == OSI_ENABLE) {
    /* Set AF Bit */
    val |= EQOS_MTL_FRP_IE2_AF;
  }

  if (data->reject_frame == OSI_ENABLE) {
    /* Set RF Bit */
    val |= EQOS_MTL_FRP_IE2_RF;
  }

  if (data->inverse_match == OSI_ENABLE) {
    /* Set IM Bit */
    val |= EQOS_MTL_FRP_IE2_IM;
  }

  if (data->next_ins_ctrl == OSI_ENABLE) {
    /* Set NIC Bit */
    val |= EQOS_MTL_FRP_IE2_NC;
  }

  tmp  = data->frame_offset;
  val |= ((tmp << EQOS_MTL_FRP_IE2_FO_SHIFT) & EQOS_MTL_FRP_IE2_FO);
  tmp  = data->ok_index;
  val |= ((tmp << EQOS_MTL_FRP_IE2_OKI_SHIFT) & EQOS_MTL_FRP_IE2_OKI);
  tmp  = data->dma_chsel;
  val |= ((tmp << EQOS_MTL_FRP_IE2_DCH_SHIFT) & EQOS_MTL_FRP_IE2_DCH);
  ret  = eqos_frp_write (osi_core, EQOS_MTL_FRP_IE2 (pos), val);
  if (ret < 0) {
    /* FRP IE2 Write fail */
    ret = -1;
    goto done;
  }

  /** Write DCH into IE3 **/
  val = OSI_NONE;
  ret = eqos_frp_write (osi_core, EQOS_MTL_FRP_IE3 (pos), val);
  if (ret < 0) {
    /* DCH Write fail */
    ret = -1;
  }

done:
  return ret;
}

#ifndef OSI_STRIPPED_LIB
/** \cond DO_NOT_DOCUMENT */

/**
 * @brief eqos_configure_rxq_priority - Configure Priorities Selected in
 *    the Receive Queue
 *
 * @note
 * Algorithm:
 *  - This takes care of mapping user priority to Rx queue.
 *    User provided priority mask updated to register. Valid input can have
 *    all TC(0xFF) in one queue to None(0x00) in rx queue.
 *    The software must ensure that the content of this field is mutually
 *    exclusive to the PSRQ fields for other queues, that is, the same
 *    priority is not mapped to multiple Rx queues.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MAC has to be out of reset.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void
eqos_configure_rxq_priority (
  struct osi_core_priv_data *const  osi_core
  )
{
  nveu32_t  val;
  nveu32_t  temp;
  nveu32_t  qinx, mtlq;
  nveu32_t  pmask = 0x0U;
  nveu32_t  mfix_var1, mfix_var2;

  /* make sure EQOS_MAC_RQC2R is reset before programming */
  osi_writela (
    osi_core,
    OSI_DISABLE,
    (nveu8_t *)osi_core->base +
    EQOS_MAC_RQC2R
    );

  for (qinx = 0; qinx < osi_core->num_mtl_queues; qinx++) {
    mtlq = osi_core->mtl_queues[qinx];
    /* check for PSRQ field mutual exclusive for all queues */
    if ((osi_core->rxq_prio[mtlq] <= 0xFFU) &&
        (osi_core->rxq_prio[mtlq] > 0x0U) &&
        ((pmask & osi_core->rxq_prio[mtlq]) == 0U))
    {
      pmask |= osi_core->rxq_prio[mtlq];
      temp   = osi_core->rxq_prio[mtlq];
    } else {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "Invalid rxq Priority for Q\n",
        (nveul64_t)mtlq
        );
      continue;
    }

    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MAC_RQC2R
            );
    mfix_var1   = mtlq * (nveu32_t)EQOS_MAC_RQC2_PSRQ_SHIFT;
    mfix_var2   = (nveu32_t)EQOS_MAC_RQC2_PSRQ_MASK;
    mfix_var2 <<= mfix_var1;
    val        &= ~mfix_var2;
    temp        = temp << (mtlq * EQOS_MAC_RQC2_PSRQ_SHIFT);
    mfix_var1   = mtlq * (nveu32_t)EQOS_MAC_RQC2_PSRQ_SHIFT;
    mfix_var2   = (nveu32_t)EQOS_MAC_RQC2_PSRQ_MASK;
    mfix_var2 <<= mfix_var1;
    val        |= (temp & mfix_var2);
    /* Priorities Selected in the Receive Queue 0 */
    osi_writela (osi_core, val, (nveu8_t *)osi_core->base + EQOS_MAC_RQC2R);
  }
}

#endif /* !OSI_STRIPPED_LIB */

#ifdef HSI_SUPPORT

/**
 * @brief eqos_hsi_configure - Configure HSI features
 *
 * @note
 * Algorithm:
 *  - Enables LIC interrupt and configure HSI features
 *
 * @param[in, out] osi_core: OSI core private data structure.
 * @param[in] enable: OSI_ENABLE for enabling HSI feature, else disable
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t
eqos_hsi_configure (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable
  )
{
  nveu32_t  value;
  nve32_t   ret = 0;

  if (enable == OSI_ENABLE) {
    /* T23X-EQOS_HSIv2-19: Enabling of Consistency Monitor for TX Frame Errors */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + EQOS_MAC_IMR
              );
    value |= EQOS_IMR_TXESIE;
    osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_IMR);

    /*  T23X-EQOS_HSIv2-1: Enabling of Memory ECC */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + EQOS_MTL_ECC_CONTROL
              );
    value |= EQOS_MTL_ECC_MTXEE;
    value |= EQOS_MTL_ECC_MRXEE;
    value |= EQOS_MTL_ECC_MESTEE;
    value |= EQOS_MTL_ECC_MRXPEE;
    value |= EQOS_MTL_ECC_TSOEE;
    value |= EQOS_MTL_ECC_DSCEE;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + EQOS_MTL_ECC_CONTROL
      );

    /* T23X-EQOS_HSIv2-2: Enabling of Bus Parity */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + EQOS_MTL_DPP_CONTROL
              );
    value |= EQOS_EDPP | EQOS_OPE;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + EQOS_MTL_DPP_CONTROL
      );

    /* For T26x CE/UCE are not handled by SW driver,since they are directly
     * reported to FSI through HSM , so not enabling it
     */
    if (osi_core->mac_ver < OSI_EQOS_MAC_5_40) {
      /* Enable Interrupts */
      /*  T23X-EQOS_HSIv2-1: Enabling of Memory ECC */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + EQOS_MTL_ECC_INTERRUPT_ENABLE
                );
      value |= EQOS_MTL_TXCEIE;
      value |= EQOS_MTL_RXCEIE;
      value |= EQOS_MTL_ECEIE;
      value |= EQOS_MTL_RPCEIE;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + EQOS_MTL_ECC_INTERRUPT_ENABLE
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + EQOS_DMA_ECC_INTERRUPT_ENABLE
                );
      value |= EQOS_DMA_TCEIE;
      value |= EQOS_DMA_DCEIE;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + EQOS_DMA_ECC_INTERRUPT_ENABLE
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                EQOS_WRAP_COMMON_INTR_ENABLE
                );
      value |= EQOS_REGISTER_PARITY_ERR;
      value |= EQOS_CORE_CORRECTABLE_ERR;
      value |= EQOS_CORE_UNCORRECTABLE_ERR;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        EQOS_WRAP_COMMON_INTR_ENABLE
        );
    }

    /* Below 2 are applicable only for THOR EQOS */
    /* T264-EQOS_HSIv2-34: Initialization of Transaction Timeout in PCS */
    /* T264-EQOS_HSIv2-35: Initialization of Watchdog Timer for PCS FSM States */
    if (osi_core->mac_ver == OSI_EQOS_MAC_5_40) {
      value  = (0xCBU << XPCS_SFTY_1US_MULT_SHIFT) & XPCS_SFTY_1US_MULT_MASK;
      value |= ((nveu32_t)0x01U << XPCS_FSM_TO_SEL_SHIFT) & XPCS_FSM_TO_SEL_MASK;
      value |= EQOS_PCS_SFTY_TMR_CTRL_RXFPEI;
      ret    = xpcs_write_safety (osi_core, EQOS_PCS_SFTY_TMR_CTRL, value);
      if (ret != 0) {
        goto fail;
      }

      /* Enable FSM time-out safety mechanism inside PCS */
      ret = xpcs_write_safety (
              osi_core,
              EQOS_PCS_SFTY_DISABLE_0,
              PCS_FSM_TIMEOUT_ENABLE
              );
      if (ret != 0) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "eqos fsm timeout enable failed\n",
          0ULL
          );
        goto fail;
      }
    }

    /* T23X-EQOS_HSIv2-5: Enabling and Initialization of Transaction Timeout */
    value  = (0x198U << EQOS_TMR_SHIFT) & EQOS_TMR_MASK;
    value |= ((nveu32_t)0x2U << EQOS_LTMRMD_SHIFT) & EQOS_LTMRMD_MASK;
    value |= ((nveu32_t)0x2U << EQOS_NTMRMD_SHIFT) & EQOS_NTMRMD_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + EQOS_MAC_FSM_ACT_TIMER
      );

    /* T23X-EQOS_HSIv2-3: Enabling and Initialization of Watchdog */
    /* T23X-EQOS_HSIv2-4: Enabling of Consistency Monitor for FSM States */
    value = EQOS_PRTYEN | EQOS_TMOUTEN;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + EQOS_MAC_FSM_CONTROL
      );
  } else {
    /* T23X-EQOS_HSIv2-19: Disable of Consistency Monitor for TX Frame Errors */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + EQOS_MAC_IMR
              );
    value &= ~EQOS_IMR_TXESIE;
    osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_IMR);

    /*  T23X-EQOS_HSIv2-1: Disable of Memory ECC */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + EQOS_MTL_ECC_CONTROL
              );
    value &= ~EQOS_MTL_ECC_MTXEE;
    value &= ~EQOS_MTL_ECC_MRXEE;
    value &= ~EQOS_MTL_ECC_MESTEE;
    value &= ~EQOS_MTL_ECC_MRXPEE;
    value &= ~EQOS_MTL_ECC_TSOEE;
    value &= ~EQOS_MTL_ECC_DSCEE;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + EQOS_MTL_ECC_CONTROL
      );

    /* T23X-EQOS_HSIv2-2: Disable of Bus Parity */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + EQOS_MTL_DPP_CONTROL
              );
    value &= ~EQOS_EDPP;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + EQOS_MTL_DPP_CONTROL
      );

    if (osi_core->mac_ver < OSI_EQOS_MAC_5_40) {
      /* Disable Interrupts */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + EQOS_MTL_ECC_INTERRUPT_ENABLE
                );
      value &= ~EQOS_MTL_TXCEIE;
      value &= ~EQOS_MTL_RXCEIE;
      value &= ~EQOS_MTL_ECEIE;
      value &= ~EQOS_MTL_RPCEIE;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + EQOS_MTL_ECC_INTERRUPT_ENABLE
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + EQOS_DMA_ECC_INTERRUPT_ENABLE
                );
      value &= ~EQOS_DMA_TCEIE;
      value &= ~EQOS_DMA_DCEIE;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + EQOS_DMA_ECC_INTERRUPT_ENABLE
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                EQOS_WRAP_COMMON_INTR_ENABLE
                );
      value &= ~EQOS_REGISTER_PARITY_ERR;
      value &= ~EQOS_CORE_CORRECTABLE_ERR;
      value &= ~EQOS_CORE_UNCORRECTABLE_ERR;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        EQOS_WRAP_COMMON_INTR_ENABLE
        );
    }

    /* Deinitialization of Watchdog Timer */
    if (osi_core->mac_ver == OSI_EQOS_MAC_5_40) {
      value &= ~(XPCS_SFTY_1US_MULT_MASK | XPCS_FSM_TO_SEL_MASK |
                 EQOS_PCS_SFTY_TMR_CTRL_RXFPEI);
      ret = xpcs_write_safety (osi_core, EQOS_PCS_SFTY_TMR_CTRL, value);
      if (ret != 0) {
        goto fail;
      }

      /* disable FSM time-out safety mechanism inside PCS */
      ret = xpcs_write_safety (
              osi_core,
              EQOS_PCS_SFTY_DISABLE_0,
              PCS_FSM_TIMEOUT_DISABLE
              );
      if (ret != 0) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "eqos fsm timeout disable failed\n",
          0ULL
          );
        goto fail;
      }
    }

    /* T23X-EQOS_HSIv2-5: Denitialization of Transaction Timeout */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + EQOS_MAC_FSM_ACT_TIMER
              );
    value &= ~(EQOS_TMR_MASK | EQOS_LTMRMD_MASK | EQOS_NTMRMD_MASK);
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + EQOS_MAC_FSM_ACT_TIMER
      );

    /* T23X-EQOS_HSIv2-4: Disable of Consistency Monitor for FSM States */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + EQOS_MAC_FSM_CONTROL
              );
    value &= ~EQOS_PRTYEN;
    value &= ~EQOS_TMOUTEN;
    osi_writela (
      osi_core,
      0,
      (nveu8_t *)osi_core->base + EQOS_MAC_FSM_CONTROL
      );
  }

fail:
  return ret;
}

  #ifdef NV_VLTEST_BUILD

/**
 * @brief eqos_hsi_inject_err - inject error
 *
 * @note
 * Algorithm:
 *  - Use error injection method induce error
 *
 * @param[in, out] osi_core: OSI core private data structure.
 * @param[in] error_code: UE_IDX/CE_IDX uncorrectable/correctable error
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t
eqos_hsi_inject_err (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    error_code
  )
{
  nveu32_t  value;
  nve32_t   ret = 0;

  switch (error_code) {
    case OSI_CORRECTABLE_ERR:
      value = (EQOS_MTL_DBG_CTL_EIEC | EQOS_MTL_DBG_CTL_EIEE);
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        EQOS_MTL_DBG_CTL
        );
      break;
    case OSI_UNCORRECTABLE_ERR:
      value = EQOS_MTL_DPP_ECC_EIC_BLEI;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        EQOS_MTL_DPP_ECC_EIC
        );

      value = (EQOS_MTL_DBG_CTL_EIEC | EQOS_MTL_DBG_CTL_EIEE);
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        EQOS_MTL_DBG_CTL
        );
      break;
    default:
      ret = hsi_common_error_inject (osi_core, error_code);
      break;
  }

  return ret;
}

  #endif
#endif

/**
 * @brief eqos_configure_mac - Configure MAC
 *
 * @note
 * Algorithm:
 *  - This takes care of configuring the  below
 *  parameters for the MAC
 *    - Enable required MAC control fields in MCR
 *    - Enable JE/JD/WD/GPSLCE based on the MTU size
 *    - Enable Multicast and Broadcast Queue
 *    - Disable MMC interrupts and Configure the MMC counters
 *    - Enable required MAC interrupts
 *
 * @param[in, out] osi_core: OSI core private data structure.
 *
 * @pre MAC has to be out of reset.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void
eqos_configure_mac (
  struct osi_core_priv_data *const  osi_core
  )
{
  nveu32_t  value;
  nveu32_t  mac_ext;

  /* Read MAC Configuration Register */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + EQOS_MAC_MCR
            );
  /* Enable Automatic Pad or CRC Stripping */
  /* Enable CRC stripping for Type packets */
  /* Enable Full Duplex mode */
  /* Enable Rx checksum offload engine by default */
  value |= EQOS_MCR_ACS | EQOS_MCR_CST | EQOS_MCR_DM | EQOS_MCR_IPC;

  if ((osi_core->mtu > OSI_DFLT_MTU_SIZE) &&
      (osi_core->mtu <= OSI_MTU_SIZE_9000))
  {
    /* if MTU less than or equal to 9K use JE */
    value |= EQOS_MCR_JE;
    value |= EQOS_MCR_JD;
  } else if (osi_core->mtu > OSI_MTU_SIZE_9000) {
    /* if MTU greater 9K use GPSLCE */
    value |= EQOS_MCR_JD | EQOS_MCR_WD;
    value |= EQOS_MCR_GPSLCE;
    /* Read MAC Extension Register */
    mac_ext = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                EQOS_MAC_EXTR
                );
    /* Configure GPSL */
    mac_ext &= ~EQOS_MAC_EXTR_GPSL_MSK;
    mac_ext |= OSI_MAX_MTU_SIZE & EQOS_MAC_EXTR_GPSL_MSK;
    /* Write MAC Extension Register */
    osi_writela (
      osi_core,
      mac_ext,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_EXTR
      );
  } else {
    /* do nothing for default mtu size */
  }

  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_MCR);

  if (osi_core->mac_ver >= OSI_EQOS_MAC_5_30) {
    /* Enable common interrupt at wrapper level */
    value  = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_WRAP_COMMON_INTR_ENABLE);
    value |= EQOS_MAC_SBD_INTR;
    osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_WRAP_COMMON_INTR_ENABLE);
  }

  /* enable Packet Duplication Control */
  value = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_EXTR);
  if (osi_core->mac_ver >= OSI_EQOS_MAC_5_00) {
    value |= EQOS_MAC_EXTR_PDC;
  }

  /* Write to MAC Extension Register */
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_EXTR);

  /* Enable Multicast and Broadcast Queue, default is Q0 */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + EQOS_MAC_RQC1R
            );
  value |= EQOS_MAC_RQC1R_MCBCQEN;

  /* Routing Multicast and Broadcast depending on mac version */
  value &= ~(EQOS_MAC_RQC1R_MCBCQ);
  if (osi_core->mac_ver > OSI_EQOS_MAC_5_00) {
    value |= ((nveu32_t)EQOS_MAC_RQC1R_MCBCQ7) << EQOS_MAC_RQC1R_MCBCQ_SHIFT;
  } else {
    value |= ((nveu32_t)EQOS_MAC_RQC1R_MCBCQ3) << EQOS_MAC_RQC1R_MCBCQ_SHIFT;
  }

  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_RQC1R);

  /* Disable all MMC interrupts */
  /* Disable all MMC Tx Interrupts */
  osi_writela (
    osi_core,
    EQOS_MMC_INTR_DISABLE,
    (nveu8_t *)osi_core->base +
    EQOS_MMC_TX_INTR_MASK
    );
  /* Disable all MMC RX interrupts */
  osi_writela (
    osi_core,
    EQOS_MMC_INTR_DISABLE,
    (nveu8_t *)osi_core->base +
    EQOS_MMC_RX_INTR_MASK
    );
  /* Disable MMC Rx interrupts for IPC */
  osi_writela (
    osi_core,
    EQOS_MMC_INTR_DISABLE,
    (nveu8_t *)osi_core->base +
    EQOS_MMC_IPC_RX_INTR_MASK
    );

  /* Configure MMC counters */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + EQOS_MMC_CNTRL
            );
  value |= EQOS_MMC_CNTRL_CNTRST | EQOS_MMC_CNTRL_RSTONRD |
           EQOS_MMC_CNTRL_CNTPRST | EQOS_MMC_CNTRL_CNTPRSTLVL;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + EQOS_MMC_CNTRL
    );

  /* Enable MAC interrupts */
  /* Read MAC IMR Register */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + EQOS_MAC_IMR
            );

  /* RGSMIIIE - RGMII/SMII interrupt Enable.
   * LPIIE is not enabled. MMC LPI counters is maintained in HW */
  value |= EQOS_IMR_RGSMIIIE;
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_IMR);

  /* Enable VLAN configuration */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + EQOS_MAC_VLAN_TAG
            );

  /* Enable VLAN Tag stripping always
   * Enable operation on the outer VLAN Tag, if present
   * Disable double VLAN Tag processing on TX and RX
   * Enable VLAN Tag in RX Status
   * Disable VLAN Type Check
   */
 #ifndef OSI_STRIPPED_LIB
  if (osi_core->strip_vlan_tag == OSI_ENABLE) {
    value |= EQOS_MAC_VLANTR_EVLS_ALWAYS_STRIP;
  }

 #endif /* !OSI_STRIPPED_LIB */
  value |= EQOS_MAC_VLANTR_EVLRXS | EQOS_MAC_VLANTR_DOVLTC;
  value &= ~EQOS_MAC_VLANTR_ERIVLT;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + EQOS_MAC_VLAN_TAG
    );

  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + EQOS_MAC_VLANTIR
            );
  /* Enable VLAN tagging through context descriptor */
  value |= EQOS_MAC_VLANTIR_VLTI;
  /* insert/replace C_VLAN in 13th & 14th bytes of transmitted frames */
  value &= ~EQOS_MAC_VLANTIRR_CSVL;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + EQOS_MAC_VLANTIR
    );

  /* Moved to stripped since in safety dcs is always enabled */
 #ifndef OSI_STRIPPED_LIB
  /* USP (user Priority) to RxQ Mapping, only if DCS not enabled */
  if (osi_core->dcs_en != OSI_ENABLE) {
    eqos_configure_rxq_priority (osi_core);
  }

 #endif /* !OSI_STRIPPED_LIB */
}

/**
 * @brief eqos_configure_dma - Configure DMA
 *
 * @note
 * Algorithm:
 *  - This takes care of configuring the  below
 *  parameters for the DMA
 *    - Programming different burst length for the DMA
 *    - Enable enhanced Address mode
 *    - Programming max read outstanding request limit
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MAC has to be out of reset.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void
eqos_configure_dma (
  struct osi_core_priv_data *const  osi_core
  )
{
  nveu32_t  value = 0;
  void      *base = osi_core->base;

  /* AXI Burst Length 8*/
  value |= EQOS_DMA_SBUS_BLEN8;
  /* AXI Burst Length 16*/
  value |= EQOS_DMA_SBUS_BLEN16;
  /* Enhanced Address Mode Enable */
  value |= EQOS_DMA_SBUS_EAME;
  /* AXI Maximum Read Outstanding Request Limit = 31 */
  value |= EQOS_DMA_SBUS_RD_OSR_LMT;
  /* AXI Maximum Write Outstanding Request Limit = 31 */
  value |= EQOS_DMA_SBUS_WR_OSR_LMT;

  osi_writela (osi_core, value, (nveu8_t *)base + EQOS_DMA_SBUS);
}

/** \endcond */

/**
 * @brief Map DMA channels to a specific VM IRQ.
 *
 * @param[in] osi_core: OSI private data structure.
 *
 * @note
 *      Dependencies: OSD layer needs to update number of VM channels and
 *                    DMA channel list in osi_vm_irq_data.
 *      Protection: None.
 *
 * @retval None.
 */
static void
eqos_dma_chan_to_vmirq_map (
  struct osi_core_priv_data  *osi_core
  )
{
  struct osi_vm_irq_data  *irq_data;
  nveu32_t                i, j;
  nveu32_t                chan;

  for (i = 0; i < osi_core->num_vm_irqs; i++) {
    irq_data = &osi_core->irq_data[i];
    for (j = 0; j < irq_data->num_vm_chans; j++) {
      chan = irq_data->vm_chans[j];
      if (chan >= OSI_EQOS_MAX_NUM_CHANS) {
        continue;
      }

      osi_writel (
        OSI_BIT (irq_data->vm_num),
        (nveu8_t *)osi_core->base +
        EQOS_VIRT_INTR_APB_CHX_CNTRL (chan)
        );
    }

    osi_writel (
      OSI_BIT (irq_data->vm_num),
      (nveu8_t *)osi_core->base + VIRTUAL_APB_ERR_CTRL
      );
  }
}

#ifndef OSI_STRIPPED_LIB
static void
eqos_configure_asid (
  struct osi_core_priv_data *const  osi_core
  )
{
  if (osi_core->use_virtualization == OSI_DISABLE) {
    if (osi_core->hv_base != OSI_NULL) {
      osi_writela (
        osi_core,
        EQOS_5_30_ASID_CTRL_VAL,
        (nveu8_t *)osi_core->hv_base +
        EQOS_AXI_ASID_CTRL
        );

      osi_writela (
        osi_core,
        EQOS_5_30_ASID1_CTRL_VAL,
        (nveu8_t *)osi_core->hv_base +
        EQOS_AXI_ASID1_CTRL
        );
    }

    if (osi_core->mac_ver < OSI_EQOS_MAC_5_30) {
      /* AXI ASID CTRL for channel 0 to 3 */
      osi_writela (
        osi_core,
        EQOS_AXI_ASID_CTRL_VAL,
        (nveu8_t *)osi_core->base +
        EQOS_AXI_ASID_CTRL
        );

      /* AXI ASID1 CTRL for channel 4 to 7 */
      if (osi_core->mac_ver > OSI_EQOS_MAC_5_00) {
        osi_writela (
          osi_core,
          EQOS_AXI_ASID1_CTRL_VAL,
          (nveu8_t *)osi_core->base +
          EQOS_AXI_ASID1_CTRL
          );
      }
    }
  }
}

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief eqos_core_init - EQOS MAC, MTL and common DMA Initialization
 *
 * @note
 * Algorithm:
 *  - This function will take care of initializing MAC, MTL and
 *    common DMA registers.
 *  - Refer to OSI column of <<RM_06, (sequence diagram)>> for sequence
 * of execution.
 *  - TraceID:ETHERNET_NVETHERNETRM_006
 *
 * @param[in] osi_core: OSI core private data structure. Used params are
 *  - base, dcs_en, num_mtl_queues, mtl_queues, mtu, stip_vlan_tag, pause_frames.
 *
 * @pre
 *  - MAC should be out of reset. See osi_poll_for_mac_reset_complete()
 *          for details.
 *  - osi_core->base needs to be filled based on ioremap.
 *  - osi_core->num_mtl_queues needs to be filled.
 *  - osi_core->mtl_queues[qinx] need to be filled.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_core_init (
  struct osi_core_priv_data *const  osi_core
  )
{
  nve32_t                   ret          = 0;
  nveu32_t                  qinx         = 0;
  nveu32_t                  value        = 0;
  nveu32_t                  value1       = 0;
  nveu16_t                  frp_cnt      = 0;
  struct osi_core_frp_data  bypass_entry = { 0 };

  /* Add Bypass entry in all FRP entries to initlize the FRP table */
  bypass_entry.match_en     = 0x0U;
  bypass_entry.accept_frame = 1;
  bypass_entry.reject_frame = 1;
  for (frp_cnt = 0; frp_cnt < OSI_FRP_MAX_ENTRY; frp_cnt++) {
    ret = eqos_update_frp_entry (osi_core, frp_cnt, &bypass_entry);
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Fail to update BYPASS entry for max FRP entries\n",
        OSI_NONE
        );
      goto fail;
    }
  }

  if (osi_core->mac_ver != OSI_EQOS_MAC_5_40) {
    /* PAD calibration */
    ret = eqos_pad_calibrate (osi_core);
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "eqos pad calibration failed\n",
        0ULL
        );
      goto fail;
    }
  }

  /* reset mmc counters */
  osi_writela (
    osi_core,
    EQOS_MMC_CNTRL_CNTRST,
    (nveu8_t *)osi_core->base + EQOS_MMC_CNTRL
    );

 #ifndef OSI_STRIPPED_LIB
  if (osi_core->mac_ver == OSI_EQOS_MAC_5_30) {
    /* Configure ASID for T234 EQOS
     * Keep the on reset value for T264 EQOS
     * which is zero since EQOS SID is 0x160
     * and driver needs to program lower 8 bit values
     * which will be zero for T264
     */
    eqos_configure_asid (osi_core);
  }

  /* Mapping MTL Rx queue and DMA Rx channel */
  if (osi_core->dcs_en == OSI_ENABLE) {
    value  = EQOS_RXQ_TO_DMA_CHAN_MAP_DCS_EN;
    value1 = EQOS_RXQ_TO_DMA_CHAN_MAP1_DCS_EN;
  } else {
    value  = EQOS_RXQ_TO_DMA_CHAN_MAP;
    value1 = EQOS_RXQ_TO_DMA_CHAN_MAP1;
  }

 #else
  /* DCS is enabled in safety by default */
  value  = EQOS_RXQ_TO_DMA_CHAN_MAP_DCS_EN;
  value1 = EQOS_RXQ_TO_DMA_CHAN_MAP1_DCS_EN;
 #endif /* !OSI_STRIPPED_LIB */

  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MTL_RXQ_DMA_MAP0);
  if (osi_core->mac_ver >= OSI_EQOS_MAC_5_30) {
    osi_writela (osi_core, value1, (nveu8_t *)osi_core->base + EQOS_MTL_RXQ_DMA_MAP1);
  }

  if (osi_unlikely (osi_core->num_mtl_queues > OSI_EQOS_MAX_NUM_QUEUES)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Number of queues is incorrect\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  /* Configure MTL Queues */
  for (qinx = 0; qinx < osi_core->num_mtl_queues; qinx++) {
    if (osi_unlikely (
          osi_core->mtl_queues[qinx] >=
          OSI_EQOS_MAX_NUM_QUEUES
          ))
    {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "Incorrect queues number\n",
        0ULL
        );
      ret = -1;
      goto fail;
    }

    ret = eqos_configure_mtl_queue (osi_core, osi_core->mtl_queues[qinx]);
    if (ret < 0) {
      goto fail;
    }

    /* Enable by default to configure forward error packets.
     * Since this is a local function this will always return sucess,
     * so no need to check for return value
     */
    (void)hw_config_fw_err_pkts (osi_core, osi_core->mtl_queues[qinx], OSI_ENABLE);
  }

  /* configure EQOS MAC HW */
  eqos_configure_mac (osi_core);

  /* configure EQOS DMA */
  eqos_configure_dma (osi_core);

  /* tsn initialization */
  hw_tsn_init (osi_core);

 #if !defined (L3L4_WILDCARD_FILTER)
  /* initialize L3L4 Filters variable */
  osi_core->l3l4_filter_bitmask = OSI_NONE;
 #endif /* !L3L4_WILDCARD_FILTER */

  if (osi_core->mac_ver >= OSI_EQOS_MAC_5_30) {
    /* Enabling by default from T23x */
    eqos_dma_chan_to_vmirq_map (osi_core);
  }

fail:
  return ret;
}

/**
 * @brief eqos_handle_mac_fpe_intrs
 *
 * Algorithm: This function takes care of handling the
 *      MAC FPE interrupts.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC interrupts need to be enabled
 */
static void
eqos_handle_mac_fpe_intrs (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t  val = 0;

  /* interrupt bit clear on read as CSR_SW is reset */
  val = osi_readla (
          osi_core,
          (nveu8_t *)osi_core->base + EQOS_MAC_FPE_CTS
          );

  if ((val & EQOS_MAC_FPE_CTS_RVER) == EQOS_MAC_FPE_CTS_RVER) {
    val &= ~EQOS_MAC_FPE_CTS_RVER;
    val |= EQOS_MAC_FPE_CTS_SRSP;
  }

  if ((val & EQOS_MAC_FPE_CTS_RRSP) == EQOS_MAC_FPE_CTS_RRSP) {
    /* received respose packet  Nothing to be done, it means other
     * IP also support FPE
     */
    val                &= ~EQOS_MAC_FPE_CTS_RRSP;
    val                &= ~EQOS_MAC_FPE_CTS_TVER;
    osi_core->fpe_ready = OSI_ENABLE;
    val                |= EQOS_MAC_FPE_CTS_EFPE;
  }

  if ((val & EQOS_MAC_FPE_CTS_TRSP) == EQOS_MAC_FPE_CTS_TRSP) {
    /* TX response packet sucessful */
    osi_core->fpe_ready = OSI_ENABLE;
    /* Enable frame preemption */
    val &= ~EQOS_MAC_FPE_CTS_TRSP;
    val &= ~EQOS_MAC_FPE_CTS_TVER;
    val |= EQOS_MAC_FPE_CTS_EFPE;
  }

  if ((val & EQOS_MAC_FPE_CTS_TVER) == EQOS_MAC_FPE_CTS_TVER) {
    /*Transmit verif packet sucessful*/
    osi_core->fpe_ready = OSI_DISABLE;
    val                &= ~EQOS_MAC_FPE_CTS_TVER;
    val                &= ~EQOS_MAC_FPE_CTS_EFPE;
  }

  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->base + EQOS_MAC_FPE_CTS
    );
}

/**
 * @brief eqos_handle_mac_link_intrs
 *
 * Algorithm: This function takes care of handling the
 *      MAC link interrupts.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC interrupts need to be enabled
 */
static void
eqos_handle_mac_link_intrs (
  struct osi_core_priv_data  *osi_core
  )
{
 #ifndef OSI_STRIPPED_LIB
  nveu32_t  mac_pcs = 0;
  nve32_t   ret     = 0;

  mac_pcs = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_PCS);
  /* check whether Link is UP or NOT - if not return. */
  if ((mac_pcs & EQOS_MAC_PCS_LNKSTS) == EQOS_MAC_PCS_LNKSTS) {
    /* check for Link mode (full/half duplex) */
    if ((mac_pcs & EQOS_MAC_PCS_LNKMOD) == EQOS_MAC_PCS_LNKMOD) {
      ret = hw_set_mode (osi_core, OSI_FULL_DUPLEX);
      if (osi_unlikely (ret < 0)) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "set mode in full duplex failed\n",
          0ULL
          );
      }
    } else {
      ret = hw_set_mode (osi_core, OSI_HALF_DUPLEX);
      if (osi_unlikely (ret < 0)) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "set mode in half duplex failed\n",
          0ULL
          );
      }
    }

    /* set speed at MAC level */
    /* TODO: set_tx_clk needs to be done */
    /* Maybe through workqueue for QNX */

    /* hw_set_speed is treated as void since it is
     * an internal functin which will be always success
     */
    if ((mac_pcs & EQOS_MAC_PCS_LNKSPEED) == EQOS_MAC_PCS_LNKSPEED_10) {
      (void)hw_set_speed (osi_core, OSI_SPEED_10);
    } else if ((mac_pcs & EQOS_MAC_PCS_LNKSPEED) == EQOS_MAC_PCS_LNKSPEED_100) {
      (void)hw_set_speed (osi_core, OSI_SPEED_100);
    } else if ((mac_pcs & EQOS_MAC_PCS_LNKSPEED) == EQOS_MAC_PCS_LNKSPEED_1000) {
      (void)hw_set_speed (osi_core, OSI_SPEED_1000);
    } else {
      /* Nothing here */
    }
  }

 #else
  (void)osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_PCS);
 #endif /* !OSI_STRIPPED_LIB */
}

/**
 * @brief eqos_handle_mac_intrs - Handle MAC interrupts
 *
 * @note
 * Algorithm:
 *  - This function takes care of handling the
 *    MAC interrupts to configure speed, mode for linkup use case.
 *   - Ignore handling for following cases:
 *    - If no MAC interrupt in dma_isr,
 *    - RGMII/SMII MAC interrupt
 *    - If link is down
 *  - Identify speed and mode changes from EQOS_MAC_PCS register and configure the same by calling
 *    hw_set_speed(), hw_set_mode()(proceed even on error for this call) API's.
 *  - SWUD_ID: ETHERNET_NVETHERNETRM_010_1
 *
 * @param[in] osi_core: OSI core private data structure. Used param base.
 * @param[in] dma_isr: DMA ISR register read value.
 *
 * @pre MAC interrupts need to be enabled
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void
eqos_handle_mac_intrs (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          dma_isr
  )
{
  nveu32_t  mac_imr = 0;
  nveu32_t  mac_isr = 0;

 #ifdef HSI_SUPPORT
  nveu64_t  tx_frame_err = 0;
 #endif
  mac_isr = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_ISR);

  /* Handle MAC interrupts */
  if ((dma_isr & EQOS_DMA_ISR_MACIS) == EQOS_DMA_ISR_MACIS) {
 #ifdef HSI_SUPPORT
    if (osi_core->mac_ver >= OSI_EQOS_MAC_5_30) {
      /* T23X-EQOS_HSIv2-19: Consistency Monitor for TX Frame */
      if ((dma_isr & EQOS_DMA_ISR_TXSTSIS) == EQOS_DMA_ISR_TXSTSIS) {
        osi_core->hsi.tx_frame_err_count =
          osi_update_stats_counter (
            osi_core->hsi.tx_frame_err_count,
            1UL
            );
        tx_frame_err = osi_core->hsi.tx_frame_err_count /
                       osi_core->hsi.err_count_threshold;
        if (osi_core->hsi.tx_frame_err_threshold < tx_frame_err) {
          osi_core->hsi.tx_frame_err_threshold             = tx_frame_err;
          osi_core->hsi.report_count_err[TX_FRAME_ERR_IDX] = OSI_ENABLE;
        }

        osi_core->hsi.err_code[TX_FRAME_ERR_IDX] = OSI_TX_FRAME_ERR;
        osi_core->hsi.report_err                 = OSI_ENABLE;
      }
    }

 #endif
    /* handle only those MAC interrupts which are enabled */
    mac_imr = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_IMR);
    mac_isr = (mac_isr & mac_imr);

    if (((mac_isr & EQOS_MAC_IMR_FPEIS) == EQOS_MAC_IMR_FPEIS) &&
        ((mac_imr & EQOS_IMR_FPEIE) == EQOS_IMR_FPEIE))
    {
      eqos_handle_mac_fpe_intrs (osi_core);
    }

    if ((mac_isr & EQOS_MAC_ISR_RGSMIIS) == EQOS_MAC_ISR_RGSMIIS) {
      eqos_handle_mac_link_intrs (osi_core);
    }
  }

  return;
}

#ifndef OSI_STRIPPED_LIB
/** \cond DO_NOT_DOCUMENT */

/**
 * @brief update_dma_sr_stats - stats for dma_status error
 *
 * @note
 * Algorithm:
 *  - increment error stats based on corresponding bit filed.
 *
 * @param[in, out] osi_core: OSI core private data structure.
 * @param[in] dma_sr: Dma status register read value
 * @param[in] qinx: Queue index
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void
update_dma_sr_stats (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          dma_sr,
  nveu32_t                          qinx
  )
{
  nveu64_t  val;

  if ((dma_sr & EQOS_DMA_CHX_STATUS_RBU) == EQOS_DMA_CHX_STATUS_RBU) {
    val                                        = osi_core->stats.rx_buf_unavail_irq_n[qinx];
    osi_core->stats.rx_buf_unavail_irq_n[qinx] =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & EQOS_DMA_CHX_STATUS_TPS) == EQOS_DMA_CHX_STATUS_TPS) {
    val                                         = osi_core->stats.tx_proc_stopped_irq_n[qinx];
    osi_core->stats.tx_proc_stopped_irq_n[qinx] =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & EQOS_DMA_CHX_STATUS_TBU) == EQOS_DMA_CHX_STATUS_TBU) {
    val                                        = osi_core->stats.tx_buf_unavail_irq_n[qinx];
    osi_core->stats.tx_buf_unavail_irq_n[qinx] =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & EQOS_DMA_CHX_STATUS_RPS) == EQOS_DMA_CHX_STATUS_RPS) {
    val                                         = osi_core->stats.rx_proc_stopped_irq_n[qinx];
    osi_core->stats.rx_proc_stopped_irq_n[qinx] =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & EQOS_DMA_CHX_STATUS_RWT) == EQOS_DMA_CHX_STATUS_RWT) {
    val                               = osi_core->stats.rx_watchdog_irq_n;
    osi_core->stats.rx_watchdog_irq_n =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & EQOS_DMA_CHX_STATUS_FBE) == EQOS_DMA_CHX_STATUS_FBE) {
    val                                   = osi_core->stats.fatal_bus_error_irq_n;
    osi_core->stats.fatal_bus_error_irq_n =
      osi_update_stats_counter (val, 1U);
  }
}

/** \endcond */
#endif /* !OSI_STRIPPED_LIB */

static void
eqos_handle_head_of_line_bloking (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          val
  )
{
  nveul64_t  stat_val = 0U;
  nveu32_t   sch_err  = 0U;
  nveu32_t   frm_err  = 0U;
  nveu32_t   temp     = 0U;
  nveu32_t   value    = 0U;
  nveu32_t   i        = 0;

  if ((val & EQOS_MTL_EST_STATUS_HLBS) == EQOS_MTL_EST_STATUS_HLBS) {
    osi_core->est_ready                  = OSI_DISABLE;
    stat_val                             = osi_core->stats.head_of_line_blk_sch;
    osi_core->stats.head_of_line_blk_sch =
      osi_update_stats_counter (stat_val, 1U);
    /* Need to read MTL_EST_Sch_Error register and cleared */
    sch_err = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                EQOS_MTL_EST_SCH_ERR
                );
    for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
      temp = OSI_ENABLE;
      temp = temp << i;
      if ((sch_err & temp) == temp) {
        stat_val                  = osi_core->stats.hlbs_q[i];
        osi_core->stats.hlbs_q[i] =
          osi_update_stats_counter (stat_val, 1U);
      }
    }

    sch_err &= 0xFFU;             /* only 8 TC allowed so clearing all */
    osi_writela (
      osi_core,
      sch_err,
      (nveu8_t *)osi_core->base + EQOS_MTL_EST_SCH_ERR
      );
    /* Disable est as error happen */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MTL_EST_CONTROL
              );
    /* DBFS 0 means do not packet */
    if ((value & EQOS_MTL_EST_CONTROL_DFBS) == OSI_DISABLE) {
      value &= ~EQOS_MTL_EST_CONTROL_EEST;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        EQOS_MTL_EST_CONTROL
        );
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "Disabling EST due to HLBS, correct GCL\n",
        OSI_NONE
        );
    }
  }

  if ((val & EQOS_MTL_EST_STATUS_HLBF) == EQOS_MTL_EST_STATUS_HLBF) {
    osi_core->est_ready                  = OSI_DISABLE;
    stat_val                             = osi_core->stats.head_of_line_blk_frm;
    osi_core->stats.head_of_line_blk_frm =
      osi_update_stats_counter (stat_val, 1U);
    /* Need to read MTL_EST_Frm_Size_Error register and cleared */
    frm_err = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                EQOS_MTL_EST_FRMS_ERR
                );
    for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
      temp = OSI_ENABLE;
      temp = temp << i;
      if ((frm_err & temp) == temp) {
        stat_val                  = osi_core->stats.hlbf_q[i];
        osi_core->stats.hlbf_q[i] =
          osi_update_stats_counter (stat_val, 1U);
      }
    }

    frm_err &= 0xFFU;             /* 8 TC allowed so clearing all */
    osi_writela (
      osi_core,
      frm_err,
      (nveu8_t *)osi_core->base +
      EQOS_MTL_EST_FRMS_ERR
      );
    /* Disable est as error happen */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MTL_EST_CONTROL
              );
    /* DDBF 1 means don't drop packet */
    if ((value & EQOS_MTL_EST_CONTROL_DDBF) ==
        EQOS_MTL_EST_CONTROL_DDBF)
    {
      value &= ~EQOS_MTL_EST_CONTROL_EEST;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        EQOS_MTL_EST_CONTROL
        );
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "Disabling EST due to HLBF, correct GCL\n",
        OSI_NONE
        );
    }
  }
}

/**
 * @brief eqos_handle_mtl_intrs - Handle MTL interrupts
 *
 * Algorithm: Code to handle interrupt for MTL EST error and status.
 * There are possible 4 errors which can be part of common interrupt in case of
 * MTL_EST_SCH_ERR (sheduling error)- HLBS
 * MTL_EST_FRMS_ERR (Frame size error) - HLBF
 * MTL_EST_FRMC_ERR (frame check error) - HLBF
 * Constant Gate Control Error - when time interval in less
 * than or equal to cycle time, llr = 1
 * There is one status interrupt which says swich to SWOL complete.
 *
 * @param[in] osi_core: osi core priv data structure
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void
eqos_handle_mtl_intrs (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t   val      = 0U;
  nveul64_t  stat_val = 0U;

  val = osi_readla (
          osi_core,
          (nveu8_t *)osi_core->base + EQOS_MTL_EST_STATUS
          );
  val &= (EQOS_MTL_EST_STATUS_CGCE | EQOS_MTL_EST_STATUS_HLBS |
          EQOS_MTL_EST_STATUS_HLBF | EQOS_MTL_EST_STATUS_BTRE |
          EQOS_MTL_EST_STATUS_SWLC);

  /* return if interrupt is not related to EST */
  if (val == OSI_DISABLE) {
    goto done;
  }

  /* increase counter write 1 back will clear */
  if ((val & EQOS_MTL_EST_STATUS_CGCE) == EQOS_MTL_EST_STATUS_CGCE) {
    osi_core->est_ready                = OSI_DISABLE;
    stat_val                           = osi_core->stats.const_gate_ctr_err;
    osi_core->stats.const_gate_ctr_err =
      osi_update_stats_counter (stat_val, 1U);
  }

  /* Handle HOL blocking */
  eqos_handle_head_of_line_bloking (osi_core, val);

  if ((val & EQOS_MTL_EST_STATUS_SWLC) == EQOS_MTL_EST_STATUS_SWLC) {
    if ((val & EQOS_MTL_EST_STATUS_BTRE) !=
        EQOS_MTL_EST_STATUS_BTRE)
    {
      osi_core->est_ready = OSI_ENABLE;
    }

    stat_val                             = osi_core->stats.sw_own_list_complete;
    osi_core->stats.sw_own_list_complete =
      osi_update_stats_counter (stat_val, 1U);
  }

  if ((val & EQOS_MTL_EST_STATUS_BTRE) == EQOS_MTL_EST_STATUS_BTRE) {
    osi_core->est_ready               = OSI_DISABLE;
    stat_val                          = osi_core->stats.base_time_reg_err;
    osi_core->stats.base_time_reg_err =
      osi_update_stats_counter (stat_val, 1U);
    osi_core->est_ready = OSI_DISABLE;
  }

  /* clear EST status register as interrupt is handled */
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->base + EQOS_MTL_EST_STATUS
    );

done:
  return;
}

#ifdef HSI_SUPPORT

/**
 * @brief eqos_handle_hsi_intr - Handles HSI interrupt.
 *
 * @note
 * Algorithm:
 *  - Clear HSI interrupt source.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void
eqos_handle_hsi_intr (
  struct osi_core_priv_data *const  osi_core
  )
{
  nveu32_t  val  = 0U;
  nveu32_t  val2 = 0U;
  nveu64_t  ce_count_threshold;

  val = osi_readla (
          osi_core,
          (nveu8_t *)osi_core->base +
          EQOS_WRAP_COMMON_INTR_STATUS
          );
  if (((val & EQOS_REGISTER_PARITY_ERR) == EQOS_REGISTER_PARITY_ERR) ||
      ((val & EQOS_CORE_UNCORRECTABLE_ERR) == EQOS_CORE_UNCORRECTABLE_ERR))
  {
    osi_core->hsi.err_code[UE_IDX]         = OSI_UNCORRECTABLE_ERR;
    osi_core->hsi.report_err               = OSI_ENABLE;
    osi_core->hsi.report_count_err[UE_IDX] = OSI_ENABLE;
    /* Disable the interrupt */
    val2 = osi_readla (
             osi_core,
             (nveu8_t *)osi_core->base +
             EQOS_WRAP_COMMON_INTR_ENABLE
             );
    val2 &= ~EQOS_REGISTER_PARITY_ERR;
    val2 &= ~EQOS_CORE_UNCORRECTABLE_ERR;
    osi_writela (
      osi_core,
      val2,
      (nveu8_t *)osi_core->base +
      EQOS_WRAP_COMMON_INTR_ENABLE
      );
  }

  if ((val & EQOS_CORE_CORRECTABLE_ERR) == EQOS_CORE_CORRECTABLE_ERR) {
    osi_core->hsi.err_code[CE_IDX] = OSI_CORRECTABLE_ERR;
    osi_core->hsi.report_err       = OSI_ENABLE;
    osi_core->hsi.ce_count         =
      osi_update_stats_counter (osi_core->hsi.ce_count, 1UL);
    ce_count_threshold = osi_core->hsi.ce_count / osi_core->hsi.err_count_threshold;
    if (osi_core->hsi.ce_count_threshold < ce_count_threshold) {
      osi_core->hsi.ce_count_threshold       = ce_count_threshold;
      osi_core->hsi.report_count_err[CE_IDX] = OSI_ENABLE;
    }
  }

  val &= ~EQOS_MAC_SBD_INTR;
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->base +
    EQOS_WRAP_COMMON_INTR_STATUS
    );

  if (((val & EQOS_CORE_CORRECTABLE_ERR) == EQOS_CORE_CORRECTABLE_ERR) ||
      ((val & EQOS_CORE_UNCORRECTABLE_ERR) == EQOS_CORE_UNCORRECTABLE_ERR))
  {
    /* Clear FSM error status. Clear on read */
    (void)osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MAC_DPP_FSM_INTERRUPT_STATUS
            );

    /* Clear ECC error status register */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_ECC_INTERRUPT_STATUS
            );
    if (val != 0U) {
      osi_writela (
        osi_core,
        val,
        (nveu8_t *)osi_core->base +
        EQOS_MTL_ECC_INTERRUPT_STATUS
        );
    }

    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_DMA_ECC_INTERRUPT_STATUS
            );
    if (val != 0U) {
      osi_writela (
        osi_core,
        val,
        (nveu8_t *)osi_core->base +
        EQOS_DMA_ECC_INTERRUPT_STATUS
        );
    }
  }
}

#endif

/**
 * @brief eqos_handle_common_intr - Handles common interrupt.
 *
 * @note
 * Algorithm:
 *  - Reads DMA ISR register
 *   - Returns if calue is 0.
 *   - Handle Non-TI/RI interrupts for all MTL queues and
 *     increments osi_core_priv_data->stats
 *     based on error detected per cahnnel.
 *  - Calls eqos_handle_mac_intrs() to handle MAC interrupts.
 *  - Refer to EQOS column of <<RM_10, (sequence diagram)>> for API details.
 *  - TraceID:ETHERNET_NVETHERNETRM_010
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void
eqos_handle_common_intr (
  struct osi_core_priv_data *const  osi_core
  )
{
  void      *base   = osi_core->base;
  nveu32_t  dma_isr = 0;
  nveu32_t  qinx    = 0;
  nveu32_t  i       = 0;
  nveu32_t  dma_sr  = 0;
  nveu32_t  dma_ier = 0;
  nveu32_t  mtl_isr = 0;
  nveu32_t  frp_isr = 0U;

  if (osi_core->mac_ver >= OSI_EQOS_MAC_5_30) {
    osi_writela (
      osi_core,
      EQOS_MAC_SBD_INTR,
      (nveu8_t *)osi_core->base +
      EQOS_WRAP_COMMON_INTR_STATUS
      );
 #ifdef HSI_SUPPORT
    if ((osi_core->hsi.enabled == OSI_ENABLE) &&
        (osi_core->mac_ver < OSI_EQOS_MAC_5_40))
    {
      eqos_handle_hsi_intr (osi_core);
    }

 #endif
  }

  dma_isr = osi_readla (osi_core, (nveu8_t *)base + EQOS_DMA_ISR);
  if (dma_isr != 0U) {
    // FIXME Need to check how we can get the DMA channel here instead of
    // MTL Queues
    if ((dma_isr & EQOS_DMA_CHAN_INTR_STATUS) != 0U) {
      /* Handle Non-TI/RI interrupts */
      for (i = 0; i < osi_core->num_mtl_queues; i++) {
        qinx = osi_core->mtl_queues[i];
        if (qinx >= OSI_EQOS_MAX_NUM_CHANS) {
          continue;
        }

        /* read dma channel status register */
        dma_sr = osi_readla (
                   osi_core,
                   (nveu8_t *)base +
                   EQOS_DMA_CHX_STATUS (qinx)
                   );
        /* read dma channel interrupt enable register */
        dma_ier = osi_readla (
                    osi_core,
                    (nveu8_t *)base +
                    EQOS_DMA_CHX_IER (qinx)
                    );

        /* process only those interrupts which we
         * have enabled.
         */
        dma_sr = (dma_sr & dma_ier);

        /* mask off RI and TI */
        dma_sr &= ~(OSI_BIT (6) | OSI_BIT (0));
        if (dma_sr == 0U) {
          continue;
        }

        /* ack non ti/ri ints */
        osi_writela (
          osi_core,
          dma_sr,
          (nveu8_t *)base +
          EQOS_DMA_CHX_STATUS (qinx)
          );
 #ifndef OSI_STRIPPED_LIB
        update_dma_sr_stats (osi_core, dma_sr, qinx);
 #endif /* !OSI_STRIPPED_LIB */
      }
    }

    eqos_handle_mac_intrs (osi_core, dma_isr);

    /* Handle MTL inerrupts */
    mtl_isr = osi_readla (osi_core, (nveu8_t *)base + EQOS_MTL_INTR_STATUS);
    if (((mtl_isr & EQOS_MTL_IS_ESTIS) == EQOS_MTL_IS_ESTIS) &&
        ((dma_isr & EQOS_DMA_ISR_MTLIS) == EQOS_DMA_ISR_MTLIS))
    {
      eqos_handle_mtl_intrs (osi_core);
      mtl_isr &= ~EQOS_MTL_IS_ESTIS;
      osi_writela (osi_core, mtl_isr, (nveu8_t *)base + EQOS_MTL_INTR_STATUS);
    }

    /* Clear FRP Interrupt MTL_RXP_Interrupt_Control_Status */
    frp_isr  = osi_readla (osi_core, (nveu8_t *)base + EQOS_MTL_RXP_INTR_CS);
    frp_isr |= (EQOS_MTL_RXP_INTR_CS_NVEOVIS | EQOS_MTL_RXP_INTR_CS_NPEOVIS |
                EQOS_MTL_RXP_INTR_CS_FOOVIS | EQOS_MTL_RXP_INTR_CS_PDRFIS);
    osi_writela (osi_core, frp_isr, (nveu8_t *)base + EQOS_MTL_RXP_INTR_CS);
  } else {
    /* Do Nothing */
  }
}

#if defined (MACSEC_SUPPORT)

/**
 * @brief eqos_config_mac_tx - Enable/Disable MAC Tx
 *
 * @note
 * Algorithm:
 *  - Enable or Disables MAC Transmitter
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: Enable or Disable.MAC Tx
 *
 * @pre MAC init should be complete. See osi_hw_core_init()
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void
eqos_config_mac_tx (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable
  )
{
  nveu32_t  value;
  void      *addr = osi_core->base;

  if (enable == OSI_ENABLE) {
    value = osi_readla (osi_core, (nveu8_t *)addr + EQOS_MAC_MCR);
    /* Enable MAC Transmit */
    value |= EQOS_MCR_TE;
    osi_writela (osi_core, value, (nveu8_t *)addr + EQOS_MAC_MCR);
  } else {
    value = osi_readla (osi_core, (nveu8_t *)addr + EQOS_MAC_MCR);
    /* Disable MAC Transmit */
    value &= ~EQOS_MCR_TE;
    osi_writela (osi_core, value, (nveu8_t *)addr + EQOS_MAC_MCR);
  }
}

#endif /*  MACSEC_SUPPORT */

/**
 * @brief eqos_update_mac_addr_helper - Function to update DCS and MBC; helper function for
 * eqos_update_mac_addr_low_high_reg()
 *
 * @note
 * Algorithm:
 *  - Validation of dma_chan if dma_routing_enable is OSI_ENABLE and addr_mask
 *   - corresponding sections not updated if invalid.
 *  - This helper routine is to update value parameter based on DCS and MBC
 *  sections of L2 register.
 *  dsc_en status performed before updating DCS bits.
 *  - Refer to EQOS column of <<RM_18, (sequence diagram)>> for API details.
 *  - TraceID:ETHERNET_NVETHERNETRM_018
 *
 * @param[in] osi_core: OSI core private data structure. Used param base.
 * @param[out] value: nveu32_t pointer which has value read from register.
 * @param[in] idx: Refer osi_filter->index for details.
 * @param[in] dma_chan: Refer osi_filter->dma_chan for details.
 * @param[in] dma_chansel: Refer osi_filter->dma_chansel for details.
 * @param[in] addr_mask: Refer osi_filter->addr_mask for details.
 * @param[in] src_dest: source/destination MAC address.
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *  - osi_core->osd should be populated.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t
eqos_update_mac_addr_helper (
  const struct osi_core_priv_data  *osi_core,
  nveu32_t                         *value,
  const nveu32_t                   idx,
  const nveu32_t                   dma_chan,
  const nveu32_t                   dma_chansel,
  const nveu32_t                   addr_mask,
  OSI_UNUSED const nveu32_t        src_dest
  )
{
  nveu32_t  temp;
  nve32_t   ret = 0;

  (void)src_dest;       // unused

  /* PDC bit of MAC_Ext_Configuration register is set so binary
   * value representation form index 32-127 else hot-bit
   * representation.
   */
  if ((idx < EQOS_MAX_MAC_ADDR_REG) &&
      (osi_core->mac_ver >= OSI_EQOS_MAC_5_00))
  {
    *value &= EQOS_MAC_ADDRH_DCS;
    temp    = (OSI_BIT (dma_chan) | dma_chansel);
    temp    = temp << EQOS_MAC_ADDRH_DCS_SHIFT;
    temp    = temp & EQOS_MAC_ADDRH_DCS;
    *value  = *value | temp;
  } else {
    *value = OSI_DISABLE;
    temp   = dma_chan;
    temp   = temp << EQOS_MAC_ADDRH_DCS_SHIFT;
    temp   = temp & EQOS_MAC_ADDRH_DCS;
    *value = temp;
  }

  /* Address mask is valid for address 1 to 31 index only */
  if ((addr_mask <= EQOS_MAX_MASK_BYTE) &&
      (addr_mask > OSI_AMASK_DISABLE))
  {
    if ((idx > 0U) && (idx < EQOS_MAX_MAC_ADDR_REG)) {
      *value = (*value |
                ((addr_mask << EQOS_MAC_ADDRH_MBC_SHIFT) &
                 EQOS_MAC_ADDRH_MBC));
    } else {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "invalid address index for MBC\n",
        0ULL
        );
      ret = -1;
    }
  }

  return ret;
}

/**
 * @brief eqos_l2_filter_delete - Function to delete L2 filter
 *
 * @note
 * Algorithm:
 *  - This helper routine is to delete L2 filter based on DCS and MBC
 *    parameter.
 *  - Handling for EQOS mac version 4.10 differently.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[out] value: nveu32_t pointer which has value read from register.
 * @param[in] filter_idx: filter index
 * @param[in] dma_routing_enable: dma channel routing enable(1)
 * @param[in] dma_chan: dma channel number
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *  - osi_core->osd should be populated.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 */
static void
eqos_l2_filter_delete (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   *value,
  const nveu32_t             filter_idx,
  const nveu32_t             dma_routing_enable,
  const nveu32_t             dma_chan
  )
{
  nveu32_t  dcs_check = *value;
  nveu32_t  temp      = OSI_DISABLE;
  nveu32_t  idx       = (filter_idx  & 0xFFU);

  osi_writela (
    osi_core,
    OSI_MAX_32BITS,
    (nveu8_t *)osi_core->base + EQOS_MAC_ADDRL ((idx))
    );

  *value |= OSI_MASK_16BITS;
 #ifdef OSI_STRIPPED_LIB
  (void)dma_routing_enable;
 #endif
 #ifndef OSI_STRIPPED_LIB
  if ((dma_routing_enable == OSI_DISABLE) ||
      (osi_core->mac_ver < OSI_EQOS_MAC_5_00))
  {
    *value &= ~(EQOS_MAC_ADDRH_AE | EQOS_MAC_ADDRH_DCS);
    osi_writela (
      osi_core,
      *value,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_ADDRH ((idx))
      );
  } else
 #endif /* !OSI_STRIPPED_LIB */
  {
    dcs_check &= EQOS_MAC_ADDRH_DCS;
    dcs_check  = dcs_check >> EQOS_MAC_ADDRH_DCS_SHIFT;

    if (idx >= EQOS_MAX_MAC_ADDR_REG) {
      dcs_check = OSI_DISABLE;
    } else {
      temp       = OSI_BIT (dma_chan);
      dcs_check &= ~(temp);
    }

    if (dcs_check == OSI_DISABLE) {
      *value &= ~(EQOS_MAC_ADDRH_AE | EQOS_MAC_ADDRH_DCS);
      osi_writela (
        osi_core,
        *value,
        (nveu8_t *)osi_core->base +
        EQOS_MAC_ADDRH ((idx))
        );
    } else {
      *value &= ~(EQOS_MAC_ADDRH_DCS);
      *value |= (dcs_check << EQOS_MAC_ADDRH_DCS_SHIFT);
      osi_writela (
        osi_core,
        *value,
        (nveu8_t *)osi_core->base +
        EQOS_MAC_ADDRH ((idx))
        );
    }
  }

  return;
}

/**
 * @brief eqos_update_mac_addr_low_high_reg- Update L2 address in filter
 *    register
 *
 * @note
 * Algorithm:
 *  - This routine validates index and addr of osi_filter.
 *  - calls eqos_update_mac_addr_helper() to update DCS and MBS.
 *  dsc_en status performed before updating DCS bits.
 *  - Update MAC address to L2 filter register.
 *  - Refer to EQOS column of <<RM_18, (sequence diagram)>> for API details.
 *  - TraceID:ETHERNET_NVETHERNETRM_018
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter: OSI filter structure.
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *  - osi_core->osd should be populated.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_update_mac_addr_low_high_reg (
  struct osi_core_priv_data *const  osi_core,
  const struct osi_filter           *filter
  )
{
  const struct core_local  *l_core            = (struct core_local *)(void *)osi_core;
  nveu32_t                 idx                = filter->index;
  nveu32_t                 dma_routing_enable = filter->dma_routing;
  nveu32_t                 dma_chan           = filter->dma_chan;
  nveu64_t                 dma_chansel        = filter->dma_chansel;
  nveu32_t                 addr_mask          = filter->addr_mask;
  nveu32_t                 src_dest           = filter->src_dest;
  nveu32_t                 value              = OSI_DISABLE;
  nve32_t                  ret                = 0;
  const nveu32_t           eqos_max_madd[2]   = {
    EQOS_MAX_MAC_ADDRESS_FILTER,
    EQOS_MAX_MAC_5_3_ADDRESS_FILTER
  };

  if ((idx >= eqos_max_madd[l_core->l_mac_ver]) ||
      (dma_chan >= OSI_EQOS_MAX_NUM_CHANS))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "invalid MAC filter index or channel number\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  /* read current value at index preserve DCS current value */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MAC_ADDRH ((idx))
            );

  /* High address reset DCS and AE bits*/
  if ((filter->oper_mode & OSI_OPER_ADDR_DEL) != OSI_NONE) {
    eqos_l2_filter_delete (
      osi_core,
      &value,
      idx,
      dma_routing_enable,
      dma_chan
      );
  } else {
    ret = eqos_update_mac_addr_helper (
            osi_core,
            &value,
            idx,
            dma_chan,
            dma_chansel,
            addr_mask,
            src_dest
            );
    /* Check return value from helper code */
    if (ret == -1) {
      goto fail;
    }

    /* Update AE bit */
    value |= EQOS_MAC_ADDRH_AE;

    /* Setting Source/Destination Address match valid for 1 to 32 index */
    if (((idx > 0U) && (idx < EQOS_MAX_MAC_ADDR_REG)) && (src_dest <= OSI_SA_MATCH)) {
      value = (value | ((src_dest << EQOS_MAC_ADDRH_SA_SHIFT) &
                        EQOS_MAC_ADDRH_SA));
    }

    osi_writela (
      osi_core,
      ((nveu32_t)filter->mac_addr[4] |
       ((nveu32_t)filter->mac_addr[5] << 8) | value),
      (nveu8_t *)osi_core->base + EQOS_MAC_ADDRH ((idx))
      );

    osi_writela (
      osi_core,
      ((nveu32_t)filter->mac_addr[0] |
       ((nveu32_t)filter->mac_addr[1] << 8) |
       ((nveu32_t)filter->mac_addr[2] << 16) |
       ((nveu32_t)filter->mac_addr[3] << 24)),
      (nveu8_t *)osi_core->base +  EQOS_MAC_ADDRL ((idx))
      );
  }

fail:
  return ret;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief eqos_config_ptp_offload - Enable/Disable PTP offload
 *
 * Algorithm: Based on input argument, update PTO and TSCR registers.
 * Update ptp_filter for TSCR register.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] pto_config: The PTP Offload configuration from function
 *            driver.
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) configure_ptp() should be called after this API
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_ptp_offload (
  struct osi_core_priv_data *const  osi_core,
  struct osi_pto_config *const      pto_config
  )
{
  nveu8_t   *addr     = (nveu8_t *)osi_core->base;
  nve32_t   ret       = 0;
  nveu32_t  value     = 0x0U;
  nveu32_t  ptc_value = 0x0U;
  nveu32_t  port_id   = 0x0U;

  /* Read MAC TCR */
  value = osi_readla (osi_core, (nveu8_t *)addr + EQOS_MAC_TCR);
  /* clear old configuration */
  value &= ~(EQOS_MAC_TCR_TSENMACADDR | OSI_MAC_TCR_SNAPTYPSEL_3 |
             OSI_MAC_TCR_TSMASTERENA | OSI_MAC_TCR_TSEVENTENA |
             OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT |
             OSI_MAC_TCR_TSCTRLSSR | OSI_MAC_TCR_TSVER2ENA |
             OSI_MAC_TCR_TSIPENA);

  /** Handle PTO disable */
  if (pto_config->en_dis == OSI_DISABLE) {
    osi_core->ptp_config.ptp_filter = value;
    osi_writela (osi_core, ptc_value, addr + EQOS_MAC_PTO_CR);
    osi_writela (osi_core, value, addr + EQOS_MAC_TCR);
    osi_writela (osi_core, OSI_NONE, addr + EQOS_MAC_PIDR0);
    osi_writela (osi_core, OSI_NONE, addr + EQOS_MAC_PIDR1);
    osi_writela (osi_core, OSI_NONE, addr + EQOS_MAC_PIDR2);
    return 0;
  }

  /** Handle PTO enable */
  /* Set PTOEN bit */
  ptc_value |= EQOS_MAC_PTO_CR_PTOEN;
  ptc_value |= ((pto_config->domain_num << EQOS_MAC_PTO_CR_DN_SHIFT)
                & EQOS_MAC_PTO_CR_DN);
  /* Set TSCR register flag */
  value |= (OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT |
            OSI_MAC_TCR_TSCTRLSSR | OSI_MAC_TCR_TSVER2ENA |
            OSI_MAC_TCR_TSIPENA);

  if (pto_config->snap_type > 0U) {
    /* Set APDREQEN bit if snap_type > 0 */
    ptc_value |= EQOS_MAC_PTO_CR_APDREQEN;
  }

  /* Set SNAPTYPSEL for Taking Snapshots mode */
  value |= ((pto_config->snap_type << EQOS_MAC_TCR_SNAPTYPSEL_SHIFT) &
            OSI_MAC_TCR_SNAPTYPSEL_3);

  /* Set/Reset TSMSTRENA bit for Master/Slave */
  if (pto_config->master == OSI_ENABLE) {
    /* Set TSMSTRENA bit for master */
    value |= OSI_MAC_TCR_TSMASTERENA;
    if (pto_config->snap_type != OSI_PTP_SNAP_P2P) {
      /* Set ASYNCEN bit on PTO Control Register */
      ptc_value |= EQOS_MAC_PTO_CR_ASYNCEN;
    }
  } else {
    /* Reset TSMSTRENA bit for slave */
    value &= ~OSI_MAC_TCR_TSMASTERENA;
  }

  /* Set/Reset TSENMACADDR bit for UC/MC MAC */
  if (pto_config->mc_uc == OSI_ENABLE) {
    /* Set TSENMACADDR bit for MC/UC MAC PTP filter */
    value |= EQOS_MAC_TCR_TSENMACADDR;
  } else {
    /* Reset TSENMACADDR bit */
    value &= ~EQOS_MAC_TCR_TSENMACADDR;
  }

  /* Set TSEVENTENA bit for PTP events */
  value                          |= OSI_MAC_TCR_TSEVENTENA;
  osi_core->ptp_config.ptp_filter = value;
  /** Write PTO_CR and TCR registers */
  osi_writela (osi_core, ptc_value, addr + EQOS_MAC_PTO_CR);
  osi_writela (osi_core, value, addr + EQOS_MAC_TCR);
  /* Port ID for PTP offload packet created */
  port_id = pto_config->portid & EQOS_MAC_PIDR_PID_MASK;
  osi_writela (osi_core, port_id, addr + EQOS_MAC_PIDR0);
  osi_writela (osi_core, OSI_NONE, addr + EQOS_MAC_PIDR1);
  osi_writela (osi_core, OSI_NONE, addr + EQOS_MAC_PIDR2);

  return ret;
}

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief eqos_config_l3l4_filters - Config L3L4 filters.
 *
 * @note
 * Algorithm:
 * - This sequence is used to configure L3L4 filters for SA and DA Port Number matching.
 * - Prepare register data using prepare_l3l4_registers().
 * - Write l3l4 reigsters using mgbe_l3l4_filter_write().
 * - Return 0 on success.
 * - Return -1 on any register failure.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no_r: filter index
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (osi_l3_l4_filter)
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_l3l4_filters (
  struct osi_core_priv_data *const      osi_core,
  nveu32_t                              filter_no_r,
  const struct osi_l3_l4_filter *const  l3_l4
  )
{
  void  *base = osi_core->base;

 #ifndef OSI_STRIPPED_LIB
  nveu32_t  l3_addr0_reg = 0;
  nveu32_t  l3_addr2_reg = 0;
  nveu32_t  l3_addr3_reg = 0;
  nveu32_t  l4_addr_reg  = 0;
 #endif /* !OSI_STRIPPED_LIB */
  nveu32_t  l3_addr1_reg = 0;
  nveu32_t  ctr_reg      = 0;
  nveu32_t  filter_no    = filter_no_r & (OSI_MGBE_MAX_L3_L4_FILTER - 1U);

  prepare_l3l4_registers (
    osi_core,
    l3_l4,
 #ifndef OSI_STRIPPED_LIB
    &l3_addr0_reg,
    &l3_addr2_reg,
    &l3_addr3_reg,
    &l4_addr_reg,
 #endif /* !OSI_STRIPPED_LIB */
    &l3_addr1_reg,
    &ctr_reg
    );

 #ifndef OSI_STRIPPED_LIB
  /* Update l3 ip addr MGBE_MAC_L3_AD0R register */
  osi_writela (osi_core, l3_addr0_reg, (nveu8_t *)base + EQOS_MAC_L3_AD0R (filter_no));

  /* Update l3 ip addr MGBE_MAC_L3_AD2R register */
  osi_writela (osi_core, l3_addr2_reg, (nveu8_t *)base + EQOS_MAC_L3_AD2R (filter_no));

  /* Update l3 ip addr MGBE_MAC_L3_AD3R register */
  osi_writela (osi_core, l3_addr3_reg, (nveu8_t *)base + EQOS_MAC_L3_AD3R (filter_no));

  /* Update l4 port EQOS_MAC_L4_ADR register */
  osi_writela (osi_core, l4_addr_reg, (nveu8_t *)base + EQOS_MAC_L4_ADR (filter_no));
 #endif /* !OSI_STRIPPED_LIB */

  /* Update l3 ip addr MGBE_MAC_L3_AD1R register */
  osi_writela (osi_core, l3_addr1_reg, (nveu8_t *)base + EQOS_MAC_L3_AD1R (filter_no));

  /* Write CTR register */
  osi_writela (osi_core, ctr_reg, (nveu8_t *)base + EQOS_MAC_L3L4_CTR (filter_no));

  return 0;
}

/**
 * @brief eqos_poll_for_update_ts_complete - Poll for update time stamp
 *
 * @note
 * Algorithm:
 *  - Read time stamp update value from TCR register until it is
 *    equal to zero.
 *   - Max loop count of 1000 with 1 ms delay between iterations.
 *  - SWUD_ID: ETHERNET_NVETHERNETRM_022_1
 *
 * @param[in] osi_core: OSI core private data structure.  Used param is base, osd_ops.udelay.
 * @param[in, out] mac_tcr: Address to store time stamp control register read
 *  value
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t
eqos_poll_for_update_ts_complete (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          *mac_tcr
  )
{
  nveu32_t  retry = OSI_POLL_COUNT;
  nveu32_t  count;
  nve32_t   cond = COND_NOT_MET;
  nve32_t   ret  = 0;

  /* Wait for previous(if any) time stamp  value update to complete */
  count = 0;
  while (cond == COND_NOT_MET) {
    if (count > retry) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "poll_for_update_ts: timeout\n",
        0ULL
        );
      ret = -1;
      goto fail;
    }

    /* Read and Check TSUPDT in MAC_Timestamp_Control register */
    *mac_tcr = osi_readla (
                 osi_core,
                 (nveu8_t *)osi_core->base +
                 EQOS_MAC_TCR
                 );
    if ((*mac_tcr & EQOS_MAC_TCR_TSUPDT) == 0U) {
      cond = COND_MET;
    }

    count++;
    osi_core->osd_ops.udelay (OSI_DELAY_1US);
  }

fail:
  return ret;
}

/**
 * @brief eqos_adjust_mactime - Adjust MAC time with system time
 *
 * @note
 * Algorithm:
 *  - Update MAC time with system time based on input arguments(except osi_core)
 *  - Calls eqos_poll_for_update_ts_complete() before and after setting time.
 *   - return -1 if API fails.
 *  - Refer to EQOS column of <<RM_22, (sequence diagram)>> for API details.
 *  - TraceID:ETHERNET_NVETHERNETRM_022
 *
 * @param[in] osi_core: OSI core private data structure. Used param is base.
 * @param[in] sec: Seconds to be configured
 * @param[in] nsec: Nano seconds to be configured
 * @param[in] add_sub: To decide on add/sub with system time
 * @param[in] one_nsec_accuracy: One nano second accuracy
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *  - osi_core->ptp_config.one_nsec_accuracy need to be set to 1
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_adjust_mactime (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    sec,
  const nveu32_t                    nsec,
  const nveu32_t                    add_sub,
  OSI_UNUSED const nveu32_t         one_nsec_accuracy
  )
{
  void       *addr   = osi_core->base;
  nveu32_t   mac_tcr = 0U;
  nveu32_t   value   = 0;
  nveul64_t  temp    = 0;
  nveu32_t   sec1    = sec;
  nveu32_t   nsec1   = nsec;
  nve32_t    ret     = 0;

  (void)one_nsec_accuracy;
  ret = eqos_poll_for_update_ts_complete (osi_core, &mac_tcr);
  if (ret == -1) {
    goto fail;
  }

  if (add_sub != 0U) {
    /* If the new sec value needs to be subtracted with
     * the system time, then MAC_STSUR reg should be
     * programmed with (2^32 – <new_sec_value>)
     */
    temp = (TWO_POWER_32 - sec1);
    if (temp < UINT_MAX) {
      sec1 = (nveu32_t)temp;
    } else {
      /* do nothing here */
    }

    /* If the new nsec value need to be subtracted with
     * the system time, then MAC_STNSUR.TSSS field should be
     * programmed with, (10^9 - <new_nsec_value>) if
     * MAC_TCR.TSCTRLSSR is set or
     * (2^32 - <new_nsec_value> if MAC_TCR.TSCTRLSSR is reset)
     */
    /* one_nsec_accuracy is always enabled*/
    if (nsec1 < UINT_MAX) {
      nsec1 = (TEN_POWER_9 - nsec1);
    }
  }

  /* write seconds value to MAC_System_Time_Seconds_Update register */
  osi_writela (osi_core, sec1, (nveu8_t *)addr + EQOS_MAC_STSUR);

  /* write nano seconds value and add_sub to
   * MAC_System_Time_Nanoseconds_Update register
   */
  value |= nsec1;
  value |= add_sub << EQOS_MAC_STNSUR_ADDSUB_SHIFT;
  osi_writela (osi_core, value, (nveu8_t *)addr + EQOS_MAC_STNSUR);

  /* issue command to initialize system time with the value
   * specified in MAC_STSUR and MAC_STNSUR
   */
  mac_tcr |= EQOS_MAC_TCR_TSUPDT;
  osi_writela (osi_core, mac_tcr, (nveu8_t *)addr + EQOS_MAC_TCR);

  ret = eqos_poll_for_update_ts_complete (osi_core, &mac_tcr);

fail:
  return ret;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief eqos_config_ptp_rxq - To config PTP RX packets queue
 *
 * Algorithm: This function is used to program the PTP RX packets queue.
 *
 * @param[in] osi_core: OSI core private data.
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_ptp_rxq (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    rxq_idx,
  const nveu32_t                    enable
  )
{
  nveu8_t   *base = osi_core->base;
  nveu32_t  value = OSI_NONE;
  nveu32_t  i     = 0U;

  /* Validate the RX queue index argment */
  if (rxq_idx >= OSI_EQOS_MAX_NUM_QUEUES) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid PTP RX queue index\n",
      rxq_idx
      );
    return -1;
  }

  /* Check MAC version */
  if (osi_core->mac_ver <= OSI_EQOS_MAC_5_00) {
    /* MAC 4_10 and 5 doesn't have PTP RX Queue route support */
    return 0;
  }

  /* Validate enable argument */
  if ((enable != OSI_ENABLE) && (enable != OSI_DISABLE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid enable input\n",
      enable
      );
    return -1;
  }

  /* Validate PTP RX queue enable */
  for (i = 0; i < osi_core->num_mtl_queues; i++) {
    if (osi_core->mtl_queues[i] == rxq_idx) {
      /* Given PTP RX queue is enabled */
      break;
    }
  }

  if (i == osi_core->num_mtl_queues) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "PTP RX queue not enabled\n",
      rxq_idx
      );
    return -1;
  }

  /* Read MAC_RxQ_Ctrl1 */
  value = osi_readla (osi_core, (nveu8_t *)base + EQOS_MAC_RQC1R);
  if (enable == OSI_DISABLE) {
    /** Reset OMCBCQ bit to disable over-riding the MCBC Queue
     * priority for the PTP RX queue.
     */
    value &= ~EQOS_MAC_RQC1R_OMCBCQ;
  } else {
    /* Program PTPQ with ptp_rxq */
    osi_core->ptp_config.ptp_rx_queue = rxq_idx;
    value                            &= ~EQOS_MAC_RQC1R_PTPQ;
    value                            |= (rxq_idx << EQOS_MAC_RQC1R_PTPQ_SHIFT);
    /* Reset TPQC before setting TPQC0 */
    value &= ~EQOS_MAC_RQC1R_TPQC;

    /** Set TPQC to 0x1 for VLAN Tagged PTP over
     * ethernet packets are routed to Rx Queue specified
     * by PTPQ field
     **/
    value |= EQOS_MAC_RQC1R_TPQC0;

    /** Set OMCBCQ bit to enable over-riding the MCBC Queue
     * priority for the PTP RX queue.
     */
    value |= EQOS_MAC_RQC1R_OMCBCQ;
  }

  /* Write MAC_RxQ_Ctrl1 */
  osi_writela (osi_core, value, base + EQOS_MAC_RQC1R);

  return 0;
}

#endif /* !OSI_STRIPPED_LIB */

/** \cond DO_NOT_DOCUMENT */

/**
 * @brief poll_for_mii_idle Query the status of an ongoing DMA transfer
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 * @pre MAC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t
poll_for_mii_idle (
  struct osi_core_priv_data  *osi_core
  )
{
  /* half sec timeout */
  nveu32_t  retry = ((RETRY_COUNT) * 50U);
  nveu32_t  mac_gmiiar;
  nveu32_t  count;
  nve32_t   cond = COND_NOT_MET;
  nve32_t   ret  = 0;

  count = 0;
  while (cond == COND_NOT_MET) {
    if (count > retry) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "MII operation timed out\n",
        0ULL
        );
      ret = -1;
      goto fail;
    }

    count++;

    mac_gmiiar = osi_readla (
                   osi_core,
                   (nveu8_t *)osi_core->base +
                   EQOS_MAC_MDIO_ADDRESS
                   );
    if ((mac_gmiiar & EQOS_MAC_GMII_BUSY) == 0U) {
      /* exit loop */
      cond = COND_MET;
    } else {
      /* wait on GMII Busy set */
      osi_core->osd_ops.usleep (OSI_DELAY_10US);
    }
  }

fail:
  return ret;
}

/** \endcond */

/**
 * @brief eqos_write_phy_reg - Write to a PHY register through MAC over MDIO bus
 *
 * @note
 * Algorithm:
 *  - Before proceeding for reading for PHY register check whether any MII
 *    operation going on MDIO bus by polling MAC_GMII_BUSY bit.
 *  - Program data into MAC MDIO data register.
 *  - Populate required parameters like phy address, phy register etc,,
 *    in MAC MDIO Address register. write and GMII busy bits needs to be set
 *    in this operation.
 *  - Write into MAC MDIO address register poll for GMII busy for MDIO
 *  operation to complete.
 *  - Refer to EQOS column of <<RM_02, (sequence diagram)>> for API details.
 *  - Traceid:ETHERNET_NVETHERNETRM_002
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be write to PHY.
 * @param[in] phydata: Data to write to a PHY register.
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_write_phy_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    phyreg,
  const nveu16_t                    phydata
  )
{
  nveu32_t  mac_gmiiar;
  nveu32_t  mac_gmiidr;
  nveu32_t  devaddr;
  nve32_t   ret = 0;

  /* Wait for any previous MII read/write operation to complete */
  ret = poll_for_mii_idle (osi_core);
  if (ret < 0) {
    /* poll_for_mii_idle fail */
    goto fail;
  }

  /* C45 register access */
  if ((phyreg & OSI_MII_ADDR_C45) == OSI_MII_ADDR_C45) {
    /* Write the PHY register address and data into data register */
    mac_gmiidr = (phyreg & EQOS_MDIO_DATA_REG_PHYREG_MASK) <<
                 EQOS_MDIO_DATA_REG_PHYREG_SHIFT;
    mac_gmiidr |= phydata;
    osi_writela (
      osi_core,
      mac_gmiidr,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_MDIO_DATA
      );

    /* Extract the device address */
    devaddr = (phyreg >> EQOS_MDIO_DATA_REG_DEV_ADDR_SHIFT) &
              EQOS_MDIO_DATA_REG_DEV_ADDR_MASK;

    /* Initiate the clause 45 transaction with auto
     * generation of address packet
     */
    mac_gmiiar = (EQOS_MDIO_PHY_REG_C45E |
                  ((phyaddr) << EQOS_MDIO_PHY_ADDR_SHIFT) |
                  (devaddr << EQOS_MDIO_PHY_REG_SHIFT) |
                  ((osi_core->mdc_cr) << EQOS_MDIO_PHY_REG_CR_SHIF) |
                  (EQOS_MDIO_PHY_REG_WRITE) | (EQOS_MAC_GMII_BUSY));
  } else {
    mac_gmiidr = osi_readla (
                   osi_core,
                   (nveu8_t *)osi_core->base +
                   EQOS_MAC_MDIO_DATA
                   );
    mac_gmiidr = ((mac_gmiidr & EQOS_MAC_GMIIDR_GD_WR_MASK) |
                  ((phydata) & EQOS_MAC_GMIIDR_GD_MASK));

    osi_writela (
      osi_core,
      mac_gmiidr,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_MDIO_DATA
      );

    /* initiate the MII write operation by updating desired */
    /* phy address/id (0 - 31) */
    /* phy register offset */
    /* CSR Clock Range (20 - 35MHz) */
    /* Select write operation */
    /* set busy bit */
    mac_gmiiar = osi_readla (
                   osi_core,
                   (nveu8_t *)osi_core->base +
                   EQOS_MAC_MDIO_ADDRESS
                   );
    mac_gmiiar = (mac_gmiiar & (EQOS_MDIO_PHY_REG_SKAP |
                                EQOS_MDIO_PHY_REG_C45E));
    mac_gmiiar = (mac_gmiiar |
                  ((phyaddr) << EQOS_MDIO_PHY_ADDR_SHIFT) |
                  ((phyreg) << EQOS_MDIO_PHY_REG_SHIFT) |
                  ((osi_core->mdc_cr) << EQOS_MDIO_PHY_REG_CR_SHIF) |
                  (EQOS_MDIO_PHY_REG_WRITE) | EQOS_MAC_GMII_BUSY);
  }

  osi_writela (
    osi_core,
    mac_gmiiar,
    (nveu8_t *)osi_core->base +
    EQOS_MAC_MDIO_ADDRESS
    );
  /* wait for MII write operation to complete */
  ret = poll_for_mii_idle (osi_core);
fail:
  return ret;
}

/**
 * @brief eqos_read_phy_reg - Read from a PHY register through MAC over MDIO bus
 *
 * @note
 * Algorithm:
 *  -  Before proceeding for reading for PHY register check whether any MII
 *    operation going on MDIO bus by polling MAC_GMII_BUSY bit.
 *  - Populate required parameters like phy address, phy register etc,,
 *    in program it in MAC MDIO Address register. Read and GMII busy bits
 *    needs to be set in this operation.
 *  - Write into MAC MDIO address register poll for GMII busy for MDIO
 *    operation to complete. After this data will be available at MAC MDIO
 *    data register.
 *  - Refer to EQOS column of <<RM_03, (sequence diagram)>> for API details.
 *  - Traceid:ETHERNET_NVETHERNETRM_003
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be read from PHY.
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval data from PHY register on success
 * @retval -1 on failure
 */
static nve32_t
eqos_read_phy_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    phyreg
  )
{
  nveu32_t  mac_gmiiar;
  nveu32_t  mac_gmiidr;
  nveu32_t  data;
  nveu32_t  devaddr;
  nve32_t   ret = 0;

  /* Wait for any previous MII read/write operation to complete */
  ret = poll_for_mii_idle (osi_core);
  if (ret < 0) {
    /* poll_for_mii_idle fail */
    goto fail;
  }

  /* C45 register access */
  if ((phyreg & OSI_MII_ADDR_C45) == OSI_MII_ADDR_C45) {
    /* First write the register address */
    mac_gmiidr = (phyreg & EQOS_MDIO_DATA_REG_PHYREG_MASK) <<
                 EQOS_MDIO_DATA_REG_PHYREG_SHIFT;
    osi_writela (
      osi_core,
      mac_gmiidr,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_MDIO_DATA
      );

    /* Extract the device address from PHY register */
    devaddr = (phyreg >> EQOS_MDIO_DATA_REG_DEV_ADDR_SHIFT) &
              EQOS_MDIO_DATA_REG_DEV_ADDR_MASK;

    /* Initiate the clause 45 transaction with auto
     * generation of address packet.
     */
    mac_gmiiar = (EQOS_MDIO_PHY_REG_C45E |
                  ((phyaddr) << EQOS_MDIO_PHY_ADDR_SHIFT) |
                  (devaddr << EQOS_MDIO_PHY_REG_SHIFT) |
                  ((osi_core->mdc_cr) << EQOS_MDIO_PHY_REG_CR_SHIF) |
                  (EQOS_MDIO_PHY_REG_GOC_READ) | EQOS_MAC_GMII_BUSY);
  } else {
    mac_gmiiar = osi_readla (
                   osi_core,
                   (nveu8_t *)osi_core->base +
                   EQOS_MAC_MDIO_ADDRESS
                   );
    /* initiate the MII read operation by updating desired */
    /* phy address/id (0 - 31) */
    /* phy register offset */
    /* CSR Clock Range (20 - 35MHz) */
    /* Select read operation */
    /* set busy bit */
    mac_gmiiar = (mac_gmiiar & (EQOS_MDIO_PHY_REG_SKAP |
                                EQOS_MDIO_PHY_REG_C45E));
    mac_gmiiar = (mac_gmiiar |
                  ((phyaddr) << EQOS_MDIO_PHY_ADDR_SHIFT) |
                  ((phyreg) << EQOS_MDIO_PHY_REG_SHIFT) |
                  ((osi_core->mdc_cr) << EQOS_MDIO_PHY_REG_CR_SHIF) |
                  (EQOS_MDIO_PHY_REG_GOC_READ) | EQOS_MAC_GMII_BUSY);
  }

  osi_writela (
    osi_core,
    mac_gmiiar,
    (nveu8_t *)osi_core->base +
    EQOS_MAC_MDIO_ADDRESS
    );

  /* wait for MII write operation to complete */
  ret = poll_for_mii_idle (osi_core);
  if (ret < 0) {
    /* poll_for_mii_idle fail */
    goto fail;
  }

  mac_gmiidr = osi_readla (
                 osi_core,
                 (nveu8_t *)osi_core->base +
                 EQOS_MAC_MDIO_DATA
                 );
  data = (mac_gmiidr & EQOS_MAC_GMIIDR_GD_MASK);

  ret = (nve32_t)data;
fail:
  return ret;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief eqos_read_reg - Read a reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval data from register on success
 */
static nveu32_t
eqos_read_reg (
  struct osi_core_priv_data *const  osi_core,
  const nve32_t                     reg
  )
{
  return osi_readla (osi_core, (nveu8_t *)osi_core->base + reg);
}

/**
 * @brief eqos_write_reg - Write a reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] val:  Value to be written.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nveu32_t
eqos_write_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    val,
  const nve32_t                     reg
  )
{
  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + reg);
  return 0;
}

#endif

#if defined MACSEC_SUPPORT && !defined OSI_STRIPPED_LIB

/**
 * @brief eqos_read_macsec_reg - Read a MACSEC reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval data from register on success and 0xffffffff on failure
 */
static nveu32_t
eqos_read_macsec_reg (
  struct osi_core_priv_data *const  osi_core,
  const nve32_t                     reg
  )
{
  nveu32_t                 ret     = 0;
  const struct core_local  *l_core = (struct core_local *)(void *)osi_core;

  if (l_core->macsec_ops != OSI_NULL) {
    ret = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->macsec_base +
            reg
            );
  } else {
    /* macsec is not supported or not enabled in DT */
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "read reg failed",
      0ULL
      );
    ret = 0xffffffffU;
  }

  return ret;
}

/**
 * @brief eqos_write_macsec_reg - Write a MACSEC reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] val:  Value to be written.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0 on success or 0xffffffff on error
 */
static nveu32_t
eqos_write_macsec_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    val,
  const nve32_t                     reg
  )
{
  nveu32_t                 ret     = 0;
  const struct core_local  *l_core = (struct core_local *)(void *)osi_core;

  if (l_core->macsec_ops != OSI_NULL) {
    osi_writela (
      osi_core,
      val,
      (nveu8_t *)osi_core->macsec_base +
      reg
      );
  } else {
    /* macsec is not supported or not enabled in DT */
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "write reg failed",
      0ULL
      );
    ret = 0xffffffffU;
  }

  return ret;
}

#endif /*  MACSEC_SUPPORT */

#ifndef OSI_STRIPPED_LIB

/**
 * @brief eqos_disable_tx_lpi - Helper function to disable Tx LPI.
 *
 * @note
 * Algorithm:
 *  - Clear the bits to enable Tx LPI, Tx LPI automate, LPI Tx Timer and
 *    PHY Link status in the LPI control/status register
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MAC has to be out of reset, and clocks supplied.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void
eqos_disable_tx_lpi (
  struct osi_core_priv_data *const  osi_core
  )
{
  void      *addr   = osi_core->base;
  nveu32_t  lpi_csr = 0;

  /* Disable LPI control bits */
  lpi_csr = osi_readla (
              osi_core,
              (nveu8_t *)addr + EQOS_MAC_LPI_CSR
              );
  lpi_csr &= ~(EQOS_MAC_LPI_CSR_LPITE | EQOS_MAC_LPI_CSR_LPITXA |
               EQOS_MAC_LPI_CSR_PLS | EQOS_MAC_LPI_CSR_LPIEN);
  osi_writela (
    osi_core,
    lpi_csr,
    (nveu8_t *)addr + EQOS_MAC_LPI_CSR
    );
}

/**
 * @brief eqos_config_rx_crc_check - Configure CRC Checking for Rx Packets
 *
 * @note
 * Algorithm:
 *  - When this bit is set, the MAC receiver does not check the CRC
 *    field in the received packets. When this bit is reset, the MAC receiver
 *    always checks the CRC field in the received packets.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] crc_chk: Enable or disable checking of CRC field in received pkts
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_rx_crc_check (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    crc_chk
  )
{
  void      *addr = osi_core->base;
  nveu32_t  val;

  /* return on invalid argument */
  if ((crc_chk != OSI_ENABLE) && (crc_chk != OSI_DISABLE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "rx_crc: invalid input\n",
      0ULL
      );
    return -1;
  }

  /* Read MAC Extension Register */
  val = osi_readla (osi_core, (nveu8_t *)addr + EQOS_MAC_EXTR);

  /* crc_chk: 1 is for enable and 0 is for disable */
  if (crc_chk == OSI_ENABLE) {
    /* Enable Rx packets CRC check */
    val &= ~EQOS_MAC_EXTR_DCRCC;
  } else if (crc_chk == OSI_DISABLE) {
    /* Disable Rx packets CRC check */
    val |= EQOS_MAC_EXTR_DCRCC;
  } else {
    /* Nothing here */
  }

  /* Write to MAC Extension Register */
  osi_writela (osi_core, val, (nveu8_t *)addr + EQOS_MAC_EXTR);

  return 0;
}

/**
 * @brief eqos_config_tx_status - Configure MAC to forward the tx pkt status
 *
 * @note
 * Algorithm:
 *  - When DTXSTS bit is reset, the Tx packet status received
 *    from the MAC is forwarded to the application.
 *    When DTXSTS bit is set, the Tx packet status received from the MAC
 *    are dropped in MTL.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_status: Enable or Disable the forwarding of tx pkt status
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_tx_status (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    tx_status
  )
{
  void      *addr = osi_core->base;
  nveu32_t  val;

  /* don't allow if tx_status is other than 0 or 1 */
  if ((tx_status != OSI_ENABLE) && (tx_status != OSI_DISABLE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "tx_status: invalid input\n",
      0ULL
      );
    return -1;
  }

  /* Read MTL Operation Mode Register */
  val = osi_readla (osi_core, (nveu8_t *)addr + EQOS_MTL_OP_MODE);

  if (tx_status == OSI_ENABLE) {
    /* When DTXSTS bit is reset, the Tx packet status received
     * from the MAC are forwarded to the application.
     */
    val &= ~EQOS_MTL_OP_MODE_DTXSTS;
  } else if (tx_status == OSI_DISABLE) {
    /* When DTXSTS bit is set, the Tx packet status received from
     * the MAC are dropped in the MTL
     */
    val |= EQOS_MTL_OP_MODE_DTXSTS;
  } else {
    /* Nothing here */
  }

  /* Write to DTXSTS bit of MTL Operation Mode Register to enable or
   * disable the Tx packet status
   */
  osi_writela (osi_core, val, (nveu8_t *)addr + EQOS_MTL_OP_MODE);

  return 0;
}

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief eqos_set_avb_algorithm - Set TxQ/TC avb config
 *
 * @note
 * Algorithm:
 *  - Check if queue index is valid
 *  - Update operation mode of TxQ/TC
 *   - Set TxQ operation mode
 *   - Set Algo and Credit control
 *   - Set Send slope credit
 *   - Set Idle slope credit
 *   - Set Hi credit
 *   - Set low credit
 *  - Update register values
 *
 * @param[in] osi_core: OSI core priv data structure
 * @param[in] avb: structure having configuration for avb algorithm
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *  - osi_core->osd should be populated.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_set_avb_algorithm (
  struct osi_core_priv_data *const            osi_core,
  const struct osi_core_avb_algorithm *const  avb
  )
{
  nveu32_t  value;
  nve32_t   ret = -1;
  nveu32_t  qinx;

  /* queue index in range */
  if (avb->qindex >= OSI_EQOS_MAX_NUM_QUEUES) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Queue index\n",
      (nveul64_t)avb->qindex
      );
    goto done;
  }

  /* queue oper_mode in range check*/
  if (avb->oper_mode >= OSI_MTL_QUEUE_MODEMAX) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Queue mode\n",
      (nveul64_t)avb->qindex
      );
    goto done;
  }

  /* Validate algo is valid  */
  if (avb->algo > OSI_MTL_TXQ_AVALG_CBS) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Algo input\n",
      (nveul64_t)avb->algo
      );
    goto done;
  }

  /* can't set AVB mode for queue 0 */
  if ((avb->qindex == 0U) && (avb->oper_mode == OSI_MTL_QUEUE_AVB)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OPNOTSUPP,
      "Not allowed to set AVB for Q0\n",
      (nveul64_t)avb->qindex
      );
    goto done;
  }

  /* Check for CC */
  if (avb->credit_control > OSI_ENABLE) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid credit control\n",
      (nveul64_t)avb->credit_control
      );
    goto done;
  }

  qinx  = avb->qindex;
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_CHX_TX_OP_MODE (qinx)
            );
  value &= ~EQOS_MTL_TXQEN_MASK;
  /* Set TxQ/TC mode as per input struct after masking 3 bit */
  value |= (avb->oper_mode << EQOS_MTL_TXQEN_MASK_SHIFT) &
           EQOS_MTL_TXQEN_MASK;
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MTL_CHX_TX_OP_MODE (qinx));

  /* Set Algo and Credit control */
  value = OSI_DISABLE;
  if (avb->algo == OSI_MTL_TXQ_AVALG_CBS) {
    value = (avb->credit_control << EQOS_MTL_TXQ_ETS_CR_CC_SHIFT) &
            EQOS_MTL_TXQ_ETS_CR_CC;
  }

  value |= (avb->algo << EQOS_MTL_TXQ_ETS_CR_AVALG_SHIFT) &
           EQOS_MTL_TXQ_ETS_CR_AVALG;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    EQOS_MTL_TXQ_ETS_CR (qinx)
    );

  if (avb->algo == OSI_MTL_TXQ_AVALG_CBS) {
    /* Set Send slope credit */
    value = avb->send_slope & EQOS_MTL_TXQ_ETS_SSCR_SSC_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      EQOS_MTL_TXQ_ETS_SSCR (qinx)
      );

    /* Set Idle slope credit*/
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MTL_TXQ_QW (qinx)
              );
    value &= ~EQOS_MTL_TXQ_ETS_QW_ISCQW_MASK;
    value |= avb->idle_slope & EQOS_MTL_TXQ_ETS_QW_ISCQW_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      EQOS_MTL_TXQ_QW (qinx)
      );

    /* Set Hi credit */
    value = avb->hi_credit & EQOS_MTL_TXQ_ETS_HCR_HC_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      EQOS_MTL_TXQ_ETS_HCR (qinx)
      );

    /* low credit  is -ve number, osi_write need a nveu32_t
     * take only 28:0 bits from avb->low_credit
     */
    value = avb->low_credit & EQOS_MTL_TXQ_ETS_LCR_LC_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      EQOS_MTL_TXQ_ETS_LCR (qinx)
      );
  } else {
    /* Reset register values to POR/initialized values */
    osi_writela (
      osi_core,
      OSI_DISABLE,
      (nveu8_t *)osi_core->base +
      EQOS_MTL_TXQ_ETS_SSCR (qinx)
      );

    osi_writela (
      osi_core,
      EQOS_MTL_TXQ_QW_ISCQW,
      (nveu8_t *)osi_core->base + EQOS_MTL_TXQ_QW (qinx)
      );

    osi_writela (
      osi_core,
      OSI_DISABLE,
      (nveu8_t *)osi_core->base +
      EQOS_MTL_TXQ_ETS_HCR (qinx)
      );

    osi_writela (
      osi_core,
      OSI_DISABLE,
      (nveu8_t *)osi_core->base +
      EQOS_MTL_TXQ_ETS_LCR (qinx)
      );
  }

  ret = 0;
done:
  return ret;
}

/**
 * @brief eqos_get_avb_algorithm - Get TxQ/TC avb config
 *
 * @note
 * Algorithm:
 *  - Check if queue index is valid
 *  - read operation mode of TxQ/TC
 *   - read TxQ operation mode
 *   - read Algo and Credit control
 *   - read Send slope credit
 *   - read Idle slope credit
 *   - read Hi credit
 *   - read low credit
 *  - updated pointer
 *
 * @param[in] osi_core: OSI core priv data structure
 * @param[out] avb: structure pointer having configuration for avb algorithm
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *  - osi_core->osd should be populated.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_get_avb_algorithm (
  struct osi_core_priv_data *const      osi_core,
  struct osi_core_avb_algorithm *const  avb
  )
{
  nveu32_t  value;
  nve32_t   ret  = -1;
  nveu32_t  qinx = 0U;

  if ((avb->qindex >= OSI_EQOS_MAX_NUM_QUEUES) ||
      (avb->qindex == OSI_NONE))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Queue index\n",
      (nveul64_t)avb->qindex
      );
    goto done;
  }

  qinx  = avb->qindex;
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_CHX_TX_OP_MODE (qinx)
            );

  /* Get TxQ/TC mode as per input struct after masking 3:2 bit */
  value          = (value & EQOS_MTL_TXQEN_MASK) >> EQOS_MTL_TXQEN_MASK_SHIFT;
  avb->oper_mode = value;

  /* Get Algo and Credit control */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_TXQ_ETS_CR (qinx)
            );
  avb->credit_control = (value & EQOS_MTL_TXQ_ETS_CR_CC) >>
                        EQOS_MTL_TXQ_ETS_CR_CC_SHIFT;
  avb->algo = (value & EQOS_MTL_TXQ_ETS_CR_AVALG) >>
              EQOS_MTL_TXQ_ETS_CR_AVALG_SHIFT;

  /* Get Send slope credit */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_TXQ_ETS_SSCR (qinx)
            );
  avb->send_slope = value & EQOS_MTL_TXQ_ETS_SSCR_SSC_MASK;

  /* Get Idle slope credit*/
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_TXQ_QW (qinx)
            );
  avb->idle_slope = value & EQOS_MTL_TXQ_ETS_QW_ISCQW_MASK;

  /* Get Hi credit */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_TXQ_ETS_HCR (qinx)
            );
  avb->hi_credit = value & EQOS_MTL_TXQ_ETS_HCR_HC_MASK;

  /* Get Low credit for which bit 31:29 are unknown
   * return 28:0 valid bits to application
   */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            EQOS_MTL_TXQ_ETS_LCR (qinx)
            );
  avb->low_credit = value & EQOS_MTL_TXQ_ETS_LCR_LC_MASK;

  ret = 0;

done:
  return ret;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief eqos_config_arp_offload - Enable/Disable ARP offload
 *
 * @note
 * Algorithm:
 *  - Read the MAC configuration register
 *  - If ARP offload is to be enabled, program the IP address in
 *    ARPPA register
 *  - Enable/disable the ARPEN bit in MCR and write back to the MCR.
 *
 * @param[in] osi_core: OSI core priv data structure
 * @param[in] enable: Flag variable to enable/disable ARP offload
 * @param[in] ip_addr: IP address of device to be programmed in HW.
 *        HW will use this IP address to respond to ARP requests.
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *  - Valid 4 byte IP address as argument ip_addr
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_arp_offload (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable,
  const nveu8_t                     *ip_addr
  )
{
  void      *addr   = osi_core->base;
  nveu32_t  mac_ver = osi_core->mac_ver;
  nveu32_t  mac_mcr;
  nveu32_t  val;

  mac_mcr = osi_readla (osi_core, (nveu8_t *)addr + EQOS_MAC_MCR);

  if (enable == OSI_ENABLE) {
    val = (((nveu32_t)ip_addr[0]) << 24) |
          (((nveu32_t)ip_addr[1]) << 16) |
          (((nveu32_t)ip_addr[2]) << 8) |
          (((nveu32_t)ip_addr[3]));

    if (mac_ver == OSI_EQOS_MAC_4_10) {
      osi_writela (
        osi_core,
        val,
        (nveu8_t *)addr +
        EQOS_4_10_MAC_ARPPA
        );
    } else if (mac_ver == OSI_EQOS_MAC_5_00) {
      osi_writela (
        osi_core,
        val,
        (nveu8_t *)addr +
        EQOS_5_00_MAC_ARPPA
        );
    } else {
      /* Unsupported MAC ver */
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "arp_offload: invalid HW\n",
        0ULL
        );
      return -1;
    }

    mac_mcr |= EQOS_MCR_ARPEN;
  } else {
    mac_mcr &= ~EQOS_MCR_ARPEN;
  }

  osi_writela (osi_core, mac_mcr, (nveu8_t *)addr + EQOS_MAC_MCR);

  return 0;
}

/**
 * @brief eqos_config_vlan_filter_reg - config vlan filter register
 *
 * @note
 * Algorithm:
 *  - This sequence is used to enable/disable VLAN filtering and
 *    also selects VLAN filtering mode- perfect/hash
 *
 * @param[in] osi_core: OSI core priv data structure
 * @param[in] filter_enb_dis: vlan filter enable/disable
 * @param[in] perfect_hash_filtering: perfect or hash filter
 * @param[in] perfect_inverse_match: normal or inverse filter
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *  - osi_core->osd should be populated.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_vlan_filtering (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    filter_enb_dis,
  const nveu32_t                    perfect_hash_filtering,
  const nveu32_t                    perfect_inverse_match
  )
{
  nveu32_t  value;
  void      *base = osi_core->base;

  if ((filter_enb_dis != OSI_ENABLE) &&
      (filter_enb_dis != OSI_DISABLE))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "vlan_filter: invalid input\n",
      0ULL
      );
    return -1;
  }

  if ((perfect_hash_filtering != OSI_ENABLE) &&
      (perfect_hash_filtering != OSI_DISABLE))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "vlan_filter: invalid input\n",
      0ULL
      );
    return -1;
  }

  if ((perfect_inverse_match != OSI_ENABLE) &&
      (perfect_inverse_match != OSI_DISABLE))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "vlan_filter: invalid input\n",
      0ULL
      );
    return -1;
  }

  value  = osi_readla (osi_core, (nveu8_t *)base + EQOS_MAC_PFR);
  value &= ~(EQOS_MAC_PFR_VTFE);
  value |= ((filter_enb_dis << EQOS_MAC_PFR_SHIFT) & EQOS_MAC_PFR_VTFE);
  osi_writela (osi_core, value, (nveu8_t *)base + EQOS_MAC_PFR);

  value  = osi_readla (osi_core, (nveu8_t *)base + EQOS_MAC_VLAN_TR);
  value &= ~(EQOS_MAC_VLAN_TR_VTIM | EQOS_MAC_VLAN_TR_VTHM);
  value |= ((perfect_inverse_match << EQOS_MAC_VLAN_TR_VTIM_SHIFT) &
            EQOS_MAC_VLAN_TR_VTIM);
  if (perfect_hash_filtering == OSI_HASH_FILTER_MODE) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OPNOTSUPP,
      "VLAN hash filter is not supported, no update of VTHM\n",
      0ULL
      );
  }

  osi_writela (osi_core, value, (nveu8_t *)base + EQOS_MAC_VLAN_TR);
  return 0;
}

/**
 * @brief eqos_configure_eee - Configure the EEE LPI mode
 *
 * @note
 * Algorithm:
 *  - This routine configures EEE LPI mode in the MAC.
 *  - The time (in microsecond) to wait before resuming transmission after
 *    exiting from LPI,
 *  - The time (in millisecond) to wait before LPI pattern can be
 *    transmitted after PHY link is up) are not configurable. Default values
 *    are used in this routine.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_lpi_enabled: enable (1)/disable (0) flag for Tx lpi
 * @param[in] tx_lpi_timer: Tx LPI entry timer in usec. Valid upto
 *            OSI_MAX_TX_LPI_TIMER in steps of 8usec.
 *
 * @pre
 *  - Required clks and resets has to be enabled.
 *  - MAC/PHY should be initialized
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 */
static void
eqos_configure_eee (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    tx_lpi_enabled,
  const nveu32_t                    tx_lpi_timer
  )
{
  nveu32_t  lpi_csr             = 0;
  nveu32_t  lpi_timer_ctrl      = 0;
  nveu32_t  lpi_entry_timer     = 0;
  nveu32_t  lpi_1US_tic_counter = OSI_LPI_1US_TIC_COUNTER_DEFAULT;
  nveu8_t   *addr               =  (nveu8_t *)osi_core->base;

  if (osi_core->mac_ver == OSI_EQOS_MAC_5_40) {
    if (xpcs_eee (osi_core, tx_lpi_enabled) != 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "eqos xpcs_eee call failed\n",
        0ULL
        );
      return;
    }
  }

  if (tx_lpi_enabled != OSI_DISABLE) {
    /* Configure the following timers.
     * 1. LPI LS timer - minimum time (in milliseconds) for
     * which the link status from PHY should be up before
     * the LPI pattern can be transmitted to the PHY.
     * Default 1sec
     * 2. LPI TW timer - minimum time (in microseconds) for
     * which MAC waits after it stops transmitting LPI
     * pattern before resuming normal tx. Default 21us
     * 3. LPI entry timer - Time in microseconds that MAC
     * will wait to enter LPI mode after all tx is complete.
     * Default 1sec
     */
    lpi_timer_ctrl |= ((OSI_DEFAULT_LPI_LS_TIMER &
                        OSI_LPI_LS_TIMER_MASK) <<
                       OSI_LPI_LS_TIMER_SHIFT);
    lpi_timer_ctrl |= (OSI_DEFAULT_LPI_TW_TIMER &
                       OSI_LPI_TW_TIMER_MASK);
    osi_writela (
      osi_core,
      lpi_timer_ctrl,
      addr + EQOS_MAC_LPI_TIMER_CTRL
      );

    lpi_entry_timer |= (tx_lpi_timer &
                        OSI_LPI_ENTRY_TIMER_MASK);
    osi_writela (
      osi_core,
      lpi_entry_timer,
      addr + EQOS_MAC_LPI_EN_TIMER
      );

    /* Program the MAC_1US_Tic_Counter as per the frequency of the
     * clock used for accessing the CSR slave
     */
    /* fix cert error */
    if (osi_core->csr_clk_speed > 1U) {
      lpi_1US_tic_counter = ((osi_core->csr_clk_speed - 1U) &
                             OSI_LPI_1US_TIC_COUNTER_MASK);
    }

    osi_writela (
      osi_core,
      lpi_1US_tic_counter,
      addr + EQOS_MAC_1US_TIC_CNTR
      );

    /* Set LPI timer enable and LPI Tx automate, so that MAC
     * can enter/exit Tx LPI on its own using timers above.
     * Set LPI Enable & PHY links status (PLS) up.
     */
    lpi_csr  = osi_readla (osi_core, addr + EQOS_MAC_LPI_CSR);
    lpi_csr |= (EQOS_MAC_LPI_CSR_LPITE |
                EQOS_MAC_LPI_CSR_LPITXA |
                EQOS_MAC_LPI_CSR_PLS |
                EQOS_MAC_LPI_CSR_LPIEN);
    osi_writela (osi_core, lpi_csr, addr + EQOS_MAC_LPI_CSR);
  } else {
    /* Disable LPI control bits */
    eqos_disable_tx_lpi (osi_core);
  }
}

/**
 * @brief eqos_config_mac_loopback - Configure MAC to support loopback
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lb_mode: Enable or Disable MAC loopback mode
 *
 * @pre MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
eqos_config_mac_loopback (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    lb_mode
  )
{
  nveu32_t  clk_ctrl_val;
  nveu32_t  mcr_val;
  void      *addr = osi_core->base;

  /* Read MAC Configuration Register */
  mcr_val = osi_readla (osi_core, (nveu8_t *)addr + EQOS_MAC_MCR);

  /* Read EQOS wrapper clock control 0 register */
  clk_ctrl_val = osi_readla (
                   osi_core,
                   (nveu8_t *)addr + EQOS_CLOCK_CTRL_0
                   );

  if (lb_mode == OSI_ENABLE) {
    /* Enable Loopback Mode */
    mcr_val |= EQOS_MAC_ENABLE_LM;
    /* Enable RX_CLK_SEL so that TX Clock is fed to RX domain */
    clk_ctrl_val |= EQOS_RX_CLK_SEL;
  } else if (lb_mode == OSI_DISABLE) {
    /* Disable Loopback Mode */
    mcr_val &= ~EQOS_MAC_ENABLE_LM;
    /* Disable RX_CLK_SEL so that TX Clock is fed to RX domain */
    clk_ctrl_val &= ~EQOS_RX_CLK_SEL;
  } else {
    /* Nothing here */
  }

  /* Write to EQOS wrapper clock control 0 register */
  osi_writela (
    osi_core,
    clk_ctrl_val,
    (nveu8_t *)addr + EQOS_CLOCK_CTRL_0
    );

  /* Write to MAC Configuration Register */
  osi_writela (osi_core, mcr_val, (nveu8_t *)addr + EQOS_MAC_MCR);

  return 0;
}

#endif /* !OSI_STRIPPED_LIB */

static void
eqos_get_hw_features (
  struct osi_core_priv_data *const  osi_core,
  struct osi_hw_features            *hw_feat
  )
{
  nveu32_t  mac_hfr0 = 0;
  nveu32_t  mac_hfr1 = 0;
  nveu32_t  mac_hfr2 = 0;
  nveu32_t  mac_hfr3 = 0;

  mac_hfr0 = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_HFR0);
  mac_hfr1 = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_HFR1);
  mac_hfr2 = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_HFR2);
  mac_hfr3 = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_HFR3);

  hw_feat->mii_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_MIISEL_SHIFT) &
                      EQOS_MAC_HFR0_MIISEL_MASK);
  hw_feat->gmii_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_GMIISEL_SHIFT) &
                       EQOS_MAC_HFR0_GMIISEL_MASK);
  hw_feat->hd_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_HDSEL_SHIFT) &
                     EQOS_MAC_HFR0_HDSEL_MASK);
  hw_feat->pcs_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_PCSSEL_SHIFT) &
                      EQOS_MAC_HFR0_PCSSEL_MASK);
  hw_feat->vlan_hash_en = ((mac_hfr0 >> EQOS_MAC_HFR0_VLHASH_SHIFT) &
                           EQOS_MAC_HFR0_VLHASH_MASK);
  hw_feat->sma_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_SMASEL_SHIFT) &
                      EQOS_MAC_HFR0_SMASEL_MASK);
  hw_feat->rwk_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_RWKSEL_SHIFT) &
                      EQOS_MAC_HFR0_RWKSEL_MASK);
  hw_feat->mgk_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_MGKSEL_SHIFT) &
                      EQOS_MAC_HFR0_MGKSEL_MASK);
  hw_feat->mmc_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_MMCSEL_SHIFT) &
                      EQOS_MAC_HFR0_MMCSEL_MASK);
  hw_feat->arp_offld_en = ((mac_hfr0 >> EQOS_MAC_HFR0_ARPOFFLDEN_SHIFT) &
                           EQOS_MAC_HFR0_ARPOFFLDEN_MASK);
  hw_feat->ts_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_TSSSEL_SHIFT) &
                     EQOS_MAC_HFR0_TSSSEL_MASK);
  hw_feat->eee_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_EEESEL_SHIFT) &
                      EQOS_MAC_HFR0_EEESEL_MASK);
  hw_feat->tx_coe_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_TXCOESEL_SHIFT) &
                         EQOS_MAC_HFR0_TXCOESEL_MASK);
  hw_feat->rx_coe_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_RXCOE_SHIFT) &
                         EQOS_MAC_HFR0_RXCOE_MASK);
  hw_feat->mac_addr_sel =
    ((mac_hfr0 >> EQOS_MAC_HFR0_ADDMACADRSEL_SHIFT) &
     EQOS_MAC_HFR0_ADDMACADRSEL_MASK);
  hw_feat->mac_addr32_sel =
    ((mac_hfr0 >> EQOS_MAC_HFR0_MACADR32SEL_SHIFT) &
     EQOS_MAC_HFR0_MACADR32SEL_MASK);
  hw_feat->mac_addr64_sel =
    ((mac_hfr0 >> EQOS_MAC_HFR0_MACADR64SEL_SHIFT) &
     EQOS_MAC_HFR0_MACADR64SEL_MASK);
  hw_feat->tsstssel = ((mac_hfr0 >> EQOS_MAC_HFR0_TSINTSEL_SHIFT) &
                       EQOS_MAC_HFR0_TSINTSEL_MASK);
  hw_feat->sa_vlan_ins = ((mac_hfr0 >> EQOS_MAC_HFR0_SAVLANINS_SHIFT) &
                          EQOS_MAC_HFR0_SAVLANINS_MASK);
  hw_feat->act_phy_sel = ((mac_hfr0 >> EQOS_MAC_HFR0_ACTPHYSEL_SHIFT) &
                          EQOS_MAC_HFR0_ACTPHYSEL_MASK);
  hw_feat->rx_fifo_size = ((mac_hfr1 >> EQOS_MAC_HFR1_RXFIFOSIZE_SHIFT) &
                           EQOS_MAC_HFR1_RXFIFOSIZE_MASK);
  hw_feat->tx_fifo_size = ((mac_hfr1 >> EQOS_MAC_HFR1_TXFIFOSIZE_SHIFT) &
                           EQOS_MAC_HFR1_TXFIFOSIZE_MASK);
  hw_feat->ost_en = ((mac_hfr1 >> EQOS_MAC_HFR1_OSTEN_SHIFT) &
                     EQOS_MAC_HFR1_OSTEN_MASK);
  hw_feat->pto_en = ((mac_hfr1 >> EQOS_MAC_HFR1_PTOEN_SHIFT) &
                     EQOS_MAC_HFR1_PTOEN_MASK);
  hw_feat->adv_ts_hword = ((mac_hfr1 >> EQOS_MAC_HFR1_ADVTHWORD_SHIFT) &
                           EQOS_MAC_HFR1_ADVTHWORD_MASK);
  hw_feat->addr_64 = ((mac_hfr1 >> EQOS_MAC_HFR1_ADDR64_SHIFT) &
                      EQOS_MAC_HFR1_ADDR64_MASK);
  hw_feat->dcb_en = ((mac_hfr1 >> EQOS_MAC_HFR1_DCBEN_SHIFT) &
                     EQOS_MAC_HFR1_DCBEN_MASK);
  hw_feat->sph_en = ((mac_hfr1 >> EQOS_MAC_HFR1_SPHEN_SHIFT) &
                     EQOS_MAC_HFR1_SPHEN_MASK);
  hw_feat->tso_en = ((mac_hfr1 >> EQOS_MAC_HFR1_TSOEN_SHIFT) &
                     EQOS_MAC_HFR1_TSOEN_MASK);
  hw_feat->dma_debug_gen =
    ((mac_hfr1 >> EQOS_MAC_HFR1_DMADEBUGEN_SHIFT) &
     EQOS_MAC_HFR1_DMADEBUGEN_MASK);
  hw_feat->av_sel = ((mac_hfr1 >> EQOS_MAC_HFR1_AVSEL_SHIFT) &
                     EQOS_MAC_HFR1_AVSEL_MASK);
  hw_feat->rav_sel = ((mac_hfr1 >> EQOS_MAC_HFR1_RAVSEL_SHIFT) &
                      EQOS_MAC_HFR1_RAVSEL_MASK);
  hw_feat->ost_over_udp = ((mac_hfr1 >> EQOS_MAC_HFR1_POUOST_SHIFT) &
                           EQOS_MAC_HFR1_POUOST_MASK);
  hw_feat->hash_tbl_sz = ((mac_hfr1 >> EQOS_MAC_HFR1_HASHTBLSZ_SHIFT) &
                          EQOS_MAC_HFR1_HASHTBLSZ_MASK);
  hw_feat->l3l4_filter_num =
    ((mac_hfr1 >> EQOS_MAC_HFR1_L3L4FILTERNUM_SHIFT) &
     EQOS_MAC_HFR1_L3L4FILTERNUM_MASK);
  hw_feat->rx_q_cnt = ((mac_hfr2 >> EQOS_MAC_HFR2_RXQCNT_SHIFT) &
                       EQOS_MAC_HFR2_RXQCNT_MASK);
  hw_feat->tx_q_cnt = ((mac_hfr2 >> EQOS_MAC_HFR2_TXQCNT_SHIFT) &
                       EQOS_MAC_HFR2_TXQCNT_MASK);
  hw_feat->rx_ch_cnt = ((mac_hfr2 >> EQOS_MAC_HFR2_RXCHCNT_SHIFT) &
                        EQOS_MAC_HFR2_RXCHCNT_MASK);
  hw_feat->tx_ch_cnt = ((mac_hfr2 >> EQOS_MAC_HFR2_TXCHCNT_SHIFT) &
                        EQOS_MAC_HFR2_TXCHCNT_MASK);
  hw_feat->pps_out_num = ((mac_hfr2 >> EQOS_MAC_HFR2_PPSOUTNUM_SHIFT) &
                          EQOS_MAC_HFR2_PPSOUTNUM_MASK);
  hw_feat->aux_snap_num = ((mac_hfr2 >> EQOS_MAC_HFR2_AUXSNAPNUM_SHIFT) &
                           EQOS_MAC_HFR2_AUXSNAPNUM_MASK);
  hw_feat->num_vlan_filters = ((mac_hfr3 >> EQOS_MAC_HFR3_NRVF_SHIFT) &
                               EQOS_MAC_HFR3_NRVF_MASK);
  hw_feat->cbti_sel = ((mac_hfr3 >> EQOS_MAC_HFR3_CBTISEL_SHIFT) &
                       EQOS_MAC_HFR3_CBTISEL_MASK);
  hw_feat->double_vlan_en = ((mac_hfr3 >> EQOS_MAC_HFR3_DVLAN_SHIFT) &
                             EQOS_MAC_HFR3_DVLAN_MASK);
  /* TODO: packet duplication feature */
  hw_feat->frp_sel = ((mac_hfr3 >> EQOS_MAC_HFR3_FRPSEL_SHIFT) &
                      EQOS_MAC_HFR3_FRPSEL_MASK);
  hw_feat->max_frp_bytes = ((mac_hfr3 >> EQOS_MAC_HFR3_FRPPB_SHIFT) &
                            EQOS_MAC_HFR3_FRPPB_MASK);
  hw_feat->max_frp_entries = ((mac_hfr3 >> EQOS_MAC_HFR3_FRPES_SHIFT) &
                              EQOS_MAC_HFR3_FRPES_MASK);
  hw_feat->est_sel = ((mac_hfr3 >> EQOS_MAC_HFR3_ESTSEL_SHIFT) &
                      EQOS_MAC_HFR3_ESTSEL_MASK);
  hw_feat->gcl_depth = ((mac_hfr3 >> EQOS_MAC_HFR3_GCLDEP_SHIFT) &
                        EQOS_MAC_HFR3_GCLDEP_MASK);
  hw_feat->gcl_width = ((mac_hfr3 >> EQOS_MAC_HFR3_GCLWID_SHIFT) &
                        EQOS_MAC_HFR3_GCLWID_MASK);
  hw_feat->fpe_sel = ((mac_hfr3 >> EQOS_MAC_HFR3_FPESEL_SHIFT) &
                      EQOS_MAC_HFR3_FPESEL_MASK);
  hw_feat->tbs_sel = ((mac_hfr3 >> EQOS_MAC_HFR3_TBSSEL_SHIFT) &
                      EQOS_MAC_HFR3_TBSSEL_MASK);
  hw_feat->auto_safety_pkg = ((mac_hfr3 >> EQOS_MAC_HFR3_ASP_SHIFT) &
                              EQOS_MAC_HFR3_ASP_MASK);
}

/**
 * @brief poll_for_mac_tx_rx_idle - check mac tx/rx idle or not
 *
 * Algorithm: Check MAC tx/rx engines are idle.
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 * @pre
 *   - MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t
poll_for_mac_tx_rx_idle (
  struct osi_core_priv_data  *osi_core
  )
{
  nve32_t   ret   = 0;
  nveu32_t  retry = 0;
  nveu32_t  mac_debug;

  while (retry < OSI_TXRX_IDLE_RETRY) {
    mac_debug = osi_readla (
                  osi_core,
                  (nveu8_t *)osi_core->base +
                  EQOS_MAC_DEBUG
                  );
    if (((mac_debug & EQOS_MAC_DEBUG_RPESTS) !=
         EQOS_MAC_DEBUG_RPESTS) &&
        ((mac_debug & EQOS_MAC_DEBUG_TPESTS) !=
         EQOS_MAC_DEBUG_TPESTS))
    {
      break;
    }

    /* wait */
    osi_core->osd_ops.usleep (OSI_DELAY_COUNT);
    retry++;
  }

  if (retry >= OSI_TXRX_IDLE_RETRY) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "RGMII idle timed out\n",
      mac_debug
      );
    ret = -1;
  }

  return ret;
}

/**
 * @brief eqos_pre_pad_calibrate -  pre PAD calibration
 *
 * Algorithm:
 *      - Disable interrupt RGSMIIIE bit in MAC_Interrupt_Enable register
 *      - Stop MAC
 *      - Check MDIO interface idle and ensure no read/write to MDIO interface
 *      - Wait for MII idle (Ensure MAC_Debug register value is 0)
 *      - Disable RGMII Rx traffic by disabling padctl pins
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre
 *    - MAC should out of reset and clocks enabled.
 *
 * @retval 0 on success
 * @retval negative value on failure.
 */
static nve32_t
eqos_pre_pad_calibrate (
  struct osi_core_priv_data *const  osi_core
  )
{
  nve32_t   ret = 0;
  nveu32_t  value;

  /* Disable MAC RGSMIIIE - RGMII/SMII interrupts */
  /* Read MAC IMR Register */
  value  = osi_readla (osi_core, (nveu8_t *)osi_core->base + EQOS_MAC_IMR);
  value &= ~(EQOS_IMR_RGSMIIIE);
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_IMR);
  hw_stop_mac (osi_core);
  ret = poll_for_mii_idle (osi_core);
  if (ret < 0) {
    goto error;
  }

  ret = poll_for_mac_tx_rx_idle (osi_core);
  if (ret < 0) {
    goto error;
  }

  ret = osi_core->osd_ops.padctrl_mii_rx_pins (osi_core->osd, OSI_DISABLE);
  if (ret < 0) {
    goto error;
  }

  goto done;

error:
  /* roll back on fail */
  hw_start_mac (osi_core);
  (void)osi_core->osd_ops.padctrl_mii_rx_pins (osi_core->osd, OSI_ENABLE);

  /* Enable MAC RGSMIIIE - RGMII/SMII interrupts */
  /* Read MAC IMR Register */
  value  = osi_readl ((nveu8_t *)osi_core->base + EQOS_MAC_IMR);
  value |= EQOS_IMR_RGSMIIIE;
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MAC_IMR);

done:
  return ret;
}

/**
 * @brief eqos_post_pad_calibrate - post PAD calibration
 *
 * Algorithm:
 *      - Enable RGMII Rx traffic by enabling padctl pins
 *      - Read MAC_PHY_Control_Status register to clear any pending
 *          interrupts due to enable rx pad pins
 *      - start MAC
 *      - Enable interrupt RGSMIIIE bit in MAC_Interrupt_Enable register
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre
 *   - MAC should out of reset and clocks enabled.
 *
 * @retval 0 on success
 * @retval negative value on failure.
 */
static nve32_t
eqos_post_pad_calibrate (
  struct osi_core_priv_data *const  osi_core
  )
{
  nve32_t   ret     = 0;
  nveu32_t  mac_imr = 0;
  nveu32_t  mac_pcs = 0;
  nveu32_t  mac_isr = 0;

  ret = osi_core->osd_ops.padctrl_mii_rx_pins (osi_core->osd, OSI_ENABLE);

  /* handle only those MAC interrupts which are enabled */
  mac_imr = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MAC_IMR
              );
  mac_isr = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MAC_ISR
              );
  /* RGMII/SMII interrupt disabled in eqos_pre_pad_calibrate */
  if (((mac_isr & EQOS_MAC_ISR_RGSMIIS) == EQOS_MAC_ISR_RGSMIIS) &&
      ((mac_imr & EQOS_MAC_ISR_RGSMIIS) == OSI_DISABLE))
  {
    /* clear RGSMIIIE pending interrupt status due to pad enable */
    mac_pcs = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                EQOS_MAC_PCS
                );
    if (mac_pcs != 0U) {
      /* do nothing */
    }
  }

  hw_start_mac (osi_core);
  /* Enable MAC RGSMIIIE - RGMII/SMII interrupts */
  mac_imr |= EQOS_IMR_RGSMIIIE;
  osi_writela (osi_core, mac_imr, (nveu8_t *)osi_core->base + EQOS_MAC_IMR);
  return ret;
}

/**
 * @brief eqos_free_rchlist_index - Free index.
 *
 * Algorithm: This function just free the Receive channel index.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] rch_idx: Receive channel index.
 *
 */
static void
eqos_free_rchlist_index (
  struct osi_core_priv_data  *osi_core,
  const nve32_t              rch_idx
  )
{
  (void)osi_core;
  (void)rch_idx;
  OSI_CORE_ERR (
    osi_core->osd,
    OSI_LOG_ARG_HW_FAIL,
    "Free RCHLIST not supported by EQOS\n",
    0ULL
    );
}

/**
 * @brief eqos_rchlist_get_index - find free index
 *
 * Algorithm: This function gets free index for receive channel list.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] mac_addr: Mac address.
 *
 * @retval -1 on failure.

**/
static nve32_t
eqos_get_rchlist_index (
  struct osi_core_priv_data  *osi_core,
  nveu8_t const              *mac_addr
  )
{
  (void)osi_core;
  (void)mac_addr;
  OSI_CORE_ERR (
    osi_core->osd,
    OSI_LOG_ARG_HW_FAIL,
    "RCHLIST not supported by EQOS\n",
    0ULL
    );
  return -1;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief eqos_config_rss - Configure RSS
 *
 * Algorithm: Programes RSS hash table or RSS hash key.
 *
 * @param[in] osi_core: OSI core private data.
 *
 * @retval -1 Always
 */
static nve32_t
eqos_config_rss (
  struct osi_core_priv_data  *osi_core,
  const struct osi_core_rss  *rss
  )
{
  (void)osi_core;
  (void)rss;
  OSI_CORE_ERR (
    osi_core->osd,
    OSI_LOG_ARG_HW_FAIL,
    "RSS not supported by EQOS\n",
    0ULL
    );

  return -1;
}

/**
 * @brief eqos_get_rss - Get RSS
 *
 * Algorithm: Get  RSS hash table or RSS hash key.
 *
 * @param[out] osi_core: OSI core private data.
 *
 * @retval -1 Always
 */
static nve32_t
eqos_get_rss (
  struct osi_core_priv_data *const  osi_core,
  struct osi_core_rss *const        rss
  )
{
  (void)osi_core;
  (void)rss;
  OSI_CORE_ERR (
    osi_core->osd,
    OSI_LOG_ARG_HW_FAIL,
    "Get RSS not supported by EQOS\n",
    0ULL
    );

  return -1;
}

#endif /* !OSI_STRIPPED_LIB */

#if defined (MACSEC_SUPPORT)

/**
 * @brief eqos_config_for_macsec - Configure MAC according to macsec IAS
 *
 * @note
 * Algorithm:
 *  - Stop MAC Tx
 *  - Update MAC IPG value to accommodate macsec 32 byte SECTAG.
 *  - Start MAC Tx
 *  - Update MTL_EST value as MACSEC is enabled/disabled
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] enable: enable/disable macsec ipg value in mac
 *
 * @pre
 *     1) MAC has to be out of reset.
 *     2) Shall not use this ipg value in half duplex mode
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void
eqos_config_for_macsec (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable
  )
{
  nveu32_t  value = 0U, temp = 0U;

  /* stop MAC Tx */
  eqos_config_mac_tx (osi_core, OSI_DISABLE);
  if (enable == OSI_ENABLE) {
    /* Configure IPG  {EIPG,IPG} value according to macsec IAS in
     * MAC_Configuration and MAC_Extended_Configuration
     * IPG (12 B[default] + 32 B[sectag]) = 352 bits
     */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MAC_MCR
              );
    temp   = EQOS_MCR_IPG;
    temp   = temp << EQOS_MCR_IPG_SHIFT;
    value |= temp & EQOS_MCR_IPG_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_MCR
      );
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MAC_EXTR
              );
    value |= EQOS_MAC_EXTR_EIPGEN;
    temp   = EQOS_MAC_EXTR_EIPG;
    temp   = temp << EQOS_MAC_EXTR_EIPG_SHIFT;
    value |= temp & EQOS_MAC_EXTR_EIPG_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_EXTR
      );
  } else {
    /* reset to default IPG 12B */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MAC_MCR
              );
    value &= ~EQOS_MCR_IPG_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_MCR
      );
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              EQOS_MAC_EXTR
              );
    value &= ~EQOS_MAC_EXTR_EIPGEN;
    value &= ~EQOS_MAC_EXTR_EIPG_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      EQOS_MAC_EXTR
      );
  }

  /* start MAC Tx */
  eqos_config_mac_tx (osi_core, OSI_ENABLE);

  /* Updated MTL_EST depending on MACSEC enable/disable */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + EQOS_MTL_EST_CONTROL
            );
  value &= ~EQOS_MTL_EST_CONTROL_CTOV;
  if (enable == OSI_ENABLE) {
    temp   = EQOS_MTL_EST_CTOV_MACSEC_RECOMMEND;
    temp   = temp << EQOS_MTL_EST_CONTROL_CTOV_SHIFT;
    value |= temp & EQOS_MTL_EST_CONTROL_CTOV;
  } else {
    temp   = EQOS_MTL_EST_CTOV_RECOMMEND;
    temp   = temp << EQOS_MTL_EST_CONTROL_CTOV_SHIFT;
    value |= temp & EQOS_MTL_EST_CONTROL_CTOV;
  }

  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + EQOS_MTL_EST_CONTROL);
  return;
}

#endif /*  MACSEC_SUPPORT */

void
eqos_init_core_ops (
  struct core_ops  *ops
  )
{
  ops->core_init                    = eqos_core_init;
  ops->handle_common_intr           = eqos_handle_common_intr;
  ops->pad_calibrate                = eqos_pad_calibrate;
  ops->update_mac_addr_low_high_reg = eqos_update_mac_addr_low_high_reg;
  ops->adjust_mactime               = eqos_adjust_mactime;
  ops->read_mmc                     = eqos_read_mmc;
  ops->write_phy_reg                = eqos_write_phy_reg;
  ops->read_phy_reg                 = eqos_read_phy_reg;
  ops->get_hw_features              = eqos_get_hw_features;
 #ifndef OSI_STRIPPED_LIB
  ops->read_reg  = eqos_read_reg;
  ops->write_reg = eqos_write_reg;
 #endif /* !OSI_STRIPPED_LIB */
  ops->set_avb_algorithm  = eqos_set_avb_algorithm;
  ops->get_avb_algorithm  = eqos_get_avb_algorithm;
  ops->config_frp         = eqos_config_frp;
  ops->update_frp_entry   = eqos_update_frp_entry;
  ops->update_frp_nve     = eqos_update_frp_nve;
  ops->get_rchlist_index  = eqos_get_rchlist_index;
  ops->free_rchlist_index = eqos_free_rchlist_index;
 #if defined MACSEC_SUPPORT && !defined OSI_STRIPPED_LIB
  ops->read_macsec_reg  = eqos_read_macsec_reg;
  ops->write_macsec_reg = eqos_write_macsec_reg;
 #endif /*  MACSEC_SUPPORT */
 #ifdef MACSEC_SUPPORT
  ops->macsec_config_mac = eqos_config_for_macsec;
 #endif /*  MACSEC_SUPPORT */
  ops->config_l3l4_filters = eqos_config_l3l4_filters;
 #ifndef OSI_STRIPPED_LIB
  ops->config_tx_status      = eqos_config_tx_status;
  ops->config_rx_crc_check   = eqos_config_rx_crc_check;
  ops->config_flow_control   = eqos_config_flow_control;
  ops->config_arp_offload    = eqos_config_arp_offload;
  ops->config_ptp_offload    = eqos_config_ptp_offload;
  ops->config_vlan_filtering = eqos_config_vlan_filtering;
  ops->configure_eee         = eqos_configure_eee;
  ops->config_mac_loopback   = eqos_config_mac_loopback;
  ops->config_rss            = eqos_config_rss;
  ops->get_rss               = eqos_get_rss;
  ops->config_ptp_rxq        = eqos_config_ptp_rxq;
 #endif /* !OSI_STRIPPED_LIB */
 #ifdef HSI_SUPPORT
  ops->core_hsi_configure = eqos_hsi_configure;
 #ifdef NV_VLTEST_BUILD
  ops->core_hsi_inject_err = eqos_hsi_inject_err;
 #endif
 #endif
}
