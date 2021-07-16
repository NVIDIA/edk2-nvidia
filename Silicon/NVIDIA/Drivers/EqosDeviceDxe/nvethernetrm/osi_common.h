/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
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
 *
 *  Portions provided under the following terms:
 *  Copyright (c) 2018-2019 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 *  property and proprietary rights in and to this material, related
 *  documentation and any modifications thereto. Any use, reproduction,
 *  disclosure or distribution of this material and related documentation
 *  without an express license agreement from NVIDIA CORPORATION or
 *  its affiliates is strictly prohibited.
 *
 *  SPDX-FileCopyrightText: Copyright (c) 2018-2019 NVIDIA CORPORATION & AFFILIATES
 *  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 */

#ifndef OSI_COMMON_H
#define OSI_COMMON_H

/**
 * @addtogroup EQOS-Helper Helper MACROS
 * @{
 */
#define OSI_UNLOCKED		0x0U
#define OSI_LOCKED		0x1U
#define TEN_POWER_9		0x3B9ACA00U
#define TWO_POWER_32		0x100000000ULL
#define TWO_POWER_31		0x80000000U
#define OSI_NSEC_PER_SEC	1000000000ULL
#define OSI_INVALID_VALUE	0xFFFFFFFFU

/* System clock is 62.5MHz */
#define OSI_ETHER_SYSCLOCK		62500000U
#define OSI_ONE_MEGA_HZ			1000000U
#define OSI_MAX_RX_COALESCE_USEC	1020U
#define OSI_MIN_RX_COALESCE_USEC	3U

#define OSI_PAUSE_FRAMES_ENABLE		0U
#define OSI_PAUSE_FRAMES_DISABLE	1U
#define OSI_FLOW_CTRL_TX		OSI_BIT(0)
#define OSI_FLOW_CTRL_RX		OSI_BIT(1)
#define OSI_FLOW_CTRL_DISABLE		0U

#define OSI_ADDRESS_32BIT		0
#define OSI_ADDRESS_40BIT		1
#define OSI_ADDRESS_48BIT		2

#ifndef ULONG_MAX
#define ULONG_MAX			(~0UL)
#endif
#ifndef UINT_MAX
#define UINT_MAX			(~0U)
#endif
#ifndef INT_MAX
#define INT_MAX				(0x7FFFFFFF)
#endif

/* MAC Time stamp contorl reg bit fields */
#define OSI_MAC_TCR_TSENA		OSI_BIT(0)
#define OSI_MAC_TCR_TSCFUPDT		OSI_BIT(1)
#define OSI_MAC_TCR_TSENALL		OSI_BIT(8)
#define OSI_MAC_TCR_TSCTRLSSR		OSI_BIT(9)
#define OSI_MAC_TCR_TSVER2ENA		OSI_BIT(10)
#define OSI_MAC_TCR_TSIPENA		OSI_BIT(11)
#define OSI_MAC_TCR_TSIPV6ENA		OSI_BIT(12)
#define OSI_MAC_TCR_TSIPV4ENA		OSI_BIT(13)
#define OSI_MAC_TCR_TSEVENTENA		OSI_BIT(14)
#define OSI_MAC_TCR_TSMASTERENA		OSI_BIT(15)
#define OSI_MAC_TCR_SNAPTYPSEL_1	OSI_BIT(16)
#define OSI_MAC_TCR_SNAPTYPSEL_2	OSI_BIT(17)
#define OSI_MAC_TCR_SNAPTYPSEL_3	(OSI_BIT(16) | OSI_BIT(17))
#define OSI_MAC_TCR_AV8021ASMEN		OSI_BIT(28)

#define OSI_ULLONG_MAX			(~0ULL)
#define OSI_UCHAR_MAX			(0xFFU)

/* Default maximum Gaint Packet Size Limit */
#define OSI_MAX_MTU_SIZE	9000U
#define OSI_DFLT_MTU_SIZE	1500U
#define OSI_MTU_SIZE_2K		2048U
#define OSI_MTU_SIZE_4K		4096U
#define OSI_MTU_SIZE_8K		8192U
#define OSI_MTU_SIZE_16K	16384U

#define EQOS_DMA_CHX_STATUS(x)		((0x0080U * (x)) + 0x1160U)
#define EQOS_DMA_CHX_IER(x)		((0x0080U * (x)) + 0x1134U)

/* FIXME add logic based on HW version */
#define EQOS_MAX_MAC_ADDRESS_FILTER		128U
#define EQOS_MAX_L3_L4_FILTER			8U
#define OSI_EQOS_MAX_NUM_CHANS	4U
#define OSI_EQOS_MAX_NUM_QUEUES	4U
/* HW supports 8 Hash table regs, but eqos_validate_core_regs only checks 4 */
#define OSI_EQOS_MAX_HASH_REGS	4U


#define MAC_VERSION		0x110
#define MAC_VERSION_SNVER_MASK	0x7FU

#define OSI_MAC_HW_EQOS		0U
#define OSI_ETH_ALEN		6U

#define OSI_NULL                ((void *)0)
#define OSI_ENABLE		1U
#define OSI_DISABLE		0U
#define OSI_AMASK_DISABLE	0U

#define OSI_HASH_FILTER_MODE	1U
#define OSI_PERFECT_FILTER_MODE	0U
#define OSI_IPV6_MATCH		1U
#define OSI_SOURCE_MATCH	0U

#define OSI_SA_MATCH		1U
#define OSI_DA_MATCH		0U


#define OSI_L4_FILTER_TCP	0U
#define OSI_L4_FILTER_UDP	1U

#define OSI_IP4_FILTER		0U
#define OSI_IP6_FILTER		1U

#define CHECK_CHAN_BOUND(chan)						\
	{								\
		if ((chan) >= OSI_EQOS_MAX_NUM_CHANS) {			\
			return;						\
		}							\
	}								\

#define OSI_BIT(nr)             ((unsigned int)1 << (nr))

#define OSI_EQOS_MAC_4_10       0x41U
#define OSI_EQOS_MAC_5_00       0x50U
#define OSI_EQOS_MAC_5_10       0x51U

#define OSI_SPEED_10		10
#define OSI_SPEED_100		100
#define OSI_SPEED_1000		1000

#define OSI_FULL_DUPLEX		1
#define OSI_HALF_DUPLEX		0

#define NV_ETH_FRAME_LEN   1514U
#define NV_ETH_FCS_LEN	0x4U
#define NV_VLAN_HLEN		0x4U

#define MAX_ETH_FRAME_LEN_DEFAULT \
	(NV_ETH_FRAME_LEN + NV_ETH_FCS_LEN + NV_VLAN_HLEN)

#define L32(data)       ((data) & 0xFFFFFFFFU)
#define H32(data)       (((data) & 0xFFFFFFFF00000000UL) >> 32UL)

#define OSI_INVALID_CHAN_NUM    0xFFU
/** @} */

/**
 * @addtogroup EQOS-MAC EQOS MAC HW supported features
 *
 * @brief Helps in identifying the features that are set in MAC HW
 * @{
 */
#define EQOS_MAC_HFR0		0x11c
#define EQOS_MAC_HFR1		0x120
#define EQOS_MAC_HFR2		0x124

#define EQOS_MAC_HFR0_MIISEL_MASK	0x1U
#define EQOS_MAC_HFR0_GMIISEL_MASK	0x1U
#define EQOS_MAC_HFR0_HDSEL_MASK	0x1U
#define EQOS_MAC_HFR0_PCSSEL_MASK	0x1U
#define EQOS_MAC_HFR0_SMASEL_MASK	0x1U
#define EQOS_MAC_HFR0_RWKSEL_MASK	0x1U
#define EQOS_MAC_HFR0_MGKSEL_MASK	0x1U
#define EQOS_MAC_HFR0_MMCSEL_MASK	0x1U
#define EQOS_MAC_HFR0_ARPOFFLDEN_MASK	0x1U
#define EQOS_MAC_HFR0_TSSSEL_MASK	0x1U
#define EQOS_MAC_HFR0_EEESEL_MASK	0x1U
#define EQOS_MAC_HFR0_TXCOESEL_MASK	0x1U
#define EQOS_MAC_HFR0_RXCOE_MASK	0x1U
#define EQOS_MAC_HFR0_ADDMACADRSEL_MASK	0x1fU
#define EQOS_MAC_HFR0_MACADR32SEL_MASK	0x1U
#define EQOS_MAC_HFR0_MACADR64SEL_MASK	0x1U
#define EQOS_MAC_HFR0_TSINTSEL_MASK	0x3U
#define EQOS_MAC_HFR0_SAVLANINS_MASK	0x1U
#define EQOS_MAC_HFR0_ACTPHYSEL_MASK	0x7U
#define EQOS_MAC_HFR1_RXFIFOSIZE_MASK	0x1fU
#define EQOS_MAC_HFR1_TXFIFOSIZE_MASK	0x1fU
#define EQOS_MAC_HFR1_ADVTHWORD_MASK	0x1U
#define EQOS_MAC_HFR1_ADDR64_MASK	0x3U
#define EQOS_MAC_HFR1_DCBEN_MASK	0x1U
#define EQOS_MAC_HFR1_SPHEN_MASK	0x1U
#define EQOS_MAC_HFR1_TSOEN_MASK	0x1U
#define EQOS_MAC_HFR1_DMADEBUGEN_MASK	0x1U
#define EQOS_MAC_HFR1_AVSEL_MASK	0x1U
#define EQOS_MAC_HFR1_LPMODEEN_MASK	0x1U
#define EQOS_MAC_HFR1_HASHTBLSZ_MASK	0x3U
#define EQOS_MAC_HFR1_L3L4FILTERNUM_MASK	0xfU
#define EQOS_MAC_HFR2_RXQCNT_MASK	0xfU
#define EQOS_MAC_HFR2_TXQCNT_MASK	0xfU
#define EQOS_MAC_HFR2_RXCHCNT_MASK	0xfU
#define EQOS_MAC_HFR2_TXCHCNT_MASK	0xfU
#define EQOS_MAC_HFR2_PPSOUTNUM_MASK	0x7U
#define EQOS_MAC_HFR2_AUXSNAPNUM_MASK	0x7U
/** @} */

/**
 * @brief struct osi_hw_features - MAC HW supported features.
 */
struct osi_hw_features {
	/** It is set to 1 when 10/100 Mbps is selected as the Mode of
	 * Operation */
	unsigned int mii_sel;
	/** It sets to 1 when 1000 Mbps is selected as the Mode of Operation */
	unsigned int gmii_sel;
	/** It sets to 1 when the half-duplex mode is selected */
	unsigned int hd_sel;
	/** It sets to 1 when the TBI, SGMII, or RTBI PHY interface
	 * option is selected */
	unsigned int pcs_sel;
	/** It sets to 1 when the Enable VLAN Hash Table Based Filtering
	 * option is selected */
	unsigned int vlan_hash_en;
	/** It sets to 1 when the Enable Station Management (MDIO Interface)
	 * option is selected */
	unsigned int sma_sel;
	/** It sets to 1 when the Enable Remote Wake-Up Packet Detection
	 * option is selected */
	unsigned int rwk_sel;
	/** It sets to 1 when the Enable Magic Packet Detection option is
	 * selected */
	unsigned int mgk_sel;
	/** It sets to 1 when the Enable MAC Management Counters (MMC) option
	 * is selected */
	unsigned int mmc_sel;
	/** It sets to 1 when the Enable IPv4 ARP Offload option is selected */
	unsigned int arp_offld_en;
	/** It sets to 1 when the Enable IEEE 1588 Timestamp Support option
	 * is selected */
	unsigned int ts_sel;
	/** It sets to 1 when the Enable Energy Efficient Ethernet (EEE) option
	 * is selected */
	unsigned int eee_sel;
	/** It sets to 1 when the Enable Transmit TCP/IP Checksum Insertion
	 * option is selected */
	unsigned int tx_coe_sel;
	/** It sets to 1 when the Enable Receive TCP/IP Checksum Check option
	 * is selected */
	unsigned int rx_coe_sel;
	/** It sets to 1 when the Enable Additional 1-31 MAC Address Registers
	 * option is selected */
	unsigned int mac_addr16_sel;
	/** It sets to 1 when the Enable Additional 32-63 MAC Address Registers
	 * option is selected */
	unsigned int mac_addr32_sel;
	/** It sets to 1 when the Enable Additional 64-127 MAC Address Registers
	 * option is selected */
	unsigned int mac_addr64_sel;
	/** It sets to 1 when the Enable IEEE 1588 Timestamp Support option
	 * is selected */
	unsigned int tsstssel;
	/** It sets to 1 when the Enable SA and VLAN Insertion on Tx option
	 * is selected */
	unsigned int sa_vlan_ins;
	/** Active PHY Selected
	 * When you have multiple PHY interfaces in your configuration,
	 * this field indicates the sampled value of phy_intf_sel_i during
	 * reset de-assertion:
	 * 000: GMII or MII
	 * 001: RGMII
	 * 010: SGMII
	 * 011: TBI
	 * 100: RMII
	 * 101: RTBI
	 * 110: SMII
	 * 111: RevMII
	 * All Others: Reserved */
	unsigned int act_phy_sel;
	/** MTL Receive FIFO Size
	 * This field contains the configured value of MTL Rx FIFO in bytes
	 * expressed as Log to base 2 minus 7, that is, Log2(RXFIFO_SIZE) -7:
	 * 00000: 128 bytes
	 * 00001: 256 bytes
	 * 00010: 512 bytes
	 * 00011: 1,024 bytes
	 * 00100: 2,048 bytes
	 * 00101: 4,096 bytes
	 * 00110: 8,192 bytes
	 * 00111: 16,384 bytes
	 * 01000: 32,767 bytes
	 * 01000: 32 KB
	 * 01001: 64 KB
	 * 01010: 128 KB
	 * 01011: 256 KB
	 * 01100-11111: Reserved */
	unsigned int rx_fifo_size;
	/** MTL Transmit FIFO Size.
	 * This field contains the configured value of MTL Tx FIFO in
	 * bytes expressed as Log to base 2 minus 7, that is,
	 * Log2(TXFIFO_SIZE) -7:
	 * 00000: 128 bytes
	 * 00001: 256 bytes
	 * 00010: 512 bytes
	 * 00011: 1,024 bytes
	 * 00100: 2,048 bytes
	 * 00101: 4,096 bytes
	 * 00110: 8,192 bytes
	 * 00111: 16,384 bytes
	 * 01000: 32 KB
	 * 01001: 64 KB
	 * 01010: 128 KB
	 * 01011-11111: Reserved */
	unsigned int tx_fifo_size;
	/** It set to 1 when Advance timestamping High Word selected */
	unsigned int adv_ts_hword;
	/** Address Width.
	 * This field indicates the configured address width:
	 * 00: 32
	 * 01: 40
	 * 10: 48
	 * 11: Reserved */
	unsigned int addr_64;
	/** It sets to 1 when DCB Feature Enable */
	unsigned int dcb_en;
	/** It sets to 1 when Split Header Feature Enable */
	unsigned int sph_en;
	/** It sets to 1 when TCP Segmentation Offload Enable */
	unsigned int tso_en;
	/** It sets to 1 when DMA debug registers are enabled */
	unsigned int dma_debug_gen;
	/** It sets to 1 if AV Feature Enabled */
	unsigned int av_sel;
	/** This field indicates the size of the hash table:
	 * 00: No hash table
	 * 01: 64
	 * 10: 128
	 * 11: 256 */
	unsigned int hash_tbl_sz;
	/** This field indicates the total number of L3 or L4 filters:
	 * 0000: No L3 or L4 Filter
	 * 0001: 1 L3 or L4 Filter
	 * 0010: 2 L3 or L4 Filters
	 * ..
	 * 1000: 8 L3 or L4 */
	unsigned int l3l4_filter_num;
	/** It holds number of MTL Receive Queues */
	unsigned int rx_q_cnt;
	/** It holds number of MTL Transmit Queues */
	unsigned int tx_q_cnt;
	/** It holds number of DMA Receive channels */
	unsigned int rx_ch_cnt;
	/** This field indicates the number of DMA Transmit channels:
	 * 0000: 1 DMA Tx Channel
	 * 0001: 2 DMA Tx Channels
	 * ..
	 * 0111: 8 DMA Tx */
	unsigned int tx_ch_cnt;
	/** This field indicates the number of PPS outputs:
	 * 000: No PPS output
	 * 001: 1 PPS output
	 * 010: 2 PPS outputs
	 * 011: 3 PPS outputs
	 * 100: 4 PPS outputs
	 * 101-111: Reserved */
	unsigned int pps_out_num;
	/** Number of Auxiliary Snapshot Inputs
	 * This field indicates the number of auxiliary snapshot inputs:
	 * 000: No auxiliary input
	 * 001: 1 auxiliary input
	 * 010: 2 auxiliary inputs
	 * 011: 3 auxiliary inputs
	 * 100: 4 auxiliary inputs
	 * 101-111: Reserved */
	unsigned int aux_snap_num;
};

/**
 * @brief osi_lock_init - Initialize lock to unlocked state.
 *
 * Algorithm: Set lock to unlocked state.
 *
 * @param[in] lock - Pointer to lock to be initialized
 */
static inline void osi_lock_init(unsigned int *lock)
{
	*lock = OSI_UNLOCKED;
}

/**
 * @brief osi_lock_irq_enabled - Spin lock. Busy loop till lock is acquired.
 *
 * Algorithm: Atomic compare and swap operation till lock is held.
 *
 * @param[in] lock - Pointer to lock to be acquired.
 *
 * @note Does not disable irq. Do not call this API to acquire any
 *	lock that is shared between top/bottom half. It will result in deadlock.
 */
static inline void osi_lock_irq_enabled(unsigned int *lock)
{
	/* __sync_val_compare_and_swap(lock, old value, new value) returns the
	 * old value if successful.
	 */
	while (__sync_val_compare_and_swap(lock, OSI_UNLOCKED, OSI_LOCKED) !=
	      OSI_UNLOCKED) {
		/* Spinning.
		 * Will deadlock if any ISR tried to lock again.
		 */
	}
}

/**
 * @brief osi_unlock_irq_enabled - Release lock.
 *
 * Algorithm: Atomic compare and swap operation to release lock.
 *
 * @param[in] lock - Pointer to lock to be released.
 *
 * @note Does not disable irq. Do not call this API to release any
 *	lock that is shared between top/bottom half.
 */
static inline void osi_unlock_irq_enabled(unsigned int *lock)
{
	if (__sync_val_compare_and_swap(lock, OSI_LOCKED, OSI_UNLOCKED) !=
	    OSI_LOCKED) {
		/* Do nothing. Already unlocked */
	}
}

/**
 * @brief osi_readl - Read a memory mapped register.
 *
 * @param[in] addr: Memory mapped address.
 *
 * @note Physical address has to be memmory mapped.
 *
 * @return Data from memory mapped register - success.
 */
static inline unsigned int osi_readl(void *addr)
{
	return *(volatile unsigned int *)addr;
}

/**
 * @brief osi_writel - Write to a memory mapped register.
 *
 * @param[in] val:  Value to be written.
 * @param[in] addr: Memory mapped address.
 *
 * @note Physical address has to be memmory mapped.
 */
static inline void osi_writel(unsigned int val, void *addr)
{
	*(volatile unsigned int *)addr = val;
}

/**
 * @brief is_valid_mac_version - Check if read MAC IP is valid or not.
 *
 * @param[in] mac_ver: MAC version read.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 - for not Valid MAC
 * @retval 1 - for Valid MAC
 */
static inline int is_valid_mac_version(unsigned int mac_ver)
{
	if ((mac_ver == OSI_EQOS_MAC_4_10) ||
	    (mac_ver == OSI_EQOS_MAC_5_00) ||
	    (mac_ver == OSI_EQOS_MAC_5_10)) {
		return 1;
	}

	return 0;
}

/**
 * @brief osi_update_stats_counter - update value by increment passed
 *	as parameter
 *
 * Algorithm: Check for boundary and return sum
 *
 * @param[in] last_value: last value of stat counter
 * @param[in] incr: increment value
 *
 * @note Input parameter should be only unsigned long type
 *
 * @return unsigned long value
 */
static inline unsigned long osi_update_stats_counter(unsigned long last_value,
						     unsigned long incr)
{
	unsigned long long temp;

	temp = (unsigned long long)last_value;
	temp = temp + incr;
	if (temp > ULONG_MAX) {
		/* Do nothing */
	} else {
		return (unsigned long)temp;
	}

	return last_value;
}

/**
 * @brief osi_get_mac_version - Reading MAC version
 *
 * Algorithm: Reads MAC version and check whether its valid or not.
 *
 * @param[in] addr: io-remap MAC base address.
 * @param[in] mac_ver: holds mac version.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_get_mac_version(void *addr, unsigned int *mac_ver);

/**
 * @brief osi_get_hw_features - Reading MAC HW features
 *
 * @param[in] base: io-remap MAC base address.
 * @param[in] hw_feat: holds the supported features of the hardware.
 *
 * @note MAC has to be out of reset.
 */
void osi_get_hw_features(void *base, struct osi_hw_features *hw_feat);
/**
 * @brief osi_memset - osi memset
 *
 * @param[in] s: source that need to be set
 * @param[in] c: value to fill in source
 * @param[in] count: first n bytes of source
 *
 */
void osi_memset(void *s, unsigned int c, unsigned long count);
#endif /* OSI_COMMON_H */
