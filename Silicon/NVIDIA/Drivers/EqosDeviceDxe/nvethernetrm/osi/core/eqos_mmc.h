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

#ifndef INCLUDED_EQOS_MMC_H
#define INCLUDED_EQOS_MMC_H

/**
 * @addtogroup EQOS-MMC MMC HW register offsets
 *
 * @brief MMC HW register offsets
 * @{
 */
#define MMC_TXOCTETCOUNT_GB           0x00714U
#define MMC_TXPACKETCOUNT_GB          0x00718
#define MMC_TXBROADCASTPACKETS_G      0x0071c
#define MMC_TXMULTICASTPACKETS_G      0x00720
#define MMC_TX64OCTETS_GB             0x00724
#define MMC_TX65TO127OCTETS_GB        0x00728
#define MMC_TX128TO255OCTETS_GB       0x0072c
#define MMC_TX256TO511OCTETS_GB       0x00730
#define MMC_TX512TO1023OCTETS_GB      0x00734
#define MMC_TX1024TOMAXOCTETS_GB      0x00738
#define MMC_TXUNICASTPACKETS_GB       0x0073c
#define MMC_TXMULTICASTPACKETS_GB     0x00740
#define MMC_TXBROADCASTPACKETS_GB     0x00744
#define MMC_TXUNDERFLOWERROR          0x00748
#define MMC_TXSINGLECOL_G             0x0074c
#define MMC_TXMULTICOL_G              0x00750
#define MMC_TXDEFERRED                0x00754
#define MMC_TXLATECOL                 0x00758
#define MMC_TXEXESSCOL                0x0075c
#define MMC_TXCARRIERERROR            0x00760
#define MMC_TXOCTETCOUNT_G            0x00764
#define MMC_TXPACKETSCOUNT_G          0x00768
#define MMC_TXEXCESSDEF               0x0076c
#define MMC_TXPAUSEPACKETS            0x00770
#define MMC_TXVLANPACKETS_G           0x00774
#define MMC_TXOVERSIZE_G              0x00778
#define MMC_RXPACKETCOUNT_GB          0x00780
#define MMC_RXOCTETCOUNT_GB           0x00784
#define MMC_RXOCTETCOUNT_G            0x00788
#define MMC_RXBROADCASTPACKETS_G      0x0078c
#define MMC_RXMULTICASTPACKETS_G      0x00790
#define MMC_RXCRCERROR                0x00794
#define MMC_RXALIGNMENTERROR          0x00798
#define MMC_RXRUNTERROR               0x0079c
#define MMC_RXJABBERERROR             0x007a0
#define MMC_RXUNDERSIZE_G             0x007a4
#define MMC_RXOVERSIZE_G              0x007a8
#define MMC_RX64OCTETS_GB             0x007ac
#define MMC_RX65TO127OCTETS_GB        0x007b0
#define MMC_RX128TO255OCTETS_GB       0x007b4
#define MMC_RX256TO511OCTETS_GB       0x007b8
#define MMC_RX512TO1023OCTETS_GB      0x007bc
#define MMC_RX1024TOMAXOCTETS_GB      0x007c0
#define MMC_RXUNICASTPACKETS_G        0x007c4
#define MMC_RXLENGTHERROR             0x007c8
#define MMC_RXOUTOFRANGETYPE          0x007cc
#define MMC_RXPAUSEPACKETS            0x007d0
#define MMC_RXFIFOOVERFLOW            0x007d4
#define MMC_RXVLANPACKETS_GB          0x007d8
#define MMC_RXWATCHDOGERROR           0x007dc
#define MMC_RXRCVERROR                0x007e0
#define MMC_RXCTRLPACKETS_G           0x007e4
#define MMC_TXLPIUSECCNTR             0x007ec
#define MMC_TXLPITRANCNTR             0x007f0
#define MMC_RXLPIUSECCNTR             0x007f4
#define MMC_RXLPITRANCNTR             0x007f8
#define MMC_RXIPV4_GD_PKTS            0x00810
#define MMC_RXIPV4_HDRERR_PKTS        0x00814
#define MMC_RXIPV4_NOPAY_PKTS         0x00818
#define MMC_RXIPV4_FRAG_PKTS          0x0081c
#define MMC_RXIPV4_UBSBL_PKTS         0x00820
#define MMC_RXIPV6_GD_PKTS            0x00824
#define MMC_RXIPV6_HDRERR_PKTS        0x00828
#define MMC_RXIPV6_NOPAY_PKTS         0x0082c
#define MMC_RXUDP_GD_PKTS             0x00830
#define MMC_RXUDP_ERR_PKTS            0x00834
#define MMC_RXTCP_GD_PKTS             0x00838
#define MMC_RXTCP_ERR_PKTS            0x0083c
#define MMC_RXICMP_GD_PKTS            0x00840
#define MMC_RXICMP_ERR_PKTS           0x00844
#define MMC_RXIPV4_GD_OCTETS          0x00850
#define MMC_RXIPV4_HDRERR_OCTETS      0x00854
#define MMC_RXIPV4_NOPAY_OCTETS       0x00858
#define MMC_RXIPV4_FRAG_OCTETS        0x0085c
#define MMC_RXIPV4_UDSBL_OCTETS       0x00860
#define MMC_RXIPV6_GD_OCTETS          0x00864
#define MMC_RXIPV6_HDRERR_OCTETS      0x00868
#define MMC_RXIPV6_NOPAY_OCTETS       0x0086c
#define MMC_RXUDP_GD_OCTETS           0x00870
#define MMC_RXUDP_ERR_OCTETS          0x00874
#define MMC_RXTCP_GD_OCTETS           0x00878
#define MMC_RXTCP_ERR_OCTETS          0x0087c
#define MMC_RXICMP_GD_OCTETS          0x00880
#define MMC_RXICMP_ERR_OCTETS         0x00884
#define MMC_TX_FPE_FRAG_COUNTER       0x008A8
#define MMC_TX_HOLD_REQ_COUNTER       0x008AC
#define MMC_RX_PKT_ASSEMBLY_ERR_CNTR  0x008C8
#define MMC_RX_PKT_SMD_ERR_CNTR       0x008CC
#define MMC_RX_PKT_ASSEMBLY_OK_CNTR   0x008D0
#define MMC_RX_FPE_FRAG_CNTR          0x008D4
/** @} */

void
eqos_read_mmc (
  struct osi_core_priv_data *const  osi_core
  );

#endif /* INCLUDED_EQOS_MMC_H */
