/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "EmacDxeUtil.h"
#include "PhyDxeUtil.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DmaLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>

#include "osi_core.h"
#include "osi_dma.h"

VOID
EFIAPI
EmacSetMacAddress (
  IN  EFI_MAC_ADDRESS   *MacAddress,
  IN  UINTN             MacBaseAddress
  )
{
  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));

  // Note: This MAC_ADDR0 registers programming sequence cannot be swap:
  // Must program HIGH Offset first before LOW Offset
  // because synchronization is triggered when MAC Address0 Low Register are written.
  MmioWrite32 (MacBaseAddress + MAC_ADDRESS0_HIGH_OFFSET,
               (UINT32)(MacAddress->Addr[4] & 0xFF) |
                      ((MacAddress->Addr[5] & 0xFF) << 8)
                    );
  // MacAddress->Addr[0,1,2] is the 3 bytes OUI
  MmioWrite32 (MacBaseAddress + MAC_ADDRESS0_LOW_OFFSET,
                       (MacAddress->Addr[0] & 0xFF) |
                      ((MacAddress->Addr[1] & 0xFF) << 8) |
                      ((MacAddress->Addr[2] & 0xFF) << 16) |
                      ((MacAddress->Addr[3] & 0xFF) << 24)
                    );

  DEBUG ((DEBUG_INFO, "SNP:MAC: gmacgrp_mac_address0_low  = 0x%08X \r\n",
          MmioRead32 (MacBaseAddress + MAC_ADDRESS0_LOW_OFFSET)));
  DEBUG ((DEBUG_INFO, "SNP:MAC: gmacgrp_mac_address0_high = 0x%08X \r\n",
          MmioRead32 (MacBaseAddress +MAC_ADDRESS0_HIGH_OFFSET)));
}


VOID
EFIAPI
EmacReadMacAddress (
  OUT  EFI_MAC_ADDRESS   *MacAddress,
  IN   UINTN             MacBaseAddress
  )
{
  UINT32          MacAddrHighValue;
  UINT32          MacAddrLowValue;

  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));

  // Read the Mac Addr high register
  MacAddrHighValue = (MmioRead32 (MacBaseAddress + MAC_ADDRESS0_HIGH_OFFSET) & 0xFFFF);
  // Read the Mac Addr low register
  MacAddrLowValue = MmioRead32 (MacBaseAddress + MAC_ADDRESS0_LOW_OFFSET);

  ZeroMem (MacAddress, sizeof(*MacAddress));
  MacAddress->Addr[0] = (MacAddrLowValue & 0xFF);
  MacAddress->Addr[1] = (MacAddrLowValue & 0xFF00) >> 8;
  MacAddress->Addr[2] = (MacAddrLowValue & 0xFF0000) >> 16;
  MacAddress->Addr[3] = (MacAddrLowValue & 0xFF000000) >> 24;
  MacAddress->Addr[4] = (MacAddrHighValue & 0xFF);
  MacAddress->Addr[5] = (MacAddrHighValue & 0xFF00) >> 8;

  DEBUG ((DEBUG_INFO, "SNP:MAC: MAC Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n",
    MacAddress->Addr[0], MacAddress->Addr[1], MacAddress->Addr[2],
    MacAddress->Addr[3], MacAddress->Addr[4], MacAddress->Addr[5]
  ));
}

EFI_STATUS
EFIAPI
EmacDmaInit (
  IN  EMAC_DRIVER   *EmacDriver,
  IN  UINTN         MacBaseAddress
  )
{
  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));

  // Software reset

  MmioOr32 (MacBaseAddress +
             DMA_MODE_OFFSET,
             DMA_MODE_SWR);

  while ((MmioRead32 (MacBaseAddress + DMA_MODE_OFFSET) && DMA_MODE_SWR) == DMA_MODE_SWR);

  /* Program AXI interface burst length and read, write outstanding request limit */
  MmioWrite32 (MacBaseAddress +
                DMA_SYSBUS_MODE_OFFSET,
                DMA_SYSBUS_MODE_EAME |
                DMA_SYSBUS_MODE_BLEN16 |
                DMA_SYSBUS_MODE_BLEN8 |
                DMA_SYSBUS_MODE_BLEN256 |
                (0xF << DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT) |
                (0xF << DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT));

  EmacSetupTxdesc (EmacDriver, MacBaseAddress);
  EmacSetupRxdesc (EmacDriver, MacBaseAddress);

  /* Set receive buffer size */
  MmioAndThenOr32 (MacBaseAddress + DMA_CH0_RX_CONTROL_OFFSET,
                   ~DMA_CH0_RX_CONTROL_RBSZ_MASK,
                   CONFIG_ETH_BUFSIZE << DMA_CH0_RX_CONTROL_RBSZ_SHIFT);

  /* Enable OSP mode */
  MmioOr32 (MacBaseAddress + DMA_CH0_TX_CONTROL_OFFSET, DMA_CH0_TX_CONTROL_OSF);

  /* Set Programmable Burst Lengths */
  MmioAndThenOr32 (MacBaseAddress + DMA_CH0_TX_CONTROL_OFFSET,
                   ~DMA_CH0_TX_CONTROL_TXPBL_MASK,
                   32 << DMA_CH0_TX_CONTROL_TXPBL_SHIFT);
  MmioAndThenOr32 (MacBaseAddress + DMA_CH0_RX_CONTROL_OFFSET,
                   ~DMA_CH0_RX_CONTROL_RXPBL_MASK,
                   8 << DMA_CH0_RX_CONTROL_RXPBL_SHIFT);

  /* Enable receive and transmit interrupts */
  MmioOr32 (MacBaseAddress + DMA_CH0_INTERRUPT_ENABLE_OFFSET,
      DMA_CH0_INTERRUPT_ENABLE_TIE |
      DMA_CH0_INTERRUPT_ENABLE_RIE |
      DMA_CH0_INTERRUPT_ENABLE_NIE );

  /* Enable DMA */
  MmioOr32 (MacBaseAddress + DMA_CH0_TX_CONTROL_OFFSET, DMA_CH0_TX_CONTROL_ST);
  MmioOr32 (MacBaseAddress + DMA_CH0_RX_CONTROL_OFFSET, DMA_CH0_RX_CONTROL_SR);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EmacMtlInit (
  IN  EMAC_DRIVER   *EmacDriver,
  IN  UINTN         MacBaseAddress
  )
{
  UINT32 Value;
  UINT32 Tqs;
  UINT32 Rqs;
  UINT32 TxFifoSize;
  UINT32 TxFifoSizeBytes;
  UINT32 RxFifoSize;
  UINT32 RxFifoSizeBytes;

  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));

  /* Tx scheduling algorithm */
  MmioAndThenOr32 (MacBaseAddress + MTL_OPERATION_MODE_OFFSET,
                     ~MTL_OPERATION_MODE_SCHALG_MASK,
                      MTL_OPERATION_MODE_SCHALG_STRICT);

  /* Rx queue and DMA channel mapping */
  MmioAndThenOr32 (MacBaseAddress + MTL_RXQ_DMA_MAP0_OFFSET,
                     ~(MTL_RXQ_DMA_MAP0_Q0MDMACH_MASK |
                       MTL_RXQ_DMA_MAP0_Q1MDMACH_MASK |
                       MTL_RXQ_DMA_MAP0_Q2MDMACH_MASK |
                       MTL_RXQ_DMA_MAP0_Q3MDMACH_MASK ),
                       ((MTL_RXQ_DMA_MAP0_QxMDMACH_DMA_CHANNEL_0 << MTL_RXQ_DMA_MAP0_Q0MDMACH_SHIFT) |
                        (MTL_RXQ_DMA_MAP0_QxMDMACH_DMA_CHANNEL_1 << MTL_RXQ_DMA_MAP0_Q1MDMACH_SHIFT) |
                        (MTL_RXQ_DMA_MAP0_QxMDMACH_DMA_CHANNEL_2 << MTL_RXQ_DMA_MAP0_Q2MDMACH_SHIFT) |
                        (MTL_RXQ_DMA_MAP0_QxMDMACH_DMA_CHANNEL_3 << MTL_RXQ_DMA_MAP0_Q3MDMACH_SHIFT))) ;

  /* Flush transmit queue */
  MmioOr32 (MacBaseAddress + MTL_TXQ0_OPERATION_MODE_OFFSET, MTL_TXQ0_OPERATION_MODE_FTQ);
  /* Wait till flush completes */
  while ((MmioRead32 (MacBaseAddress + MTL_TXQ0_OPERATION_MODE_OFFSET) & MTL_TXQ0_OPERATION_MODE_FTQ) != 0);

  /* Transmit queue operating modes */
  /* Start transmission only when a full packet resides in the Tx queue */
  /* Enable the transmit queue */
  MmioAndThenOr32 (MacBaseAddress + MTL_TXQ0_OPERATION_MODE_OFFSET,
                   ~(MTL_TXQ0_OPERATION_MODE_TSF|MTL_TXQ0_OPERATION_MODE_TXQEN_MASK),
                   MTL_TXQ0_OPERATION_MODE_TSF|MTL_TXQ0_OPERATION_MODE_TXQEN_ENABLED);

  /* Receive queue operating modes */
  /* Read a packet from the Rx queue only after the complete packet has been wriiten to it */
  MmioOr32 (MacBaseAddress + MTL_RXQ0_OPERATION_MODE_OFFSET, MTL_RXQ0_OPERATION_MODE_RSF);

  /* Get Transmit and Receive FIFO size */
  Value = MmioRead32 (MacBaseAddress + MAC_HW_FEATURE_1_OFFSET);
  TxFifoSize = (Value & MAC_HW_FEATURE_1_TXFIFOSIZE_MASK) >> MAC_HW_FEATURE_1_TXFIFOSIZE_SHIFT;
  RxFifoSize = (Value & MAC_HW_FEATURE_1_RXFIFOSIZE_MASK) >> MAC_HW_FEATURE_1_RXFIFOSIZE_SHIFT;

  /* t/rx_fifo_sz is encoded as log2(n / 128). Undo that by shifting. */
  TxFifoSizeBytes = (128 << TxFifoSize);
  RxFifoSizeBytes = (128 << RxFifoSize);
  /* t/rqs is encoded as log2(t/rx_fifo_size_bytes / 256) */
  Tqs = (TxFifoSizeBytes/256) - 1;
  Rqs = (RxFifoSizeBytes/256) - 1;

  /* Set Transmit and Receive FIFO size */
  MmioAndThenOr32 (MacBaseAddress + MTL_TXQ0_OPERATION_MODE_OFFSET,
                   ~(MTL_TXQ0_OPERATION_MODE_TQS_MASK),
                   (Tqs << MTL_TXQ0_OPERATION_MODE_TQS_SHIFT));
  MmioAndThenOr32 (MacBaseAddress + MTL_RXQ0_OPERATION_MODE_OFFSET,
                   ~(MTL_RXQ0_OPERATION_MODE_RQS_MASK),
                   (Rqs << MTL_RXQ0_OPERATION_MODE_RQS_SHIFT));

  MmioOr32 (MacBaseAddress + MTL_RXQ0_OPERATION_MODE_OFFSET, MTL_RXQ0_OPERATION_MODE_EHFC);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EmacMacInit (
  IN  EMAC_DRIVER   *EmacDriver,
  IN  UINTN         MacBaseAddress
  )
{
  /*
   * Configure MAC:
   * enable full duplex mode
   * allow jumbo packets of 9018 bytes
   * disable jabber timer
   * minimum inter-packet gap between packets during transmission
   */
  MmioAndThenOr32 (MacBaseAddress + MAC_CONFIGURATION_OFFSET,
                     ~(MAC_CONFIGURATION_DM|MAC_CONFIGURATION_JE|MAC_CONFIGURATION_JD|
                         MAC_CONFIGURATION_ACS|MAC_CONFIGURATION_CST),
                     (MAC_CONFIGURATION_DM|MAC_CONFIGURATION_JE|MAC_CONFIGURATION_JD|
                         MAC_CONFIGURATION_ACS|MAC_CONFIGURATION_CST));
  MmioAndThenOr32 (MacBaseAddress + MAC_CONFIGURATION_OFFSET,
                     ~(MAC_CONFIGURATION_IPG_MASK),
                     (MAC_CONFIGURATION_IPG_40_BIT_TIMES << MAC_CONFIGURATION_IPG_SHIFT));

  /* Filter broadcast packets */
  MmioWrite32 (MacBaseAddress + MAC_PACKET_FILTER_OFFSET, MAC_PACKET_FILTER_HPF | MAC_PACKET_FILTER_HMC);

  /* Enable RX queue 0 for DCB or generic */
  MmioWrite32 (MacBaseAddress + MAC_RXQ_CTRL0_OFFSET, MAC_RXQ_CTRL0_FIELD_DCB << MAC_RXQ_CTRL0_RXQ0EN_SHIFT);

  /* Route all multicast, broadcast, AV, DCB and untagged packets to the Rx queue 0 */
  MmioWrite32 (MacBaseAddress + MAC_RXQ_CTRL1_OFFSET, 0);

  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
EmacDxeInitialization (
  IN  EMAC_DRIVER   *EmacDriver,
  IN  UINTN         MacBaseAddress
  )
{
  EFI_STATUS Status;
  struct osi_core_priv_data *osi_core;
  struct osi_dma_priv_data *osi_dma;
  struct osi_hw_features hw_feat;
  int ret;
  unsigned int i;
  UINTN tx_desc_size, tx_swcx_size;
  UINTN rx_desc_size, rx_swcx_size, rx_buf_len;

  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));

  // Allocate OSI resources
  osi_core = AllocateZeroPool (sizeof (*osi_core));
  if (osi_core == NULL) {
    DEBUG ((DEBUG_ERROR, "unable to allocate osi_core\n"));
    return EFI_UNSUPPORTED;
  } else {
    EmacDriver->osi_core = osi_core;
  }

  osi_dma = AllocateZeroPool (sizeof (*osi_dma));
  if (osi_dma == NULL) {
    DEBUG ((DEBUG_ERROR, "unable to allocate osi_dma\n"));
    return EFI_UNSUPPORTED;
  } else {
    EmacDriver->osi_dma = osi_dma;
  }

  osi_core->osd = EmacDriver;
  osi_dma->osd = EmacDriver;

  //Initialize the variables of osi_core
  osi_core->mac = OSI_MAC_HW_EQOS;
  osi_core->num_mtl_queues = 1;
  osi_core->mtl_queues[0] = 0;
  osi_core->dcs_en = OSI_DISABLE;
  osi_core->pause_frames = OSI_PAUSE_FRAMES_DISABLE;
  osi_core->rxq_prio[0] = 0;
  osi_core->rxq_ctrl[0] = 2;

  //Initialize the variables of osi_dma
  osi_dma->num_dma_chans = 1;
  osi_dma->dma_chans[0] = 0;
  osi_dma->mac = OSI_MAC_HW_EQOS;
  osi_dma->mtu = OSI_DFLT_MTU_SIZE;

  if (osi_init_core_ops(osi_core) != 0) {
    DEBUG ((DEBUG_ERROR, "unable to get osi_core ops\n"));
  }

  if (osi_init_dma_ops(osi_dma) != 0) {
    DEBUG ((DEBUG_ERROR, "unable to get osi_dma ops\n"));
  }

  osi_set_rx_buf_len(osi_dma);
  osi_core->base = (void *)MacBaseAddress;
  osi_dma->base = (void *)MacBaseAddress;

  osi_get_hw_features(osi_core->base, &hw_feat);
  EmacDriver->hw_feat = hw_feat;

  //Allocate TX DMA resources
  tx_desc_size = sizeof(struct osi_tx_desc) * (unsigned long)TX_DESC_CNT;
  tx_swcx_size = sizeof(struct osi_tx_swcx) * (unsigned long)TX_DESC_CNT;

  osi_dma->tx_ring[0] = AllocateZeroPool(sizeof(struct osi_tx_ring));
  if (osi_dma->tx_ring[0] == NULL) {
    DEBUG((DEBUG_ERROR, "ENOMEM for tx_ring\n"));
    return EFI_OUT_OF_RESOURCES;
  }
  osi_dma->tx_ring[0]->tx_swcx = AllocateZeroPool(tx_swcx_size);
  if (osi_dma->tx_ring[0]->tx_swcx == NULL) {
    DEBUG ((DEBUG_ERROR, "ENOMEM for tx_ring[0]->swcx\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DmaAllocateBuffer (EfiBootServicesData, tx_desc_size, (VOID *)&osi_dma->tx_ring[0]->tx_desc);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to DMA alloc for Tx desc ring\n"));
    return Status;
  }

  Status = DmaMap (MapOperationBusMasterCommonBuffer, osi_dma->tx_ring[0]->tx_desc,
                   &tx_desc_size, (EFI_PHYSICAL_ADDRESS *)&osi_dma->tx_ring[0]->tx_desc_phy_addr, &EmacDriver->tx_ring_dma_mapping);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to DMA Map for Tx desc ring\n"));
    return Status;
  }

  //Allocate RX DMA resources
  rx_desc_size = sizeof(struct osi_rx_desc) * (unsigned long)RX_DESC_CNT;
  rx_swcx_size = sizeof(struct osi_rx_swcx) * (unsigned long)RX_DESC_CNT;

  osi_dma->rx_ring[0] = AllocateZeroPool(sizeof(struct osi_rx_ring));
  if (osi_dma->rx_ring[0] == NULL) {
    DEBUG ((DEBUG_ERROR, "ENOMEM for rx_ring\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  osi_dma->rx_ring[0]->rx_swcx = AllocateZeroPool(rx_swcx_size);
  if (osi_dma->rx_ring[0]->rx_swcx == NULL) {
    DEBUG ((DEBUG_ERROR, "ENOMEM for rx_ring[0]->swcx\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DmaAllocateBuffer (EfiBootServicesData, rx_desc_size, (VOID *)&osi_dma->rx_ring[0]->rx_desc);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to DMA alloc for Rx desc ring\n"));
    return Status;
  }

  Status = DmaMap (MapOperationBusMasterCommonBuffer, osi_dma->rx_ring[0]->rx_desc,
                   &rx_desc_size, (EFI_PHYSICAL_ADDRESS *)&osi_dma->rx_ring[0]->rx_desc_phy_addr, &EmacDriver->rx_ring_dma_mapping);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to DMA Map for Rx desc ring\n"));
    return Status;
  }

  //Allocate Rx buffers
  struct osi_rx_swcx *rx_swcx;
  rx_buf_len = osi_dma->rx_buf_len;
  for (i = 0; i < RX_DESC_CNT; i++) {
    rx_swcx = osi_dma->rx_ring[0]->rx_swcx + i;
    Status = DmaAllocateBuffer (EfiBootServicesData, rx_buf_len, (VOID *)&rx_swcx->buf_virt_addr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to DMA alloc Rx buffers\n"));
      return Status;
    }

    Status = DmaMap (MapOperationBusMasterCommonBuffer, rx_swcx->buf_virt_addr, &rx_buf_len,
                     (EFI_PHYSICAL_ADDRESS *)&rx_swcx->buf_phy_addr, &EmacDriver->rx_buffer_dma_mapping[i]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to DMA map Rx buffers\n"));
      return Status;
    }
  }

  //Allocate Tx buffers
  struct osi_tx_swcx *tx_swcx;
  UINTN tx_buf_len = CONFIG_ETH_BUFSIZE;
  for (i = 0; i < TX_DESC_CNT; i++) {
    tx_swcx = osi_dma->tx_ring[0]->tx_swcx + i;
    Status = DmaAllocateBuffer (EfiBootServicesData, tx_buf_len, (VOID *)&EmacDriver->tx_buffers[i]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to DMA alloc Tx buffers\n"));
      return Status;
    }

    Status = DmaMap (MapOperationBusMasterCommonBuffer, EmacDriver->tx_buffers[i],
                     &tx_buf_len, (EFI_PHYSICAL_ADDRESS *)&EmacDriver->tx_buffers_phy_addr[i],
                     &EmacDriver->tx_buffer_dma_mapping[i]);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to DMA map Tx buffers\n"));
      return Status;
    }
  }

  // Init EMAC DMA
  ret = osi_hw_dma_init(osi_dma);
  if (ret < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to initialize MAC DMA\n"));
  } else {
    ret = osi_hw_core_init(osi_core, hw_feat.tx_fifo_size, hw_feat.rx_fifo_size);
    if (ret < 0) {
      DEBUG ((DEBUG_ERROR, "Failed to initialize MAC Core: %d\n", ret));
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EmacSetupTxdesc (
  IN  EMAC_DRIVER   *EmacDriver,
  IN  UINTN         MacBaseAddress
 )
{
  UINT32 HighAddress;
  UINT32 LowAddress;
  UINTN  Address;
  ZeroMem (EmacDriver->TxdescRing, sizeof (DESIGNWARE_HW_DESCRIPTOR)*CONFIG_TX_DESCR_NUM);

  Address = EmacDriver->TxdescRingMap.AddrMap;
  HighAddress = ((Address & 0xFFFFFFFF00000000) >> 32);
  LowAddress = (Address & 0xFFFFFFFF0);
  MmioWrite32 (MacBaseAddress + DMA_CH0_TXDESC_RING_LENGTH_OFFSET, CONFIG_TX_DESCR_NUM-1);
  MmioWrite32 (MacBaseAddress + DMA_CH0_TXDESC_LIST_HADDRESS_OFFSET, HighAddress);
  MmioWrite32 (MacBaseAddress + DMA_CH0_TXDESC_LIST_ADDRESS_OFFSET, LowAddress);

  Address = EmacDriver->TxdescRingMap.AddrMap + ((CONFIG_TX_DESCR_NUM-1) * sizeof (DESIGNWARE_HW_DESCRIPTOR));
  LowAddress = (Address & 0xFFFFFFFF);
  MmioWrite32 (MacBaseAddress + DMA_CH0_TXDESC_TAIL_POINTER_OFFSET, LowAddress);

  EmacDriver->TxCurrentDescriptorNum = 0;
  EmacDriver->TxNextDescriptorNum = 0;
  EmacDriver->TxRecycledBufferNum = 0;
  ZeroMem (EmacDriver->TxBuffers, sizeof(EmacDriver->TxBuffers));

  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
EmacSetupRxdesc (
  IN  EMAC_DRIVER   *EmacDriver,
  IN  UINTN         MacBaseAddress
  )
{
  INTN                        Index;
  EFI_PHYSICAL_ADDRESS        Address;
  UINT32                      HighAddress;
  UINT32                      LowAddress;

  for (Index = 0; Index < CONFIG_RX_DESCR_NUM; Index++) {
    Address = EmacDriver->RxBufferRingMap.AddrMap + (Index * CONFIG_ETH_BUFSIZE);
    HighAddress = ((Address & 0xFFFFFFFF00000000) >> 32);
    LowAddress = (Address & 0xFFFFFFFF0);

    EmacDriver->RxdescRing[Index].Des0 = LowAddress;
    EmacDriver->RxdescRing[Index].Des1 = HighAddress;
    EmacDriver->RxdescRing[Index].Des2 = 0;
    EmacDriver->RxdescRing[Index].Des3 = RDES_3_READ_OWN | RDES_3_READ_IOC | RDES_3_READ_BUF1V;
  }

  Address = EmacDriver->RxdescRingMap.AddrMap;
  HighAddress = ((Address & 0xFFFFFFFF00000000) >> 32);
  LowAddress = (Address & 0xFFFFFFFF0);
  MmioWrite32 (MacBaseAddress + DMA_CH0_RXDESC_RING_LENGTH_OFFSET, CONFIG_RX_DESCR_NUM-1);
  MmioWrite32 (MacBaseAddress + DMA_CH0_RXDESC_LIST_HADDRESS_OFFSET, HighAddress);
  MmioWrite32 (MacBaseAddress + DMA_CH0_RXDESC_LIST_ADDRESS_OFFSET, LowAddress);

  Address = EmacDriver->RxdescRingMap.AddrMap + ((CONFIG_RX_DESCR_NUM-1) * sizeof (DESIGNWARE_HW_DESCRIPTOR));
  LowAddress = (Address & 0xFFFFFFFF);
  MmioWrite32 (MacBaseAddress + DMA_CH0_RXDESC_TAIL_POINTER_OFFSET, LowAddress);

  EmacDriver->RxCurrentDescriptorNum = 0;
  EmacDriver->RxNextDescriptorNum = 0;

  return EFI_SUCCESS;
}


VOID
EFIAPI
EmacStartTransmission (
  IN  UINTN   MacBaseAddress
  )
{
  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));
  MmioOr32 (MacBaseAddress +
            MAC_CONFIGURATION_OFFSET,
            MAC_CONFIGURATION_RE |
            MAC_CONFIGURATION_TE
            );
}


EFI_STATUS
EFIAPI
EmacRxFilters (
  IN  UINT32            ReceiveFilterSetting,
  IN  BOOLEAN           Reset,
  IN  UINTN             NumMfilter          OPTIONAL,
  IN  EFI_MAC_ADDRESS   *Mfilter            OPTIONAL,
  IN  UINTN             MacBaseAddress
  )
{
  UINT32  MacFilter;
  UINT32  Crc;
  UINT32  Count;
  UINT32  HashReg;
  UINT32  HashBit;
  UINT32  Reg;
  UINT32  Val;

  // If reset then clear the filter registers
  if (Reset) {
    for (Count = 0; Count < NumMfilter; Count++)
    {
      MmioWrite32 (MacBaseAddress + HASH_TABLE_REG(Count), 0x00000000);
    }
  }

  MacFilter =  MAC_PACKET_FILTER_HPF | MAC_PACKET_FILTER_HMC;

  if (ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST) {
    // Set the hash tables
    if ((NumMfilter > 0) && (!Reset)) {
      // Go through each filter address and set appropriate bits on hash table
      for (Count = 0; Count < NumMfilter; Count++) {
        // Generate a 32-bit CRC
        Crc = GenEtherCrc32 (&Mfilter[Count], 6);
        // reserve CRC + take upper 8 bit = take lower 8 bit and reverse it
        Val = BitReverse(Crc & 0xff);
        // The most significant bits determines the register to be used (Hash Table Register X),
        // and the least significant five bits determine the bit within the register.
        // For example, a hash value of 8b'10111111 selects Bit 31 of the Hash Table Register 5.
        HashReg = (Val >> 5);
        HashBit = (Val & 0x1f);

        Reg = MmioRead32 (MacBaseAddress + HASH_TABLE_REG(HashReg));
        // set 1 to HashBit of HashReg
        // for example, set 1 to bit 31 to Reg 5 as in above example
        Reg |= (1 << HashBit);
        MmioWrite32(MacBaseAddress + HASH_TABLE_REG(HashReg), Reg);
      }
    }
  }

  if ((ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST) == 0) {
    MacFilter |=  MAC_PACKET_FILTER_DBF;
  }

  if (ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS) {
    MacFilter |=  MAC_PACKET_FILTER_PR;
  }

  if (ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST) {
    MacFilter |= ( MAC_PACKET_FILTER_PM);
  }

  // Set MacFilter to EMAC register
  MmioWrite32 (MacBaseAddress +  MAC_PACKET_FILTER_OFFSET, MacFilter);
  return EFI_SUCCESS;
}


UINT32
EFIAPI
GenEtherCrc32 (
  IN  EFI_MAC_ADDRESS   *Mac,
  IN  UINT32            AddrLen
  )
{
  INT32   Iter;
  UINT32  Remainder;
  UINT8   *Ptr;

  Iter = 0;
  Remainder = 0xFFFFFFFF;    // 0xFFFFFFFF is standard seed for Ethernet

  // Convert Mac Address to array of bytes
  Ptr = (UINT8 *)Mac;

  // Generate the Crc bit-by-bit (LSB first)
  while (AddrLen--) {
    Remainder ^= *Ptr++;
    for (Iter = 0; Iter < 8; Iter++) {
      // Check if exponent is set
      if (Remainder & 1) {
        Remainder = (Remainder >> 1) ^ CRC_POLYNOMIAL;
      } else {
        Remainder = (Remainder >> 1) ^ 0;
      }
    }
  }

  return (~Remainder);
}


STATIC CONST UINT8 NibbleTab[] = {
    /* 0x0 0000 -> 0000 */  0x0,
    /* 0x1 0001 -> 1000 */  0x8,
    /* 0x2 0010 -> 0100 */  0x4,
    /* 0x3 0011 -> 1100 */  0xc,
    /* 0x4 0100 -> 0010 */  0x2,
    /* 0x5 0101 -> 1010 */  0xa,
    /* 0x6 0110 -> 0110 */  0x6,
    /* 0x7 0111 -> 1110 */  0xe,
    /* 0x8 1000 -> 0001 */  0x1,
    /* 0x9 1001 -> 1001 */  0x9,
    /* 0xa 1010 -> 0101 */  0x5,
    /* 0xb 1011 -> 1101 */  0xd,
    /* 0xc 1100 -> 0011 */  0x3,
    /* 0xd 1101 -> 1011 */  0xb,
    /* 0xe 1110 -> 0111 */  0x7,
    /* 0xf 1111 -> 1111 */  0xf
};

UINT8
EFIAPI
BitReverse (
  UINT8   Value
  )
{
  return (NibbleTab[Value & 0xf] << 4) | NibbleTab[Value >> 4];
}


VOID
EFIAPI
EmacStopTxRx (
   IN  UINTN   MacBaseAddress
  )
{
  UINT32 Value;
  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));

  // Stop DMA TX
  MmioAnd32 (MacBaseAddress +
             DMA_CH0_TX_CONTROL_OFFSET,
             ~DMA_CH0_TX_CONTROL_ST);

  //Wait for transmissions to complete
  while (TRUE) {
    Value = MmioRead32 (MacBaseAddress + MTL_TXQ0_DEBUG_OFFSET);
    if (((Value & MTL_TXQ0_DEBUG_TRCSTS_MASK) != (0x01 << MTL_TXQ0_DEBUG_TRCSTS_SHIFT)) &&
        ((Value & MTL_TXQ0_DEBUG_TXQSTS) == 0)) {
      break;
    }
  }

  //Disable MAC transmitter and receiver
  MmioAnd32 (MacBaseAddress +
             MAC_CONFIGURATION_OFFSET,
             ~(MAC_CONFIGURATION_RE|MAC_CONFIGURATION_TE));


  //Wait for Rx FIFO done
  while (TRUE) {
    Value = MmioRead32 (MacBaseAddress + MTL_RXQ0_DEBUG_OFFSET);
    if (((Value & MTL_RXQ0_DEBUG_PRXQ_MASK) == 0x00) &&
        ((Value & MTL_RXQ0_DEBUG_RXQSTS_MASK) == 0)) {
      break;
    }
  }

  //Disable Receive DMA
  MmioAnd32 (MacBaseAddress +
             DMA_CH0_RX_CONTROL_OFFSET,
             ~DMA_CH0_RX_CONTROL_SR);

  //Wait for Tx and Rx queue are empty
  while (TRUE) {
    Value = MmioRead32 (MacBaseAddress + MTL_TXQ0_DEBUG_OFFSET);
    if ((Value & MTL_TXQ0_DEBUG_TXQSTS) == 0) {
      break;
    }
  }
  while (TRUE) {
    Value = MmioRead32 (MacBaseAddress + MTL_RXQ0_DEBUG_OFFSET);
    if ((Value & MTL_RXQ0_DEBUG_RXQSTS_MASK) == 0) {
      break;
    }
  }
}


EFI_STATUS
EFIAPI
EmacDmaResume (
  IN  EMAC_DRIVER   *EmacDriver,
  IN  UINTN   MacBaseAddress,
  IN  BOOLEAN Transmit
  )
{
  UINT32 LowAddress;
  UINTN  Address;
  MemoryFence ();
  if (Transmit) {
    Address = EmacDriver->TxdescRingMap.AddrMap + ((CONFIG_TX_DESCR_NUM-1) * sizeof (DESIGNWARE_HW_DESCRIPTOR));
    LowAddress = (Address & 0xFFFFFFFF);
    MmioWrite32 (MacBaseAddress + DMA_CH0_TXDESC_TAIL_POINTER_OFFSET, LowAddress);
  } else {
    Address = EmacDriver->RxdescRingMap.AddrMap + (EmacDriver->RxCurrentDescriptorNum * sizeof (DESIGNWARE_HW_DESCRIPTOR));
    LowAddress = (Address & 0xFFFFFFFF);
    MmioWrite32 (MacBaseAddress + DMA_CH0_RXDESC_TAIL_POINTER_OFFSET, LowAddress);
  }
  return EFI_SUCCESS;
}


VOID
EFIAPI
EmacGetDmaStatus (
  OUT  UINT32   *IrqStat  OPTIONAL,
  IN   UINTN    MacBaseAddress
  )
{
  UINT32  DmaStatus;

  if(IrqStat != NULL) {
    *IrqStat = 0;
    DmaStatus = MmioRead32 (MacBaseAddress +
                            DMA_CH0_STATUS_OFFSET);
    if (DmaStatus & DMA_CH0_STATUS_NIS) {
      // Rx interrupt
      if (DmaStatus & DMA_CH0_STATUS_RI) {
        *IrqStat |= EFI_SIMPLE_NETWORK_RECEIVE_INTERRUPT;
      }
      // Tx interrupt
      if (DmaStatus & DMA_CH0_STATUS_TI) {
        *IrqStat |= EFI_SIMPLE_NETWORK_TRANSMIT_INTERRUPT;
      }
    }
    MmioOr32 (MacBaseAddress +
              DMA_CH0_STATUS_OFFSET,
              DMA_CH0_STATUS_NIS|DMA_CH0_STATUS_RI|DMA_CH0_STATUS_TI);
  }
}


VOID
EFIAPI
EmacGetStatistic (
  OUT  EFI_NETWORK_STATISTICS   *Statistic,
  IN   UINTN                     MacBaseAddress
  )
{
  DEBUG ((DEBUG_INFO, "SNP:MAC: %a ()\r\n", __FUNCTION__));

  Statistic->RxTotalFrames     = MmioRead32 (MacBaseAddress + RX_PACKETS_COUNT_GOOD_BAD_OFFSET);
  Statistic->RxUndersizeFrames = MmioRead32 (MacBaseAddress + RX_UNDERSIZE_PACKETS_GOOD_OFFSET);
  Statistic->RxOversizeFrames  = MmioRead32 (MacBaseAddress + RX_OVERSIZE_PACKETS_GOOD_OFFSET);
  Statistic->RxUnicastFrames   = MmioRead32 (MacBaseAddress + RX_UNICAST_PACKETS_GOOD_OFFSET);
  Statistic->RxBroadcastFrames = MmioRead32 (MacBaseAddress + RX_BROADCAST_PACKETS_GOOD_OFFSET);
  Statistic->RxMulticastFrames = MmioRead32 (MacBaseAddress + RX_MULTICAST_PACKETS_GOOD_OFFSET);
  Statistic->RxCrcErrorFrames  = MmioRead32 (MacBaseAddress + RX_CRC_ERROR_PACKETS_OFFSET);
  Statistic->RxTotalBytes      = MmioRead32 (MacBaseAddress + RX_OCTET_COUNT_GOOD_BAD_OFFSET);
  Statistic->RxGoodFrames      = Statistic->RxUnicastFrames +
                                 Statistic->RxBroadcastFrames +
                                 Statistic->RxMulticastFrames;

  Statistic->TxTotalFrames     = MmioRead32 (MacBaseAddress + TX_PACKETS_COUNT_GOOD_BAD_OFFSET);
  Statistic->TxGoodFrames      = MmioRead32 (MacBaseAddress + TX_PACKET_COUNT_GOOD_OFFSET);
  Statistic->TxOversizeFrames  = MmioRead32 (MacBaseAddress + TX_OVERSIZE_PACKETS_GOOD_OFFSET);
  Statistic->TxUnicastFrames   = MmioRead32 (MacBaseAddress + TX_UNICAST_PACKETS_GOOD_OFFSET);
  Statistic->TxBroadcastFrames = MmioRead32 (MacBaseAddress + TX_BROADCAST_PACKETS_GOOD_OFFSET);
  Statistic->TxMulticastFrames = MmioRead32 (MacBaseAddress + TX_MULTICAST_PACKETS_GOOD_OFFSET);
  Statistic->TxTotalBytes      = MmioRead32 (MacBaseAddress + TX_OCTET_COUNT_GOOD_BAD_OFFSET);
  Statistic->Collisions        = MmioRead32 (MacBaseAddress + TX_LATE_COLLISION_PACKETS_OFFSET) +
                                 MmioRead32 (MacBaseAddress + TX_EXCESSIVE_COLLISION_PACKETS_OFFSET);
}


VOID
EFIAPI
EmacConfigAdjust (
  IN  UINT32   Speed,
  IN  UINT32   Duplex,
  IN  UINTN    MacBaseAddress
  )
{
  UINT32   Config;

  Config = MAC_CONFIGURATION_BE | MAC_CONFIGURATION_DO;
  if (Speed != SPEED_1000) {
   Config |= MAC_CONFIGURATION_PS;
  }
  if (Speed == SPEED_100) {
    Config |= MAC_CONFIGURATION_FES;
  }
  if (Duplex == DUPLEX_FULL) {
    Config |= MAC_CONFIGURATION_DM;
  }
  MmioAndThenOr32 (MacBaseAddress + MAC_CONFIGURATION_OFFSET,
                   ~(MAC_CONFIGURATION_BE |
                     MAC_CONFIGURATION_DO |
                     MAC_CONFIGURATION_PS |
                     MAC_CONFIGURATION_FES |
                     MAC_CONFIGURATION_DM),
                     Config);
}
