/*
 * SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_OSI_CORE_H
#define INCLUDED_OSI_CORE_H

#include "nvethernetrm_export.h"
#include "nvethernetrm_l3l4.h"
#include <osi_common.h>
#include "mmc.h"

#ifdef OSI_RM_FTRACE
  #ifdef __QNX__
    #include <sys/slog.h>
#define ethernet_server_entry_log()      do {\
                        slogf(0, 2, "%s : Function Entry\n", __func__); \
} while (0)
#define ethernet_server_exit_log()       do {\
                        slogf(0, 2, "%s : Function Exit\n", __func__); \
} while (0)
#define ethernet_server_cmd_log(string)  do {\
                        slogf(0, 2, "%s: command is %s \n", __func__, string); \
} while (0)

  #else

    #include <common-c.h>
#define ethernet_server_entry_log()  do {\
                        RtosCWrapStringLog(__func__); \
                        RtosCWrapStringLog(": Function Entry"); \
                        RtosCWrapStringLog("\n"); \
        } while (0)

#define ethernet_server_exit_log()  do {\
                        RtosCWrapStringLog(__func__); \
                        RtosCWrapStringLog(": Function Exit"); \
                        RtosCWrapStringLog("\n"); \
        } while (0)

#define ethernet_server_cmd_log(string)  do {\
                        RtosCWrapStringLog(__func__); \
                        RtosCWrapStringLog(": command is " string ""); \
                        RtosCWrapStringLog("\n"); \
        } while (0)
  #endif //__QNX__
#endif //OSI_RM_FTRACE

struct ivc_msg_common;

/**
 * @addtogroup typedef related info
 *
 * @brief typedefs that indicate size and signness
 * @{
 */
/* Following added to avoid misraC 4.6
 * Here we are defining intermediate type
 */
/** intermediate type for long long */
typedef long long my_lint_64;

/** typedef equivalent to long long */
typedef my_lint_64 nvel64_t;
/** @} */

#ifndef OSI_STRIPPED_LIB
#define OSI_OPER_EN_L2_DA_INV   OSI_BIT(4)
#define OSI_OPER_DIS_L2_DA_INV  OSI_BIT(5)
#define OSI_PTP_SNAP_TRANSPORT  1U
#define OSI_VLAN_ACTION_DEL     0x0U
#define OSI_VLAN_ACTION_ADD     OSI_BIT(31)
#define OSI_RXQ_ROUTE_PTP       0U
#define EQOS_MAX_HTR_REGS       8U

/**
 * @addtogroup RSS related information
 *
 * @brief RSS hash key and table size.
 * @{
 */
#define OSI_RSS_HASH_KEY_SIZE   40U
#define OSI_RSS_MAX_TABLE_SIZE  128U
/** @} */

#define OSI_CMD_RESET_MMC            12U
#define OSI_CMD_MAC_LB               14U
#define OSI_CMD_FLOW_CTRL            15U
#define OSI_CMD_CONFIG_TXSTATUS      27U
#define OSI_CMD_CONFIG_RX_CRC_CHECK  25U
#define OSI_CMD_CONFIG_EEE           32U
#define OSI_CMD_ARP_OFFLOAD          30U
#define OSI_CMD_UPDATE_VLAN_ID       26U
#define OSI_CMD_VLAN_FILTER          31U
#define OSI_CMD_CONFIG_PTP_OFFLOAD   34U
#define OSI_CMD_PTP_RXQ_ROUTE        35U
#define OSI_CMD_CONFIG_RSS           37U
#define OSI_CMD_CONFIG_FW_ERR        29U
#define OSI_CMD_SET_MODE             16U
#define OSI_CMD_POLL_FOR_MAC_RST     4U
#define OSI_CMD_GET_MAC_VER          10U

/**
 * @addtogroup PTP-offload PTP offload defines
 * @{
 */
#define OSI_PTP_MAX_PORTID     0xFFFFU
#define OSI_PTP_MAX_DOMAIN     0xFFU
#define OSI_PTP_SNAP_ORDINARY  0U
#define OSI_PTP_SNAP_P2P       3U
/** @} */

#define OSI_MAC_TCR_TSMASTERENA   OSI_BIT(15)
#define OSI_MAC_TCR_TSEVENTENA    OSI_BIT(14)
#define OSI_MAC_TCR_TSENALL       OSI_BIT(8)
#define OSI_MAC_TCR_SNAPTYPSEL_3  (OSI_BIT(16) | OSI_BIT(17))
#define OSI_MAC_TCR_SNAPTYPSEL_2  OSI_BIT(17)
#define OSI_MAC_TCR_CSC           OSI_BIT(19)
#define OSI_MAC_TCR_AV8021ASMEN   OSI_BIT(28)

#define OSI_INSTANCE_ID_MBGE0  0
#define OSI_INSTANCE_ID_MGBE1  1
#define OSI_INSTANCE_ID_MGBE2  2
#define OSI_INSTANCE_ID_MGBE3  3
#define OSI_INSTANCE_ID_EQOS   4

#endif /* !OSI_STRIPPED_LIB */

/**
 * @addtogroup XPCS related defines
 * @{
 */
#define XPCS_REG_ADDR_SHIFT  10U
#define XPCS_REG_ADDR_MASK   0x1FFFU
#define XPCS_ADDRESS         0x03FC
#define XPCS_REG_VALUE_MASK  0x3FFU
#ifndef OSI_STRIPPED_LIB
#define XPCS_VR_XS_PCS_EEE_MCTRL0  0xE0018
#define XPCS_VR_XS_PCS_EEE_MCTRL1  0xE002C
#define XLGPCS_VR_PCS_EEE_MCTRL    0xe0018
#define XLGPCS_VR_PCS_DIG_STS      0xe0040

#define XPCS_VR_XS_PCS_EEE_MCTRL1_TRN_LPI  OSI_BIT(0)
#define XPCS_VR_XS_PCS_EEE_MCTRL0_LTX_EN   OSI_BIT(0)
#define XPCS_VR_XS_PCS_EEE_MCTRL0_LRX_EN   OSI_BIT(1)
#define XLGPCS_VR_PCS_DIG_STSLTXRX_STATE   (OSI_BIT(15) | OSI_BIT(14) |      \
                                                OSI_BIT(13) | OSI_BIT(12) | \
                                                OSI_BIT(11) | OSI_BIT(10))

#endif /* !OSI_STRIPPED_LIB */
/** @} */

/**
 * @addtogroup Status readback related defines
 * @{
 */
#define COND_MET      0
#define COND_NOT_MET  1
#define RETRY_ONCE    1U
/* 7usec is minimum to use usleep, anything less should use udelay, set to 10us */
#define MIN_USLEEP_10US  10U
/** @} */

#ifdef MACSEC_SUPPORT

/**
 * @addtogroup MACSEC related helper MACROs
 *
 * @brief MACSEC generic helper MACROs
 * @{
 */
/**
 * @brief Maximum number of Secure Channels
 */
#define OSI_MAX_NUM_SC       8U
#define OSI_MAX_NUM_SC_T26x  48U

/**
 * @brief MACSEC Secure Channel Identifier length
 */
#define OSI_SCI_LEN  8U

/**
 * @brief MACSEC Key length for AES_128 Algorithm
 */
#define OSI_KEY_LEN_128  16U

/**
 * @brief MACSEC Key length for AES_256 Algorithm
 */
#define OSI_KEY_LEN_256  32U

/**
 * @brief Number of MACSEC Controllers
 */
#define OSI_NUM_CTLR  2U
/** @} */
#endif /* MACSEC_SUPPORT */

/**
 * @addtogroup PTP PTP related information
 *
 * @brief PTP MAC-to-MAC sync role
 * @{
 */
/** @brief PTP MAC to MAC is disabled */
#define OSI_PTP_M2M_INACTIVE  0U
/** @brief PTP MAC to MAC is primary role */
#define OSI_PTP_M2M_PRIMARY  1U
/** @brief PTP MAC to MAC is secondary role */
#define OSI_PTP_M2M_SECONDARY  2U
/** @} */

/**
 * @addtogroup EQOS_PTP PTP Helper MACROS
 *
 * @brief EQOS PTP MAC Time stamp control reg bit fields
 * @{
 */
#define OSI_MAC_TCR_TSENA         OSI_BIT(0)
#define OSI_MAC_TCR_TSCFUPDT      OSI_BIT(1)
#define OSI_MAC_TCR_TSCTRLSSR     OSI_BIT(9)
#define OSI_MAC_TCR_TSVER2ENA     OSI_BIT(10)
#define OSI_MAC_TCR_TSIPENA       OSI_BIT(11)
#define OSI_MAC_TCR_TSIPV6ENA     OSI_BIT(12)
#define OSI_MAC_TCR_TSIPV4ENA     OSI_BIT(13)
#define OSI_MAC_TCR_SNAPTYPSEL_1  OSI_BIT(16)
#define OSI_MAC_TCR_TXTSSMIS      OSI_BIT(31)
/** @} */

/**
 * @addtogroup Helper MACROS
 *
 * @brief EQOS generic helper MACROS.
 * @{
 */
#define EQOS_MAX_MAC_ADDRESS_FILTER           128U
#define EQOS_MAX_MAC_5_3_ADDRESS_FILTER       32U
#define EQOS_MAX_L3_L4_FILTER                 8U
#define OSI_MGBE_MAX_MAC_ADDRESS_FILTER       32U
#define OSI_MGBE_MAX_MAC_ADDRESS_FILTER_T26X  48U
#define OSI_DA_MATCH                          0U
#ifndef OSI_STRIPPED_LIB
#define OSI_INV_MATCH  1U
#endif /* !OSI_STRIPPED_LIB */
#define OSI_AMASK_DISABLE  0U
#define OSI_CHAN_ANY       0xFFU
/** @brief default MTU supported */
#define OSI_DFLT_MTU_SIZE  1500U
/** @brief MTU as 9k */
#define OSI_MTU_SIZE_9000  9000U
/* Reg ETHER_QOS_AUTO_CAL_CONFIG_0[AUTO_CAL_PD/PU_OFFSET] max value */
#define OSI_PAD_CAL_CONFIG_PD_PU_OFFSET_MAX  0x1FU

#ifndef OSI_STRIPPED_LIB
/* HW supports 8 Hash table regs, but eqos_validate_core_regs only checks 4 */
#define OSI_EQOS_MAX_HASH_REGS  4U
#endif /* !OSI_STRIPPED_LIB */

/** @brief enable flow control for Tx */
#define OSI_FLOW_CTRL_TX  OSI_BIT(0)
/** @brief enable flow control for Rx */
#define OSI_FLOW_CTRL_RX  OSI_BIT(1)

#define OSI_FULL_DUPLEX  1
#define OSI_HALF_DUPLEX  0

/* L2 filter operations supported by OSI layer. These operation modes shall be
 * set by OSD driver as input to update registers accordingly.
 */
/** @brief enables promiscous mode */
#define OSI_OPER_EN_PROMISC  OSI_BIT(0)
/** @brief disables promiscous mode */
#define OSI_OPER_DIS_PROMISC  OSI_BIT(1)
/** @brief enable all multicast mode */
#define OSI_OPER_EN_ALLMULTI  OSI_BIT(2)
/** @brief disable all multicast mode */
#define OSI_OPER_DIS_ALLMULTI  OSI_BIT(3)
/** @brief Enable perfect filtering */
#define OSI_OPER_EN_PERFECT  OSI_BIT(6)
/** @brief disable perfect filtering */
#define OSI_OPER_DIS_PERFECT  OSI_BIT(7)
/** @brief address update */
#define OSI_OPER_ADDR_UPDATE  OSI_BIT(8)
/** @brief address delete */
#define OSI_OPER_ADDR_DEL  OSI_BIT(9)

#define OSI_PFT_MATCH  0U
#define OSI_SA_MATCH   1U

#define OSI_SPEED_10     10
#define OSI_SPEED_100    100
#define OSI_SPEED_1000   1000
#define OSI_SPEED_2500   2500
#define OSI_SPEED_5000   5000
#define OSI_SPEED_10000  10000
#define OSI_SPEED_25000  25000

#define TEN_POWER_9   0x3B9ACA00U
#define TWO_POWER_32  0x100000000ULL
/* MDIO clause 45 bit */
#define OSI_MII_ADDR_C45  OSI_BIT(30)
/** @brief EQOS default MDC CR value - CSR 300-500 MHz, div=204 */
#define OSI_EQOS_DEFAULT_MDC_CR  0x6U
/** @brief MGBE default MDC CR value - CSR 400-500 MHz, div=202 */
#define OSI_MGBE_DEFAULT_MDC_CR  0x5U
/** @brief Maximum allowed MDC CR value */
#define OSI_MAX_MDC_CR  0xFU
/** @} */

/**
 * @brief Ethernet PHY Interface Modes
 */
#define OSI_XFI_MODE_10G      0U
#define OSI_XFI_MODE_5G       1U
#define OSI_USXGMII_MODE_10G  2U
#define OSI_USXGMII_MODE_5G   3U

/**
 * @brief Ethernet UPHY GBE Modes
 */
#define OSI_GBE_MODE_5G    0U
#define OSI_GBE_MODE_10G   1U
#define OSI_GBE_MODE_25G   2U
#define OSI_GBE_MODE_1G    3U
#define OSI_GBE_MODE_2_5G  4U

/**
 * @addtogroup IOCTL OPS MACROS
 *
 * @brief IOCTL OPS for runtime commands
 * @{
 */
/**
 * @brief Command to set L3L4 filters
 */
#define OSI_CMD_L3L4_FILTER  3U

/**
 * @brief Command to handle common ISR
 */
#define OSI_CMD_COMMON_ISR  7U

/**
 * @brief Command to do pad calibration
 */
#define OSI_CMD_PAD_CALIBRATION  8U

/**
 * @brief Command to read MMC counters
 */
#define OSI_CMD_READ_MMC  9U

/**
 * @brief Command to set speed
 */
#define OSI_CMD_SET_SPEED  17U

/**
 * @brief Command to set L2 filter
 */
#define OSI_CMD_L2_FILTER  18U

/**
 * @brief Command to enable/disable RXCSUM offload
 */
#define OSI_CMD_RXCSUM_OFFLOAD  19U

/**
 * @brief Command to adjust frequency
 */
#define OSI_CMD_ADJ_FREQ  20U

/**
 * @brief Command to adjust time
 */
#define OSI_CMD_ADJ_TIME  21U

/**
 * @brief Command to configure PTP
 */
#define OSI_CMD_CONFIG_PTP  22U

/**
 * @brief Command to GET AVB
 */
#define OSI_CMD_GET_AVB  23U

/**
 * @brief Command to SET AVB
 */
#define OSI_CMD_SET_AVB  24U

/**
 * @brief Command to GET hardware supported features
 */
#define OSI_CMD_GET_HW_FEAT  28U

/**
 * @brief Command to set system time to hardware
 */
#define OSI_CMD_SET_SYSTOHW_TIME  33U

/**
 * @brief Command to configure FRP
 */
#define OSI_CMD_CONFIG_FRP  36U

/**
 * @brief Command to configure EST
 */
#define OSI_CMD_CONFIG_EST  38U

/**
 * @brief Command to configure FPE
 */
#define OSI_CMD_CONFIG_FPE  39U

/**
 * @brief Command to read from a register
 */
#define OSI_CMD_READ_REG  40U

/**
 * @brief Command to write to a register
 */
#define OSI_CMD_WRITE_REG  41U

/**
 * @brief Command to get transmit timestamp
 */
#define OSI_CMD_GET_TX_TS  42U

/**
 * @brief Command to free already stored timestamp
 */
#define OSI_CMD_FREE_TS  43U
#ifdef OSI_DEBUG
#define OSI_CMD_REG_DUMP      44U
#define OSI_CMD_STRUCTS_DUMP  45U
#endif /* OSI_DEBUG */

/**
 * @brief Command to capture TSC-PTP timestamp
 */
#define OSI_CMD_CAP_TSC_PTP  46U

/**
 * @brief Command to update MAC-MTU
 */
#define OSI_CMD_MAC_MTU  47U

/**
 * @brief Command to configure MAC to MAC time syncronyzation
 */
#define OSI_CMD_CONF_M2M_TS  48U
#ifdef MACSEC_SUPPORT

/**
 * @brief Command to read from a MACSEC register
 */
#define OSI_CMD_READ_MACSEC_REG  49U

/**
 * @brief Command to write to a MACSEC register
 */
#define OSI_CMD_WRITE_MACSEC_REG  50U
#endif /* MACSEC_SUPPORT */
#ifdef HSI_SUPPORT

/**
 * @brief Command to configure HSI
 */
#define OSI_CMD_HSI_CONFIGURE  51U
#endif
#ifdef OSI_DEBUG
#define OSI_CMD_DEBUG_INTR_CONFIG  52U
#endif

/**
 * @brief Command to handle suspend event
 */
#define OSI_CMD_SUSPEND  53U

/**
 * @brief Command to handle resume event
 */
#define OSI_CMD_RESUME  54U
#if defined HSI_SUPPORT && defined (NV_VLTEST_BUILD)

/**
 * @brief Command to inject HSI error
 */
#define OSI_CMD_HSI_INJECT_ERR  55U
#endif /* HSI_SUPPORT */

/**
 * @brief Command to read MAC stats
 */
#define OSI_CMD_READ_STATS  56U
#ifdef HSI_SUPPORT

/**
 * @brief Command to read HSI error
 */
#define OSI_CMD_READ_HSI_ERR  57U
#endif /* HSI_SUPPORT */

/**
 * @brief Command to GET RSS Configuration
 */
#define OSI_CMD_GET_RSS  58U
/** @} */

#ifdef LOG_OSI

/**
 * @brief OSI error macro definition,
 * @param[in] priv: OSD private data OR NULL
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 */
#define OSI_CORE_ERR(priv, type, err, loga)                     \
{                                                               \
        osi_core->osd_ops.ops_log(priv, __func__, __LINE__,     \
                                  OSI_LOG_ERR, type, err, loga);\
}

/**
 * @brief OSI info macro definition
 * @param[in] priv: OSD private data OR NULL
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 */
#define OSI_CORE_INFO(priv, type, err, loga)                            \
{                                                                       \
        osi_core->osd_ops.ops_log(priv, __func__, __LINE__,             \
                                  OSI_LOG_INFO, type, err, loga);       \
}
#else
#define OSI_CORE_ERR(priv, type, err, loga)
#define OSI_CORE_INFO(priv, type, err, loga)
#endif

#define VLAN_NUM_VID      4096U
#define OSI_DELAY_1000US  1000U
#define OSI_DELAY_1US     1U
#define RCHLIST_SIZE      48U

/**
 * @addtogroup PTP PTP related information
 *
 * @brief PTP SSINC values
 * @{
 */
#define OSI_PTP_SSINC_4  4U
#define OSI_PTP_SSINC_6  6U
/** @} */

/**
 * @addtogroup Flexible Receive Parser related information
 *
 * @brief Flexible Receive Parser commands, table size and other defines
 * @{
 */
#ifndef OSI_STRIPPED_LIB
#define OSI_FRP_CMD_MAX    3U
#define OSI_FRP_MATCH_MAX  10U
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief Maximum number of FRP entries
 */
#define OSI_FRP_MAX_ENTRY  256U
/** @brief Maximum offset for FRP match */
#define OSI_FRP_OFFSET_MAX  64U
/* FRP Command types */
/** @brief Command to add an FRP rule */
#define OSI_FRP_CMD_ADD  0U
/** @brief Command to update an FRP rule */
#define OSI_FRP_CMD_UPDATE  1U
/** @brief Command to delete an FRP rule */
#define OSI_FRP_CMD_DEL  2U
/* FRP Filter mode defines */
/** @brief FRP mode to route the frame if match */
#define OSI_FRP_MODE_ROUTE  0U
/** @brief FRP mode to drop the frame if match */
#define OSI_FRP_MODE_DROP  1U
/** @brief FRP mode to bypass the frame if match */
#define OSI_FRP_MODE_BYPASS  2U
/** @brief FRP mode to link the frame if match */
#define OSI_FRP_MODE_LINK  3U
/** @brief FRP mode to route the frame if inverse match */
#define OSI_FRP_MODE_IM_ROUTE  4U
/** @brief FRP mode to drop the frame if inverse match */
#define OSI_FRP_MODE_IM_DROP  5U
/** @brief FRP mode to bypass the frame if inverse match */
#define OSI_FRP_MODE_IM_BYPASS  6U
/** @brief FRP mode to link if inverse match */
#define OSI_FRP_MODE_IM_LINK  7U
/** @brief Maximum numer of FRP modes */
#define OSI_FRP_MODE_MAX  8U
/* Match data defines */
/** @brief FRP NORMAL MATCH */
#define OSI_FRP_MATCH_NORMAL  0U
/** @brief FRP MATCH in L2 desination Address */
#define OSI_FRP_MATCH_L2_DA  1U
/** @brief FRP MATCH in L2 source Address */
#define OSI_FRP_MATCH_L2_SA  2U
/** @brief FRP MATCH in L3 source IP */
#define OSI_FRP_MATCH_L3_SIP  3U
/** @brief FRP MATCH in L3 destination IP */
#define OSI_FRP_MATCH_L3_DIP  4U
/** @brief FRP MATCH in L4 source UDP port */
#define OSI_FRP_MATCH_L4_S_UPORT  5U
/** @brief FRP MATCH in L4 destination UDP port */
#define OSI_FRP_MATCH_L4_D_UPORT  6U
/** @brief FRP MATCH in L4 source TCP port */
#define OSI_FRP_MATCH_L4_S_TPORT  7U
/** @brief FRP MATCH in L4 destination TCP port */
#define OSI_FRP_MATCH_L4_D_TPORT  8U
/** @brief FRP MATCH in VLANID */
#define OSI_FRP_MATCH_VLAN  9U
/** @} */

#define XPCS_WRITE_FAIL_CODE  -9

#ifdef HSI_SUPPORT

/**
 * @addtogroup osi_hsi_err_code_idx
 *
 * @brief data index for osi_hsi_err_code array
 * @{
 */
/** @brief UnCorrectable Error Index */
#define UE_IDX  0U
/** @brief Correctable Error Index */
#define CE_IDX  1U
/** @brief RX CRC Error Index */
#define RX_CRC_ERR_IDX  2U
/** @brief TX frame error Index */
#define TX_FRAME_ERR_IDX  3U
/** @brief RX checksum error Index */
#define RX_CSUM_ERR_IDX  4U
/** @brief autonegotiation error Index */
#define AUTONEG_ERR_IDX  5U
/** @brief xpcs write error Index */
#define XPCS_WRITE_FAIL_IDX  6U
/** @brief phy write verify error Index */
#define PHY_WRITE_VERIFY_FAIL_IDX  7U
/** @brief MAC to MAC error Index */
#define MAC2MAC_ERR_IDX  8U
/** @brief Link training monitor error Index */
#define PCS_LNK_ERR_IDX  9U
/** @brief mac common interrupt status monitor error Index */
#define MAC_CMN_INTR_ERR_IDX  10U
/** @brief MACSEC RX CRC error Index */
#define MACSEC_RX_CRC_ERR_IDX  0U
/** @brief MACSEC TX CRC error Index */
#define MACSEC_TX_CRC_ERR_IDX  1U
/** @brief MACSEC RX ICV error Index */
#define MACSEC_RX_ICV_ERR_IDX  2U
/** @brief MACSEC tegister violation error Index */
#define MACSEC_REG_VIOL_ERR_IDX  3U
/** @} */

/**
 * @addtogroup HSI_TIME_THRESHOLD
 *
 * @brief HSI time threshold to report error in ms
 * @{
 */
/** @brief default time to report error */
#define OSI_HSI_ERR_TIME_THRESHOLD_DEFAULT  3000U
/** @brief minimum time to report error */
#define OSI_HSI_ERR_TIME_THRESHOLD_MIN  1000U
/** @brief maximum time to report error */
#define OSI_HSI_ERR_TIME_THRESHOLD_MAX  60000U
/** @} */

/**
 * @brief HSI error count threshold to report error
 */
#define OSI_HSI_ERR_COUNT_THRESHOLD  1000U

/**
 * @brief Maximum number of different mac error code
 * HSI_SW_ERR_CODE + Two (Corrected and Uncorrected error code)
 */
#define OSI_HSI_MAX_MAC_ERROR_CODE  11U

/**
 * @brief Maximum number of different macsec error code
 */
#define HSI_MAX_MACSEC_ERROR_CODE  4U

/**
 * @addtogroup HSI_SW_ERR_CODE
 *
 * @brief software defined error code
 * @{
 */
/** @brief Uncorrectable error code */
#define OSI_UNCORRECTABLE_ERR  0x1U
/** @brief correctable error code */
#define OSI_CORRECTABLE_ERR  0x2U
/** @brief inbound bus crc error code */
#define OSI_INBOUND_BUS_CRC_ERR  0x3U
/** @brief tx frame error code */
#define OSI_TX_FRAME_ERR  0x4U
/** @brief receive checksum error code */
#define OSI_RECEIVE_CHECKSUM_ERR  0x5U
/** @brief pcs autonegotiation error code */
#define OSI_PCS_AUTONEG_ERR  0x6U
/** @brief MACSEC RX CRC error code */
#define OSI_MACSEC_RX_CRC_ERR  0x7U
/** @brief MACSEC TX CRC error code */
#define OSI_MACSEC_TX_CRC_ERR  0x8U
/** @brief MACSEC RX ICV error code */
#define OSI_MACSEC_RX_ICV_ERR  0x9U
/** @brief MACSEC register violation error code */
#define OSI_MACSEC_REG_VIOL_ERR  0xAU
/** @brief XPCS write fail error code */
#define OSI_XPCS_WRITE_FAIL_ERR  0xBU
/** @brief PHY write verify error code */
#define OSI_PHY_WRITE_VERIFY_ERR  0xCU
/** @brief M2M TSC read error code */
#define OSI_M2M_TSC_READ_ERR  0xDU
/** @brief M2M time cal error code */
#define OSI_M2M_TIME_CAL_ERR  0xEU
/** @brief M2M adjust frequency error code */
#define OSI_M2M_ADJ_FREQ_ERR  0xFU
/** @brief M2M adjust time error code */
#define OSI_M2M_ADJ_TIME_ERR  0x10U
/** @brief M2M set time error code */
#define OSI_M2M_SET_TIME_ERR  0x11U
/** @brief M2M config PTP error code */
#define OSI_M2M_CONFIG_PTP_ERR  0x12U
/** @brief pcs link status error code */
#define OSI_PCS_LNK_ERR  0x13U
/** @brief MAC common interrupt status error code */
#define OSI_MAC_CMN_INTR_ERR  0x14U

/** @brief EQOS uncorrectable attribute */
#define OSI_EQOS_UNCORRECTABLE_ATTR  0x109
/** @brief EQOS correctable attribute */
#define OSI_EQOS_CORRECTABLE_ATTR  0x309
/** @brief MGBE0 uncorrectable attribute */
#define OSI_MGBE0_UNCORRECTABLE_ATTR  0x119
/** @brief MGBE0 correctable attribute */
#define OSI_MGBE0_CORRECTABLE_ATTR  0x319
/** @brief MGBE1 uncorrectable attribute */
#define OSI_MGBE1_UNCORRECTABLE_ATTR  0x11A
/** @brief MGBE1 correctable attribute */
#define OSI_MGBE1_CORRECTABLE_ATTR  0x31A
/** @brief MGBE2 uncorrectable attribute */
#define OSI_MGBE2_UNCORRECTABLE_ATTR  0x11B
/** @brief MGBE2 correctable attribute */
#define OSI_MGBE2_CORRECTABLE_ATTR  0x31B
/** @brief MGBE3 uncorrectable attribute */
#define OSI_MGBE3_UNCORRECTABLE_ATTR  0x11C
/** @brief MGBE3 correctable attribute */
#define OSI_MGBE3_CORRECTABLE_ATTR  0x31C
/** @} */
#endif

struct osi_core_priv_data;

/**
 * @brief OSI core structure for filters
 */
struct osi_filter {
  /** indicates operation needs to perform. refer to OSI_OPER_* */
  nveu32_t    oper_mode;

  /** Indicates the index of the filter to be modified.
   * Filter index must be between 0 - 127 */
  nveu32_t    index;
  /** Ethernet MAC address to be added */
  nveu8_t     mac_addr[OSI_ETH_ALEN];
  /** Indicates dma channel routing enable(1) disable (0) */
  nveu32_t    dma_routing;
  /**  indicates dma channel number to program */
  nveu32_t    dma_chan;

  /** filter will not consider byte in comparison
   *    Bit 5: MAC_Address${i}_High[15:8]
   *    Bit 4: MAC_Address${i}_High[7:0]
   *    Bit 3: MAC_Address${i}_Low[31:24]
   *    ..
   *    Bit 0: MAC_Address${i}_Low[7:0] */
  nveu32_t    addr_mask;
  /** src_dest: SA(1) or DA(0) */
  nveu32_t    src_dest;
  /**  indicates one hot encoded DMA receive channels to program */
  nveu64_t    dma_chansel;
  /** Indicates packet duplication enable(1) disable (0) */
  nveu32_t    pkt_dup;
};

/**
 * @brief OSI core structure for RCHlist
 */
struct rchlist_index {
  nveu8_t     mac_address[OSI_ETH_ALEN];
  nveu32_t    in_use;
  nveu64_t    dch;
};

#ifndef OSI_STRIPPED_LIB

/**
 * @brief OSI core structure for RXQ route
 */
struct osi_rxq_route {
  #define OSI_RXQ_ROUTE_PTP  0U
  /** Indicates RX routing type OSI_RXQ_ROUTE_* */
  nveu32_t    route_type;
  /** RXQ routing enable(1) disable (0) */
  nveu32_t    enable;
  /** RX queue index */
  nveu32_t    idx;
};

#endif

/**
 * @brief struct osi_hw_features - MAC HW supported features.
 */
struct osi_hw_features {
  /** It is set to 1 when 10/100 Mbps is selected as the Mode of
   * Operation */
  nveu32_t    mii_sel;
  /** It is set to 1 when the RGMII Interface option is selected */
  nveu32_t    rgmii_sel;
  /** It is set to 1 when the RMII Interface option is selected */
  nveu32_t    rmii_sel;
  /** It sets to 1 when 1000 Mbps is selected as the Mode of Operation */
  nveu32_t    gmii_sel;
  /** It sets to 1 when the half-duplex mode is selected */
  nveu32_t    hd_sel;

  /** It sets to 1 when the TBI, SGMII, or RTBI PHY interface
   * option is selected */
  nveu32_t    pcs_sel;

  /** It sets to 1 when the Enable VLAN Hash Table Based Filtering
   * option is selected */
  nveu32_t    vlan_hash_en;

  /** It sets to 1 when the Enable Station Management (MDIO Interface)
   * option is selected */
  nveu32_t    sma_sel;

  /** It sets to 1 when the Enable Remote Wake-Up Packet Detection
   * option is selected */
  nveu32_t    rwk_sel;

  /** It sets to 1 when the Enable Magic Packet Detection option is
   * selected */
  nveu32_t    mgk_sel;

  /** It sets to 1 when the Enable MAC Management Counters (MMC) option
   * is selected */
  nveu32_t    mmc_sel;
  /** It sets to 1 when the Enable IPv4 ARP Offload option is selected */
  nveu32_t    arp_offld_en;

  /** It sets to 1 when the Enable IEEE 1588 Timestamp Support option
   * is selected */
  nveu32_t    ts_sel;

  /** It sets to 1 when the Enable Energy Efficient Ethernet (EEE) option
   * is selected */
  nveu32_t    eee_sel;

  /** It sets to 1 when the Enable Transmit TCP/IP Checksum Insertion
   * option is selected */
  nveu32_t    tx_coe_sel;

  /** It sets to 1 when the Enable Receive TCP/IP Checksum Check option
   * is selected */
  nveu32_t    rx_coe_sel;

  /** It sets to 1 when the Enable Additional 1-31 MAC Address Registers
   * option is selected */
  nveu32_t    mac_addr_sel;

  /** It sets to 1 when the Enable Additional 32-63 MAC Address Registers
   * option is selected */
  nveu32_t    mac_addr32_sel;

  /** It sets to 1 when the Enable Additional 64-127 MAC Address Registers
   * option is selected */
  nveu32_t    mac_addr64_sel;

  /** It sets to 1 when the Enable IEEE 1588 Timestamp Support option
   * is selected */
  nveu32_t    tsstssel;

  /** It sets to 1 when the Enable SA and VLAN Insertion on Tx option
   * is selected */
  nveu32_t    sa_vlan_ins;

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
  nveu32_t    act_phy_sel;

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
  nveu32_t    rx_fifo_size;

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
  nveu32_t    tx_fifo_size;
  /** It set to 1 when Advance timestamping High Word selected */
  nveu32_t    adv_ts_hword;

  /** Address Width.
   * This field indicates the configured address width:
   * 00: 32
   * 01: 40
   * 10: 48
   * 11: Reserved */
  nveu32_t    addr_64;
  /** It sets to 1 when DCB Feature Enable */
  nveu32_t    dcb_en;
  /** It sets to 1 when Split Header Feature Enable */
  nveu32_t    sph_en;
  /** It sets to 1 when TCP Segmentation Offload Enable */
  nveu32_t    tso_en;
  /** It sets to 1 when DMA debug registers are enabled */
  nveu32_t    dma_debug_gen;
  /** It sets to 1 if AV Feature Enabled */
  nveu32_t    av_sel;
  /** It sets to 1 if Receive side AV Feature Enabled */
  nveu32_t    rav_sel;

  /** This field indicates the size of the hash table:
   * 00: No hash table
   * 01: 64
   * 10: 128
   * 11: 256 */
  nveu32_t    hash_tbl_sz;

  /** This field indicates the total number of L3 or L4 filters:
   * 0000: No L3 or L4 Filter
   * 0001: 1 L3 or L4 Filter
   * 0010: 2 L3 or L4 Filters
   * ..
   * 1000: 8 L3 or L4 */
  nveu32_t    l3l4_filter_num;
  /** It holds number of MTL Receive Queues */
  nveu32_t    rx_q_cnt;
  /** It holds number of MTL Transmit Queues */
  nveu32_t    tx_q_cnt;
  /** It holds number of DMA Receive channels */
  nveu32_t    rx_ch_cnt;

  /** This field indicates the number of DMA Transmit channels:
   * 0000: 1 DMA Tx Channel
   * 0001: 2 DMA Tx Channels
   * ..
   * 0111: 8 DMA Tx */
  nveu32_t    tx_ch_cnt;

  /** This field indicates the number of PPS outputs:
   * 000: No PPS output
   * 001: 1 PPS output
   * 010: 2 PPS outputs
   * 011: 3 PPS outputs
   * 100: 4 PPS outputs
   * 101-111: Reserved */
  nveu32_t    pps_out_num;

  /** Number of Auxiliary Snapshot Inputs
   * This field indicates the number of auxiliary snapshot inputs:
   * 000: No auxiliary input
   * 001: 1 auxiliary input
   * 010: 2 auxiliary inputs
   * 011: 3 auxiliary inputs
   * 100: 4 auxiliary inputs
   * 101-111: Reserved */
  nveu32_t    aux_snap_num;
  /** VxLAN/NVGRE Support */
  nveu32_t    vxn;

  /** Enhanced DMA.
   * This bit is set to 1 when the "Enhanced DMA" option is
   * selected. */
  nveu32_t    edma;

  /** Different Descriptor Cache
   * When set to 1, then EDMA mode Separate Memory is
   * selected for the Descriptor Cache.*/
  nveu32_t    ediffc;

  /** PFC Enable
   * This bit is set to 1 when the Enable PFC Feature is selected */
  nveu32_t    pfc_en;
  /** One-Step Timestamping Enable */
  nveu32_t    ost_en;
  /** PTO Offload Enable */
  nveu32_t    pto_en;
  /** Receive Side Scaling Enable */
  nveu32_t    rss_en;
  /** Number of Traffic Classes */
  nveu32_t    num_tc;
  /** Number of Extended VLAN Tag Filters Enabled */
  nveu32_t    num_vlan_filters;

  /** Supported Flexible Receive Parser.
   * This bit is set to 1 when the Enable Flexible Programmable
   * Receive Parser option is selected */
  nveu32_t    frp_sel;

  /** Queue/Channel based VLAN tag insertion on Tx Enable
   * This bit is set to 1 when the Enable Queue/Channel based
   * VLAN tag insertion on Tx Feature is selected. */
  nveu32_t    cbti_sel;

  /** Supported Parallel Instruction Processor Engines (PIPEs)
   * This field indicates the maximum number of Instruction
   * Processors supported by flexible receive parser. */
  nveu32_t    num_frp_pipes;

  /** One Step for PTP over UDP/IP Feature Enable
   * This bit is set to 1 when the Enable One step timestamp for
   * PTP over UDP/IP feature is selected */
  nveu32_t    ost_over_udp;

  /** Supported Flexible Receive Parser Parsable Bytes
   * This field indicates the supported Max Number of bytes of the
   * packet data to be Parsed by Flexible Receive Parser */
  nveu32_t    max_frp_bytes;

  /** Supported Flexible Receive Parser Instructions
   * This field indicates the Max Number of Parser Instructions
   * supported by Flexible Receive Parser */
  nveu32_t    max_frp_entries;

  /** Double VLAN Processing Enabled
   * This bit is set to 1 when the Enable Double VLAN Processing
   * feature is selected */
  nveu32_t    double_vlan_en;

  /** Automotive Safety Package
   * Following are the encoding for the different Safety features
   * Values:
   * 0x0 (NONE): No Safety features selected
   * 0x1 (ECC_ONLY): Only "ECC protection for external
   * memory" feature is selected
   * 0x2 (AS_NPPE): All the Automotive Safety features are
   * selected without the "Parity Port Enable for external interface"
   * feature
   * 0x3 (AS_PPE): All the Automotive Safety features are
   * selected with the "Parity Port Enable for external interface"
   * feature */
  nveu32_t    auto_safety_pkg;

  /** Tx Timestamp FIFO Depth
   * This value indicates the depth of the Tx Timestamp FIFO
   * 3'b000: Reserved
   * 3'b001: 1
   * 3'b010: 2
   * 3'b011: 4
   * 3'b100: 8
   * 3'b101: 16
   * 3'b110: Reserved
   * 3'b111: Reserved */
  nveu32_t    tts_fifo_depth;

  /** Enhancements to Scheduling Traffic Enable
   * This bit is set to 1 when the Enable Enhancements to
   * Scheduling Traffic feature is selected.
   * Values:
   * 0x0 (INACTIVE): Enable Enhancements to Scheduling
   * Traffic feature is not selected
   * 0x1 (ACTIVE): Enable Enhancements to Scheduling
   * Traffic feature is selected */
  nveu32_t    est_sel;

  /** Depth of the Gate Control List
   * This field indicates the depth of Gate Control list expressed as
   * Log2(DWCXG_GCL_DEP)-5
   * Values:
   * 0x0 (NODEPTH): No Depth configured
   * 0x1 (DEPTH64): 64
   * 0x2 (DEPTH128): 128
   * 0x3 (DEPTH256): 256
   * 0x4 (DEPTH512): 512
   * 0x5 (DEPTH1024): 1024
   * 0x6 (RSVD): Reserved */
  nveu32_t    gcl_depth;

  /** Width of the Time Interval field in the Gate Control List
   * This field indicates the width of the Configured Time Interval
   * Field
   * Values:
   * 0x0 (NOWIDTH): Width not configured
   * 0x1 (WIDTH16): 16
   * 0x2 (WIDTH20): 20
   * 0x3 (WIDTH24): 24 */
  nveu32_t    gcl_width;

  /** Frame Preemption Enable
   * This bit is set to 1 when the Enable Frame preemption feature
   * is selected.
   * Values:
   * 0x0 (INACTIVE): Frame Preemption Enable feature is not
   * selected
   * 0x1 (ACTIVE): Frame Preemption Enable feature is
   * selected */
  nveu32_t    fpe_sel;

  /** Time Based Scheduling Enable
   * This bit is set to 1 when the Time Based Scheduling feature is
   * selected.
   * Values:
   * 0x0 (INACTIVE): Time Based Scheduling Enable feature is
   * not selected
   * 0x1 (ACTIVE): Time Based Scheduling Enable feature is
   * selected */
  nveu32_t    tbs_sel;

  /** The number of DMA channels enabled for TBS (starting from
   * the highest Tx Channel in descending order)
   * This field provides the number of DMA channels enabled for
   * TBS (starting from the highest Tx Channel in descending
   * order):
   * 0000: 1 DMA Tx Channel enabled for TBS
   * 0001: 2 DMA Tx Channels enabled for TBS
   * 0010: 3 DMA Tx Channels enabled for TBS
   * ...
   * 1111: 16 DMA Tx Channels enabled for TBS */
  nveu32_t    num_tbs_ch;
};

#ifndef OSI_STRIPPED_LIB

/**
 * @brief Vlan filter Function dependent parameter
 */
struct osi_vlan_filter {
  /** vlan filter enable(1) or disable(0) */
  nveu32_t    filter_enb_dis;
  /** perfect(0) or hash(1) */
  nveu32_t    perfect_hash;
  /** perfect(0) or inverse(1) */
  nveu32_t    perfect_inverse_match;
};

/**
 * @brief L2 filter function dependent parameter
 */
struct osi_l2_da_filter {
  /** perfect(0) or hash(1) */
  nveu32_t    perfect_hash;
  /** perfect(0) or inverse(1) */
  nveu32_t    perfect_inverse_match;
};

/**
 * @brief struct ptp_offload_param - Parameter to support PTP offload.
 */
struct osi_pto_config {
  /** enable(0) / disable(1) */
  nveu32_t    en_dis;

  /** Flag for Master mode.
   * OSI_ENABLE for master OSI_DISABLE for slave */
  nveu32_t    master;
  /** Flag to Select PTP packets for Taking Snapshots */
  nveu32_t    snap_type;
  /** ptp domain */
  nveu32_t    domain_num;

  /**  The PTP Offload function qualifies received PTP
   *  packet with unicast Destination  address
   *  0 - only multicast, 1 - unicast and multicast */
  nveu32_t    mc_uc;
  /** Port identification */
  nveu32_t    portid;
};

/**
 * @brief osi_core_rss - Struture used to store RSS Hash key and table
 * information.
 */
struct osi_core_rss {
  /** Flag to represent to enable RSS or not */
  nveu32_t    enable;
  /** Array for storing RSS Hash key */
  nveu8_t     key[OSI_RSS_HASH_KEY_SIZE];
  /** Array for storing RSS Hash table */
  nveu32_t    table[OSI_RSS_MAX_TABLE_SIZE];
};

/**
 * @brief Max num of MAC core registers to backup. It should be max of or >=
 * (EQOS_MAX_BAK_IDX=380, coreX,...etc) backup registers.
 */
#define CORE_MAX_BAK_IDX  700U

/**
 * @brief core_backup - Struct used to store backup of core HW registers.
 */
struct core_backup {
  /** Array of reg MMIO addresses (base of MAC + offset of reg) */
  void        *reg_addr[CORE_MAX_BAK_IDX];
  /** Array of value stored in each corresponding register */
  nveu32_t    reg_val[CORE_MAX_BAK_IDX];
};

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief PTP configuration structure
 */
struct osi_ptp_config {
  /** PTP filter parameters bit fields.
   *
   * Enable Timestamp, Fine Timestamp, 1 nanosecond accuracy
   * are enabled by default.
   *
   * Need to set below bit fields accordingly as per the requirements.
   *
   * Enable Timestamp for All Packets                   OSI_BIT(8)
   *
   * Enable PTP Packet Processing for Version 2 Format  OSI_BIT(10)
   *
   * Enable Processing of PTP over Ethernet Packets     OSI_BIT(11)
   *
   * Enable Processing of PTP Packets Sent over IPv6-UDP        OSI_BIT(12)
   *
   * Enable Processing of PTP Packets Sent over IPv4-UDP        OSI_BIT(13)
   *
   * Enable Timestamp Snapshot for Event Messages               OSI_BIT(14)
   *
   * Enable Snapshot for Messages Relevant to Master    OSI_BIT(15)
   *
   * Select PTP packets for Taking Snapshots            OSI_BIT(16)
   *
   * Select PTP packets for Taking Snapshots            OSI_BIT(17)
   *
   * Select PTP packets for Taking Snapshots (OSI_BIT(16) + OSI_BIT(17))
   *
   * AV 802.1AS Mode Enable                             OSI_BIT(28)
   *
   * if ptp_filter is set to Zero then Time stamping is disabled */
  nveu32_t    ptp_filter;

  /** seconds to be updated to MAC
   * valid values are from 0 to UINT32_MAX */
  nveu32_t    sec;

  /** nano seconds to be updated to MAC
   * valid values are from 0 to UINT32_MAX */
  nveu32_t    nsec;

  /** PTP reference clock read from DT
   * valid values are from 0 to UINT32_MAX */
  nveu32_t    ptp_ref_clk_rate;

  /** Use one nsec accuracy (need to set 1)
   * valid values are 0 and 1 */
  nveu32_t    one_nsec_accuracy;

  /** PTP system clock which is 62500000Hz
   * valid values are from 0 to UINT32_MAX */
  nveu32_t    ptp_clock;

  /** PTP Packets RX Queue
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_EQOS_MAX_NUM_QUEUES-1 for eqos
   * and 0 to NVETHERNETRM_PIF$OSI_MGBE_MAX_NUM_QUEUES-1 */
  nveu32_t    ptp_rx_queue;
};

/**
 * @brief osi_core_ptp_tsc_data - Struture used to store TSC and PTP time
 * information.
 */
struct osi_core_ptp_tsc_data {
  /** high bits of MAC */
  nveu32_t    ptp_high_bits;
  /** low bits of MAC */
  nveu32_t    ptp_low_bits;
  /** high bits of TSC */
  nveu32_t    tsc_high_bits;
  /** low bits of TSC */
  nveu32_t    tsc_low_bits;
};

/**
 * @brief OSI VM IRQ data
 */
struct osi_vm_irq_data {
  /** Number of VM channels per VM IRQ */
  nveu32_t    num_vm_chans;
  /** VM/OS number to be used */
  nveu32_t    vm_num;

  /** Array of VM channel list
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_EQOS_MAX_NUM_CHANS-1 for eqos
   * and 0 to NVETHERNETRM_PIF$OSI_MGBE_MAX_NUM_CHANS-1 */
  nveu32_t    vm_chans[OSI_MGBE_MAX_NUM_CHANS];
};

/**
 *@brief OSD Core callbacks
 */
struct osd_core_ops {
  /** padctrl rx pin disable/enable callback */
  nve32_t    (*padctrl_mii_rx_pins)(
    void      *priv,
    nveu32_t  enable
    );
  /** logging callback */
  void       (*ops_log)(
    void          *priv,
    const nve8_t  *func,
    nveu32_t      line,
    nveu32_t      level,
    nveu32_t      type,
    const nve8_t  *err,
    nveul64_t     loga
    );
  /** udelay callback for sleep < 7usec as this is busy wait in most OSes */
  void       (*udelay)(
    nveu64_t  usec
    );
  /** usleep callback for longer sleep duration */
  void       (*usleep)(
    nveu64_t  usec
    );
  /** ivcsend callback*/
  nve32_t    (*ivc_send)(
    void                   *priv,
    struct ivc_msg_common  *ivc,
    nveu32_t               len
    );
 #ifdef MACSEC_SUPPORT
  /** Program macsec key table through Trust Zone callback */
  nve32_t    (*macsec_tz_kt_config)(
    void         *priv,
    nveu8_t      cmd,
    void *const  kt_config,
    void *const  genl_info
    );
 #endif /* MACSEC_SUPPORT */
 #ifdef OSI_DEBUG
  /**.printf function callback */
  void    (*printf)(
    struct osi_core_priv_data  *osi_core,
    nveu32_t                   type,
    const char                 *fmt,
    ...
    );
 #endif
  /** Lane bringup restart callback */
  void    (*restart_lane_bringup)(
    void      *priv,
    nveu32_t  en_disable
    );
};

#ifdef MACSEC_SUPPORT

/**
 * @brief MACSEC secure channel basic information
 */
struct osi_macsec_sc_info {
  /** Secure channel identifier
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t     sci[OSI_SCI_LEN];

  /** Secure association key
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t     sak[OSI_KEY_LEN_256];
 #ifdef MACSEC_KEY_PROGRAM
  /** Secure association key */
  nveu8_t     hkey[OSI_KEY_LEN_128];
 #endif /* MACSEC_KEY_PROGRAM */

  /** current AN
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_MAX_NUM_SA-1 */
  nveu8_t     curr_an;

  /** Next PN to use for the current AN
   * valid values are from 0 to UINT32_MAX */
  nveu32_t    next_pn;

  /** Lowest PN to use for the current AN
   * valid values are from 0 to UINT32_MAX */
  nveu32_t    lowest_pn;

  /** bitmap of valid AN
   * valid values are from 0 to 0xF */
  nveu32_t    an_valid;

  /** PN window
   * valid values are from 0 to UINT32_MAX */
  nveu32_t    pn_window;

  /** SC LUT index
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_SC_INDEX_MAX */
  nveu32_t    sc_idx_start;

  /** flags - encoding various states of SA
   * valid values are 0 and NVETHERNETRM_PIF$OSI_ENABLE_SA */
  nveu32_t    flags;

  /** flag indicating the prosition of vlan tag
   * valid values are either 0(vlan not in clear) or 1(vlan in clear) */
  nveu8_t     vlan_in_clear;

  /** Indicates 1 bit for encription configuration
  0: Indicates disabled
  1: Indicates enabled
  */
  nveu8_t     encrypt;

  /** Indicates 2 bit for confidentiality offset configuration
  0: Indicates offset as 0
  1: Indicates offset as 30
  2: Indicates offset as 50
  */
  nveu8_t     conf_offset;

  /** Peer MACID is stored
   * valid values are from 0 to UINT8_MAX */
  nveu8_t     peer_macid[OSI_ETH_ALEN];
};

/**
 * @brief MACSEC HW controller LUT's global status
 */
struct osi_macsec_lut_status {
  /** List of max SC's supported */
  struct osi_macsec_sc_info    sc_info[OSI_MAX_NUM_SC_T26x];

  /** next available BYP LUT index
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_BYP_LUT_MAX_INDEX */
  nveu16_t                     next_byp_idx;

  /** number of active SCs
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_MAX_NUM_SC */
  nveu32_t                     num_of_sc_used;
};

/**
 * @brief MACsec interrupt stats structure.
 */
struct osi_macsec_irq_stats {
  /** Tx debug buffer capture done
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_dbg_capture_done;

  /** Tx MTU check failed
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_mtu_check_fail;

  /** Tx MAC CRC err
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_mac_crc_error;

  /** Tx SC AN not valid
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_sc_an_not_valid;

  /** Tx AES GCM buffer overflow
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_aes_gcm_buf_ovf;

  /** Tx LUT lookup miss
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_lkup_miss;

  /** Tx uninitialized key slot
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_uninit_key_slot;

  /** Tx PN threshold reached
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_pn_threshold;

  /** Tx PN exhausted
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    tx_pn_exhausted;

  /** Tx debug buffer capture done
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_dbg_capture_done;

  /** Rx ICV error threshold
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_icv_err_threshold;

  /** Rx replay error
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_replay_error;

  /** Rx MTU check failed
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_mtu_check_fail;

  /** Rx MAC CRC err
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_mac_crc_error;

  /** Rx AES GCM buffer overflow
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_aes_gcm_buf_ovf;

  /** Rx LUT lookup miss
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_lkup_miss;

  /** Rx uninitialized key slot
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_uninit_key_slot;

  /** Rx PN exhausted
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    rx_pn_exhausted;

  /** Secure reg violation
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    secure_reg_viol;
};

#endif /* MACSEC_SUPPORT */

/**
 * @brief FRP Instruction configuration structure
 */
struct osi_core_frp_data {
  /** Entry Match Data
   * Valid values are from 0 to UINT32_MAX */
  nveu32_t    match_data;

  /** Entry Match Enable mask
   * Valid values are from 0 to UINT32_MAX */
  nveu32_t    match_en;

  /** Entry Accept frame flag
   * valid values are 0(disable) and 1(enable) */
  nveu8_t     accept_frame;

  /** Entry Reject Frame flag
   * valid values are 0(disable) and 1(enable) */
  nveu8_t     reject_frame;

  /** Entry Inverse match flag
   * valid values are 0(disable) and 1(enable) */
  nveu8_t     inverse_match;

  /** Entry Next Instruction Control match flag
   * valid values are 0(disable) and 1(enable) */
  nveu8_t     next_ins_ctrl;

  /** Entry Frame offset in the packet data
   * valid values are from 0 to 0xFF */
  nveu8_t     frame_offset;

  /** Entry OK Index - Next Instruction
   * valid values are from 0 to 0xFF */
  nveu8_t     ok_index;
  /** Entry dcht */
  nveu8_t     dcht;
  /** Entry DMA Channel selection (1-bit for each channel) */
  nveu64_t    dma_chsel;
  /** Entry RChlist index */
  nve32_t     rchlist_indx;
};

/**
 * @brief FRP Instruction table entry configuration structure
 */
struct osi_core_frp_entry {
  /** FRP ID
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_FRP_MAX_ENTRY-1 */
  nve32_t                     frp_id;

  /** FRP Entry data structure
   * refer NVETHERNETRM_PIF$osi_core_frp_data for more details on the data*/
  struct osi_core_frp_data    data;
};

/**
 * @brief Core time stamp data strcuture
 */
struct osi_core_tx_ts {
  /** Pointer to next item in the link */
  struct osi_core_tx_ts    *next;
  /** Pointer to prev item in the link */
  struct osi_core_tx_ts    *prev;

  /** Packet ID for corresponding timestamp
   * valid values are from 1 to 0x3FF*/
  nveu32_t                 pkt_id;
  /** vdma ID for corresponding timestamp */
  nveu32_t                 vdma_id;
  /** Time in seconds*/
  nveu32_t                 sec;
  /** Time in nano seconds */
  nveu32_t                 nsec;

  /** Variable which says if pkt_id is in use or not
   * valid values are 0(not in use) and 1(in use) */
  nveu32_t                 in_use;
};

/**
 * @brief OSI Core data structure for runtime commands.
 */
struct osi_ioctl {
  /** runtime command */
  nveu32_t     cmd;
  /** u32 general argument 1 */
  nveu32_t     arg1_u32;
  /** u32 general argument 2 */
  nveu32_t     arg2_u32;
  /** u32 general argument 3 */
  nveu32_t     arg3_u32;
  /** u32 general argument 4 */
  nveu32_t     arg4_u32;
  /** u64 general argument 5 */
  nveul64_t    arg5_u64;
  /** s32 general argument 6 */
  nve32_t      arg6_32;
  /** u8 string pointer general argument 7 for string */
  nveu8_t      *arg7_u8_p;
  /** s64 general argument 8 */
  nvel64_t     arg8_64;
  union {
    /** L2 filter structure */
    struct osi_filter                l2_filter;
    /** l3_l4 filter structure */
    struct osi_l3_l4_filter          l3l4_filter;
    /**  HW feature structure */
    struct osi_hw_features           hw_feat;
    /** AVB structure */
    struct osi_core_avb_algorithm    avb;
 #ifndef OSI_STRIPPED_LIB
    /** VLAN filter structure */
    struct osi_vlan_filter           vlan_filter;
    /** PTP offload config structure*/
    struct osi_pto_config            pto_config;
    /** RXQ route structure */
    struct osi_rxq_route             rxq_route;
    /** RSS core structure */
    struct osi_core_rss              rss;
 #endif /* !OSI_STRIPPED_LIB */
    /** FRP structure */
    struct osi_core_frp_cmd          frp_cmd;
    /** EST structure */
    struct osi_est_config            est;
    /** FRP structure */
    struct osi_fpe_config            fpe;
    /** PTP configuration settings */
    struct osi_ptp_config            ptp_config;
    /** TX Timestamp structure */
    struct osi_core_tx_ts            tx_ts;
    /** PTP TSC data */
    struct osi_core_ptp_tsc_data     ptp_tsc;
  } data;
};

/**
 * @brief core_padctrl - Struct used to eqos padctrl details.
 */
struct core_padctrl {
  /** Memory mapped base address of eqos padctrl registers */
  void        *padctrl_base;
  /** EQOS_RD0_0 register offset */
  nveu32_t    offset_rd0;
  /** EQOS_RD1_0 register offset */
  nveu32_t    offset_rd1;
  /** EQOS_RD2_0 register offset */
  nveu32_t    offset_rd2;
  /** EQOS_RD3_0 register offset */
  nveu32_t    offset_rd3;
  /** RX_CTL_0 register offset */
  nveu32_t    offset_rx_ctl;
  /** is pad calibration in progress */
  nveu32_t    is_pad_cal_in_progress;
  /** This flag set/reset using priv ioctl and DT entry */
  nveu32_t    pad_calibration_enable;
  /** Reg ETHER_QOS_AUTO_CAL_CONFIG_0[AUTO_CAL_PD_OFFSET] value */
  nveu32_t    pad_auto_cal_pd_offset;
  /** Reg ETHER_QOS_AUTO_CAL_CONFIG_0[AUTO_CAL_PU_OFFSET] value */
  nveu32_t    pad_auto_cal_pu_offset;
};

#ifdef HSI_SUPPORT

/**
 * @brief The OSI Core HSI private data structure.
 */
struct osi_hsi_data {
  /** Indicates if HSI feature is enabled */
  nveu32_t    enabled;

  /** time threshold to report error
   * valid values are from NVETHERNETRM_PIF$OSI_HSI_ERR_TIME_THRESHOLD_MIN
   * to NVETHERNETRM_PIF$OSI_HSI_ERR_TIME_THRESHOLD_MIN */
  nveu32_t    err_time_threshold;

  /** error count threshold to report error
   * which can be refered from NVETHERNETRM_PIF$OSI_HSI_ERR_COUNT_THRESHOLD */
  nveu32_t    err_count_threshold;
  /** HSI reporter ID */
  nveu16_t    reporter_id;

  /** HSI error codes
   * refere from NVETHERNETRM_PIF$OSI_UNCORRECTABLE_ERR to
   * NVETHERNETRM_PIF$OSI_M2M_CONFIG_PTP_ERR for different error codes */
  nveu32_t    err_code[OSI_HSI_MAX_MAC_ERROR_CODE];

  /** HSI error attribute
   * refer OSI_*CORRECTABLE_ATTR for different error attributes */
  nveu32_t    err_attr[OSI_HSI_MAX_MAC_ERROR_CODE];

  /** HSI MAC report count threshold based error
   * which can be refered from NVETHERNETRM_PIF$OSI_HSI_ERR_COUNT_THRESHOLD */
  nveu32_t    report_count_err[OSI_HSI_MAX_MAC_ERROR_CODE];
  /** Indicates if error reporting to FSI is pending */
  nveu32_t    report_err;

  /** HSI MACSEC error codes
   * refere from NVETHERNETRM_PIF$OSI_UNCORRECTABLE_ERR to
   * NVETHERNETRM_PIF$OSI_M2M_CONFIG_PTP_ERR for different error codes */
  nveu32_t    macsec_err_code[HSI_MAX_MACSEC_ERROR_CODE];

  /** HSI MACSEC error attribute
   * refer OSI_*CORRECTABLE_ATTR for different error attributes */
  nveu32_t    macsec_err_attr[HSI_MAX_MACSEC_ERROR_CODE];

  /** HSI MACSEC report error based on count threshold
   * which can be refered from NVETHERNETRM_PIF$OSI_HSI_ERR_COUNT_THRESHOLD */
  nveu32_t    macsec_report_count_err[HSI_MAX_MACSEC_ERROR_CODE];
  /** Indicates if error report to FSI is pending for MACSEC*/
  nveu32_t    macsec_report_err;
  /** RX CRC error report count */
  nveu64_t    rx_crc_err_count;
  /** RX Checksum error report count */
  nveu64_t    rx_checksum_err_count;
  /** MACSEC RX CRC error report count */
  nveu64_t    macsec_rx_crc_err_count;
  /** MACSEC TX CRC error report count */
  nveu64_t    macsec_tx_crc_err_count;
  /** MACSEC RX ICV error report count */
  nveu64_t    macsec_rx_icv_err_count;
  /** HW correctable error count */
  nveu64_t    ce_count;
  /** HW correctable error count hit threshold limit */
  nveu64_t    ce_count_threshold;
  /** tx frame error count */
  nveu64_t    tx_frame_err_count;
  /** tx frame error count threshold hit */
  nveu64_t    tx_frame_err_threshold;
  /** Rx UDP error injection count */
  nveu64_t    inject_udp_err_count;
  /** Rx CRC error injection count */
  nveu64_t    inject_crc_err_count;
};

#endif

/**
 * @brief The OSI Core (MAC & MTL) private data structure.
 */
struct osi_core_priv_data {
  /** Memory mapped base address of MAC IP
   * non NULL pointer*/
  void                              *base;

  /** Memory mapped base address of DMA window of MAC IP
   * non NULL pointer*/
  void                              *dma_base;

  /** Memory mapped base address of XPCS IP
   * non NULL pointer*/
  void                              *xpcs_base;

  /** Memory mapped base address of MACsec IP
   * non NULL pointer*/
  void                              *macsec_base;
 #ifdef MACSEC_SUPPORT

  /** Memory mapped base address of MACsec TZ page
   * non NULL pointer*/
  void                              *tz_base;

  /** Instance of macsec interrupt stats structure
   * refer NVETHERNETRM_PIF$osi_macsec_irq_stats for more details */
  struct osi_macsec_irq_stats       macsec_irq_stats;

  /** Instance of macsec HW controller Tx/Rx LUT status
   * refer NVETHERNETRM_PIF$osi_macsec_lut_status for more details */
  struct osi_macsec_lut_status      macsec_lut_status[OSI_NUM_CTLR];

  /** macsec mmc counters
   * for more detais refer NVETHERNETRM_PIF$osi_macsec_mmc_counters */
  struct osi_macsec_mmc_counters    macsec_mmc;

  /** MACSEC enabled state
   * valid values are 0(disable) and 1(enable)*/
  nveu32_t                          is_macsec_enabled;

  /** macsec_fpe_lock used to exclusively configure either macsec
   * non-zero value*/
  nveu32_t                          macsec_fpe_lock;

  /** FPE HW configuration initited to enable/disable
   * 1- FPE HW configuration initiated to enable
   * 0- FPE HW configuration initiated to disable */
  nveu32_t                          is_fpe_enabled;
 #ifdef DUMMY_SC

  /** Dummy SCI/SC/SA etc LUTs programmed with dummy parameter when no
   * session setup. SCI LUT hit created with VF's MACID
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t                           macsec_dummy_sc_macids[OSI_MAX_NUM_SC_T26x][OSI_ETH_ALEN];
 #endif

  /** MACSEC initialization state
   * valid vaues are 0(not initialized) and 1(Initialized) */
  nveu32_t                          macsec_initialized;
 #endif /* MACSEC_SUPPORT */

  /** Pointer to OSD private data structure
   * non NULL pointer */
  void                              *osd;

  /** OSD callback ops structure
   * Refer NVETHERNETRM_PIF$osd_core_ops for more details */
  struct osd_core_ops               osd_ops;

  /** Number of MTL queues enabled in MAC
   * max value for EQOS is NVETHERNETRM_PIF$OSI_EQOS_MAX_NUM_QUEUES
   * max value for MGBE is NVETHERNETRM_PIF$OSI_MGBE_MAX_NUM_QUEUES */
  nveu32_t                          num_mtl_queues;

  /** Array of MTL queues
   * each array element has max value same as num_mtl_queues */
  nveu32_t                          mtl_queues[OSI_MGBE_MAX_NUM_QUEUES];
  /** List of MTL Rx queue mode that need to be enabled */
  nveu32_t                          rxq_ctrl[OSI_MGBE_MAX_NUM_QUEUES];

  /** Rx MTl Queue mapping based on User Priority field
   * valid values are from 1 to 0xFF */
  nveu32_t                          rxq_prio[OSI_MGBE_MAX_NUM_QUEUES];

  /** MAC HW type EQOS based on DT compatible
   * valid values are NVETHERNETRM_PIF$OSI_MAC_HW_EQOS and
   * NVETHERNETRM_PIF$OSI_MAC_HW_MGBE*/
  nveu32_t                          mac;
  /** MACSEC HW type based on DT compatible */
  nveu32_t                          macsec;

  /** MAC version
   * valid values are NVETHERNETRM_PIF$OSI_EQOS_MAC_5_00,
   * NVETHERNETRM_PIF$OSI_EQOS_MAC_5_30
   * and NVETHERNETRM_PIF$OSI_MGBE_MAC_3_10*/
  nveu32_t                          mac_ver;

  /** MAC version
   * valid values are NVETHERNETRM_PIF$MAC_CORE_VER_TYPE_EQOS,
   * NVETHERNETRM_PIF$MAC_CORE_VER_TYPE_EQOS_5_30,
   * NVETHERNETRM_PIF$MAC_CORE_VER_TYPE_MGBE,
   * and NVETHERNETRM_PIF$MAC_CORE_VER_TYPE_EQOS_5_40*/
  nveu32_t                          mac_ver_type;
  /** HW supported feature list */
  struct osi_hw_features            *hw_feat;

  /** MTU size
   * mximum support MTU is NVETHERNETRM_PIF$OSI_MAX_MTU_SIZE*/
  nveu32_t                          mtu;

  /** Ethernet MAC address
   * valid values are from 0 to 0xFF for each array element */
  nveu8_t                           mac_addr[OSI_ETH_ALEN];

  /** Current flow control settings
   * valid values are 0(disable flow control)
   * NVETHERNETRM_PIF$OSI_FLOW_CTRL_TX and NVETHERNETRM_PIF$OSI_FLOW_CTRL_RX */
  nveu32_t                          flow_ctrl;
  /** PTP configuration settings */
  struct osi_ptp_config             ptp_config;

  /** Default addend value
   * valid values are from 0 to UINT32_MAX*/
  nveu32_t                          default_addend;
  /** mmc counter structure */
  struct osi_mmc_counters           mmc;
  /** DMA channel selection enable (1) disable(0) */
  nveu32_t                          dcs_en;

  /** TQ:TC mapping
   * valid values are from 0 to 7 */
  nveu32_t                          tc[OSI_MGBE_MAX_NUM_PDMA_CHANS];
 #ifndef OSI_STRIPPED_LIB
  /** Memory mapped base address of HV window */
  void                              *hv_base;

  /** csr clock is to program LPI 1 us tick timer register.
   * Value stored in MHz
   */
  nveu32_t                          csr_clk_speed;
  nveu64_t                          vf_bitmap;
  /** Array to maintain VLAN filters */
  nveu16_t                          vid[VLAN_NUM_VID];
  /** Count of number of VLAN filters in vid array */
  nveu16_t                          vlan_filter_cnt;
 #endif
  /** DT entry to enable(1) or disable(0) pause frame support */
  nveu32_t                          pause_frames;

  /** Residual queue valid with FPE support
   * Value range for EQOS 1 to NVETHERNETRM_PIF$OSI_EQOS_MAX_NUM_QUEUES-1
   * Value range for MGBE 1 to NVETHERNETRM_PIF$OSI_MGBE_MAX_NUM_QUEUES-1*/
  nveu32_t                          residual_queue;
  /** FRP Instruction Table */
  struct osi_core_frp_entry         frp_table[OSI_FRP_MAX_ENTRY];

  /** Number of valid Entries in the FRP Instruction Table
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_FRP_MAX_ENTRY*/
  nveu32_t                          frp_cnt;

  /* Switch to Software Owned List Complete.
   *  1 - Successful and User configured GCL in placed
   */
  nveu32_t                          est_ready;

  /* FPE enabled, verify and respose done with peer device
   * 1- Successful and can be used between P2P device
   */
  nveu32_t                          fpe_ready;
  /** MAC stats counters */
  struct osi_stats                  stats;
  /** eqos pad control structure */
  struct core_padctrl               padctrl;

  /** MDC clock rate
   * valid values are from 0 to 7 */
  nveu32_t                          mdc_cr;
  /** VLAN tag stripping enable(1) or disable(0) */
  nveu32_t                          strip_vlan_tag;
 #if !defined (L3L4_WILDCARD_FILTER)

  /** L3L4 filter bit bask, set index corresponding bit for
   * filter if filter enabled */
  nveu64_t                          l3l4_filter_bitmask;
 #endif /* !L3L4_WILDCARD_FILTER */
  /** Flag which decides virtualization is enabled(1) or disabled(0) */
  nveu32_t                          use_virtualization;
  /** HW supported feature list */
  struct osi_hw_features            *hw_feature;
  /** MC packets Multiple DMA channel selection flags */
  nveu32_t                          mc_dmasel;
  /** UPHY GBE mode (2 for 25F, 1 for 10G, 0 for 5G) */
  nveu32_t                          uphy_gbe_mode;
  /** number of PDMA's */
  nveu32_t                          num_of_pdma;
  /** Array of PDMA to VDMA mapping */
  struct osi_pdma_vdma_data         pdma_data[OSI_MGBE_MAX_NUM_PDMA_CHANS];
  /** Number of channels enabled in MAC */
  nveu32_t                          num_dma_chans;
  /** Array of supported DMA channels */
  nveu32_t                          dma_chans[OSI_MGBE_MAX_NUM_CHANS];
  /** Array of VM IRQ's */
  struct osi_vm_irq_data            irq_data[OSI_MAX_VM_IRQS];

  /** number of VM IRQ's
   * Fixed value filled by NvEthernet unit as 4*/
  nveu32_t                          num_vm_irqs;

  /** PHY interface mode (0/1 for XFI 10/5G, 2/3 for USXGMII 10/5)
   * (4 for XFI 25G) (5 for USXGMII 25G */
  nveu32_t                          phy_iface_mode;

  /** MGBE MAC instance ID's
   * valid values are from 0 to 4
   * 0 to 3 fo reach MGBE instance and 4 for EQOS */
  nveu32_t                          instance_id;

  /** Ethernet controller MAC to MAC Time sync role
   * valid values are NVETHERNETRM_PIF$OSI_PTP_M2M_INACTIVE,
   * NVETHERNETRM_PIF$OSI_PTP_M2M_PRIMARY and
   * NVETHERNETRM_PIF$OSI_PTP_M2M_SECONDARY
   */
  nveu32_t                          m2m_role;

  /** control pps output signal
   * 0(disable) and 1(enable) are the valid values */
  nveu32_t                          pps_frq;
 #ifdef HSI_SUPPORT
  struct osi_hsi_data               hsi;
 #endif
  /** pre-silicon flag */
  nveu32_t                          pre_sil;
  /** rCHlist bookkeeping **/
  struct rchlist_index              rch_index[RCHLIST_SIZE];
  /** Parameter indicates the current operating speed */
  nve32_t                           speed;
  /** PCS BASE-R FEC enable */
  nveu32_t                          pcs_base_r_fec_en;

  /** skip auto neg for usxgmii mode.
   * 0(enable AN) and 1(disable AN) are the valid values */
  nveu32_t                          skip_usxgmii_an;
  /** MAC common interrupt received */
  nveu32_t                          mac_common_intr_rcvd;
};

/**
 * @brief
 * Description: EQOS MAC, MTL and common DMA initialization.
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *   * Used Structure variables: osi_core.base, osi_core.num_mtl_queues, osi_core.mtl_queues
 *     * Refer NVETHERNETRM_PIF$osi_core_priv_data
 *
 * @pre
 * - MAC should be out of reset. See osi_poll_for_mac_reset_complete()
 *   for details.
 * - osi_core->base needs to be filled based on ioremap.
 * - osi_core->num_mtl_queues needs to be filled.
 * - osi_core->mtl_queues[qinx] need to be filled.
 *
 * @return
 *  - 0 on successful completion of the NVETHERNETRM_PIF#osi_hw_core_init/osi_core
 *    deinitialization operation.
 *  - -1 on NVETHERNETRM_PIF#osi_hw_core_init/osi_core core deinitialization operation fail
 *  - -1 on NVETHERNETRM_PIF#osi_hw_core_init/osi_core is NULL
 *
 * @note
 * - This API also indirectly programs Tx PBL. It must be made sure that
 *   the Tx FIFO is deep enough to store a complete packet before that packet
 *   is transferred to the MAC transmitter. The reason being that when space
 *   is not available to accept the programmed burst length of data, then the
 *   MTL Tx FIFO starts reading to avoid dead-lock. In such a case, the COE
 *   fails as the start of the packet header is read out before the payload
 *   checksum can be calculated and inserted.It must enable checksum insertion
 *   only in the packets that are less than the number of bytes, given by the
 *   following equation:
 *
 *   Packet size < TxQSize - (PBL + N)*(DATAWIDTH/8),
 *
 *   where, if Datawidth = 32, N = 7, elseif Datawidth != 32, N = 5
 *   and Packet size is determined by the osi_core->mtu.
 *
 *   The above is applicable only for Thor as PBL setting is per core PDMA.
 *   osi_core->mtu is same as the Platforma-max-MTU. Care must be taken that
 *   the platform-max-MTU is not greater than MTL Tx Qsize.
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
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_001
 */
#else

/**
 *
 * @dir
 * - forward
 */
#endif
nve32_t
osi_hw_core_init (
  struct osi_core_priv_data *const  osi_core
  );

/**
 * @brief
 * Description: EQOS MAC deinitialization.
 *
 * @pre MAC has to be out of reset.
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *
 * @return
 *  - 0 on successful completion of the NVETHERNETRM_PIF#osi_hw_core_deinit/osi_core
 *    deinitialization operation.
 *  - -1 on NVETHERNETRM_PIF#osi_hw_core_deinit/osi_core core deinitialization operation fail
 *  - -1 on NVETHERNETRM_PIF#osi_hw_core_deinit/osi_core is NULL
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
 *  - Run time: No
 *  - De-initialization: Yes
 *
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_002
 */
#else

/**
 *
 * @dir
 * - forward
 */
#endif
nve32_t
osi_hw_core_deinit (
  struct osi_core_priv_data *const  osi_core
  );

/**
 * @brief
 * Description: Write to a PHY register through MAC over MDIO bus.
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 *   * Range: 0 to UINT32_MAX
 * @param[in] phyreg: Register which needs to be written to PHY.
 *   * Range: 0 to UINT32_MAX
 * @param[in] phydata: Data to write to a PHY register.
 *   * Range: 0 to UINT32_MAX
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @return
 *  - 0 on NVETHERNETRM_PIF#osi_write_phy_reg/osi_core PHY register write operation success
 *  - -1 on NVETHERNETRM_PIF#osi_write_phy_reg/osi_core PHY register write operation fail
 *  - -1 on NVETHERNETRM_PIF#osi_write_phy_reg/osi_core is NULL
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
 *  - Run time: Yes
 *  - De-initialization: No
 *
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_003
 */
#else

/**
 *
 * @dir
 *  - forward
 */
#endif
nve32_t
osi_write_phy_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    phyreg,
  const nveu16_t                    phydata
  );

/**
 * @brief
 * Description: Read from a PHY register through MAC over MDIO bus.
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 *   * Range: 0 to UINT32_MAX
 * @param[in] phyreg: Register which needs to be read from PHY.
 *   * Range: 0 to UINT32_MAX
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @return
 *  - 0 on NVETHERNETRM_PIF#osi_read_phy_reg/osi_core PHY register read operation success
 *  - -1 on NVETHERNETRM_PIF#osi_read_phy_reg/osi_core PHY register read operation fail
 *  - -1 on NVETHERNETRM_PIF#osi_read_phy_reg/osi_core is NULL
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
 *  - Run time: Yes
 *  - De-initialization: No
 *
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_004
 *
 */
#else

/**
 *
 * @dir
 *  - forward
 */
#endif
nve32_t
osi_read_phy_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    phyreg
  );

/**
 * @brief
 * Description: initializing the core operations
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *   * Used Structure variables: osi_core.use_virtualization
 *    * Refer NVETHERNETRM_PIF$osi_core_priv_data/use_virtualization
 *
 * @pre Obtain a valid osi_core pointer by using the NVETHERNETRM_PIF#osi_get_core function.
 *
 * @return
 *  - 0 on successful initialization of the NVETHERNETRM_PIF#osi_init_core_ops/osi_core operation
 *  - -1 on NVETHERNETRM_PIF#osi_init_core_ops/osi_core is NULL
 *  - -1 on NVETHERNETRM_PIF#osi_init_core_ops/osi_core$use_virtualization>
 *    NVETHERNETRM_PIF$OSI_ENABLE
 *  - -1 on NVETHERNETRM_PIF#osi_init_core_ops/osi_core core operation init fail
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *   - Signal handler: No
 *   - Thread safe: No
 *   - Async/Sync: Sync
 *  - Required Privileges: None
 *  - API Group:
 *   - Initialization: Yes
 *   - Run time: No
 *   - De-initialization: No
 *
 */
#ifndef DOXYGEN_ICD

/**
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_005
 *
 */
#else

/**
 *
 * @dir
 *  - forward
 */
#endif
nve32_t
osi_init_core_ops (
  struct osi_core_priv_data *const  osi_core
  );

/**
 * @brief
 * Description: API designed to manage runtime IOCTL commands.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *   * Used Structure variables: none
 * @param[in] data: A pointer referring to the osi_ioctl data structure.
 *   * Range: A non-null pointer to a valid NVETHERNETRM_PIF$osi_ioctl structure.
 *   * Used Structure variables: data/all
 *    * Refer NVETHERNETRM_PIF$osi_ioctl
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
 * @return
 *  - 0 on successful execution of IOCTL command
 *  - -1 on IOCTL command exection fail
 *  - -1 on NVETHERNETRM_PIF#osi_handle_ioctl/osi_core is NULL
 *  - -1 on NVETHERNETRM_PIF#osi_handle_ioctl/data is NULL
 *
 */
#ifndef DOXYGEN_ICD

/**
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_006 to handle OSI_CMD_SUSPEND
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_007 to handle OSI_CMD_RESUME
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_008 to handle OSI_CMD_RXCSUM_OFFLOAD
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_009 to handle OSI_CMD_PAD_CALIBRATION
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_010 to handle OSI_CMD_READ_MMC
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_011 to handle OSI_CMD_CAP_TSC_PTP
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_012 to handle OSI_CMD_FREE_TS
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_013 to handle OSI_CMD_MAC_MTU
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_014 to handle OSI_CMD_CONFIG_EST
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_015 to handle OSI_CMD_SET_AVB
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_016 to handle OSI_CMD_GET_AVB
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_017 to handle OSI_CMD_CONF_M2M_TS
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_018 to handle OSI_CMD_GET_TX_TS
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_019 to handle OSI_CMD_SET_SPEED
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_020 to handle OSI_CMD_L3L4_FILTER
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_021 to handle OSI_CMD_L2_FILTER
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_022 to handle OSI_CMD_SET_SYSTOHW_TIME
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_023 to handle OSI_CMD_CONFIG_FRP
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_024 to handle OSI_CMD_CONFIG_FPE
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_025 to handle OSI_CMD_COMMON_ISR
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_026 to handle OSI_CMD_READ_HSI_ERR
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_027 to handle OSI_CMD_HSI_CONFIGURE
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_028 to handle OSI_CMD_CONFIG_PTP
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_029 to handle OSI_CMD_READ_STATS
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_030 to handle OSI_CMD_ADJ_FREQ
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_031 to handle OSI_CMD_ADJ_TIME
 *
 */
#else

/**
 *
 * @dir
 *  - forward
 */
#endif
nve32_t
osi_handle_ioctl (
  struct osi_core_priv_data  *osi_core,
  struct osi_ioctl           *data
  );

/**
 * @brief
 * Description: Get pointer to osi_core data structure.
 *
 * @pre OSD layer should use this as first API to get osi_core pointer and
 * use the same in remaning API invocation.
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
 * @return
 * - NVETHERNETRM_PIF$osi_core_priv_data pointer on each success exection.
 * - NULL if the total enabled VFs exceed the maximum core
 *   instances allowed (MAX_CORE_INSTANCES).
 *
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_032
 */
#else

/**
 *
 * @dir
 *  - forward
 */
#endif
struct osi_core_priv_data *
osi_get_core (
  void
  );

#ifdef PHY_PROG

/**
 * @brief
 * Description: Write to a PHY register through MAC over MDIO bus.
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 *   * Range: 0 to UINT32_MAX
 * @param[in] macMdioForAddrReg: Value to be written to MAC's MDIO address register for indirect PHY access
 *   * Range: 0 to UINT32_MAX
 * @param[in] macMdioForDataReg: Value to be written to MAC's MDIO data register for indirect PHY access
 *   * Range: 0 to UINT32_MAX
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @return
 *  - 0 on NVETHERNETRM_PIF#osi_write_phy_reg_dt PHY register write operation success
 *  - -1 on NVETHERNETRM_PIF#osi_write_phy_reg_dt mdio access timeout
 *  - -1 on NVETHERNETRM_PIF#osi_write_phy_reg_dt osi_core is NULL
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
 *  - Run time: Yes
 *  - De-initialization: Yes
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_043
 */
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_write_phy_reg_dt (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    macMdioForAddrReg,
  const nveu32_t                    macMdioForDataReg
  );

/**
 * @brief
 * Description: Read from a PHY register through MAC over MDIO bus.
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 *   * Range: 0 to UINT32_MAX
 * @param[in] macMdioForAddrReg: Value to be written to MAC's MDIO address register for indirect PHY access
 *   * Range: 0 to UINT32_MAX
 * @param[in] macMdioForDataReg: Value to be written to MAC's MDIO data register for indirect PHY access
 *   * Range: 0 to UINT32_MAX
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @return
 *  - Register value on success
 *  - -1 on NVETHERNETRM_PIF#osi_read_phy_reg_dt mdio access timeout
 *  - -1 on NVETHERNETRM_PIF#osi_read_phy_reg_dt osi_core is NULL
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
 *  - Run time: Yes
 *  - De-initialization: Yes
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETRM_044
 */
  #else

/**
 *
 * @dir
 *  - forward
 */
  #endif
nve32_t
osi_read_phy_reg_dt (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    macMdioForAddrReg,
  const nveu32_t                    macMdioForDataReg
  );

#endif /* PHY_PROG */

#ifdef FSI_EQOS_SUPPORT

/**
 * @brief
 * Description: Release the osi_core data structure.
 *
 * @pre OSD layer should use this as last API to release osi_core pointer and
 * shall not use after that.
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
 *  - Run time: No
 *  - De-initialization: Yes
 *
 * @return
 * 0 on success
 * -1 on failure
 *
 */
nve32_t
osi_release_core (
  struct osi_core_priv_data  *osi_core
  );

#endif

/**
 * @brief
 * Description: Initialiation of EQoS XPCS IP.
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *     * Refer NVETHERNETRM_PIF$osi_core_priv_data
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
 * @return
 * 0 on success
 * -1 on failure
 *
 */
nve32_t
eqos_xpcs_init (
  struct osi_core_priv_data  *osi_core
  );

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
  );

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
  );

/**
 * @brief
 * Description: Lane bringup of XPCS IP.
 *
 * @param[in] osi_core: A pointer to the osi_core_priv_data structure
 *   * Range: A non-null pointer to NVETHERNETRM_PIF$osi_core_priv_data structure.
 *     * Refer NVETHERNETRM_PIF$osi_core_priv_data
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
 * @return
 * 0 on success
 * -1 on failure
 *
 */
nve32_t
xpcs_lane_bring_up (
  struct osi_core_priv_data  *osi_core
  );

#endif /* INCLUDED_OSI_CORE_H */
