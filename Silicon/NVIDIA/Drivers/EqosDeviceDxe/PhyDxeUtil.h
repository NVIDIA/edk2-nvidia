/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef _PHY_DXE_H__
#define _PHY_DXE_H__

#include <Protocol/EmbeddedGpio.h>

typedef struct {
  UINT32             PhyPage;
  UINT32             PhyCurrentLink;
  UINT32             PhyOldLink;
  EFI_HANDLE         ControllerHandle;
  EMBEDDED_GPIO_PIN  ResetPin;
  EMBEDDED_GPIO_MODE ResetMode0;
  EMBEDDED_GPIO_MODE ResetMode1;
} PHY_DRIVER;


/************************************************************************************************************/
#define NON_EXISTENT_ON_PRESIL                        0xDEADBEEF

#define PAGE_COPPER                                   0

#define REG_COPPER_CONTROL                            0
#define COPPER_CONTROL_RESET                          BIT15
#define COPPER_CONTROL_ENABLE_AUTO_NEG                BIT12
#define COPPER_RESTART_AUTO_NEG                       BIT9

#define REG_COPPER_STATUS                             1

#define REG_COPPER_AUTO_NEG_ADVERTISEMENT             4

#define REG_COPPER_LINK_PARTNER_ABILITY               5

#define REG_COPPER_AUTO_NEG_EXPANSION                 6

#define REG_1000_BASE_T_STATUS                        10

#define REG_COPPER_CONTROL1                           16
#define COPPER_CONTROL1_ENABLE_AUTO_CROSSOVER         (BIT6|BIT5)

#define REG_COPPER_STATUS1                            17
#define COPPER_STATUS1_SPEED_SHIFT                    14
#define COPPER_STATUS1_SPEED_MASK                     (BIT14|BIT15)
/*
 * bits 15, 14
 * 00 = 10 Mbps
 * 01 = 100 Mbps
 * 10 = 1000 Mbps
 */
#define COPPER_STATUS1_SPEED_10_MBPS                  0
#define COPPER_STATUS1_SPEED_100_MBPS                 BIT14
#define COPPER_STATUS1_SPEED_1000_MBPS                BIT15
#define COPPER_STATUS1_DUPLEX_MODE                    BIT13
#define COPPER_STATUS1_LINK_STATUS                    BIT10

#define REG_COPPER_INTR_STATUS                        19
#define COPPER_INTR_STATUS_AUTO_NEG_COMPLETED         BIT11

/************************************************************************************************************/
#define PAGE_MAC                                      2

#define REG_MAC_CONTROL1                              16
#define MAC_CONTROL1_ENABLE_RX_CLK                    BIT10
#define MAC_CONTROL1_PASS_ODD_NIBBLE_PREAMBLES        BIT6
#define MAC_CONTROL1_RGMII_INTF_POWER_DOWN            BIT3
#define MAC_CONTROL1_TX_FIFO_DEPTH_16_BITS            0
#define MAC_CONTROL1_TX_FIFO_DEPTH_24_BITS            BIT14
#define MAC_CONTROL1_TX_FIFO_DEPTH_32_BITS            BIT15
#define MAC_CONTROL1_TX_FIFO_DEPTH_40_BITS            (BIT15|BIT14)

#define REG_MAC_CONTROL2                              21
/*
 * Bits 6, 13
 * 00 = 10 Mbps
 * 01 = 100 Mbps
 * 10 = 1000 Mbps
 */
#define MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_10_MBPS   0
#define MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_100_MBPS  BIT13
#define MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_1000_MBPS BIT6
#define MAC_CONTROL2_RGMII_RX_TIMING_CTRL             BIT5
#define MAC_CONTROL2_RGMII_TX_TIMING_CTRL             BIT4

/************************************************************************************************************/
#define REG_PHY_PAGE                                  22

#define PHY_ID                                        0
#define MAC_MDIO_ADDR_OFFSET                          0x200
#define MAC_MDIO_ADDR_PA_SHIFT                        21
#define MAC_MDIO_ADDR_RDA_SHIFT                       16
#define MAC_MDIO_ADDR_CR_SHIFT                        8
#define MAC_MDIO_ADDR_CR_20_35                        2
#define MAC_MDIO_ADDR_GOC_SHIFT                       2
#define MAC_MDIO_ADDR_GOC_READ                        3
#define MAC_MDIO_ADDR_GOC_WRITE                       1
#define MAC_MDIO_ADDR_GB                              BIT0

#define MAC_MDIO_DATA_OFFSET                          0x204

#define SPEED_1000                            1000
#define SPEED_100                             100
#define SPEED_10                              10

#define DUPLEX_FULL                           1
#define DUPLEX_HALF                           0


#define LINK_UP                               1
#define LINK_DOWN                             0
#define PHY_TIMEOUT                           200000


EFI_STATUS
EFIAPI
PhyDxeInitialization (
  IN  PHY_DRIVER     *PhyDriver,
  IN  UINTN          MacBaseAddress
  );

EFI_STATUS
EFIAPI
PhySoftReset (
  IN  PHY_DRIVER    *PhyDriver,
  IN  UINTN         MacBaseAddress
  );

EFI_STATUS
EFIAPI
PhyLinkAdjustEmacConfig (
  IN  PHY_DRIVER    *PhyDriver,
  IN  UINTN         MacBaseAddress
  );

#endif /* _PHY_DXE_H__ */
