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

#ifndef INCLUDED_XPCS_H_
#define INCLUDED_XPCS_H_

#include "core_local.h"
#include <osi_core.h>

/**
 * @addtogroup XPCS Register offsets
 *
 * @brief XPCS register offsets
 * @{
 */
#define XPCS_SR_XS_PCS_STS1                      0xC0004
#define XPCS_SR_XS_PCS_CTRL2                     0xC001C
#define XPCS_VR_XS_PCS_DIG_CTRL1                 0xE0000
#define XPCS_VR_XS_PCS_KR_CTRL                   0xE001C
#define XPCS_SR_AN_CTRL                          0x1C0000
#define XPCS_SR_PMA_KR_FEC_CTRL                  0x402ac
#define XPCS_SR_MII_CTRL                         0x7C0000
#define XPCS_VR_MII_AN_INTR_STS                  0x7E0008
#define XPCS_VS_MII_MMD_VR_MII_AN_CTRL_0         0x7E0004
#define XPCS_WRAP_UPHY_HW_INIT_CTRL              0x8020
#define XPCS_WRAP_UPHY_STATUS                    0x8044
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0            0x801C
#define XPCS_WRAP_INTERRUPT_STATUS               0x8050
#define T26X_XPCS_WRAP_UPHY_HW_INIT_CTRL         0x8038
#define T26X_XPCS_WRAP_UPHY_STATUS               0x8080
#define T26X_XPCS_WRAP_INTERRUPT_STATUS          0x808C
#define T26X_XPCS_WRAP_CONFIG_0                  0x8094
#define T26X_XPCS_WRAP_UPHY_T0_CTRL_2_0          0x8078
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_5_0          0x804c
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_4            0x8048
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_1            0x8004
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_1            0x803c
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_P_DN_0       0x806c
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_0       0x8020
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_1       0x8024
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_2       0x8028
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_3       0x802c
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_P_DN_4       0x8030
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_4            0x8010
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_5            0x8014
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_7            0x805c
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_8            0x8060
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_9            0x8064
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_3            0x800c
#define T26X_XPCS_WRAP_UPHY_TX_CTRL_2            0x8008
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_2            0x8040
#define T26X_XPCS_WRAP_UPHY_RX_CTRL_3            0x8044
#define T26X_XPCS_WRAP_UPHY_TIMEOUT_CONTROL_0_0  0x8070

/** @} */

/**
 * @addtogroup XLGPCS Register offsets
 *
 * @brief XLGPCS register offsets
 * @{
 */
#define XLGPCS_SR_PMA_CTRL2      0x4001c
#define XLGPCS_SR_PCS_CTRL1      0xc0000
#define XLGPCS_SR_PCS_STS1       0xc0004
#define XLGPCS_SR_PCS_CTRL2      0xc001c
#define XLGPCS_VR_PCS_DIG_CTRL1  0xe0000
#define XLGPCS_VR_PCS_DIG_CTRL3  0xe000c
#define XLGPCS_SR_AN_CTRL        0x1c0000

/** @} */

/**
 * @addtogroup XLGPCS-BIT Register bit fileds
 *
 * @brief XLGPCS register bit fields and values
 * @{
 */
#define XLGPCS_SR_PCS_CTRL1_RST                OSI_BIT(15)
#define XLGPCS_SR_AN_CTRL_AN_EN                OSI_BIT(12)
#define XLGPCS_SR_PCS_STS1_RLU                 OSI_BIT(2)
#define XLGPCS_SR_PCS_CTRL1_SS5_2              OSI_BIT(2) | OSI_BIT(4)
#define XLGPCS_SR_PCS_CTRL1_SS5_2_MASK         OSI_BIT(5) | OSI_BIT(4) |  \
                                                OSI_BIT(3) | OSI_BIT(2)
#define XLGPCS_SR_PCS_CTRL2_PCS_TYPE_SEL       OSI_BIT(2) | OSI_BIT(1) |  \
                                                OSI_BIT(0)
#define XLGPCS_SR_PCS_CTRL2_PCS_TYPE_SEL_MASK  OSI_BIT(3) | OSI_BIT(2) |  \
                                                OSI_BIT(1) | OSI_BIT(0)
#define XLGPCS_SR_PMA_CTRL2_PMA_TYPE           OSI_BIT(5) | OSI_BIT(4) |  \
                                                OSI_BIT(3) | OSI_BIT(0)
#define XLGPCS_SR_PMA_CTRL2_PMA_TYPE_MASK      0x7F
#define XLGPCS_VR_PCS_DIG_CTRL3_CNS_EN         OSI_BIT(0)
#define XLGPCS_VR_PCS_DIG_CTRL1_VR_RST         OSI_BIT(15)

#define XLGPCS_WRAP_UPHY_TO_CTRL2_EQ_DONE_TOV              0xFFFFU
#define XLGPCS_WRAP_UPHY_RX_CTRL5_RX_EQ_ENABLE             0x80000000U
#define XPCS_WRAP_UPHY_RX_CTRL_4_CDR_RESET_WIDTH           0x21U
#define XPCS_WRAP_UPHY_TX_CTRL_1_IDDQ_SLEEP_DLY            0x29U
#define XPCS_WRAP_UPHY_RX_CTRL_P_DN_0_RXEN_SLEEP_DLY       0xFFFF0000U
#define XPCS_WRAP_UPHY_RX_CTRL_P_DN_0_SLEEP_IDDQ_DLY       0xA1U
#define XPCS_WRAP_UPHY_TX_CTRL_P_DN_0_DATARDY_SLEEP_DLY    0x29U
#define XPCS_WRAP_UPHY_TX_CTRL_P_DN_1_DATAEN_RDY_DLY       0x81U
#define XPCS_WRAP_UPHY_TX_CTRL_P_DN_2_SLEEP_IDDQ_DLY       0xA1U
#define XPCS_WRAP_UPHY_TX_CTRL_P_DN_3_IDDQ_P_DN_DLY        0x285U
#define XPCS_WRAP_UPHY_TX_CTRL_P_DN_4_CAL_DONE_SLP_DLY     0x29U
#define XPCS_WRAP_UPHY_TX_CTRL_4_CAL_EN_HIGH_LOW_DLY       0x19U
#define XPCS_WRAP_UPHY_TX_CTRL_5_CAL_EN_LOW_DTRDY_DLY      0x79U
#define XPCS_WRAP_UPHY_RX_CTRL_7_EQ_RESET_WIDTH            0x29U
#define XPCS_WRAP_UPHY_RX_CTRL_8_EQ_TRAIN_EN_DELAY         0x29U
#define XPCS_WRAP_UPHY_RX_CTRL_9_EQ_TRAIN_EN_HILO_DLY      0x19U
#define XPCS_WRAP_UPHY_TX_CTRL_3_DATAREADY_DATAEN_DLY      0x82U
#define XPCS_WRAP_UPHY_RX_CTRL_2_SLEEP_CAL_EN_DLY          0xFBDU
#define XPCS_WRAP_UPHY_RX_CTRL_3_CAL_DONE_DATA_EN_DLY      0x78U
#define XPCS_10G_WRAP_UPHY_RX_CTRL_2_SLEEP_CAL_EN_DLY      0xCAAU
#define XPCS_10G_WRAP_UPHY_TX_CTRL_3_DATAREADY_DATAEN_DLY  0x50U
#define XPCS_10G_WRAP_UPHY_RX_CTRL_3_CAL_DONE_DATA_EN_DLY  0x32U
#define XPCS_WRAP_UPHY_TIMEOUT_CONTROL_0_0_VALUE           0x3FFFD90

#define EQOS_XPCS_WRAP_UPHY_HW_INIT_CTRL      0x8038
#define EQOS_XPCS_WRAP_UPHY_STATUS            0x8064
#define EQOS_XPCS_WRAP_UPHY_INTERRUPT_STATUS  0x8070
/** @} */

/**
 * @addtogroup XPCS-BIT Register bit fileds
 *
 * @brief XPCS register bit fields
 * @{
 */
#define XPCS_SR_XS_PCS_CTRL2_PCS_TYPE_SEL_BASE_R   0x0U
#define XPCS_SR_XS_PCS_STS1_RLU                    OSI_BIT(2)
#define XPCS_SR_XS_PCS_STS1_FLT                    OSI_BIT(7)
#define XPCS_VR_XS_PCS_DIG_CTRL1_USXG_EN           OSI_BIT(9)
#define XPCS_VR_XS_PCS_DIG_CTRL1_VR_RST            OSI_BIT(15)
#define XPCS_VR_XS_PCS_DIG_CTRL1_USRA_RST          OSI_BIT(10)
#define XPCS_VR_XS_PCS_DIG_CTRL1_CL37_BP           OSI_BIT(12)
#define XPCS_VS_MII_MMD_VR_MII_AN_CTRL_PCS_MODE    OSI_BIT(2)
#define XPCS_VS_MII_MMD_VR_MII_AN_CTRL_AN_INTR_EN  OSI_BIT(0)

#define XPCS_SR_AN_CTRL_AN_EN                         OSI_BIT(12)
#define XPCS_SR_MII_CTRL_RESTART_AN                   OSI_BIT(9)
#define XPCS_SR_MII_CTRL_AN_ENABLE                    OSI_BIT(12)
#define XPCS_VR_MII_AN_INTR_STS_CL37_ANCMPLT_INTR     OSI_BIT(0)
#define EQOS_XPCS_VR_MII_AN_INTR_STS_LINK_UP          OSI_BIT(4)
#define XPCS_SR_MII_CTRL_SS5                          OSI_BIT(5)
#define XPCS_SR_MII_CTRL_SS6                          OSI_BIT(6)
#define XPCS_SR_MII_CTRL_SS13                         OSI_BIT(13)
#define XPCS_USXG_AN_STS_SPEED_MASK                   0x1C00U
#define EQOS_XPCS_VR_MII_AN_STS_SPEED_MASK            0xCU
#define XPCS_USXG_AN_STS_SPEED_10                     0x0U
#define XPCS_USXG_AN_STS_SPEED_100                    0x4U
#define XPCS_USXG_AN_STS_SPEED_1000                   0x8U
#define XPCS_USXG_AN_STS_SPEED_2500                   0x1000U
#define XPCS_USXG_AN_STS_SPEED_5000                   0x1400U
#define XPCS_USXG_AN_STS_SPEED_10000                  0xC00U
#define XPCS_VR_XS_PCS_KR_CTRL_USXG_MODE_MASK         (OSI_BIT(12) |\
                                                 OSI_BIT(11) | \
                                                 OSI_BIT(10))
#define XPCS_VR_XS_PCS_KR_CTRL_USXG_MODE_5G           OSI_BIT(10)
#define XPCS_WRAP_UPHY_HW_INIT_CTRL_TX_EN             OSI_BIT(0)
#define XPCS_WRAP_UPHY_HW_INIT_CTRL_RX_EN             OSI_BIT(2)
#define XPCS_WRAP_UPHY_HW_INIT_CTRL_RX_P_DN           OSI_BIT(3)
#define XPCS_WRAP_IRQ_STATUS_PCS_LINK_STS             OSI_BIT(6)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_DATA_EN      OSI_BIT(0)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_IDDQ         OSI_BIT(4)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_AUX_RX_IDDQ     OSI_BIT(5)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_SLEEP        (OSI_BIT(6) |   \
                                                         OSI_BIT(7))
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CAL_EN       OSI_BIT(8)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CDR_RESET    OSI_BIT(9)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_PCS_PHY_RDY  OSI_BIT(10)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_SW_OVRD      OSI_BIT(31)
#define XPCS_WRAP_UPHY_STATUS_TX_P_UP_STATUS          OSI_BIT(0)
#define XPCS_WRAP_UPHY_STATUS_RX_P_UP_STATUS          OSI_BIT(2)
#define XPCS_SR_PMA_KR_FEC_CTRL_FEC_EN                OSI_BIT(0)
#define XPCS_SR_PMA_KR_FEC_CTRL_EN_ERR_IND            OSI_BIT(1)

#ifdef HSI_SUPPORT
#define XPCS_WRAP_INTERRUPT_CONTROL           0x8048
#define T26X_XPCS_WRAP_INTERRUPT_CONTROL      0x8084
#define XPCS_CORE_CORRECTABLE_ERR             OSI_BIT(10)
#define XPCS_CORE_UNCORRECTABLE_ERR           OSI_BIT(9)
#define XPCS_REGISTER_PARITY_ERR              OSI_BIT(8)
#define XPCS_VR_XS_PCS_SFTY_UE_INTR0          0xE03C0
#define XPCS_VR_XS_PCS_SFTY_CE_INTR           0xE03C8
#define XPCS_VR_XS_PCS_SFTY_DISABLE_0         0xE03D0
#define XPCS_VR_XS_PCS_SFTY_TMR_CTRL          0xE03D4
#define XPCS_SFTY_1US_MULT_MASK               0xFFU
#define XPCS_SFTY_1US_MULT_SHIFT              0U
#define XPCS_FSM_TO_SEL_SHIFT                 10U
#define XPCS_FSM_TO_SEL_MASK                  0xC00U
#define EQOS_PCS_SFTY_TMR_CTRL                0x7E03D4
#define EQOS_PCS_SFTY_TMR_CTRL_RXFPEI         OSI_BIT(8)
#define XLGPCS_VR_PCS_SFTY_TMR_CTRL           0xE03E4
#define XPCS_VR_XS_PCS_SFTY_TMR_CTRL_IFT_SEL  OSI_BIT(8)
#define XLGPCS_VR_XS_PCS_SFTY_DISABLE_0       0xE03C4
#define EQOS_PCS_SFTY_DISABLE_0               0x7E03D0
#define XPCS_SFTY_ENABLE_VAL                  0x0U
#define XPCS_SFTY_DISABLE_VAL                 0x1U
#define XLGPCS_SFTY_ENABLE_VAL                0x0U
#define XLGPCS_SFTY_DISABLE_VAL               0x3FU
#define PCS_FSM_TIMEOUT_DISABLE               0x1U
#define PCS_FSM_TIMEOUT_ENABLE                0x0U
#endif
/** @} */

nve32_t
xpcs_init (
  struct osi_core_priv_data  *osi_core
  );

nve32_t
xpcs_start (
  struct osi_core_priv_data  *osi_core
  );

nve32_t
xlgpcs_init (
  struct osi_core_priv_data  *osi_core
  );

nve32_t
xlgpcs_start (
  struct osi_core_priv_data  *osi_core
  );

#ifndef OSI_STRIPPED_LIB
nve32_t
xlgpcs_eee (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   en_dis
  );

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief xpcs_read - read from xpcs.
 *
 * Algorithm: This routine reads data from XPCS register.
 *
 * @param[in] xpcs_base: XPCS virtual base address
 * @param[in] reg_addr: register address to be read
 *
 * @retval value read from xpcs register.
 */
static inline nveu32_t
xpcs_read (
  void      *xpcs_base,
  nveu32_t  reg_addr
  )
{
  osi_writel (
    ((reg_addr >> XPCS_REG_ADDR_SHIFT) & XPCS_REG_ADDR_MASK),
    ((nveu8_t *)xpcs_base + XPCS_ADDRESS)
    );
  return osi_readl (
           (nveu8_t *)xpcs_base +
           ((reg_addr) & XPCS_REG_VALUE_MASK)
           );
}

/**
 * @brief xpcs_write - write to xpcs.
 *
 * Algorithm: This routine writes data to XPCS register.
 *
 * @param[in] xpcs_base: XPCS virtual base address
 * @param[in] reg_addr: register address for writing
 * @param[in] val: write value to register address
 */
static inline void
xpcs_write (
  void      *xpcs_base,
  nveu32_t  reg_addr,
  nveu32_t  val
  )
{
  osi_writel (
    ((reg_addr >> XPCS_REG_ADDR_SHIFT) & XPCS_REG_ADDR_MASK),
    ((nveu8_t *)xpcs_base + XPCS_ADDRESS)
    );
  osi_writel (
    val,
    (nveu8_t *)xpcs_base +
    (((reg_addr) & XPCS_REG_VALUE_MASK))
    );
}

/**
 * @brief xpcs_write_safety - write to xpcs.
 *
 * Algorithm: This routine writes data to XPCS register.
 * And verifiy by reading back the value
 *
 * @param[in] osi_core: OSI core data structure
 * @param[in] reg_addr: register address for writing
 * @param[in] val: write value to register address
 *
 * @retval 0 on success
 * @retval XPCS_WRITE_FAIL_CODE on failure
 *
 */
static inline nve32_t
xpcs_write_safety (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   reg_addr,
  nveu32_t                   val
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  read_val;
  /* 1 busy wait, and the remaining retries are sleeps of granularity MIN_USLEEP_10US */
  nveu32_t  retry = RETRY_ONCE;
  nveu32_t  count = 0;
  nveu32_t  once  = 0U;
  nve32_t   ret   = XPCS_WRITE_FAIL_CODE;
  nve32_t   cond  = COND_NOT_MET;

  while (cond == COND_NOT_MET) {
    xpcs_write (xpcs_base, reg_addr, val);
    read_val = xpcs_read (xpcs_base, reg_addr);
    if (val == read_val) {
      ret  = 0;
      cond = COND_MET;
    } else {
      if (count > retry) {
        break;
      }

      count++;
      if (once == 0U) {
        osi_core->osd_ops.udelay (OSI_DELAY_1US);

        /* udelay is a busy wait, so don't call it too frequently.
         * call it once to be optimistic, and then use usleep with
         * a longer timeout to yield to other CPU users.
         */
        once = 1U;
      } else {
        osi_core->osd_ops.usleep (MIN_USLEEP_10US);
      }
    }
  }

 #ifndef OSI_STRIPPED_LIB
  if (ret != 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "xpcs_write_safety failed",
      reg_addr
      );
  }

 #endif /* !OSI_STRIPPED_LIB */
  return ret;
}

#endif /* INCLUDED_XPCS_H_ */
