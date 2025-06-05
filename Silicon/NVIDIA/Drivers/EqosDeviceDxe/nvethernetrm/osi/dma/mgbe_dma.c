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

#ifndef OSI_STRIPPED_LIB
#include "mgbe_dma.h"
#include "dma_local.h"

/**
 * @brief mgbe_config_slot - Configure slot Checking for DMA channel
 *
 * Algorithm: Set/Reset the slot function of DMA channel based on given inputs
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA channel number to enable slot function
 * @param[in] set: flag for set/reset with value OSI_ENABLE/OSI_DISABLE
 * @param[in] interval: slot interval fixed for MGBE - its 125usec
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) OSD should be initialized
 *
 */
static void
mgbe_config_slot (
  struct osi_dma_priv_data  *osi_dma,
  unsigned int              chan,
  unsigned int              set,
  OSI_UNUSED unsigned int   interval
  )
{
  unsigned int  value;
  unsigned int  local_chan = chan % OSI_MGBE_MAX_NUM_CHANS;

 #if 0
  MGBE_CHECK_CHAN_BOUND (chan);
 #endif
  if (set == OSI_ENABLE) {
    /* Program SLOT CTRL register SIV and set ESC bit */
    value = osi_dma_readl (
              (unsigned char *)osi_dma->base +
              MGBE_DMA_CHX_SLOT_CTRL (local_chan)
              );
    /* Set ESC bit */
    value |= MGBE_DMA_CHX_SLOT_ESC;
    osi_dma_writel (
      value,
      (unsigned char *)osi_dma->base +
      MGBE_DMA_CHX_SLOT_CTRL (local_chan)
      );
  } else {
    /* Clear ESC bit of SLOT CTRL register */
    value = osi_dma_readl (
              (unsigned char *)osi_dma->base +
              MGBE_DMA_CHX_SLOT_CTRL (local_chan)
              );
    value &= ~MGBE_DMA_CHX_SLOT_ESC;
    osi_dma_writel (
      value,
      (unsigned char *)osi_dma->base +
      MGBE_DMA_CHX_SLOT_CTRL (local_chan)
      );
  }
}

#ifdef OSI_DEBUG

/**
 * @brief Enable/disable debug interrupt
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * Algorithm:
 * - if osi_dma->ioctl_data.arg_u32 == OSI_ENABLE enable debug interrupt
 * - else disable bebug inerrupts
 */
static void
mgbe_debug_intr_config (
  struct osi_dma_priv_data  *osi_dma
  )
{
  nveu32_t  chinx;
  nveu32_t  chan;
  nveu32_t  val;
  nveu32_t  enable = osi_dma->ioctl_data.arg_u32;

  if (enable == OSI_ENABLE) {
    for (chinx = 0; chinx < osi_dma->num_dma_chans; chinx++) {
      chan = osi_dma->dma_chans[chinx];
      val  = osi_dma_readl (
               (nveu8_t *)osi_dma->base +
               MGBE_DMA_CHX_INTR_ENA (chan)
               );

      val |= (MGBE_DMA_CHX_INTR_AIE |
              MGBE_DMA_CHX_INTR_FBEE |
              MGBE_DMA_CHX_INTR_RBUE |
              MGBE_DMA_CHX_INTR_TBUE |
              MGBE_DMA_CHX_INTR_NIE);
      osi_dma_writel (
        val,
        (nveu8_t *)osi_dma->base +
        MGBE_DMA_CHX_INTR_ENA (chan)
        );
    }
  } else {
    for (chinx = 0; chinx < osi_dma->num_dma_chans; chinx++) {
      chan = osi_dma->dma_chans[chinx];
      val  = osi_dma_readl (
               (nveu8_t *)osi_dma->base +
               MGBE_DMA_CHX_INTR_ENA (chan)
               );
      val &= (~MGBE_DMA_CHX_INTR_AIE &
              ~MGBE_DMA_CHX_INTR_FBEE &
              ~MGBE_DMA_CHX_INTR_RBUE &
              ~MGBE_DMA_CHX_INTR_TBUE &
              ~MGBE_DMA_CHX_INTR_NIE);
      osi_dma_writel (
        val,
        (nveu8_t *)osi_dma->base +
        MGBE_DMA_CHX_INTR_ENA (chan)
        );
    }
  }
}

#endif

void
mgbe_init_dma_chan_ops (
  struct dma_chan_ops  *ops
  )
{
  ops->config_slot = mgbe_config_slot;
 #ifdef OSI_DEBUG
  ops->debug_intr_config = mgbe_debug_intr_config;
 #endif
}

#endif /* !OSI_STRIPPED_LIB */
