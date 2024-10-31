/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.
  Copyright (c) 2023, Connect Tech Inc. All rights reserved.
  SPDX-FileCopyrightText: Copyright (c) 2020-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PhyDxeUtil.h"
#include "PhyMicrel.h"
#include "EmacDxeUtil.h"

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <libfdt.h>

/***************************************************************/
#define REG_PHY_CTRL          0x1F
#define PHY_CTRL_SPEED_1000   BIT6
#define PHY_CTRL_SPEED_100    BIT5
#define PHY_CTRL_SPEED_10     BIT4
#define PHY_CTRL_SPEED_MASK   (BIT6|BIT5|BIT4)
#define PHY_CTRL_DUPLEX_MODE  BIT3

#define PHY_STATUS_LINK  BIT2

#define REG_PHY_1000T_STATUS  0x0A

#define PAGE_RGMII_TIMING  2

#define REG_PHY_CTRL_SKEW     0x4
#define REG_PHY_RX_DATA_SKEW  0x5
#define REG_PHY_TX_DATA_SKEW  0x6
#define REG_PHY_CLK_SKEW      0x8

#define REG_FLP_BURST_TX_LO  0x3
#define REG_FLP_BURST_TX_HI  0x4

typedef struct {
  UINT16    CtrlSkew;
  UINT16    DataTxSkew;
  UINT16    DataRxSkew;
  UINT16    ClkSkew;
  UINT16    FLPBurstTxHi;
  UINT16    FLPBurstTxLo;
} MICREL_TIMING_VALUES;

static const MICREL_TIMING_VALUES  TimingsKSZ9031Orin =
{
  .CtrlSkew     = 0x0077,
  .DataTxSkew   = 0x7777,
  .DataRxSkew   = 0x7777,
  .ClkSkew      = 0x0379,
  .FLPBurstTxHi = 0x0006,
  .FLPBurstTxLo = 0x1A80,
};

static const MICREL_TIMING_VALUES  TimingsKSZ9031Xavier =
{
  .CtrlSkew     = 0x0007,
  .DataTxSkew   = 0x0000,
  .DataRxSkew   = 0x7777,
  .ClkSkew      = 0x03F9,
  .FLPBurstTxHi = 0x0006,
  .FLPBurstTxLo = 0x1A80,
};

// Start auto-negotiation
EFI_STATUS
EFIAPI
PhyMicrelStartAutoNeg (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  UINT32  Data32;

  PhyDriver->AutoNegState = PHY_AUTONEG_RUNNING;

  /* Advertise 1000 MBPS full duplex mode */
  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_GB_CONTROL, &Data32);
  Data32 |= REG_PHY_GB_CONTROL_ADVERTISE_1000_BASE_T_FULL;
  PhyWrite (PhyDriver, PAGE_PHY, REG_PHY_GB_CONTROL, Data32);

  /* Selector Field: 0x1 = IEEE 802.3 */
  Data32  = 0x1;
  Data32 |= REG_PHY_AUTONEG_ADVERTISE_100_BASE_TX_FULL |
            REG_PHY_AUTONEG_ADVERTISE_100_BASE_TX_HALF |
            REG_PHY_AUTONEG_ADVERTISE_10_BASE_T_FULL   |
            REG_PHY_AUTONEG_ADVERTISE_10_BASE_T_HALF;

  PhyWrite (PhyDriver, PAGE_PHY, REG_PHY_AUTONEG_ADVERTISE, Data32);

  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_CONTROL, &Data32);
  Data32 |= REG_PHY_CONTROL_AUTO_NEGOTIATION_ENABLE | REG_PHY_CONTROL_RESTART_AUTO_NEGOTIATION;

  return PhyWrite (PhyDriver, PAGE_PHY, REG_PHY_CONTROL, Data32);
}

// Check auto-negotiation completion
EFI_STATUS
EFIAPI
PhyMicrelCheckAutoNeg (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  UINT64      TimeoutNS;
  UINT32      Data32;
  EFI_STATUS  Status;

  if (PhyDriver->AutoNegState == PHY_AUTONEG_IDLE) {
    return EFI_SUCCESS;
  }

  // Only check once if we are in timeout state
  if (PhyDriver->AutoNegState == PHY_AUTONEG_TIMEOUT) {
    TimeoutNS = GetTimeInNanoSecond (GetPerformanceCounter ());
  } else {
    TimeoutNS = GetTimeInNanoSecond (GetPerformanceCounter ()) + (PHY_TIMEOUT * 1000);
  }

  do {
    // Read PHY_STATUS register
    Status = PhyRead (PhyDriver, PAGE_PHY, REG_PHY_STATUS, &Data32);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Micrel Failed to read PHY_STATUS register\r\n"));
      goto Exit;
    }

    // Check Auto-Negotiation Complete bit
    if ((Data32 & REG_PHY_STATUS_AUTO_NEGOTIATION_COMPLETED) != 0) {
      break;
    }
  } while (TimeoutNS > GetTimeInNanoSecond (GetPerformanceCounter ()));

  if ((Data32 & REG_PHY_STATUS_AUTO_NEGOTIATION_COMPLETED) == 0) {
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

EFI_STATUS
EFIAPI
PhyMicrelGetRGMIITimings (
  IN  PHY_DRIVER            *PhyDriver,
  OUT MICREL_TIMING_VALUES  *Timings
  )
{
  UINTN  ChipID;

  if ((PhyDriver == NULL) || (Timings == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // TODO: Add device tree parsing support

  ChipID = TegraGetChipID ();

  if (ChipID == T234_CHIP_ID) {
    CopyMem (Timings, &TimingsKSZ9031Orin, sizeof (MICREL_TIMING_VALUES));
  } else {
    DEBUG ((DEBUG_ERROR, "Micrel: %a: Unsupported Chip ID %X\n", __FUNCTION__, ChipID));
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyMicrelSetTimings (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  EFI_STATUS            Status;
  MICREL_TIMING_VALUES  Timings;

  Status = PhyMicrelGetRGMIITimings (PhyDriver, &Timings);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PhyWrite (PhyDriver, PAGE_RGMII_TIMING, REG_PHY_CTRL_SKEW, Timings.CtrlSkew);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PhyWrite (PhyDriver, PAGE_RGMII_TIMING, REG_PHY_RX_DATA_SKEW, Timings.DataRxSkew);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PhyWrite (PhyDriver, PAGE_RGMII_TIMING, REG_PHY_TX_DATA_SKEW, Timings.DataTxSkew);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PhyWrite (PhyDriver, PAGE_RGMII_TIMING, REG_PHY_CLK_SKEW, Timings.ClkSkew);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PhyWrite (PhyDriver, PAGE_PHY, REG_FLP_BURST_TX_HI, Timings.FLPBurstTxHi);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PhyWrite (PhyDriver, PAGE_PHY, REG_FLP_BURST_TX_LO, Timings.FLPBurstTxLo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

/*
 * @brief Configure Micrel PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMicrelConfig (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  EFI_STATUS  Status;

  Status = PhyMicrelSetTimings (PhyDriver);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Micrel: %a: Failed to Set RGMII Timings\n", __FUNCTION__));
    return Status;
  }

  return EFI_SUCCESS;
}

/*
 * @brief Detect link between Micrel PHY and MAC
 *
 * @param phy PHY object
 */
VOID
EFIAPI
PhyMicrelDetectLink (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  UINT32      Data32;
  EFI_STATUS  Status;

  Status = PhyRead (PhyDriver, PAGE_PHY, REG_PHY_1000T_STATUS, &Data32);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Micrel: Failed to read 1000T_STATUS register\r\n"));
    return;
  }

  // If idle error maxed out the KSZ needs a reset
  if ((Data32 & 0xFF) == 0xFF) {
    DEBUG ((DEBUG_ERROR, "Micrel: Idle error maxed, resetting\r\n"));
    PhySoftReset (PhyDriver);
    return;
  }

  Status = PhyRead (PhyDriver, PAGE_PHY, REG_PHY_STATUS, &Data32);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Micrel: Failed to read PHY_STATUS register\r\n"));
    return;
  }

  if ((Data32 & PHY_STATUS_LINK) == 0) {
    PhyDriver->PhyCurrentLink = LINK_DOWN;
  } else {
    PhyDriver->PhyCurrentLink = LINK_UP;
  }

  Status = PhyRead (PhyDriver, PAGE_PHY, REG_PHY_CTRL, &Data32);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Micrel: Failed to read PHY_CTRL register\r\n"));
    return;
  }

  if (PhyDriver->PhyOldLink != PhyDriver->PhyCurrentLink) {
    if (PhyDriver->PhyCurrentLink == LINK_UP) {
      if ((Data32 & PHY_CTRL_DUPLEX_MODE) == 0) {
        PhyDriver->Duplex = DUPLEX_HALF;
      } else {
        PhyDriver->Duplex = DUPLEX_FULL;
      }

      if ((Data32 & PHY_CTRL_SPEED_MASK) == PHY_CTRL_SPEED_1000) {
        PhyDriver->Speed = SPEED_1000;
      } else if ((Data32 & PHY_CTRL_SPEED_MASK) == PHY_CTRL_SPEED_100) {
        PhyDriver->Speed = SPEED_100;
      } else if ((Data32 & PHY_CTRL_SPEED_MASK) == PHY_CTRL_SPEED_10) {
        PhyDriver->Speed = SPEED_10;
      } else {
        PhyDriver->Speed = SPEED_10;
      }

      DEBUG ((
        DEBUG_ERROR,
        "Micrel: Link is up, Speed %dMbps %a Duplex\r\n",
        PhyDriver->Speed,
        (PhyDriver->Duplex == DUPLEX_FULL ? "FULL" : "HALF")
        ));
    } else {
      DEBUG ((DEBUG_ERROR, "Micrel: Link is Down\r\n"));
    }
  }
}
