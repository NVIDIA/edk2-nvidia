/** @file

  Copyright (c) 2019 - 2021, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include "EmacDxeUtil.h"
#include "PhyDxeUtil.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DmaLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>

#include "osd.h"
#include "osi_core.h"
#include "osi_dma.h"

EFI_STATUS
EFIAPI
EmacDxeInitialization (
  IN  EMAC_DRIVER   *EmacDriver,
  IN  UINTN         MacBaseAddress,
  IN  UINT32        MacType
  )
{
  EFI_STATUS             Status;
  struct osi_hw_features hw_feat;
  INT32                  OsiReturn;
  UINTN                  Index;
  UINT8                  *RxFullBuffer;
  UINT8                  *TxFullBuffer;
  UINT64                 MaxPacketSize;

  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));

  EmacDriver->tx_completed_buffer = NULL;
  EmacDriver->rxpkt_cx = NULL;
  EmacDriver->rx_pkt_swcx = NULL;

  EmacDriver->osi_core = osi_get_core ();
  if (EmacDriver->osi_core == NULL) {
    DEBUG ((DEBUG_ERROR, "unable to allocate osi_core\n"));
    return EFI_UNSUPPORTED;
  }

  EmacDriver->osi_dma = osi_get_dma ();
  if (EmacDriver->osi_dma == NULL) {
    DEBUG ((DEBUG_ERROR, "unable to allocate osi_dma\n"));
    return EFI_UNSUPPORTED;
  }

  EmacDriver->osi_core->osd = EmacDriver;
  EmacDriver->osi_dma->osd = EmacDriver;

  //Initialize the variables of osi_core
  EmacDriver->osi_core->base = (void *)MacBaseAddress;
  EmacDriver->osi_core->mac = MacType;
  EmacDriver->osi_core->num_mtl_queues = 1;
  EmacDriver->osi_core->mtl_queues[0] = 0;
  EmacDriver->osi_core->dcs_en = OSI_DISABLE;
  EmacDriver->osi_core->pause_frames = OSI_PAUSE_FRAMES_DISABLE;
  EmacDriver->osi_core->rxq_prio[0] = 2;
  EmacDriver->osi_core->rxq_ctrl[0] = 2;
  EmacDriver->osi_core->osd_ops.ops_log = osd_log;
  EmacDriver->osi_core->osd_ops.udelay = osd_udelay;
  EmacDriver->osi_core->osd_ops.usleep_range = osd_usleep_range;
  EmacDriver->osi_core->osd_ops.msleep = osd_msleep;
#ifdef OSI_DEBUG
  EmacDriver->osi_core->osd_ops.printf = osd_core_printf;
#endif

  //Initialize the variables of osi_dma
  EmacDriver->osi_dma->base = (void *)MacBaseAddress;
  EmacDriver->osi_dma->num_dma_chans = 1;
  EmacDriver->osi_dma->dma_chans[0] = 0;
  EmacDriver->osi_dma->mac = MacType;
  EmacDriver->osi_dma->mtu = OSI_DFLT_MTU_SIZE;
  EmacDriver->osi_dma->osd_ops.transmit_complete = osd_transmit_complete;
  EmacDriver->osi_dma->osd_ops.receive_packet = osd_receive_packet;
  EmacDriver->osi_dma->osd_ops.ops_log = osd_log;
  EmacDriver->osi_dma->osd_ops.udelay = osd_udelay;
#ifdef OSI_DEBUG
  EmacDriver->osi_dma->osd_ops.printf = osd_dma_printf;
#endif

  if (osi_init_core_ops(EmacDriver->osi_core) != 0) {
    DEBUG ((DEBUG_ERROR, "unable to get osi_core ops\n"));
  }

  if (osi_init_dma_ops(EmacDriver->osi_dma) != 0) {
    DEBUG ((DEBUG_ERROR, "unable to get osi_dma ops\n"));
  }

  osi_set_rx_buf_len(EmacDriver->osi_dma);
  MaxPacketSize = EmacDriver->osi_dma->rx_buf_len;

  osi_get_hw_features(EmacDriver->osi_core, &hw_feat);

  //Allocate TX DMA resources
  EmacDriver->osi_dma->tx_ring[0] = AllocateZeroPool(sizeof(struct osi_tx_ring));
  if (EmacDriver->osi_dma->tx_ring[0] == NULL) {
    DEBUG((DEBUG_ERROR, "ENOMEM for tx_ring\n"));
    return EFI_OUT_OF_RESOURCES;
  }
  EmacDriver->osi_dma->tx_ring[0]->tx_swcx = AllocateZeroPool(sizeof(struct osi_tx_swcx) * (unsigned long)TX_DESC_CNT);
  if (EmacDriver->osi_dma->tx_ring[0]->tx_swcx == NULL) {
    DEBUG ((DEBUG_ERROR, "ENOMEM for tx_ring[0]->swcx\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DmaAllocateBuffer (EfiBootServicesData,
                              EFI_SIZE_TO_PAGES(sizeof(struct osi_tx_desc) * (unsigned long)TX_DESC_CNT),
                              (VOID *)&EmacDriver->osi_dma->tx_ring[0]->tx_desc);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to DMA alloc for Tx desc ring\n"));
    return Status;
  }

  EmacDriver->osi_dma->tx_ring[0]->tx_desc_phy_addr = (UINTN)EmacDriver->osi_dma->tx_ring[0]->tx_desc;

  //Allocate RX DMA resources
  EmacDriver->osi_dma->rx_ring[0] = AllocateZeroPool(sizeof(struct osi_rx_ring));
  if (EmacDriver->osi_dma->rx_ring[0] == NULL) {
    DEBUG ((DEBUG_ERROR, "ENOMEM for rx_ring\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  EmacDriver->osi_dma->rx_ring[0]->rx_swcx = AllocateZeroPool(sizeof(struct osi_rx_swcx) * (unsigned long)RX_DESC_CNT);
  if (EmacDriver->osi_dma->rx_ring[0]->rx_swcx == NULL) {
    DEBUG ((DEBUG_ERROR, "ENOMEM for rx_ring[0]->swcx\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DmaAllocateBuffer (EfiBootServicesData,
                              EFI_SIZE_TO_PAGES(sizeof(struct osi_rx_desc) * (unsigned long)RX_DESC_CNT),
                              (VOID *)&EmacDriver->osi_dma->rx_ring[0]->rx_desc);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to DMA alloc for Rx desc ring\n"));
    return Status;
  }

  EmacDriver->osi_dma->rx_ring[0]->rx_desc_phy_addr = (UINTN)EmacDriver->osi_dma->rx_ring[0]->rx_desc;

  //Allocate Rx buffers
  Status = DmaAllocateBuffer (EfiBootServicesData,
                              EFI_SIZE_TO_PAGES(MaxPacketSize * RX_DESC_CNT), (VOID *)&RxFullBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to DMA alloc Rx buffers\n"));
    return Status;
  }
  for (Index = 0; Index < RX_DESC_CNT; Index++) {
    EmacDriver->osi_dma->rx_ring[0]->rx_swcx[Index].buf_virt_addr = RxFullBuffer + (MaxPacketSize * Index);
    EmacDriver->osi_dma->rx_ring[0]->rx_swcx[Index].buf_phy_addr = (UINTN)EmacDriver->osi_dma->rx_ring[0]->rx_swcx[Index].buf_virt_addr;
  }

  //Allocate Tx buffers
  Status = DmaAllocateBuffer (EfiBootServicesData,
                              EFI_SIZE_TO_PAGES(MaxPacketSize * TX_DESC_CNT), (VOID *)&TxFullBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to DMA alloc Tx buffers\n"));
    return Status;
  }
  for (Index = 0; Index < TX_DESC_CNT; Index++) {
    EmacDriver->tx_buffers[Index] = TxFullBuffer + (MaxPacketSize * Index);
  }

  osi_poll_for_mac_reset_complete ( EmacDriver->osi_core );

  // Init EMAC DMA
  OsiReturn = osi_hw_dma_init( EmacDriver->osi_dma);
  if (OsiReturn < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to initialize MAC DMA\n"));
    Status = EFI_DEVICE_ERROR;
  } else {
    OsiReturn = osi_hw_core_init( EmacDriver->osi_core, hw_feat.tx_fifo_size, hw_feat.rx_fifo_size);
    if (OsiReturn < 0) {
      DEBUG ((DEBUG_ERROR, "Failed to initialize MAC Core: %d\n", OsiReturn));
      Status = EFI_DEVICE_ERROR;
    }
  }

  return Status;
}
