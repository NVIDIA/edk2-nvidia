/*
 * Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_OSI_MACSEC_H
#define INCLUDED_OSI_MACSEC_H

#include <osi_core.h>

//////////////////////////////////////////////////////////////////////////
	/* MACSEC OSI data structures */
//////////////////////////////////////////////////////////////////////////

/**
 * @addtogroup TX/RX BYP/SCI LUT helpers macros
 *
 * @brief Helper macros for LUT programming
 * @{
 */
#define OSI_SCI_LEN			8
#define OSI_KEY_LEN_128 		16
#define OSI_KEY_LEN_256 		32
#define OSI_AN0_VALID			OSI_BIT(0)
#define OSI_AN1_VALID			OSI_BIT(1)
#define OSI_AN2_VALID			OSI_BIT(2)
#define OSI_AN3_VALID			OSI_BIT(3)
#define OSI_MAX_NUM_SC			8
#define OSI_MAX_NUM_SA			4
#define OSI_CURR_AN_MAX 		3
#define OSI_KEY_INDEX_MAX		31
#define OSI_PN_MAX_DEFAULT		0xFFFFFFFF
#define OSI_PN_THRESHOLD_DEFAULT	0xC0000000
#define OSI_TCI_DEFAULT 		0x1
#define OSI_VLAN_IN_CLEAR_DEFAULT	0x0
#define OSI_SC_INDEX_MAX		15
#define OSI_ETHTYPE_LEN 		2
#define OSI_LUT_BYTE_PATTERN_MAX	4
/* LUT byte pattern offset range 0-63 */
#define OSI_LUT_BYTE_PATTERN_MAX_OFFSET 63
/* VLAN PCP range 0-7 */
#define OSI_VLAN_PCP_MAX		7
/* VLAN ID range 1-4095 */
#define OSI_VLAN_ID_MAX 		4095
#define OSI_LUT_SEL_BYPASS		0U
#define OSI_LUT_SEL_SCI 		1U
#define OSI_LUT_SEL_SC_PARAM		2U
#define OSI_LUT_SEL_SC_STATE		3U
#define OSI_LUT_SEL_SA_STATE		4U
#define OSI_LUT_SEL_MAX 		4U

/* LUT input fields flags bit offsets */
#define OSI_LUT_FLAGS_DA_BYTE0_VALID	OSI_BIT(0)
#define OSI_LUT_FLAGS_DA_BYTE1_VALID	OSI_BIT(1)
#define OSI_LUT_FLAGS_DA_BYTE2_VALID	OSI_BIT(2)
#define OSI_LUT_FLAGS_DA_BYTE3_VALID	OSI_BIT(3)
#define OSI_LUT_FLAGS_DA_BYTE4_VALID	OSI_BIT(4)
#define OSI_LUT_FLAGS_DA_BYTE5_VALID	OSI_BIT(5)
#define OSI_LUT_FLAGS_DA_VALID		(OSI_BIT(0) | OSI_BIT(1) | OSI_BIT(2) |\
					 OSI_BIT(3) | OSI_BIT(4) | OSI_BIT(5))
#define OSI_LUT_FLAGS_SA_BYTE0_VALID	OSI_BIT(6)
#define OSI_LUT_FLAGS_SA_BYTE1_VALID	OSI_BIT(7)
#define OSI_LUT_FLAGS_SA_BYTE2_VALID	OSI_BIT(8)
#define OSI_LUT_FLAGS_SA_BYTE3_VALID	OSI_BIT(9)
#define OSI_LUT_FLAGS_SA_BYTE4_VALID	OSI_BIT(10)
#define OSI_LUT_FLAGS_SA_BYTE5_VALID	OSI_BIT(11)
#define OSI_LUT_FLAGS_SA_VALID		(OSI_BIT(6) | OSI_BIT(7) | OSI_BIT(8) |\
					 OSI_BIT(9) | OSI_BIT(10) | OSI_BIT(11))
#define OSI_LUT_FLAGS_ETHTYPE_VALID	OSI_BIT(12)
#define OSI_LUT_FLAGS_VLAN_PCP_VALID	OSI_BIT(13)
#define OSI_LUT_FLAGS_VLAN_ID_VALID	OSI_BIT(14)
#define OSI_LUT_FLAGS_VLAN_VALID	OSI_BIT(15)
#define OSI_LUT_FLAGS_BYTE0_PATTERN_VALID	OSI_BIT(16)
#define OSI_LUT_FLAGS_BYTE1_PATTERN_VALID	OSI_BIT(17)
#define OSI_LUT_FLAGS_BYTE2_PATTERN_VALID	OSI_BIT(18)
#define OSI_LUT_FLAGS_BYTE3_PATTERN_VALID	OSI_BIT(19)
#define OSI_LUT_FLAGS_PREEMPT		OSI_BIT(20)
#define OSI_LUT_FLAGS_PREEMPT_VALID	OSI_BIT(21)
#define OSI_LUT_FLAGS_CONTROLLED_PORT	OSI_BIT(22)
#define OSI_LUT_FLAGS_DVLAN_PKT		OSI_BIT(23)
#define OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL	OSI_BIT(24)
#define OSI_LUT_FLAGS_ENTRY_VALID	OSI_BIT(31)
/** @} */

/**
 * @addtogroup Generic table CONFIG register helpers macros
 *
 * @brief Helper macros for generic table CONFIG register programming
 * @{
 */
#define OSI_CTLR_SEL_TX		0U
#define OSI_CTLR_SEL_RX		1U
#define OSI_CTLR_SEL_MAX	1U
#define OSI_NUM_CTLR		2U
#define OSI_LUT_READ		0U
#define OSI_LUT_WRITE		1U
#define OSI_RW_MAX		1U
#define OSI_TABLE_INDEX_MAX	31U
#define OSI_BYP_LUT_MAX_INDEX	OSI_TABLE_INDEX_MAX
#define OSI_SC_LUT_MAX_INDEX	15U
#define OSI_SA_LUT_MAX_INDEX	OSI_TABLE_INDEX_MAX
/** @} */

/**
 * @addtogroup Debug buffer table CONFIG register helpers macros
 *
 * @brief Helper macros for debug buffer table CONFIG register programming
 * @{
 */
#define OSI_DBG_TBL_READ	OSI_LUT_READ
#define OSI_DBG_TBL_WRITE	OSI_LUT_WRITE
/* Num of Tx debug buffers */
#define OSI_TX_DBG_BUF_IDX_MAX		12U
/* Num of Rx debug buffers */
#define OSI_RX_DBG_BUF_IDX_MAX		13U
#define OSI_DBG_BUF_IDX_MAX		OSI_RX_DBG_BUF_IDX_MAX
/** flag - encoding various debug event bits */
#define OSI_TX_DBG_LKUP_MISS_EVT	OSI_BIT(0)
#define OSI_TX_DBG_AN_NOT_VALID_EVT	OSI_BIT(1)
#define OSI_TX_DBG_KEY_NOT_VALID_EVT	OSI_BIT(2)
#define OSI_TX_DBG_CRC_CORRUPT_EVT	OSI_BIT(3)
#define OSI_TX_DBG_ICV_CORRUPT_EVT	OSI_BIT(4)
#define OSI_TX_DBG_CAPTURE_EVT		OSI_BIT(5)
#define OSI_RX_DBG_LKUP_MISS_EVT	OSI_BIT(6)
#define OSI_RX_DBG_KEY_NOT_VALID_EVT	OSI_BIT(7)
#define OSI_RX_DBG_REPLAY_ERR_EVT	OSI_BIT(8)
#define OSI_RX_DBG_CRC_CORRUPT_EVT	OSI_BIT(9)
#define OSI_RX_DBG_ICV_ERROR_EVT	OSI_BIT(10)
#define OSI_RX_DBG_CAPTURE_EVT		OSI_BIT(11)
/** @} */

/**
 * @addtogroup AES ciphers
 *
 * @brief Helper macro's for AES ciphers
 * @{
 */
#define OSI_MACSEC_CIPHER_AES128	0U
#define OSI_MACSEC_CIPHER_AES256	1U
/** @} */

/**
 * @addtogroup MACSEC Misc helper macro's
 *
 * @brief MACSEC Helper macro's
 * @{
 */
#define OSI_MACSEC_TX_EN	OSI_BIT(0)
#define OSI_MACSEC_RX_EN	OSI_BIT(1)
/* MACSEC SECTAG + ICV + 2B ethertype adds upto 34B */
#define MACSEC_TAG_ICV_LEN		34
/* MACSEC TZ key config cmd */
#define OSI_MACSEC_CMD_TZ_CONFIG	0x1
/* MACSEC TZ key table entries reset cmd */
#define OSI_MACSEC_CMD_TZ_KT_RESET	0x2
/** @} */

/**
 * @brief MACSEC SA State LUT entry outputs structure
 */
struct osi_sa_state_outputs {
	/** Indicates next PN to use */
	nveu32_t next_pn;
	/** Indicates lowest PN to use */
	nveu32_t lowest_pn;
};

/**
 * @brief MACSEC SC State LUT entry outputs structure
 */
struct osi_sc_state_outputs {
	/** Indicates current AN to use */
	nveu32_t curr_an;
};

/**
 * @brief MACSEC SC Param LUT entry outputs structure
 */
struct osi_sc_param_outputs {
	/** Indicates Key index start */
	nveu32_t key_index_start;
	/** PN max for given AN, after which HW will roll over to next AN */
	nveu32_t pn_max;
	/** PN threshold to trigger irq when threshold is reached */
	nveu32_t pn_threshold;
	/** Indidate PN window for engress packets */
	nveu32_t pn_window;
	/** SC identifier */
	nveu8_t sci[OSI_SCI_LEN];
	/** Indicates SECTAG 3 TCI bits V, ES, SC
	 * Default TCI value V=1, ES=0, SC = 1
	 */
	nveu8_t tci;
	/** Indicates 1 bit VLAN IN CLEAR config */
	nveu8_t vlan_in_clear;
};

/**
 * @brief MACSEC SCI LUT entry outputs structure
 */
struct osi_sci_lut_outputs {
	/** Indicates SC index to use */
	nveu32_t sc_index;
	/** SC identifier */
	nveu8_t sci[OSI_SCI_LEN];
	/** AN's valid */
	nveu32_t an_valid;
};

/**
 * @brief MACSEC LUT config data structure
 */
struct osi_macsec_table_config {
	/** Indicates controller select, Tx=0, Rx=1 */
	nveu16_t ctlr_sel;
	/** Read or write operation select, Read=0, Write=1 */
	nveu16_t rw;
	/** LUT entry index */
	nveu16_t index;
};

/**
 * @brief MACSEC Key Table entry structure
 */
struct osi_kt_entry {
	/** Indicates SAK key - max 256bit */
	nveu8_t sak[OSI_KEY_LEN_256];
	/** Indicates Hash-key */
	nveu8_t h[OSI_KEY_LEN_128];
};

/**
 * @brief MACSEC BYP/SCI LUT entry inputs structure
 */
struct osi_lut_inputs {
	/** MAC DA to compare */
	nveu8_t da[OSI_ETH_ALEN];
	/** MAC SA to compare */
	nveu8_t sa[OSI_ETH_ALEN];
	/** Ethertype to compare */
	nveu8_t ethtype[OSI_ETHTYPE_LEN];
	/** 4-Byte pattern to compare */
	nveu8_t byte_pattern[OSI_LUT_BYTE_PATTERN_MAX];
	/** Offset for 4-Byte pattern to compare */
	nveu32_t byte_pattern_offset[OSI_LUT_BYTE_PATTERN_MAX];
	/** VLAN PCP to compare */
	nveu32_t vlan_pcp;
	/** VLAN ID to compare */
	nveu32_t vlan_id;
};

/**
 * @brief MACSEC secure channel basic information
 */
struct osi_macsec_sc_info {
	/** Secure channel identifier */
	nveu8_t sci[OSI_SCI_LEN];
	/** Secure association key */
	nveu8_t sak[OSI_KEY_LEN_128];
	/** current AN */
	nveu8_t curr_an;
	/** Next PN to use for the current AN */
	nveu32_t next_pn;
	/** bitmap of valid AN */
	nveu32_t an_valid;
	/** PN window */
	nveu32_t pn_window;
	/** SC LUT index */
	nveu32_t sc_idx_start;
};

/**
 * @brief MACSEC HW controller LUT's global status
 */
struct osi_macsec_lut_status {
	/** List of max SC's supported */
	struct osi_macsec_sc_info sc_info[OSI_MAX_NUM_SC];
	/** next available BYP LUT index */
	nveu32_t next_byp_idx;
	/** next available SC LUT index */
	nveu32_t next_sc_idx;
};

/**
 * @brief MACSEC LUT config data structure
 */
struct osi_macsec_lut_config {
	/** Generic table config */
	struct osi_macsec_table_config table_config;
	/** Indicates LUT to select
	 * 0: Bypass LUT
	 * 1: SCI LUT
	 * 2: SC PARAM LUT
	 * 3: SC STATE LUT
	 * 4: SA STATE LUT
	 */
	nveu16_t lut_sel;
	/** flag - encoding various valid LUT bits for above fields */
	nveu32_t flags;
	/** LUT inputs to use */
	struct osi_lut_inputs lut_in;
	/** SCI LUT outputs */
	struct osi_sci_lut_outputs sci_lut_out;
	/** SC Param LUT outputs */
	struct osi_sc_param_outputs sc_param_out;
	/** SC State LUT outputs */
	struct osi_sc_state_outputs sc_state_out;
	/** SA State LUT outputs */
	struct osi_sa_state_outputs sa_state_out;
};

/**
 * @brief MACSEC Key Table config data structure
 */
struct osi_macsec_kt_config {
	/** Generic table config */
	struct osi_macsec_table_config table_config;
	/** Key Table entry config */
	struct osi_kt_entry entry;
	/** Indicates key table entry valid or not, bit 31 */
	nveu32_t flags;
};

/**
 * @brief MACSEC Debug buffer config data structure
 */
struct osi_macsec_dbg_buf_config {
	/** Indicates Controller select, Tx=0, Rx=1 */
	nveu16_t ctlr_sel;
	/** Read or write operation select, Read=0, Write=1 */
	nveu16_t rw;
	/** Indicates debug data buffer */
	nveu32_t dbg_buf[4];
	/** flag - encoding various debug event bits */
	nveu32_t flags;
	/** Indicates debug buffer index */
	nveu32_t index;
};

/**
 * @brief MACSEC core operations structure
 */
struct osi_macsec_core_ops {
	/** macsec init */
	nve32_t (*init)(struct osi_core_priv_data *const osi_core);
	/** macsec de-init */
	nve32_t (*deinit)(struct osi_core_priv_data *const osi_core);
	/** Non Secure irq handler */
	void (*handle_ns_irq)(struct osi_core_priv_data *const osi_core);
	/** Secure irq handler */
	void (*handle_s_irq)(struct osi_core_priv_data *const osi_core);
	/** macsec lut config */
	nve32_t (*lut_config)(struct osi_core_priv_data *const osi_core,
			  struct osi_macsec_lut_config *const lut_config);
#ifdef MACSEC_KEY_PROGRAM
	/** macsec kt config */
	nve32_t (*kt_config)(struct osi_core_priv_data *const osi_core,
			 struct osi_macsec_kt_config *const kt_config);
#endif /* MACSEC_KEY_PROGRAM */
	/** macsec cipher config */
	nve32_t (*cipher_config)(struct osi_core_priv_data *const osi_core,
			     nveu32_t cipher);
	/** macsec loopback config */
	nve32_t (*loopback_config)(struct osi_core_priv_data *const osi_core,
			       nveu32_t enable);
	/** macsec enable */
	nve32_t (*macsec_en)(struct osi_core_priv_data *const osi_core,
			 nveu32_t enable);
	/** macsec config SA in HW LUT */
	nve32_t (*config)(struct osi_core_priv_data *const osi_core,
		      struct osi_macsec_sc_info *const sc,
		      nveu32_t enable, nveu16_t ctlr,
		      nveu16_t *kt_idx);
	/** macsec read mmc counters */
	void (*read_mmc)(struct osi_core_priv_data *const osi_core);
	/** macsec debug buffer config */
	nve32_t (*dbg_buf_config)(struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config);
	/** macsec debug buffer config */
	nve32_t (*dbg_events_config)(struct osi_core_priv_data *const osi_core,
		 struct osi_macsec_dbg_buf_config *const dbg_buf_config);

};

//////////////////////////////////////////////////////////////////////////
	/* MACSEC OSI interface API prototypes */
//////////////////////////////////////////////////////////////////////////

/**
 * @brief initializing the macsec core operations
 *
 * @note
 * Algorithm:
 * - Init osi_core macsec ops and lut status structure members
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_init_macsec_ops(struct osi_core_priv_data *const osi_core);

/**
 * @brief Initialize the macsec controller
 *
 * @note
 * Algorithm:
 * - Configure MTU, controller configs, interrupts, clear all LUT's and
 *    set BYP LUT entries for MKPDU and BC packets
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre
 * - MACSEC should be out of reset and clocks are enabled
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_init(struct osi_core_priv_data *const osi_core);

/**
 * @brief De-Initialize the macsec controller
 *
 * @note
 * Algorithm:
 * - Resets macsec global data structures
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre
 * - MACSEC TX/RX engine shall be disabled.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_deinit(struct osi_core_priv_data *const osi_core);

/**
 * @brief Non-secure irq handler.
 *
 * @note
 * Algorithm:
 *  - Takes care of handling the non secture interrupts accordingly as per
 *    the MACSEC IP
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MACSEC should be inited and enabled.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
 *
 * @note
 * Classification:
 * - Interrupt: Yes
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
 * @retval None
 */
void osi_macsec_ns_isr(struct osi_core_priv_data *const osi_core);

/**
 * @brief Secure irq handler
 *
 * @note
 * Algorithm:
 *  - Takes care of handling the secture interrupts accordingly as per
 *    the MACSEC IP
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MACSEC should be inited and enabled.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
 *
 * @note
 * Classification:
 * - Interrupt: Yes
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
 * @retval None
 */
void osi_macsec_s_isr(struct osi_core_priv_data *const osi_core);

/**
 * @brief MACSEC Lookup table configuration
 *
 * @note
 * Algorithm:
 * - Configures MACSEC LUT entry for BYP, SCI, SC PARAM, SC STATE, SA STATE
 *   table
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lut_config: OSI macsec LUT config data structure.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_lut_config(struct osi_core_priv_data *const osi_core,
			  struct osi_macsec_lut_config *const lut_confg);

/**
 * @brief MACSEC Key table configuration
 *
 * @note
 * Algorithm:
 * - Configures MACSEC Key Table entry
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] kt_config: OSI macsec Key table config data structure.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_kt_config(struct osi_core_priv_data *const osi_core,
			 struct osi_macsec_kt_config *const kt_config);

/**
 * @brief MACSEC cipher configuration
 *
 * @note
 * Algorithm:
 * - Configure MACSEC tx/rx controller cipther mode.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cipher: AES cipher to be configured to controller.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_cipher_config(struct osi_core_priv_data *const osi_core,
				 nveu32_t cipher);

/**
 * @brief MACSEC Loopback configuration
 *
 * @note
 * Algorithm:
 * - Configure MACSEC controller to loopback mode.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: Loopback enable/disable flag.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_loopback(struct osi_core_priv_data *const osi_core,
			nveu32_t enable);

/**
 * @brief MACSEC Controller Enable/Disable
 *
 * @note
 * Algorithm:
 * - Configure MACSEC controller to loopback mode.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: Loopback enable/disable flag.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_en(struct osi_core_priv_data *const osi_core,
		  nveu32_t enable);

/**
 * @brief MACSEC update secure channel/association in controller
 *
 * @note
 * Algorithm:
 * - Create/Delete/Update SC/AN in controller.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] sc: Pointer to osi_macsec_sc_info struct for the tx SA.
 * @param[in] enable: flag to indicate enable/disable for the Tx SA.
 * @param[out] kt_idx: Key table index to program SAK.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config(struct osi_core_priv_data *const osi_core,
		      struct osi_macsec_sc_info *const sc,
		      nveu32_t enable, nveu16_t ctlr,
		      nveu16_t *kt_idx);

/**
 * @brief MACSEC read statistics counters
 *
 * @note
 * Algorithm:
 * - Reads the MACSEC statistics counters
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_read_mmc(struct osi_core_priv_data *const osi_core);

/**
 * @brief MACSEC debug buffer configuration
 *
 * @note
 * Algorithm:
 * - Read or Write MACSEC debug buffers
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dbg_buf_config: OSI macsec debug buffer config data structure.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_dbg_buf_config(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config);

/**
 * @brief MACSEC debug events configuration
 *
 * @note
 * Algorithm:
 * - Configures MACSEC debug events to be triggered.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dbg_buf_config: OSI macsec debug buffer config data structure.
 *
 * @pre
 * - MACSEC shall be initialized and enalbed
 *
 * @note
 * Traceability Details:
 * - SWUD_ID:
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
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_dbg_events_config(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config);

#endif /* INCLUDED_OSI_MACSEC_H */
