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

#ifndef VLAN_FILTER_H
#define VLAN_FILTER_H

#include <osi_core.h>
#include "core_local.h"

/**
 * @addtogroup MAC-VLAN MAC VLAN configuration registers and bit fields
 *
 * @brief These are the macros for register offsets and bit fields
 * for VLAN configuration.
 * @{
 */
#define MAC_VLAN_TAG_CTRL	0x50
#define MAC_VLAN_TAG_DATA	0x54
#define MAC_VLAN_HASH_FILTER	0x58
#define MAC_VLAN_TAG_CTRL_OFS_MASK	0x7C
#define MAC_VLAN_TAG_CTRL_OFS_SHIFT	2U
#define MAC_VLAN_TAG_CTRL_CT	OSI_BIT(1)
#define MAC_VLAN_TAG_CTRL_OB	OSI_BIT(0)
#define MAC_VLAN_TAG_CTRL_VHTM	OSI_BIT(25)
#define MAC_VLAN_TAG_DATA_ETV	OSI_BIT(16)
#define MAC_VLAN_TAG_DATA_VEN	OSI_BIT(17)
/** @} */

/**
 * @addtogroup VLAN filter macros
 *
 * @brief VLAN filtering releated macros
 * @{
 */
#define VLAN_HW_MAX_NRVF	32U
#define VLAN_HW_FILTER_FULL_IDX	VLAN_HW_MAX_NRVF
#define VLAN_VID_MASK		0xFFFF
#define VLAN_ID_INVALID		0xFFFF
#define VLAN_HASH_ALLOW_ALL	0xFFFF
#define VLAN_ACTION_MASK	OSI_BIT(31)
/** @} */

/**
 * @brief update_vlan_id - Add/Delete VLAN ID.
 *
 * Algorithm: Add/deleted VLAN ID from HW filters or SW VID array.
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] vid: VLAN ID to be added/deleted
 *
 * @return 0 on success
 * @return -1 on failure.
 */
int update_vlan_id(struct osi_core_priv_data *osi_core,
		   struct core_ops *ops_p,
                   unsigned int vid);
#endif /* VLAN_FILTER_H */
