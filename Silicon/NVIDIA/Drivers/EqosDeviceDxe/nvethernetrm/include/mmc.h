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

#ifndef INCLUDED_MMC_H
#define INCLUDED_MMC_H

#include <nvethernet_type.h>
#include "osi_common.h"

#ifdef MACSEC_SUPPORT

/**
 * @brief The structure hold macsec statistics counters
 */
struct osi_macsec_mmc_counters {
  /** This counter provides the number of controller port macsec
   * untaged packets
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    rx_pkts_no_tag;

  /** This counter provides the number of controller port macsec
   * untaged packets validateFrame != strict
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    rx_pkts_untagged;

  /** This counter provides the number of invalid tag or icv packets
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    rx_pkts_bad_tag;

  /** This counter provides the number of no sc lookup hit or sc match
   * packets
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    rx_pkts_no_sa_err;

  /** This counter provides the number of no sc lookup hit or sc match
   * packets validateFrame != strict
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    rx_pkts_no_sa;

  /** This counter provides the number of late packets
   *received PN < lowest PN
   * valid values are between 0 and UINT64_MAX */
  nveu32_t     rx_pkts_late[OSI_MACSEC_SC_INDEX_MAX];

  /** This counter provides the number of overrun packets
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    rx_pkts_overrun;

  /** This counter provides the number of octets after IVC passing
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    rx_octets_validated;
  /** This counter provides the number of octets after decryption */
  nveul64_t    rx_octets_decrypted;

  /** This counter provides the number not valid packets
   * valid values are between 0 and UINT64_MAX */
  nveu32_t     rx_pkts_not_valid[OSI_MACSEC_SC_INDEX_MAX];

  /** This counter provides the number of invalid packets
   * valid values are between 0 and UINT64_MAX */
  nveu32_t     in_pkts_invalid[OSI_MACSEC_SC_INDEX_MAX];

  /** This counter provides the number of in packet delayed
   * valid values are between 0 and UINT64_MAX */
  nveu32_t     rx_pkts_delayed[OSI_MACSEC_SC_INDEX_MAX];

  /** This counter provides the number of in packets un checked
   * valid values are between 0 and UINT64_MAX */
  nveu32_t     rx_pkts_unchecked[OSI_MACSEC_SC_INDEX_MAX];

  /** This counter provides the number of in packets ok
   * valid values are between 0 and UINT64_MAX */
  nveu32_t     rx_pkts_ok[OSI_MACSEC_SC_INDEX_MAX];

  /** This counter provides the number of out packets untaged
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    tx_pkts_untaged;

  /** This counter provides the number of out too long
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    tx_pkts_too_long;

  /** This counter provides the number of out packets protected
   * valid values are between 0 and UINT64_MAX */
  nveu32_t     tx_pkts_protected[OSI_MACSEC_SC_INDEX_MAX];
  /** This counter provides the number of out packets encrypted */
  nveu32_t     tx_pkts_encrypted[OSI_MACSEC_SC_INDEX_MAX];

  /** This counter provides the number of out octets protected/
   * valid values are between 0 and UINT64_MAX */
  nveul64_t    tx_octets_protected;
  /** This counter provides the number of out octets encrypted */
  nveul64_t    tx_octets_encrypted;
};

#endif /* MACSEC_SUPPORT */
#endif /* INCLUDED_MMC_H */
