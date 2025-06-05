// SPDX-License-Identifier: MIT

/* SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "common.h"
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
static void
frp_entry_copy (
  struct osi_core_frp_entry         *dst,
  struct osi_core_frp_entry *const  src
  )
{
  dst->frp_id             = src->frp_id;
  dst->data.match_data    = src->data.match_data;
  dst->data.match_en      = src->data.match_en;
  dst->data.accept_frame  = src->data.accept_frame;
  dst->data.reject_frame  = src->data.reject_frame;
  dst->data.inverse_match = src->data.inverse_match;
  dst->data.next_ins_ctrl = src->data.next_ins_ctrl;
  dst->data.frame_offset  = src->data.frame_offset;
  dst->data.ok_index      = src->data.ok_index;
  dst->data.dma_chsel     = src->data.dma_chsel;
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
static nve32_t
frp_entry_find (
  struct osi_core_priv_data *const  osi_core,
  nve32_t                           frp_id,
  nveu8_t                           *start,
  nveu8_t                           *no_entries
  )
{
  nveu8_t                    count = OSI_NONE, found = OSI_NONE;
  struct osi_core_frp_entry  *entry = OSI_NULL;
  nve32_t                    ret    = 0;

  /* Parse the FRP table for give frp_id */
  for (count = 0U; count < osi_core->frp_cnt; count++) {
    entry = &osi_core->frp_table[count];
    if (entry->frp_id == frp_id) {
      /* Entry found break */
      if (found == OSI_NONE) {
        *start      = count;
        *no_entries = 1;
        found       = OSI_ENABLE;
      } else {
        /* Increment entries */
        *no_entries =  (nveu8_t)(*no_entries + 1U);
      }
    }
  }

  if (found == OSI_NONE) {
    /* No entry found return error */
    ret = -1;
  }

  return ret;
}

/**
 * @brief frp_req_entries - Calculates required FRP entries.
 *
 * Algorithm: Calculates required FRP entries for
 *      the given offset and data length.
 *
 * @param[in] offset: Actual match data offset position.
 * @param[in] match_length: Match data length.
 *
 * @retval No of FRP entries required.
 */
static nveu8_t
frp_req_entries (
  nveu8_t  offset,
  nveu8_t  match_length
  )
{
  nveu8_t  req                 = 0U;
  nveu8_t  temp_match_length   = match_length;
  nveu8_t  frp_offset_bytes_u8 = (nveu8_t)FRP_OFFSET_BYTES (offset);

  /* Check does the given length can fit in fist entry */
  if (temp_match_length <= frp_offset_bytes_u8) {
    /* Require one entry */
    req = 1U;
    goto done;
  }

  /* Initialize req as 1U and decrement length by FRP_OFFSET_BYTES */
  req               = 1U;
  temp_match_length = (nveu8_t)(temp_match_length - frp_offset_bytes_u8);
  req               = (nveu8_t)(req + (temp_match_length /  FRP_MD_SIZE));
  if ((temp_match_length % FRP_MD_SIZE) != OSI_NONE) {
    /* Need one more entry */
    req = (nveu8_t)(req + 1U);
  }

done:
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
static void
frp_entry_mode_parse (
  nveu8_t                   filter_mode,
  struct osi_core_frp_data  *data
  )
{
  switch (filter_mode) {
    case OSI_FRP_MODE_ROUTE:
      data->accept_frame  = OSI_ENABLE;
      data->reject_frame  = OSI_DISABLE;
      data->inverse_match = OSI_DISABLE;
      break;
    case OSI_FRP_MODE_DROP:
      data->accept_frame  = OSI_DISABLE;
      data->reject_frame  = OSI_ENABLE;
      data->inverse_match = OSI_DISABLE;
      break;
    case OSI_FRP_MODE_BYPASS:
      data->accept_frame  = OSI_ENABLE;
      data->reject_frame  = OSI_ENABLE;
      data->inverse_match = OSI_DISABLE;
      break;
    case OSI_FRP_MODE_LINK:
      data->accept_frame  = OSI_DISABLE;
      data->reject_frame  = OSI_DISABLE;
      data->inverse_match = OSI_DISABLE;
      break;
    case OSI_FRP_MODE_IM_ROUTE:
      data->accept_frame  = OSI_ENABLE;
      data->reject_frame  = OSI_DISABLE;
      data->inverse_match = OSI_ENABLE;
      break;
    case OSI_FRP_MODE_IM_DROP:
      data->accept_frame  = OSI_DISABLE;
      data->reject_frame  = OSI_ENABLE;
      data->inverse_match = OSI_ENABLE;
      break;
    case OSI_FRP_MODE_IM_BYPASS:
      data->accept_frame  = OSI_ENABLE;
      data->reject_frame  = OSI_ENABLE;
      data->inverse_match = OSI_ENABLE;
      break;
    case OSI_FRP_MODE_IM_LINK:
      data->accept_frame  = OSI_DISABLE;
      data->reject_frame  = OSI_DISABLE;
      data->inverse_match = OSI_DISABLE;
      break;
    default:
      // OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
      //        "Invalid filter mode argment\n",
      //        filter_mode);
      break;
  }
}

static nve32_t
validate_frp_args (
  struct osi_core_priv_data *const  osi_core,
  nveu8_t                           length,
  nveu8_t                           offset,
  nveu8_t                           filter_mode,
  nveu32_t                          dma_sel,
  OSI_UNUSED nveu8_t                pos,
  nveu32_t                          *req_entries
  )
{
  nveu64_t  dma_sel_val[OSI_MAX_MAC_IP_TYPES] = { 0xFFU, 0x3FFU, 0xFFFFFFFFFFFFU };
  nve32_t   ret                               = 0;

  (void)pos;

  /* Validate filter_mode */
  if (filter_mode >= OSI_FRP_MODE_MAX) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid filter mode argment\n",
      filter_mode
      );
    ret = -1;
    goto done;
  }

  /* Validate offset */
  if (offset >= OSI_FRP_OFFSET_MAX) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid offset value\n",
      offset
      );
    ret = -1;
    goto done;
  }

  /* Validate channel selection */
  if (dma_sel > dma_sel_val[osi_core->mac]) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid DMA selection\n",
      (nveu64_t)dma_sel
      );
    ret = -1;
    goto done;
  }

  /* Check for avilable space */
  *req_entries = frp_req_entries (offset, length);

done:
  return ret;
}

/**
 * @brief frp_entry_add - Add new FRP entries in table.
 *
 * Algorithm: This function will prepare the FRP entries
 *      for given inputs add or update them from a given
 *      position into the FRP table.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] frp_id: FRP ID to add.
 * @param[in] pos: FRP entry position.
 * @param[in] match: Pointer to match data.
 * @param[in] length: Match data length.
 * @param[in] offset: Actual match data offset position.
 * @param[in] filter_mode: Filter mode from FRP command.
 * @param[in] next_frp_id: FRP ID to link this ID.
 * @param[in] dma_sel: DMA channel Selection Value.
 * @param[in] dcht: DMA Channel Selection Type.
 * @param[in] rchlist_indx: Receive Channel list index.
 *
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static nve32_t
frp_entry_add (
  struct osi_core_priv_data *const  osi_core,
  nve32_t                           frp_id,
  nveu8_t                           pos,
  nveu8_t *const                    match,
  nveu8_t                           length,
  nveu8_t                           offset,
  nveu8_t                           filter_mode,
  nve32_t                           next_frp_id,
  nveu32_t                          dma_sel,
  nveu8_t                           dcht,
  nve32_t                           rchlist_indx
  )
{
  struct osi_core_frp_entry  *entry = OSI_NULL;
  struct osi_core_frp_data   *data = OSI_NULL;
  nveu32_t                   req_entries = 0U;
  nveu8_t                    ok_index = 0U;
  nveu8_t                    fo_t = 0U;
  nveu8_t                    fp_t = 0U;
  nveu8_t                    i = 0U, j = 0U, md_pos = 0U;
  nveu16_t                   temp_pos_16bit = 0U;
  nveu8_t                    temp_pos       = pos;
  nve32_t                    ret;

  /* Validate FRP arguments */
  ret = validate_frp_args (
          osi_core,
          length,
          offset,
          filter_mode,
          dma_sel,
          pos,
          &req_entries
          );
  if (ret < 0) {
    goto done;
  }

  /* Validate next_frp_id index ok_index */
  switch (filter_mode) {
    case OSI_FRP_MODE_LINK:
    case OSI_FRP_MODE_IM_LINK:
      if (frp_entry_find (osi_core, next_frp_id, &i, &j) < 0) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "No Link FRP ID index found\n",
          OSI_NONE
          );
        i = (nveu8_t)next_frp_id;
      }

      ok_index = i;
      break;
    default:
      /* Do Nothing */
      break;
  }

  /* Start data fill from 0U ... (length - 1U) */
  fo_t   = (offset / FRP_MD_SIZE);
  fp_t   = (offset % FRP_MD_SIZE);
  md_pos = 0U;
  for (i = 0U; i < req_entries; i++) {
    /* Get FRP entry*/
    entry = &osi_core->frp_table[temp_pos];
    data  = &entry->data;

    /* Fill FRP ID */
    entry->frp_id = frp_id;

    /* Fill MD and ME */
    data->match_data = OSI_NONE;
    data->match_en   = OSI_NONE;
    for (j = fp_t; j < FRP_MD_SIZE; j++) {
      data->match_data |= ((nveu32_t)match[md_pos])
                          << (j * FRP_ME_BYTE_SHIFT);
      data->match_en |= ((nveu32_t)FRP_ME_BYTE <<
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
    frp_entry_mode_parse (filter_mode, data);

    /* Fill DCH */
    data->dma_chsel = dma_sel;

    /* Fill dcht & rchlist_indx */
    data->rchlist_indx = rchlist_indx;
    data->dcht         = dcht;
    /* Check for the remain data and update FRP flags */
    if (md_pos < length) {
      /* Reset AF, RF and set NIC, OKI */
      data->accept_frame  = OSI_DISABLE;
      data->reject_frame  = OSI_DISABLE;
      data->next_ins_ctrl = OSI_ENABLE;

      /* Init next FRP entry */
      temp_pos_16bit = (nveu16_t)((nveu16_t)temp_pos + 1U);
      temp_pos       = (nveu8_t)(temp_pos_16bit & 0xFFU);
      fo_t++;
      fp_t           = OSI_NONE;
      data->ok_index = temp_pos;
    } else {
      data->next_ins_ctrl = OSI_DISABLE;
      data->ok_index      = OSI_DISABLE;
    }
  }

  /* Check and fill final OKI */
  switch (filter_mode) {
    case OSI_FRP_MODE_LINK:
    case OSI_FRP_MODE_IM_LINK:
      /* Update NIC and OKI in final entry */
      data->next_ins_ctrl = OSI_ENABLE;
      data->ok_index      = ok_index;
      break;
    default:
      /* Do Nothing */
      break;
  }

  ret = 0;
done:
  return ret;
}

/**
 * @brief frp_hw_write - Update HW FRP table.
 *
 * Algorithm: Update FRP table into HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] ops_p: Core operations data structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
nve32_t
frp_hw_write (
  struct osi_core_priv_data *const  osi_core,
  struct core_ops *const            ops_p
  )
{
  nve32_t                    ret = 0;
  nve32_t                    tmp = 0;
  struct osi_core_frp_entry  *entry;
  struct osi_core_frp_data   bypass_entry = { };
  nveu32_t                   frp_cnt = osi_core->frp_cnt, i = OSI_NONE;

  /* Disable the FRP in HW */
  ret = ops_p->config_frp (osi_core, OSI_DISABLE);
  if (ret < 0) {
    /* Fail to disable try to enable it back */
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "HW Fail on FRP update\n",
      OSI_NONE
      );
    tmp = ops_p->config_frp (osi_core, OSI_ENABLE);
    goto frp_hw_write_error;
  }

  /* Check HW table size for non-zero */
  if (frp_cnt != 0U) {
    /* Write FRP entries into HW  */
    for (i = 0; i < frp_cnt; i++) {
      entry = &osi_core->frp_table[i];
      ret   = ops_p->update_frp_entry (
                       osi_core,
                       i,
                       &entry->data
                       );
      if (ret < 0) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "Fail to update FRP entry\n",
          OSI_NONE
          );
        goto hw_write_enable_frp;
      }
    }

    /* Write BYPASS rule for XDCS */
    bypass_entry.match_en     = 0x0U;
    bypass_entry.accept_frame = 1;
    bypass_entry.reject_frame = 1;
    ret                       = ops_p->update_frp_entry (osi_core, frp_cnt, &bypass_entry);
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Fail to update BYPASS entry\n",
        OSI_NONE
        );
      goto hw_write_enable_frp;
    }

    /* Update the NVE */
    ops_p->update_frp_nve (osi_core, frp_cnt);

    /* Enable the FRP in HW */
hw_write_enable_frp:
    tmp = ops_p->config_frp (osi_core, OSI_ENABLE);
  }

frp_hw_write_error:

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
 * @param[in] rchlist_indx: Index to the rchlist.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static nve32_t
frp_add_proto (
  struct osi_core_priv_data *const  osi_core,
  struct osi_core_frp_cmd *const    cmd,
  nveu8_t                           *pos,
  nve32_t                           rchlist_indx
  )
{
  nve32_t  ret, proto_oki;
  nveu8_t  proto_entry = OSI_DISABLE;
  nveu8_t  req         = 0U;
  nveu8_t  proto_match[FRP_PROTO_LENGTH];
  nveu8_t  proto_length;
  nveu8_t  proto_offset;
  nveu8_t  match_type = cmd->match_type;

  switch (match_type) {
    case OSI_FRP_MATCH_L4_S_UPORT:
      proto_entry    = OSI_ENABLE;
      proto_match[0] = FRP_L4_UDP_MD;
      proto_length   = 1U;
      proto_offset   = FRP_L4_IP4_PROTO_OFFSET;
      break;
    case OSI_FRP_MATCH_L4_D_UPORT:
      proto_entry    = OSI_ENABLE;
      proto_match[0] = FRP_L4_UDP_MD;
      proto_length   = 1U;
      proto_offset   = FRP_L4_IP4_PROTO_OFFSET;
      break;
    case OSI_FRP_MATCH_L4_S_TPORT:
      proto_entry    = OSI_ENABLE;
      proto_match[0] = FRP_L4_TCP_MD;
      proto_length   = 1U;
      proto_offset   = FRP_L4_IP4_PROTO_OFFSET;
      break;
    case OSI_FRP_MATCH_L4_D_TPORT:
      proto_entry    = OSI_ENABLE;
      proto_match[0] = FRP_L4_TCP_MD;
      proto_length   = 1U;
      proto_offset   = FRP_L4_IP4_PROTO_OFFSET;
      break;
    case OSI_FRP_MATCH_VLAN:
      proto_entry    = OSI_ENABLE;
      proto_match[0] = FRP_L2_VLAN_MD0;
      proto_match[1] = FRP_L2_VLAN_MD1;
      proto_length   = 2U;
      proto_offset   = FRP_L2_VLAN_PROTO_OFFSET;
      break;
    case OSI_FRP_MATCH_NORMAL:
    case OSI_FRP_MATCH_L2_DA:
    case OSI_FRP_MATCH_L2_SA:
    case OSI_FRP_MATCH_L3_SIP:
    case OSI_FRP_MATCH_L3_DIP:
      proto_entry = OSI_DISABLE;
      ret         = 0;
      break;
    default:
      proto_entry = OSI_DISABLE;
      ret         = -1;
      break;
  }

  /* Check and Add protocol FRP entire */
  if (proto_entry == OSI_ENABLE) {
    /* Check for space */
    req = (nveu8_t)((frp_req_entries (cmd->offset, cmd->match_length) + 1U) & (0xFFU));
    if (*pos > (OSI_FRP_MAX_ENTRY - req)) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Fail add FRP protocol entry\n",
        OSI_NONE
        );
      ret = -1;
      goto done;
    }

    /* Add protocol FRP entire */
    proto_oki  = (nve32_t)*pos;
    proto_oki += 1;
    ret        = frp_entry_add (
                   osi_core,
                   cmd->frp_id,
                   *pos,
                   proto_match,
                   proto_length,
                   proto_offset,
                   OSI_FRP_MODE_LINK,
                   proto_oki,
                   cmd->dma_sel,
                   cmd->dcht,
                   rchlist_indx
                   );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Fail add FRP protocol entry\n",
        OSI_NONE
        );
      goto done;
    }

    /* Increment pos value */
    *pos = (nveu8_t)(*pos + (nveu8_t)1);
  }

done:
  return ret;
}

/**
 * @brief frp_parse_offset - Process and update FRP Command offset.
 *
 * Algorithm: Parse give FRP command match type and update it's offset.
 *
 * @param[in] cmd: OSI FRP command structure.
 *
 */
static void
frp_parse_mtype (
  struct osi_core_frp_cmd *const  cmd
  )
{
  nveu8_t  offset;
  nveu8_t  match_type = cmd->match_type;

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
 * @param[in] ops_p: Core operations data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static nve32_t
frp_delete (
  struct osi_core_priv_data *const  osi_core,
  struct core_ops                   *ops_p,
  struct osi_core_frp_cmd *const    cmd
  )
{
  nve32_t                    ret;
  nveu8_t                    i = 0U, pos = 0U, count = 0U;
  nve32_t                    frp_id  = cmd->frp_id;
  nveu32_t                   frp_cnt = osi_core->frp_cnt;
  struct osi_core_frp_entry  *entry  = OSI_NULL;

  /* Check for FRP entries  */
  if (frp_cnt == 0U) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "No FRP entries in the table\n",
      OSI_NONE
      );
    ret = -1;
    goto done;
  }

  /* Find the FRP entry */
  if (frp_entry_find (osi_core, frp_id, &pos, &count) < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "No FRP entry found to delete\n",
      OSI_NONE
      );
    ret = -1;
    goto done;
  }

  /* Free the RCHLIST index */
  entry = &osi_core->frp_table[frp_id];
  ops_p->free_rchlist_index (
           osi_core,
           entry->data.rchlist_indx
           );

  /* Update the frp_table entry */
  osi_memset (
    &osi_core->frp_table[pos],
    0U,
    (sizeof (struct osi_core_frp_entry) * count)
    );

  /* Move in FRP table entries by count */
  for (i = (nveu8_t)((pos + count) & (0xFFU)); i <= frp_cnt; i++) {
    frp_entry_copy (
      &osi_core->frp_table[pos],
      &osi_core->frp_table[i]
      );
    pos++;
  }

  /* Update the frp_cnt entry */
  osi_core->frp_cnt = (frp_cnt - count);

  /* Write FRP Table into HW */
  ret = frp_hw_write (osi_core, ops_p);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to update FRP NVE\n",
      OSI_NONE
      );
  }

done:
  return ret;
}

/**
 * @brief frp_update - Process FRP Update Command.
 *
 * Algorithm: Parse give FRP update command and update it on OSI data and HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] ops_p: Core operations data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static nve32_t
frp_update (
  struct osi_core_priv_data *const  osi_core,
  struct core_ops                   *ops_p,
  struct osi_core_frp_cmd *const    cmd
  )
{
  nve32_t                    ret;
  nveu8_t                    pos = 0U, count = 0U, req = 0U;
  nveu16_t                   req_16bit    = 0U;
  nve32_t                    frp_id       = cmd->frp_id;
  struct osi_core_frp_entry  *entry       = OSI_NULL;
  nve32_t                    rchlist_indx = 0;

  /* Validate given frp_id */
  if (frp_entry_find (osi_core, frp_id, &pos, &count) < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "No FRP entry found\n",
      OSI_NONE
      );
    ret = -1;
    goto done;
  }

  entry        = &osi_core->frp_table[frp_id];
  rchlist_indx = entry->data.rchlist_indx;

  /* Parse match type and update command offset */
  frp_parse_mtype (cmd);

  /* Calculate the required FRP entries for Update Command. */
  req = frp_req_entries (cmd->offset, cmd->match_length);
  switch (cmd->match_type) {
    case OSI_FRP_MATCH_L4_S_UPORT:
    case OSI_FRP_MATCH_L4_D_UPORT:
    case OSI_FRP_MATCH_L4_S_TPORT:
    case OSI_FRP_MATCH_L4_D_TPORT:
    case OSI_FRP_MATCH_VLAN:
      req_16bit = (nveu16_t)((nveu16_t)req + 1U);
      req       = (nveu8_t)(req_16bit & 0xFFU);
      break;
    default:
      /* No need of Protocal Entry */
      break;
  }

  /* Reject update on old and new required FRP entries mismatch */
  if (count != req) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Old and New required FRP entries mismatch\n",
      OSI_NONE
      );
    ret = -1;
    goto done;
  }

  /* Process and update FRP Command Protocol Entry */
  ret = frp_add_proto (osi_core, cmd, &pos, rchlist_indx);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to parse match type\n",
      OSI_NONE
      );
    goto done;
  }

  /* Update FRP entries */
  ret = frp_entry_add (
          osi_core,
          frp_id,
          pos,
          cmd->match,
          cmd->match_length,
          cmd->offset,
          cmd->filter_mode,
          cmd->next_frp_id,
          cmd->dma_sel,
          cmd->dcht,
          rchlist_indx
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to update FRP entry\n",
      OSI_NONE
      );
    goto done;
  }

  /* Write FRP Table into HW */
  ret = frp_hw_write (osi_core, ops_p);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to update FRP NVE\n",
      OSI_NONE
      );
  }

done:
  return ret;
}

/**
 * @brief frp_add - Process FRP Add Command.
 *
 * Algorithm: Parse give FRP Add command and update it on OSI data and HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] ops_p: Core operations data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
static nve32_t
frp_add (
  struct osi_core_priv_data *const  osi_core,
  struct core_ops                   *ops_p,
  struct osi_core_frp_cmd *const    cmd
  )
{
  nve32_t  ret;
  nveu8_t  pos = 0U, count = 0U;
  nve32_t  frp_id       = cmd->frp_id;
  nve32_t  rchlist_indx = 0;

  /* Check for MAX FRP entries */
  if (osi_core->frp_cnt >= OSI_FRP_MAX_ENTRY) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OUTOFBOUND,
      "FRP etries are full\n",
      osi_core->frp_cnt
      );
    ret = -1;
    goto done;
  }

  /* Check the FRP entry already exists */
  ret = frp_entry_find (osi_core, frp_id, &pos, &count);
  if (ret >= 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "FRP entry already exists\n",
      OSI_NONE
      );
    ret = -1;
    goto done;
  }

  /* Parse match type and update command offset */
  frp_parse_mtype (cmd);

  if (cmd->dcht == OSI_ENABLE) {
    /* Find rchlist Free index */
    rchlist_indx = ops_p->get_rchlist_index (osi_core, OSI_NULL);
    if (rchlist_indx < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Fail to get rchlist index\n",
        OSI_NONE
        );
      goto done;
    }

    osi_core->rch_index[rchlist_indx /*ret*/].in_use = OSI_ENABLE;
  }

  /* Process and add FRP Command Protocal Entry */
  ret = frp_add_proto (osi_core, cmd, (nveu8_t *)&osi_core->frp_cnt, rchlist_indx);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to parse match type\n",
      OSI_NONE
      );
    goto done;
  }

  /* Add Match data FRP Entry */
  ret = frp_entry_add (
          osi_core,
          frp_id,
          (nveu8_t)(osi_core->frp_cnt & 0xFFU),
          cmd->match,
          cmd->match_length,
          cmd->offset,
          cmd->filter_mode,
          cmd->next_frp_id,
          cmd->dma_sel,
          cmd->dcht,
          rchlist_indx
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to add FRP entry\n",
      osi_core->frp_cnt
      );
    goto done;
  }

  osi_core->frp_cnt = (nveu32_t)(osi_core->frp_cnt & 0xFFU) +
                      ((nveu32_t)frp_req_entries (cmd->offset, cmd->match_length) & 0xFFU);

  /* Write FRP Table into HW */
  ret = frp_hw_write (osi_core, ops_p);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to update FRP NVE\n",
      OSI_NONE
      );
  }

done:
  return ret;
}

/**
 * @brief setup_frp - Process OSD FRP Command.
 *
 * Algorithm: Parse give FRP command and update it on OSI data and HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] ops_p: Core operations data structure.
 * @param[in] cmd: OSI FRP command structure.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
nve32_t
setup_frp (
  struct osi_core_priv_data *const  osi_core,
  struct core_ops                   *ops_p,
  struct osi_core_frp_cmd *const    cmd
  )
{
  nve32_t  ret = -1;

  if ((cmd->frp_id > OSI_FRP_ID_MAX) ||
      (cmd->next_frp_id > OSI_FRP_ID_MAX))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Invalid FRP ID\n",
      cmd->frp_id
      );
    goto error;
  }

  if ((cmd->match_length < OSI_FRP_MATCH_DATA_MIN) ||
      (cmd->match_length > OSI_FRP_MATCH_DATA_MAX))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Invalid FRP match_lenght \n",
      cmd->frp_id
      );
    goto error;
  }

  switch (cmd->cmd) {
    case OSI_FRP_CMD_ADD:
      ret = frp_add (osi_core, ops_p, cmd);
      break;
    case OSI_FRP_CMD_UPDATE:
      ret = frp_update (osi_core, ops_p, cmd);
      break;
    case OSI_FRP_CMD_DEL:
      ret = frp_delete (osi_core, ops_p, cmd);
      break;
    default:
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Invalid FRP command\n",
        cmd->cmd
        );
      break;
  }

  OSI_CORE_INFO (
    (osi_core->osd),
    (OSI_LOG_ARG_HW_FAIL),
    ("FRP instrctions count\n"),
    (osi_core->frp_cnt)
    );

  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "FRP command fail\n",
      cmd->cmd
      );
  }

error:
  return ret;
}
