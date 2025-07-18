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

#ifndef INCLUDED_MGBE_MMC_H_
#define INCLUDED_MGBE_MMC_H_

/**
 * @addtogroup MGBE-MMC MMC HW register offsets
 *
 * @brief MMC HW register offsets
 * @{
 */
#define MMC_TXOCTETCOUNT_GB_L        0x00814
#define MMC_TXOCTETCOUNT_GB_H        0x00818
#define MMC_TXPACKETCOUNT_GB_L       0x0081C
#define MMC_TXPACKETCOUNT_GB_H       0x00820
#define MMC_TXBROADCASTPACKETS_G_L   0x00824
#define MMC_TXBROADCASTPACKETS_G_H   0x00828
#define MMC_TXMULTICASTPACKETS_G_L   0x0082C
#define MMC_TXMULTICASTPACKETS_G_H   0x00830
#define MMC_TX64OCTETS_GB_L          0x00834
#define MMC_TX64OCTETS_GB_H          0x00838
#define MMC_TX65TO127OCTETS_GB_L     0x0083C
#define MMC_TX65TO127OCTETS_GB_H     0x00840
#define MMC_TX128TO255OCTETS_GB_L    0x00844
#define MMC_TX128TO255OCTETS_GB_H    0x00848
#define MMC_TX256TO511OCTETS_GB_L    0x0084C
#define MMC_TX256TO511OCTETS_GB_H    0x00850
#define MMC_TX512TO1023OCTETS_GB_L   0x00854
#define MMC_TX512TO1023OCTETS_GB_H   0x00858
#define MMC_TX1024TOMAXOCTETS_GB_L   0x0085C
#define MMC_TX1024TOMAXOCTETS_GB_H   0x00860
#define MMC_TXUNICASTPACKETS_GB_L    0x00864
#define MMC_TXUNICASTPACKETS_GB_H    0x00868
#define MMC_TXMULTICASTPACKETS_GB_L  0x0086C
#define MMC_TXMULTICASTPACKETS_GB_H  0x00870
#define MMC_TXBROADCASTPACKETS_GB_L  0x00874
#define MMC_TXBROADCASTPACKETS_GB_H  0x00878
#define MMC_TXUNDERFLOWERROR_L       0x0087C
#define MMC_TXUNDERFLOWERROR_H       0x00880
#define MMC_TXOCTETCOUNT_G_L         0x00884
#define MMC_TXOCTETCOUNT_G_H         0x00888
#define MMC_TXPACKETSCOUNT_G_L       0x0088C
#define MMC_TXPACKETSCOUNT_G_H       0x00890
#define MMC_TXPAUSEPACKETS_L         0x00894
#define MMC_TXPAUSEPACKETS_H         0x00898
#define MMC_TXVLANPACKETS_G_L        0x0089C
#define MMC_TXVLANPACKETS_G_H        0x008A0
#define MMC_TXLPIUSECCNTR            0x008A4
#define MMC_TXLPITRANCNTR            0x008A8

#define MMC_RXPACKETCOUNT_GB_L          0x00900
#define MMC_RXPACKETCOUNT_GB_H          0x00904
#define MMC_RXOCTETCOUNT_GB_L           0x00908
#define MMC_RXOCTETCOUNT_GB_H           0x0090C
#define MMC_RXOCTETCOUNT_G_L            0x00910
#define MMC_RXOCTETCOUNT_G_H            0x00914
#define MMC_RXBROADCASTPACKETS_G_L      0x00918
#define MMC_RXBROADCASTPACKETS_G_H      0x0091C
#define MMC_RXMULTICASTPACKETS_G_L      0x00920
#define MMC_RXMULTICASTPACKETS_G_H      0x00924
#define MMC_RXCRCERROR_L                0x00928
#define MMC_RXCRCERROR_H                0x0092C
#define MMC_RXRUNTERROR                 0x00930
#define MMC_RXJABBERERROR               0x00934
#define MMC_RXUNDERSIZE_G               0x00938
#define MMC_RXOVERSIZE_G                0x0093C
#define MMC_RX64OCTETS_GB_L             0x00940
#define MMC_RX64OCTETS_GB_H             0x00944
#define MMC_RX65TO127OCTETS_GB_L        0x00948
#define MMC_RX65TO127OCTETS_GB_H        0x0094C
#define MMC_RX128TO255OCTETS_GB_L       0x00950
#define MMC_RX128TO255OCTETS_GB_H       0x00954
#define MMC_RX256TO511OCTETS_GB_L       0x00958
#define MMC_RX256TO511OCTETS_GB_H       0x0095C
#define MMC_RX512TO1023OCTETS_GB_L      0x00960
#define MMC_RX512TO1023OCTETS_GB_H      0x00964
#define MMC_RX1024TOMAXOCTETS_GB_L      0x00968
#define MMC_RX1024TOMAXOCTETS_GB_H      0x0096C
#define MMC_RXUNICASTPACKETS_G_L        0x00970
#define MMC_RXUNICASTPACKETS_G_H        0x00974
#define MMC_RXLENGTHERROR_L             0x00978
#define MMC_RXLENGTHERROR_H             0x0097C
#define MMC_RXOUTOFRANGETYPE_L          0x00980
#define MMC_RXOUTOFRANGETYPE_H          0x00984
#define MMC_RXPAUSEPACKETS_L            0x00988
#define MMC_RXPAUSEPACKETS_H            0x0098C
#define MMC_RXFIFOOVERFLOW_L            0x00990
#define MMC_RXFIFOOVERFLOW_H            0x00994
#define MMC_RXVLANPACKETS_GB_L          0x00998
#define MMC_RXVLANPACKETS_GB_H          0x0099C
#define MMC_RXWATCHDOGERROR             0x009A0
#define MMC_RXLPIUSECCNTR               0x009A4
#define MMC_RXLPITRANCNTR               0x009A8
#define MMC_RXALIGNMENTERROR            0x009BC
#define MMC_TX_FPE_FRAG_COUNTER         0x00A08
#define MMC_TX_HOLD_REQ_COUNTER         0x00A0C
#define MMC_RX_PKT_ASSEMBLY_ERR_CNTR    0x00A28
#define MMC_RX_PKT_SMD_ERR_CNTR         0x00A2C
#define MMC_RX_PKT_ASSEMBLY_OK_CNTR     0x00A30
#define MMC_RX_FPE_FRAG_CNTR            0x00A34
#define MMC_TXSINGLECOL_G               0x00A40
#define MMC_TXMULTICOL_G                0x00A44
#define MMC_TXDEFERRED                  0x00A48
#define MMC_TXLATECOL                   0x00A4C
#define MMC_TXEXESSCOL                  0x00A50
#define MMC_TXCARRIERERROR              0x00A54
#define MMC_TXEXECESS_DEFERRED          0x00A58
#define MMC_RXIPV4_GD_PKTS_L            0x00A64
#define MMC_RXIPV4_GD_PKTS_H            0x00A68
#define MMC_RXIPV4_HDRERR_PKTS_L        0x00A6C
#define MMC_RXIPV4_HDRERR_PKTS_H        0x00A70
#define MMC_RXIPV4_NOPAY_PKTS_L         0x00A74
#define MMC_RXIPV4_NOPAY_PKTS_H         0x00A78
#define MMC_RXIPV4_FRAG_PKTS_L          0x00A7C
#define MMC_RXIPV4_FRAG_PKTS_H          0x00A80
#define MMC_RXIPV4_UBSBL_PKTS_L         0x00A84
#define MMC_RXIPV4_UBSBL_PKTS_H         0x00A88
#define MMC_RXIPV6_GD_PKTS_L            0x00A8C
#define MMC_RXIPV6_GD_PKTS_H            0x00A90
#define MMC_RXIPV6_HDRERR_PKTS_L        0x00A94
#define MMC_RXIPV6_HDRERR_PKTS_H        0x00A98
#define MMC_RXIPV6_NOPAY_PKTS_L         0x00A9C
#define MMC_RXIPV6_NOPAY_PKTS_H         0x00AA0
#define MMC_RXUDP_GD_PKTS_L             0x00AA4
#define MMC_RXUDP_GD_PKTS_H             0x00AA8
#define MMC_RXUDP_ERR_PKTS_L            0x00AAC
#define MMC_RXUDP_ERR_PKTS_H            0x00AB0
#define MMC_RXTCP_GD_PKTS_L             0x00AB4
#define MMC_RXTCP_GD_PKTS_H             0x00AB8
#define MMC_RXTCP_ERR_PKTS_L            0x00ABC
#define MMC_RXTCP_ERR_PKTS_H            0x00AC0
#define MMC_RXICMP_GD_PKTS_L            0x00AC4
#define MMC_RXICMP_GD_PKTS_H            0x00AC8
#define MMC_RXICMP_ERR_PKTS_L           0x00ACC
#define MMC_RXICMP_ERR_PKTS_H           0x00AD0
#define MMC_RXIPV4_GD_OCTETS_L          0x00AD4
#define MMC_RXIPV4_GD_OCTETS_H          0x00AD8
#define MMC_RXIPV4_HDRERR_OCTETS_L      0x00ADC
#define MMC_RXIPV4_HDRERR_OCTETS_H      0x00AE0
#define MMC_RXIPV4_NOPAY_OCTETS_L       0x00AE4
#define MMC_RXIPV4_NOPAY_OCTETS_H       0x00AE8
#define MMC_RXIPV4_FRAG_OCTETS_L        0x00AEC
#define MMC_RXIPV4_FRAG_OCTETS_H        0x00AF0
#define MMC_RXIPV4_UDP_CHKSM_DIS_OCT_L  0x00AF4
#define MMC_RXIPV4_UDP_CHKSM_DIS_OCT_H  0x00AF8
#define MMC_RXIPV6_GD_OCTETS_L          0x00AFC
#define MMC_RXIPV6_GD_OCTETS_H          0x00B00
#define MMC_RXIPV6_HDRERR_OCTETS_L      0x00B04
#define MMC_RXIPV6_HDRERR_OCTETS_H      0x00B08
#define MMC_RXIPV6_NOPAY_OCTETS_L       0x00B0C
#define MMC_RXIPV6_NOPAY_OCTETS_H       0x00B10
#define MMC_RXUDP_GD_OCTETS_L           0x00B14
#define MMC_RXUDP_GD_OCTETS_H           0x00B18
#define MMC_RXUDP_ERR_OCTETS_L          0x00B1C
#define MMC_RXUDP_ERR_OCTETS_H          0x00B20
#define MMC_RXTCP_GD_OCTETS_L           0x00B24
#define MMC_RXTCP_GD_OCTETS_H           0x00B28
#define MMC_RXTCP_ERR_OCTETS_L          0x00B2C
#define MMC_RXTCP_ERR_OCTETS_H          0x00B30
#define MMC_RXICMP_GD_OCTETS_L          0x00B34
#define MMC_RXICMP_GD_OCTETS_H          0x00B38
#define MMC_RXICMP_ERR_OCTETS_L         0x00B3C
#define MMC_RXICMP_ERR_OCTETS_H         0x00B40
/** @} */

/**
 * @brief mgbe_read_mmc - To read MMC registers and ether_mmc_counter structure
 *         variable
 *
 * Algorithm: Pass register offset and old value to helper function and
 *         update structure.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 *      1) MAC should be init and started. see osi_start_mac()
 *      2) osi_core->osd should be populated
 */
void
mgbe_read_mmc (
  struct osi_core_priv_data *const  osi_core
  );

#endif /* INCLUDED_MGBE_MMC_H */
