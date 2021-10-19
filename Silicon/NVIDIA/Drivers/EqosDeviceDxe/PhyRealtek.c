/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2020 - 2021, NVIDIA Corporation.  All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/


#include "PhyDxeUtil.h"
#include "PhyRealtek.h"
#include "EmacDxeUtil.h"

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/UefiBootServicesTableLib.h>

/************************************************************************************************************/
#define PAGE_A43              0xA43

/***************************************************************/
#define REG_PHYSR             26
#define PHYSR_SPEED_1000      BIT5
#define PHYSR_SPEED_100       BIT4
#define PHYSR_SPEED_MASK      (BIT5|BIT4)
#define PHYSR_DUPLEX_MODE     BIT3
#define PHYSR_LINK            BIT2

/***************************************************************/
#define REG_PHY_PAGE          31

/************************************************************************************************************/
#define PAGE_LED              0xd04

/***************************************************************/
#define REG_LCR               16
#define LCR_LED1_ACT          BIT9
#define LCR_LED1_LINK_1000    BIT8
#define LCR_LED1_LINK_100     BIT6
#define LCR_LED1_LINK_10      BIT5
#define LCR_LED0_LINK_1000    BIT3

/***************************************************************/
#define REG_EEELCR            17

/************************************************************************************************************/
// Start auto-negotiation
EFI_STATUS
EFIAPI
PhyRealtekStartAutoNeg (
  IN  PHY_DRIVER   *PhyDriver
  )
{
  UINT32        Data32;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));
  PhyDriver->AutoNegState = PHY_AUTONEG_RUNNING;

  /* Advertise 1000 MBPS full duplex mode */
  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_GB_CONTROL, &Data32);
  Data32 |= REG_PHY_GB_CONTROL_ADVERTISE_1000_BASE_T_FULL;
  PhyWrite (PhyDriver, PAGE_PHY, REG_PHY_GB_CONTROL, Data32);

  /* Advertise 100, 10 MBPS with full and half duplex mode */
  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_AUTONEG_ADVERTISE, &Data32);
  Data32 |= REG_PHY_AUTONEG_ADVERTISE_100_BASE_T4      |
            REG_PHY_AUTONEG_ADVERTISE_100_BASE_TX_FULL |
            REG_PHY_AUTONEG_ADVERTISE_100_BASE_TX_HALF |
            REG_PHY_AUTONEG_ADVERTISE_10_BASE_T_FULL   |
            REG_PHY_AUTONEG_ADVERTISE_10_BASE_T_HALF;
  PhyWrite (PhyDriver, PAGE_PHY, REG_PHY_AUTONEG_ADVERTISE, Data32);

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a: Start auto-negotiation\r\n", __FUNCTION__));

  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_CONTROL, &Data32);
  Data32 |= REG_PHY_CONTROL_AUTO_NEGOTIATION_ENABLE | REG_PHY_CONTROL_RESTART_AUTO_NEGOTIATION;

  return PhyWrite (PhyDriver, PAGE_PHY, REG_PHY_CONTROL, Data32);
}

// Check auto-negotiation completion
EFI_STATUS
EFIAPI
PhyRealtekCheckAutoNeg (
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


  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver, PAGE_PHY, REG_PHY_STATUS, &Data32);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_INFO, "Failed to read PHY_BASIC_CTRL register\r\n"));
      goto Exit;
    }
    // Wait until PHYCTRL_RESET become zero
    if ((Data32 & REG_PHY_STATUS_AUTO_NEGOTIATION_COMPLETED) != 0) {
      break;
    }
  } while (TimeoutNS > GetTimeInNanoSecond (GetPerformanceCounter ()));
  if ((Data32 & REG_PHY_STATUS_AUTO_NEGOTIATION_COMPLETED) == 0) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: ERROR! auto-negotiation timeout\n"));
    Status = EFI_TIMEOUT;
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
 * @brief Configure Realtek PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyRealtekConfig (
  IN  PHY_DRIVER   *PhyDriver
  )
{
  UINT32        Data32;
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "%s(): %u\n", __FUNCTION__, __LINE__));

  PhyDriver->PhyPageSelRegister = REG_PHY_PAGE;

  /* Enable link and activity indication for all speeds on LED1 and LED0 for GBE */
  Status = PhyRead (PhyDriver, PAGE_LED, REG_LCR, &Data32);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Data32 |= LCR_LED1_ACT |
            LCR_LED1_LINK_1000 |
            LCR_LED1_LINK_100  |
            LCR_LED1_LINK_10   |
            LCR_LED0_LINK_1000;
  Status = PhyWrite (PhyDriver, PAGE_LED, REG_LCR, Data32);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Disable Energy Efficient Ethernet (EEE) LED indication */
  return PhyWrite (PhyDriver, PAGE_LED, REG_EEELCR, 0);
}

/*
 * @brief Detect link between Realtek PHY and MAC
 *
 * @param phy PHY object
 */
VOID
EFIAPI
PhyRealtekDetectLink (
  IN  PHY_DRIVER   *PhyDriver
  )
{
  UINT32       Data32;

  PhyRead (PhyDriver, PAGE_A43, REG_PHYSR, &Data32);

  if ((Data32 & PHYSR_LINK) == 0) {
    PhyDriver->PhyCurrentLink = LINK_DOWN;
  } else {
    PhyDriver->PhyCurrentLink = LINK_UP;
  }

  if (PhyDriver->PhyOldLink != PhyDriver->PhyCurrentLink) {
    if (PhyDriver->PhyCurrentLink == LINK_UP) {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is up - Network Cable is Plugged\r\n"));
      if ((Data32 & PHYSR_DUPLEX_MODE) == 0) {
        PhyDriver->Duplex = DUPLEX_HALF;
      } else {
        PhyDriver->Duplex = DUPLEX_FULL;
      }
      if ((Data32 & PHYSR_SPEED_MASK) == PHYSR_SPEED_1000) {
        PhyDriver->Speed = SPEED_1000;
      } else if ((Data32 & PHYSR_SPEED_MASK) == PHYSR_SPEED_100) {
        PhyDriver->Speed = SPEED_100;
      } else {
        PhyDriver->Speed = SPEED_10;
      }
    } else {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is Down - Network Cable is Unplugged?\r\n"));
    }
  }
}
