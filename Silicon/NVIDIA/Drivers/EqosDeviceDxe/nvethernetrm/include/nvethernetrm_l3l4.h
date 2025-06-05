/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_NVETHERNETRM_L3L4_H
#define INCLUDED_NVETHERNETRM_L3L4_H

#include <nvethernet_type.h>

/** helper macro for enable */
#define OSI_L3L4_ENABLE  (1U)

/** helper macro to disable */
#define OSI_L3L4_DISABLE  (0U)

/** helper macro for enable */
#define OSI_TRUE  (OSI_L3L4_ENABLE)

/** helper macro to disable */
#define OSI_FALSE  (OSI_L3L4_DISABLE)

/**
 * @brief L3/L4 filter function dependent parameter
 */
struct osi_l3_l4_filter {
  struct {
 #ifndef OSI_STRIPPED_LIB
    /** udp (OSI_L3L4_ENABLE) or tcp (OSI_L3L4_DISABLE) */
    nveu32_t    is_udp;
    /** ipv6 (OSI_L3L4_ENABLE) or ipv4 (OSI_L3L4_DISABLE) */
    nveu32_t    is_ipv6;

    /** match combined L3, L4 filters (OSI_TRUE) or ignore L3,L4
     * combined filter match (OSI_FALSE) */
    nveu32_t    is_l3l4_match_en;
 #endif /* !OSI_STRIPPED_LIB */
    struct {
      /** ipv4 address
       * valid values from 0 to 0xFF in each array element */
      nveu8_t     ip4_addr[4];
 #ifndef OSI_STRIPPED_LIB
      /** ipv6 address */
      nveu16_t    ip6_addr[8];
      /** Port number */
      nveu16_t    port_no;
      /** addr match enable (OSI_L3L4_ENABLE) or disable (OSI_L3L4_DISABLE) */
      nveu32_t    addr_match;

      /** perfect(OSI_L3L4_DISABLE) or inverse(OSI_L3L4_ENABLE)
       * match for address */
      nveu32_t    addr_match_inv;
      /** port match enable (OSI_L3L4_ENABLE) or disable (OSI_L3L4_DISABLE) */
      nveu32_t    port_match;
      /** perfect(OSI_L3L4_DISABLE) or inverse(OSI_L3L4_ENABLE) match for port */
      nveu32_t    port_match_inv;
 #endif /* !OSI_STRIPPED_LIB */
    } dst;
 #ifndef OSI_STRIPPED_LIB
    /** ip address and port information */
    struct {
      /** ipv4 address */
      nveu8_t     ip4_addr[4];
      /** ipv6 address */
      nveu16_t    ip6_addr[8];
      /** Port number */
      nveu16_t    port_no;
      /** addr match enable (OSI_L3L4_ENABLE) or disable (OSI_L3L4_DISABLE) */
      nveu32_t    addr_match;

      /** perfect(OSI_L3L4_DISABLE) or inverse(OSI_L3L4_ENABLE)
       * match for address */
      nveu32_t    addr_match_inv;
      /** port match enable (OSI_L3L4_ENABLE) or disable (OSI_L3L4_DISABLE) */
      nveu32_t    port_match;
      /** perfect(OSI_L3L4_DISABLE) or inverse(OSI_L3L4_ENABLE) match for port */
      nveu32_t    port_match_inv;
    } src;
 #endif /* !OSI_STRIPPED_LIB */
  } data;
 #ifndef OSI_STRIPPED_LIB
  /** Represents whether DMA routing enabled (OSI_L3L4_ENABLE) or not (OSI_L3L4_DISABLE) */
  nveu32_t    dma_routing_enable;
 #endif /* !OSI_STRIPPED_LIB */

  /** DMA channel number if routing enabled
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_EQOS_MAX_NUM_CHANS for EQOS
   * and 0 to NVETHERNETRM_PIF$OSI_MGBE_MAX_NUM_CHANS for MGBE */
  nveu32_t    dma_chan;
  /** filter enable (OSI_L3L4_ENABLE) or disable (OSI_L3L4_DISABLE) */
  nveu32_t    filter_enb_dis;
};

#endif /* INCLUDED_NVETHERNETRM_L3L4_H */
