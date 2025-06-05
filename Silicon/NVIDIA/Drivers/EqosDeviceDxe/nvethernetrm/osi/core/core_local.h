/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_CORE_LOCAL_H
#define INCLUDED_CORE_LOCAL_H

#include <osi_core.h>
#ifdef MACSEC_SUPPORT
  #include <osi_macsec.h>
#endif /* MACSEC_SUPPORT */
#include "common.h"

/**
 * @brief Maximum number of OSI core instances.
 */
#ifndef MAX_CORE_INSTANCES
#define MAX_CORE_INSTANCES  10U
#endif

/**
 * @brief Maximum number of interface operations.
 */
#define MAX_INTERFACE_OPS  2U

#define CHAN_START_POSITION  6U
#define PKT_ID_CNT           ((nveu32_t)1 << CHAN_START_POSITION)

/**
 * @brief Maximum number of timestamps stored in OSI from HW FIFO.
 */
// TBD: does it change for T264?
#define MAX_TX_TS_CNT  (PKT_ID_CNT * OSI_MGBE_MAX_NUM_CHANS)

/**
 * @brief FIFO size helper macro
 */
#define FIFO_SZ(x)  ((((x) * 1024U) / 256U) - 1U)

/**
 * @brief Dynamic configuration helper macros.
 */
#define DYNAMIC_CFG_L3_L4   OSI_BIT(0)
#define DYNAMIC_CFG_AVB     OSI_BIT(2)
#define DYNAMIC_CFG_L2      OSI_BIT(3)
#define DYNAMIC_CFG_L2_IDX  3U
#define DYNAMIC_CFG_RXCSUM  OSI_BIT(4)
#define DYNAMIC_CFG_PTP     OSI_BIT(7)
#define DYNAMIC_CFG_EST     OSI_BIT(8)
#define DYNAMIC_CFG_FPE     OSI_BIT(9)
#define DYNAMIC_CFG_FRP     OSI_BIT(10)
#ifdef HSI_SUPPORT
#define DYNAMIC_CFG_HSI  OSI_BIT(11)
#endif /* HSI_SUPPORT */

#ifndef OSI_STRIPPED_LIB
#define DYNAMIC_CFG_FC        OSI_BIT(1)
#define DYNAMIC_CFG_VLAN      OSI_BIT(5)
#define DYNAMIC_CFG_EEE       OSI_BIT(6)
#define DYNAMIC_CFG_FC_IDX    1U
#define DYNAMIC_CFG_VLAN_IDX  5U
#define DYNAMIC_CFG_EEE_IDX   6U
#define DYNAMIC_CFG_PTP_IDX   7U
#endif /* !OSI_STRIPPED_LIB */

#define DYNAMIC_CFG_L3_L4_IDX   0U
#define DYNAMIC_CFG_AVB_IDX     2U
#define DYNAMIC_CFG_L2_IDX      3U
#define DYNAMIC_CFG_RXCSUM_IDX  4U
#define DYNAMIC_CFG_EST_IDX     8U
#define DYNAMIC_CFG_FPE_IDX     9U
#define DYNAMIC_CFG_FRP_IDX     10U
#ifdef HSI_SUPPORT
#define DYNAMIC_CFG_HSI_IDX  11U
#endif /* HSI_SUPPORT */
#define OSI_SUSPENDED  OSI_BIT(0)

/**
 * interface core ops
 */
struct if_core_ops {
  /** Interface function called to initialize MAC and MTL registers */
  nve32_t    (*if_core_init)(
    struct osi_core_priv_data *const  osi_core
    );
  /** Interface function called to deinitialize MAC and MTL registers */
  nve32_t    (*if_core_deinit)(
    struct osi_core_priv_data *const  osi_core
    );
  /** Interface function called to write into a PHY reg over MDIO bus */
  nve32_t    (*if_write_phy_reg)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    phyaddr,
    const nveu32_t                    phyreg,
    const nveu16_t                    phydata
    );
  /** Interface function called to read a PHY reg over MDIO bus from DT*/
  nve32_t    (*if_read_phy_reg)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    phyaddr,
    const nveu32_t                    phyreg
    );
 #ifdef PHY_PROG
  /** Interface function called to program PHY from DTB enteries*/
  nve32_t    (*if_write_phy_reg_dt)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    phyaddr,
    const nveu32_t                    macMdioForAddrReg,
    const nveu32_t                    macMdioForDataReg
    );
  /** Interface function called to program PHY from DTB enteries*/
  nve32_t    (*if_read_phy_reg_dt)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    phyaddr,
    const nveu32_t                    macMdioForAddrReg,
    const nveu32_t                    macMdioForDataReg
    );
 #endif /* PHY_PROG */
  /** Initialize Interface core operations */
  nve32_t    (*if_init_core_ops)(
    struct osi_core_priv_data *const  osi_core
    );
  /** Interface function called to handle runtime commands */
  nve32_t    (*if_handle_ioctl)(
    struct osi_core_priv_data  *osi_core,
    struct osi_ioctl           *data
    );
};

/**
 * @brief Initialize MAC & MTL core operations.
 */
struct core_ops {
  /** Called to initialize MAC and MTL registers */
  nve32_t     (*core_init)(
    struct osi_core_priv_data *const  osi_core
    );
  /** Called to handle common interrupt */
  void        (*handle_common_intr)(
    struct osi_core_priv_data *const  osi_core
    );
  /** Called to do pad caliberation */
  nve32_t     (*pad_calibrate)(
    struct osi_core_priv_data *const  osi_core
    );
  /** Called to update MAC address 1-127 */
  nve32_t     (*update_mac_addr_low_high_reg)(
    struct osi_core_priv_data *const  osi_core,
    const struct osi_filter           *filter
    );
  /** Called to configure L3L4 filter */
  nve32_t     (*config_l3l4_filters)(
    struct osi_core_priv_data *const      osi_core,
    nveu32_t                              filter_no,
    const struct osi_l3_l4_filter *const  l3_l4
    );
  /** Called to adjust the mac time */
  nve32_t     (*adjust_mactime)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    sec,
    const nveu32_t                    nsec,
    const nveu32_t                    neg_adj,
    const nveu32_t                    one_nsec_accuracy
    );
  /** Called to update MMC counter from HW register */
  void        (*read_mmc)(
    struct osi_core_priv_data *const  osi_core
    );
  /** Called to write into a PHY reg over MDIO bus */
  nve32_t     (*write_phy_reg)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    phyaddr,
    const nveu32_t                    phyreg,
    const nveu16_t                    phydata
    );
  /** Called to read from a PHY reg over MDIO bus */
  nve32_t     (*read_phy_reg)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    phyaddr,
    const nveu32_t                    phyreg
    );
 #ifdef PHY_PROG
  /** Called to write into a PHY reg over MDIO bus when read from DT*/
  nve32_t     (*write_phy_reg_dt)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    phyaddr,
    const nveu32_t                    macMdioForAddrReg,
    const nveu32_t                    macMdioForDataReg
    );
  /** Called to read from a PHY reg over MDIO bus when read from DT */
  nve32_t     (*read_phy_reg_dt)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    phyaddr,
    const nveu32_t                    macMdioForAddrReg,
    const nveu32_t                    macMdioForDataReg
    );
 #endif /* PHY_PROG */
  /** Called to get HW features */
  void        (*get_hw_features)(
    struct osi_core_priv_data *const  osi_core,
    struct osi_hw_features            *hw_feat
    );
 #ifndef OSI_STRIPPED_LIB
  /** Called to read reg */
  nveu32_t    (*read_reg)(
    struct osi_core_priv_data *const  osi_core,
    const nve32_t                     reg
    );
  /** Called to write reg */
  nveu32_t    (*write_reg)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    val,
    const nve32_t                     reg
    );
 #endif
 #if defined MACSEC_SUPPORT && !defined OSI_STRIPPED_LIB
  /** Called to read macsec reg */
  nveu32_t (*read_macsec_reg)(
  struct osi_core_priv_data *const osi_core,
  const nve32_t reg
  );
  /** Called to write macsec reg */
  nveu32_t    (*write_macsec_reg)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    val,
    const nve32_t                     reg
    );
 #endif /*  MACSEC_SUPPORT */
 #ifdef MACSEC_SUPPORT
  void        (*macsec_config_mac)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    enable
    );
 #endif /*  MACSEC_SUPPORT */
 #ifndef OSI_STRIPPED_LIB
  /** Called to configure the MTL to forward/drop tx status */
  nve32_t     (*config_tx_status)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    tx_status
    );
  /** Called to configure the MAC rx crc */
  nve32_t     (*config_rx_crc_check)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    crc_chk
    );
  /** Called to configure the MAC flow control */
  nve32_t     (*config_flow_control)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    flw_ctrl
    );
  /** Called to enable/disable HW ARP offload feature */
  nve32_t     (*config_arp_offload)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    enable,
    const nveu8_t                     *ip_addr
    );
  /** Called to configure HW PTP offload feature */
  nve32_t     (*config_ptp_offload)(
    struct osi_core_priv_data *const  osi_core,
    struct osi_pto_config *const      pto_config
    );
  /** Called to configure VLAN filtering */
  nve32_t     (*config_vlan_filtering)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    filter_enb_dis,
    const nveu32_t                    perfect_hash_filtering,
    const nveu32_t                    perfect_inverse_match
    );
  /** Called to configure EEE Tx LPI */
  void        (*configure_eee)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    tx_lpi_enabled,
    const nveu32_t                    tx_lpi_timer
    );
  /** Called to configure MAC in loopback mode */
  nve32_t     (*config_mac_loopback)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    lb_mode
    );
  /** Called to configure RSS for MAC */
  nve32_t     (*config_rss)(
    struct osi_core_priv_data  *osi_core,
    const struct osi_core_rss  *rss
    );
  /** Called to get RSS parameters from MAC */
  nve32_t     (*get_rss)(
    struct osi_core_priv_data  *osi_core,
    struct osi_core_rss        *rss
    );
  /** Called to configure the PTP RX packets Queue */
  nve32_t     (*config_ptp_rxq)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    rxq_idx,
    const nveu32_t                    enable
    );
 #endif /* !OSI_STRIPPED_LIB */
  /** Called to set av parameter */
  nve32_t     (*set_avb_algorithm)(
    struct osi_core_priv_data *const            osi_core,
    const struct osi_core_avb_algorithm *const  avb
    );
  /** Called to get av parameter */
  nve32_t     (*get_avb_algorithm)(
    struct osi_core_priv_data *const      osi_core,
    struct osi_core_avb_algorithm *const  avb
    );
  /** Called to configure FRP engine */
  nve32_t     (*config_frp)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    enabled
    );
  /** Called to update FRP Instruction Table entry */
  nve32_t     (*update_frp_entry)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    pos,
    struct osi_core_frp_data *const   data
    );
  /** Called to update FRP NVE and  */
  void        (*update_frp_nve)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    nve
    );
  /** Called to get RCHList index */
  nve32_t     (*get_rchlist_index)(
    struct osi_core_priv_data *const  osi_core,
    nveu8_t const                     *mac_addr
    );
  /** Called to free RCHLIST index */
  void        (*free_rchlist_index)(
    struct osi_core_priv_data *const  osi_core,
    const nve32_t                     rch_indx
    );
 #ifdef HSI_SUPPORT
  /** Interface function called to initialize HSI */
  nve32_t     (*core_hsi_configure)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    enable
    );
 #ifdef NV_VLTEST_BUILD
  /** Interface function called to inject error */
  nve32_t     (*core_hsi_inject_err)(
    struct osi_core_priv_data *const  osi_core,
    const nveu32_t                    error_code
    );
 #endif
 #endif
};

/**
 * @brief constant values for drift MAC to MAC sync.
 */
/* No longer needed since DRIFT CAL is not used */
#define I_COMPONENT_BY_10        3LL
#define P_COMPONENT_BY_10        7LL
#define WEIGHT_BY_10             10LL
#define MAX_FREQ_POS             250000000LL
#define MAX_FREQ_NEG             -250000000LL
#define SERVO_STATS_0            0U
#define SERVO_STATS_1            1U
#define SERVO_STATS_2            2U
#define OSI_NSEC_PER_SEC_SIGNED  1000000000LL

#define ETHER_NSEC_MASK  0x7FFFFFFFU

/**
 * @brief servo data structure.
 */
struct core_ptp_servo {
  /** Offset/drift array to maintain current and last value */
  nvel64_t    offset[2];

  /** Target MAC HW time counter array to maintain current and last
   *  value
   */
  nvel64_t    local[2];

  /* Servo state. initialized with 0. This states are used to monitor
   * if there is sudden change in offset */
  nveu32_t    count;
  /* Accumulated freq drift */
  nvel64_t    drift;
  /* P component */
  nvel64_t    const_p;
  /* I component */
  nvel64_t    const_i;
  /* Last know ppb */
  nvel64_t    last_ppb;
  /* MAC to MAC locking to access HW time register within OSI calls */
  nveu32_t    m2m_lock;
};

/**
 * @brief AVB dynamic config storage structure
 */
struct core_avb {
  /** Represend whether AVB config done or not */
  nveu32_t                         used;
  /** AVB data structure */
  struct osi_core_avb_algorithm    avb_info;
};

/**
 * @brief VLAN dynamic config storage structure
 */
struct core_vlan {
  /** VID to be stored */
  nveu32_t    vid;
  /** Represens whether VLAN config done or not */
  nveu32_t    used;
};

/**
 * @brief L2 filter dynamic config storage structure
 */
struct core_l2 {
  nveu32_t             used;
  struct osi_filter    filter;
};

/**
 * @brief Dynamic config storage structure
 */
struct dynamic_cfg {
  nveu32_t                   flags;
  /** L3_L4 filters */
  struct osi_l3_l4_filter    l3_l4[OSI_MGBE_MAX_L3_L4_FILTER_T264];
  /** flow control */
  nveu32_t                   flow_ctrl;
  /** AVB */
  struct core_avb            avb[OSI_MGBE_MAX_NUM_QUEUES];
  /** RXCSUM */
  nveu32_t                   rxcsum;
  /** VLAN arguments storage */
  struct core_vlan           vlan[VLAN_NUM_VID];
  /** LPI parameters storage */
  nveu32_t                   tx_lpi_enabled;
  nveu32_t                   tx_lpi_timer;
  /** PTP information storage */
  nveu32_t                   ptp;
  /** EST information storage */
  struct osi_est_config      est;
  /** FPE information storage */
  struct osi_fpe_config      fpe;
  /** L2 filter storage */
  struct osi_filter          l2_filter;
  /** L2 filter configuration */
  struct core_l2             l2[EQOS_MAX_MAC_ADDRESS_FILTER];
 #ifdef HSI_SUPPORT
  /** HSI state */
  nveu32_t                   hsi_en_dis;
 #endif /* HSI_SUPPORT */
};

/**
 * @brief Core local data structure.
 */
struct core_local {
  /** OSI Core data variable */
  struct osi_core_priv_data     osi_core;
  /** Core local operations variable */
  struct core_ops               *ops_p;
  /** interface core local operations variable */
  struct if_core_ops            *if_ops_p;
  /** Address of MACsec HW operations structure */
  struct osi_macsec_core_ops    *macsec_ops;
  /** structure to store tx time stamps */
  struct osi_core_tx_ts         ts[MAX_TX_TS_CNT];
  /** Flag to represent infterface initialization done or not */
  nveu32_t                      if_init_done;
  /** Magic number to validate osi core pointer */
  nveu64_t                      magic_num;
  /** This is the head node for PTP packet ID queue */
  struct osi_core_tx_ts         tx_ts_head;
  /** Maximum number of queues/channels */
  nveu32_t                      num_max_chans;
  /** GCL depth supported by HW */
  nveu32_t                      gcl_dep;
  /** Max GCL width (time + gate) value supported by HW */
  nveu32_t                      gcl_width_val;
  /** TS lock */
  nveu32_t                      ts_lock;
  /** Controller mac to mac role */
  nveu32_t                      ether_m2m_role;
  /** Servo structure */
  struct core_ptp_servo         serv;
  /** HW comeout from reset successful OSI_ENABLE else OSI_DISABLE */
  nveu32_t                      hw_init_successful;
  /** Dynamic MAC to MAC time sync control for secondary interface */
  nveu32_t                      m2m_tsync;
  /** control pps output signal */
  nveu32_t                      pps_freq;
  /** Time interval mask for GCL entry */
  nveu32_t                      ti_mask;
  /** Hardware dynamic configuration context */
  struct dynamic_cfg            cfg;
  /** Hardware dynamic configuration state */
  nveu32_t                      state;
  /** XPCS Lane bringup/Block lock status */
  nveu32_t                      lane_status;
  /** XPCS power up status */
  nveu32_t                      lane_powered_up;
  /** Exact MAC used across SOCs 0:Legacy EQOS, 1:Orin EQOS, 2:Orin MGBE */
  nveu32_t                      l_mac_ver;
 #if defined (L3L4_WILDCARD_FILTER)
  /** l3l4 wildcard filter configured (OSI_ENABLE) / not configured (OSI_DISABLE) */
  nveu32_t                      l3l4_wildcard_filter_configured;
 #endif /* L3L4_WILDCARD_FILTER */
  /** Hardware features */
  struct osi_hw_features        hw_features;
};

/**
 * @brief update_counter_u_local - Increment nveu32_t counter
 *
 * @param[out] value: Pointer to value to be incremented.
 * @param[in] incr: increment value
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static inline void
update_counter_u_local (
  nveu32_t             *value,
  OSI_UNUSED nveu32_t  incr
  )
{
  (void)incr;
  *value = (((*value) & ((nveu32_t)INT_MAX)) + 1U) & (nveu32_t)INT_MAX;
}

/**
 * @brief eqos_init_core_ops - Initialize EQOS core operations.
 *
 * @param[in] ops: Core operations pointer.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void
eqos_init_core_ops (
  struct core_ops  *ops
  );

/**
 * @brief mgbe_init_core_ops - Initialize MGBE core operations.
 *
 * @param[in] ops: Core operations pointer.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void
mgbe_init_core_ops (
  struct core_ops  *ops
  );

/**
 * @brief ivc_init_macsec_ops - Initialize macsec core operations.
 *
 * @param[in] macsecops: Macsec operations pointer.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void
ivc_init_macsec_ops (
  void  *macsecops
  );

/**
 * @brief hw_interface_init_core_ops - Initialize HW interface functions.
 *
 * @param[in] if_ops_p: interface core operations pointer.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void
hw_interface_init_core_ops (
  struct if_core_ops  *if_ops_p
  );

/**
 * @brief ivc_interface_init_core_ops - Initialize IVC interface functions
 *
 * @param[in] if_ops_p: interface core operations pointer.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void
ivc_interface_init_core_ops (
  struct if_core_ops  *if_ops_p
  );

/**
 * @brief get osi pointer for PTP primary/sec interface
 *
 * @note
 * Algorithm:
 *  - Returns OSI core data structure corresponding to mac-to-mac PTP
 *    role.
 *
 * @pre OSD layer should use this as first API to get osi_core pointer and
 * use the same in remaning API invocation for mac-to-mac time sync.
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval valid and unique osi_core pointer on success
 * @retval NULL on failure.
 */
struct osi_core_priv_data *
get_role_pointer (
  nveu32_t  role
  );

/**
 * @brief
 * Description: osi_update_stats_counter - update value by increment passed
 * as parameter
 *
 * @param[in] last_value: last value of stat counter
 *   * Range: 0 to UINT64_MAX
 * @param[in] incr: increment value
 *   * Range: 0 to UINT64_MAX
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: Yes
 *  - Signal handler: Yes
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on sucess
 * @retval -1 on failure
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_016
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_042
 **/
#else

/**
 *
 * @dir
 *  - forward
 */
#endif
static inline nveu64_t
osi_update_stats_counter (
  nveu64_t  last_value,
  nveu64_t  incr
  )
{
  return ((last_value & (nveu64_t)OSI_LLONG_MAX) + (incr & (nveu64_t)OSI_LLONG_MAX));
}

/**
 * @addtogroup Generic helper MACROS
 *
 * @brief These are Generic helper macros used at various places.
 * @{
 */
/* RETRY_COUNT should be atleast MIN_USLEEP_10US
 * so that RETRY_COUNT/MIN_USLEEP_10US will result in
 * atleast 1 iteration.
 */
#define RETRY_COUNT     1000U
#define RETRY_DELAY     1U
#define OSI_DELAY_4US   4U
#define OSI_DELAY_10US  10U
#ifndef OSI_STRIPPED_LIB
#define OSI_DELAY_100US  100U
#endif
#define OSI_DELAY_200US    200U
#define OSI_DELAY_1000US   1000U
#define OSI_DELAY_10000US  10000U
#define OSI_DELAY_30000US  30000U

/** @} */

/** \cond DO_NOT_DOCUMENT */

/**
 * @brief osi_readl_poll_timeout - Periodically poll an address until
 * a condition is met or a timeout occurs
 *
 * @param[in] addr: Memory mapped address.
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lmask: input mask to be masked against register value for poll condition.
 * @param[in] rmask: expected output value to be compared against masked register value
 * with lmask for poll condition.
 * @param[in] delay_us: Maximum time to sleep between reads in us.
 * @param[in] retry: Retry count.

 * @note Physical address has to be memmory mapped.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
/* note: all users of osi_readl_poll_timeout are calling delay_us with 1us.
 * if delay_us > MIN_USLEEP_10US, then min_delay can be adjusted to input param instead.
 * currently adding this check to avoid logical dead code
 */
static inline nve32_t
osi_readl_poll_timeout (
  void                       *addr,
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   lmask,
  nveu32_t                   rmask,
  nveu32_t                   delay_us,
  nveu32_t                   retry
  )
{
  nveu32_t  once          = 0;
  nveu32_t  total_delay   = (delay_us) * (retry);
  nveu16_t  min_delay     = MIN_USLEEP_10US;
  nveu32_t  elapsed_delay = 0;
  nve32_t   ret           = -1;
  nveu32_t  val;

  while (elapsed_delay < total_delay) {
    val = osi_readl ((nveu8_t *)addr);
    if ((val & lmask) == rmask) {
      ret = 0;
      break;
    }

    if (once == 0U) {
      osi_core->osd_ops.udelay (OSI_DELAY_1US);
      once           = 1U;
      elapsed_delay += 1U;
    } else {
      osi_core->osd_ops.usleep (min_delay);
      elapsed_delay &= (nveu32_t)INT_MAX;
      elapsed_delay += min_delay;
    }
  }

  return ret;
}

/** \endcond */

#endif /* INCLUDED_CORE_LOCAL_H */
