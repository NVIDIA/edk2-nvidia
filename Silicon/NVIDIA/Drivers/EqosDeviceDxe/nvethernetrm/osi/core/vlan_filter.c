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

#include "../osi/common/common.h"
#include "vlan_filter.h"

/**
 * @brief get_vlan_filter_idx - Get VLAN HW filter index which match vlan_id
 *
 * Algorithm: From VID flags gets the set bit position and get the value
 * from that position and compare with vlan ID passed to this function
 *
 * @param[in] osi_core: OSI core private data
 * @param[in] vlan_id: VLAN ID to be searched in HW filters
 *
 * @return Index from VID array if match found.
 * @return Return VLAN_HW_FILTER_FULL_IDX if not found.
 */
static inline unsigned int get_vlan_filter_idx(
					struct osi_core_priv_data *osi_core,
					unsigned short vlan_id)
{
	unsigned int vid_idx = VLAN_HW_FILTER_FULL_IDX;
	unsigned long bitmap = osi_core->vf_bitmap;
	unsigned long temp = 0U;

	while (bitmap != 0U) {
		temp = __builtin_ctzl(bitmap);

		if (osi_core->vid[temp] == vlan_id) {
			/* vlan ID match found */
			vid_idx = temp;
			break;
		}

		bitmap &= ~OSI_BIT(temp);
	}

	return vid_idx;
}

/**
 * @brief allow_all_vid_tags - Program MAC to pass all VID
 *
 * Algorithm: Enable HASH filtering and program the hash to 0xFFFF
 * if all VID's to pass. For not to pass all VID's disable HASH
 * filtering and program the HASH as zero.
 *
 * @param[in] base: MAC base address
 * @param[in] pass_all_vids: Flag to pass all VID's or not.
 *
 * @return 0 on success
 */
static inline int allow_all_vid_tags(unsigned char *base,
				     unsigned int pass_all_vids)
{
	unsigned int vlan_tag_reg = 0;
	unsigned int hash_filter_reg = 0;

	vlan_tag_reg = osi_readl(base + MAC_VLAN_TAG_CTRL);
	hash_filter_reg = osi_readl(base + MAC_VLAN_HASH_FILTER);

	if (pass_all_vids == OSI_ENABLE) {
		vlan_tag_reg |= MAC_VLAN_TAG_CTRL_VHTM;
		hash_filter_reg |= VLAN_HASH_ALLOW_ALL;
	} else {
		vlan_tag_reg &= ~MAC_VLAN_TAG_CTRL_VHTM;
		hash_filter_reg &= ~VLAN_HASH_ALLOW_ALL;
	}

	osi_writel(vlan_tag_reg, base + MAC_VLAN_TAG_CTRL);
	osi_writel(hash_filter_reg, base + MAC_VLAN_HASH_FILTER);

	return 0;
}

/**
 * @brief is_vlan_id_enqueued - Checks passed VID already queued or not.
 *
 * Algorithm: Search VID array from index VLAN_HW_FILTER_FULL_IDX
 * to total VID programmed count. If match found return index to VID
 * array or return negative value if no match.
 *
 * @param[in] osi_core: OSI core private data
 * @param[in] vlan_id: VLAN ID to be searched in VID array.
 * @param[out] idx: Index of the VID array after match
 *
 * @return 0 on Success.
 * @return negative value on failure
 */
static inline int is_vlan_id_enqueued(struct osi_core_priv_data *osi_core,
				      unsigned short vlan_id,
				      unsigned int *idx)
{
	unsigned int i = 0;

	if (osi_core->vlan_filter_cnt == VLAN_HW_FILTER_FULL_IDX) {
		/* No elements in SW queue to search */
		return -1;
	}

	for (i = VLAN_HW_FILTER_FULL_IDX; i <= osi_core->vlan_filter_cnt; i++) {
		if (osi_core->vid[i] == vlan_id) {
			*idx = i;
			/* match found */
			return 0;
		}
	}

	return -1;
}

/**
 * @brief enqueue_vlan_id - ADD vlan_id to VID array.
 *
 * Algorithm: Add VLAN ID to VID array at filter_cnt index.
 *
 * @param[in] osi_core: OSI core private data
 * @param[in] vlan_id: VLAN ID to be added to VID array.
 *
 * @return 0 on success.
 * @return negative value on failure.
 */
static inline int enqueue_vlan_id(struct osi_core_priv_data *osi_core,
				  unsigned short vlan_id)
{
	int ret = 0;
	unsigned int idx;

	if (osi_core->vlan_filter_cnt == VLAN_NUM_VID) {
		/* Entire SW queue full */
		return -1;
	}

	/* Check if requested vlan_id alredy queued */
	ret = is_vlan_id_enqueued(osi_core, vlan_id, &idx);
	if (ret == 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			"VLAN ID already programmed\n",
			0ULL);
		return -1;
	}

	osi_core->vid[osi_core->vlan_filter_cnt] = vlan_id;
	osi_core->vlan_filter_cnt++;

	return 0;
}

/**
 * @brief poll_for_vlan_filter_reg_rw - Poll for VLAN filter register update
 *
 * Algorithm: Program VLAN filter registers through indirect address
 * mechanism.
 *
 * @param[in] addr: MAC base address
 *
 * @return 0 on success.
 * @return -1 on failure.
 */
static inline int poll_for_vlan_filter_reg_rw(
					struct osi_core_priv_data *osi_core)
{
	unsigned int retry = 10;
	unsigned int count;
	unsigned int val = 0;
	int cond = 1;

	count = 0;
	while (cond == 1) {
		if (count > retry) {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
				     "VLAN filter update timedout\n", 0ULL);
			return -1;
		}

		count++;

		val = osi_readl((unsigned char *)osi_core->base +
				MAC_VLAN_TAG_CTRL);
		if ((val & MAC_VLAN_TAG_CTRL_OB) == OSI_NONE) {
			/* Set cond to 0 to exit loop */
			cond = 0;
		} else {
			/* wait for 10 usec for XB clear */
			osi_core->osd_ops.udelay(10U);
		}
	}

	return 0;
}

/**
 * @brief update_vlan_filters - Update HW filter registers
 *
 * Algorithm: Program VLAN HW filter registers through indirect
 * address mechanism.
 *
 * @param[in] base: MAC base address.
 * @param[in] vid_idx: HW filter index in VLAN filter registers.
 * @param[in] val: VLAN ID to be programmed.
 *
 * @return 0 on success
 * @return -1 on failure.
 */
static inline int update_vlan_filters(struct osi_core_priv_data *osi_core,
				      unsigned int vid_idx,
				      unsigned int val)
{
	unsigned char *base = (unsigned char *)osi_core->base;
	int ret = 0;

	osi_writel(val, base + MAC_VLAN_TAG_DATA);

	val = osi_readl(base + MAC_VLAN_TAG_CTRL);
	val &= ~MAC_VLAN_TAG_CTRL_OFS_MASK;
	val |= vid_idx << MAC_VLAN_TAG_CTRL_OFS_SHIFT;
	val &= ~MAC_VLAN_TAG_CTRL_CT;
	val |= MAC_VLAN_TAG_CTRL_OB;
	osi_writel(val, base + MAC_VLAN_TAG_CTRL);

	ret = poll_for_vlan_filter_reg_rw(osi_core);
	if (ret < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			     "Failed to update VLAN filters\n", 0ULL);
		return -1;
	}

	return 0;
}

/**
 * @brief add_vlan_id - Add VLAN ID.
 *
 * Algorithm: ADD VLAN ID to HW filters and SW VID array.
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] val: VLAN ID to be programmed.
 *
 * @return 0 on success
 * @return -1 on failure.
 */
static inline int add_vlan_id(struct osi_core_priv_data *osi_core,
			      struct core_ops *ops_p,
			      unsigned short vlan_id)
{
	unsigned int vid_idx = 0;
	unsigned int val = 0;
	int ret = 0;

	/* Check if VLAN ID already programmed */
	vid_idx = get_vlan_filter_idx(osi_core, vlan_id);
	if (vid_idx != VLAN_HW_FILTER_FULL_IDX) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"VLAN ID already added\n",
			0ULL);
		return -1;
	}

	/* Get free index to add the VID */
	vid_idx = __builtin_ctzl(~osi_core->vf_bitmap);
	/* If there is no free filter index add into SW VLAN filter queue to store */
	if (vid_idx == VLAN_HW_FILTER_FULL_IDX) {
		/* Add VLAN ID to SW queue */
		ret = enqueue_vlan_id(osi_core, vlan_id);
		if (ret < 0)
			return ret;

		/* Since VLAN HW filters full - program to allow all packets */
		return allow_all_vid_tags(osi_core->base, OSI_ENABLE);
	}

	osi_core->vf_bitmap |= OSI_BIT(vid_idx);
	osi_core->vid[vid_idx] = vlan_id;
	osi_core->vlan_filter_cnt++;

	if (osi_core->vlan_filter_cnt > 0U) {
		ret = ops_p->config_vlan_filtering(osi_core,
						   OSI_ENABLE,
						   OSI_DISABLE,
						   OSI_DISABLE);
		if (ret < 0) {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				"Failed to enable VLAN filtering\n", 0ULL);
			return -1;
		}
	}

	val = osi_readl((unsigned char *)osi_core->base + MAC_VLAN_TAG_DATA);
	val &= ~VLAN_VID_MASK;
	val |= (vlan_id | MAC_VLAN_TAG_DATA_ETV | MAC_VLAN_TAG_DATA_VEN);

	return update_vlan_filters(osi_core, vid_idx, val);
}

/**
 * @brief dequeue_vlan_id - Remove VLAN ID from VID array
 *
 * Algorithm: Do the left shift of array from index to
 * total filter count. Allow all VID tags after removal
 * VID if numbers of filter count is 32.
 *
 * @param[in]: osi_core: OSI core private data.
 * @param[in] idx: Index at which VLAN ID to be deleted.
 *
 * @return 0 on success
 * @return -1 on failure.
 */
static inline int dequeue_vlan_id(struct osi_core_priv_data *osi_core,
				  unsigned short idx)
{
	unsigned int i;

	if (osi_core->vlan_filter_cnt == VLAN_HW_MAX_NRVF) {
		return -1;
	}

	/* Left shift the array elements by one for the VID order */
	for (i = idx; i <= osi_core->vlan_filter_cnt; i++) {
		osi_core->vid[i] = osi_core->vid[i + 1];
	}

	osi_core->vid[i] = VLAN_ID_INVALID;
	osi_core->vlan_filter_cnt--;

	if (osi_core->vlan_filter_cnt == VLAN_HW_MAX_NRVF) {
		allow_all_vid_tags(osi_core->base, OSI_DISABLE);
	}

	return 0;
}

/**
 * @brief dequeue_vid_to_add_filter_reg - Get VID from Array and add to HW
 * filters.
 *
 * Algorithm: Get the VID from index 32 and program in HW filter
 * registers. With this first added VID will be programmed in filter registers
 * if any VID deleted from HW filter registers.
 *
 * @param[in]: osi_core: OSI core private data.
 * @param[in] idx: Index at which VLAN ID to be deleted.
 *
 * @return 0 on success
 * @return -1 on failure.
 */
static inline int dequeue_vid_to_add_filter_reg(
					struct osi_core_priv_data *osi_core,
					unsigned int vid_idx)
{
	unsigned int val = 0;
	unsigned short vlan_id = 0;
	unsigned int i = 0;
	int ret = 0;

	vlan_id = osi_core->vid[VLAN_HW_FILTER_FULL_IDX];
	if (vlan_id == VLAN_ID_INVALID) {
		return 0;
	}

	osi_core->vf_bitmap |= OSI_BIT(vid_idx);
	osi_core->vid[vid_idx] = vlan_id;

	val = osi_readl((unsigned char *)osi_core->base + MAC_VLAN_TAG_DATA);
	val &= ~VLAN_VID_MASK;
	val |= (vlan_id | MAC_VLAN_TAG_DATA_ETV | MAC_VLAN_TAG_DATA_VEN);

	ret = update_vlan_filters(osi_core, vid_idx, val);
	if (ret < 0) {
		return -1;
	}

	for (i = VLAN_HW_FILTER_FULL_IDX; i <=  osi_core->vlan_filter_cnt; i++) {
		osi_core->vid[i] = osi_core->vid[i + 1];
	}

	osi_core->vid[i] = VLAN_ID_INVALID;

	return 0;
}

/**
 * @brief del_vlan_id - Delete VLAN ID.
 *
 * Algorithm: Delete VLAN ID from HW filters or SW VID array.
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] val: VLAN ID to be deleted
 *
 * @return 0 on success
 * @return -1 on failure.
 */
static inline int del_vlan_id(struct osi_core_priv_data *osi_core,
			      struct core_ops *ops_p,
			      unsigned short vlan_id)
{
	unsigned int vid_idx = 0;
	unsigned int val = 0;
	unsigned int idx;
	int ret = 0;

	/* Search for vlan filter index to be deleted */
	vid_idx = get_vlan_filter_idx(osi_core, vlan_id);
	if (vid_idx == VLAN_HW_FILTER_FULL_IDX) {
		ret = is_vlan_id_enqueued(osi_core, vlan_id, &idx);
		if (ret == 0) {
			/* VID found to be deleted in SW queue */
			return dequeue_vlan_id(osi_core, idx);
		}
	}

	osi_core->vf_bitmap &= ~OSI_BIT(vid_idx);
	osi_core->vid[vid_idx] = VLAN_ID_INVALID;

	ret = update_vlan_filters(osi_core, vid_idx, val);
	if (ret < 0) {
		return -1;
	}

	osi_core->vlan_filter_cnt--;

	if (osi_core->vlan_filter_cnt == 0U) {
		ret = ops_p->config_vlan_filtering(osi_core,
						   OSI_DISABLE,
						   OSI_DISABLE,
						   OSI_DISABLE);
		if (ret < 0) {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				"Failed to disable VLAN filtering\n", 0ULL);
			return -1;
		}
	}

	if (osi_core->vlan_filter_cnt == VLAN_HW_MAX_NRVF) {
		allow_all_vid_tags(osi_core->base, OSI_DISABLE);
	}

	/* if SW queue is not empty dequeue from SW queue and update filter */
	return dequeue_vid_to_add_filter_reg(osi_core, vid_idx);
}

int update_vlan_id(struct osi_core_priv_data *osi_core,
		   struct core_ops *ops_p,
		   unsigned int vid)
{
	unsigned int action = vid & VLAN_ACTION_MASK;
	unsigned short vlan_id = vid & VLAN_VID_MASK;

	if (action == OSI_VLAN_ACTION_ADD) {
		return add_vlan_id(osi_core, ops_p, vlan_id);
	}

	return del_vlan_id(osi_core, ops_p, vlan_id);
}
