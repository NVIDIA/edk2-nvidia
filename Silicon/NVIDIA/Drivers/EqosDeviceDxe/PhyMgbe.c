/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2020 - 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PhyDxeUtil.h"
#include "PhyMgbe.h"
#include "EmacDxeUtil.h"

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define REG_COPPER_STATUS  1

// MGBE

// Start auto-negotiation
EFI_STATUS
EFIAPI
PhyMGBEStartAutoNeg (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  return EFI_SUCCESS;
}

// Check auto-negotiation completion
EFI_STATUS
EFIAPI
PhyMGBECheckAutoNeg (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyMGBEConfig (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  return EFI_SUCCESS;
}

VOID
EFIAPI
PhyMGBEDetectLink (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  UINT32  Data32;

  PhyRead (PhyDriver, 0, REG_COPPER_STATUS, &Data32);

  if ((Data32 & BIT2) == 0) {
    PhyDriver->PhyCurrentLink = LINK_DOWN;
  } else {
    PhyDriver->PhyCurrentLink = LINK_UP;
  }

  // Setup speed

  if (PhyDriver->PhyCurrentLink == LINK_UP) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: Link is up - Network Cable is Plugged\r\n"));
    PhyDriver->Duplex = DUPLEX_FULL;
    PhyDriver->Speed  = SPEED_10000;
  } else {
    DEBUG ((DEBUG_INFO, "SNP:PHY: Link is Down - Network Cable is Unplugged?\r\n"));
  }
}
