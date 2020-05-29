/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.

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
  ASSERT(Reg < 31);

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
STATIC
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
    PhyMdioWrite(REG_PHY_PAGE, Page, CsrClockRange, MacBaseAddress);
    MicroSecondDelay(20);
    PhyDriver->PhyPage = Page;
  }

  return PhyMdioRead(Reg, Data, CsrClockRange, MacBaseAddress);
}


// Function to write to the MII register (PHY Access)
STATIC
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
    PhyMdioWrite(REG_PHY_PAGE, Page, CsrClockRange, MacBaseAddress);
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
  PhyWrite (PhyDriver, PAGE_COPPER, REG_COPPER_CONTROL, COPPER_CONTROL_RESET, MacBaseAddress);

  // Wait for completion
  TimeOut = 0;
  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_CONTROL, &Data32, MacBaseAddress);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    // Wait until PHYCTRL_RESET become zero
    if ((Data32 & COPPER_CONTROL_RESET) == 0) {
      break;
    }
    MicroSecondDelay(1);
  } while (TimeOut++ < PHY_TIMEOUT);
  if (TimeOut >= PHY_TIMEOUT) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: ERROR! PhySoftReset timeout\n"));
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

// Do auto-negotiation
STATIC
EFI_STATUS
EFIAPI
PhyAutoNego (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{

  UINT32        TimeOut;
  UINT32        Data32;
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_CONTROL, &Data32, MacBaseAddress);
  Data32 |= COPPER_CONTROL_ENABLE_AUTO_NEG | COPPER_RESTART_AUTO_NEG | COPPER_CONTROL_RESET;
  PhyWrite (PhyDriver, PAGE_COPPER, REG_COPPER_CONTROL, Data32, MacBaseAddress);

  // Wait for completion
  TimeOut = 0;
  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_CONTROL, &Data32, MacBaseAddress);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    // Wait until PHYCTRL_RESET become zero
    if ((Data32 & COPPER_CONTROL_RESET) == 0) {
      break;
    }
    MicroSecondDelay(1);
  } while (TimeOut++ < PHY_TIMEOUT);
  if (TimeOut >= PHY_TIMEOUT) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: ERROR! PhySoftReset timeout\n"));
    return EFI_TIMEOUT;
  }

  TimeOut = 0;
  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_INTR_STATUS, &Data32, MacBaseAddress);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    // Wait until PHYCTRL_RESET become zero
    if ((Data32 & COPPER_INTR_STATUS_AUTO_NEG_COMPLETED) != 0) {
      break;
    }
    MicroSecondDelay(1);
  } while (TimeOut++ < PHY_TIMEOUT);
  if (TimeOut >= PHY_TIMEOUT) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: ERROR! auto-negotiation timeout\n"));
    return EFI_TIMEOUT;
  }
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyConfig (
  IN  PHY_DRIVER   *PhyDriver,
  IN  UINTN        MacBaseAddress
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  /* Program Page: 2, Register: 0 */
  PhyWrite(PhyDriver, PAGE_MAC, REG_COPPER_CONTROL, 0, MacBaseAddress);

  Status = PhySoftReset (PhyDriver, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  /* Program Page: 2, Register: 16 */
  PhyWrite(PhyDriver, PAGE_MAC,
           REG_MAC_CONTROL1,
           MAC_CONTROL1_TX_FIFO_DEPTH_24_BITS |
           MAC_CONTROL1_ENABLE_RX_CLK |
           MAC_CONTROL1_PASS_ODD_NIBBLE_PREAMBLES |
           MAC_CONTROL1_RGMII_INTF_POWER_DOWN,
           MacBaseAddress);

  /* Program Page: 2, Register: 21 */
  PhyWrite(PhyDriver, PAGE_MAC,
           REG_MAC_CONTROL2,
           MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_1000_MBPS |
           MAC_CONTROL2_RGMII_RX_TIMING_CTRL |
           MAC_CONTROL2_RGMII_TX_TIMING_CTRL,
           MacBaseAddress);

  /* Program Page: 0, Register: 16 */
  /* Automatically detect whether it needs to crossover between pairs or not */
  PhyWrite(PhyDriver, PAGE_COPPER,
           REG_COPPER_CONTROL1,
           COPPER_CONTROL1_ENABLE_AUTO_CROSSOVER,
           MacBaseAddress);

  // Configure AN and Advertise
  PhyAutoNego (PhyDriver, MacBaseAddress);

  return EFI_SUCCESS;
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
    //return Status;
  }

  PhyConfig (PhyDriver, MacBaseAddress);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyLinkAdjustEmacConfig (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  UINT32       Speed;
  UINT32       Duplex;
  UINT32       Data32;
  EFI_STATUS   Status;
  UINT64       ClockRate;

  Status = EFI_SUCCESS;
  Speed = SPEED_10;
  Duplex = DUPLEX_HALF;

  if (PhyDriver->PhyOldLink == LINK_DOWN) {
    PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_INTR_STATUS, &Data32, MacBaseAddress);
    if ((Data32 & 0xC00) == 0xC00) {
      PhyAutoNego (PhyDriver, MacBaseAddress);
    }
  }

  PhyRead (PhyDriver, PAGE_COPPER, REG_COPPER_STATUS1, &Data32, MacBaseAddress);

  if ((Data32 & COPPER_STATUS1_LINK_STATUS) == 0) {
    PhyDriver->PhyCurrentLink = LINK_DOWN;
  } else {
    PhyDriver->PhyCurrentLink = LINK_UP;
  }

  if (PhyDriver->PhyOldLink != PhyDriver->PhyCurrentLink) {
    if (PhyDriver->PhyCurrentLink == LINK_UP) {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is up - Network Cable is Plugged\r\n"));
      if ((Data32 & COPPER_STATUS1_DUPLEX_MODE) == 0) {
        Duplex = DUPLEX_HALF;
      } else {
        Duplex = DUPLEX_FULL;
      }
      if ((Data32 & COPPER_STATUS1_SPEED_1000_MBPS) != 0) {
        Speed = 1000;
        ClockRate = 125000000;
      } else if ((Data32 & COPPER_STATUS1_SPEED_100_MBPS) != 0) {
        Speed = 100;
        ClockRate = 25000000;
      } else {
        Speed = 10;
        ClockRate = 2500000;
      }
      EmacConfigAdjust (Speed, Duplex, MacBaseAddress);

      Status = DeviceDiscoverySetClockFreq (PhyDriver->ControllerHandle, "eqos_tx", ClockRate);
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
