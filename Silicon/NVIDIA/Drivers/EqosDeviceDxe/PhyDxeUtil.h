/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2020 - 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PHY_DXE_H__
#define _PHY_DXE_H__

#include <Protocol/EmbeddedGpio.h>
#include "EmacDxeUtil.h"

typedef struct _PHY_DRIVER PHY_DRIVER;

typedef
EFI_STATUS
(EFIAPI *NVIDIA_EQOS_PHY_CONFIG)(
  IN  PHY_DRIVER   *PhyDriver
  );

typedef
EFI_STATUS
(EFIAPI *NVIDIA_EQOS_PHY_AUTO_NEG)(
  IN  PHY_DRIVER   *PhyDriver
  );

typedef
VOID
(EFIAPI *NVIDIA_EQOS_PHY_DETECT_LINK)(
  IN  PHY_DRIVER   *PhyDriver
  );

struct _PHY_DRIVER {
  UINT32                         PhyPage;
  UINT32                         PhyPageSelRegister;
  UINT32                         PhyCurrentLink;
  UINT32                         PhyOldLink;
  UINT32                         Speed;
  UINT32                         Duplex;
  EFI_HANDLE                     ControllerHandle;
  EMBEDDED_GPIO_PIN              ResetPin;
  EMBEDDED_GPIO_MODE             ResetMode0;
  EMBEDDED_GPIO_MODE             ResetMode1;
  NVIDIA_EQOS_PHY_CONFIG         Config;
  NVIDIA_EQOS_PHY_AUTO_NEG       StartAutoNeg;
  NVIDIA_EQOS_PHY_AUTO_NEG       CheckAutoNeg;
  NVIDIA_EQOS_PHY_DETECT_LINK    DetectLink;
  UINT8                          AutoNegState;
  EMAC_DRIVER                    *MacDriver;
  UINT32                         PhyAddress;
  UINT32                         ResetDelay;
  UINT32                         PostResetDelay;
  BOOLEAN                        MgbeDevice;
};

#define PHY_AUTONEG_IDLE     0
#define PHY_AUTONEG_RUNNING  1
#define PHY_AUTONEG_TIMEOUT  2

#define PAGE_PHY  0

#define REG_PHY_CONTROL                           0
#define REG_PHY_CONTROL_RESET                     BIT15
#define REG_PHY_CONTROL_AUTO_NEGOTIATION_ENABLE   BIT12
#define REG_PHY_CONTROL_RESTART_AUTO_NEGOTIATION  BIT9

#define REG_PHY_STATUS                             1
#define REG_PHY_STATUS_AUTO_NEGOTIATION_COMPLETED  BIT5

#define REG_PHY_IDENTIFIER_1  2

#define REG_PHY_IDENTIFIER_2        3
#define REG_PHY_IDENTIFIER_2_WIDTH  ((15 - 10) + 1)
#define REG_PHY_IDENTIFIER_2_SHIFT  10

#define REG_PHY_AUTONEG_ADVERTISE                   4
#define REG_PHY_AUTONEG_ADVERTISE_100_BASE_T4       BIT9
#define REG_PHY_AUTONEG_ADVERTISE_100_BASE_TX_FULL  BIT8
#define REG_PHY_AUTONEG_ADVERTISE_100_BASE_TX_HALF  BIT7
#define REG_PHY_AUTONEG_ADVERTISE_10_BASE_T_FULL    BIT6
#define REG_PHY_AUTONEG_ADVERTISE_10_BASE_T_HALF    BIT5

#define REG_PHY_GB_CONTROL                             9
#define REG_PHY_GB_CONTROL_ADVERTISE_1000_BASE_T_FULL  BIT9

/************************************************************************************************************/
#define NON_EXISTENT_ON_PLATFORM  0xDEADBEEF

#define SPEED_10000  10000
#define SPEED_1000   1000
#define SPEED_100    100
#define SPEED_10     10

#define DUPLEX_FULL  1
#define DUPLEX_HALF  0

#define LINK_UP      1
#define LINK_DOWN    0
#define PHY_TIMEOUT  200000

#define PHY_DEFAULT_ADDRESS                0
#define PHY_DEFAULT_RESET_DELAY_USEC       0
#define PHY_DEFAULT_POST_RESET_DELAY_USEC  0
#define PHY_PAGE_SWITCH_DELAY_USEC         20

// Clocks section
#define TX_CLK_RATE_1G    125000000
#define TX_CLK_RATE_100M  25000000
#define TX_CLK_RATE_10M   2500000

#define ETHER_MGBE_TX_CLK_USXGMII_10G      644531250UL
#define ETHER_MGBE_TX_PCS_CLK_USXGMII_10G  156250000UL
#define ETHER_MGBE_MAC_DIV_RATE_10G        312500000UL

#define ETHER_MGBE_RX_PCS_CLK_USXGMII_10G  156250000UL
#define ETHER_RX_INPUT_CLK_RATE            125000000UL
#define ETHER_MGBE_RX_PCS_CLK_USXGMII_10G  156250000UL

// need this?
#define ETHER_MGBE_RX_CLK_USXGMII_10G  644531250UL

#define ETHER_EEE_PCS_CLK_RATE      102000000UL
#define ETHER_MGBE_APP_CLK_RATE     480000000UL
#define ETHER_MGBE_PTP_REF_CLK_10G  312500000UL

EFI_STATUS
EFIAPI
PhyRead (
  IN PHY_DRIVER  *PhyDriver,
  IN  UINT32     Page,
  IN  UINT32     Reg,
  OUT UINT32     *Data
  );

// Function to write to the MII register (PHY Access)
EFI_STATUS
EFIAPI
PhyWrite (
  IN PHY_DRIVER  *PhyDriver,
  IN UINT32      Page,
  IN UINT32      Reg,
  IN UINT32      Data
  );

EFI_STATUS
EFIAPI
PhyDxeInitialization (
  IN  PHY_DRIVER   *PhyDriver,
  IN  EMAC_DRIVER  *MacDriver
  );

EFI_STATUS
EFIAPI
PhySoftReset (
  IN  PHY_DRIVER  *PhyDriver
  );

EFI_STATUS
EFIAPI
PhyLinkAdjustEmacConfig (
  IN  PHY_DRIVER  *PhyDriver
  );

#endif /* _PHY_DXE_H__ */
