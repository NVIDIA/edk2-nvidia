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
#include <osi_common.h>
#include <osi_core.h>
#include "mgbe_core.h"
#include "core_local.h"
#include "xpcs.h"
#include "mgbe_mmc.h"
#include "core_common.h"

/**
 * @brief mgbe_poll_for_mac_accrtl - Poll for Indirect Access control and status
 * register operations complete.
 *
 * @note
 * Algorithm: Waits for waits for transfer busy bit to be cleared in
 * MAC Indirect address control register to complete operations.
 *
 * @param[in] osi_core: osi core priv data structure
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_poll_for_mac_acrtl (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t  count               = 0U;
  nveu32_t  mac_indir_addr_ctrl = 0U;
  nve32_t   ret                 = -1;

  /* Poll Until MAC_Indir_Access_Ctrl OB is clear */
  while (count < MGBE_MAC_INDIR_AC_OB_RETRY) {
    mac_indir_addr_ctrl = osi_readla (
                            osi_core,
                            (nveu8_t *)osi_core->base +
                            MGBE_MAC_INDIR_AC
                            );
    if ((mac_indir_addr_ctrl & MGBE_MAC_INDIR_AC_OB) == OSI_NONE) {
      /* OB is clear exit the loop */
      ret = 0;
      break;
    }

    /* wait for 10 usec for OB clear and retry */
    osi_core->osd_ops.usleep (MGBE_MAC_INDIR_AC_OB_WAIT);
    count++;
  }

  return ret;
}

/**
 * @brief mgbe_mac_indir_addr_write - MAC Indirect AC register write.
 *
 * @note
 * Algorithm: writes MAC Indirect AC register
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] mc_no: MAC AC Mode Select number
 * @param[in] addr_offset: MAC AC Address Offset.
 * @param[in] value: MAC AC register value
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_mac_indir_addr_write (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   mc_no,
  nveu32_t                   addr_offset,
  nveu32_t                   value
  )
{
  const nveu32_t  ac_msel_mask[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_MAC_INDIR_AC_MSEL,
    MGBE_MAC_INDIR_AC_MSEL_T26X
  };
  const nveu32_t  ac_msel_shift[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_MAC_INDIR_AC_MSEL_SHIFT,
    MGBE_MAC_INDIR_AC_MSEL_SHIFT_T264
  };
  void            *base = osi_core->base;
  nveu32_t        mac   = osi_core->mac;
  nveu32_t        addr  = 0;
  nve32_t         ret   = 0;

  /* Write MAC_Indir_Access_Data register value */
  osi_writela (osi_core, value, (nveu8_t *)base + MGBE_MAC_INDIR_DATA);

  /* Program MAC_Indir_Access_Ctrl */
  addr = osi_readla (osi_core, (nveu8_t *)base + MGBE_MAC_INDIR_AC);

  /* update Mode Select */
  addr &= ~ac_msel_mask[mac];
  addr |= ((mc_no << ac_msel_shift[mac]) & ac_msel_mask[mac]);

  /* update Address Offset */
  addr &= ~(MGBE_MAC_INDIR_AC_AOFF);
  addr |= ((addr_offset << MGBE_MAC_INDIR_AC_AOFF_SHIFT) &
           MGBE_MAC_INDIR_AC_AOFF);

  /* Set CMD filed bit 0 for write */
  addr &= ~(MGBE_MAC_INDIR_AC_CMD);

  /* Set OB bit to initiate write */
  addr |= MGBE_MAC_INDIR_AC_OB;

  /* Write MGBE_MAC_L3L4_ADDR_CTR */
  osi_writela (osi_core, addr, (nveu8_t *)base + MGBE_MAC_INDIR_AC);

  /* Wait until OB bit reset */
  if (mgbe_poll_for_mac_acrtl (osi_core) < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to write MAC_Indir_Access_Ctrl\n",
      mc_no
      );
    ret = -1;
  }

  return ret;
}

/**
 * @brief mgbe_mac_indir_addr_read - MAC Indirect AC register read.
 *
 * @note
 * Algorithm: Reads MAC Indirect AC register
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] mc_no: MAC AC Mode Select number
 * @param[in] addr_offset: MAC AC Address Offset.
 * @param[in] value: Pointer MAC AC register value
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_mac_indir_addr_read (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   mc_no,
  nveu32_t                   addr_offset,
  nveu32_t                   *value
  )
{
  const nveu32_t  ac_msel_mask[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_MAC_INDIR_AC_MSEL,
    MGBE_MAC_INDIR_AC_MSEL_T26X
  };
  const nveu32_t  ac_msel_shift[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_MAC_INDIR_AC_MSEL_SHIFT,
    MGBE_MAC_INDIR_AC_MSEL_SHIFT_T264
  };
  void            *base = osi_core->base;
  nveu32_t        mac   = osi_core->mac;
  nveu32_t        addr  = 0;
  nve32_t         ret   = 0;

  /* Program MAC_Indir_Access_Ctrl */
  addr = osi_readla (osi_core, (nveu8_t *)base + MGBE_MAC_INDIR_AC);

  /* update Mode Select */
  addr &= ~ac_msel_mask[mac];
  addr |= ((mc_no << ac_msel_shift[mac]) & ac_msel_mask[mac]);

  /* update Address Offset */
  addr &= ~(MGBE_MAC_INDIR_AC_AOFF);
  addr |= ((addr_offset << MGBE_MAC_INDIR_AC_AOFF_SHIFT) &
           MGBE_MAC_INDIR_AC_AOFF);

  /* Set CMD filed bit to 1 for read */
  addr |= MGBE_MAC_INDIR_AC_CMD;

  /* Set OB bit to initiate write */
  addr |= MGBE_MAC_INDIR_AC_OB;

  /* Write MGBE_MAC_L3L4_ADDR_CTR */
  osi_writela (osi_core, addr, (nveu8_t *)base + MGBE_MAC_INDIR_AC);

  /* Wait until OB bit reset */
  if (mgbe_poll_for_mac_acrtl (osi_core) < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to write MAC_Indir_Access_Ctrl\n",
      mc_no
      );
    ret = -1;
    goto fail;
  }

  /* Read MAC_Indir_Access_Data register value */
  *value = osi_readla (osi_core, (nveu8_t *)base + MGBE_MAC_INDIR_DATA);
fail:
  return ret;
}

/**
 * @brief mgbe_filter_args_validate - Validates the filter arguments
 *
 * @note
 * Algorithm: This function just validates all arguments provided by
 * the osi_filter structure variable.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter: OSI filter structure.
 *
 * @note 1) MAC should be initialized and stated. see osi_start_mac()
 *       2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_filter_args_validate (
  struct osi_core_priv_data *const  osi_core,
  const struct osi_filter           *filter
  )
{
  struct core_local  *l_core                       = (struct core_local *)(void *)osi_core;
  const nveu32_t     idx_max[OSI_MAX_MAC_IP_TYPES] = {
    0,
    OSI_MGBE_MAX_MAC_ADDRESS_FILTER,
    OSI_MGBE_MAX_MAC_ADDRESS_FILTER_T26X
  };
  const nveu64_t     chansel_max[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_MAC_XDCS_DMA_MAX,
    MGBE_MAC_XDCS_DMA_MAX_T26X
  };
  nveu32_t           mac                = osi_core->mac;
  nveu32_t           idx                = filter->index;
  nveu32_t           dma_routing_enable = filter->dma_routing;
  nveu32_t           dma_chan           = filter->dma_chan;
  nveu32_t           addr_mask          = filter->addr_mask;
  nveu32_t           src_dest           = filter->src_dest;
  nveu64_t           dma_chansel        = filter->dma_chansel;
  nve32_t            ret                = 0;

  /* check for valid index */
  if (idx >= idx_max[mac]) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "invalid MAC filter index\n",
      idx
      );
    ret = -1;
    goto fail;
  }

  /* check for DMA channel index */
  if ((l_core->num_max_chans > 0x0U) &&
      (dma_chan > (l_core->num_max_chans - 0x1U)) &&
      (dma_chan != OSI_CHAN_ANY))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OUTOFBOUND,
      "invalid dma channel\n",
      (nveul64_t)dma_chan
      );
    ret = -1;
    goto fail;
  }

  /* validate dma_chansel argument */
  if (dma_chansel > chansel_max[mac]) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OUTOFBOUND,
      "invalid dma_chansel value\n",
      dma_chansel
      );
    ret = -1;
    goto fail;
  }

  /* validate addr_mask argument */
  if (addr_mask > MGBE_MAB_ADDRH_MBC_MAX_MASK) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid addr_mask value\n",
      addr_mask
      );
    ret = -1;
    goto fail;
  }

  /* validate src_dest argument */
  if ((src_dest != OSI_SA_MATCH) && (src_dest != OSI_DA_MATCH)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid src_dest value\n",
      src_dest
      );
    ret = -1;
    goto fail;
  }

  /* validate dma_routing_enable argument */
  if ((dma_routing_enable != OSI_ENABLE) &&
      (dma_routing_enable != OSI_DISABLE))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid dma_routing value\n",
      dma_routing_enable
      );
    ret = -1;
  }

fail:
  return ret;
}

/**
 * @brief check_mac_addr - Compare macaddress with rchannel address
 *
 * @note
 * Algorithm: This function just validates macaddress with rchannel address.
 *
 * @param[in] mac_addr: Mac address.
 * @param[in] rch_addr: Receive channel address.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
check_mac_addr (
  nveu8_t const  *mac_addr,
  nveu8_t        *rch_addr
  )
{
  nve32_t  i   = 0;
  nve32_t  ret = OSI_NONE;

  for (i = 0; i < 6; i++) {
    if (*(mac_addr + i) != *(rch_addr + i)) {
      ret = -1;
      break;
    }
  }

  return ret;
}

/**
 * @brief mgbe_free_rchlist_index - Free index.
 *
 * @note
 * Algorithm: This function just free the Receive channel index.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] rch_idx: Receive channel index.
 *
 */
static void
mgbe_free_rchlist_index (
  struct osi_core_priv_data  *osi_core,
  const nve32_t              rch_idx
  )
{
  if ((rch_idx < 0) || (rch_idx >= (nve32_t)RCHLIST_SIZE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid rch_idx\n",
      rch_idx
      );
    return;
  }

  osi_core->rch_index[rch_idx].in_use = OSI_NONE;
  osi_core->rch_index[rch_idx].dch    = 0;
  osi_memset (&osi_core->rch_index[rch_idx].mac_address, 0, OSI_ETH_ALEN);
}

/**
 * @brief mgbe_get_rchlist_index - find free index
 *
 * @note
 * Algorithm: This function gets free index for receive channel list.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] mac_addr: Mac address.
 *
 * @retval 0 on success
 * @retval -1 on failure.

**/
static nve32_t
mgbe_get_rchlist_index (
  struct osi_core_priv_data  *osi_core,
  nveu8_t const              *mac_addr
  )
{
  nve32_t   ret = -1;
  nveu32_t  i   = 0;

  if (mac_addr != OSI_NULL) {
    for (i = 0; i < RCHLIST_SIZE; i++) {
      if (osi_core->rch_index[i].in_use == OSI_NONE) {
        continue;
      }

      if (check_mac_addr (
            mac_addr,
            osi_core->rch_index[i].mac_address
            )
          == OSI_NONE)
      {
        ret = i;
        goto done;
      }
    }
  }

  for (i = 0; i < RCHLIST_SIZE; i++) {
    if (osi_core->rch_index[i].in_use == OSI_NONE) {
      ret = i;
      break;
    }
  }

done:
  return ret;
}

/**
 * @brief mgbe_write_rchlist - add/update rchlist index with new value
 *
 * @note
 * Algorithm: This function will write Receive channel list entry registers into HW.
 * This function should be called 2 times, 1 for 0-31 channel update,
 * 2nd for 32-47 channel update, data filed in 2nd read should be 0 for bit
 * 48-63.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] acc_mode: 1 - continuation, 0 - single acccess
 * @param[in] addr: Rchlist register address.
 * @param[inout] data: Rchlist register data.
 * @param[in] read_write: Rchlist read - 0, write - 1
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_rchlist_write (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   acc_mode,
  nveu32_t                   addr,
  nveu32_t                   *data,
  nveu32_t                   read_write
  )
{
  nve32_t   ret   = 0;
  nveu8_t   *base = osi_core->base;
  nveu32_t  val   = 0U;

  if ((acc_mode != OSI_ENABLE) && (acc_mode != OSI_DISABLE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid acc_mode argment\n",
      acc_mode
      );
    ret = -1;
    goto done;
  }

  if ((read_write != OSI_ENABLE) && (read_write != OSI_DISABLE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid read_write argment\n",
      read_write
      );
    ret = -1;
    goto done;
  }

  /* Wait for ready */
  ret = osi_readl_poll_timeout (
          (base + MGBE_MTL_RXP_IND_CS),
          osi_core,
          MGBE_MTL_RXP_IND_CS_BUSY,
          OSI_NONE,
          MGBE_MTL_RCHlist_READ_UDELAY,
          MGBE_MTL_RCHlist_READ_RETRY
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to read/write\n",
      val
      );
    ret = -1;
    goto done;
  }

  if (read_write == OSI_ENABLE) {
    /* Write data into MTL_RXP_Indirect_Acc_Data */
    osi_writela (osi_core, *data, base + MGBE_MTL_RXP_IND_DATA);
  }

  /* Program MTL_RXP_Indirect_Acc_Control_Status */
  val = osi_readla (osi_core, base + MGBE_MTL_RXP_IND_CS);
  /* Reset ACCSEL bit */
  val &= ~MGBE_MTL_RXP_IND_CS_ACCSEL;
  /* ACCSEL for Rxchlist 0x2 */
  val |= MGBE_MTL_RXP_IND_RCH_ACCSEL;
  if (acc_mode == OSI_ENABLE) {
    val |= (MGBE_MTL_RXP_IND_CS_CRWEN | MGBE_MTL_RXP_IND_CS_CRWSEL);
  } else {
    val &= ~(MGBE_MTL_RXP_IND_CS_CRWEN | MGBE_MTL_RXP_IND_CS_CRWSEL);
  }

  /* Set WRRDN for write */
  if (read_write == OSI_ENABLE) {
    val |= MGBE_MTL_RXP_IND_CS_WRRDN;
  } else {
    val &= ~MGBE_MTL_RXP_IND_CS_WRRDN;
  }

  /* Clear and add ADDR */
  val &= ~MGBE_MTL_RXP_IND_CS_ADDR;
  val |= (addr & MGBE_MTL_RXP_IND_CS_ADDR);
  /* Start write */
  val |= MGBE_MTL_RXP_IND_CS_BUSY;
  osi_writela (osi_core, val, base + MGBE_MTL_RXP_IND_CS);

  /* Wait for complete */
  ret = osi_readl_poll_timeout (
          (base + MGBE_MTL_RXP_IND_CS),
          osi_core,
          MGBE_MTL_RXP_IND_CS_BUSY,
          OSI_NONE,
          MGBE_MTL_RCHlist_READ_UDELAY,
          MGBE_MTL_RCHlist_READ_RETRY
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to write\n",
      ret
      );
    ret = -1;
  }

  if (read_write == OSI_DISABLE) {
    /* Write data from MTL_RXP_Indirect_Acc_Data */
    *data = osi_readla (osi_core, base + MGBE_MTL_RXP_IND_DATA);
  }

done:
  return ret;
}

/**
 * @brief mgbe_rchlist_add_del - Add or delete based on the channel
 *
 * @note
 * Algorithm:
 * - This function will add or delete the receive channel list.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter: OSI filter structure.
 * @param[in] add_del: Rchlist add - 1, del - 0.
 * @param[inout] idx: Rchlist index.
 * @param[inout] rch: rch status.
 * if rch0_data and rch1_data is zero than rch is zero.
 * if rch0_data and rch1_data is non zero than rch is nozero.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_rchlist_add_del (
  struct osi_core_priv_data  *osi_core,
  const struct osi_filter    *filter,
  nveu32_t                   add_del,
  nve32_t                    *idx,
  nveu32_t                   *rch
  )
{
  nveu32_t  rch0_data = 0x0U;
  nveu32_t  rch1_data = 0x0U;
  nve32_t   rch_idx   = 0;
  nve32_t   ret       = 0;
  nveu32_t  dma_chan  = filter->dma_chan;

  rch_idx = mgbe_get_rchlist_index (osi_core, filter->mac_addr);
  if (rch_idx < 0) {
    /* error case */
    ret = -1;
    goto fail;
  }

  if (idx != OSI_NULL) {
    *idx =  rch_idx;
  }

  /* read currentl channel in rchlist for index */
  if (osi_core->rch_index[rch_idx].in_use != OSI_NONE) {
    /* handle error */
    ret = mgbe_rchlist_write (
            osi_core,
            0,
            (rch_idx * 16) + 0,
            &rch0_data,
            0
            );
    if (ret != OSI_NONE) {
      /* error case */
      goto fail;
    }

    if (osi_core->num_dma_chans > 32) {
      ret = mgbe_rchlist_write (
              osi_core,
              0,
              (rch_idx * 16) + 1,
              &rch1_data,
              0
              );
      if (ret != OSI_NONE) {
        /* error case */
        goto fail;
      }
    }
  }

  if (add_del) {
    /* add */
    if (dma_chan < 32) {
      rch0_data |= (nveu32_t)(1 << dma_chan);
    } else {
      rch1_data |= (nveu32_t)(1 << (dma_chan - 32));
    }
  } else {
    /* add */
    if (dma_chan < 32) {
      rch0_data &= (nveu32_t) ~(1 << dma_chan);
    } else {
      rch1_data &= (nveu32_t) ~(1 << (dma_chan - 32));
    }
  }

  if ((rch0_data == 0U) && (rch1_data == 0U)) {
    *rch = OSI_DISABLE;
  } else {
    *rch = OSI_ENABLE;
  }

  /* coresponding to each index there will be 2 entries address 0_0 and 0_1 */
  ret = mgbe_rchlist_write (osi_core, 0, (rch_idx * 16) + 0, &rch0_data, 1);
  if (ret != OSI_NONE) {
    /* error case */
    goto fail;
  }

  if (osi_core->num_dma_chans > 32) {
    ret = mgbe_rchlist_write (osi_core, 0, (rch_idx * 16) + 1, &rch1_data, 1);
    if (ret != OSI_NONE) {
      /* error case */
      goto fail;
    }
  }

  osi_core->rch_index[rch_idx].dch = rch1_data;
  osi_core->rch_index[rch_idx].dch = ((osi_core->rch_index[rch_idx].dch << 32) | rch0_data);
  if (add_del) {
    /* add */
    osi_core->rch_index[rch_idx].in_use = OSI_ENABLE;
    osi_memcpy (&osi_core->rch_index[rch_idx], filter->mac_addr, OSI_ETH_ALEN);
  } else {
    /* delete */
    if (osi_core->rch_index[rch_idx].dch == 0) {
      mgbe_free_rchlist_index (osi_core, rch_idx);
    }
  }

fail:
  return ret;
}

/**
 * @brief mgbe_update_mac_addr_low_high_reg- Update L2 address in filter
 *        register
 *
 * @note
 * Algorithm: This routine update MAC address to register for filtering
 *      based on dma_routing_enable, addr_mask and src_dest. Validation of
 *      dma_chan as well as DCS bit enabled in RXQ to DMA mapping register
 *      performed before updating DCS bits.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter: OSI filter structure.
 *
 * @note 1) MAC should be initialized and stated. see osi_start_mac()
 *       2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_update_mac_addr_low_high_reg (
  struct osi_core_priv_data *const  osi_core,
  const struct osi_filter           *filter
  )
{
  const nveu32_t  dch_dpc_reg[OSI_MAX_MAC_IP_TYPES] = {
    0xFF,             /* place holder */
    MGBE_MAC_DCHSEL,
    MGBE_MAC_DPCSEL
  };

  nveu32_t       idx         = filter->index;
  nveu32_t       dma_chan    = filter->dma_chan;
  nveu32_t       addr_mask   = filter->addr_mask;
  nveu32_t       src_dest    = filter->src_dest;
  const nveu8_t  *addr       = filter->mac_addr;
  nveu64_t       dma_chansel = filter->dma_chansel;
  nveu32_t       dpsel_value;
  nveu32_t       value   = 0x0U;
  nve32_t        ret     = 0;
  nve32_t        rch_idx = 0;
  nveu32_t       rch     = 0x0U;
  nveu32_t       xdcs_dds;

  /* Validate filter values */
  if (mgbe_filter_args_validate (osi_core, filter) < 0) {
    /* Filter argments validation got failed */
    ret = -1;
    goto fail;
  }

  // To make sure idx is not more than max to address CERT INT30-C
  idx   = idx % OSI_MGBE_MAX_MAC_ADDRESS_FILTER_T26X;
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MAC_ADDRH ((idx))
            );

  /* read current value at index preserve XDCS current value */
  ret = mgbe_mac_indir_addr_read (
          osi_core,
          dch_dpc_reg[osi_core->mac],
          idx,
          &xdcs_dds
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "indirect register read failed\n",
      0ULL
      );
    goto fail;
  }

  dpsel_value = xdcs_dds;

  /* Incase of T264
  1. DCH filed is extended to have 48 channel number as binary in DCH field so
     it shhould be used by detfault for all unicast packets
  2. XDCH and XDCHT are used to tell about 2 cases but as we have number of DMA
     channels as 48 , we should use XDCH as rchlistindex and XDCHT as 1
  algo:
  1. write DCH bit as binary representation for channel number by default
  2. DDS bit for that index  should be 0 , xDCS/XDCT is don't care
  3. If request to add one more channel for that index
     (check by seeing DCH field is not 0xffff and AE bit 0x1 or not)
     a) set DDS bit to 1 for that L2 index
     b) write hot bit representation of channel in rchlist for free index.
     use a 48*64 bit array for book keeping
          set bits for earlier dch and new dch as hot bit representation
     c) set XDCS as rchindex and XDCST as 1
     d) DCH filed is don't care but we need to have non zero value
  4. If request to delete one channel (expect all delete request one after other)
     a) if DDS filed is 1 for that index, if yes, it is using rxchanlist.
     b) read rxchlist and update bit as channel asked for delate, if only 1 channel
          get binary repesentation of that channel, update DCH
          reset XDCHT and XDCH to 0 for index
          DDS filed set to 0 for index
     c) if DDS files is 0, do not duplication for that index
          clear DCH files to 0xffff, AE bit set to 0x0
   */

  /* preserve last XDCS bits */
  xdcs_dds &= ((osi_core->mac == OSI_MAC_HW_MGBE) ?
               MGBE_MAC_XDCS_DMA_MAX : UINT_MAX);

  /* High address reset DCS and AE bits  and XDCS in MAC_DChSel_IndReg or
   * reset DDS bit in DPCSel reg
   */
  if ((filter->oper_mode & OSI_OPER_ADDR_DEL) != OSI_NONE) {
    if ((osi_core->mac == OSI_MAC_HW_MGBE_T26X) &&
        (filter->pkt_dup != OSI_NONE))
    {
      ret = mgbe_rchlist_add_del (osi_core, filter, 0, &rch_idx, &rch);
    }

    if ((osi_core->mac != OSI_MAC_HW_MGBE_T26X) || (rch == OSI_DISABLE)) {
      xdcs_dds &= ((osi_core->mac == OSI_MAC_HW_MGBE) ?
                   ~OSI_BIT(dma_chan) : ~OSI_BIT(1));
      ret = mgbe_mac_indir_addr_write (
              osi_core,
              dch_dpc_reg[osi_core->mac],
              idx,
              xdcs_dds
              );
      value &= ~(MGBE_MAC_ADDRH_DCS);
    }

    /* XDCS values is always maintained */
    if ((osi_core->mac == OSI_MAC_HW_MGBE) &&
        (xdcs_dds == OSI_DISABLE))
    {
      value &= ~(MGBE_MAC_ADDRH_AE);
    } else {
      value &= ~(MGBE_MAC_ADDRH_AE);
    }

    value |= OSI_MASK_16BITS;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MAC_ADDRH ((idx))
      );
    osi_writela (
      osi_core,
      OSI_MAX_32BITS,
      (nveu8_t *)osi_core->base +  MGBE_MAC_ADDRL ((idx))
      );
  } else {
    /* Add DMA channel to value in binary */
    value  = OSI_NONE;
    value |= ((dma_chan << MGBE_MAC_ADDRH_DCS_SHIFT) & MGBE_MAC_ADDRH_DCS);
    if (idx != 0U) {
      /* Add Address mask */
      value |= ((addr_mask << MGBE_MAC_ADDRH_MBC_SHIFT) &
                MGBE_MAC_ADDRH_MBC);

      /* Setting Source/Destination Address match valid */
      value |= ((src_dest << MGBE_MAC_ADDRH_SA_SHIFT) &
                MGBE_MAC_ADDRH_SA);
    }

    osi_writela (
      osi_core,
      ((nveu32_t)addr[4] | ((nveu32_t)addr[5] << 8) |
       MGBE_MAC_ADDRH_AE | value),
      (nveu8_t *)osi_core->base + MGBE_MAC_ADDRH ((idx))
      );

    osi_writela (
      osi_core,
      ((nveu32_t)addr[0] | ((nveu32_t)addr[1] << 8) |
       ((nveu32_t)addr[2] << 16) | ((nveu32_t)addr[3] << 24)),
      (nveu8_t *)osi_core->base +  MGBE_MAC_ADDRL ((idx))
      );

    if (osi_core->mac != OSI_MAC_HW_MGBE_T26X) {
      /* Write XDCS configuration into MAC_DChSel_IndReg(x) */
      /* Append DCS DMA channel to XDCS hot bit selection */
      xdcs_dds |= (OSI_BIT_64 (dma_chan) | dma_chansel);
      ret       = mgbe_mac_indir_addr_write (
                    osi_core,
                    MGBE_MAC_DCHSEL,
                    idx,
                    xdcs_dds
                    );
    } else {
      /* check for packet duplicate 0 - disable, 1 - enable */
      if (filter->pkt_dup != OSI_NONE) {
        dpsel_value |= MGBE_MAC_DPCSEL_DDS;
        ret          = mgbe_rchlist_add_del (
                         osi_core,
                         filter,
                         1,
                         &rch_idx,
                         &rch
                         );
        if (ret < 0) {
          OSI_CORE_ERR (
            osi_core->osd,
            OSI_LOG_ARG_INVALID,
            "rchlist add del failed\n",
            0ULL
            );
          goto fail;
        }

        value = OSI_NONE;

        if (idx != 0U) {
          /* Add Address mask */
          value |= ((addr_mask << MGBE_MAC_ADDRH_MBC_SHIFT) &
                    MGBE_MAC_ADDRH_MBC);

          /* Setting Source/Destination Address match valid */
          value |= ((src_dest << MGBE_MAC_ADDRH_SA_SHIFT) &
                    MGBE_MAC_ADDRH_SA);
        }

        // Restricting rch_idx to RCHLIST_SIZE to avoid CERT INT32-C
        rch_idx %= RCHLIST_SIZE;
        value   |= (((nveu32_t)rch_idx << MGBE_MAC_ADDRH_DCS_SHIFT) & MGBE_MAC_ADDRH_DCS);
        osi_writela (
          osi_core,
          ((nveu32_t)addr[4] | ((nveu32_t)addr[5] << 8) |
           MGBE_MAC_ADDRH_AE | value),
          (nveu8_t *)osi_core->base + MGBE_MAC_ADDRH ((idx))
          );

        osi_writela (
          osi_core,
          ((nveu32_t)addr[0] | ((nveu32_t)addr[1] << 8) |
           ((nveu32_t)addr[2] << 16) | ((nveu32_t)addr[3] << 24)),
          (nveu8_t *)osi_core->base +  MGBE_MAC_ADDRL ((idx))
          );
      } else {
        /* No duplication */
        xdcs_dds    &= ~(MGBE_MAC_XDCS_DMA_MAX | MGBE_MAC_XDCST_DMA_MAX);
        dpsel_value &= ~MGBE_MAC_DPCSEL_DDS;
      }

      /* TODO add error check */
      ret = mgbe_mac_indir_addr_write (
              osi_core,
              MGBE_MAC_DPCSEL,
              idx,
              dpsel_value
              );

      if (osi_core->mac != OSI_MAC_HW_MGBE_T26X) {
        ret = mgbe_mac_indir_addr_write (
                osi_core,
                MGBE_MAC_DCHSEL,
                idx,
                xdcs_dds
                );
      }
    }
  }

fail:
  return ret;
}

/**
 * @brief mgbe_poll_for_l3l4crtl - Poll for L3_L4 filter register operations.
 *
 * @note
 * Algorithm: Waits for waits for transfer busy bit to be cleared in
 * L3_L4 address control register to complete filter register operations.
 *
 * @param[in] osi_core: osi core priv data structure
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_poll_for_l3l4crtl (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t  retry = 10;
  nveu32_t  count;
  nveu32_t  l3l4_addr_ctrl = 0;
  nve32_t   cond           = 1;
  nve32_t   ret            = 0;

  /* Poll Until L3_L4_Address_Control XB is clear */
  count = 0;
  while (cond == 1) {
    if (count > retry) {
      /* Return error after max retries */
      ret = -1;
      goto fail;
    }

    count++;

    l3l4_addr_ctrl = osi_readla (
                       osi_core,
                       (nveu8_t *)osi_core->base +
                       MGBE_MAC_L3L4_ADDR_CTR
                       );
    if ((l3l4_addr_ctrl & MGBE_MAC_L3L4_ADDR_CTR_XB) == OSI_NONE) {
      /* Set cond to 0 to exit loop */
      cond = 0;
    } else {
      /* wait for 10 usec for XB clear */
      osi_core->osd_ops.usleep (MGBE_MAC_XB_WAIT);
    }
  }

fail:
  return ret;
}

/**
 * @brief mgbe_l3l4_filter_write - L3_L4 filter register write.
 *
 * @note
 * Algorithm: writes L3_L4 filter register
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] filter_no: MGBE  L3_L4 filter number
 * @param[in] filter_type: MGBE L3_L4 filter register type.
 * @param[in] value: MGBE  L3_L4 filter register value
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_l3l4_filter_write (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   filter_no,
  nveu32_t                   filter_type,
  nveu32_t                   value
  )
{
  void            *base                      = osi_core->base;
  nveu32_t        addr                       = 0;
  nve32_t         ret                        = 0;
  const nveu32_t  fnum[OSI_MAX_MAC_IP_TYPES] = {
    MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM,
    MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM,
    MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM_T264,
  };

  /* Write MAC_L3_L4_Data register value */
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)base + MGBE_MAC_L3L4_DATA
    );

  /* Program MAC_L3_L4_Address_Control */
  addr = osi_readla (
           osi_core,
           (nveu8_t *)base + MGBE_MAC_L3L4_ADDR_CTR
           );

  /* update filter number */
  addr &= ~(fnum[osi_core->mac]);
  addr |= ((filter_no << MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM_SHIFT) &
           (fnum[osi_core->mac]));

  /* update filter type */
  addr &= ~(MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE);
  addr |= ((filter_type << MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE_SHIFT) &
           MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE);

  /* Set TT filed 0 for write */
  addr &= ~(MGBE_MAC_L3L4_ADDR_CTR_TT);

  /* Set XB bit to initiate write */
  addr |= MGBE_MAC_L3L4_ADDR_CTR_XB;

  /* Write MGBE_MAC_L3L4_ADDR_CTR */
  osi_writela (
    osi_core,
    addr,
    (nveu8_t *)base + MGBE_MAC_L3L4_ADDR_CTR
    );

  /* Wait untile XB bit reset */
  if (mgbe_poll_for_l3l4crtl (osi_core) < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to write L3_L4_Address_Control\n",
      filter_type
      );
    ret = -1;
  }

  return ret;
}

/**
 * @brief mgbe_config_l3l4_filters - Config L3L4 filters.
 *
 * @note
 * Algorithm:
 * - This sequence is used to configure L3L4 filters for SA and DA Port Number matching.
 * - Prepare register data using prepare_l3l4_registers().
 * - Write l3l4 reigsters using mgbe_l3l4_filter_write().
 * - Return 0 on success.
 * - Return -1 on any register failure.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no_r: filter index
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (osi_l3_l4_filter)
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_l3l4_filters (
  struct osi_core_priv_data *const      osi_core,
  nveu32_t                              filter_no_r,
  const struct osi_l3_l4_filter *const  l3_l4
  )
{
 #ifndef OSI_STRIPPED_LIB
  nveu32_t  l3_addr0_reg = 0;
  nveu32_t  l3_addr2_reg = 0;
  nveu32_t  l3_addr3_reg = 0;
  nveu32_t  l4_addr_reg  = 0;
 #endif /* !OSI_STRIPPED_LIB */
  nveu32_t        l3_addr1_reg                        = 0;
  nveu32_t        ctr_reg                             = 0;
  const nveu32_t  max_filter_no[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_MAX_L3_L4_FILTER - 1U,
    OSI_MGBE_MAX_L3_L4_FILTER - 1U,
    OSI_MGBE_MAX_L3_L4_FILTER_T264 - 1U,
  };
  nveu32_t        filter_no = filter_no_r;
  nve32_t         err;
  nve32_t         ret = -1;

  if (filter_no_r > max_filter_no[osi_core->mac]) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Filter number is more than allowed\n",
      filter_no_r
      );
    goto exit_func;
  }

  prepare_l3l4_registers (
    osi_core,
    l3_l4,
 #ifndef OSI_STRIPPED_LIB
    &l3_addr0_reg,
    &l3_addr2_reg,
    &l3_addr3_reg,
    &l4_addr_reg,
 #endif /* !OSI_STRIPPED_LIB */
    &l3_addr1_reg,
    &ctr_reg
    );

 #ifndef OSI_STRIPPED_LIB
  /* Update l3 ip addr MGBE_MAC_L3_AD0R register */
  err = mgbe_l3l4_filter_write (osi_core, filter_no, MGBE_MAC_L3_AD0R, l3_addr0_reg);
  if (err < 0) {
    /* Write MGBE_MAC_L3_AD0R fail return error */
    goto exit_func;
  }

  /* Update l3 ip addr MGBE_MAC_L3_AD2R register */
  err = mgbe_l3l4_filter_write (osi_core, filter_no, MGBE_MAC_L3_AD2R, l3_addr2_reg);
  if (err < 0) {
    /* Write MGBE_MAC_L3_AD2R fail return error */
    goto exit_func;
  }

  /* Update l3 ip addr MGBE_MAC_L3_AD3R register */
  err = mgbe_l3l4_filter_write (osi_core, filter_no, MGBE_MAC_L3_AD3R, l3_addr3_reg);
  if (err < 0) {
    /* Write MGBE_MAC_L3_AD3R fail return error */
    goto exit_func;
  }

  /* Update l4 port register MGBE_MAC_L4_ADDR register */
  err = mgbe_l3l4_filter_write (osi_core, filter_no, MGBE_MAC_L4_ADDR, l4_addr_reg);
  if (err < 0) {
    /* Write MGBE_MAC_L4_ADDR fail return error */
    goto exit_func;
  }

 #endif /* !OSI_STRIPPED_LIB */

  /* Update l3 ip addr MGBE_MAC_L3_AD1R register */
  err = mgbe_l3l4_filter_write (osi_core, filter_no, MGBE_MAC_L3_AD1R, l3_addr1_reg);
  if (err < 0) {
    /* Write MGBE_MAC_L3_AD1R fail return error */
    goto exit_func;
  }

  /* Write CTR register */
  err = mgbe_l3l4_filter_write (osi_core, filter_no, MGBE_MAC_L3L4_CTR, ctr_reg);
  if (err < 0) {
    /* Write MGBE_MAC_L3L4_CTR fail return error */
    goto exit_func;
  }

  /* success */
  ret = 0;

exit_func:

  return ret;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief mgbe_config_vlan_filter_reg - config vlan filter register
 *
 * @note
 * Algorithm: This sequence is used to enable/disable VLAN filtering and
 *      also selects VLAN filtering mode- perfect/hash
 *
 * @param[in] osi_core: Base address  from OSI core private data structure.
 * @param[in] filter_enb_dis: vlan filter enable/disable
 * @param[in] perfect_hash_filtering: perfect or hash filter
 * @param[in] perfect_inverse_match: normal or inverse filter
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_vlan_filtering (
  struct osi_core_priv_data  *osi_core,
  const nveu32_t             filter_enb_dis,
  const nveu32_t             perfect_hash_filtering,
  const nveu32_t             perfect_inverse_match
  )
{
  nveu32_t  value;
  nveu8_t   *base = osi_core->base;

  /* validate perfect_inverse_match argument */
  if (perfect_hash_filtering == OSI_HASH_FILTER_MODE) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OPNOTSUPP,
      "VLAN hash filter is not supported, VTHM not updated\n",
      0ULL
      );
    return -1;
  }

  if (perfect_hash_filtering != OSI_PERFECT_FILTER_MODE) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid perfect_hash_filtering value\n",
      perfect_hash_filtering
      );
    return -1;
  }

  /* validate filter_enb_dis argument */
  if ((filter_enb_dis != OSI_ENABLE) && (filter_enb_dis != OSI_DISABLE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid filter_enb_dis value\n",
      filter_enb_dis
      );
    return -1;
  }

  /* validate perfect_inverse_match argument */
  if ((perfect_inverse_match != OSI_ENABLE) &&
      (perfect_inverse_match != OSI_DISABLE))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid perfect_inverse_match value\n",
      perfect_inverse_match
      );
    return -1;
  }

  /* Read MAC PFR value set VTFE bit */
  value  = osi_readla (osi_core, base + MAC_PKT_FILTER_REG);
  value &= ~(MGBE_MAC_PFR_VTFE);
  value |= ((filter_enb_dis << MGBE_MAC_PFR_VTFE_SHIFT) &
            MGBE_MAC_PFR_VTFE);
  osi_writela (osi_core, value, base + MAC_PKT_FILTER_REG);

  /* Read MAC VLAN TR register value set VTIM bit */
  value  = osi_readla (osi_core, base + MGBE_MAC_VLAN_TR);
  value &= ~(MGBE_MAC_VLAN_TR_VTIM | MGBE_MAC_VLAN_TR_VTHM);
  value |= ((perfect_inverse_match << MGBE_MAC_VLAN_TR_VTIM_SHIFT) &
            MGBE_MAC_VLAN_TR_VTIM);
  osi_writela (osi_core, value, base + MGBE_MAC_VLAN_TR);

  return 0;
}

/**
 * @brief mgbe_config_ptp_rxq - Config PTP RX packets queue route
 *
 * @note
 * Algorithm: This function is used to program the PTP RX packets queue.
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] rxq_idx: PTP RXQ index.
 * @param[in] enable: PTP RXQ route enable(1) or disable(0).
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_ptp_rxq (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    rxq_idx,
  const nveu32_t                    enable
  )
{
  nveu8_t   *base = osi_core->base;
  nveu32_t  value = 0U;
  nveu32_t  i     = 0U;

  /* Validate the RX queue index argument */
  if (rxq_idx >= OSI_MGBE_MAX_NUM_QUEUES) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid PTP RX queue index\n",
      rxq_idx
      );
    return -1;
  }

  /* Validate enable argument */
  if ((enable != OSI_ENABLE) && (enable != OSI_DISABLE)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid enable input\n",
      enable
      );
    return -1;
  }

  /* Validate PTP RX queue enable */
  for (i = 0; i < osi_core->num_mtl_queues; i++) {
    if (osi_core->mtl_queues[i] == rxq_idx) {
      /* Given PTP RX queue is enabled */
      break;
    }
  }

  if (i == osi_core->num_mtl_queues) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "PTP RX queue not enabled\n",
      rxq_idx
      );
    return -1;
  }

  /* Read MAC_RxQ_Ctrl1 */
  value = osi_readla (osi_core, base + MGBE_MAC_RQC1R);
  /* Check for enable or disable */
  if (enable == OSI_DISABLE) {
    /** Reset OMCBCQ bit to disable over-riding the MCBC Queue
     * priority for the PTP RX queue.
     **/
    value &= ~MGBE_MAC_RQC1R_OMCBCQ;
  } else {
    /* Store PTP RX queue into OSI private data */
    osi_core->ptp_config.ptp_rx_queue = rxq_idx;
    /* Program PTPQ with ptp_rxq */
    value &= ~MGBE_MAC_RQC1R_PTPQ;
    value |= (rxq_idx << MGBE_MAC_RQC1R_PTPQ_SHIFT);

    /** Set TPQC to 0x1 for VLAN Tagged PTP over
     * ethernet packets are routed to Rx Queue specified
     * by PTPQ field
     **/
    value |= MGBE_MAC_RQC1R_TPQC0;

    /** Set OMCBCQ bit to enable over-riding the MCBC Queue
     * priority for the PTP RX queue.
     **/
    value |= MGBE_MAC_RQC1R_OMCBCQ;
  }

  /* Write MAC_RxQ_Ctrl1 */
  osi_writela (osi_core, value, base + MGBE_MAC_RQC1R);

  return 0;
}

/**
 * @brief mgbe_config_mac_loopback - Configure MAC to support loopback
 *
 * @param[in] addr: Base address indicating the start of
 *            memory mapped IO region of the MAC.
 * @param[in] lb_mode: Enable or Disable MAC loopback mode
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_mac_loopback (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          lb_mode
  )
{
  nveu32_t  value;
  void      *addr = osi_core->base;

  /* don't allow only if loopback mode is other than 0 or 1 */
  if ((lb_mode != OSI_ENABLE) && (lb_mode != OSI_DISABLE)) {
    return -1;
  }

  /* Read MAC Configuration Register */
  value = osi_readla (osi_core, (nveu8_t *)addr + MGBE_MAC_RMCR);
  if (lb_mode == OSI_ENABLE) {
    /* Enable Loopback Mode */
    value |= MGBE_MAC_RMCR_LM;
  } else {
    value &= ~MGBE_MAC_RMCR_LM;
  }

  osi_writela (osi_core, value, (nveu8_t *)addr + MGBE_MAC_RMCR);

  return 0;
}

/**
 * @brief mgbe_config_arp_offload - Enable/Disable ARP offload
 *
 * @note
 * Algorithm:
 *      1) Read the MAC configuration register
 *      2) If ARP offload is to be enabled, program the IP address in
 *      ARPPA register
 *      3) Enable/disable the ARPEN bit in MCR and write back to the MCR.
 *
 * @param[in] addr: MGBE virtual base address.
 * @param[in] enable: Flag variable to enable/disable ARP offload
 * @param[in] ip_addr: IP address of device to be programmed in HW.
 *            HW will use this IP address to respond to ARP requests.
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) Valid 4 byte IP address as argument ip_addr
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_arp_offload (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable,
  const nveu8_t                     *ip_addr
  )
{
  nveu32_t  mac_rmcr;
  nveu32_t  val;
  void      *addr = osi_core->base;

  if ((enable != OSI_ENABLE) && (enable != OSI_DISABLE)) {
    return -1;
  }

  mac_rmcr = osi_readla (osi_core, (nveu8_t *)addr + MGBE_MAC_RMCR);

  if (enable == OSI_ENABLE) {
    val = (((nveu32_t)ip_addr[0]) << 24) |
          (((nveu32_t)ip_addr[1]) << 16) |
          (((nveu32_t)ip_addr[2]) << 8) |
          (((nveu32_t)ip_addr[3]));

    osi_writela (
      osi_core,
      val,
      (nveu8_t *)addr + MGBE_MAC_ARPPA
      );

    mac_rmcr |= MGBE_MAC_RMCR_ARPEN;
  } else {
    mac_rmcr &= ~MGBE_MAC_RMCR_ARPEN;
  }

  osi_writela (osi_core, mac_rmcr, (nveu8_t *)addr + MGBE_MAC_RMCR);

  return 0;
}

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief mgbe_config_frp - Enable/Disale RX Flexible Receive Parser in HW
 *
 * @note
 * Algorithm:
 *      1) Read the MTL OP Mode configuration register.
 *      2) Enable/Disable FRPE bit based on the input.
 *      3) Write the MTL OP Mode configuration register.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enabled: Flag to indicate feature is to be enabled/disabled.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_frp (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enabled
  )
{
  nveu8_t   *base = osi_core->base;
  nveu32_t  op_mode = 0U, val = 0U;
  nve32_t   ret = 0;

  op_mode = osi_readla (osi_core, base + MGBE_MTL_OP_MODE);
  if (enabled == OSI_ENABLE) {
    /* Set FRPE bit of MTL_Operation_Mode register */
    op_mode |= MGBE_MTL_OP_MODE_FRPE;
    osi_writela (osi_core, op_mode, base + MGBE_MTL_OP_MODE);

    /* Verify RXPI bit set in MTL_RXP_Control_Status */
    ret = osi_readl_poll_timeout (
            (base + MGBE_MTL_RXP_CS),
            osi_core,
            MGBE_MTL_RXP_CS_RXPI,
            MGBE_MTL_RXP_CS_RXPI,
            (MGBE_MTL_FRP_READ_UDELAY),
            MGBE_MTL_FRP_READ_RETRY
            );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Fail to enable FRP\n",
        val
        );
      ret = -1;
      goto done;
    }

    /* Enable FRP Interrupts in MTL_RXP_Interrupt_Control_Status */
    val  = osi_readla (osi_core, base + MGBE_MTL_RXP_INTR_CS);
    val |= (MGBE_MTL_RXP_INTR_CS_NVEOVIE |
            MGBE_MTL_RXP_INTR_CS_NPEOVIE |
            MGBE_MTL_RXP_INTR_CS_FOOVIE |
            MGBE_MTL_RXP_INTR_CS_PDRFIE);
    osi_writela (osi_core, val, base + MGBE_MTL_RXP_INTR_CS);
  } else {
    /* Reset FRPE bit of MTL_Operation_Mode register */
    op_mode &= ~MGBE_MTL_OP_MODE_FRPE;
    osi_writela (osi_core, op_mode, base + MGBE_MTL_OP_MODE);

    /* Verify RXPI bit reset in MTL_RXP_Control_Status */
    ret = osi_readl_poll_timeout (
            (base + MGBE_MTL_RXP_CS),
            osi_core,
            MGBE_MTL_RXP_CS_RXPI,
            OSI_NONE,
            (MGBE_MTL_FRP_READ_UDELAY),
            (MGBE_MTL_FRP_READ_RETRY)
            );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Fail to disable FRP\n",
        val
        );
      ret = -1;
      goto done;
    }

    /* Disable FRP Interrupts in MTL_RXP_Interrupt_Control_Status */
    val  = osi_readla (osi_core, base + MGBE_MTL_RXP_INTR_CS);
    val &= ~(MGBE_MTL_RXP_INTR_CS_NVEOVIE |
             MGBE_MTL_RXP_INTR_CS_NPEOVIE |
             MGBE_MTL_RXP_INTR_CS_FOOVIE |
             MGBE_MTL_RXP_INTR_CS_PDRFIE);
    osi_writela (osi_core, val, base + MGBE_MTL_RXP_INTR_CS);
  }

done:
  return ret;
}

/**
 * @brief mgbe_frp_write - Write FRP entry into HW
 *
 * @note
 * Algorithm: This function will write FRP entry registers into HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] acc_sel: FRP Indirect Access Selection.
 *                     0x0 : Access FRP Instruction Table.
 *                     0x1 : Access Indirect FRP Register block.
 * @param[in] addr: FRP register address.
 * @param[in] data: FRP register data.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_frp_write (
  struct osi_core_priv_data  *osi_core,
  OSI_UNUSED nveu32_t        acc_sel,
  nveu32_t                   addr,
  nveu32_t                   data
  )
{
  nve32_t   ret   = 0;
  nveu8_t   *base = osi_core->base;
  nveu32_t  val   = 0U;

  (void)acc_sel;

  /* Wait for ready */
  ret = osi_readl_poll_timeout (
          (base + MGBE_MTL_RXP_IND_CS),
          osi_core,
          MGBE_MTL_RXP_IND_CS_BUSY,
          OSI_NONE,
          (MGBE_MTL_FRP_READ_UDELAY),
          (MGBE_MTL_FRP_READ_RETRY)
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to write\n",
      val
      );
    ret = -1;
    goto done;
  }

  /* Write data into MTL_RXP_Indirect_Acc_Data */
  osi_writela (osi_core, data, base + MGBE_MTL_RXP_IND_DATA);

  /* Program MTL_RXP_Indirect_Acc_Control_Status */
  val = osi_readla (osi_core, base + MGBE_MTL_RXP_IND_CS);
  /* Reset RCH bit */
  val &= ~MGBE_MTL_RXP_IND_RCH_ACCSEL;

  /* Currently acc_sel is always 0 which means FRP Indirect Access Selection
   * is Access FRP Instruction Table
   */
  val &= ~MGBE_MTL_RXP_IND_CS_ACCSEL;

  /* Set WRRDN for write */
  val |= MGBE_MTL_RXP_IND_CS_WRRDN;
  /* Clear and add ADDR */
  val &= ~MGBE_MTL_RXP_IND_CS_ADDR;
  val |= (addr & MGBE_MTL_RXP_IND_CS_ADDR);
  /* Start write */
  val |= MGBE_MTL_RXP_IND_CS_BUSY;
  osi_writela (osi_core, val, base + MGBE_MTL_RXP_IND_CS);

  /* Wait for complete */
  ret = osi_readl_poll_timeout (
          (base + MGBE_MTL_RXP_IND_CS),
          osi_core,
          MGBE_MTL_RXP_IND_CS_BUSY,
          OSI_NONE,
          (MGBE_MTL_FRP_READ_UDELAY),
          (MGBE_MTL_FRP_READ_RETRY)
          );
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "Fail to write\n",
      val
      );
    ret = -1;
  }

done:
  return ret;
}

/**
 * @brief mgbe_update_frp_entry - Update FRP Instruction Table entry in HW
 *
 * @note
 * Algorithm:
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] pos_val: FRP Instruction Table entry location.
 * @param[in] data: FRP entry data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_update_frp_entry (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    pos_val,
  struct osi_core_frp_data *const   data
  )
{
  nveu32_t  val = 0U, tmp = 0U;
  nve32_t   ret       = -1;
  nveu32_t  rch0_data = 0x0U;
  nveu32_t  rch1_data = 0x0U;
  nve32_t   rch_idx   = 0;
  nveu32_t  pos       = (pos_val & 0xFFU);

  if ((osi_core->mac == OSI_MAC_HW_MGBE_T26X) && (data->dcht == OSI_ENABLE)) {
    if (data->accept_frame == OSI_ENABLE) {
      rch0_data = (nveu32_t)(data->dma_chsel  & 0xFFFFFFFF);
      rch1_data = (nveu32_t)((data->dma_chsel >> 32) & 0xFFFFFFFF);
      ret       = mgbe_rchlist_write (
                    osi_core,
                    OSI_DISABLE,
                    (rch_idx * 16) + 0,
                    &rch0_data,
                    OSI_ENABLE
                    );
      if (ret != OSI_NONE) {
        /* error case */
        ret = -1;
        goto done;
      }

      if (osi_core->num_dma_chans > 32) {
        mgbe_rchlist_write (
          osi_core,
          OSI_DISABLE,
          (rch_idx * 16) + 1,
          &rch1_data,
          OSI_ENABLE
          );
        if (ret != OSI_NONE) {
          /* error case */
          ret = -1;
          goto done;
        }
      }
    }
  }

  /** Write Match Data into IE0 **/
  val = data->match_data;
  ret = mgbe_frp_write (osi_core, OSI_DISABLE, MGBE_MTL_FRP_IE0 (pos), val);
  if (ret < 0) {
    /* Match Data Write fail */
    ret = -1;
    goto done;
  }

  /** Write Match Enable into IE1 **/
  val = data->match_en;
  ret = mgbe_frp_write (osi_core, OSI_DISABLE, MGBE_MTL_FRP_IE1 (pos), val);
  if (ret < 0) {
    /* Match Enable Write fail */
    ret = -1;
    goto done;
  }

  /** Write AF, RF, IM, NIC, FO and OKI into IE2 **/
  val = 0;
  if (data->accept_frame == OSI_ENABLE) {
    /* Set AF Bit */
    val |= MGBE_MTL_FRP_IE2_AF;
  }

  if (data->reject_frame == OSI_ENABLE) {
    /* Set RF Bit */
    val |= MGBE_MTL_FRP_IE2_RF;
  }

  if (data->inverse_match == OSI_ENABLE) {
    /* Set IM Bit */
    val |= MGBE_MTL_FRP_IE2_IM;
  }

  if (data->next_ins_ctrl == OSI_ENABLE) {
    /* Set NIC Bit */
    val |= MGBE_MTL_FRP_IE2_NC;
  }

  if ((osi_core->mac == OSI_MAC_HW_MGBE_T26X) && (data->dcht == OSI_ENABLE)) {
    val |= MGBE_MTL_FRP_IE2_DCHT;
  }

  tmp  = data->frame_offset;
  val |= ((tmp << MGBE_MTL_FRP_IE2_FO_SHIFT) & MGBE_MTL_FRP_IE2_FO);
  tmp  = data->ok_index;
  val |= ((tmp << MGBE_MTL_FRP_IE2_OKI_SHIFT) & MGBE_MTL_FRP_IE2_OKI);
  ret  = mgbe_frp_write (osi_core, OSI_DISABLE, MGBE_MTL_FRP_IE2 (pos), val);
  if (ret < 0) {
    /* FRP IE2 Write fail */
    ret = -1;
    goto done;
  }

  /** Write DCH into IE3 **/
  if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
    if (data->dcht == OSI_DISABLE) {
      val = (data->dma_chsel & MGBE_MTL_FRP_IE3_DCH_MASK);
    } else {
      val = 0;
    }
  } else {
    val = (data->dma_chsel & MGBE_MTL_FRP_IE3_DCH_MASK);
  }

  ret = mgbe_frp_write (osi_core, OSI_DISABLE, MGBE_MTL_FRP_IE3 (pos), val);
  if (ret < 0) {
    /* DCH Write fail */
    ret = -1;
  }

done:
  return ret;
}

/**
 * @brief mgbe_update_frp_nve - Update FRP NVE into HW
 *
 * @note
 * Algorithm:
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] nve: Number of Valid Entries.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static void
mgbe_update_frp_nve (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    nve
  )
{
  nveu32_t  val;
  nveu8_t   *base = osi_core->base;

  /* Update NVE and NPE in MTL_RXP_Control_Status register */
  val = osi_readla (osi_core, base + MGBE_MTL_RXP_CS);
  /* Clear old NVE and NPE */
  val &= ~(MGBE_MTL_RXP_CS_NVE | MGBE_MTL_RXP_CS_NPE);
  /* Add new NVE and NPE */
  val |= (nve & MGBE_MTL_RXP_CS_NVE);
  val |= ((nve << MGBE_MTL_RXP_CS_NPE_SHIFT) & MGBE_MTL_RXP_CS_NPE);
  if (osi_core->uphy_gbe_mode == OSI_GBE_MODE_25G) {
    /* Clear ANP first and for 25G overwrite Active Number of PIPE with 3 */
    val = (val & MGBE_MTL_RXP_CS_CLR_ANP) | MGBE_MTL_RXP_CS_ANP;
  }

  osi_writela (osi_core, val, base + MGBE_MTL_RXP_CS);
}

/**
 * @brief mgbe_configure_mtl_queue - Configure MTL Queue
 *
 * @note
 * Algorithm: This takes care of configuring the  below
 *      parameters for the MTL Queue
 *      1) Mapping MTL Rx queue and DMA Rx channel
 *      2) Flush TxQ
 *      3) Enable Store and Forward mode for Tx, Rx
 *      4) Configure Tx and Rx MTL Queue sizes
 *      5) Configure TxQ weight
 *      6) Enable Rx Queues
 *      7) Enable TX Underflow Interrupt for MTL Q
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] hw_qinx: Queue number that need to be configured.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_configure_mtl_queue (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   hw_qinx
  )
{
  nveu32_t  qinx = hw_qinx & 0xFU;

  /*
   * Total available Rx queue size is 192KB in T23x, 256KB in T26x.
   * Below is the destribution among the Rx queueu -
   * Q0 - 160KB for T23x and 224KB for T26x
   * Q1 to Q8 - 2KB each = 8 * 2KB = 16KB
   * Q9 - 16KB (MVBCQ)
   *
   * Formula to calculate the value to be programmed in HW
   *
   * vale= (size in KB / 256) - 1U
   */
  const nveu32_t  rx_fifo_sz[OSI_MAX_MAC_IP_TYPES][OSI_MGBE_MAX_NUM_QUEUES] = {
    {
      0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
    },
    {
      FIFO_SZ (160U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U),
      FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U),
      FIFO_SZ (2U), FIFO_SZ (16U)
    },
    {
      FIFO_SZ (224U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U),
      FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U),
      FIFO_SZ (2U), FIFO_SZ (16U)
    },
  };
  const nveu32_t  tx_fifo_sz[OSI_MGBE_MAX_NUM_QUEUES] = {
    TX_FIFO_SZ, TX_FIFO_SZ, TX_FIFO_SZ, TX_FIFO_SZ, TX_FIFO_SZ,
    TX_FIFO_SZ, TX_FIFO_SZ, TX_FIFO_SZ, TX_FIFO_SZ, TX_FIFO_SZ,
  };
  const nveu32_t  ufpga_tx_fifo_sz[OSI_MGBE_MAX_NUM_QUEUES] = {
    TX_FIFO_SZ_UFPGA, TX_FIFO_SZ_UFPGA, TX_FIFO_SZ_UFPGA,
    TX_FIFO_SZ_UFPGA, TX_FIFO_SZ_UFPGA, TX_FIFO_SZ_UFPGA,
    TX_FIFO_SZ_UFPGA, TX_FIFO_SZ_UFPGA, TX_FIFO_SZ_UFPGA,
    TX_FIFO_SZ_UFPGA
  };
  const nveu32_t  ufpga_rx_fifo_sz[OSI_MGBE_MAX_NUM_QUEUES] = {
    FIFO_SZ (40U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U),
    FIFO_SZ (2U),  FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (2U), FIFO_SZ (8U),
  };
  const nveu32_t  rfd_rfa[OSI_MGBE_MAX_NUM_QUEUES] = {
    FULL_MINUS_32_K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
    FULL_MINUS_1_5K,
  };
  nveu32_t        value = 0;
  nve32_t         ret   = 0;

  /* Program ETSALG (802.1Qaz) and RAA in MTL_Operation_Mode
   * register to initialize the MTL operation in case
   * of multiple Tx and Rx queues default : ETSALG WRR RAA SP
   */

  /* Program the priorities mapped to the Selected Traffic
   * Classes in MTL_TC_Prty_Map0-3 registers. This register is
   * to tell traffic class x should be blocked from transmitting
   * for the specified pause time when a PFC packet is received
   * with priorities matching the priorities programmed in this field
   * default: 0x0
   */

  /* Program the Transmit Selection Algorithm (TSA) in
   * MTL_TC[n]_ETS_Control register for all the Selected
   * Traffic Classes (n=0, 1, â€¦, Max selected number of TCs - 1)
   * Setting related to CBS will come here for TC.
   * default: 0x0 SP
   */
  ret = hw_flush_mtl_tx_queue (osi_core, qinx);
  if (ret < 0) {
    goto fail;
  }

  if (osi_unlikely (
        (qinx >= OSI_MGBE_MAX_NUM_QUEUES) ||
        (osi_core->tc[qinx] >= OSI_MAX_TC_NUM)
        ))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Incorrect queues/TC number\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_CHX_TX_OP_MODE (qinx)
            );
  value &= ~MGBE_MTL_Q_SIZE_MASK;
  if (osi_core->pre_sil == OSI_ENABLE) {
    value |= (ufpga_tx_fifo_sz[qinx] << MGBE_MTL_TXQ_SIZE_SHIFT);
  } else {
    value |= (tx_fifo_sz[qinx] << MGBE_MTL_TXQ_SIZE_SHIFT);
  }

  /* Enable Store and Forward mode */
  value |= MGBE_MTL_TSF;
  /*TTC  not applicable for TX*/
  /* Enable TxQ */
  value |= MGBE_MTL_TXQEN;

  if (osi_core->mac == OSI_MAC_HW_MGBE) {
    /* Q2TCMAP is reserved for T26x */
    value &= ~MGBE_MTL_TX_OP_MODE_Q2TCMAP;
    value |= (osi_core->tc[qinx] << MGBE_MTL_CHX_TX_OP_MODE_Q2TC_SH);
  }

  osi_writela (
    osi_core,
    value,
    (nveu8_t *)
    osi_core->base + MGBE_MTL_CHX_TX_OP_MODE (qinx)
    );

  /* read RX Q0 Operating Mode Register */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_CHX_RX_OP_MODE (qinx)
            );
  value &= ~MGBE_MTL_Q_SIZE_MASK;
  if (osi_core->pre_sil == OSI_ENABLE) {
    value |= (ufpga_rx_fifo_sz[qinx] << MGBE_MTL_RXQ_SIZE_SHIFT);
  } else {
    value |= (rx_fifo_sz[osi_core->mac][qinx] << MGBE_MTL_RXQ_SIZE_SHIFT);
  }

  /* Enable Store and Forward mode */
  value |= MGBE_MTL_RSF;
  /* Enable HW flow control */
  value |= MGBE_MTL_RXQ_OP_MODE_EHFC;

  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_CHX_RX_OP_MODE (qinx)
    );

  /* Update RFA and RFD
   * RFA: Threshold for Activating Flow Control
   * RFD: Threshold for Deactivating Flow Control
   */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_RXQ_FLOW_CTRL (qinx)
            );
  value &= ~MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
  value &= ~MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
  value |= (rfd_rfa[qinx] << MGBE_MTL_RXQ_OP_MODE_RFD_SHIFT) & MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
  value |= (rfd_rfa[qinx] << MGBE_MTL_RXQ_OP_MODE_RFA_SHIFT) & MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_RXQ_FLOW_CTRL (qinx)
    );

  /* Transmit Queue weight, all TX weights are equal */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_TCQ_QW (qinx)
            );
  value |= MGBE_MTL_TCQ_QW_ISCQW;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_TCQ_QW (qinx)
    );

  /* Default ETS tx selection algo */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_TCQ_ETS_CR (osi_core->tc[qinx])
            );
  value &= ~MGBE_MTL_TCQ_ETS_CR_AVALG;
  value |= OSI_MGBE_TXQ_AVALG_ETS;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_TCQ_ETS_CR (osi_core->tc[qinx])
    );

  /* Enable Rx Queue Control */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MAC_RQC0R
            );
  value |= ((osi_core->rxq_ctrl[qinx] & MGBE_MAC_RXQC0_RXQEN_MASK) <<
            (MGBE_MAC_RXQC0_RXQEN_SHIFT (qinx)));
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MAC_RQC0R
    );
fail:
  return ret;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief mgbe_rss_write_reg - Write into RSS registers
 *
 * @note
 * Algorithm: Programes RSS hash table or RSS hash key.
 *
 * @param[in] addr: MAC base address
 * @param[in] idx: Hash table or key index
 * @param[in] value: Value to be programmed in RSS data register.
 * @param[in] is_key: To represent passed value key or table data.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_rss_write_reg (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   idx,
  nveu32_t                   value,
  nveu32_t                   is_key
  )
{
  nveu8_t   *addr = (nveu8_t *)osi_core->base;
  nveu32_t  retry = 100U;
  nveu32_t  ctrl  = 0U;
  nveu32_t  count = 0U;
  nve32_t   cond  = 1;
  nve32_t   ret   = 0;

  /* data into RSS Lookup Table or RSS Hash Key */
  osi_writela (osi_core, value, addr + MGBE_MAC_RSS_DATA);

  if (is_key == OSI_ENABLE) {
    ctrl |= MGBE_MAC_RSS_ADDR_ADDRT;
  }

  ctrl |= idx << MGBE_MAC_RSS_ADDR_RSSIA_SHIFT;
  ctrl |= MGBE_MAC_RSS_ADDR_OB;
  ctrl &= ~MGBE_MAC_RSS_ADDR_CT;
  osi_writela (osi_core, ctrl, addr + MGBE_MAC_RSS_ADDR);

  /* poll for write operation to complete */
  while (cond == 1) {
    if (count > retry) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Failed to update RSS Hash key or table\n",
        0ULL
        );
      ret = -1;
      goto err;
    }

    count++;

    value = osi_readla (osi_core, addr + MGBE_MAC_RSS_ADDR);
    if ((value & MGBE_MAC_RSS_ADDR_OB) == OSI_NONE) {
      cond = 0;
    } else {
      osi_core->osd_ops.usleep (OSI_DELAY_100US);
    }
  }

err:
  return ret;
}

static nve32_t
mgbe_rss_wait_for_completion (
  struct osi_core_priv_data *const  osi_core,
  nveu8_t                           *addr
  )
{
  nveu32_t  retry = 100U;
  nveu32_t  count = 0U;
  nve32_t   cond  = 1;
  nveu32_t  value = 0U;
  nve32_t   ret   = 0;

  /* poll for write operation to complete */
  while (cond == 1) {
    if (count > retry) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Failed to read RSS Hash key or table\n",
        0ULL
        );
      ret = -1;
      goto err;
    }

    count++;

    value = osi_readla (osi_core, addr + MGBE_MAC_RSS_ADDR);
    if ((value & MGBE_MAC_RSS_ADDR_OB) == OSI_NONE) {
      cond = 0;
    } else {
      osi_core->osd_ops.usleep (OSI_DELAY_100US);
    }
  }

err:
  return ret;
}

static nve32_t
mgbe_rss_read_key (
  struct osi_core_priv_data *const  osi_core,
  nveu8_t                           *rss_key
  )
{
  nveu8_t   *addr = (nveu8_t *)osi_core->base;
  nveu32_t  ctrl  = 0U;
  nveu32_t  value = 0U;
  nveu32_t  i     = 0U;
  nveu32_t  j     = 0U;
  nve32_t   ret   = 0;

  /* Read hash key - 4 bytes at a time to match write pattern */
  for (i = 0U; i < OSI_RSS_HASH_KEY_SIZE; i += 4U) {
    /* Setup control register for reading hash key */
    ctrl  = MGBE_MAC_RSS_ADDR_ADDRT;            /* Set for hash key read */
    ctrl |= (j << MGBE_MAC_RSS_ADDR_RSSIA_SHIFT);
    ctrl |= MGBE_MAC_RSS_ADDR_OB;
    ctrl |= MGBE_MAC_RSS_ADDR_CT;             /* Set read bit */
    osi_writela (osi_core, ctrl, addr + MGBE_MAC_RSS_ADDR);

    /* Wait for read operation to complete */
    ret = mgbe_rss_wait_for_completion (osi_core, addr);
    if (ret < 0) {
      break;
    }

    /* Read 4 bytes of hash key */
    value           = osi_readla (osi_core, addr + MGBE_MAC_RSS_DATA);
    rss_key[i]      = (nveu8_t)(value & 0xFFU);
    rss_key[i + 1U] = (nveu8_t)((value >> 8U) & 0xFFU);
    rss_key[i + 2U] = (nveu8_t)((value >> 16U) & 0xFFU);
    rss_key[i + 3U] = (nveu8_t)((value >> 24U) & 0xFFU);
    j++;
  }

  return ret;
}

static nve32_t
mgbe_rss_read_table (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          *table
  )
{
  nveu8_t   *addr = (nveu8_t *)osi_core->base;
  nveu32_t  ctrl  = 0U;
  nveu32_t  i;
  nve32_t   ret = 0;

  /* Read hash table */
  for (i = 0U; i < OSI_RSS_MAX_TABLE_SIZE; i++) {
    /* Setup control register for reading hash table */
    ctrl  = 0U;            /* Clear ADDRT bit for table read */
    ctrl |= (i << MGBE_MAC_RSS_ADDR_RSSIA_SHIFT);
    ctrl |= MGBE_MAC_RSS_ADDR_OB;
    ctrl |= MGBE_MAC_RSS_ADDR_CT;             /* Set read bit */
    osi_writela (osi_core, ctrl, addr + MGBE_MAC_RSS_ADDR);

    /* Wait for read operation to complete */
    ret = mgbe_rss_wait_for_completion (osi_core, addr);
    if (ret < 0) {
      break;
    }

    /* Read the hash table entry */
    table[i] = osi_readla (osi_core, addr + MGBE_MAC_RSS_DATA);
  }

  return ret;
}

/**
 * @brief mgbe_get_rss - Get RSS configuration
 *
 * Algorithm: get RSS hash table or RSS hash key.
 *
 * @param[in] osi_core: OSI core private data.
 * @param[out] rss: RSS data.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_get_rss (
  struct osi_core_priv_data  *osi_core,
  struct osi_core_rss        *rss
  )
{
  nveu8_t   *addr = (nveu8_t *)osi_core->base;
  nveu32_t  value;
  nve32_t   ret = 0;

  /* Read hash key */
  ret = mgbe_rss_read_key (osi_core, rss->key);
  if (ret < 0) {
    goto err;
  }

  /* Read hash table */
  ret = mgbe_rss_read_table (osi_core, rss->table);
  if (ret < 0) {
    goto err;
  }

  /* Read RSS enable status */
  value       = osi_readla (osi_core, addr + MGBE_MAC_RSS_CTRL);
  rss->enable = ((value & MGBE_MAC_RSS_CTRL_RSSE) != 0U) ? OSI_ENABLE : OSI_DISABLE;

err:
  return ret;
}

/**
 * @brief mgbe_config_rss - Configure RSS
 *
 * @note
 * Algorithm: Programes RSS hash table or RSS hash key.
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] rss: RSS data.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_rss (
  struct osi_core_priv_data  *osi_core,
  const struct osi_core_rss  *rss
  )
{
  nveu8_t   *addr = (nveu8_t *)osi_core->base;
  nveu32_t  value = 0;
  nveu32_t  i = 0, j = 0;
  nve32_t   ret = 0;

  if (rss->enable == OSI_DISABLE) {
    /* RSS not supported */
    goto exit;
  }

  /* No need to enable RSS for single Queue */
  if (osi_core->num_mtl_queues == 1U) {
    goto exit;
  }

  /* Program the hash key */
  for (i = 0; i < OSI_RSS_HASH_KEY_SIZE; i += 4U) {
    value = ((nveu32_t)rss->key[i] |
             ((nveu32_t)rss->key[i + 1U] << 8U) |
             ((nveu32_t)rss->key[i + 2U] << 16U) |
             ((nveu32_t)rss->key[i + 3U] << 24U));
    ret = mgbe_rss_write_reg (osi_core, j, value, OSI_ENABLE);
    if (ret < 0) {
      goto exit;
    }

    j++;
  }

  /* Program Hash table */
  for (i = 0; i < OSI_RSS_MAX_TABLE_SIZE; i++) {
    ret = mgbe_rss_write_reg (
            osi_core,
            i,
            rss->table[i],
            OSI_NONE
            );
    if (ret < 0) {
      goto exit;
    }
  }

  /* Enable RSS */
  value  = osi_readla (osi_core, addr + MGBE_MAC_RSS_CTRL);
  value |= MGBE_MAC_RSS_CTRL_UDP4TE | MGBE_MAC_RSS_CTRL_TCP4TE |
           MGBE_MAC_RSS_CTRL_IP2TE | MGBE_MAC_RSS_CTRL_RSSE;
  osi_writela (osi_core, value, addr + MGBE_MAC_RSS_CTRL);
exit:
  return ret;
}

/**
 * @brief mgbe_config_flow_control - Configure MAC flow control settings
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] flw_ctrl: flw_ctrl settings
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_flow_control (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    flw_ctrl
  )
{
  nveu32_t  val;
  void      *addr = osi_core->base;

  /* return on invalid argument */
  if (flw_ctrl > (OSI_FLOW_CTRL_RX | OSI_FLOW_CTRL_TX)) {
    return -1;
  }

  /* Configure MAC Tx Flow control */
  /* Read MAC Tx Flow control Register of Q0 */
  val = osi_readla (
          osi_core,
          (nveu8_t *)addr + MGBE_MAC_QX_TX_FLW_CTRL (0U)
          );

  /* flw_ctrl BIT0: 1 is for tx flow ctrl enable
   * flw_ctrl BIT0: 0 is for tx flow ctrl disable
   */
  if ((flw_ctrl & OSI_FLOW_CTRL_TX) == OSI_FLOW_CTRL_TX) {
    /* Enable Tx Flow Control */
    val |= MGBE_MAC_QX_TX_FLW_CTRL_TFE;
    /* Mask and set Pause Time */
    val &= ~MGBE_MAC_PAUSE_TIME_MASK;
    val |= MGBE_MAC_PAUSE_TIME & MGBE_MAC_PAUSE_TIME_MASK;
  } else {
    /* Disable Tx Flow Control */
    val &= ~MGBE_MAC_QX_TX_FLW_CTRL_TFE;
  }

  /* Write to MAC Tx Flow control Register of Q0 */
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)addr + MGBE_MAC_QX_TX_FLW_CTRL (0U)
    );

  /* Configure MAC Rx Flow control*/
  /* Read MAC Rx Flow control Register */
  val = osi_readla (
          osi_core,
          (nveu8_t *)addr + MGBE_MAC_RX_FLW_CTRL
          );

  /* flw_ctrl BIT1: 1 is for rx flow ctrl enable
   * flw_ctrl BIT1: 0 is for rx flow ctrl disable
   */
  if ((flw_ctrl & OSI_FLOW_CTRL_RX) == OSI_FLOW_CTRL_RX) {
    /* Enable Rx Flow Control */
    val |= MGBE_MAC_RX_FLW_CTRL_RFE;
  } else {
    /* Disable Rx Flow Control */
    val &= ~MGBE_MAC_RX_FLW_CTRL_RFE;
  }

  /* Write to MAC Rx Flow control Register */
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)addr + MGBE_MAC_RX_FLW_CTRL
    );

  return 0;
}

#endif /* !OSI_STRIPPED_LIB */

#ifdef HSI_SUPPORT

/**
 * @brief pcs_configure_fsm - Configure FSM for XPCS/XLGPCS
 *
 * @note
 * Algorithm: enable/disable the FSM timeout safety feature
 *
 * @param[in, out] osi_core: OSI core private data structure.
 * @param[in] enable: OSI_ENABLE for Enabling FSM timeout safety feature, else disable
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t
pcs_configure_fsm (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable
  )
{
  nve32_t   ret           = 0;
  nveu32_t  xpcs_sfty_val = (enable == OSI_ENABLE) ?
                            XPCS_SFTY_ENABLE_VAL : XPCS_SFTY_DISABLE_VAL;
  nveu32_t  xlgpcs_sfty_val = (enable == OSI_ENABLE) ?
                              XLGPCS_SFTY_ENABLE_VAL : XLGPCS_SFTY_DISABLE_VAL;

  /* Enable/Disable FSM time-out safety mechanism inside XPCS */
  ret = xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_SFTY_DISABLE_0, xpcs_sfty_val);
  if (ret != 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "XPCS safety register write failure\n",
      0ULL
      );
    goto fail;
  }

  /* Applicable only for 25G */
  if (osi_core->uphy_gbe_mode == OSI_GBE_MODE_25G) {
    /* Enabling/Disabling FT_DIS/FP_DIS/DPP_DIS/ECC_DIS/CSRP_DIS/IFT_DIS in XLGPCS */
    ret = xpcs_write_safety (osi_core, XLGPCS_VR_XS_PCS_SFTY_DISABLE_0, xlgpcs_sfty_val);
    if (ret != 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "XLGPCS safety register write failure\n",
        0ULL
        );
      goto fail;
    }
  }

fail:
  return ret;
}

/**
 * @brief mgbe_hsi_configure - Configure HSI
 *
 * @note
 * Algorithm: enable LIC interrupt and HSI features
 *
 * @param[in, out] osi_core: OSI core private data structure.
 * @param[in] enable: OSI_ENABLE for Enabling HSI feature, else disable
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t
mgbe_hsi_configure (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable
  )
{
  nveu32_t        value                                    = 0U;
  nve32_t         ret                                      = 0;
  const nveu32_t  xpcs_intr_ctrl_reg[OSI_MAX_MAC_IP_TYPES] = {
    0,
    XPCS_WRAP_INTERRUPT_CONTROL,
    T26X_XPCS_WRAP_INTERRUPT_CONTROL
  };
  const nveu32_t  intr_en[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_WRAP_COMMON_INTR_ENABLE,
    MGBE_T26X_WRAP_COMMON_INTR_ENABLE
  };

  if (enable == OSI_ENABLE) {
    /* T23X-MGBE_HSIv2-12:Initialization of Transaction Timeout in PCS */
    /* T23X-MGBE_HSIv2-11:Initialization of Watchdog Timer */
    value  = (0xCCU << XPCS_SFTY_1US_MULT_SHIFT) & XPCS_SFTY_1US_MULT_MASK;
    value |= ((nveu32_t)0x01U << XPCS_FSM_TO_SEL_SHIFT) & XPCS_FSM_TO_SEL_MASK;
    value |= XPCS_VR_XS_PCS_SFTY_TMR_CTRL_IFT_SEL;
    ret    = xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_SFTY_TMR_CTRL, value);
    if (ret != 0) {
      goto fail;
    }

    /* Below setting is applicable only for 25G in XLGPCS */
    if (osi_core->uphy_gbe_mode == OSI_GBE_MODE_25G) {
      /* T264-MGBE_HSIv2-59     Initialization of Transaction Timeout in XLGPCS */
      /* T264-MGBE_HSIv2-60     Initialization of Watchdog Timer for XLGPCS FSM States */
      value  = (0xCBU << XPCS_SFTY_1US_MULT_SHIFT) & XPCS_SFTY_1US_MULT_MASK;
      value |= ((nveu32_t)0x01U << XPCS_FSM_TO_SEL_SHIFT) & XPCS_FSM_TO_SEL_MASK;
      /* IFT_SEL field same as */
      value |= XPCS_VR_XS_PCS_SFTY_TMR_CTRL_IFT_SEL;
      ret    = xpcs_write_safety (osi_core, XLGPCS_VR_PCS_SFTY_TMR_CTRL, value);
      if (ret != 0) {
        goto fail;
      }
    }

    /* T23X-MGBE_HSIv2-38: Initialization of Register Parity for control registers */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MGBE_MAC_SCSR_CONTROL
              );
    value |= MGBE_CPEN;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_MAC_SCSR_CONTROL
      );

    /* For T26x CE/UCE are not handled by SW driver,since they are directly
     * reported to FSI through HSM , so not enabling it
     */
    if (osi_core->mac != OSI_MAC_HW_MGBE_T26X) {
      /* T23X-MGBE_HSIv2-1 Configure ECC */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + MGBE_MTL_ECC_CONTROL
                );
      value &= ~MGBE_MTL_ECC_MTXED;
      value &= ~MGBE_MTL_ECC_MRXED;
      value &= ~MGBE_MTL_ECC_MGCLED;
      value &= ~MGBE_MTL_ECC_MRXPED;
      value &= ~MGBE_MTL_ECC_TSOED;
      value &= ~MGBE_MTL_ECC_DESCED;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + MGBE_MTL_ECC_CONTROL
        );
      /* Enable Interrupt */
      /*  T23X-MGBE_HSIv2-1: Enabling of Memory ECC */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + MGBE_MTL_ECC_INTERRUPT_ENABLE
                );
      value |= MGBE_MTL_TXCEIE;
      value |= MGBE_MTL_RXCEIE;
      value |= MGBE_MTL_GCEIE;
      value |= MGBE_MTL_RPCEIE;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + MGBE_MTL_ECC_INTERRUPT_ENABLE
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + MGBE_DMA_ECC_INTERRUPT_ENABLE
                );
      value |= MGBE_DMA_TCEIE;
      value |= MGBE_DMA_DCEIE;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + MGBE_DMA_ECC_INTERRUPT_ENABLE
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                intr_en[osi_core->mac]
                );
      value |= MGBE_REGISTER_PARITY_ERR;
      value |= MGBE_CORE_CORRECTABLE_ERR;
      value |= MGBE_CORE_UNCORRECTABLE_ERR;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        intr_en[osi_core->mac]
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->xpcs_base +
                xpcs_intr_ctrl_reg[osi_core->mac]
                );
      value |= XPCS_CORE_CORRECTABLE_ERR;
      value |= XPCS_CORE_UNCORRECTABLE_ERR;
      value |= XPCS_REGISTER_PARITY_ERR;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->xpcs_base +
        xpcs_intr_ctrl_reg[osi_core->mac]
        );

      /* T23X-MGBE_HSIv2-2: Enabling of Bus Parity */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + MGBE_MTL_DPP_CONTROL
                );
      value &= ~MGBE_DDPP;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + MGBE_MTL_DPP_CONTROL
        );
    }

    /* T23X-MGBE_HSIv2-5: Enabling and Initialization of Transaction Timeout  */
    value  = (0x198U << MGBE_TMR_SHIFT) & MGBE_TMR_MASK;
    value |= ((nveu32_t)0x0U << MGBE_CTMR_SHIFT) & MGBE_CTMR_MASK;

    /** Set NTMRMD and LTMRMD to 16ms(0x3) as per hardware team's
     * guidelines specified bug 3584387 and 4502985.
     */
    value |= ((nveu32_t)0x3U << MGBE_LTMRMD_SHIFT) & MGBE_LTMRMD_MASK;
    value |= ((nveu32_t)0x3U << MGBE_NTMRMD_SHIFT) & MGBE_NTMRMD_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_DWCXG_CORE_MAC_FSM_ACT_TIMER
      );

    /** Deactivate below TX/RX FSMs as per the HW guidelines specified
     * in bug 4502985 during the link down state:
     * SNPS_SCS_REG1[0] for RPERXLPI, RXLPI-GMII, RARP
     * SNPS_SCS_REG1[16] for TRC
     */
    value = (MGBE_SNPS_SCS_REG1_TRCFSM | MGBE_SNPS_SCS_REG1_RPERXLPIFSM);
    osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_SNPS_SCS_REG1);

    /* T23X-MGBE_HSIv2-3: Enabling and Initialization of Watchdog Timer */
    /* T23X-MGBE_HSIv2-4: Enabling of Consistency Monitor for XGMAC FSM State */
    value = MGBE_PRTYEN | MGBE_TMOUTEN;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_MAC_FSM_CONTROL
      );

    /* T23X-MGBE_HSIv2-20: Enabling of error reporting for Inbound Bus CRC errors */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MGBE_MMC_RX_INTR_EN
              );
    value |= MGBE_RXCRCERPIE;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_MMC_RX_INTR_EN
      );

    ret = pcs_configure_fsm (osi_core, OSI_ENABLE);
  } else {
    /* T23X-MGBE_HSIv2-11:Deinitialization of Watchdog Timer */
    ret = xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_SFTY_TMR_CTRL, 0);
    if (ret != 0) {
      goto fail;
    }

    if (osi_core->mac != OSI_MAC_HW_MGBE_T26X) {
      /* T23X-MGBE_HSIv2-1 Disable ECC */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + MGBE_MTL_ECC_CONTROL
                );
      value |= MGBE_MTL_ECC_MTXED;
      value |= MGBE_MTL_ECC_MRXED;
      value |= MGBE_MTL_ECC_MGCLED;
      value |= MGBE_MTL_ECC_MRXPED;
      value |= MGBE_MTL_ECC_TSOED;
      value |= MGBE_MTL_ECC_DESCED;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + MGBE_MTL_ECC_CONTROL
        );
      /* Disable Interrupts */
      osi_writela (
        osi_core,
        0,
        (nveu8_t *)osi_core->base + MGBE_MTL_ECC_INTERRUPT_ENABLE
        );

      osi_writela (
        osi_core,
        0,
        (nveu8_t *)osi_core->base + MGBE_DMA_ECC_INTERRUPT_ENABLE
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                intr_en[osi_core->mac]
                );
      value &= ~MGBE_REGISTER_PARITY_ERR;
      value &= ~MGBE_CORE_CORRECTABLE_ERR;
      value &= ~MGBE_CORE_UNCORRECTABLE_ERR;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        intr_en[osi_core->mac]
        );

      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->xpcs_base +
                xpcs_intr_ctrl_reg[osi_core->mac]
                );
      value &= ~XPCS_CORE_CORRECTABLE_ERR;
      value &= ~XPCS_CORE_UNCORRECTABLE_ERR;
      value &= ~XPCS_REGISTER_PARITY_ERR;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->xpcs_base +
        xpcs_intr_ctrl_reg[osi_core->mac]
        );

      /* T23X-MGBE_HSIv2-2: Disable of Bus Parity */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base + MGBE_MTL_DPP_CONTROL
                );
      value |=  MGBE_DDPP;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base + MGBE_MTL_DPP_CONTROL
        );
    }

    /* T23X-MGBE_HSIv2-38: Disable Register Parity for control registers */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MGBE_MAC_SCSR_CONTROL
              );
    value &= ~MGBE_CPEN;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_MAC_SCSR_CONTROL
      );

    /* T23X-MGBE_HSIv2-5: Disabling and DeInitialization of Transaction Timeout  */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MGBE_DWCXG_CORE_MAC_FSM_ACT_TIMER
              );
    value &= ~(MGBE_TMR_MASK | MGBE_CTMR_MASK | MGBE_LTMRMD_MASK | MGBE_NTMRMD_MASK);
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_DWCXG_CORE_MAC_FSM_ACT_TIMER
      );

    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MGBE_MAC_FSM_CONTROL
              );
    value &= ~MGBE_PRTYEN;
    value &= ~MGBE_TMOUTEN;
    /* T23X-MGBE_HSIv2-4: Disabling of Consistency Monitor for XGMAC FSM State */
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_MAC_FSM_CONTROL
      );

    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MGBE_MMC_RX_INTR_EN
              );
    value &= ~MGBE_RXCRCERPIE;
    /* T23X-MGBE_HSIv2-20: Disabling of error reporting for Inbound Bus CRC errors */
    osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_MMC_RX_INTR_EN);

    ret = pcs_configure_fsm (osi_core, OSI_DISABLE);
  }

fail:
  return ret;
}

  #ifdef NV_VLTEST_BUILD

/**
 * @brief mgbe_hsi_inject_err - Inject error
 *
 * @note
 * Algorithm: Use error injection method to induce error
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] error_code: HSI Error code
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t
mgbe_hsi_inject_err (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    error_code
  )
{
  const nveu32_t  val_ce = (MGBE_MTL_DEBUG_CONTROL_FDBGEN |
                            MGBE_MTL_DEBUG_CONTROL_DBGMOD |
                            MGBE_MTL_DEBUG_CONTROL_FIFORDEN |
                            MGBE_MTL_DEBUG_CONTROL_EIEE |
                            MGBE_MTL_DEBUG_CONTROL_EIEC);

  const nveu32_t  val_ue = (MGBE_MTL_DEBUG_CONTROL_FDBGEN |
                            MGBE_MTL_DEBUG_CONTROL_DBGMOD |
                            MGBE_MTL_DEBUG_CONTROL_FIFORDEN |
                            MGBE_MTL_DEBUG_CONTROL_EIEE);
  nve32_t  ret = 0;

  switch (error_code) {
    case OSI_CORRECTABLE_ERR:
      osi_writela (
        osi_core,
        val_ce,
        (nveu8_t *)osi_core->base +
        MGBE_MTL_DEBUG_CONTROL
        );
      break;
    case OSI_UNCORRECTABLE_ERR:
      osi_writela (
        osi_core,
        val_ue,
        (nveu8_t *)osi_core->base +
        MGBE_MTL_DEBUG_CONTROL
        );
      break;
    default:
      ret = hsi_common_error_inject (osi_core, error_code);
      break;
  }

  return ret;
}

  #endif
#endif

/**
 * @brief mgbe_configure_mac - Configure MAC
 *
 * @note
 * Algorithm: This takes care of configuring the  below
 *      parameters for the MAC
 *      1) Programming the MAC address
 *      2) Enable required MAC control fields in MCR
 *      3) Enable Multicast and Broadcast Queue
 *      4) Disable MMC nve32_terrupts and Configure the MMC counters
 *      5) Enable required MAC nve32_terrupts
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static void
mgbe_configure_mac (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t        value = 0U, max_queue = 0U, i = 0U;
  const nveu32_t  intr_en[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_WRAP_COMMON_INTR_ENABLE,
    MGBE_T26X_WRAP_COMMON_INTR_ENABLE
  };

  /* TODO: Need to check if we need to enable anything in Tx configuration
   * value = osi_readla(osi_core,
                        (nveu8_t *)osi_core->base + MGBE_MAC_TMCR);
   */
  /* Read MAC Rx Configuration Register */
  value = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_RMCR);
  /* Enable Automatic Pad or CRC Stripping */
  /* Enable CRC stripping for Type packets */
  /* Enable Rx checksum offload engine by default */
  value |= MGBE_MAC_RMCR_ACS | MGBE_MAC_RMCR_CST | MGBE_MAC_RMCR_IPC;

  /* Jumbo Packet Enable */
  if ((osi_core->mtu > OSI_DFLT_MTU_SIZE) &&
      (osi_core->mtu <= OSI_MTU_SIZE_9000))
  {
    value |= MGBE_MAC_RMCR_JE;
  } else if (osi_core->mtu > OSI_MTU_SIZE_9000) {
    /* if MTU greater 9K use GPSLCE */
    value |= MGBE_MAC_RMCR_GPSLCE | MGBE_MAC_RMCR_WD;
    value &= ~MGBE_MAC_RMCR_GPSL_MSK;
    value |=  ((((nveu32_t)OSI_MAX_MTU_SIZE) << 16U) & MGBE_MAC_RMCR_GPSL_MSK);
  } else {
    value &= ~MGBE_MAC_RMCR_JE;
    value &= ~MGBE_MAC_RMCR_GPSLCE;
    value &= ~MGBE_MAC_RMCR_WD;
  }

  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + MGBE_MAC_RMCR
    );

  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + MGBE_MAC_TMCR
            );
  /* DDIC bit set is needed to improve MACSEC Tput */
  value |= MGBE_MAC_TMCR_DDIC;
  /* Jabber Disable */
  if (osi_core->mtu > OSI_DFLT_MTU_SIZE) {
    value |= MGBE_MAC_TMCR_JD;
  }

  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + MGBE_MAC_TMCR
    );

  /* Enable Multicast and Broadcast Queue */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + MGBE_MAC_RQC1R
            );
  value |= MGBE_MAC_RQC1R_MCBCQEN;
  /* Set MCBCQ to highest enabled RX queue index */
  for (i = 0; i < osi_core->num_mtl_queues; i++) {
    if ((max_queue < osi_core->mtl_queues[i]) &&
        (osi_core->mtl_queues[i] < OSI_MGBE_MAX_NUM_QUEUES))
    {
      /* Update max queue number */
      max_queue = osi_core->mtl_queues[i];
    }
  }

  value &= ~(MGBE_MAC_RQC1R_MCBCQ);
  value |= (max_queue << MGBE_MAC_RQC1R_MCBCQ_SHIFT);
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + MGBE_MAC_RQC1R
    );

  /* Disable all MMC nve32_terrupts */
  /* Disable all MMC Tx nve32_terrupts */
  osi_writela (
    osi_core,
    OSI_NONE,
    (nveu8_t *)osi_core->base +
    MGBE_MMC_TX_INTR_EN
    );

  /* Configure MMC counters */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + MGBE_MMC_CNTRL
            );
  value |= MGBE_MMC_CNTRL_CNTRST | MGBE_MMC_CNTRL_RSTONRD |
           MGBE_MMC_CNTRL_CNTMCT | MGBE_MMC_CNTRL_CNTPRST;
  if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
    value |= MGBE_MMC_CNTRL_DRCHM;
  }

  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + MGBE_MMC_CNTRL
    );

  /* Enable MAC nve32_terrupts */
  /* Read MAC IMR Register */
  value = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
  /* RGSMIIIM - RGMII/SMII interrupt and TSIE Enable */
  /* TXESIE - Transmit Error Status Interrupt Enable */
  /* TODO: LPI need to be enabled during EEE implementation */
 #ifndef OSI_STRIPPED_LIB
  value |= (MGBE_IMR_TXESIE);
 #endif /* !OSI_STRIPPED_LIB */
  /* Clear link status interrupt and enable after lane bring up done */
  value &= ~MGBE_IMR_RGSMIIIE;
  value |= MGBE_IMR_TSIE;
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_MAC_IER);

  /* Mask the mmc counters interrupts */
  value = MGBE_MMC_IPC_RX_INT_MASK_VALUE;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MMC_IPC_RX_INT_MASK
    );

  /* Enable common interrupt at wrapper level */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            intr_en[osi_core->mac]
            );
  value |= MGBE_MAC_SBD_INTR;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    intr_en[osi_core->mac]
    );

  if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
    /* configure L3L4 filter index to be 48 in Rx desc2 */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG
              );
    value &= ~MGBE_MAC_PFR_DHLFRS_MASK;
    value |= MGBE_MAC_PFR_DHLFRS;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG
      );
  }

  /* Enable VLAN configuration */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + MGBE_MAC_VLAN_TR
            );

  /* Enable VLAN Tag in RX Status
   * Disable double VLAN Tag processing on TX and RX
   */
 #ifndef OSI_STRIPPED_LIB
  if (osi_core->strip_vlan_tag == OSI_ENABLE) {
    /* Enable VLAN Tag stripping always */
    value |= MGBE_MAC_VLANTR_EVLS_ALWAYS_STRIP;
  }

 #endif /* !OSI_STRIPPED_LIB */
  value |= MGBE_MAC_VLANTR_EVLRXS | MGBE_MAC_VLANTR_DOVLTC;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + MGBE_MAC_VLAN_TR
    );

  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + MGBE_MAC_VLANTIR
            );
  /* Enable VLAN tagging through context descriptor */
  value |= MGBE_MAC_VLANTIR_VLTI;
  /* insert/replace C_VLAN in 13th & 14th bytes of transmitted frames */
  value &= ~MGBE_MAC_VLANTIRR_CSVL;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + MGBE_MAC_VLANTIR
    );
}

/**
 * @brief mgbe_dma_ind_config - Configures the DMA indirect registers
 *
 * @note
 * Algorithm: Write to Indirect DMA registers
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] mode: Indirect DMA register to write.
 * @param[in] chan: Indirect DMA channel register offset.
 * @param[in] value: Data to be written to DMA indirect register
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_dma_indir_addr_write (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   mode,
  nveu32_t                   chan,
  nveu32_t                   value
  )
{
  nveu8_t   *addr = (nveu8_t *)osi_core->base;
  nveu32_t  ctrl  = 0;
  nve32_t   ret   = 0;
  nveu32_t  val   = 0U;

  /* Write data to indirect register */
  osi_writela (osi_core, value, addr + MGBE_DMA_INDIR_DATA);
  ctrl |= (mode << MGBE_DMA_INDIR_CTRL_MSEL_SHIFT) &
          MGBE_DMA_INDIR_CTRL_MSEL_MASK;
  ctrl |= (chan << MGBE_DMA_INDIR_CTRL_AOFF_SHIFT) &
          MGBE_DMA_INDIR_CTRL_AOFF_MASK;
  ctrl |= MGBE_DMA_INDIR_CTRL_OB;
  ctrl &= ~MGBE_DMA_INDIR_CTRL_CT;
  /* Write cmd to indirect control register */
  osi_writela (osi_core, ctrl, addr + MGBE_DMA_INDIR_CTRL);
  /* poll for write operation to complete */
  ret = poll_check (
          osi_core,
          addr + MGBE_DMA_INDIR_CTRL,
          MGBE_DMA_INDIR_CTRL_OB,
          &val
          );
  if (ret == -1) {
    goto done;
  }

done:
  return ret;
}

/**
 * @brief mgbe_configure_pdma - Configure PDMA parameters and TC mapping
 *
 * @note
 * Algorithm:
 *      1) Program Tx/Rx PDMA PBL, ORR, OWR parameters
 *      2) Program PDMA to TC mapping for Tx and Rx
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_configure_pdma (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t        value = 0;
  nve32_t         ret = 0;
  nveu32_t        pbl = 0;
  nveu32_t        i, j, pdma_chan, vdma_chan;
  const nveu32_t  tx_orr = (MGBE_DMA_CHX_TX_CNTRL2_ORRQ_RECOMMENDED /
                            osi_core->num_of_pdma);
  const nveu32_t  rx_owrq = (MGBE_DMA_CHX_RX_CNTRL2_OWRQ_MCHAN /
                             osi_core->num_of_pdma);
  nveu32_t  tx_pbl = 0U, max_txq_size = 0U, adjusted_txq_size = 0U;
  nveu32_t  divided_txq_size = 0U;
  /* Total Rx Queue size is 256KB */
  const nveu32_t  rx_pbl[OSI_MGBE_MAX_NUM_QUEUES] = {
    Q_SZ_DEPTH (224U) / 2U, Q_SZ_DEPTH (2U) / 2U, Q_SZ_DEPTH (2U) / 2U,
    Q_SZ_DEPTH (2U) / 2U,   Q_SZ_DEPTH (2U) / 2U, Q_SZ_DEPTH (2U) / 2U,
    Q_SZ_DEPTH (2U) / 2U,   Q_SZ_DEPTH (2U) / 2U, Q_SZ_DEPTH (2U) / 2U,
    Q_SZ_DEPTH (16U) / 2U
  };
  nveu32_t        tx_pbl_ufpga = 0U;
  /* uFPGA Rx Queue size is 64KB */
  const nveu32_t  rx_pbl_ufpga[OSI_MGBE_MAX_NUM_QUEUES] = {
    Q_SZ_DEPTH (40U)/2U, Q_SZ_DEPTH (2U)/2U, Q_SZ_DEPTH (2U)/2U,
    Q_SZ_DEPTH (2U)/2U,  Q_SZ_DEPTH (2U)/2U, Q_SZ_DEPTH (2U),
    Q_SZ_DEPTH (2U)/2U,  Q_SZ_DEPTH (2U)/2U, Q_SZ_DEPTH (2U)/2U,
    Q_SZ_DEPTH (8U)/2U
  };

  for (i = 0; i < osi_core->num_of_pdma; i++) {
    pdma_chan = osi_core->pdma_data[i].pdma_chan;
    /* Update PDMA_CH(#i)_TxExtCfg register */
    value  = (tx_orr << MGBE_PDMA_CHX_TXRX_EXTCFG_ORRQ_SHIFT);
    value |= (pdma_chan << MGBE_PDMA_CHX_TXRX_EXTCFG_P2TCMP_SHIFT) &
             MGBE_PDMA_CHX_TXRX_EXTCFG_P2TCMP_MASK;
    value |= MGBE_PDMA_CHX_TXRX_EXTCFG_PBLX8;

    /*
     * Formula for TxPBL calculation is
     * (TxPBL) < ((TXQSize - MTU)/(DATAWIDTH/8)) - 5
     * if TxPBL exceeds the value of 256 then we need to make use of 256
     * as the TxPBL else we should be using the value whcih we get after
     * calculation by using above formula
     */
    if (osi_core->pre_sil == OSI_ENABLE) {
      max_txq_size = MGBE_TXQ_SIZE_UFPGA / OSI_MGBE_MAX_NUM_QUEUES;
      if (osi_core->mtu > max_txq_size) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "Invalid MTU related to Q size received in pre-sil case\n",
          osi_core->mtu
          );
        ret = -1;
        goto done;
      }

      adjusted_txq_size = max_txq_size - osi_core->mtu;
      divided_txq_size  = adjusted_txq_size / (MGBE_AXI_DATAWIDTH / 8U);
      if (divided_txq_size < 5U) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "Invalid MTU received in pre-sil case\n",
          osi_core->mtu
          );
        ret = -1;
        goto done;
      }

      tx_pbl_ufpga =
        ((((MGBE_TXQ_SIZE_UFPGA / OSI_MGBE_MAX_NUM_QUEUES) -
           osi_core->mtu) / (MGBE_AXI_DATAWIDTH / 8U)) - 5U);
      pbl    = osi_valid_pbl_value (tx_pbl_ufpga);
      value |= (pbl << MGBE_PDMA_CHX_EXTCFG_PBL_SHIFT);
    } else {
      max_txq_size = MGBE_TXQ_SIZE / OSI_MGBE_MAX_NUM_QUEUES;
      if (osi_core->mtu > max_txq_size) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "Invalid MTU related to Q size received in silicon case\n",
          osi_core->mtu
          );
        ret = -1;
        goto done;
      }

      adjusted_txq_size = max_txq_size - osi_core->mtu;
      divided_txq_size  = adjusted_txq_size / (MGBE_AXI_DATAWIDTH / 8U);
      if (divided_txq_size < 5U) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "Invalid MTU received in silicon case\n",
          osi_core->mtu
          );
        ret = -1;
        goto done;
      }

      tx_pbl =
        ((((MGBE_TXQ_SIZE / OSI_MGBE_MAX_NUM_QUEUES) -
           osi_core->mtu) / (MGBE_AXI_DATAWIDTH / 8U)) - 5U);
      pbl    = osi_valid_pbl_value (tx_pbl);
      value |= (pbl << MGBE_PDMA_CHX_EXTCFG_PBL_SHIFT);
    }

    ret = mgbe_dma_indir_addr_write (
            osi_core,
            MGBE_PDMA_CHX_TX_EXTCFG,
            pdma_chan,
            value
            );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "MGBE_PDMA_CHX_TX_EXTCFG failed\n",
        0ULL
        );
      goto done;
    }

    /* Update PDMA_CH(#i)_RxExtCfg register */
    value  = (rx_owrq << MGBE_PDMA_CHX_TXRX_EXTCFG_ORRQ_SHIFT);
    value |= (pdma_chan << MGBE_PDMA_CHX_TXRX_EXTCFG_P2TCMP_SHIFT) &
             MGBE_PDMA_CHX_TXRX_EXTCFG_P2TCMP_MASK;
    value |= MGBE_PDMA_CHX_TXRX_EXTCFG_PBLX8;

    if (osi_core->pre_sil == OSI_ENABLE) {
      pbl    = osi_valid_pbl_value (rx_pbl_ufpga[i]);
      value |= (pbl << MGBE_PDMA_CHX_EXTCFG_PBL_SHIFT);
    } else {
      pbl    = osi_valid_pbl_value (rx_pbl[i]);
      value |= (pbl << MGBE_PDMA_CHX_EXTCFG_PBL_SHIFT);
    }

    value |= MGBE_PDMA_CHX_RX_EXTCFG_RXPEN;
    ret    = mgbe_dma_indir_addr_write (
               osi_core,
               MGBE_PDMA_CHX_RX_EXTCFG,
               pdma_chan,
               value
               );
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "MGBE_PDMA_CHX_RX_EXTCFG failed\n",
        0ULL
        );
      goto done;
    }

    /* program the vdma's descriptor cache size and
     * pre-fetch threshold */
    for (j = 0; j < osi_core->pdma_data[i].num_vdma_chans; j++) {
      vdma_chan = osi_core->pdma_data[i].vdma_chans[j];
      if (osi_core->pre_sil == OSI_ENABLE) {
        value = MGBE_VDMA_CHX_TXRX_DESC_CTRL_DCSZ_UFPGA &
                MGBE_VDMA_CHX_TXRX_DESC_CTRL_DCSZ_MASK;
      } else {
        value = MGBE_VDMA_CHX_TXRX_DESC_CTRL_DCSZ &
                MGBE_VDMA_CHX_TXRX_DESC_CTRL_DCSZ_MASK;
      }

      value |= (MGBE_VDMA_CHX_TXRX_DESC_CTRL_DPS <<
                MGBE_VDMA_CHX_TXRX_DESC_CTRL_DPS_SHIFT) &
               MGBE_VDMA_CHX_TXRX_DESC_CTRL_DPS_MASK;
      ret = mgbe_dma_indir_addr_write (
              osi_core,
              MGBE_VDMA_CHX_TX_DESC_CTRL,
              vdma_chan,
              value
              );
      if (ret < 0) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "MGBE_VDMA_CHX_TX_DESC_CTRL failed\n",
          0ULL
          );
        goto done;
      }

      ret = mgbe_dma_indir_addr_write (
              osi_core,
              MGBE_VDMA_CHX_RX_DESC_CTRL,
              vdma_chan,
              value
              );
      if (ret < 0) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "MGBE_VDMA_CHX_RX_DESC_CTRL failed\n",
          0ULL
          );
        goto done;
      }
    }
  }

  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base + MGBE_DMA_MODE
            );
  /* set DMA_Mode register DSCB bit */
  value |= MGBE_DMA_MODE_DSCB;
  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_DMA_MODE);
  /* poll for Tx/Rx Dcache calculations complete and fixed */
  ret = poll_check (
          osi_core,
          (nveu8_t *)osi_core->base + MGBE_DMA_MODE,
          MGBE_DMA_MODE_DSCB,
          &value
          );
  if (ret == -1) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "MGBE_DMA_MODE_DSCB timeout\n",
      0ULL
      );
  }

done:
  return ret;
}

/**
 * @brief mgbe_configure_dma - Configure DMA
 *
 * @note
 * Algorithm: This takes care of configuring the  below
 *      parameters for the DMA
 *      1) Programming different burst length for the DMA
 *      2) Enable enhanced Address mode
 *      3) Programming max read outstanding request limit
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_configure_dma (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t  value = 0;
  nve32_t   ret   = 0;

  /* Set AXI Undefined Burst Length */
  value |= MGBE_DMA_SBUS_UNDEF;
  /* AXI Burst Length 256*/
  value |= MGBE_DMA_SBUS_BLEN256;
  /* Enhanced Address Mode Enable */
  value |= MGBE_DMA_SBUS_EAME;
  /* AXI Maximum Read Outstanding Request Limit = 63 */
  value |= MGBE_DMA_SBUS_RD_OSR_LMT;
  /* AXI Maximum Write Outstanding Request Limit = 63 */
  value |= MGBE_DMA_SBUS_WR_OSR_LMT;

  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base + MGBE_DMA_SBUS
    );
  if (osi_core->mac == OSI_MAC_HW_MGBE) {
    /* Configure TDPS to 5 */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MGBE_DMA_TX_EDMA_CTRL
              );
    value |= MGBE_DMA_TX_EDMA_CTRL_TDPS;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_DMA_TX_EDMA_CTRL
      );

    /* Configure RDPS to 5 */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base + MGBE_DMA_RX_EDMA_CTRL
              );
    value |= MGBE_DMA_RX_EDMA_CTRL_RDPS;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base + MGBE_DMA_RX_EDMA_CTRL
      );
  }

  /* configure MGBE PDMA */
  if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
    ret = mgbe_configure_pdma (osi_core);
    if (ret < 0) {
      goto done;
    }
  }

done:
  return ret;
}

/**
 * @brief Map DMA channels to a specific VM IRQ.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note OSD layer needs to update number of VM channels and
 *      DMA channel list in osi_vm_irq_data.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_dma_chan_to_vmirq_map (
  struct osi_core_priv_data  *osi_core
  )
{
 #ifndef OSI_STRIPPED_LIB
  nveu32_t  sid[3][4] = {
    { 0U,             0U,             0U,             0U             },
    { MGBE0_SID,      MGBE1_SID,      MGBE2_SID,      MGBE3_SID      },
    { MGBE0_SID_T264, MGBE1_SID_T264, MGBE2_SID_T264, MGBE3_SID_T264 }
  };
 #endif
  struct core_local       *l_core = (struct core_local *)(void *)osi_core;
  struct osi_vm_irq_data  *irq_data;
  nve32_t                 ret = 0;
  nveu32_t                i, j;
  nveu32_t                chan;

  for (i = 0; i < osi_core->num_vm_irqs; i++) {
    irq_data = &osi_core->irq_data[i];

    for (j = 0; j < irq_data->num_vm_chans; j++) {
      chan = irq_data->vm_chans[j];

      if (chan >= l_core->num_max_chans) {
        OSI_CORE_ERR (
          osi_core->osd,
          OSI_LOG_ARG_HW_FAIL,
          "Invalid channel number\n",
          chan
          );
        ret = -1;
        goto exit;
      }

      osi_writel (
        OSI_BIT (irq_data->vm_num),
        (nveu8_t *)osi_core->base +
        MGBE_VIRT_INTR_APB_CHX_CNTRL (chan)
        );
    }

    osi_writel (
      OSI_BIT (irq_data->vm_num),
      (nveu8_t *)osi_core->base + MGBE_VIRTUAL_APB_ERR_CTRL
      );
  }

 #ifndef OSI_STRIPPED_LIB
  if ((osi_core->use_virtualization == OSI_DISABLE) &&
      (osi_core->hv_base != OSI_NULL))
  {
    if (osi_core->instance_id > 3U) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "Wrong MAC instance-ID\n",
        osi_core->instance_id
        );
      ret = -1;
      goto exit;
    }

    osi_writela (
      osi_core,
      MGBE_SID_VAL1 (sid[osi_core->mac][osi_core->instance_id]),
      (nveu8_t *)osi_core->hv_base + MGBE_WRAP_AXI_ASID0_CTRL
      );

    osi_writela (
      osi_core,
      MGBE_SID_VAL1 (sid[osi_core->mac][osi_core->instance_id]),
      (nveu8_t *)osi_core->hv_base + MGBE_WRAP_AXI_ASID1_CTRL
      );

    osi_writela (
      osi_core,
      MGBE_SID_VAL2 (sid[osi_core->mac][osi_core->instance_id]),
      (nveu8_t *)osi_core->hv_base + MGBE_WRAP_AXI_ASID2_CTRL
      );
  }

 #endif

exit:
  return ret;
}

/**
 * @brief mgbe_core_init - MGBE MAC, MTL and common DMA Initialization
 *
 * @note
 * Algorithm: This function will take care of initializing MAC, MTL and
 *      common DMA registers.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note 1) MAC should be out of reset. See osi_poll_for_swr() for details.
 *       2) osi_core->base needs to be filled based on ioremap.
 *       3) osi_core->num_mtl_queues needs to be filled.
 *       4) osi_core->mtl_queues[qinx] need to be filled.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_core_init (
  struct osi_core_priv_data *const  osi_core
  )
{
  nve32_t   ret   = 0;
  nveu32_t  qinx  = 0;
  nveu32_t  value = 0;

  /* reset mmc counters */
  osi_writela (
    osi_core,
    MGBE_MMC_CNTRL_CNTRST,
    (nveu8_t *)osi_core->base +
    MGBE_MMC_CNTRL
    );

  /* Mapping MTL Rx queue and DMA Rx channel */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_RXQ_DMA_MAP0
            );
  value |= MGBE_RXQ_TO_DMA_CHAN_MAP0;
  value |= MGBE_RXQ_TO_DMA_MAP_DDMACH;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_RXQ_DMA_MAP0
    );

  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_RXQ_DMA_MAP1
            );
  value |= MGBE_RXQ_TO_DMA_CHAN_MAP1;
  value |= MGBE_RXQ_TO_DMA_MAP_DDMACH;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_RXQ_DMA_MAP1
    );

  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_RXQ_DMA_MAP2
            );
  value |= MGBE_RXQ_TO_DMA_CHAN_MAP2;
  value |= MGBE_RXQ_TO_DMA_MAP_DDMACH;
  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_RXQ_DMA_MAP2
    );
  /* T264 DDS bit moved */
  if (osi_core->mac != OSI_MAC_HW_MGBE_T26X) {
    /* Enable DDS in MAC_Extended_Configuration */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MAC_EXT_CNF
              );
    value |= MGBE_MAC_EXT_CNF_DDS;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MAC_EXT_CNF
      );
  }

  /* Configure MTL Queues */
  /* TODO: Iterate over Number MTL queues need to be removed */
  for (qinx = 0; qinx < osi_core->num_mtl_queues; qinx++) {
    ret = mgbe_configure_mtl_queue (osi_core, osi_core->mtl_queues[qinx]);
    if (ret < 0) {
      goto fail;
    }

    /* Enable by default to configure forward error packets.
     * Since this is a local function this will always return sucess,
     * so no need to check for return value
     */
 #ifndef OSI_STRIPPED_LIB
    ret = hw_config_fw_err_pkts (osi_core, osi_core->mtl_queues[qinx], OSI_ENABLE);
    if (ret < 0) {
      goto fail;
    }

 #else
    (void)hw_config_fw_err_pkts (osi_core, osi_core->mtl_queues[qinx], OSI_ENABLE);
 #endif /* !OSI_STRIPPED_LIB */
  }

  /* configure MGBE MAC HW */
  mgbe_configure_mac (osi_core);

  /* configure MGBE DMA */
  ret = mgbe_configure_dma (osi_core);
  if (ret < 0) {
    goto fail;
  }

  /* tsn initialization */
  hw_tsn_init (osi_core);

 #if !defined (L3L4_WILDCARD_FILTER)
  /* initialize L3L4 Filters variable */
  osi_core->l3l4_filter_bitmask = OSI_NONE;
 #endif /* !L3L4_WILDCARD_FILTER */

  ret = mgbe_dma_chan_to_vmirq_map (osi_core);
  // TBD: debugging reset mmc counters for T264
  if (osi_core->pre_sil == OSI_ENABLE) {
    // TODO: removed in tot dev-main
    // mgbe_reset_mmc(osi_core);
  }

  osi_memset (
    &osi_core->rch_index,
    0,
    sizeof (struct rchlist_index)*RCHLIST_SIZE
    );
fail:
  return ret;
}

/**
 * @brief mgbe_handle_mac_fpe_intrs
 *
 * @note
 * Algorithm: This function takes care of handling the
 *      MAC FPE interrupts.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC interrupts need to be enabled
 *
 */
static void
mgbe_handle_mac_fpe_intrs (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t  val = 0;

  /* interrupt bit clear on read as CSR_SW is reset */
  val = osi_readla (
          osi_core,
          (nveu8_t *)
          osi_core->base + MGBE_MAC_FPE_CTS
          );
  if (val != 0U ) {
    osi_core->mac_common_intr_rcvd = OSI_ENABLE;
  }

  if ((val & MGBE_MAC_FPE_CTS_RVER) == MGBE_MAC_FPE_CTS_RVER) {
    val &= ~MGBE_MAC_FPE_CTS_RVER;
    val |= MGBE_MAC_FPE_CTS_SRSP;
  }

  if ((val & MGBE_MAC_FPE_CTS_RRSP) == MGBE_MAC_FPE_CTS_RRSP) {
    /* received respose packet  Nothing to be done, it means other
     * IP also support FPE
     */
    val                &= ~MGBE_MAC_FPE_CTS_RRSP;
    val                &= ~MGBE_MAC_FPE_CTS_TVER;
    osi_core->fpe_ready = OSI_ENABLE;
    val                |= MGBE_MAC_FPE_CTS_EFPE;
  }

  if ((val & MGBE_MAC_FPE_CTS_TRSP) == MGBE_MAC_FPE_CTS_TRSP) {
    /* TX response packet sucessful */
    osi_core->fpe_ready = OSI_ENABLE;
    /* Enable frame preemption */
    val &= ~MGBE_MAC_FPE_CTS_TRSP;
    val &= ~MGBE_MAC_FPE_CTS_TVER;
    val |= MGBE_MAC_FPE_CTS_EFPE;
  }

  if ((val & MGBE_MAC_FPE_CTS_TVER) == MGBE_MAC_FPE_CTS_TVER) {
    /*Transmit verif packet sucessful*/
    osi_core->fpe_ready = OSI_DISABLE;
    val                &= ~MGBE_MAC_FPE_CTS_TVER;
    val                &= ~MGBE_MAC_FPE_CTS_EFPE;
  }

  osi_writela (
    osi_core,
    val,
    (nveu8_t *)
    osi_core->base + MGBE_MAC_FPE_CTS
    );
}

/**
 * @brief Get free timestamp index from TS array by validating in_use param
 *
 * @param[in] l_core: Core local private data structure.
 *
 * @retval Free timestamp index.
 *
 * @post If timestamp index is MAX_TX_TS_CNT, then its no free count index
 *       is available.
 */
static inline nveu32_t
get_free_ts_idx (
  struct core_local  *l_core
  )
{
  nveu32_t  i;

  for (i = 0; i < MAX_TX_TS_CNT; i++) {
    if (l_core->ts[i].in_use == OSI_NONE) {
      break;
    }
  }

  return i;
}

static void
mgbe_handle_link_change_and_fpe_intrs (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   mac_isr
  )
{
  struct core_local  *l_core = (struct core_local *)(void *)osi_core;
  nveu32_t           mac_ier = 0;
  nveu8_t            *base   = (nveu8_t *)osi_core->base;
  nveu32_t           value   = 0U;

 #ifdef HSI_SUPPORT
  const nveu32_t  fsm[2] = {
    (MGBE_SNPS_SCS_REG1_TRCFSM | MGBE_SNPS_SCS_REG1_RPERXLPIFSM),
    OSI_NONE
  };
  nveu32_t        link_ok = 0;
 #endif /* HSI_SUPPORT */

  /* T264-MGBE_HSIv2-72, T264-MGBE_HSIv2-78 we wil be relying on MAC interrupt
   * for any fault occurs during link training */

  /* Check for Link status change interrupt */
  if ((mac_isr & MGBE_MAC_ISR_LSI) == OSI_ENABLE) {
    /* For Local fault need to stop network data and restart the LANE bringup */
    if ((mac_isr & MGBE_MAC_ISR_LS_MASK) == MGBE_MAC_ISR_LS_LOCAL_FAULT) {
      /* Disable the Link Status interrupt before the lane_restart_task,
       * so that multiple interrupts can be avoided from the HW.
       * The Link Status interrupt will be enabled by hw_set_speed
       * which is called after lane bring up task
       */
      value  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
      value &= ~MGBE_IMR_RGSMIIIE;
      osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_MAC_IER);

      /* Mark that UPHY lane is down */
      l_core->lane_status = OSI_DISABLE;
      osi_core->osd_ops.restart_lane_bringup (osi_core->osd, OSI_DISABLE);
    } else if (((mac_isr & MGBE_MAC_ISR_LS_MASK) == MGBE_MAC_ISR_LS_LINK_OK) &&
               (l_core->lane_status == OSI_ENABLE))
    {
      osi_core->osd_ops.restart_lane_bringup (osi_core->osd, OSI_ENABLE);
 #ifdef HSI_SUPPORT
      link_ok = 1;
 #endif /* HSI_SUPPORT */

      /* re-enable interrupt */
      value  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
      value |= MGBE_IMR_RGSMIIIE;
      osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
    } else {
      /* Do nothing */
    }

 #ifdef HSI_SUPPORT
    osi_writela (
      osi_core,
      fsm[link_ok],
      (nveu8_t *)osi_core->base + MGBE_SNPS_SCS_REG1
      );
 #endif /* HSI_SUPPORT */
  }

  mac_ier = osi_readla (osi_core, base + MGBE_MAC_IER);
  if (((mac_isr & MGBE_MAC_IMR_FPEIS) == MGBE_MAC_IMR_FPEIS) &&
      ((mac_ier & MGBE_IMR_FPEIE) == MGBE_IMR_FPEIE))
  {
    mgbe_handle_mac_fpe_intrs (osi_core);
  }
}

/**
 * @brief mgbe_handle_mac_intrs - Handle MAC interrupts
 *
 * @note
 * Algorithm: This function takes care of handling the
 *      MAC nve32_terrupts which includes speed, mode detection.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC nve32_terrupts need to be enabled
 */
static void
mgbe_handle_mac_intrs (
  struct osi_core_priv_data  *osi_core
  )
{
  struct core_local  *l_core   = (struct core_local *)(void *)osi_core;
  nveu32_t           mac_isr   = 0;
  nveu32_t           tx_errors = 0;
  nveu8_t            *base     = (nveu8_t *)osi_core->base;
  nveu32_t           pktid     = 0U;

 #ifdef HSI_SUPPORT
  nveu64_t  tx_frame_err = 0;
 #endif

  mac_isr = osi_readla (osi_core, base + MGBE_MAC_ISR);

  if (mac_isr != 0U ) {
    osi_core->mac_common_intr_rcvd = OSI_ENABLE;
  }

  /* handle mgbe link change and fpe interrupts */
  mgbe_handle_link_change_and_fpe_intrs (osi_core, mac_isr);

  /* Check for any MAC Transmit Error Status Interrupt */
  if ((mac_isr & MGBE_IMR_TXESIE) == MGBE_IMR_TXESIE) {
    /* Check for the type of Tx error by reading  MAC_Rx_Tx_Status
     * register
     */
    tx_errors = osi_readl (base + MGBE_MAC_RX_TX_STS);
 #ifndef OSI_STRIPPED_LIB
    if ((tx_errors & MGBE_MAC_TX_TJT) == MGBE_MAC_TX_TJT) {
      /* increment Tx Jabber timeout stats */
      osi_core->stats.mgbe_jabber_timeout_err =
        osi_update_stats_counter (
          osi_core->stats.mgbe_jabber_timeout_err,
          1UL
          );
    }

    if ((tx_errors & MGBE_MAC_TX_IHE) == MGBE_MAC_TX_IHE) {
      /* IP Header Error */
      osi_core->stats.mgbe_ip_header_err =
        osi_update_stats_counter (
          osi_core->stats.mgbe_ip_header_err,
          1UL
          );
    }

    if ((tx_errors & MGBE_MAC_TX_PCE) == MGBE_MAC_TX_PCE) {
      /* Payload Checksum error */
      osi_core->stats.mgbe_payload_cs_err =
        osi_update_stats_counter (
          osi_core->stats.mgbe_payload_cs_err,
          1UL
          );
    }

 #endif /* !OSI_STRIPPED_LIB */

 #ifdef HSI_SUPPORT
    tx_errors &= (MGBE_MAC_TX_TJT | MGBE_MAC_TX_IHE | MGBE_MAC_TX_PCE);
    if (tx_errors != OSI_NONE) {
      osi_core->hsi.tx_frame_err_count =
        osi_update_stats_counter (
          osi_core->hsi.tx_frame_err_count,
          1UL
          );
      tx_frame_err = osi_core->hsi.tx_frame_err_count /
                     osi_core->hsi.err_count_threshold;
      if (osi_core->hsi.tx_frame_err_threshold <
          tx_frame_err)
      {
        osi_core->hsi.tx_frame_err_threshold             = tx_frame_err;
        osi_core->hsi.report_count_err[TX_FRAME_ERR_IDX] = OSI_ENABLE;
      }

      osi_core->hsi.err_code[TX_FRAME_ERR_IDX] = OSI_TX_FRAME_ERR;
      osi_core->hsi.report_err                 = OSI_ENABLE;
    }

 #endif
  }

  if ((mac_isr & MGBE_ISR_TSIS) == MGBE_ISR_TSIS) {
    struct osi_core_tx_ts  *head = &l_core->tx_ts_head;

    if (__sync_fetch_and_add (&l_core->ts_lock, 1) == 1U) {
      /* mask return as initial value is returned always */
      (void)__sync_fetch_and_sub (&l_core->ts_lock, 1);
 #ifndef OSI_STRIPPED_LIB
      osi_core->stats.ts_lock_add_fail =
        osi_update_stats_counter (osi_core->stats.ts_lock_add_fail, 1U);
 #endif /* !OSI_STRIPPED_LIB */
      goto done;
    }

    /* TXTSC bit should get reset when all timestamp read */
    while (((osi_readla (osi_core, base + MGBE_MAC_TSS) &
             MGBE_MAC_TSS_TXTSC) == MGBE_MAC_TSS_TXTSC))
    {
      nveu32_t  i = get_free_ts_idx (l_core);

      if (i == MAX_TX_TS_CNT) {
        struct osi_core_tx_ts  *temp = l_core->tx_ts_head.next;

        /* Remove oldest stale TS from list to make
         * space for new TS
         */
        OSI_CORE_INFO (
          (osi_core->osd),
          (OSI_LOG_ARG_INVALID),
          ("Removing TS from queue pkt_id\n"),
          (temp->pkt_id)
          );

        temp->in_use = OSI_DISABLE;
        /* remove temp node from the link */
        temp->next->prev = temp->prev;
        temp->prev->next =  temp->next;
        i                = get_free_ts_idx (l_core);
        if (i == MAX_TX_TS_CNT) {
          OSI_CORE_ERR (
            osi_core->osd,
            OSI_LOG_ARG_HW_FAIL,
            "TS queue is full\n",
            i
            );
          break;
        }
      }

      l_core->ts[i].nsec = osi_readla (osi_core, base + MGBE_MAC_TSNSSEC);

      l_core->ts[i].in_use  = OSI_ENABLE;
      pktid                 = osi_readla (osi_core, base + MGBE_MAC_TSPKID);
      l_core->ts[i].pkt_id  = (pktid & MGBE_PKTID_MASK);
      l_core->ts[i].vdma_id = 0U;
      if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
        l_core->ts[i].vdma_id =
          ((pktid & MGBE_VDMAID_MASK) >> MGBE_VDMAID_OFFSET);
      }

      l_core->ts[i].sec = osi_readla (osi_core, base + MGBE_MAC_TSSEC);
      /* Add time stamp to end of list */
      l_core->ts[i].next = head->prev->next;
      head->prev->next   = &l_core->ts[i];
      l_core->ts[i].prev = head->prev;
      head->prev         = &l_core->ts[i];
    }

    /* mask return as initial value is returned always */
    (void)__sync_fetch_and_sub (&l_core->ts_lock, 1);
  }

done:
  return;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief mgbe_update_dma_sr_stats - stats for dma_status error
 *
 * @note
 * Algorithm: increament error stats based on corresponding bit filed.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dma_sr: Dma status register read value
 * @param[in] chan: DMA channel number
 */
static inline void
mgbe_update_dma_sr_stats (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   dma_sr,
  nveu32_t                   chan
  )
{
  nveu64_t  val;

  if ((dma_sr & MGBE_DMA_CHX_STATUS_RBU) == MGBE_DMA_CHX_STATUS_RBU) {
    val                                        = osi_core->stats.rx_buf_unavail_irq_n[chan];
    osi_core->stats.rx_buf_unavail_irq_n[chan] =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & MGBE_DMA_CHX_STATUS_TPS) == MGBE_DMA_CHX_STATUS_TPS) {
    val                                         = osi_core->stats.tx_proc_stopped_irq_n[chan];
    osi_core->stats.tx_proc_stopped_irq_n[chan] =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & MGBE_DMA_CHX_STATUS_TBU) == MGBE_DMA_CHX_STATUS_TBU) {
    val                                        = osi_core->stats.tx_buf_unavail_irq_n[chan];
    osi_core->stats.tx_buf_unavail_irq_n[chan] =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & MGBE_DMA_CHX_STATUS_RPS) == MGBE_DMA_CHX_STATUS_RPS) {
    val                                         = osi_core->stats.rx_proc_stopped_irq_n[chan];
    osi_core->stats.rx_proc_stopped_irq_n[chan] =
      osi_update_stats_counter (val, 1U);
  }

  if ((dma_sr & MGBE_DMA_CHX_STATUS_FBE) == MGBE_DMA_CHX_STATUS_FBE) {
    val                                   = osi_core->stats.fatal_bus_error_irq_n;
    osi_core->stats.fatal_bus_error_irq_n =
      osi_update_stats_counter (val, 1U);
  }
}

#endif /* !OSI_STRIPPED_LIB */

static nve32_t
validate_avb_args (
  struct osi_core_priv_data *const            osi_core,
  const struct osi_core_avb_algorithm *const  avb
  )
{
  nve32_t  ret = -1;

  /* queue index in range */
  if (avb->qindex >= OSI_MGBE_MAX_NUM_QUEUES) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Queue index\n",
      (nveul64_t)avb->qindex
      );
    goto done;
  }

  /* queue oper_mode in range check*/
  if (avb->oper_mode >= OSI_MTL_QUEUE_MODEMAX) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Queue mode\n",
      (nveul64_t)avb->qindex
      );
    goto done;
  }

  /* Validate algo is valid  */
  if (avb->algo > OSI_MTL_TXQ_AVALG_CBS) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Algo input\n",
      (nveul64_t)avb->algo
      );
    goto done;
  }

  /* can't set AVB mode for queue 0 */
  if ((avb->qindex == 0U) && (avb->oper_mode == OSI_MTL_QUEUE_AVB)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OPNOTSUPP,
      "Not allowed to set AVB for Q0\n",
      (nveul64_t)avb->qindex
      );
    goto done;
  }

  /* TC index range check */
  if ((avb->tcindex == 0U) || (avb->tcindex >= OSI_MAX_TC_NUM)) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Queue TC mapping\n",
      (nveul64_t)avb->tcindex
      );
    goto done;
  }

  /* Check for CC */
  if (avb->credit_control > OSI_ENABLE) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid credit control\n",
      (nveul64_t)avb->credit_control
      );
    goto done;
  }

  ret = 0;

done:
  return ret;
}

/**
 * @brief mgbe_set_avb_algorithm - Set TxQ/TC avb config
 *
 * @note
 * Algorithm:
 *      1) Check if queue index is valid
 *      2) Update operation mode of TxQ/TC
 *       2a) Set TxQ operation mode
 *       2b) Set Algo and Credit contro
 *       2c) Set Send slope credit
 *       2d) Set Idle slope credit
 *       2e) Set Hi credit
 *       2f) Set low credit
 *      3) Update register values
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] avb: structure having configuration for avb algorithm
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_set_avb_algorithm (
  struct osi_core_priv_data *const            osi_core,
  const struct osi_core_avb_algorithm *const  avb
  )
{
  nveu32_t  value;
  nve32_t   ret   = 0;
  nveu32_t  qinx  = 0U;
  nveu32_t  tcinx = 0U;

  /* Validate AVB arguments */
  ret = validate_avb_args (osi_core, avb);
  if (ret == -1) {
    goto done;
  }

  qinx  = avb->qindex;
  tcinx = avb->tcindex;
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_CHX_TX_OP_MODE (qinx)
            );
  value &= ~MGBE_MTL_TX_OP_MODE_TXQEN;
  /* Set TXQEN mode as per input struct after masking 3 bit */
  value |= ((avb->oper_mode << MGBE_MTL_TX_OP_MODE_TXQEN_SHIFT) &
            MGBE_MTL_TX_OP_MODE_TXQEN);
  if (osi_core->mac == OSI_MAC_HW_MGBE) {
    /* Set TC mapping */
    value &= ~MGBE_MTL_TX_OP_MODE_Q2TCMAP;
    value |= ((tcinx << MGBE_MTL_TX_OP_MODE_Q2TCMAP_SHIFT) &
              MGBE_MTL_TX_OP_MODE_Q2TCMAP);
  }

  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_CHX_TX_OP_MODE (qinx)
    );

  /* Set Algo and Credit control */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_TCQ_ETS_CR (tcinx)
            );
  value &= ~MGBE_MTL_TCQ_ETS_CR_AVALG;
  value &= ~MGBE_MTL_TCQ_ETS_CR_CC;
  if (avb->algo == OSI_MTL_TXQ_AVALG_CBS) {
    value |= (avb->credit_control << MGBE_MTL_TCQ_ETS_CR_CC_SHIFT) &
             MGBE_MTL_TCQ_ETS_CR_CC;
    value |= (OSI_MTL_TXQ_AVALG_CBS << MGBE_MTL_TCQ_ETS_CR_AVALG_SHIFT) &
             MGBE_MTL_TCQ_ETS_CR_AVALG;
  } else {
    value |= (OSI_MGBE_TXQ_AVALG_ETS << MGBE_MTL_TCQ_ETS_CR_AVALG_SHIFT) &
             MGBE_MTL_TCQ_ETS_CR_AVALG;
  }

  osi_writela (
    osi_core,
    value,
    (nveu8_t *)osi_core->base +
    MGBE_MTL_TCQ_ETS_CR (tcinx)
    );

  if (avb->algo == OSI_MTL_TXQ_AVALG_CBS) {
    /* Set Idle slope credit*/
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MTL_TCQ_QW (tcinx)
              );
    value &= ~MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK;
    value |= avb->idle_slope & MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_TCQ_QW (tcinx)
      );

    /* Set Send slope credit */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MTL_TCQ_ETS_SSCR (tcinx)
              );
    value &= ~MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK;
    value |= avb->send_slope & MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_TCQ_ETS_SSCR (tcinx)
      );

    /* Set Hi credit */
    value = avb->hi_credit & MGBE_MTL_TCQ_ETS_HCR_HC_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_TCQ_ETS_HCR (tcinx)
      );

    /* low credit  is -ve number, osi_write need a nveu32_t
     * take only 28:0 bits from avb->low_credit
     */
    value = avb->low_credit & MGBE_MTL_TCQ_ETS_LCR_LC_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_TCQ_ETS_LCR (tcinx)
      );
  } else {
    /* Reset register values to POR/initialized values */
    osi_writela (
      osi_core,
      MGBE_MTL_TCQ_QW_ISCQW,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_TCQ_QW (tcinx)
      );
    osi_writela (
      osi_core,
      OSI_DISABLE,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_TCQ_ETS_SSCR (tcinx)
      );
    osi_writela (
      osi_core,
      OSI_DISABLE,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_TCQ_ETS_HCR (tcinx)
      );
    osi_writela (
      osi_core,
      OSI_DISABLE,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_TCQ_ETS_LCR (tcinx)
      );
    if (osi_core->mac == OSI_MAC_HW_MGBE) {
      /* Q2TCMAP is reserved for T26x */
      value = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                MGBE_MTL_CHX_TX_OP_MODE (qinx)
                );
      value &= ~MGBE_MTL_TX_OP_MODE_Q2TCMAP;
      value |= (osi_core->tc[qinx] << MGBE_MTL_CHX_TX_OP_MODE_Q2TC_SH);
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        MGBE_MTL_CHX_TX_OP_MODE (qinx)
        );
    }
  }

done:
  return ret;
}

/**
 * @brief mgbe_get_avb_algorithm - Get TxQ/TC avb config
 *
 * @note
 * Algorithm:
 *      1) Check if queue index is valid
 *      2) read operation mode of TxQ/TC
 *       2a) read TxQ operation mode
 *       2b) read Algo and Credit contro
 *       2c) read Send slope credit
 *       2d) read Idle slope credit
 *       2e) read Hi credit
 *       2f) read low credit
 *      3) updated pointer
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[out] avb: structure pointer having configuration for avb algorithm
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_get_avb_algorithm (
  struct osi_core_priv_data *const      osi_core,
  struct osi_core_avb_algorithm *const  avb
  )
{
  nveu32_t  value;
  nve32_t   ret   = 0;
  nveu32_t  qinx  = 0U;
  nveu32_t  tcinx = 0U;

  if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_OPNOTSUPP,
      "Not supported for T26x\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  if ((avb->qindex >= OSI_MGBE_MAX_NUM_QUEUES) ||
      (avb->qindex == OSI_NONE))
  {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Invalid Queue index\n",
      (nveul64_t)avb->qindex
      );
    ret = -1;
    goto fail;
  }

  qinx  = avb->qindex;
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_CHX_TX_OP_MODE (qinx)
            );

  /* Get TxQ/TC mode as per input struct after masking 3:2 bit */
  avb->oper_mode = (value & MGBE_MTL_TX_OP_MODE_TXQEN) >>
                   MGBE_MTL_TX_OP_MODE_TXQEN_SHIFT;

  /* Get Queue Traffic Class Mapping */
  avb->tcindex = ((value & MGBE_MTL_TX_OP_MODE_Q2TCMAP) >>
                  MGBE_MTL_TX_OP_MODE_Q2TCMAP_SHIFT);
  tcinx = avb->tcindex;

  /* Get Algo and Credit control */
  value = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_TCQ_ETS_CR (tcinx)
            );
  avb->credit_control = (value & MGBE_MTL_TCQ_ETS_CR_CC) >>
                        MGBE_MTL_TCQ_ETS_CR_CC_SHIFT;
  avb->algo = (value & MGBE_MTL_TCQ_ETS_CR_AVALG) >>
              MGBE_MTL_TCQ_ETS_CR_AVALG_SHIFT;

  if (avb->algo == OSI_MTL_TXQ_AVALG_CBS) {
    /* Get Idle slope credit*/
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MTL_TCQ_QW (tcinx)
              );
    avb->idle_slope = value & MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK;

    /* Get Send slope credit */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MTL_TCQ_ETS_SSCR (tcinx)
              );
    avb->send_slope = value & MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK;

    /* Get Hi credit */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MTL_TCQ_ETS_HCR (tcinx)
              );
    avb->hi_credit = value & MGBE_MTL_TCQ_ETS_HCR_HC_MASK;

    /* Get Low credit for which bit 31:29 are unknown
     * return 28:0 valid bits to application
     */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MTL_TCQ_ETS_LCR (tcinx)
              );
    avb->low_credit = value & MGBE_MTL_TCQ_ETS_LCR_LC_MASK;
  }

fail:
  return ret;
}

static void
mgbe_handle_cgce_hlbs_hlbf (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   val
  )
{
  nveu32_t   i        = 0;
  nveu32_t   sch_err  = 0U;
  nveu32_t   frm_err  = 0U;
  nveu32_t   temp     = 0U;
  nveul64_t  stat_val = 0U;
  nveu32_t   value    = 0U;

  /* increase counter write 1 back will clear */
  if ((val & MGBE_MTL_EST_STATUS_CGCE) == MGBE_MTL_EST_STATUS_CGCE) {
    osi_core->est_ready                = OSI_DISABLE;
    stat_val                           = osi_core->stats.const_gate_ctr_err;
    osi_core->stats.const_gate_ctr_err =
      osi_update_stats_counter (stat_val, 1U);
  }

  if ((val & MGBE_MTL_EST_STATUS_HLBS) == MGBE_MTL_EST_STATUS_HLBS) {
    osi_core->est_ready                  = OSI_DISABLE;
    stat_val                             = osi_core->stats.head_of_line_blk_sch;
    osi_core->stats.head_of_line_blk_sch =
      osi_update_stats_counter (stat_val, 1U);
    /* Need to read MTL_EST_Sch_Error register and cleared */
    sch_err = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                MGBE_MTL_EST_SCH_ERR
                );
    for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
      temp = OSI_ENABLE;
      temp = temp << i;
      if ((sch_err & temp) == temp) {
        stat_val                  = osi_core->stats.hlbs_q[i];
        osi_core->stats.hlbs_q[i] =
          osi_update_stats_counter (stat_val, 1U);
      }
    }

    sch_err &= 0xFFU;             // only 8 TC allowed so clearing all
    osi_writela (
      osi_core,
      sch_err,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_EST_SCH_ERR
      );
    /* Reset EST with prnve32_t to configure it properly */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MTL_EST_CONTROL
              );
    value &= ~MGBE_MTL_EST_EEST;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_EST_CONTROL
      );
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "Disabling EST due to HLBS, correct GCL\n",
      OSI_NONE
      );
  }

  if ((val & MGBE_MTL_EST_STATUS_HLBF) == MGBE_MTL_EST_STATUS_HLBF) {
    osi_core->est_ready                  = OSI_DISABLE;
    stat_val                             = osi_core->stats.head_of_line_blk_frm;
    osi_core->stats.head_of_line_blk_frm =
      osi_update_stats_counter (stat_val, 1U);
    /* Need to read MTL_EST_Frm_Size_Error register and cleared */
    frm_err = osi_readla (
                osi_core,
                (nveu8_t *)osi_core->base +
                MGBE_MTL_EST_FRMS_ERR
                );
    for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
      temp = OSI_ENABLE;
      temp = temp << i;
      if ((frm_err & temp) == temp) {
        stat_val                  = osi_core->stats.hlbf_q[i];
        osi_core->stats.hlbf_q[i] =
          osi_update_stats_counter (stat_val, 1U);
      }
    }

    frm_err &= 0xFFU;             // only 8 TC allowed so clearing all
    osi_writela (
      osi_core,
      frm_err,
      (nveu8_t *)osi_core->base +
      MGBE_MTL_EST_FRMS_ERR
      );

    /* Reset EST with prnve32_t to configure it properly */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MTL_EST_CONTROL
              );
    /* DDBF 1 means don't drop packets */
    if ((value & MGBE_MTL_EST_CONTROL_DDBF) ==
        MGBE_MTL_EST_CONTROL_DDBF)
    {
      value &= ~MGBE_MTL_EST_EEST;
      osi_writela (
        osi_core,
        value,
        (nveu8_t *)osi_core->base +
        MGBE_MTL_EST_CONTROL
        );
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "Disabling EST due to HLBF, correct GCL\n",
        OSI_NONE
        );
    }
  }
}

/**
 * @brief mgbe_handle_mtl_intrs - Handle MTL interrupts
 *
 * @note
 * Algorithm: Code to handle interrupt for MTL EST error and status.
 * There are possible 4 errors which can be part of common interrupt in case of
 * MTL_EST_SCH_ERR (sheduling error)- HLBS
 * MTL_EST_FRMS_ERR (Frame size error) - HLBF
 * MTL_EST_FRMC_ERR (frame check error) - HLBF
 * Constant Gate Control Error - when time interval in less
 * than or equal to cycle time, llr = 1
 * There is one status interrupt which says swich to SWOL complete.
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] mtl_isr: MTL interrupt status value
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void
mgbe_handle_mtl_intrs (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   mtl_isr
  )
{
  nveu32_t   val      = 0U;
  nveu32_t   i        = 0;
  nveul64_t  stat_val = 0U;
  nveu32_t   qstatus  = 0U;
  nveu32_t   qinx     = 0U;

  /* Check for all MTL queues */
  for (i = 0; i < osi_core->num_mtl_queues; i++) {
    qinx = osi_core->mtl_queues[i];
    if ((mtl_isr & OSI_BIT (qinx)) ==  OSI_BIT (qinx)) {
      /* check if Q has underflow error */
      qstatus = osi_readl (
                  (nveu8_t *)osi_core->base +
                  MGBE_MTL_QINT_STATUS (qinx)
                  );
      if (qstatus != 0U ) {
        osi_core->mac_common_intr_rcvd = OSI_ENABLE;
      }

      /* Transmit Queue Underflow Interrupt Status */
      if ((qstatus & MGBE_MTL_QINT_TXUNIFS) == MGBE_MTL_QINT_TXUNIFS) {
 #ifndef OSI_STRIPPED_LIB
        osi_core->stats.mgbe_tx_underflow_err =
          osi_update_stats_counter (
            osi_core->stats.mgbe_tx_underflow_err,
            1UL
            );
 #endif /* !OSI_STRIPPED_LIB */
      }

      /* Clear interrupt status by writing back with 1 */
      osi_writel (
        1U,
        (nveu8_t *)osi_core->base +
        MGBE_MTL_QINT_STATUS (qinx)
        );
    }
  }

  if ((mtl_isr & MGBE_MTL_IS_ESTIS) != MGBE_MTL_IS_ESTIS) {
    goto done;
  }

  val = osi_readla (
          osi_core,
          (nveu8_t *)osi_core->base + MGBE_MTL_EST_STATUS
          );
  val &= (MGBE_MTL_EST_STATUS_CGCE | MGBE_MTL_EST_STATUS_HLBS |
          MGBE_MTL_EST_STATUS_HLBF | MGBE_MTL_EST_STATUS_BTRE |
          MGBE_MTL_EST_STATUS_SWLC);

  /* return if interrupt is not related to EST */
  if (val == OSI_DISABLE) {
    goto done;
  }

  /* Handle Constant Gate Control Error,
   * Head-Of-Line Blocking due to Scheduling
   * Head-Of-Line Blocking due to Frame Size
   */
  mgbe_handle_cgce_hlbs_hlbf (osi_core, val);

  if ((val & MGBE_MTL_EST_STATUS_SWLC) == MGBE_MTL_EST_STATUS_SWLC) {
    if ((val & MGBE_MTL_EST_STATUS_BTRE) !=
        MGBE_MTL_EST_STATUS_BTRE)
    {
      osi_core->est_ready = OSI_ENABLE;
    }

    stat_val                             = osi_core->stats.sw_own_list_complete;
    osi_core->stats.sw_own_list_complete =
      osi_update_stats_counter (stat_val, 1U);
  }

  if ((val & MGBE_MTL_EST_STATUS_BTRE) == MGBE_MTL_EST_STATUS_BTRE) {
    osi_core->est_ready               = OSI_DISABLE;
    stat_val                          = osi_core->stats.base_time_reg_err;
    osi_core->stats.base_time_reg_err =
      osi_update_stats_counter (stat_val, 1U);
    osi_core->est_ready = OSI_DISABLE;
  }

  /* clear EST status register as interrupt is handled */
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->base + MGBE_MTL_EST_STATUS
    );

done:
  return;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief mgbe_config_ptp_offload - Enable/Disable PTP offload
 *
 * @note
 * Algorithm: Based on input argument, update PTO and TSCR registers.
 * Update ptp_filter for TSCR register.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) configure_ptp() should be called after this API
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_config_ptp_offload (
  struct osi_core_priv_data *const  osi_core,
  struct osi_pto_config *const      pto_config
  )
{
  nveu8_t   *addr     = (nveu8_t *)osi_core->base;
  nve32_t   ret       = 0;
  nveu32_t  value     = 0x0U;
  nveu32_t  ptc_value = 0x0U;
  nveu32_t  port_id   = 0x0U;

  /* Read MAC TCR */
  value = osi_readla (osi_core, (nveu8_t *)addr + MGBE_MAC_TCR);
  /* clear old configuration */

  value &= ~(MGBE_MAC_TCR_TSENMACADDR | OSI_MAC_TCR_SNAPTYPSEL_3 |
             OSI_MAC_TCR_TSMASTERENA | OSI_MAC_TCR_TSEVENTENA |
             OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT |
             OSI_MAC_TCR_TSCTRLSSR | OSI_MAC_TCR_TSVER2ENA |
             OSI_MAC_TCR_TSIPENA);

  /** Handle PTO disable */
  if (pto_config->en_dis == OSI_DISABLE) {
    /* update global setting in ptp_filter */
    osi_core->ptp_config.ptp_filter = value;
    osi_writela (osi_core, ptc_value, addr + MGBE_MAC_PTO_CR);
    osi_writela (osi_core, value, addr + MGBE_MAC_TCR);
    /* Setting PORT ID as 0 */
    osi_writela (osi_core, OSI_NONE, addr + MGBE_MAC_PIDR0);
    osi_writela (osi_core, OSI_NONE, addr + MGBE_MAC_PIDR1);
    osi_writela (osi_core, OSI_NONE, addr + MGBE_MAC_PIDR2);
    return 0;
  }

  /** Handle PTO enable */
  /* Set PTOEN bit */
  ptc_value |= MGBE_MAC_PTO_CR_PTOEN;
  ptc_value |= ((pto_config->domain_num << MGBE_MAC_PTO_CR_DN_SHIFT)
                & MGBE_MAC_PTO_CR_DN);

  /* Set TSCR register flag */
  value |= (OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT |
            OSI_MAC_TCR_TSCTRLSSR | OSI_MAC_TCR_TSVER2ENA |
            OSI_MAC_TCR_TSIPENA);

  if (pto_config->snap_type > 0U) {
    /* Set APDREQEN bit if snap_type > 0 */
    ptc_value |= MGBE_MAC_PTO_CR_APDREQEN;
  }

  /* Set SNAPTYPSEL for Taking Snapshots mode */
  value |= ((pto_config->snap_type << MGBE_MAC_TCR_SNAPTYPSEL_SHIFT) &
            OSI_MAC_TCR_SNAPTYPSEL_3);
  /* Set/Reset TSMSTRENA bit for Master/Slave */
  if (pto_config->master == OSI_ENABLE) {
    /* Set TSMSTRENA bit for master */
    value |= OSI_MAC_TCR_TSMASTERENA;
    if (pto_config->snap_type != OSI_PTP_SNAP_P2P) {
      /* Set ASYNCEN bit on PTO Control Register */
      ptc_value |= MGBE_MAC_PTO_CR_ASYNCEN;
    }
  } else {
    /* Reset TSMSTRENA bit for slave */
    value &= ~OSI_MAC_TCR_TSMASTERENA;
  }

  /* Set/Reset TSENMACADDR bit for UC/MC MAC */
  if (pto_config->mc_uc == OSI_ENABLE) {
    /* Set TSENMACADDR bit for MC/UC MAC PTP filter */
    value |= MGBE_MAC_TCR_TSENMACADDR;
  } else {
    /* Reset TSENMACADDR bit */
    value &= ~MGBE_MAC_TCR_TSENMACADDR;
  }

  /* Set TSEVNTENA bit for PTP events */
  value |= OSI_MAC_TCR_TSEVENTENA;

  /* update global setting in ptp_filter */
  osi_core->ptp_config.ptp_filter = value;
  /** Write PTO_CR and TCR registers */
  osi_writela (osi_core, ptc_value, addr + MGBE_MAC_PTO_CR);
  osi_writela (osi_core, value, addr + MGBE_MAC_TCR);
  /* Port ID for PTP offload packet created */
  port_id = pto_config->portid & MGBE_MAC_PIDR_PID_MASK;
  osi_writela (osi_core, port_id, addr + MGBE_MAC_PIDR0);
  osi_writela (osi_core, OSI_NONE, addr + MGBE_MAC_PIDR1);
  osi_writela (osi_core, OSI_NONE, addr + MGBE_MAC_PIDR2);

  return ret;
}

#endif /* !OSI_STRIPPED_LIB */

#ifdef HSI_SUPPORT
static void
mgbe_handle_hsi_wrap_common_intr (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t        val  = 0;
  nveu32_t        val2 = 0;
  nveu64_t        ce_count_threshold;
  const nveu32_t  intr_en[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_WRAP_COMMON_INTR_ENABLE,
    MGBE_T26X_WRAP_COMMON_INTR_ENABLE
  };
  const nveu32_t  intr_status[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_WRAP_COMMON_INTR_STATUS,
    MGBE_T26X_WRAP_COMMON_INTR_STATUS
  };

  val = osi_readla (
          osi_core,
          (nveu8_t *)osi_core->base +
          intr_status[osi_core->mac]
          );
  if (((val & MGBE_REGISTER_PARITY_ERR) == MGBE_REGISTER_PARITY_ERR) ||
      ((val & MGBE_CORE_UNCORRECTABLE_ERR) == MGBE_CORE_UNCORRECTABLE_ERR))
  {
    osi_core->hsi.err_code[UE_IDX]         = OSI_UNCORRECTABLE_ERR;
    osi_core->hsi.report_err               = OSI_ENABLE;
    osi_core->hsi.report_count_err[UE_IDX] = OSI_ENABLE;
    /* Disable the interrupt */
    val2 = osi_readla (
             osi_core,
             (nveu8_t *)osi_core->base +
             intr_en[osi_core->mac]
             );
    val2 &= ~MGBE_REGISTER_PARITY_ERR;
    val2 &= ~MGBE_CORE_UNCORRECTABLE_ERR;
    osi_writela (
      osi_core,
      val2,
      (nveu8_t *)osi_core->base +
      intr_en[osi_core->mac]
      );
  }

  if ((val & MGBE_CORE_CORRECTABLE_ERR) == MGBE_CORE_CORRECTABLE_ERR) {
    osi_core->hsi.err_code[CE_IDX] = OSI_CORRECTABLE_ERR;
    osi_core->hsi.report_err       = OSI_ENABLE;
    osi_core->hsi.ce_count         =
      osi_update_stats_counter (osi_core->hsi.ce_count, 1UL);
    ce_count_threshold = osi_core->hsi.ce_count / osi_core->hsi.err_count_threshold;
    if (osi_core->hsi.ce_count_threshold < ce_count_threshold) {
      osi_core->hsi.ce_count_threshold       = ce_count_threshold;
      osi_core->hsi.report_count_err[CE_IDX] = OSI_ENABLE;
    }
  }

  val &= ~MGBE_MAC_SBD_INTR;
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->base +
    intr_status[osi_core->mac]
    );

  if (((val & MGBE_CORE_CORRECTABLE_ERR) == MGBE_CORE_CORRECTABLE_ERR) ||
      ((val & MGBE_CORE_UNCORRECTABLE_ERR) == MGBE_CORE_UNCORRECTABLE_ERR))
  {
    /* Clear status register for FSM errors. Clear on read*/
    (void)osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MAC_DPP_FSM_INTERRUPT_STATUS
            );

    /* Clear status register for ECC error */
    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_MTL_ECC_INTERRUPT_STATUS
            );
    if (val != 0U) {
      osi_writela (
        osi_core,
        val,
        (nveu8_t *)osi_core->base +
        MGBE_MTL_ECC_INTERRUPT_STATUS
        );
    }

    val = osi_readla (
            osi_core,
            (nveu8_t *)osi_core->base +
            MGBE_DMA_ECC_INTERRUPT_STATUS
            );
    if (val != 0U) {
      osi_writela (
        osi_core,
        val,
        (nveu8_t *)osi_core->base +
        MGBE_DMA_ECC_INTERRUPT_STATUS
        );
    }
  }
}

/**
 * @brief mgbe_handle_hsi_intr - Handles hsi interrupt.
 *
 * @note
 * Algorithm:
 * - Read safety interrupt status register and clear it.
 * - Update error code in osi_hsi_data structure
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void
mgbe_handle_hsi_intr (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t        val        = 0;
  nveu32_t        val2       = 0;
  void            *xpcs_base = osi_core->xpcs_base;
  nveu64_t        ce_count_threshold;
  const nveu32_t  xpcs_intr_ctrl_reg[OSI_MAX_MAC_IP_TYPES] = {
    0,
    XPCS_WRAP_INTERRUPT_CONTROL,
    T26X_XPCS_WRAP_INTERRUPT_CONTROL
  };
  const nveu32_t  xpcs_intr_sts_reg[OSI_MAX_MAC_IP_TYPES] = {
    0,
    XPCS_WRAP_INTERRUPT_STATUS,
    T26X_XPCS_WRAP_INTERRUPT_STATUS
  };

  /* Handle HSI wrapper common interrupt */
  mgbe_handle_hsi_wrap_common_intr (osi_core);

  val = osi_readla (
          osi_core,
          (nveu8_t *)osi_core->xpcs_base +
          xpcs_intr_sts_reg[osi_core->mac]
          );
  if (((val & XPCS_CORE_UNCORRECTABLE_ERR) == XPCS_CORE_UNCORRECTABLE_ERR) ||
      ((val & XPCS_REGISTER_PARITY_ERR) == XPCS_REGISTER_PARITY_ERR))
  {
    osi_core->hsi.err_code[UE_IDX]         = OSI_UNCORRECTABLE_ERR;
    osi_core->hsi.report_err               = OSI_ENABLE;
    osi_core->hsi.report_count_err[UE_IDX] = OSI_ENABLE;
    /* Disable uncorrectable interrupts */
    val2 = osi_readla (
             osi_core,
             (nveu8_t *)osi_core->xpcs_base +
             xpcs_intr_ctrl_reg[osi_core->mac]
             );
    val2 &= ~XPCS_CORE_UNCORRECTABLE_ERR;
    val2 &= ~XPCS_REGISTER_PARITY_ERR;
    osi_writela (
      osi_core,
      val2,
      (nveu8_t *)osi_core->xpcs_base +
      xpcs_intr_ctrl_reg[osi_core->mac]
      );
  }

  if ((val & XPCS_CORE_CORRECTABLE_ERR) == XPCS_CORE_CORRECTABLE_ERR) {
    osi_core->hsi.err_code[CE_IDX] = OSI_CORRECTABLE_ERR;
    osi_core->hsi.report_err       = OSI_ENABLE;
    osi_core->hsi.ce_count         =
      osi_update_stats_counter (osi_core->hsi.ce_count, 1UL);
    ce_count_threshold = osi_core->hsi.ce_count / osi_core->hsi.err_count_threshold;
    if (osi_core->hsi.ce_count_threshold < ce_count_threshold) {
      osi_core->hsi.ce_count_threshold       = ce_count_threshold;
      osi_core->hsi.report_count_err[CE_IDX] = OSI_ENABLE;
    }
  }

  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->xpcs_base +
    xpcs_intr_sts_reg[osi_core->mac]
    );

  if (((val & XPCS_CORE_CORRECTABLE_ERR) == XPCS_CORE_CORRECTABLE_ERR) ||
      ((val & XPCS_CORE_UNCORRECTABLE_ERR) == XPCS_CORE_UNCORRECTABLE_ERR))
  {
    /* Clear status register for PCS error */
    val = xpcs_read (xpcs_base, XPCS_VR_XS_PCS_SFTY_UE_INTR0);
    if (val != 0U) {
      (void)xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_SFTY_UE_INTR0, 0);
    }

    val = xpcs_read (xpcs_base, XPCS_VR_XS_PCS_SFTY_CE_INTR);
    if (val != 0U) {
      (void)xpcs_write_safety (osi_core, XPCS_VR_XS_PCS_SFTY_CE_INTR, 0);
    }
  }
}

#endif

/**
 * @brief mgbe_check_intr_status - Check interrupt status.
 *
 * Algorithm: Check for the MDIO, LPI, PCTH, PCTW status registers
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void
mgbe_check_intr_status (
  struct osi_core_priv_data *const  osi_core
  )
{
  nveu32_t  value;

 #ifndef OSI_STRIPPED_LIB
  /* Read for MAC_LPI_Control_Status */
  value = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_LPI_CSR);
  if ((value & MGBE_MAC_LPI_STATUS_MASK) != 0U) {
    osi_core->mac_common_intr_rcvd = OSI_ENABLE;
  }

 #endif /* !OSI_STRIPPED_LIB */

  /* Read for MDIO_Interrupt_Status */
  value = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_MDIO_INTR_STS);
  if (value != 0U) {
    osi_core->mac_common_intr_rcvd = OSI_ENABLE;
  }

  /* Read for MAC_PCTH_Intr_Status */
  value = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_PCTH_INTR_STS);
  if (value != 0U) {
    osi_core->mac_common_intr_rcvd = OSI_ENABLE;
  }

  /* Read for MAC_PCTW_Intr_Status */
  value = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_PCTW_INTR_STS);
  if (value != 0U) {
    osi_core->mac_common_intr_rcvd = OSI_ENABLE;
  }
}

/**
 * @brief mgbe_handle_common_intr - Handles common interrupt.
 *
 * @note
 * Algorithm: Clear common nve32_terrupt source.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void
mgbe_handle_common_intr (
  struct osi_core_priv_data *const  osi_core
  )
{
  struct core_local  *l_core                       = (struct core_local *)(void *)osi_core;
  const nveu32_t     intr_en[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_WRAP_COMMON_INTR_ENABLE,
    MGBE_T26X_WRAP_COMMON_INTR_ENABLE
  };
  const nveu32_t     intr_status[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_WRAP_COMMON_INTR_STATUS,
    MGBE_T26X_WRAP_COMMON_INTR_STATUS
  };
  void               *base           = osi_core->base;
  nveu32_t           dma_isr_ch0_15  = 0;
  nveu32_t           dma_isr_ch16_47 = 0;
  nveu32_t           chan            = 0;
  nveu32_t           i               = 0;
  nveu32_t           dma_sr          = 0;
  nveu32_t           dma_ier         = 0;
  nveu32_t           mtl_isr         = 0;
  nveu32_t           val             = 0;

 #ifdef HSI_SUPPORT
  if ((osi_core->hsi.enabled == OSI_ENABLE) && (osi_core->mac != OSI_MAC_HW_MGBE_T26X)) {
    mgbe_handle_hsi_intr (osi_core);
  }

 #endif
  dma_isr_ch0_15 = osi_readla (
                     osi_core,
                     (nveu8_t *)base +
                     MGBE_DMA_ISR_CH0_15
                     );
  if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
    dma_isr_ch16_47 = osi_readla (
                        osi_core,
                        (nveu8_t *)base +
                        MGBE_DMA_ISR_CH16_47
                        );
  }

  if ((dma_isr_ch0_15 == OSI_NONE) && (dma_isr_ch16_47 == OSI_NONE)) {
    goto done;
  }

  if (((dma_isr_ch0_15 & MGBE_DMA_ISR_DCH0_DCH15_MASK) != OSI_NONE) ||
      ((dma_isr_ch16_47 & MGBE_DMA_ISR_DCH16_DCH47_MASK) != OSI_NONE))
  {
    /* Handle Non-TI/RI nve32_terrupts */
    for (i = 0; i < osi_core->num_dma_chans; i++) {
      chan = osi_core->dma_chans[i];

      if (chan >= l_core->num_max_chans) {
        continue;
      }

      /* read dma channel status register */
      dma_sr = osi_readla (
                 osi_core,
                 (nveu8_t *)base +
                 MGBE_DMA_CHX_STATUS (chan)
                 );
      /* read dma channel nve32_terrupt enable register */
      dma_ier = osi_readla (
                  osi_core,
                  (nveu8_t *)base +
                  MGBE_DMA_CHX_IER (chan)
                  );

      /* process only those nve32_terrupts which we
       * have enabled.
       */
      dma_sr = (dma_sr & dma_ier);

      /* mask off RI and TI */
      dma_sr &= ~(MGBE_DMA_CHX_STATUS_TI |
                  MGBE_DMA_CHX_STATUS_RI);
      if (dma_sr == OSI_NONE) {
        continue;
      }

      /* ack non ti/ri nve32_ts */
      osi_writela (
        osi_core,
        dma_sr,
        (nveu8_t *)base +
        MGBE_DMA_CHX_STATUS (chan)
        );
 #ifndef OSI_STRIPPED_LIB
      mgbe_update_dma_sr_stats (osi_core, dma_sr, chan);
 #endif /* !OSI_STRIPPED_LIB */
    }
  }

  /* Handle MAC interrupts */
  if ((dma_isr_ch0_15 & MGBE_DMA_ISR_MACIS) == MGBE_DMA_ISR_MACIS) {
    mgbe_handle_mac_intrs (osi_core);
  }

  /* Handle MTL inerrupts */
  mtl_isr = osi_readla (
              osi_core,
              (nveu8_t *)base + MGBE_MTL_INTR_STATUS
              );
  if ((dma_isr_ch0_15 & MGBE_DMA_ISR_MTLIS) == MGBE_DMA_ISR_MTLIS) {
    mgbe_handle_mtl_intrs (osi_core, mtl_isr);
  }

  /* Check MDIO, LPI, PCTH, PCTW interrupt status */
  mgbe_check_intr_status (osi_core);

  /* Clear common interrupt status in wrapper register */
  osi_writela (
    osi_core,
    MGBE_MAC_SBD_INTR,
    (nveu8_t *)base + intr_status[osi_core->mac]
    );
  val = osi_readla (
          osi_core,
          (nveu8_t *)osi_core->base +
          intr_en[osi_core->mac]
          );
  val |= MGBE_MAC_SBD_INTR;
  osi_writela (
    osi_core,
    val,
    (nveu8_t *)osi_core->base +
    intr_en[osi_core->mac]
    );

  /* Clear FRP Interrupts in MTL_RXP_Interrupt_Control_Status */
  val  = osi_readla (osi_core, (nveu8_t *)base + MGBE_MTL_RXP_INTR_CS);
  val |= (MGBE_MTL_RXP_INTR_CS_NVEOVIS |
          MGBE_MTL_RXP_INTR_CS_NPEOVIS |
          MGBE_MTL_RXP_INTR_CS_FOOVIS |
          MGBE_MTL_RXP_INTR_CS_PDRFIS);
  osi_writela (osi_core, val, (nveu8_t *)base + MGBE_MTL_RXP_INTR_CS);
 #ifdef HSI_SUPPORT
  /* if interrupt is not from any of the below conditions then notify error */
  if ((osi_core->hsi.enabled == OSI_ENABLE) &&
      !(  (dma_sr != 0U) || (dma_isr_ch0_15 != 0U) || (dma_isr_ch16_47 != 0U)
       || (mtl_isr != 0U) || (val != 0U) || (osi_core->mac_common_intr_rcvd != 0U)))
  {
    osi_core->hsi.err_code[MAC_CMN_INTR_ERR_IDX]         = OSI_MAC_CMN_INTR_ERR;
    osi_core->hsi.report_err                             = OSI_ENABLE;
    osi_core->hsi.report_count_err[MAC_CMN_INTR_ERR_IDX] = OSI_ENABLE;
    osi_core->mac_common_intr_rcvd                       = OSI_DISABLE;
  }

 #endif

done:
  return;
}

/**
 * @brief mgbe_pad_calibrate - PAD calibration
 *
 * @note
 * Algorithm: Since PAD calibration not applicable for MGBE
 * it returns zero.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval zero always
 */
static nve32_t
mgbe_pad_calibrate (
  OSI_UNUSED
  struct osi_core_priv_data *const  osi_core
  )
{
  (void)osi_core;       // unused
  return 0;
}

#if defined (MACSEC_SUPPORT)

/**
 * @brief mgbe_config_mac_tx - Enable/Disable MAC Tx
 *
 * @note
 * Algorithm: Enable/Disable MAC Transmitter engine
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: Enable or Disable.MAC Tx
 *
 * @note 1) MAC init should be complete. See osi_hw_core_init()
 */
static void
mgbe_config_mac_tx (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable
  )
{
  nveu32_t  value;
  void      *addr = osi_core->base;

  if (enable == OSI_ENABLE) {
    value = osi_readla (osi_core, (nveu8_t *)addr + MGBE_MAC_TMCR);
    /* Enable MAC Transmit */
    value |= MGBE_MAC_TMCR_TE;
    osi_writela (osi_core, value, (nveu8_t *)addr + MGBE_MAC_TMCR);
  } else {
    value = osi_readla (osi_core, (nveu8_t *)addr + MGBE_MAC_TMCR);
    /* Disable MAC Transmit */
    value &= ~MGBE_MAC_TMCR_TE;
    osi_writela (osi_core, value, (nveu8_t *)addr + MGBE_MAC_TMCR);
  }
}

#endif /*  MACSEC_SUPPORT */

/**
 * @brief mgbe_mdio_busy_wait - MDIO busy wait loop
 *
 * @note
 * Algorithm: Wait for any previous MII read/write operation to complete
 *
 * @param[in] osi_core: OSI core data struture.
 */
static nve32_t
mgbe_mdio_busy_wait (
  struct osi_core_priv_data *const  osi_core
  )
{
  /* half second timeout */
  nveu32_t  retry = 50000;
  nveu32_t  mac_gmiiar;
  nveu32_t  count;
  nve32_t   cond = 1;
  nve32_t   ret  = 0;

  count = 0;
  while (cond == 1) {
    if (count > retry) {
      ret = -1;
      goto fail;
    }

    count++;

    mac_gmiiar = osi_readla (
                   osi_core,
                   (nveu8_t *)
                   osi_core->base + MGBE_MDIO_SCCD
                   );
    if ((mac_gmiiar & MGBE_MDIO_SCCD_SBUSY) == 0U) {
      cond = 0;
    } else {
      osi_core->osd_ops.usleep (OSI_DELAY_10US);
    }
  }

fail:
  return ret;
}

/**
 * @brief mgbe_write_phy_reg - Write to a PHY register over MDIO bus.
 *
 * @note
 * Algorithm: Write into a PHY register through MGBE MDIO bus.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be write to PHY.
 * @param[in] phydata: Data to write to a PHY register.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_write_phy_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    phyreg,
  const nveu16_t                    phydata
  )
{
  nve32_t   ret = 0;
  nveu32_t  reg;

  /* Wait for any previous MII read/write operation to complete */
  ret = mgbe_mdio_busy_wait (osi_core);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "MII operation timed out\n",
      0ULL
      );
    goto fail;
  }

  /* set MDIO address register */
  /* set device address */
  reg = ((phyreg >> MGBE_MDIO_C45_DA_SHIFT) & MGBE_MDIO_SCCA_DA_MASK) <<
        MGBE_MDIO_SCCA_DA_SHIFT;
  /* set port address and register address */
  reg |= (phyaddr << MGBE_MDIO_SCCA_PA_SHIFT) |
         (phyreg & MGBE_MDIO_SCCA_RA_MASK);
  osi_writela (
    osi_core,
    reg,
    (nveu8_t *)
    osi_core->base + MGBE_MDIO_SCCA
    );

  /* Program Data register */
  reg = phydata |
        (((nveu32_t)MGBE_MDIO_SCCD_CMD_WR) << MGBE_MDIO_SCCD_CMD_SHIFT) |
        MGBE_MDIO_SCCD_SBUSY;

  reg |= (((osi_core->mdc_cr) & MGBE_MDIO_SCCD_CR_MASK) << MGBE_MDIO_SCCD_CR_SHIFT);

  if (osi_core->mdc_cr > 7U) {
    /* Set clock range select for higher frequencies */
    reg |= MGBE_MDIO_SCCD_CRS;
  }

  osi_writela (
    osi_core,
    reg,
    (nveu8_t *)
    osi_core->base + MGBE_MDIO_SCCD
    );

  /* wait for MII write operation to complete */
  ret = mgbe_mdio_busy_wait (osi_core);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "MII operation timed out\n",
      0ULL
      );
  }

fail:
  return ret;
}

/**
 * @brief mgbe_read_phy_reg - Read from a PHY register over MDIO bus.
 *
 * @note
 * Algorithm: Write into a PHY register through MGBE MDIO bus.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be read from PHY.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_read_phy_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    phyreg
  )
{
  nveu32_t  reg;
  nveu32_t  data;
  nve32_t   ret = 0;

  ret = mgbe_mdio_busy_wait (osi_core);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "MII operation timed out\n",
      0ULL
      );
    goto fail;
  }

  /* set MDIO address register */
  /* set device address */
  reg = ((phyreg >> MGBE_MDIO_C45_DA_SHIFT) & MGBE_MDIO_SCCA_DA_MASK) <<
        MGBE_MDIO_SCCA_DA_SHIFT;
  /* set port address and register address */
  reg |= (phyaddr << MGBE_MDIO_SCCA_PA_SHIFT) |
         (phyreg & MGBE_MDIO_SCCA_RA_MASK);
  osi_writela (
    osi_core,
    reg,
    (nveu8_t *)
    osi_core->base + MGBE_MDIO_SCCA
    );

  /* Program Data register */
  reg = (((nveu32_t)MGBE_MDIO_SCCD_CMD_RD) << MGBE_MDIO_SCCD_CMD_SHIFT) |
        MGBE_MDIO_SCCD_SBUSY;

  reg |= ((osi_core->mdc_cr & MGBE_MDIO_SCCD_CR_MASK) << MGBE_MDIO_SCCD_CR_SHIFT);

  if (osi_core->mdc_cr > 7U) {
    /* Set clock range select for higher frequencies */
    reg |= MGBE_MDIO_SCCD_CRS;
  }

  osi_writela (
    osi_core,
    reg,
    (nveu8_t *)
    osi_core->base + MGBE_MDIO_SCCD
    );

  ret = mgbe_mdio_busy_wait (osi_core);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "MII operation timed out\n",
      0ULL
      );
    goto fail;
  }

  reg = osi_readla (
          osi_core,
          (nveu8_t *)
          osi_core->base + MGBE_MDIO_SCCD
          );

  data = (reg & MGBE_MDIO_SCCD_SDATA_MASK);

  ret = (nve32_t)data;
fail:
  return ret;
}

#ifdef PHY_PROG

/**
 * @brief mgbe_write_phy_reg_dt - Write to a PHY register over MDIO bus using DT values.
 *
 * Algorithm: Write into a PHY register through MGBE MDIO bus using MAC MDIO address
 * and data register values from device tree.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY.
 * @param[in] macMdioForAddrReg: MAC MDIO address register value from DT.
 * @param[in] macMdioForDataReg: MAC MDIO data register value from DT.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_write_phy_reg_dt (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    macMdioForAddrReg,
  const nveu32_t                    macMdioForDataReg
  )
{
  nve32_t   ret     = 0;
  nveu32_t  valSCCA = macMdioForAddrReg;
  nveu32_t  valSCCD = macMdioForDataReg;

  valSCCA |= (phyaddr << MGBE_MDIO_SCCA_PA_SHIFT) |
             (valSCCA & MGBE_MDIO_SCCA_RA_MASK);

  /* Wait for any previous MII read/write operation to complete */
  ret = mgbe_mdio_busy_wait (osi_core);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "MII operation timed out\n",
      0ULL
      );
    goto fail;
  }

  osi_writela (
    osi_core,
    valSCCA,
    (nveu8_t *)
    osi_core->base + MGBE_MDIO_SCCA
    );
  osi_writela (
    osi_core,
    valSCCD,
    (nveu8_t *)
    osi_core->base + MGBE_MDIO_SCCD
    );

  /* wait for MII write operation to complete */
  ret = mgbe_mdio_busy_wait (osi_core);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "MII operation timed out\n",
      0ULL
      );
  }

fail:
  return ret;
}

/**
 * @brief mgbe_read_phy_reg_dt - Read from a PHY register over MDIO bus using DT values.
 *
 * Algorithm: Read from a PHY register through MGBE MDIO bus using MAC MDIO address
 * and data register values from device tree.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY.
 * @param[in] macMdioForAddrReg: MAC MDIO address register value from DT.
 * @param[in] macMdioForDataReg: MAC MDIO data register value from DT.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval PHY register value on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_read_phy_reg_dt (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    macMdioForAddrReg,
  const nveu32_t                    macMdioForDataReg
  )
{
  nve32_t   ret = 0;
  nveu32_t  data;
  nveu32_t  valSCCA = macMdioForAddrReg;
  nveu32_t  valSCCD = macMdioForDataReg;

  valSCCA |= (phyaddr << MGBE_MDIO_SCCA_PA_SHIFT) |
             (valSCCA & MGBE_MDIO_SCCA_RA_MASK);

  ret = mgbe_mdio_busy_wait (osi_core);
  if (ret < 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "MII operation timed out\n",
      0ULL
      );
    goto fail;
  }

  osi_writela (
    osi_core,
    valSCCA,
    (nveu8_t *)
    osi_core->base + MGBE_MDIO_SCCA
    );
  osi_writela (
    osi_core,
    valSCCD,
    (nveu8_t *)
    osi_core->base + MGBE_MDIO_SCCD
    );

  data = osi_readla (
           osi_core,
           (nveu8_t *)
           osi_core->base + MGBE_MDIO_SCCD
           );

  data = (data & MGBE_MDIO_SCCD_SDATA_MASK);
  ret  = (nve32_t)data;
fail:
  return ret;
}

#endif /* PHY_PROG */

#ifndef OSI_STRIPPED_LIB

/**
 * @brief mgbe_disable_tx_lpi - Helper function to disable Tx LPI.
 *
 * @note
 * Algorithm:
 * Clear the bits to enable Tx LPI, Tx LPI automate, LPI Tx Timer and
 * PHY Link status in the LPI control/status register
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC has to be out of reset, and clocks supplied.
 */
static inline void
mgbe_disable_tx_lpi (
  struct osi_core_priv_data  *osi_core
  )
{
  nveu32_t  lpi_csr = 0;

  /* Disable LPI control bits */
  lpi_csr = osi_readla (
              osi_core,
              (nveu8_t *)
              osi_core->base + MGBE_MAC_LPI_CSR
              );
  lpi_csr &= ~(MGBE_MAC_LPI_CSR_LPITE | MGBE_MAC_LPI_CSR_LPITXA |
               MGBE_MAC_LPI_CSR_PLS | MGBE_MAC_LPI_CSR_LPIEN);
  osi_writela (
    osi_core,
    lpi_csr,
    (nveu8_t *)
    osi_core->base + MGBE_MAC_LPI_CSR
    );
}

/**
 * @brief mgbe_configure_eee - Configure the EEE LPI mode
 *
 * @note
 * Algorithm: This routine configures EEE LPI mode in the MAC.
 * 1) The time (in microsecond) to wait before resuming transmission after
 * exiting from LPI
 * 2) The time (in millisecond) to wait before LPI pattern can be transmitted
 * after PHY link is up) are not configurable. Default values are used in this
 * routine.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_lpi_enable: enable (1)/disable (0) flag for Tx lpi
 * @param[in] tx_lpi_timer: Tx LPI entry timer in usec. Valid upto
 * OSI_MAX_TX_LPI_TIMER in steps of 8usec.
 *
 * @note Required clks and resets has to be enabled.
 * MAC/PHY should be initialized
 *
 */
static void
mgbe_configure_eee (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    tx_lpi_enabled,
  const nveu32_t                    tx_lpi_timer
  )
{
  nveu32_t  lpi_csr         = 0;
  nveu32_t  lpi_timer_ctrl  = 0;
  nveu32_t  lpi_entry_timer = 0;
  nveu32_t  tic_counter     = 0;
  void      *addr           =  osi_core->base;

  if (osi_core->uphy_gbe_mode == OSI_GBE_MODE_25G) {
    if (xlgpcs_eee (osi_core, tx_lpi_enabled) != 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_INVALID,
        "xlgpcs_eee call failed\n",
        0ULL
        );
      return;
    }
  } else if (xpcs_eee (osi_core, tx_lpi_enabled) != 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_INVALID,
      "xpcs_eee call failed\n",
      0ULL
      );
    return;
  }

  if (tx_lpi_enabled != OSI_DISABLE) {
    /** 3. Program LST (bits[25:16]) and TWT (bits[15:0]) in
     *  MAC_LPI_Timers_Control Register
     * Configure the following timers.
     *  a. LPI LS timer - minimum time (in milliseconds) for
     *     which the link status from PHY should be up before
     *     the LPI pattern can be transmitted to the PHY. Default
     *     1sec
     * b. LPI TW timer - minimum time (in microseconds) for which
     *     MAC waits after it stops transmitting LPI pattern before
     *     resuming normal tx. Default 21us
     */

    lpi_timer_ctrl |= ((MGBE_DEFAULT_LPI_LS_TIMER <<
                        MGBE_LPI_LS_TIMER_SHIFT) &
                       MGBE_LPI_LS_TIMER_MASK);
    lpi_timer_ctrl |= (MGBE_DEFAULT_LPI_TW_TIMER &
                       MGBE_LPI_TW_TIMER_MASK);
    osi_writela (
      osi_core,
      lpi_timer_ctrl,
      (nveu8_t *)addr +
      MGBE_MAC_LPI_TIMER_CTRL
      );

    /* 4. For GMII, read the link status of the PHY chip by
     * using the MDIO interface and update Bit 17 of
     * MAC_LPI_Control_Status register accordingly. This update
     * should be done whenever the link status in the PHY chip
     * changes. For XGMII, the update is automatic unless
     * PLSDIS bit is set." (skip) */
    /* 5. Program the MAC_1US_Tic_Counter as per the frequency
     *  of the clock used for accessing the CSR slave port.
     */
    /* Should be same as (ABP clock freq - 1) = 12 = 0xC, currently
     * from define but we should get it from pdata->clock  TODO */
    tic_counter = MGBE_1US_TIC_COUNTER;
    osi_writela (
      osi_core,
      tic_counter,
      (nveu8_t *)addr +
      MGBE_MAC_1US_TIC_COUNT
      );

    /* 6. Program the MAC_LPI_Auto_Entry_Timer register (LPIET)
     *   with the IDLE time for which the MAC should wait
     *   before entering the LPI state on its own.
     * LPI entry timer - Time in microseconds that MAC will wait
     * to enter LPI mode after all tx is complete. Default 1sec
     */
    lpi_entry_timer |= (tx_lpi_timer & MGBE_LPI_ENTRY_TIMER_MASK);
    osi_writela (
      osi_core,
      lpi_entry_timer,
      (nveu8_t *)addr +
      MGBE_MAC_LPI_EN_TIMER
      );

    /* 7. Set LPIATE and LPITXA (bit[20:19]) of
     *    MAC_LPI_Control_Status register to enable the auto-entry
     *    into LPI and auto-exit of MAC from LPI state.
     * 8. Set LPITXEN (bit[16]) of MAC_LPI_Control_Status register
     *    to make the MAC Transmitter enter the LPI state. The MAC
     *    enters the LPI mode after completing all scheduled
     *    packets and remain IDLE for the time indicated by LPIET.
     */
    lpi_csr = osi_readla (
                osi_core,
                (nveu8_t *)
                addr + MGBE_MAC_LPI_CSR
                );
    lpi_csr |= (MGBE_MAC_LPI_CSR_LPITE | MGBE_MAC_LPI_CSR_LPITXA |
                MGBE_MAC_LPI_CSR_PLS | MGBE_MAC_LPI_CSR_LPIEN);
    osi_writela (
      osi_core,
      lpi_csr,
      (nveu8_t *)
      addr + MGBE_MAC_LPI_CSR
      );
  } else {
    /* Disable LPI control bits */
    mgbe_disable_tx_lpi (osi_core);
  }
}

#endif /* !OSI_STRIPPED_LIB */

static void
mgbe_get_hw_features (
  struct osi_core_priv_data *const  osi_core,
  struct osi_hw_features            *hw_feat
  )
{
  const nveu32_t  addmac_addrsel_shift[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_MAC_HFR0_ADDMACADRSEL_SHIFT,
    MGBE_T26X_MAC_HFR0_ADDMACADRSEL_SHIFT
  };
  const nveu32_t  addmac_addrsel_mask[OSI_MAX_MAC_IP_TYPES] = {
    0,
    MGBE_MAC_HFR0_ADDMACADRSEL_MASK,
    MGBE_T26X_MAC_HFR0_ADDMACADRSEL_MASK
  };
  nveu8_t         *base    = (nveu8_t *)osi_core->base;
  nveu32_t        mac      = osi_core->mac;
  nveu32_t        mac_hfr0 = 0;
  nveu32_t        mac_hfr1 = 0;
  nveu32_t        mac_hfr2 = 0;
  nveu32_t        mac_hfr3 = 0;

 #ifndef OSI_STRIPPED_LIB
  nveu32_t  val = 0;
 #endif /* !OSI_STRIPPED_LIB */
  nve32_t  ret = 0;

  if (osi_core->pre_sil == OSI_ENABLE) {
    /* TBD: T264 reset to get mac version for MGBE */
    osi_writela (osi_core, 0x1U, ((nveu8_t *)osi_core->base + MGBE_DMA_MODE));
    ret = hw_poll_for_swr (osi_core);
    if (ret < 0) {
      OSI_CORE_ERR (
        osi_core->osd,
        OSI_LOG_ARG_HW_FAIL,
        "T264 MGBE Reset failed\n",
        0ULL
        );
      goto done;
    }
  }

  mac_hfr0 = osi_readla (osi_core, base + MGBE_MAC_HFR0);
  mac_hfr1 = osi_readla (osi_core, base + MGBE_MAC_HFR1);
  mac_hfr2 = osi_readla (osi_core, base + MGBE_MAC_HFR2);
  mac_hfr3 = osi_readla (osi_core, base + MGBE_MAC_HFR3);

  hw_feat->rgmii_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RGMIISEL_SHIFT) &
                        MGBE_MAC_HFR0_RGMIISEL_MASK);
  hw_feat->gmii_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_GMIISEL_SHIFT) &
                       MGBE_MAC_HFR0_GMIISEL_MASK);
  hw_feat->rmii_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RMIISEL_SHIFT) &
                       MGBE_MAC_HFR0_RMIISEL_MASK);
  hw_feat->hd_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_HDSEL_SHIFT) &
                     MGBE_MAC_HFR0_HDSEL_MASK);
  hw_feat->vlan_hash_en = ((mac_hfr0 >> MGBE_MAC_HFR0_VLHASH_SHIFT) &
                           MGBE_MAC_HFR0_VLHASH_MASK);
  hw_feat->sma_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_SMASEL_SHIFT) &
                      MGBE_MAC_HFR0_SMASEL_MASK);
  hw_feat->rwk_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RWKSEL_SHIFT) &
                      MGBE_MAC_HFR0_RWKSEL_MASK);
  hw_feat->mgk_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_MGKSEL_SHIFT) &
                      MGBE_MAC_HFR0_MGKSEL_MASK);
  hw_feat->mmc_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_MMCSEL_SHIFT) &
                      MGBE_MAC_HFR0_MMCSEL_MASK);
  hw_feat->arp_offld_en = ((mac_hfr0 >> MGBE_MAC_HFR0_ARPOFFLDEN_SHIFT) &
                           MGBE_MAC_HFR0_ARPOFFLDEN_MASK);
  hw_feat->rav_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RAVSEL_SHIFT) &
                      MGBE_MAC_HFR0_RAVSEL_MASK);
  hw_feat->av_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_AVSEL_SHIFT) &
                     MGBE_MAC_HFR0_AVSEL_MASK);
  hw_feat->ts_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_TSSSEL_SHIFT) &
                     MGBE_MAC_HFR0_TSSSEL_MASK);
  hw_feat->eee_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_EEESEL_SHIFT) &
                      MGBE_MAC_HFR0_EEESEL_MASK);
  hw_feat->tx_coe_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_TXCOESEL_SHIFT) &
                         MGBE_MAC_HFR0_TXCOESEL_MASK);
  hw_feat->rx_coe_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RXCOESEL_SHIFT) &
                         MGBE_MAC_HFR0_RXCOESEL_MASK);
  hw_feat->mac_addr_sel = ((mac_hfr0 >> addmac_addrsel_shift[mac]) &
                           addmac_addrsel_mask[mac]);
  hw_feat->act_phy_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_PHYSEL_SHIFT) &
                          MGBE_MAC_HFR0_PHYSEL_MASK);
  hw_feat->tsstssel = ((mac_hfr0 >> MGBE_MAC_HFR0_TSSTSSEL_SHIFT) &
                       MGBE_MAC_HFR0_TSSTSSEL_MASK);
  hw_feat->sa_vlan_ins = ((mac_hfr0 >> MGBE_MAC_HFR0_SAVLANINS_SHIFT) &
                          MGBE_MAC_HFR0_SAVLANINS_SHIFT);
  hw_feat->vxn = ((mac_hfr0 >> MGBE_MAC_HFR0_VXN_SHIFT) &
                  MGBE_MAC_HFR0_VXN_MASK);
  hw_feat->ediffc = ((mac_hfr0 >> MGBE_MAC_HFR0_EDIFFC_SHIFT) &
                     MGBE_MAC_HFR0_EDIFFC_MASK);
  hw_feat->edma = ((mac_hfr0 >> MGBE_MAC_HFR0_EDMA_SHIFT) &
                   MGBE_MAC_HFR0_EDMA_MASK);
  hw_feat->rx_fifo_size = ((mac_hfr1 >> MGBE_MAC_HFR1_RXFIFOSIZE_SHIFT) &
                           MGBE_MAC_HFR1_RXFIFOSIZE_MASK);
  hw_feat->pfc_en = ((mac_hfr1 >> MGBE_MAC_HFR1_PFCEN_SHIFT) &
                     MGBE_MAC_HFR1_PFCEN_MASK);
  hw_feat->tx_fifo_size = ((mac_hfr1 >> MGBE_MAC_HFR1_TXFIFOSIZE_SHIFT) &
                           MGBE_MAC_HFR1_TXFIFOSIZE_MASK);
  hw_feat->ost_en = ((mac_hfr1 >> MGBE_MAC_HFR1_OSTEN_SHIFT) &
                     MGBE_MAC_HFR1_OSTEN_MASK);
  hw_feat->pto_en = ((mac_hfr1 >> MGBE_MAC_HFR1_PTOEN_SHIFT) &
                     MGBE_MAC_HFR1_PTOEN_MASK);
  hw_feat->adv_ts_hword = ((mac_hfr1 >> MGBE_MAC_HFR1_ADVTHWORD_SHIFT) &
                           MGBE_MAC_HFR1_ADVTHWORD_MASK);
  hw_feat->addr_64 = ((mac_hfr1 >> MGBE_MAC_HFR1_ADDR64_SHIFT) &
                      MGBE_MAC_HFR1_ADDR64_MASK);
  hw_feat->dcb_en = ((mac_hfr1 >> MGBE_MAC_HFR1_DCBEN_SHIFT) &
                     MGBE_MAC_HFR1_DCBEN_MASK);
  hw_feat->sph_en = ((mac_hfr1 >> MGBE_MAC_HFR1_SPHEN_SHIFT) &
                     MGBE_MAC_HFR1_SPHEN_MASK);
  hw_feat->tso_en = ((mac_hfr1 >> MGBE_MAC_HFR1_TSOEN_SHIFT) &
                     MGBE_MAC_HFR1_TSOEN_MASK);
  hw_feat->dma_debug_gen = ((mac_hfr1 >> MGBE_MAC_HFR1_DBGMEMA_SHIFT) &
                            MGBE_MAC_HFR1_DBGMEMA_MASK);
  hw_feat->rss_en = ((mac_hfr1 >> MGBE_MAC_HFR1_RSSEN_SHIFT) &
                     MGBE_MAC_HFR1_RSSEN_MASK);
  hw_feat->num_tc = ((mac_hfr1 >> MGBE_MAC_HFR1_NUMTC_SHIFT) &
                     MGBE_MAC_HFR1_NUMTC_MASK);
  hw_feat->hash_tbl_sz = ((mac_hfr1 >> MGBE_MAC_HFR1_HASHTBLSZ_SHIFT) &
                          MGBE_MAC_HFR1_HASHTBLSZ_MASK);
  hw_feat->l3l4_filter_num = ((mac_hfr1 >> MGBE_MAC_HFR1_L3L4FNUM_SHIFT) &
                              MGBE_MAC_HFR1_L3L4FNUM_MASK);
  hw_feat->rx_q_cnt = ((mac_hfr2 >> MGBE_MAC_HFR2_RXQCNT_SHIFT) &
                       MGBE_MAC_HFR2_RXQCNT_MASK);
  hw_feat->tx_q_cnt = ((mac_hfr2 >> MGBE_MAC_HFR2_TXQCNT_SHIFT) &
                       MGBE_MAC_HFR2_TXQCNT_MASK);
  hw_feat->rx_ch_cnt = ((mac_hfr2 >> MGBE_MAC_HFR2_RXCHCNT_SHIFT) &
                        MGBE_MAC_HFR2_RXCHCNT_MASK);
  hw_feat->tx_ch_cnt = ((mac_hfr2 >> MGBE_MAC_HFR2_TXCHCNT_SHIFT) &
                        MGBE_MAC_HFR2_TXCHCNT_MASK);
  hw_feat->pps_out_num = ((mac_hfr2 >> MGBE_MAC_HFR2_PPSOUTNUM_SHIFT) &
                          MGBE_MAC_HFR2_PPSOUTNUM_MASK);
  hw_feat->aux_snap_num = ((mac_hfr2 >> MGBE_MAC_HFR2_AUXSNAPNUM_SHIFT) &
                           MGBE_MAC_HFR2_AUXSNAPNUM_MASK);
  hw_feat->num_vlan_filters = ((mac_hfr3 >> MGBE_MAC_HFR3_NRVF_SHIFT) &
                               MGBE_MAC_HFR3_NRVF_MASK);
  hw_feat->frp_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_FRPSEL_SHIFT) &
                      MGBE_MAC_HFR3_FRPSEL_MASK);
  hw_feat->cbti_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_CBTISEL_SHIFT) &
                       MGBE_MAC_HFR3_CBTISEL_MASK);
  hw_feat->num_frp_pipes = ((mac_hfr3 >> MGBE_MAC_HFR3_FRPPIPE_SHIFT) &
                            MGBE_MAC_HFR3_FRPPIPE_MASK);
  hw_feat->ost_over_udp = ((mac_hfr3 >> MGBE_MAC_HFR3_POUOST_SHIFT) &
                           MGBE_MAC_HFR3_POUOST_MASK);

 #ifndef OSI_STRIPPED_LIB
  val = ((mac_hfr3 >> MGBE_MAC_HFR3_FRPPB_SHIFT) &
         MGBE_MAC_HFR3_FRPPB_MASK);
  switch (val) {
    case MGBE_MAC_FRPPB_64:
      hw_feat->max_frp_bytes = MGBE_MAC_FRP_BYTES64;
      break;
    case MGBE_MAC_FRPPB_128:
      hw_feat->max_frp_bytes = MGBE_MAC_FRP_BYTES128;
      break;
    case MGBE_MAC_FRPPB_256:
    default:
      hw_feat->max_frp_bytes = MGBE_MAC_FRP_BYTES256;
      break;
  }

 #else
  /* For safety fix the FRP bytes */
  hw_feat->max_frp_bytes = MGBE_MAC_FRP_BYTES256;
 #endif /* !OSI_STRIPPED_LIB */

 #ifndef OSI_STRIPPED_LIB
  val = ((mac_hfr3 >> MGBE_MAC_HFR3_FRPES_SHIFT) &
         MGBE_MAC_HFR3_FRPES_MASK);
  switch (val) {
    case MGBE_MAC_FRPES_64:
      hw_feat->max_frp_entries = MGBE_MAC_FRP_BYTES64;
      break;
    case MGBE_MAC_FRPES_128:
      hw_feat->max_frp_entries = MGBE_MAC_FRP_BYTES128;
      break;
    case MGBE_MAC_FRPES_256:
    default:
      hw_feat->max_frp_entries = MGBE_MAC_FRP_BYTES256;
      break;
  }

 #else
  /* For Safety fix the FRP entries */
  hw_feat->max_frp_entries = MGBE_MAC_FRP_BYTES256;
 #endif /* !OSI_STRIPPED_LIB */

  hw_feat->double_vlan_en = ((mac_hfr3 >> MGBE_MAC_HFR3_DVLAN_SHIFT) &
                             MGBE_MAC_HFR3_DVLAN_MASK);
  hw_feat->auto_safety_pkg = ((mac_hfr3 >> MGBE_MAC_HFR3_ASP_SHIFT) &
                              MGBE_MAC_HFR3_ASP_MASK);
  hw_feat->tts_fifo_depth = ((mac_hfr3 >> MGBE_MAC_HFR3_TTSFD_SHIFT) &
                             MGBE_MAC_HFR3_TTSFD_MASK);
  hw_feat->est_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_ESTSEL_SHIFT) &
                      MGBE_MAC_HFR3_ESTSEL_MASK);
  hw_feat->gcl_depth = ((mac_hfr3 >> MGBE_MAC_HFR3_GCLDEP_SHIFT) &
                        MGBE_MAC_HFR3_GCLDEP_MASK);
  hw_feat->gcl_width = ((mac_hfr3 >> MGBE_MAC_HFR3_GCLWID_SHIFT) &
                        MGBE_MAC_HFR3_GCLWID_MASK);
  hw_feat->fpe_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_FPESEL_SHIFT) &
                      MGBE_MAC_HFR3_FPESEL_MASK);
  hw_feat->tbs_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_TBSSEL_SHIFT) &
                      MGBE_MAC_HFR3_TBSSEL_MASK);
  hw_feat->num_tbs_ch = ((mac_hfr3 >> MGBE_MAC_HFR3_TBS_CH_SHIFT) &
                         MGBE_MAC_HFR3_TBS_CH_MASK);
done:
  return;
}

/**
 * @brief mgbe_poll_for_update_ts_complete - Poll for update time stamp
 *
 * @note
 * Algorithm: Read time stamp update value from TCR register until it is
 *      equal to zero.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] mac_tcr: Address to store time stamp control register read value
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t
mgbe_poll_for_update_ts_complete (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   *mac_tcr
  )
{
  nveu32_t  retry = 0U;
  nve32_t   ret   = -1;

  while (retry < OSI_POLL_COUNT) {
    /* Read and Check TSUPDT in MAC_Timestamp_Control register */
    *mac_tcr = osi_readla (
                 osi_core,
                 (nveu8_t *)
                 osi_core->base + MGBE_MAC_TCR
                 );
    if ((*mac_tcr & MGBE_MAC_TCR_TSUPDT) == 0U) {
      ret = 0;
      break;
    }

    retry++;
    osi_core->osd_ops.udelay (OSI_DELAY_1US);
  }

  return ret;
}

/**
 * @brief mgbe_adjust_mactime - Adjust MAC time with system time
 *
 * @note
 * Algorithm: Update MAC time with system time
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] sec: Seconds to be configured
 * @param[in] nsec: Nano seconds to be configured
 * @param[in] add_sub: To decide on add/sub with system time
 * @param[in] one_nsec_accuracy: One nano second accuracy
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) osi_core->ptp_config.one_nsec_accuracy need to be set to 1
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
mgbe_adjust_mactime (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    sec,
  const nveu32_t                    nsec,
  const nveu32_t                    add_sub,
  OSI_UNUSED const nveu32_t         one_nsec_accuracy
  )
{
  void       *addr = osi_core->base;
  nveu32_t   mac_tcr;
  nveu32_t   value = 0;
  nveul64_t  temp  = 0;
  nveu32_t   temp_sec;
  nveu32_t   temp_nsec;
  nve32_t    ret = 0;

  (void)one_nsec_accuracy;
  temp_sec  = sec;
  temp_nsec = nsec;
  /* To be sure previous write was flushed (if Any) */
  ret = mgbe_poll_for_update_ts_complete (osi_core, &mac_tcr);
  if (ret == -1) {
    goto fail;
  }

  if (add_sub != 0U) {
    /* If the new sec value needs to be subtracted with
     * the system time, then MAC_STSUR reg should be
     * programmed with (2^32 – <new_sec_value>)
     */
    temp = (TWO_POWER_32 - temp_sec);
    if (temp < UINT_MAX) {
      temp_sec = (nveu32_t)temp;
    } else {
      /* do nothing here */
    }

    /* If the new nsec value need to be subtracted with
     * the system time, then MAC_STNSUR.TSSS field should be
     * programmed with, (10^9 - <new_nsec_value>) if
     * MAC_TCR.TSCTRLSSR is set or
     * (2^32 - <new_nsec_value> if MAC_TCR.TSCTRLSSR is reset)
     */
    /* one_nsec_accuracy is always enabled*/
    if (temp_nsec < UINT_MAX) {
      temp_nsec = (TEN_POWER_9 - temp_nsec);
    }
  }

  /* write seconds value to MAC_System_Time_Seconds_Update register */
  osi_writela (osi_core, temp_sec, (nveu8_t *)addr + MGBE_MAC_STSUR);

  /* write nano seconds value and add_sub to
   * MAC_System_Time_Nanoseconds_Update register
   */
  value |= temp_nsec;
  value |= (add_sub << MGBE_MAC_STNSUR_ADDSUB_SHIFT);
  osi_writela (osi_core, value, (nveu8_t *)addr + MGBE_MAC_STNSUR);

  /* issue command to initialize system time with the value
   * specified in MAC_STSUR and MAC_STNSUR
   */
  mac_tcr |= MGBE_MAC_TCR_TSUPDT;
  osi_writela (osi_core, mac_tcr, (nveu8_t *)addr + MGBE_MAC_TCR);

  ret = mgbe_poll_for_update_ts_complete (osi_core, &mac_tcr);
fail:
  return ret;
}

#if (defined (MACSEC_SUPPORT) || defined (FSI_EQOS_SUPPORT)) &&  !defined OSI_STRIPPED_LIB

/**
 * @brief mgbe_read_reg - Read a register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nveu32_t
mgbe_read_reg (
  struct osi_core_priv_data *const  osi_core,
  const nve32_t                     reg
  )
{
  return osi_readla (osi_core, (nveu8_t *)osi_core->base + reg);
}

/**
 * @brief mgbe_write_reg - Write a reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] val:  Value to be written.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nveu32_t
mgbe_write_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    val,
  const nve32_t                     reg
  )
{
  osi_writela (osi_core, val, (nveu8_t *)osi_core->base + reg);
  return 0;
}

/**
 * @brief mgbe_read_macsec_reg - Read a MACSEC register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nveu32_t
mgbe_read_macsec_reg (
  struct osi_core_priv_data *const  osi_core,
  const nve32_t                     reg
  )
{
  return osi_readla (osi_core, (nveu8_t *)osi_core->macsec_base + reg);
}

/**
 * @brief mgbe_write_macsec_reg - Write to a MACSEC reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] val:  Value to be written.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nveu32_t
mgbe_write_macsec_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    val,
  const nve32_t                     reg
  )
{
  osi_writela (osi_core, val, (nveu8_t *)osi_core->macsec_base + reg);
  return 0;
}

#endif /*  MACSEC_SUPPORT */

#ifndef OSI_STRIPPED_LIB
static nve32_t
mgbe_config_tx_status (
  OSI_UNUSED
  struct osi_core_priv_data *const  osi_core,
  OSI_UNUSED const nveu32_t         tx_status
  )
{
  return 0;
}

static nve32_t
mgbe_config_rx_crc_check (
  OSI_UNUSED
  struct osi_core_priv_data *const  osi_core,
  OSI_UNUSED const nveu32_t         crc_chk
  )
{
  return 0;
}

#endif /* !OSI_STRIPPED_LIB */

#if defined (MACSEC_SUPPORT)

/**
 * @brief mgbe_config_for_macsec - Configure MAC according to macsec IAS
 *
 * @note
 * Algorithm:
 *  - Stop MAC Tx
 *  - Update MAC IPG value to accommodate macsec 32 byte SECTAG.
 *  - Start MAC Tx
 *  - Update MTL_EST value as MACSEC is enabled/disabled
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] enable: Enable or Disable MAC Tx engine
 *
 * @pre
 *     1) MAC has to be out of reset.
 *     2) Shall not use this ipg value in half duplex mode
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void
mgbe_config_for_macsec (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    enable
  )
{
  nveu32_t  value = 0U, temp = 0U;

  /* stop MAC Tx */
  mgbe_config_mac_tx (osi_core, OSI_DISABLE);
  if (enable == OSI_ENABLE) {
    /* Configure IPG  {EIPG,IPG} value according to macsec IAS in
     * MAC_Tx_Configuration and MAC_Extended_Configuration
     * IPG (12 B[default] + 32 B[sectag]) = 352 bits
     * IPG (12 B[default] + 32 B[sectag] + 15B[if encryption is supported]) = 472 bits
     */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MAC_TMCR
              );
    value &= ~MGBE_MAC_TMCR_IPG_MASK;
    value |= MGBE_MAC_TMCR_IFP;
    if (osi_core->mac == OSI_MAC_HW_MGBE_T26X) {
      value |= MGBE_MAC_TMCR_IPG;
    }

    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MAC_TMCR
      );
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MAC_EXT_CNF
              );
    value |= MGBE_MAC_EXT_CNF_EIPG;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MAC_EXT_CNF
      );
  } else {
    /* Update MAC IPG to default value 12B */
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MAC_TMCR
              );
    value &= ~MGBE_MAC_TMCR_IPG_MASK;
    value &= ~MGBE_MAC_TMCR_IFP;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MAC_TMCR
      );
    value = osi_readla (
              osi_core,
              (nveu8_t *)osi_core->base +
              MGBE_MAC_EXT_CNF
              );
    value &= ~MGBE_MAC_EXT_CNF_EIPG_MASK;
    osi_writela (
      osi_core,
      value,
      (nveu8_t *)osi_core->base +
      MGBE_MAC_EXT_CNF
      );
  }

  /* start MAC Tx */
  mgbe_config_mac_tx (osi_core, OSI_ENABLE);

  /* Configure EST */
  value  = osi_readla (osi_core, (nveu8_t *)osi_core->base + MGBE_MTL_EST_CONTROL);
  value &= ~MGBE_MTL_EST_CONTROL_CTOV;
  if (enable == OSI_ENABLE) {
    temp   = MGBE_MTL_EST_CTOV_MACSEC_RECOMMEND;
    temp   = temp << MGBE_MTL_EST_CONTROL_CTOV_SHIFT;
    value |= temp & MGBE_MTL_EST_CONTROL_CTOV;
  } else {
    temp   = MGBE_MTL_EST_CTOV_RECOMMEND;
    temp   = temp << MGBE_MTL_EST_CONTROL_CTOV_SHIFT;
    value |= temp & MGBE_MTL_EST_CONTROL_CTOV;
  }

  osi_writela (osi_core, value, (nveu8_t *)osi_core->base + MGBE_MTL_EST_CONTROL);
  return;
}

#endif /*  MACSEC_SUPPORT */

/**
 * @brief mgbe_init_core_ops - Initialize MGBE MAC core operations
 */
void
mgbe_init_core_ops (
  struct core_ops  *ops
  )
{
  ops->core_init                    = mgbe_core_init;
  ops->handle_common_intr           = mgbe_handle_common_intr;
  ops->pad_calibrate                = mgbe_pad_calibrate;
  ops->update_mac_addr_low_high_reg = mgbe_update_mac_addr_low_high_reg;
  ops->adjust_mactime               = mgbe_adjust_mactime;
  ops->read_mmc                     = mgbe_read_mmc;
  ops->write_phy_reg                = mgbe_write_phy_reg;
  ops->read_phy_reg                 = mgbe_read_phy_reg;
 #ifdef PHY_PROG
  ops->write_phy_reg_dt = mgbe_write_phy_reg_dt;
  ops->read_phy_reg_dt  = mgbe_read_phy_reg_dt;
 #endif /* PHY_PROG */
  ops->get_hw_features = mgbe_get_hw_features;
 #ifndef OSI_STRIPPED_LIB
  ops->read_reg  = mgbe_read_reg;
  ops->write_reg = mgbe_write_reg;
 #endif
  ops->set_avb_algorithm  = mgbe_set_avb_algorithm;
  ops->get_avb_algorithm  = mgbe_get_avb_algorithm;
  ops->config_frp         = mgbe_config_frp;
  ops->update_frp_entry   = mgbe_update_frp_entry;
  ops->update_frp_nve     = mgbe_update_frp_nve;
  ops->get_rchlist_index  = mgbe_get_rchlist_index;
  ops->free_rchlist_index = mgbe_free_rchlist_index;
 #if defined MACSEC_SUPPORT && !defined OSI_STRIPPED_LIB
  ops->read_macsec_reg  = mgbe_read_macsec_reg;
  ops->write_macsec_reg = mgbe_write_macsec_reg;
 #endif /*  MACSEC_SUPPORT */
 #ifdef MACSEC_SUPPORT
  ops->macsec_config_mac = mgbe_config_for_macsec;
 #endif
  ops->config_l3l4_filters = mgbe_config_l3l4_filters;
 #ifndef OSI_STRIPPED_LIB
  ops->config_tx_status      = mgbe_config_tx_status;
  ops->config_rx_crc_check   = mgbe_config_rx_crc_check;
  ops->config_flow_control   = mgbe_config_flow_control;
  ops->config_arp_offload    = mgbe_config_arp_offload;
  ops->config_ptp_offload    = mgbe_config_ptp_offload;
  ops->config_vlan_filtering = mgbe_config_vlan_filtering;
  ops->configure_eee         = mgbe_configure_eee;
  ops->config_mac_loopback   = mgbe_config_mac_loopback;
  ops->config_rss            = mgbe_config_rss;
  ops->get_rss               = mgbe_get_rss;
  ops->config_ptp_rxq        = mgbe_config_ptp_rxq;
 #endif /* !OSI_STRIPPED_LIB */
 #ifdef HSI_SUPPORT
  ops->core_hsi_configure = mgbe_hsi_configure;
 #ifdef NV_VLTEST_BUILD
  ops->core_hsi_inject_err = mgbe_hsi_inject_err;
 #endif
 #endif
}
