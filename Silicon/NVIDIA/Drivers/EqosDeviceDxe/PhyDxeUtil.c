/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.
  Copyright (c) 2020 - 2021, NVIDIA Corporation.  All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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


STATIC
EFI_STATUS
EFIAPI
PhyReset (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  EFI_STATUS    Status;
  EMBEDDED_GPIO *GpioProtocol = NULL;

  if (PhyDriver->ResetPin == NON_EXISTENT_ON_PRESIL) {
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
  Status = GpioProtocol->Set (GpioProtocol, PhyDriver->ResetPin, PhyDriver->ResetMode1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set gpio %x to %d %r\r\n", PhyDriver->ResetPin, PhyDriver->ResetMode1, Status));
    return Status;
  }
  return Status;
}


STATIC
EFI_STATUS
EFIAPI
PhyMdioWrite (
  IN UINT32   Reg,
  IN UINT32   Data,
  IN UINT32   CsrClockRange,
  IN UINTN    MacBaseAddress
  )
{
  UINT32   MiiConfig;
  UINT32   Count;

  // Check it is a valid Reg
  ASSERT(Reg <= 31);

  MiiConfig = (PHY_ID << MAC_MDIO_ADDR_PA_SHIFT) |
              (Reg << MAC_MDIO_ADDR_RDA_SHIFT) |
              (CsrClockRange << MAC_MDIO_ADDR_CR_SHIFT) |
              (MAC_MDIO_ADDR_GOC_WRITE  <<  MAC_MDIO_ADDR_GOC_SHIFT) |
              MAC_MDIO_ADDR_GB;
  // Write the desired value to the register first
  MmioWrite32 (MacBaseAddress + MAC_MDIO_DATA_OFFSET, (Data & 0xFFFF));

  // write this config to register
  MmioWrite32 (MacBaseAddress + MAC_MDIO_ADDR_OFFSET, MiiConfig);

  // Wait for busy bit to clear
  Count = 0;
  while (Count < 1000) {
    if ((MmioRead32 (MacBaseAddress + MAC_MDIO_ADDR_OFFSET) & MAC_MDIO_ADDR_GB) == 0) {
      return EFI_SUCCESS;
    }
    MemoryFence ();
    Count++;
  };

  return EFI_TIMEOUT;
}

EFI_STATUS
EFIAPI
PhyMdioRead (
  IN UINT32   Reg,
  IN UINT32   *Data,
  IN UINT32   CsrClockRange,
  IN UINTN    MacBaseAddress
  )
{
  UINT32   MiiConfig;
  UINT32   Count;

  // Check it is a valid Reg
  ASSERT(Reg < 31);

  MiiConfig = (PHY_ID << MAC_MDIO_ADDR_PA_SHIFT) |
              (Reg << MAC_MDIO_ADDR_RDA_SHIFT) |
              (CsrClockRange << MAC_MDIO_ADDR_CR_SHIFT) |
              (MAC_MDIO_ADDR_GOC_READ  <<  MAC_MDIO_ADDR_GOC_SHIFT) |
              MAC_MDIO_ADDR_GB;

  // write this config to register
  MmioWrite32 (MacBaseAddress + MAC_MDIO_ADDR_OFFSET, MiiConfig);

  // Wait for busy bit to clear
  Count = 0;
  while (Count < 1000) {
    if ((MmioRead32 (MacBaseAddress + MAC_MDIO_ADDR_OFFSET) & MAC_MDIO_ADDR_GB) == 0) {
      // Write the desired value to the register first
      *Data = (MmioRead32 (MacBaseAddress + MAC_MDIO_DATA_OFFSET) & 0xFFFF);
      return EFI_SUCCESS;
    }
    MemoryFence ();
    Count++;
  };

  return EFI_TIMEOUT;
}

// Function to read from MII register (PHY Access)
EFI_STATUS
EFIAPI
PhyRead (
  IN PHY_DRIVER   *PhyDriver,
  IN  UINT32   Page,
  IN  UINT32   Reg,
  OUT UINT32   *Data,
  IN  UINTN    MacBaseAddress
  )
{
  UINT32 CsrClockRange;

  /* TODO: remove hardcoding */
  CsrClockRange = 4; /* axi_cbb clk rate is 204 Mhz so the value is 4 */

  if (PhyDriver->PhyPage != Page) {
    PhyMdioWrite(PhyDriver->PhyPageSelRegister, Page, CsrClockRange, MacBaseAddress);
    MicroSecondDelay(20);
    PhyDriver->PhyPage = Page;
  }

  return PhyMdioRead(Reg, Data, CsrClockRange, MacBaseAddress);
}


// Function to write to the MII register (PHY Access)
EFI_STATUS
EFIAPI
PhyWrite (
  IN PHY_DRIVER   *PhyDriver,
  IN UINT32   Page,
  IN UINT32   Reg,
  IN UINT32   Data,
  IN UINTN    MacBaseAddress
  )
{
  UINT32 CsrClockRange;

  /* TODO: remove hardcoding */
  CsrClockRange = 4; /* axi_cbb clk rate is 204 Mhz so the value is 4 */

  if (PhyDriver->PhyPage != Page) {
    PhyMdioWrite(PhyDriver->PhyPageSelRegister, Page, CsrClockRange, MacBaseAddress);
    MicroSecondDelay(20);
    PhyDriver->PhyPage = Page;
  }

  return PhyMdioWrite(Reg, Data, CsrClockRange, MacBaseAddress);
}

// Perform PHY software reset
EFI_STATUS
EFIAPI
PhySoftReset (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  UINT32        TimeOut;
  UINT32        Data32;
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  // PHY Basic Control Register reset
  PhyWrite (PhyDriver, PAGE_PHY, REG_PHY_CONTROL, REG_PHY_CONTROL_RESET, MacBaseAddress);

  // Wait for completion
  TimeOut = 0;
  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver, PAGE_PHY, REG_PHY_CONTROL, &Data32, MacBaseAddress);
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

  PhyDriver->StartAutoNeg (PhyDriver, MacBaseAddress);

  return EFI_SUCCESS;
}

UINT32
EFIAPI
PhyGetOui (
  IN  PHY_DRIVER   *PhyDriver,
  IN  UINTN        MacBaseAddress
  )
{
  UINT32 OuiMsb;
  UINT32 OuiLsb;
  UINT32 Oui;

  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_IDENTIFIER_1, &OuiMsb, MacBaseAddress);
  PhyRead (PhyDriver, PAGE_PHY, REG_PHY_IDENTIFIER_2, &OuiLsb, MacBaseAddress);
  Oui = (OuiMsb << REG_PHY_IDENTIFIER_2_WIDTH) | (OuiLsb >> REG_PHY_IDENTIFIER_2_SHIFT);

  return Oui;
}

EFI_STATUS
EFIAPI
PhyConfig (
  IN  PHY_DRIVER   *PhyDriver,
  IN  UINTN        MacBaseAddress
  )
{
  UINT32      Oui;
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));
  PhyDriver->PhyPageSelRegister = 0;
  PhyDriver->PhyPage = MAX_UINT32;
  PhyDriver->AutoNegInProgress = FALSE;

  Oui = PhyGetOui (PhyDriver, MacBaseAddress);
  if (Oui == PHY_MARVELL_OUI) {
    PhyDriver->Config = PhyMarvellConfig;
    PhyDriver->StartAutoNeg = PhyMarvellStartAutoNeg;
    PhyDriver->CheckAutoNeg = PhyMarvellCheckAutoNeg;
    PhyDriver->DetectLink = PhyMarvellDetectLink;
  } else if (Oui == PHY_REALTEK_OUI) {
    PhyDriver->Config = PhyRealtekConfig;
    PhyDriver->StartAutoNeg = PhyRealtekStartAutoNeg;
    PhyDriver->CheckAutoNeg = PhyRealtekCheckAutoNeg;
    PhyDriver->DetectLink = PhyRealtekDetectLink;
  } else {
    return EFI_UNSUPPORTED;
  }

  Status = PhyDriver->Config (PhyDriver, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SNP:PHY: %a () Failed to configure Phy\r\n", __FUNCTION__));
    return Status;
  }

  // Configure AN and Advertise
  Status = PhyDriver->StartAutoNeg (PhyDriver, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SNP:PHY: %a () Failed to Start Auto Neg\r\n", __FUNCTION__));
  }

  return Status;
}

EFI_STATUS
EFIAPI
PhyDxeInitialization (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  EFI_STATUS Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  Status = PhyReset (PhyDriver, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SNP:PHY: %a () Failed to reset Phy\r\n", __FUNCTION__));
    return Status;
  }

  Status = PhyConfig (PhyDriver, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SNP:PHY: %a () Failed to configure Phy\r\n", __FUNCTION__));
  }

  return Status;
}

EFI_STATUS
EFIAPI
PhyLinkAdjustEmacConfig (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  EFI_STATUS   Status;
  UINT64       ClockRate;

  Status = EFI_SUCCESS;

  PhyDriver->CheckAutoNeg (PhyDriver, MacBaseAddress);
  PhyDriver->DetectLink (PhyDriver, MacBaseAddress);

  if (PhyDriver->PhyOldLink != PhyDriver->PhyCurrentLink) {
    if (PhyDriver->PhyCurrentLink == LINK_UP) {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is up - Network Cable is Plugged\r\n"));
      if (PhyDriver->Speed == SPEED_1000) {
        ClockRate = 125000000;
      } else if (PhyDriver->Speed == SPEED_100) {
        ClockRate = 25000000;
      } else {
        ClockRate = 2500000;
      }
      EmacConfigAdjust (PhyDriver->Speed, PhyDriver->Duplex, MacBaseAddress);

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
