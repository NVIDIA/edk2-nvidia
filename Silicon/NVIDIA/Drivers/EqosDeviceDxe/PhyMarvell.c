/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2020 - 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include "PhyDxeUtil.h"
#include "PhyMarvell.h"
#include "EmacDxeUtil.h"

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/UefiBootServicesTableLib.h>

/************************************************************************************************************/

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

// Start auto-negotiation
EFI_STATUS
EFIAPI
PhyMarvellStartAutoNeg (
  IN  PHY_DRIVER   *PhyDriver
  )
{
  UINT32        Data32;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  PhyDriver->AutoNegState = PHY_AUTONEG_RUNNING;
  PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_CONTROL, &Data32);
  Data32 |= COPPER_CONTROL_ENABLE_AUTO_NEG | COPPER_RESTART_AUTO_NEG | COPPER_CONTROL_RESET;

  return PhyWrite (PhyDriver, PAGE_COPPER, REG_COPPER_CONTROL, Data32);
}

// Check auto-negotiation completion
EFI_STATUS
EFIAPI
PhyMarvellCheckAutoNeg (
  IN  PHY_DRIVER   *PhyDriver
  )
{
  UINT64        TimeoutNS;
  UINT32        Data32;
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  if (PhyDriver->AutoNegState == PHY_AUTONEG_IDLE) {
    return EFI_SUCCESS;
  }

  //Only check once if we are in timeout state
  if (PhyDriver->AutoNegState == PHY_AUTONEG_TIMEOUT) {
    TimeoutNS = GetTimeInNanoSecond (GetPerformanceCounter ());
  } else {
    TimeoutNS = GetTimeInNanoSecond (GetPerformanceCounter ()) + (PHY_TIMEOUT * 1000);
  }

  // Wait for completion
  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_CONTROL, &Data32);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to read PHY_BASIC_CTRL register\r\n"));
      goto Exit;
    }
    // Wait until PHYCTRL_RESET become zero
    if ((Data32 & COPPER_CONTROL_RESET) == 0) {
      break;
    }
  } while (TimeoutNS > GetTimeInNanoSecond (GetPerformanceCounter ()));
  if ((Data32 & COPPER_CONTROL_RESET) != 0) {
    DEBUG ((DEBUG_INFO , "SNP:PHY: ERROR! PhySoftReset timeout\n"));
    Status = EFI_TIMEOUT;
    goto Exit;
  }

  //Only check once if we are in timeout state
  if (PhyDriver->AutoNegState == PHY_AUTONEG_TIMEOUT) {
    TimeoutNS = GetTimeInNanoSecond (GetPerformanceCounter ());
  } else {
    TimeoutNS = GetTimeInNanoSecond (GetPerformanceCounter ()) + (PHY_TIMEOUT * 1000);
  }
  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_INTR_STATUS, &Data32);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to read PHY_BASIC_CTRL register\r\n"));
      goto Exit;
    }
    // Wait until PHYCTRL_RESET become zero
    if ((Data32 & COPPER_INTR_STATUS_AUTO_NEG_COMPLETED) != 0) {
      break;
    }
  } while (TimeoutNS > GetTimeInNanoSecond (GetPerformanceCounter ()));
  if ((Data32 & COPPER_INTR_STATUS_AUTO_NEG_COMPLETED) == 0) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: ERROR! auto-negotiation timeout\n"));
    Status = EFI_TIMEOUT;
    goto Exit;
  }

Exit:
  if (!EFI_ERROR (Status)) {
    PhyDriver->AutoNegState = PHY_AUTONEG_IDLE;
  } else if (Status == EFI_TIMEOUT) {
    PhyDriver->AutoNegState = PHY_AUTONEG_TIMEOUT;
  }

  return Status;
}

/*
 * @brief Configure Marvell PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMarvellConfig (
  IN  PHY_DRIVER   *PhyDriver
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  PhyDriver->PhyPageSelRegister = REG_PHY_PAGE;

  /* Program Page: 2, Register: 0 */
  PhyWrite(PhyDriver, PAGE_MAC, REG_COPPER_CONTROL, 0);

  Status = PhySoftReset (PhyDriver);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Program Page: 2, Register: 16 */
  Status = PhyWrite(PhyDriver, PAGE_MAC,
           REG_MAC_CONTROL1,
           MAC_CONTROL1_TX_FIFO_DEPTH_24_BITS |
           MAC_CONTROL1_ENABLE_RX_CLK |
           MAC_CONTROL1_PASS_ODD_NIBBLE_PREAMBLES |
           MAC_CONTROL1_RGMII_INTF_POWER_DOWN);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Program Page: 2, Register: 21 */
  Status = PhyWrite(PhyDriver, PAGE_MAC,
           REG_MAC_CONTROL2,
           MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_1000_MBPS |
           MAC_CONTROL2_RGMII_RX_TIMING_CTRL |
           MAC_CONTROL2_RGMII_TX_TIMING_CTRL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Program Page: 0, Register: 16 */
  /* Automatically detect whether it needs to crossover between pairs or not */
  return PhyWrite(PhyDriver, PAGE_COPPER,
         REG_COPPER_CONTROL1,
         COPPER_CONTROL1_ENABLE_AUTO_CROSSOVER);
}

/*
 * @brief Detect link between Marvell PHY and MAC
 *
 * @param phy PHY object
 */
VOID
EFIAPI
PhyMarvellDetectLink (
  IN  PHY_DRIVER   *PhyDriver
  )
{
  UINT32       Data32;

  if (PhyDriver->PhyOldLink == LINK_DOWN) {
    PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_INTR_STATUS, &Data32);
    if ((Data32 & 0xC00) == 0xC00) {
      PhyDriver->StartAutoNeg (PhyDriver);
      PhyDriver->CheckAutoNeg (PhyDriver);
    }
  }

  PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_STATUS1, &Data32);

  if ((Data32 & COPPER_STATUS1_LINK_STATUS) == 0) {
    PhyDriver->PhyCurrentLink = LINK_DOWN;
  } else {
    PhyDriver->PhyCurrentLink = LINK_UP;
  }

  if (PhyDriver->PhyOldLink != PhyDriver->PhyCurrentLink) {
    if (PhyDriver->PhyCurrentLink == LINK_UP) {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is up - Network Cable is Plugged\r\n"));
      if ((Data32 & COPPER_STATUS1_DUPLEX_MODE) == 0) {
        PhyDriver->Duplex = DUPLEX_HALF;
      } else {
        PhyDriver->Duplex = DUPLEX_FULL;
      }
      if ((Data32 & COPPER_STATUS1_SPEED_1000_MBPS) != 0) {
        PhyDriver->Speed = SPEED_1000;
      } else if ((Data32 & COPPER_STATUS1_SPEED_100_MBPS) != 0) {
        PhyDriver->Speed = SPEED_100;
      } else {
        PhyDriver->Speed = SPEED_10;
      }
    } else {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is Down - Network Cable is Unplugged?\r\n"));
    }
  }
}
