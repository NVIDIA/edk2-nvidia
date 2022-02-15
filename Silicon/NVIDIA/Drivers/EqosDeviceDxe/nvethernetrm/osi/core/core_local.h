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

#ifndef INCLUDED_CORE_LOCAL_H
#define INCLUDED_CORE_LOCAL_H

#include <osi_core.h>
#include <local_common.h>

/**
 * @brief Maximum number of OSI core instances.
 */
#ifndef MAX_CORE_INSTANCES
#define MAX_CORE_INSTANCES	10U
#endif

/**
 * @brief Maximum number of interface operations.
 */
#define MAX_INTERFACE_OPS	2U

/**
 * @brief Maximum number of timestamps stored in OSI from HW FIFO.
 */
#define MAX_TX_TS_CNT		(PKT_ID_CNT * OSI_MGBE_MAX_NUM_CHANS)

/**
 * interface core ops
 */
struct if_core_ops {
	/** Interface function called to initialize MAC and MTL registers */
	nve32_t (*if_core_init)(struct osi_core_priv_data *const osi_core,
				nveu32_t tx_fifo_size, nveu32_t rx_fifo_size);
	/** Interface function called to deinitialize MAC and MTL registers */
	nve32_t (*if_core_deinit)(struct osi_core_priv_data *const osi_core);
	/** Interface function called to write into a PHY reg over MDIO bus */
	nve32_t (*if_write_phy_reg)(struct osi_core_priv_data *const osi_core,
				    const nveu32_t phyaddr,
				    const nveu32_t phyreg,
				    const nveu16_t phydata);
	/** Interface function called to read a PHY reg over MDIO bus */
	nve32_t (*if_read_phy_reg)(struct osi_core_priv_data *const osi_core,
				   const nveu32_t phyaddr,
				   const nveu32_t phyreg);
	/** Initialize Interface core operations */
	nve32_t (*if_init_core_ops)(struct osi_core_priv_data *const osi_core);
	/** Interface function called to handle runtime commands */
	nve32_t (*if_handle_ioctl)(struct osi_core_priv_data *osi_core,
				   struct osi_ioctl *data);
};

/**
 * @brief Initialize MAC & MTL core operations.
 */
struct core_ops {
	/** Called to poll for software reset bit */
	nve32_t (*poll_for_swr)(struct osi_core_priv_data *const osi_core);
	/** Called to initialize MAC and MTL registers */
	nve32_t (*core_init)(struct osi_core_priv_data *const osi_core,
			     const nveu32_t tx_fifo_size,
			     const nveu32_t rx_fifo_size);
	/** Called to deinitialize MAC and MTL registers */
	void (*core_deinit)(struct osi_core_priv_data *const osi_core);
	/**  Called to start MAC Tx and Rx engine */
	void (*start_mac)(struct osi_core_priv_data *const osi_core);
	/** Called to stop MAC Tx and Rx engine */
	void (*stop_mac)(struct osi_core_priv_data *const osi_core);
	/** Called to handle common interrupt */
	void (*handle_common_intr)(struct osi_core_priv_data *const osi_core);
	/** Called to set the mode at MAC (full/duplex) */
	nve32_t (*set_mode)(struct osi_core_priv_data *const osi_core,
			    const nve32_t mode);
	/** Called to set the speed at MAC */
	nve32_t (*set_speed)(struct osi_core_priv_data *const osi_core,
			     const nve32_t speed);
	/** Called to do pad caliberation */
	nve32_t (*pad_calibrate)(struct osi_core_priv_data *const osi_core);
	/** Called to configure MTL RxQ to forward the err pkt */
	nve32_t (*config_fw_err_pkts)(struct osi_core_priv_data *const osi_core,
				      const nveu32_t qinx,
				      const nveu32_t fw_err);
	/** Called to configure Rx Checksum offload engine */
	nve32_t (*config_rxcsum_offload)(
				struct osi_core_priv_data *const osi_core,
				const nveu32_t enabled);
	/** Called to config mac packet filter */
	nve32_t (*config_mac_pkt_filter_reg)(
				struct osi_core_priv_data *const osi_core,
				const struct osi_filter *filter);
	/** Called to update MAC address 1-127 */
	nve32_t (*update_mac_addr_low_high_reg)(
				struct osi_core_priv_data *const osi_core,
				const struct osi_filter *filter);
	/** Called to configure l3/L4 filter */
	nve32_t (*config_l3_l4_filter_enable)(
				struct osi_core_priv_data *const osi_core,
				const nveu32_t enable);
	/** Called to configure L3 filter */
	nve32_t (*config_l3_filters)(struct osi_core_priv_data *const osi_core,
				     const nveu32_t filter_no,
				     const nveu32_t enb_dis,
				     const nveu32_t ipv4_ipv6_match,
				     const nveu32_t src_dst_addr_match,
				     const nveu32_t perfect_inverse_match,
				     const nveu32_t dma_routing_enable,
				     const nveu32_t dma_chan);
	/** Called to update ip4 src or desc address */
	nve32_t (*update_ip4_addr)(struct osi_core_priv_data *const osi_core,
				   const nveu32_t filter_no,
				   const nveu8_t addr[],
				   const nveu32_t src_dst_addr_match);
	/** Called to update ip6 address */
	nve32_t (*update_ip6_addr)(struct osi_core_priv_data *const osi_core,
				   const nveu32_t filter_no,
				   const nveu16_t addr[]);
	/** Called to configure L4 filter */
	nve32_t (*config_l4_filters)(struct osi_core_priv_data *const osi_core,
				     const nveu32_t filter_no,
				     const nveu32_t enb_dis,
				     const nveu32_t tcp_udp_match,
				     const nveu32_t src_dst_port_match,
				     const nveu32_t perfect_inverse_match,
				     const nveu32_t dma_routing_enable,
				     const nveu32_t dma_chan);
	/** Called to update L4 Port for filter packet */
	nve32_t (*update_l4_port_no)(struct osi_core_priv_data *const osi_core,
				     const nveu32_t filter_no,
				     const nveu16_t port_no,
				     const nveu32_t src_dst_port_match);
	/** Called to set the addend value to adjust the time */
	nve32_t (*config_addend)(struct osi_core_priv_data *const osi_core,
				 const nveu32_t addend);
	/** Called to adjust the mac time */
	nve32_t (*adjust_mactime)(struct osi_core_priv_data *const osi_core,
				  const nveu32_t sec,
				  const nveu32_t nsec,
				  const nveu32_t neg_adj,
				  const nveu32_t one_nsec_accuracy);
	/** Called to set current system time to MAC */
	nve32_t (*set_systime_to_mac)(struct osi_core_priv_data *const osi_core,
				      const nveu32_t sec,
				      const nveu32_t nsec);
	/** Called to configure the TimeStampControl register */
	void (*config_tscr)(struct osi_core_priv_data *const osi_core,
			    const nveu32_t ptp_filter);
	/** Called to configure the sub second increment register */
	void (*config_ssir)(struct osi_core_priv_data *const osi_core,
			    const nveu32_t ptp_clock);
	/** Called to configure the PTP RX packets Queue */
	nve32_t (*config_ptp_rxq)(struct osi_core_priv_data *const osi_core,
				  const unsigned int rxq_idx,
				  const unsigned int enable);
	/** Called to update MMC counter from HW register */
	void (*read_mmc)(struct osi_core_priv_data *const osi_core);
	/** Called to write into a PHY reg over MDIO bus */
	nve32_t (*write_phy_reg)(struct osi_core_priv_data *const osi_core,
				 const nveu32_t phyaddr,
				 const nveu32_t phyreg,
				 const nveu16_t phydata);
	/** Called to read from a PHY reg over MDIO bus */
	nve32_t (*read_phy_reg)(struct osi_core_priv_data *const osi_core,
				const nveu32_t phyaddr,
				const nveu32_t phyreg);
	/** Called to read reg */
	nveu32_t (*read_reg)(struct osi_core_priv_data *const osi_core,
			     const nve32_t reg);
	/** Called to write reg */
	nveu32_t (*write_reg)(struct osi_core_priv_data *const osi_core,
			      const nveu32_t val,
			      const nve32_t reg);
#ifndef OSI_STRIPPED_LIB
	/** Called periodically to read and validate safety critical
	 * registers against last written value */
	nve32_t (*validate_regs)(struct osi_core_priv_data *const osi_core);
	/** Called to flush MTL Tx queue */
	nve32_t (*flush_mtl_tx_queue)(struct osi_core_priv_data *const osi_core,
				      const nveu32_t qinx);
	/** Called to set av parameter */
	nve32_t (*set_avb_algorithm)(struct osi_core_priv_data *const osi_core,
			   const struct osi_core_avb_algorithm *const avb);
	/** Called to get av parameter */
	nve32_t (*get_avb_algorithm)(struct osi_core_priv_data *const osi_core,
				     struct osi_core_avb_algorithm *const avb);
	/** Called to configure the MTL to forward/drop tx status */
	nve32_t (*config_tx_status)(struct osi_core_priv_data *const osi_core,
				    const nveu32_t tx_status);
	/** Called to configure the MAC rx crc */
	nve32_t (*config_rx_crc_check)(
				     struct osi_core_priv_data *const osi_core,
				     const nveu32_t crc_chk);
	/** Called to configure the MAC flow control */
	nve32_t (*config_flow_control)(
				     struct osi_core_priv_data *const osi_core,
				     const nveu32_t flw_ctrl);
	/** Called to enable/disable HW ARP offload feature */
	nve32_t (*config_arp_offload)(struct osi_core_priv_data *const osi_core,
				      const nveu32_t enable,
				      const nveu8_t *ip_addr);
	/** Called to configure VLAN filtering */
	nve32_t (*config_vlan_filtering)(
				     struct osi_core_priv_data *const osi_core,
				     const nveu32_t filter_enb_dis,
				     const nveu32_t perfect_hash_filtering,
				     const nveu32_t perfect_inverse_match);
	/** called to update VLAN id */
	nve32_t (*update_vlan_id)(struct osi_core_priv_data *const osi_core,
				  const nveu32_t vid);
	/** Called to reset MMC HW counter structure */
	void (*reset_mmc)(struct osi_core_priv_data *const osi_core);
	/** Called to configure EEE Tx LPI */
	void (*configure_eee)(struct osi_core_priv_data *const osi_core,
			      const nveu32_t tx_lpi_enabled,
			      const nveu32_t tx_lpi_timer);
	/** Called to save MAC register space during SoC suspend */
	nve32_t (*save_registers)(struct osi_core_priv_data *const osi_core);
	/** Called to restore MAC control registers during SoC resume */
	nve32_t (*restore_registers)(struct osi_core_priv_data *const osi_core);
	/** Called to set MDC clock rate for MDIO operation */
	void (*set_mdc_clk_rate)(struct osi_core_priv_data *const osi_core,
				 const nveu64_t csr_clk_rate);
	/** Called to configure MAC in loopback mode */
	nve32_t (*config_mac_loopback)(
				struct osi_core_priv_data *const osi_core,
				const nveu32_t lb_mode);
#endif /* !OSI_STRIPPED_LIB */
	/** Called to get HW features */
	nve32_t (*get_hw_features)(struct osi_core_priv_data *const osi_core,
				   struct osi_hw_features *hw_feat);
	/** Called to configure RSS for MAC */
	nve32_t (*config_rss)(struct osi_core_priv_data *osi_core);
	/** Called to update GCL config */
	int (*hw_config_est)(struct osi_core_priv_data *const osi_core,
			     struct osi_est_config *const est);
	/** Called to update FPE config */
	int (*hw_config_fpe)(struct osi_core_priv_data *const osi_core,
			struct osi_fpe_config *const fpe);
	/** Called to configure FRP engine */
	int (*config_frp)(struct osi_core_priv_data *const osi_core,
			  const unsigned int enabled);
	/** Called to update FRP Instruction Table entry */
	int (*update_frp_entry)(struct osi_core_priv_data *const osi_core,
				const unsigned int pos,
				struct osi_core_frp_data *const data);
	/** Called to update FRP NVE and  */
	int (*update_frp_nve)(struct osi_core_priv_data *const osi_core,
			      const unsigned int nve);
	/** Called to configure HW PTP offload feature */
	int (*config_ptp_offload)(struct osi_core_priv_data *const osi_core,
				  struct osi_pto_config *const pto_config);
#ifdef MACSEC_SUPPORT
	void (*config_macsec_ipg)(struct osi_core_priv_data *const osi_core,
				  const nveu32_t enable);
#endif /* MACSEC_SUPPORT */
	int (*ptp_tsc_capture)(struct osi_core_priv_data *const osi_core,
			       struct osi_core_ptp_tsc_data *data);
};


/**
 * @brief Core local data structure.
 */
struct core_local {
	/** OSI Core data variable */
	struct osi_core_priv_data osi_core;
	/** Core local operations variable */
	struct core_ops *ops_p;
	/** interface core local operations variable */
	struct if_core_ops *if_ops_p;
	/** structure to store tx time stamps */
	struct osi_core_tx_ts ts[MAX_TX_TS_CNT];
	/** Flag to represent initialization done or not */
	nveu32_t init_done;
	/** Flag to represent infterface initialization done or not */
	nveu32_t if_init_done;
	/** Magic number to validate osi core pointer */
	nveu64_t magic_num;
	/** This is the head node for PTP packet ID queue */
	struct osi_core_tx_ts tx_ts_head;
	/** Maximum number of queues/channels */
	nveu32_t max_chans;
	/** GCL depth supported by HW */
	nveu32_t gcl_dep;
	/** Max GCL width (time + gate) value supported by HW */
	nveu32_t gcl_width_val;
	/** TS lock */
	nveu32_t ts_lock;
};

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
void eqos_init_core_ops(struct core_ops *ops);

/**
 * @brief ivc_init_core_ops - Initialize IVC core operations.
 *
 * @param[in] ops: Core operations pointer.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void ivc_init_core_ops(struct core_ops *ops);

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
void mgbe_init_core_ops(struct core_ops *ops);

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
void ivc_init_macsec_ops(void *macsecops);

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
void hw_interface_init_core_ops(struct if_core_ops *if_ops_p);

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
void ivc_interface_init_core_ops(struct if_core_ops *if_ops_p);

#endif /* INCLUDED_CORE_LOCAL_H */
