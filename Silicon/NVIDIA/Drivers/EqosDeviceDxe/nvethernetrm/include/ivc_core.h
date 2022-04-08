/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef IVC_CORE_H
#define IVC_CORE_H

#include <osi_macsec.h>

/**
 * @brief Ethernet Maximum IVC BUF
 */
#define ETHER_MAX_IVC_BUF	2048

/**
 * @brief IVC maximum arguments
 */
#define MAX_ARGS	10

/**
 * @brief IVC commands between OSD & OSI.
 */
typedef enum ivc_cmd {
	core_init = 1,
	core_deinit,
	write_phy_reg,
	read_phy_reg,
	handle_ioctl,
	init_macsec,
	deinit_macsec,
	handle_ns_irq_macsec,
	handle_s_irq_macsec,
	lut_config_macsec,
	kt_config_macsec,
	cipher_config,
	loopback_config_macsec,
	en_macsec,
	config_macsec,
	read_mmc_macsec,
	dbg_buf_config_macsec,
	dbg_events_config_macsec,
}ivc_cmd;

/**
 * @brief IVC arguments structure.
 */
typedef struct ivc_args {
	/** Number of arguments */
	nveu32_t count;
	/** arguments */
	nveu32_t arguments[MAX_ARGS];
} ivc_args;

/**
 * @brief IVC core argument structure.
 */
typedef struct ivc_core_args {
	/** Number of MTL queues enabled in MAC */
	nveu32_t num_mtl_queues;
	/** Array of MTL queues */
	nveu32_t mtl_queues[OSI_EQOS_MAX_NUM_CHANS];
	/** List of MTL Rx queue mode that need to be enabled */
	nveu32_t rxq_ctrl[OSI_EQOS_MAX_NUM_CHANS];
	/** Rx MTl Queue mapping based on User Priority field */
	nveu32_t rxq_prio[OSI_EQOS_MAX_NUM_CHANS];
	/** Ethernet MAC address */
	nveu8_t mac_addr[OSI_ETH_ALEN];
	/** Tegra Pre-si platform info */
	nveu32_t pre_si;
	/** VLAN tag stripping enable(1) or disable(0) */
	nveu32_t strip_vlan_tag;
	/** pause frame support */
	nveu32_t pause_frames;
	/** Current flow control settings */
	nveu32_t flow_ctrl;
	/** Rx fifo size */
	nveu32_t rx_fifo_size;
	/** Tx fifo size */
	nveu32_t tx_fifo_size;
} ivc_core_args;

/**
 * @brief macsec config structure.
 */
#ifdef MACSEC_SUPPORT
typedef struct macsec_config {
	/** MACsec secure channel basic information */
	struct osi_macsec_sc_info sc_info;
	/** MACsec enable or disable */
	unsigned int enable;
	/** MACsec controller */
	unsigned short ctlr;
	/** MACsec KT index */
	unsigned short kt_idx;
} macsec_config;
#endif

/**
 * @brief IVC message structure.
 */
typedef struct ivc_msg_common {
	/**
	 * Status code returned as part of response message of IVC messages.
	 * Status code value is "0" for success and "< 0" for failure.
	 */
	nve32_t status;
	/** ID of the CMD. */
	ivc_cmd cmd;
	/** message count, used for debug */
	nveu32_t count;

	union {
		/** IVC argument structure */
		ivc_args args;
#ifndef OSI_STRIPPED_LIB
		/** avb algorithm structure */
		struct osi_core_avb_algorithm avb_algo;
#endif
		/** OSI filter structure */
		struct osi_filter filter;
		/** OSI HW features */
		struct osi_hw_features hw_feat;
		/** MMC counters */
		struct osi_mmc_counters mmc;
		/** core argument structure */
		ivc_core_args init_args;
		/** ioctl command structure */
		struct osi_ioctl ioctl_data;
#ifdef MACSEC_SUPPORT
		/** lut config */
		struct osi_macsec_lut_config lut_config;
#ifdef MACSEC_KEY_PROGRAM
		/** kt config */
		struct osi_macsec_kt_config kt_config;
#endif
		/** MACsec Debug buffer data structure */
		struct osi_macsec_dbg_buf_config dbg_buf_config;
		/** MACsec config */
		macsec_config macsec_cfg;
		/** macsec mmc counters */
		struct osi_macsec_mmc_counters macsec_mmc;
#endif
	}data;
} ivc_msg_common_t;

/**
 * @brief osd_ivc_send_cmd - OSD ivc send cmd
 *
 * @param[in] priv: OSD private data
 * @param[in] ivc_buf: ivc_msg_common structure
 * @param[in] len: length of data
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval ivc status
 * @retval -1 on failure
 */
nve32_t osd_ivc_send_cmd(void *priv, ivc_msg_common_t *ivc_buf,
			 nveu32_t len);

/**
 * @brief ivc_get_core_safety_config - Get core safety config
 *
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 */
void *ivc_get_core_safety_config(void);
#endif /* IVC_CORE_H */
