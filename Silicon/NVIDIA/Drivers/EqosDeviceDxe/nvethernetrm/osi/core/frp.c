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
#include "frp.h"

/**
 * @brief frp_entry_copy - Copy FRP entry
 *
 * Algorithm: Copy source FRP entry data into destination entry
 *
 * @param[in] dst: Destination FRP entry pointer.
 * @param[in] src: source FRP entry pointer.
 *
 */
static void frp_entry_copy(struct osi_core_frp_entry *dst,
			   struct osi_core_frp_entry *src)
{
	dst->frp_id = src->frp_id;
	dst->data.match_data = src->data.match_data;
	dst->data.match_en = src->data.match_en;
	dst->data.accept_frame = src->data.accept_frame;
	dst->data.reject_frame = src->data.reject_frame;
	dst->data.inverse_match = src->data.inverse_match;
	dst->data.next_ins_ctrl = src->data.next_ins_ctrl;
	dst->data.frame_offset = src->data.frame_offset;
	dst->data.ok_index = src->data.ok_index;
	dst->data.dma_chsel = src->data.dma_chsel;
}

/**
 * @brief frp_entry_find - Find FRP entry in table
 *
 * Algorithm: Parse FRP table for given ID and bookmark
 * start position and count of entries for a given ID.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] frp_id: FRP ID to find.
 * @param[out] start: Pointer to store start index for given frp_id.
 * @param[out] no_entries: No of FRP index's used for given frp_id.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static int frp_entry_find(struct osi_core_priv_data *const osi_core,
			  int frp_id,
			  unsigned char *start,
			  unsigned char *no_entries)
{
	unsigned char count = OSI_NONE, found = OSI_NONE;
	struct osi_core_frp_entry *entry = OSI_NULL;

	/* Parse the FRP table for give frp_id */
	for (count = 0U; count < osi_core->frp_cnt; count++) {
		entry = &osi_core->frp_table[count];
		if (entry->frp_id == frp_id) {
			/* Entry found break */
			if (found == OSI_NONE) {
				*start = count;
				*no_entries = 1;
				found = OSI_ENABLE;
			} else {
				/* Increment entries */
				*no_entries = *no_entries + 1U;
			}
		}
	}

	if (found == OSI_NONE) {
		/* No entry found return error */
		return -1;
	}

	return 0;
}

/**
 * @brief frp_req_entries - Calculates required FRP entries.
 *
 * Algorithm: Calculates required FRP entries for
 *	the given offset and data length.
 *
 * @param[in] offset: Actual match data offset position.
 * @param[in] match_length: Match data length.
 *
 * @retval No of FRP entries required.
 */
static unsigned char frp_req_entries(unsigned char offset,
				     unsigned char match_length)
{
	unsigned char req = 0U;

	/* Validate for match_length */
	if ((match_length == OSI_NONE) ||
	    (match_length > OSI_FRP_MATCH_DATA_MAX)) {
		/* return zero */
		return req;
	}

	/* Check does the given length can fit in fist entry */
	if (match_length <= FRP_OFFSET_BYTES(offset)) {
		/* Require one entry */
		return 1U;
	}
	/* Initialize req as 1U and decrement length by FRP_OFFSET_BYTES */
	req = 1U;
	match_length -= FRP_OFFSET_BYTES(offset);
	if ((match_length / FRP_MD_SIZE) < OSI_FRP_MATCH_DATA_MAX) {
		req += (match_length / FRP_MD_SIZE);
		if ((match_length % FRP_MD_SIZE) != OSI_NONE) {
			/* Need one more entry */
			req += 1U;
		}
	}

	return req;
}

/**
 * @brief frp_entry_mode_parse - Filter mode parse function.
 *
 * Algorithm: Parse give filter mode and set's FRP entry flags.
 *
 * @param[in] filter_mode: Filter mode from FRP command.
 * @param[in] data: FRP entry data pointer.
 *
 */
static void frp_entry_mode_parse(unsigned char filter_mode,
				 struct osi_core_frp_data *data)
{
	switch (filter_mode) {
	case OSI_FRP_MODE_ROUTE:
		data->accept_frame = OSI_ENABLE;
		data->reject_frame = OSI_DISABLE;
		data->inverse_match = OSI_DISABLE;
		break;
	case OSI_FRP_MODE_DROP:
		data->accept_frame = OSI_DISABLE;
		data->reject_frame = OSI_ENABLE;
		data->inverse_match = OSI_DISABLE;
		break;
	case OSI_FRP_MODE_BYPASS:
		data->accept_frame = OSI_ENABLE;
		data->reject_frame = OSI_ENABLE;
		data->inverse_match = OSI_DISABLE;
		break;
	case OSI_FRP_MODE_LINK:
		data->accept_frame = OSI_DISABLE;
		data->reject_frame = OSI_DISABLE;
		data->inverse_match = OSI_DISABLE;
		break;
	case OSI_FRP_MODE_IM_ROUTE:
		data->accept_frame = OSI_ENABLE;
		data->reject_frame = OSI_DISABLE;
		data->inverse_match = OSI_ENABLE;
		break;
	case OSI_FRP_MODE_IM_DROP:
		data->accept_frame = OSI_DISABLE;
		data->reject_frame = OSI_ENABLE;
		data->inverse_match = OSI_ENABLE;
		break;
	case OSI_FRP_MODE_IM_BYPASS:
		data->accept_frame = OSI_ENABLE;
		data->reject_frame = OSI_ENABLE;
		data->inverse_match = OSI_ENABLE;
		break;
	case OSI_FRP_MODE_IM_LINK:
		data->accept_frame = OSI_DISABLE;
		data->reject_frame = OSI_DISABLE;
		data->inverse_match = OSI_DISABLE;
		break;
	default:
		//OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
		//	"Invalid filter mode argment\n",
		//	filter_mode);
		break;
	}
}

/**
 * @brief frp_entry_add - Add new FRP entries in table.
 *
 * Algorithm: This function will prepare the FRP entries
 *	for given inputs add or update them from a given
 *	position into the FRP table.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] frp_id: FRP ID to add.
 * @param[in] match: Pointer to match data.
 * @param[in] length: Match data length.
 * @param[in] offset: Actual match data offset position.
 * @param[in] filter_mode: Filter mode from FRP command.
 * @param[in] next_frp_id: FRP ID to link this ID.
 * @param[in] dma_sel: Indicate the DMA Channel Number (1-bit for each).
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static int frp_entry_add(struct osi_core_priv_data *const osi_core,
			 int frp_id,
			 unsigned char pos,
			 unsigned char *const match,
			 unsigned char length,
			 unsigned char offset,
			 unsigned char filter_mode,
			 int next_frp_id,
			 unsigned int dma_sel)
{
	struct osi_core_frp_entry *entry = OSI_NULL;
	struct osi_core_frp_data *data = OSI_NULL;
	unsigned int req_entries = 0U;
	unsigned char ok_index = 0U;
	unsigned char fo_t = 0U;
	unsigned char fp_t = 0U;
	unsigned char i = 0U, j = 0U, md_pos = 0U;

	/* Validate length */
	if (length > OSI_FRP_MATCH_DATA_MAX) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"Invalid match length\n",
			length);
		return -1;
	}

	/* Validate filter_mode */
	if (filter_mode >= OSI_FRP_MODE_MAX) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid filter mode argment\n",
			filter_mode);
		return -1;
	}

	/* Validate offset */
	if (offset >= OSI_FRP_OFFSET_MAX) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid offset value\n",
			offset);
		return -1;
	}

	/* Check for avilable space */
	req_entries = frp_req_entries(offset, length);
	if ((req_entries >= OSI_FRP_MAX_ENTRY) ||
	    (req_entries + pos) >= OSI_FRP_MAX_ENTRY) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"No space to update FRP ID\n",
			OSI_NONE);
		return -1;
	}

	/* Validate next_frp_id index ok_index */
	if (filter_mode == OSI_FRP_MODE_LINK ||
	    filter_mode == OSI_FRP_MODE_IM_LINK) {
		if (frp_entry_find(osi_core, next_frp_id, &i, &j) < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				"No Link FRP ID index found\n",
				OSI_NONE);
			i = next_frp_id;
		}
		ok_index = i;
	}

	/* Start data fill from 0U ... (length - 1U) */
	fo_t = (offset / FRP_MD_SIZE);
	fp_t = (offset % FRP_MD_SIZE);
	md_pos = 0U;
	for (i = 0U; i < req_entries; i++) {
		/* Get FRP entry*/
		entry = &osi_core->frp_table[pos];
		data = &entry->data;

		/* Fill FRP ID */
		entry->frp_id = frp_id;

		/* Fill MD and ME */
		data->match_data = OSI_NONE;
		data->match_en = OSI_NONE;
		for (j = fp_t; j < FRP_MD_SIZE; j++) {
			data->match_data |= ((unsigned int)match[md_pos])
					     << (j * FRP_ME_BYTE_SHIFT);
			data->match_en |= ((unsigned int)FRP_ME_BYTE <<
					   (j * FRP_ME_BYTE_SHIFT));
			md_pos++;
			if (md_pos >= length) {
				/* data fill completed */
				break;
			}
		}

		/* Fill FO */
		data->frame_offset = fo_t;

		/* Fill AF, RF, and IM flags */
		frp_entry_mode_parse(filter_mode, data);

		/* Fill DCH */
		data->dma_chsel = dma_sel;

		/* Check for the remain data and update FRP flags */
		if (md_pos < length) {
			/* Reset AF, RF and set NIC, OKI */
			data->accept_frame = OSI_DISABLE;
			data->reject_frame = OSI_DISABLE;
			data->next_ins_ctrl = OSI_ENABLE;

			/* Init next FRP entry */
			pos++;
			fo_t++;
			fp_t = OSI_NONE;
			data->ok_index = pos;
		} else {
			data->next_ins_ctrl = OSI_DISABLE;
			data->ok_index = OSI_DISABLE;
		}
	}

	/* Check and fill final OKI */
	if (filter_mode == OSI_FRP_MODE_LINK ||
	    filter_mode == OSI_FRP_MODE_IM_LINK) {
		/* Update NIC and OKI in final entry */
		data->next_ins_ctrl = OSI_ENABLE;
		data->ok_index = ok_index;
	}

	return 0;
}

/**
 * @brief frp_hw_write - Update HW FRP table.
 *
 * Algorithm: Update FRP table into HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static int frp_hw_write(struct osi_core_priv_data *const osi_core,
			struct core_ops *ops_p)
{
	int ret = -1, tmp = -1;
	struct osi_core_frp_entry *entry;
	unsigned int frp_cnt = osi_core->frp_cnt, i = OSI_NONE;

	/* Disable the FRP in HW */
	ret = ops_p->config_frp(osi_core, OSI_DISABLE);
	if (ret < 0) {
		/* Fail to disable try to enable it back */
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"HW Fail on FRP update\n",
			OSI_NONE);
		goto hw_write_enable_frp;
	}

	/* Write FRP entries into HW  */
	for (i = 0; i < frp_cnt; i++) {
		entry = &osi_core->frp_table[i];
		ret = ops_p->update_frp_entry(osi_core, i, &entry->data);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				"Fail to update FRP entry\n",
				OSI_NONE);
			goto hw_write_enable_frp;
		}
	}

	/* Update the NVE */
	ret = ops_p->update_frp_nve(osi_core, (frp_cnt - 1U));
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to update FRP NVE\n",
			OSI_NONE);
	}

	/* Enable the FRP in HW */
hw_write_enable_frp:
	tmp = ops_p->config_frp(osi_core, OSI_ENABLE);
	return (ret < 0) ? ret : tmp;
}

/**
 * @brief frp_add_proto - Process and update FRP Command Protocal Entry.
 *
 * Algorithm: Parse give FRP command and update Protocal Entry.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cmd: OSI FRP command structure.
 * @param[in] pos: Pointer to the FRP entry position.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static int frp_add_proto(struct osi_core_priv_data *const osi_core,
			 struct osi_core_frp_cmd *const cmd,
			 unsigned char *pos)
{
	int ret = -1, proto_oki = -1;
	unsigned char proto_entry = OSI_DISABLE;
	unsigned char req = 0U;
	unsigned char proto_match[FRP_PROTO_LENGTH];
	unsigned char proto_lendth;
	unsigned char proto_offset;
	unsigned char match_type = cmd->match_type;

	switch (match_type) {
	case OSI_FRP_MATCH_L4_S_UPORT:
		proto_entry = OSI_ENABLE;
		proto_match[0] = FRP_L4_UDP_MD;
		proto_lendth = 1U;
		proto_offset = FRP_L4_IP4_PROTO_OFFSET;
		break;
	case OSI_FRP_MATCH_L4_D_UPORT:
		proto_entry = OSI_ENABLE;
		proto_match[0] = FRP_L4_UDP_MD;
		proto_lendth = 1U;
		proto_offset = FRP_L4_IP4_PROTO_OFFSET;
		break;
	case OSI_FRP_MATCH_L4_S_TPORT:
		proto_entry = OSI_ENABLE;
		proto_match[0] = FRP_L4_TCP_MD;
		proto_lendth = 1U;
		proto_offset = FRP_L4_IP4_PROTO_OFFSET;
		break;
	case OSI_FRP_MATCH_L4_D_TPORT:
		proto_entry = OSI_ENABLE;
		proto_match[0] = FRP_L4_TCP_MD;
		proto_lendth = 1U;
		proto_offset = FRP_L4_IP4_PROTO_OFFSET;
		break;
	case OSI_FRP_MATCH_VLAN:
		proto_entry = OSI_ENABLE;
		proto_match[0] = FRP_L2_VLAN_MD0;
		proto_match[1] = FRP_L2_VLAN_MD1;
		proto_lendth = 2U;
		proto_offset = FRP_L2_VLAN_PROTO_OFFSET;
		break;
	case OSI_FRP_MATCH_NORMAL:
	default:
		proto_entry = OSI_DISABLE;
		break;
	}

	/* Check and Add protocol FRP entire */
	if (proto_entry == OSI_ENABLE) {
		/* Check for space */
		req = frp_req_entries(cmd->offset, cmd->match_length) + 1U;
		if (*pos > (OSI_FRP_MAX_ENTRY - req)) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				"Fail add FRP protocol entry\n",
				OSI_NONE);
			return -1;
		}

		/* Add protocol FRP entire */
		proto_oki = *pos + 1;
		ret = frp_entry_add(osi_core, cmd->frp_id, *pos,
				    proto_match, proto_lendth,
				    proto_offset, OSI_FRP_MODE_LINK,
				    proto_oki, cmd->dma_sel);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				"Fail add FRP protocol entry\n",
				OSI_NONE);
			return ret;
		}

		/* Increment pos value */
		*pos += 1U;
	}

	return 0;
}

/**
 * @brief frp_parse_offset - Process and update FRP Command offset.
 *
 * Algorithm: Parse give FRP command match type and update it's offset.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 */
static void frp_parse_mtype(struct osi_core_priv_data *const osi_core,
			   struct osi_core_frp_cmd *const cmd)
{
	unsigned char offset;
	unsigned char match_type = cmd->match_type;

	switch (match_type) {
	case OSI_FRP_MATCH_L2_DA:
		offset = FRP_L2_DA_OFFSET;
		break;
	case OSI_FRP_MATCH_L2_SA:
		offset = FRP_L2_SA_OFFSET;
		break;
	case OSI_FRP_MATCH_L3_SIP:
		offset = FRP_L3_IP4_SIP_OFFSET;
		break;
	case OSI_FRP_MATCH_L3_DIP:
		offset = FRP_L3_IP4_DIP_OFFSET;
		break;
	case OSI_FRP_MATCH_L4_S_UPORT:
		offset = FRP_L4_IP4_SPORT_OFFSET;
		break;
	case OSI_FRP_MATCH_L4_D_UPORT:
		offset = FRP_L4_IP4_DPORT_OFFSET;
		break;
	case OSI_FRP_MATCH_L4_S_TPORT:
		offset = FRP_L4_IP4_SPORT_OFFSET;
		break;
	case OSI_FRP_MATCH_L4_D_TPORT:
		offset = FRP_L4_IP4_DPORT_OFFSET;
		break;
	case OSI_FRP_MATCH_VLAN:
		offset = FRP_L2_VLAN_TAG_OFFSET;
		break;
	case OSI_FRP_MATCH_NORMAL:
	default:
		offset = cmd->offset;
		break;
	}

	/* Update command offset */
	cmd->offset = offset;
}

/**
 * @brief frp_delete - Process FRP Delete Command.
 *
 * Algorithm: Parse give FRP delete command and update it on OSI data and HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static int frp_delete(struct osi_core_priv_data *const osi_core,
		      struct core_ops *ops_p,
		      struct osi_core_frp_cmd *const cmd)
{
	int ret = -1;
	unsigned char i = 0U, pos = 0U, count = 0U;
	int frp_id = cmd->frp_id;
	unsigned int frp_cnt = osi_core->frp_cnt;

	/* Check for FRP entries  */
	if (frp_cnt == 0U) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"No FRP entries in the table\n",
			OSI_NONE);
		return -1;
	}

	/* Find the FRP entry */
	if (frp_entry_find(osi_core, frp_id, &pos, &count) < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"No FRP entry found to delete\n",
			OSI_NONE);
		return -1;
	}

	/* Validate pos and count */
	if (((unsigned int)pos + count) > frp_cnt) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Invalid FRP entry index\n",
			OSI_NONE);
		return -1;
	}

	/* Update the frp_table entry */
	osi_memset(&osi_core->frp_table[pos], 0U,
		   (sizeof(struct osi_core_frp_entry) * count));

	/* Move in FRP table entries by count */
	for (i = (pos + count); i <= frp_cnt; i++) {
		frp_entry_copy(&osi_core->frp_table[pos],
			       &osi_core->frp_table[i]);
		pos++;
	}

	/* Write FRP Table into HW */
	ret = frp_hw_write(osi_core, ops_p);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to update FRP NVE\n",
			OSI_NONE);
	}

	/* Update the frp_cnt entry */
	osi_core->frp_cnt = (frp_cnt - count);

	return ret;
}

/**
 * @brief frp_update - Process FRP Update Command.
 *
 * Algorithm: Parse give FRP update command and update it on OSI data and HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static int frp_update(struct osi_core_priv_data *const osi_core,
		      struct core_ops *ops_p,
		      struct osi_core_frp_cmd *const cmd)
{
	int ret = -1;
	unsigned char pos = 0U, count = 0U, req = 0U;
	int frp_id = cmd->frp_id;

	/* Validate given frp_id */
	if (frp_entry_find(osi_core, frp_id, &pos, &count) < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"No FRP entry found\n",
			OSI_NONE);
		return -1;
	}

	/* Parse match type and update command offset */
	frp_parse_mtype(osi_core, cmd);

	/* Calculate the required FRP entries for Update Command. */
	req = frp_req_entries(cmd->offset, cmd->match_length);
	switch (cmd->match_type) {
	case OSI_FRP_MATCH_L4_S_UPORT:
	case OSI_FRP_MATCH_L4_D_UPORT:
	case OSI_FRP_MATCH_L4_S_TPORT:
	case OSI_FRP_MATCH_L4_D_TPORT:
	case OSI_FRP_MATCH_VLAN:
		req++;
		break;
	default:
		/* No need of Protocal Entry */
		break;
	}

	/* Reject update on old and new required FRP entries mismatch */
	if (count != req) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Old and New required FRP entries mismatch\n",
			OSI_NONE);
		return -1;
	}

	/* Process and update FRP Command Protocal Entry */
	ret = frp_add_proto(osi_core, cmd, &pos);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to parse match type\n",
			OSI_NONE);
		return ret;
	}

	/* Update FRP entries */
	ret = frp_entry_add(osi_core, frp_id, pos,
			    cmd->match, cmd->match_length,
			    cmd->offset, cmd->filter_mode,
			    cmd->next_frp_id, cmd->dma_sel);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to update FRP entry\n",
			OSI_NONE);
		return ret;
	}

	/* Write FRP Table into HW */
	ret = frp_hw_write(osi_core, ops_p);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to update FRP NVE\n",
			OSI_NONE);
	}

	return ret;
}

/**
 * @brief frp_add - Process FRP Add Command.
 *
 * Algorithm: Parse give FRP Add command and update it on OSI data and HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static int frp_add(struct osi_core_priv_data *const osi_core,
		   struct core_ops *ops_p,
		   struct osi_core_frp_cmd *const cmd)
{
	int ret = -1;
	unsigned char pos = 0U, count = 0U;
	int frp_id = cmd->frp_id;
	unsigned int nve = osi_core->frp_cnt;

	/* Check for MAX FRP entries */
	if (nve >= OSI_FRP_MAX_ENTRY) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"FRP etries are full\n",
			nve);
		return -1;
	}

	/* Check the FRP entry already exists */
	ret = frp_entry_find(osi_core, frp_id, &pos, &count);
	if (ret >= 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"FRP entry already exists\n",
			OSI_NONE);
		return -1;
	}

	/* Parse match type and update command offset */
	frp_parse_mtype(osi_core, cmd);

	/* Process and add FRP Command Protocal Entry */
	ret = frp_add_proto(osi_core, cmd, (unsigned char *)&nve);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to parse match type\n",
			OSI_NONE);
		return ret;
	}

	/* Add Match data FRP Entry */
	ret = frp_entry_add(osi_core, frp_id, (unsigned char)nve,
			    cmd->match, cmd->match_length,
			    cmd->offset, cmd->filter_mode,
			    cmd->next_frp_id, cmd->dma_sel);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to add FRP entry\n",
			nve);
		return ret;
	}
	osi_core->frp_cnt = nve + frp_req_entries(cmd->offset,
						  cmd->match_length);

	/* Write FRP Table into HW */
	ret = frp_hw_write(osi_core, ops_p);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to update FRP NVE\n",
			OSI_NONE);
	}

	return ret;
}

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
	      struct osi_core_frp_cmd *const cmd)
{
	int ret = -1;

	switch (cmd->cmd) {
	case OSI_FRP_CMD_ADD:
		ret = frp_add(osi_core, ops_p, cmd);
		break;
	case OSI_FRP_CMD_UPDATE:
		ret = frp_update(osi_core, ops_p, cmd);
		break;
	case OSI_FRP_CMD_DEL:
		ret = frp_delete(osi_core, ops_p, cmd);
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Invalid FRP command\n",
			     cmd->cmd);
		break;
	}

	OSI_CORE_INFO(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
		      "FRP instrctions count\n",
		      osi_core->frp_cnt);

	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "FRP command fail\n",
			      cmd->cmd);
	}

	return ret;
}

/**
 * @brief init_frp - Initialize FRP.
 *
 * Algorithm: Reset all the data in the FRP table Initialize FRP count to zero.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 */
void init_frp(struct osi_core_priv_data *const osi_core)
{
	/* Reset the NVE count to zero */
	osi_core->frp_cnt = 0U;
	/* Clear all instruction of FRP */
	osi_memset(osi_core->frp_table, 0U,
		   (sizeof(struct osi_core_frp_entry) * OSI_FRP_MAX_ENTRY));
}
