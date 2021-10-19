/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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

#ifndef INCLUDED_DMA_DEBUG_H
#define INCLUDED_DMA_DEBUG_H

#include <osi_common.h>
#include <osi_dma.h>
#include "hw_desc.h"
#include "../osi/common/common.h"
#include "dma_local.h"

/**
 * @addtogroup DESC-DUMP helper macros.
 *
 * @brief Helper macros used for debugging.
 * @{
 */
#define TX_DESC_DUMP		OSI_BIT(0)
#define RX_DESC_DUMP		OSI_BIT(1)
#define TXRX_DESC_DUMP_MASK     (OSI_BIT(0) | OSI_BIT(1))
#define TX_DESC_DUMP_TX		OSI_BIT(2)
#define TX_DESC_DUMP_TX_DONE	OSI_BIT(3)
#define TX_DESC_DUMP_MASK	(OSI_BIT(2) | OSI_BIT(3))
/** @} */

void desc_dump(struct osi_dma_priv_data *osi_dma, unsigned int f_idx,
	       unsigned int l_idx, unsigned int flag, unsigned int chan);
void reg_dump(struct osi_dma_priv_data *osi_dma);
void structs_dump(struct osi_dma_priv_data *osi_dma);
#endif /* INCLUDED_DMA_DEBUG_H*/
