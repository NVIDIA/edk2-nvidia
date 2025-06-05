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

#ifndef INCLUDED_OSI_MACSEC_H
#define INCLUDED_OSI_MACSEC_H

#include <osi_core.h>

#ifdef MACSEC_SUPPORT

//////////////////////////////////////////////////////////////////////////
/* MACSEC OSI data structures */
//////////////////////////////////////////////////////////////////////////

/**
 * @addtogroup TX/RX BYP/SCI LUT helpers macros
 *
 * @brief Helper macros for LUT programming
 * @{
 */
/** @brief valid AN0 flag */
#define OSI_AN0_VALID  OSI_BIT(0)
/** @brief valid AN1 flag */
#define OSI_AN1_VALID  OSI_BIT(1)
/** @brief valid AN2 flag */
#define OSI_AN2_VALID  OSI_BIT(2)
/** @brief valid AN3 flag */
#define OSI_AN3_VALID  OSI_BIT(3)
/** @brief maximum number of SAs supported */
#define OSI_MAX_NUM_SA  4U
  #ifdef DEBUG_MACSEC
#define OSI_CURR_AN_MAX  3
  #endif /* DEBUG_MACSEC */
/** @brief maximum key index */
#define OSI_KEY_INDEX_MAX       31U
#define OSI_KEY_INDEX_MAX_T26X  95U
/** @brief maximum PN by default */
#define OSI_PN_MAX_DEFAULT  0xFFFFFFFFU
/** @brief threshold PN by default */
#define OSI_PN_THRESHOLD_DEFAULT  0xC0000000U
/** @brief TCI by default */
#define OSI_TCI_DEFAULT  0x1
/** @brief maximum SCs index */
#define OSI_SC_INDEX_MAX  15U
/** @brief maximum SCs index for T26X */
#define OSI_SC_INDEX_MAX_T26X  47U

/**
 * @brief Length of ethernet type field
 */
#define OSI_ETHTYPE_LEN  2

/**
 * @brief Maximum bype pattern match
 */
#define OSI_LUT_BYTE_PATTERN_MAX  4U
/** @brief LUT byte pattern offset range 0-63 */
#define OSI_LUT_BYTE_PATTERN_MAX_OFFSET  63U
/** @brief VLAN PCP range 0-7 */
#define OSI_VLAN_PCP_MAX  7U
/** @brief VLAN ID range 1-4095 */
#define OSI_VLAN_ID_MAX  4095U
/** @brief flag to select BYPASS LUT */
#define OSI_LUT_SEL_BYPASS  0U
/** @brief flag to select SCI LUT */
#define OSI_LUT_SEL_SCI  1U
/** @brief flag to select SC_PARAM LUT */
#define OSI_LUT_SEL_SC_PARAM  2U
/** @brief flag to select SC_STATE LUT */
#define OSI_LUT_SEL_SC_STATE  3U
/** @brief flag to select SA_STATE LUT */
#define OSI_LUT_SEL_SA_STATE  4U
/** @brief maximum LUTs to select */
#define OSI_LUT_SEL_MAX  4U
/** @brief Flag indicating which bytes of DA is valid */
#define OSI_LUT_FLAGS_DA_VALID  (OSI_BIT(0) | OSI_BIT(1) | OSI_BIT(2) |        \
                                         OSI_BIT(3) | OSI_BIT(4) | OSI_BIT(5))
/** @brief Flag indicating which bytes of SA is valid */
#define OSI_LUT_FLAGS_SA_VALID  (OSI_BIT(6) | OSI_BIT(7) | OSI_BIT(8) |        \
                                         OSI_BIT(9) | OSI_BIT(10) | OSI_BIT(11))
/** @brief Flag indicating ethernet type is valid */
#define OSI_LUT_FLAGS_ETHTYPE_VALID  OSI_BIT(12)
/** @brief Flag indicating vlan PCP is valid */
#define OSI_LUT_FLAGS_VLAN_PCP_VALID  OSI_BIT(13)
/** @brief Flag indicating vlan ID is valid */
#define OSI_LUT_FLAGS_VLAN_ID_VALID  OSI_BIT(14)
/** @brief Flag indicating vlan is present */
#define OSI_LUT_FLAGS_VLAN_VALID  OSI_BIT(15)
/** @brief Flag indicating BYTE0 pattern is present */
#define OSI_LUT_FLAGS_BYTE0_PATTERN_VALID  OSI_BIT(16)
/** @brief Flag indicating BYTE1 pattern is present */
#define OSI_LUT_FLAGS_BYTE1_PATTERN_VALID  OSI_BIT(17)
/** @brief Flag indicating BYTE2 pattern is present */
#define OSI_LUT_FLAGS_BYTE2_PATTERN_VALID  OSI_BIT(18)
/** @brief Flag indicating BYTE3 pattern is present */
#define OSI_LUT_FLAGS_BYTE3_PATTERN_VALID  OSI_BIT(19)
/** @brief Flag indicating preemptable frame */
#define OSI_LUT_FLAGS_PREEMPT  OSI_BIT(20)
/** @brief Flag indicating preemptable field is valid */
#define OSI_LUT_FLAGS_PREEMPT_VALID  OSI_BIT(21)
/** @brief Flag indicating controlled port */
#define OSI_LUT_FLAGS_CONTROLLED_PORT  OSI_BIT(22)
/** @brief Flag indicating Double VLAN packet */
#define OSI_LUT_FLAGS_DVLAN_PKT  OSI_BIT(23)
/** @brief Flag indicating Double VLAN INNER tag select */
#define OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL  OSI_BIT(24)
/** @brief Flag indicating flags entry is valid */
#define OSI_LUT_FLAGS_ENTRY_VALID  OSI_BIT(31)
/** @} */

/**
 * @addtogroup MACSEC-Generic table CONFIG register helpers macros
 *
 * @brief Helper macros for generic table CONFIG register programming
 * @{
 */
/** @brief MACSEC max ip types */
#define MAX_MACSEC_IP_TYPES  2
#define OSI_MACSEC_T23X      0U
#define OSI_MACSEC_T26X      1U
/** @brief TX MACSEC controller */
#define OSI_CTLR_SEL_TX  0U
/** @brief RX MACSEC controller */
#define OSI_CTLR_SEL_RX   1U
#define OSI_CTLR_SEL_MAX  1U
/** @brief LUT read operation */
#define OSI_LUT_READ  0U
/** @brief LUT write operation */
#define OSI_LUT_WRITE  1U
#define OSI_RW_MAX     1U
/** @brief Maximum bypass lut table index */
#define OSI_BYP_LUT_MAX_INDEX  31U
/** @brief Maximum bypass lut table index for T26X */
#define OSI_BYP_LUT_MAX_INDEX_T26X  47U
/** @brief Maximum number of SAs */
#define OSI_SA_LUT_MAX_INDEX  31U
/** @brief Maximum number of SAs for T26X */
#define OSI_SA_LUT_MAX_INDEX_T26X  95U

/** @} */

  #ifdef DEBUG_MACSEC

/**
 * @addtogroup Debug buffer table CONFIG register helpers macros
 *
 * @brief Helper macros for debug buffer table CONFIG register programming
 * @{
 */
/** Num of Tx debug buffers */
#define OSI_TX_DBG_BUF_IDX_MAX  12U
/** Num of Rx debug buffers */
#define OSI_RX_DBG_BUF_IDX_MAX  13U
/** flag - encoding various debug event bits */
#define OSI_TX_DBG_LKUP_MISS_EVT      OSI_BIT(0)
#define OSI_TX_DBG_AN_NOT_VALID_EVT   OSI_BIT(1)
#define OSI_TX_DBG_KEY_NOT_VALID_EVT  OSI_BIT(2)
#define OSI_TX_DBG_CRC_CORRUPT_EVT    OSI_BIT(3)
#define OSI_TX_DBG_ICV_CORRUPT_EVT    OSI_BIT(4)
#define OSI_TX_DBG_CAPTURE_EVT        OSI_BIT(5)
#define OSI_RX_DBG_LKUP_MISS_EVT      OSI_BIT(6)
#define OSI_RX_DBG_KEY_NOT_VALID_EVT  OSI_BIT(7)
#define OSI_RX_DBG_REPLAY_ERR_EVT     OSI_BIT(8)
#define OSI_RX_DBG_CRC_CORRUPT_EVT    OSI_BIT(9)
#define OSI_RX_DBG_ICV_ERROR_EVT      OSI_BIT(10)
#define OSI_RX_DBG_CAPTURE_EVT        OSI_BIT(11)
/** @} */
  #endif /* DEBUG_MACSEC*/

/**
 * @addtogroup AES ciphers
 *
 * @brief Helper macro's for AES ciphers
 * @{
 */
/** @brief select CIPHER AES128 */
#define OSI_MACSEC_CIPHER_AES128  0U
/** @brief select CIPHER AES256 */
#define OSI_MACSEC_CIPHER_AES256  1U
/** @} */

/**
 * @brief Indicates different operations on MACSEC SA
 */
  #ifdef MACSEC_KEY_PROGRAM
/** @brief Command to create SA */
#define OSI_CREATE_SA  1U
  #endif /* MACSEC_KEY_PROGRAM */
/** @brief Command to enable SA */
#define OSI_ENABLE_SA  2U

/**
 * @brief MACSEC SA State LUT entry outputs structure
 */
struct osi_sa_state_outputs {
  /** Indicates next PN to use
   * valid values are from 1 to UINT32_MAX */
  nveu32_t    next_pn;

  /** Indicates lowest PN to use
   * valid values are from 0 to UINT32_MAX */
  nveu32_t    lowest_pn;
};

/**
 * @brief MACSEC SC State LUT entry outputs structure
 */
struct osi_sc_state_outputs {
  /** Indicates current AN to use
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_MAX_NUM_SA-1*/
  nveu32_t    curr_an;
};

/**
 * @brief MACSEC SC Param LUT entry outputs structure
 */
struct osi_sc_param_outputs {
  /** Indicates Key index start
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_KEY_INDEX_MAX */
  nveu32_t    key_index_start;

  /** PN max for given AN, after which HW will roll over to next AN
   * valid values are from 1 to NVETHERNETRM_PIF$OSI_PN_MAX_DEFAULT */
  nveu32_t    pn_max;

  /** PN threshold to trigger irq when threshold is reached
   * valid values are from 1 to NVETHERNETRM_PIF$OSI_PN_MAX_DEFAULT */
  nveu32_t    pn_threshold;

  /** Indidate PN window for engress packets
   * valid values are from 1 to NVETHERNETRM_PIF$OSI_PN_MAX_DEFAULT */
  nveu32_t    pn_window;

  /** SC identifier
   * valid values are from 0 to 0xFF for each array element*/
  nveu8_t     sci[OSI_SCI_LEN];

  /** Indicates SECTAG 3 TCI bits V, ES, SC
   * Default TCI value V=1, ES=0, SC = 1
   * valid range is from 0 to 7 */
  nveu8_t     tci;

  /** Indicates 1 bit VLAN IN CLEAR config
   * vlaid values are 0(vlan not in clear) and 1(vlan in clear) */
  nveu8_t     vlan_in_clear;
  /** Indicates 1 bit Encription config */
  nveu8_t     encrypt;
  /** Indicates 2 bit confidentiality offset config */
  nveu8_t     conf_offset;
};

/**
 * @brief MACSEC SCI LUT entry outputs structure
 */
struct osi_sci_lut_outputs {
  /** Indicates SC index to use
   * valid values are rom NVETHERNETRM_PIF$OSI_SC_INDEX_MAX */
  nveu32_t    sc_index;

  /** SC identifier
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t     sci[OSI_SCI_LEN];

  /** AN's valid bitmap
   * valid map contains NVETHERNETRM_PIF$OSI_AN0_VALID to
   * NVETHERNETRM_PIF$OSI_AN3_VALID */
  nveu32_t    an_valid;
};

/**
 * @brief MACSEC LUT config data structure
 */
struct osi_macsec_table_config {
  /** Indicates controller select
   * valid values are NVETHERNETRM_PIF$OSI_CTLR_SEL_RX and
   * NVETHERNETRM_PIF$OSI_CTLR_SEL_TX */
  nveu16_t    ctlr_sel;

  /** Read or write operation select
   * valid values are NVETHERNETRM_PIF$OSI_LUT_READ and
   * NVETHERNETRM_PIF$OSI_LUT_WRITE */
  nveu16_t    rw;

  /** LUT entry index
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_TABLE_INDEX_MAX */
  nveu16_t    index;
};

  #if defined (MACSEC_KEY_PROGRAM) || defined (LINUX_OS)

/**
 * @brief MACSEC Key Table entry structure
 */
struct osi_kt_entry {
  /** Indicates SAK key - max 256bit */
  nveu8_t    sak[OSI_KEY_LEN_256];
  /** Indicates Hash-key */
  nveu8_t    h[OSI_KEY_LEN_128];
};

  #endif /* MACSEC_KEY_PROGRAM */

/**
 * @brief MACSEC BYP/SCI LUT entry inputs structure
 */
struct osi_lut_inputs {
  /** MAC DA to compare
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t     da[OSI_ETH_ALEN];

  /** MAC SA to compare
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t     sa[OSI_ETH_ALEN];

  /** Ethertype to compare
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t     ethtype[OSI_ETHTYPE_LEN];

  /** 4-Byte pattern to compare
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t     byte_pattern[OSI_LUT_BYTE_PATTERN_MAX];

  /** Offset for 4-Byte pattern to compare
   * valid values are from 0 to 0xFF for each array element */
  nveu32_t    byte_pattern_offset[OSI_LUT_BYTE_PATTERN_MAX];

  /** VLAN PCP to compare
   * valid vaues are from 0 to NVETHERNETRM_PIF$OSI_VLAN_PCP_MAX */
  nveu32_t    vlan_pcp;

  /** VLAN ID to compare
   * valid vaues are from 0 to NVETHERNETRM_PIF$OSI_VLAN_ID_MAX */
  nveu32_t    vlan_id;
};

/**
 * @brief MACSEC LUT config data structure
 */
struct osi_macsec_lut_config {
  /** Generic table config
   * refer NVETHERNETRM_PIF$osi_macsec_table_config data for more info*/
  struct osi_macsec_table_config    table_config;

  /** Indicates LUT to select
   * NVETHERNETRM_PIF$OSI_LUT_SEL_BYPASS: Bypass LUT
   * NVETHERNETRM_PIF$OSI_LUT_SEL_SCI: SCI LUT
   * NVETHERNETRM_PIF$OSI_LUT_SEL_SC_PARAM: SC PARAM LUT
   * NVETHERNETRM_PIF$OSI_LUT_SEL_SC_STATE: SC STATE LUT
   * NVETHERNETRM_PIF$OSI_LUT_SEL_SA_STATE: SA STATE LUT
   */
  nveu16_t                       lut_sel;

  /** flag - encoding various valid LUT bits for above fields
   * this is a bit map with ma value as UINT32_MAX
   * for more details refer from NVETHERNETRM_PIF$OSI_LUT_FLAGS_DA_VALID to
   * NVETHERNETRM_PIF$OSI_LUT_FLAGS_ENTRY_VALID */
  nveu32_t                       flags;
  /** LUT inputs to use */
  struct osi_lut_inputs          lut_in;

  /** SCI LUT outputs
   * for more details refer NVETHERNETRM_PIF$osi_sci_lut_outputs */
  struct osi_sci_lut_outputs     sci_lut_out;

  /** SC Param LUT outputs
   * for more details refer NVETHERNETRM_PIF$osi_sc_param_outputs */
  struct osi_sc_param_outputs    sc_param_out;

  /** SC State LUT outputs
   * for more details refer NVETHERNETRM_PIF$osi_sc_state_outputs */
  struct osi_sc_state_outputs    sc_state_out;

  /** SA State LUT outputs
   * for more details refer NVETHERNETRM_PIF$osi_sa_state_outputs */
  struct osi_sa_state_outputs    sa_state_out;
};

  #if defined (MACSEC_KEY_PROGRAM) || defined (LINUX_OS)

/**
 * @brief MACSEC Key Table config data structure
 */
struct osi_macsec_kt_config {
  /** Generic table config */
  struct osi_macsec_table_config    table_config;
  /** Key Table entry config */
  struct osi_kt_entry               entry;
  /** Indicates key table entry valid or not, bit 31 */
  nveu32_t                          flags;
};

  #endif /* MACSEC_KEY_PROGRAM */

  #ifdef DEBUG_MACSEC

/**
 * @brief MACSEC Debug buffer config data structure
 */
struct osi_macsec_dbg_buf_config {
  /** Indicates Controller select
   * valid values are NVETHERNETRM_PIF$OSI_CTLR_SEL_RX and
   * NVETHERNETRM_PIF$OSI_CTLR_SEL_TX*/
  nveu16_t    ctlr_sel;

  /** Read or write operation select
   * valid values are NVETHERNETRM_PIF$OSI_LUT_READ and
   * NVETHERNETRM_PIF$OSI_LUT_WRITE*/
  nveu16_t    rw;

  /** Indicates debug data buffer
   * valid values are from 0 to 0xFF for each array element*/
  nveu32_t    dbg_buf[4];

  /** flag - encoding various debug event bits
   * valid values are from OSI_BIT(0) to OSI_BIT(11)*/
  nveu32_t    flags;

  /** Indicates debug buffer index
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_TABLE_INDEX_MAX */
  nveu32_t    index;
};

  #endif

/**
 * @brief MACSEC core operations structure
 */
struct osi_macsec_core_ops {
  /** macsec init */
  nve32_t    (*init)(
    struct osi_core_priv_data *const  osi_core,
    nveu32_t                          mtu,
    nveu8_t *const                    mac_addr
    );
  /** macsec de-init */
  nve32_t    (*deinit)(
    struct osi_core_priv_data *const  osi_core
    );
  /** Macsec irq handler */
  void       (*handle_irq)(
    struct osi_core_priv_data *const  osi_core
    );
  /** macsec lut config */
  nve32_t    (*lut_config)(
    struct osi_core_priv_data *const     osi_core,
    struct osi_macsec_lut_config *const  lut_config
    );
 #ifdef MACSEC_KEY_PROGRAM
  /** macsec kt config */
  nve32_t    (*kt_config)(
    struct osi_core_priv_data *const    osi_core,
    struct osi_macsec_kt_config *const  kt_config
    );
 #endif /* MACSEC_KEY_PROGRAM */
  /** macsec cipher config */
  nve32_t    (*cipher_config)(
    struct osi_core_priv_data *const  osi_core,
    nveu32_t                          cipher
    );
 #ifdef DEBUG_MACSEC
  /** macsec loopback config */
  nve32_t    (*loopback_config)(
    struct osi_core_priv_data *const  osi_core,
    nveu32_t                          enable
    );
 #endif /* DEBUG_MACSEC */
  /** macsec config SA in HW LUT */
  nve32_t    (*config)(
    struct osi_core_priv_data *const  osi_core,
    struct osi_macsec_sc_info *const  sc,
    nveu32_t                          enable,
    nveu16_t                          ctlr,
    nveu16_t                          *kt_idx
    );
  /** macsec read mmc counters */
  void       (*read_mmc)(
    struct osi_core_priv_data *const  osi_core
    );
 #ifdef DEBUG_MACSEC
  /** macsec debug buffer config */
  nve32_t    (*dbg_buf_config)(
    struct osi_core_priv_data *const         osi_core,
    struct osi_macsec_dbg_buf_config *const  dbg_buf_config
    );
  /** macsec debug buffer config */
  nve32_t    (*dbg_events_config)(
    struct osi_core_priv_data *const         osi_core,
    struct osi_macsec_dbg_buf_config *const  dbg_buf_config
    );
 #endif /* DEBUG_MACSEC */
  /** macsec get Key Index start for a given SCI */
  nve32_t    (*get_sc_lut_key_index)(
    struct osi_core_priv_data *const  osi_core,
    nveu8_t                           *sci,
    nveu32_t                          *key_index,
    nveu16_t                          ctlr
    );
  /** macsec set MTU size */
  nve32_t    (*update_mtu)(
    struct osi_core_priv_data *const  osi_core,
    nveu32_t                          mtu
    );
 #ifdef DEBUG_MACSEC
  /** macsec interrupts configuration */
  void       (*intr_config)(
    struct osi_core_priv_data *const  osi_core,
    nveu32_t                          enable
    );
 #endif /* DEBUG_MACSEC */
 #ifdef NV_VLTEST_BUILD
  void       (*hsi_macsec_error_inject)(
    struct osi_core_priv_data *const  osi_core,
    nveu32_t                          error_code
    );
 #endif
};

//////////////////////////////////////////////////////////////////////////
/* MACSEC OSI interface API prototypes */
//////////////////////////////////////////////////////////////////////////

/**
 * @brief
 * Description: Initialize MACSEC software operations
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - 0 on Successful initialization of MACSEC operatoions
 *  - -1 on NVETHERNETRM_PIF#osi_init_macsec_ops/osi_core is NULL
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_033
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_init_macsec_ops (
  struct osi_core_priv_data *const  osi_core
  );

/**
 * @brief
 * Description: Initialize MACSEC controller
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] mtu: MTU value
 *   * Range: 0 to UINT32_MAX
 * @param[in] macsec_vf_mac: A pointer to the MACID of Virtual Function
 *   * Range: A non-null pointer to VF MACID.
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - 0 on Successful initialization of MACSEC controller
 *  - -1 on NVETHERNETRM_PIF#osi_macsec_init/osi_core is NULL
 *  - -1 on pointer to VF MACID is NULL
 *  - -1 on failure in initialization of MACSEC controller
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_034
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_macsec_init (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          mtu,
  nveu8_t *const                    macsec_vf_mac
  );

/**
 * @brief
 * Description: De-Initialize the macsec controller
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - 0 on Successful de-initialization of MACSEC controller
 *  - -1 on MACSEC operations being NULL
 *  - -1 on failure in de-initialization of MACSEC controller
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: Yes
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_035
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_macsec_deinit (
  struct osi_core_priv_data *const  osi_core
  );

/**
 * @brief
 * Description: MACSEC Interrupt Handler
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - None
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_036
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
void
osi_macsec_isr (
  struct osi_core_priv_data *const  osi_core
  );

/**
 * @brief
 * Description: Read or write to macsec LUTs
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] lut_config: A pointer to the lut configuration
 *   * Range: A non-null pointer to LUT config.
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - 0 on Successful initialization of MACSEC controller
 *  - -1 on NVETHERNETRM_PIF#osi_macsec_config_lut/osi_core is NULL
 *  - -1 on pointer to LUT config is NULL
 *  - -1 on failure in reading or writing to MACSEC LUTs
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_037
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_macsec_config_lut (
  struct osi_core_priv_data *const     osi_core,
  struct osi_macsec_lut_config *const  lut_config
  );

  #ifdef MACSEC_KEY_PROGRAM

/**
 * @brief osi_macsec_config_kt - API to read or update the keys
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] kt_config: Keys that needs to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t
osi_macsec_config_kt (
  struct osi_core_priv_data *const    osi_core,
  struct osi_macsec_kt_config *const  kt_config
  );

  #endif /* MACSEC_KEY_PROGRAM */

/**
 * @brief
 * Description: Configure Cipher suite in MACSEC controller
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] cipher: Cipher suite value
 *   * Range: 0 to 1
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - 0 on Successful configuration of cipher suite
 *  - -1 on NVETHERNETRM_PIF#osi_macsec_cipher_config/osi_core is NULL
 *  - -1 on wrong cipher value obtaioned
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_038
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif

nve32_t
osi_macsec_cipher_config (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          cipher
  );

  #ifdef DEBUG_MACSEC

/**
 * @brief osi_macsec_loopback - API to enable/disable macsec loopback
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] enable: parameter to enable or disable
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */

nve32_t
osi_macsec_loopback (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          enable
  );

  #endif /* DEBUG_MACSEC */

/**
 * @brief
 * Description: Enables SC or SA in MACSEC controller
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] sc: A pointer to the secure channel parameters
 *   * Range: A non-null pointer to Secure Channel parameters
 * @param[in] enable: parameter that determines enable/disable of SC
 *   * Range: 0 or 1
 * @param[in] ctlr: parameter that determines Tx or Rx Controller selection
 *   * Range: 0 or 1
 * @param[out] kt_idx: A pointer to the key index for the give SC parameters
 *   * Range: A non-null pointer to key index
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - 0 on Successful programming of MACSEC LUTs
 *  - -1 on NVETHERNETRM_PIF#osi_macsec_config/osi_core is NULL
 *  - -1 on pointer to key index is NULL
 *  - -1 on wrong controller/enable status slection
 *  - -1 on failure in enable/disable the SC/SA
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_039
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_macsec_config (
  struct osi_core_priv_data *const  osi_core,
  struct osi_macsec_sc_info *const  sc,
  nveu32_t                          enable,
  nveu16_t                          ctlr,
  nveu16_t                          *kt_idx
  );

/**
 * @brief
 * Description: Reads different MACSEC counters
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - 0 on Successfully reading MACSEC counters
 *  - -1 on failure in readiming MACSEC counters
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_040
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_macsec_read_mmc (
  struct osi_core_priv_data *const  osi_core
  );

  #ifdef DEBUG_MACSEC

/**
 * @brief osi_macsec_config_dbg_buf - Reads the debug buffer captured
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[out] dbg_buf_config: dbg buffer data captured
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t
osi_macsec_config_dbg_buf (
  struct osi_core_priv_data *const         osi_core,
  struct osi_macsec_dbg_buf_config *const  dbg_buf_config
  );

/**
 * @brief osi_macsec_dbg_events_config - Enables debug buffer events
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] dbg_buf_config: dbg buffer data captured
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t
osi_macsec_dbg_events_config (
  struct osi_core_priv_data *const         osi_core,
  struct osi_macsec_dbg_buf_config *const  dbg_buf_config
  );

  #endif /* DEBUG_MACSEC */

/**
 * @brief
 * Description: API to get key index for a given SCI
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] sci: A pointer to the secure channel Identifier
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[out] key_index: A pointer to the key index that will be filled by this API
 *   * Range: A non-null pointer to key index
 * @param[in] ctlr: Parameter that determines the controller selection
 *   * Range: 0 or 1
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @return
 *  - 0 on Successfully obtaining key index
 *  - -1 on NVETHERNETRM_PIF#osi_macsec_get_sc_lut_key_index/osi_core is NULL
 *  - -1 on failure in obtaining the key index
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 * - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_041
 *
 **/
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_macsec_get_sc_lut_key_index (
  struct osi_core_priv_data *const  osi_core,
  nveu8_t                           *sci,
  nveu32_t                          *key_index,
  nveu16_t                          ctlr
  );

void
macsec_init_ops (
  void  *macsecops
  );

#endif /* MACSEC_SUPPORT */
#endif /* INCLUDED_OSI_MACSEC_H */
