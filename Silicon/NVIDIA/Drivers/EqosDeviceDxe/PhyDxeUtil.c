/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.
  Copyright (c) 2020 - 2021, NVIDIA Corporation.  All rights reserved.

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
#include "EmacDxeUtil.h"

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "PhyMarvell.h"
#include "PhyRealtek.h"

STATIC
EFI_STATUS
EFIAPI
PhyReset (
  IN PHY_DRIVER   *PhyDriver
  )
{
  EFI_STATUS    Status;
  EMBEDDED_GPIO *GpioProtocol = NULL;

  if (PhyDriver->ResetPin == NON_EXISTENT_ON_PLATFORM) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gEmbeddedGpioProtocolGuid, NULL, (VOID **)&GpioProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to locate gpio protocol %r\r\n", Status));
    return Status;
  }

  Status = GpioProtocol->Set (GpioProtocol, PhyDriver->ResetPin, PhyDriver->ResetMode0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set gpio %x to %d %r\r\n", PhyDriver->ResetPin, PhyDriver->ResetMode0, Status));
    return Status;
  }
  MicroSecondDelay(PhyDriver->ResetDelay);

  Status = GpioProtocol->Set (GpioProtocol, PhyDriver->ResetPin, PhyDriver->ResetMode1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set gpio %x to %d %r\r\n", PhyDriver->ResetPin, PhyDriver->ResetMode1, Status));
    return Status;
  }
  MicroSecondDelay(PhyDriver->PostResetDelay);
  return PhySoftReset (PhyDriver);
}

// Function to read from MII register (PHY Access)
EFI_STATUS
EFIAPI
PhyRead (
  IN PHY_DRIVER   *PhyDriver,
  IN  UINT32       Page,
  IN  UINT32       Reg,
  OUT UINT32      *Data
  )
{
  INT32 OsiStatus;

  if ((PhyDriver->PhyPage != Page) &&
      (PhyDriver->PhyPageSelRegister != 0)) {
    osi_write_phy_reg(PhyDriver->MacDriver->osi_core, PhyDriver->PhyAddress, PhyDriver->PhyPageSelRegister, Page);
    MicroSecondDelay(PHY_PAGE_SWITCH_DELAY_USEC);
    PhyDriver->PhyPage = Page;
  }

  OsiStatus = osi_read_phy_reg(PhyDriver->MacDriver->osi_core, PhyDriver->PhyAddress, Reg);
  if (OsiStatus == -1) {
    return EFI_DEVICE_ERROR;
  }
  *Data = OsiStatus;
  return EFI_SUCCESS;
}


// Function to write to the MII register (PHY Access)
EFI_STATUS
EFIAPI
PhyWrite (
  IN PHY_DRIVER   *PhyDriver,
  IN UINT32        Page,
  IN UINT32        Reg,
  IN UINT32        Data
  )
{
  INT32 OsiStatus;
  if ((PhyDriver->PhyPage != Page) &&
      (PhyDriver->PhyPageSelRegister != 0)) {
    osi_write_phy_reg(PhyDriver->MacDriver->osi_core, PhyDriver->PhyAddress, PhyDriver->PhyPageSelRegister, Page);
    MicroSecondDelay(PHY_PAGE_SWITCH_DELAY_USEC);
    PhyDriver->PhyPage = Page;
  }

  OsiStatus = osi_write_phy_reg(PhyDriver->MacDriver->osi_core, PhyDriver->PhyAddress, Reg, Data);
  if (OsiStatus != 0) {
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}

// Perform PHY software reset
EFI_STATUS
EFIAPI
PhySoftReset (
  IN PHY_DRIVER   *PhyDriver
  )
{
  UINT32        TimeOut;
  UINT32        Data32;
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  // PHY Basic Control Register reset
  PhyWrite (PhyDriver, PAGE_PHY, REG_PHY_CONTROL, REG_PHY_CONTROL_RESET);

  // Wait for completion
  TimeOut = 0;
  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver, PAGE_PHY, REG_PHY_CONTROL, &Data32);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    // Wait until PHYCTRL_RESET become zero
    if ((Data32 & REG_PHY_CONTROL_RESET) == 0) {
      break;
    }
    MicroSecondDelay(1);
  } while (TimeOut++ < PHY_TIMEOUT);
  if (TimeOut >= PHY_TIMEOUT) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: ERROR! PhySoftReset timeout\n"));
    return EFI_TIMEOUT;
  }

  if (PhyDriver->StartAutoNeg != NULL) {
    PhyDriver->StartAutoNeg (PhyDriver);
  }

  return EFI_SUCCESS;
}

UINT32
EFIAPI
PhyGetOui (
  IN  PHY_DRIVER  *PhyDriver
  )
{
  UINT32 OuiMsb;
  UINT32 OuiLsb;
  UINT32 Oui;

  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_IDENTIFIER_1, &OuiMsb);
  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_IDENTIFIER_2, &OuiLsb);
  Oui = (OuiMsb << REG_PHY_IDENTIFIER_2_WIDTH) | (OuiLsb >> REG_PHY_IDENTIFIER_2_SHIFT);

  return Oui;
}

EFI_STATUS
EFIAPI
PhyConfig (
  IN  PHY_DRIVER   *PhyDriver
  )
{
  UINT32      Oui;
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));
  PhyDriver->PhyPageSelRegister = 0;
  PhyDriver->PhyPage = MAX_UINT32;
  PhyDriver->AutoNegState = PHY_AUTONEG_IDLE;
  PhyDriver->PhyOldLink = LINK_DOWN;

  Oui = PhyGetOui (PhyDriver);
  switch (Oui) {
    case PHY_MARVELL_OUI:
      PhyDriver->Config = PhyMarvellConfig;
      PhyDriver->StartAutoNeg = PhyMarvellStartAutoNeg;
      PhyDriver->CheckAutoNeg = PhyMarvellCheckAutoNeg;
      PhyDriver->DetectLink = PhyMarvellDetectLink;
      break;

    case PHY_REALTEK_OUI:
      PhyDriver->Config = PhyRealtekConfig;
      PhyDriver->StartAutoNeg = PhyRealtekStartAutoNeg;
      PhyDriver->CheckAutoNeg = PhyRealtekCheckAutoNeg;
      PhyDriver->DetectLink = PhyRealtekDetectLink;
      break;

    default:
      return EFI_UNSUPPORTED;
  }

  Status = PhyDriver->Config (PhyDriver);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SNP:PHY: %a () Failed to configure Phy\r\n", __FUNCTION__));
    return Status;
  }

  // Configure AN and Advertise
  Status = PhyDriver->StartAutoNeg (PhyDriver);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SNP:PHY: %a () Failed to Start Auto Neg\r\n", __FUNCTION__));
  }

  return Status;
}

EFI_STATUS
EFIAPI
PhyDxeInitialization (
  IN PHY_DRIVER   *PhyDriver,
  IN EMAC_DRIVER  *MacDriver
  )
{
  EFI_STATUS Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  PhyDriver->MacDriver = MacDriver;

  Status = PhyReset (PhyDriver);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SNP:PHY: %a () Failed to reset Phy\r\n", __FUNCTION__));
    return Status;
  }

  Status = PhyConfig (PhyDriver);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SNP:PHY: %a () Failed to configure Phy\r\n", __FUNCTION__));
  }

  return Status;
}

EFI_STATUS
EFIAPI
PhyLinkAdjustEmacConfig (
  IN PHY_DRIVER   *PhyDriver
  )
{
  EFI_STATUS   Status;
  UINT64       ClockRate;

  Status = EFI_SUCCESS;

  PhyDriver->CheckAutoNeg (PhyDriver);
  PhyDriver->DetectLink (PhyDriver);

  if (PhyDriver->PhyOldLink != PhyDriver->PhyCurrentLink) {
    if (PhyDriver->PhyCurrentLink == LINK_UP) {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is up - Network Cable is Plugged\r\n"));
      if (PhyDriver->Speed == SPEED_1000) {
        ClockRate = 125000000;
        osi_set_speed (PhyDriver->MacDriver->osi_core, OSI_SPEED_1000);
      } else if (PhyDriver->Speed == SPEED_100) {
        ClockRate = 25000000;
        osi_set_speed (PhyDriver->MacDriver->osi_core, OSI_SPEED_100);
      } else {
        ClockRate = 2500000;
        osi_set_speed (PhyDriver->MacDriver->osi_core, OSI_SPEED_10);
      }
      if (PhyDriver->Duplex == DUPLEX_FULL) {
        osi_set_mode(PhyDriver->MacDriver->osi_core, OSI_FULL_DUPLEX);
      } else {
        osi_set_mode(PhyDriver->MacDriver->osi_core, OSI_HALF_DUPLEX);
      }

      Status = DeviceDiscoverySetClockFreq (PhyDriver->ControllerHandle, "tx", ClockRate);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a, Failed to set clock frequency %r\r\n", __FUNCTION__, Status));
        Status = EFI_SUCCESS;
      }
    } else {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is Down - Network Cable is Unplugged?\r\n"));
      Status = EFI_NOT_READY;
    }
  } else if (PhyDriver->PhyCurrentLink == LINK_DOWN) {
    Status = EFI_NOT_READY;
  }

  PhyDriver->PhyOldLink = PhyDriver->PhyCurrentLink;

  return Status;
}
