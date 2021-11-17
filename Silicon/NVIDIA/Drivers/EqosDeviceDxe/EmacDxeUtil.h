/** @file

  Copyright (c) 2019 - 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EMAC_DXE_UTIL_H__
#define EMAC_DXE_UTIL_H__

#include <Protocol/SimpleNetwork.h>
#include "osi_core.h"
#include "osi_dma.h"

typedef struct {
  struct osi_core_priv_data    *osi_core;
  struct osi_dma_priv_data     *osi_dma;
  void                         *tx_buffers[TX_DESC_CNT];
  void                         *tx_completed_buffer;
  struct osi_rx_pkt_cx         *rxpkt_cx;
  struct osi_rx_swcx           *rx_pkt_swcx;
} EMAC_DRIVER;

EFI_STATUS
EFIAPI
EmacDxeInitialization (
  IN  EMAC_DRIVER  *EmacDriver,
  IN  UINTN        MacBaseAddress,
  IN  UINTN        XpcsBaseAddress,
  IN  UINT32       MacType
  );

#endif // EMAC_DXE_UTIL_H__
