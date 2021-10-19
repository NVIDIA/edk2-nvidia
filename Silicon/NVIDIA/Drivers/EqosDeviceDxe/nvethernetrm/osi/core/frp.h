/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#ifndef FRP_H
#define FRP_H

#include <osi_common.h>
#include <osi_core.h>
#include "core_local.h"

#define FRP_MD_SIZE			(4U)
#define FRP_ME_BYTE			(0xFFU)
#define FRP_ME_BYTE_SHIFT		(8U)

/* Offset defines for Match data types */
#define FRP_L2_DA_OFFSET		0U
#define FRP_L2_SA_OFFSET		6U
#define FRP_L2_VLAN_TAG_OFFSET		14U
#define FRP_L3_IP4_SIP_OFFSET		26U
#define FRP_L3_IP4_DIP_OFFSET		30U
#define FRP_L4_IP4_SPORT_OFFSET		34U
#define FRP_L4_IP4_DPORT_OFFSET		36U

/* Protocols Match data define values  */
#define FRP_PROTO_LENGTH		2U
#define FRP_L2_VLAN_PROTO_OFFSET	12U
#define FRP_L2_VLAN_MD0			0x81U
#define FRP_L2_VLAN_MD1			0x00U
#define FRP_L4_IP4_PROTO_OFFSET		23U
#define FRP_L4_UDP_MD			17U
#define FRP_L4_TCP_MD			6U

/* Define for FRP Entries offsets and lengths */
#define FRP_OFFSET_BYTES(offset) \
	(FRP_MD_SIZE - ((offset) % FRP_MD_SIZE))

/**
 * @brief setup_frp - Process OSD FRP Command.
 *
 * Algorithm: Parse give FRP command and update it on OSI data and HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
int setup_frp(struct osi_core_priv_data *const osi_core,
	      struct core_ops *ops_p,
	      struct osi_core_frp_cmd *const cmd);

/**
 * @brief init_frp - Init the FRP Instruction Table.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 *	1) MAC and PHY should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
void init_frp(struct osi_core_priv_data *const osi_core);

#endif /* FRP_H */
