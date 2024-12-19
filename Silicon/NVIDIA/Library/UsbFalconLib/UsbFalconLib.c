/** @file

  Falcon Register Access

  SPDX-FileCopyrightText: Copyright (c) 2019-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UsbFalconLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DmaLib.h>

/* Base Address of Xhci Controller's Configuration registers. These config
 * registers are used to access the Falcon Registers and for FW Loading.
 * This address should be set first by calling FalconSetHostCfgAddr() before
 * accessing any other functions in the Falcon Library
 */
STATIC UINTN  XusbHostCfgAddr   = 0;
STATIC UINTN  XusbHostBase2Addr = 0;
STATIC UINTN  XusbAoAddr        = 0;

VOID
FalconSetHostCfgAddr (
  IN UINTN  Address
  )
{
  XusbHostCfgAddr = Address;
}

VOID
FalconSetHostBase2Addr (
  IN UINTN  Address
  )
{
  XusbHostBase2Addr = Address;
}

VOID
FalconSetAoAddr (
  IN UINTN  Address
  )
{
  XusbAoAddr = Address;
}

VOID *
FalconMapReg (
  IN  UINTN  Address
  )
{
  UINTN  PageIndex  = Address / 0x200;
  UINTN  PageOffset = Address % 0x200;
  VOID   *Register;

  if (XusbHostBase2Addr != 0) {
    MmioWrite32 (XusbHostBase2Addr + XUSB_BAR2_ARU_C11_CSBRANGE /* BAR2 CSBRANGE */, PageIndex);
    Register = (VOID *)(XusbHostBase2Addr + XUSB_BAR2_CSB_BASE_ADDR + PageOffset);
    return Register;
  }

  /* write page index into XUSB PCI CFG register CSBRANGE */
  MmioWrite32 (XusbHostCfgAddr + 0x41c /* CSBRANGE */, PageIndex);

  /* calculate falcon register address within 512-byte aperture in XUSB PCI CFG space between offsets 0x800 and 0xa00 */
  Register = (VOID *)(XusbHostCfgAddr + 0x800 + PageOffset);

  return Register;
}

UINT32
FalconRead32 (
  IN  UINTN  Address
  )
{
  UINT32  *Register32;

  if (XusbHostCfgAddr == 0) {
    DEBUG ((DEBUG_ERROR, "%a:Invalid Xhci Config Address\n", __FUNCTION__));
    return 0;
  }

  Register32 = (UINT32 *)FalconMapReg (Address);
  UINT32  Value = MmioRead32 ((UINTN)Register32);

  DEBUG ((DEBUG_VERBOSE, "%a: %x --> %x\r\n", __FUNCTION__, Address, Value));

  return Value;
}

UINT32
FalconWrite32 (
  IN  UINTN   Address,
  IN  UINT32  Value
  )
{
  UINT32  *Register32;

  if (XusbHostCfgAddr == 0) {
    DEBUG ((DEBUG_ERROR, "%a:Invalid Xhci Config Address\n", __FUNCTION__));
    return 0;
  }

  Register32 = (UINT32 *)FalconMapReg (Address);

  DEBUG ((DEBUG_VERBOSE, "%a: %x <-- %x\r\n", __FUNCTION__, Address, Value));

  MmioWrite32 ((UINTN)Register32, Value);

  return Value;
}

UINT32
Fpci2Read32 (
  IN  UINTN  Address
  )
{
  UINT32  *Register32;

  if (XusbHostBase2Addr == 0) {
    DEBUG ((DEBUG_ERROR, "%a:Invalid Xhci Config Address\n", __FUNCTION__));
    return 0;
  }

  Register32 = (UINT32 *)(XusbHostBase2Addr + Address);
  UINT32  Value = MmioRead32 ((UINTN)Register32);

  DEBUG ((DEBUG_VERBOSE, "%a: %x --> %x\r\n", __FUNCTION__, Address, Value));

  return Value;
}

UINT32
Fpci2Write32 (
  IN  UINTN   Address,
  IN  UINT32  Value
  )
{
  UINT32  *Register32;

  if (XusbHostBase2Addr == 0) {
    DEBUG ((DEBUG_ERROR, "%a:Invalid Xhci Config Address\n", __FUNCTION__));
    return 0;
  }

  Register32 = (UINT32 *)(XusbHostBase2Addr + Address);
  DEBUG ((DEBUG_VERBOSE, "%a: %x <-- %x\r\n", __FUNCTION__, Address, Value));

  MmioWrite32 ((UINTN)Register32, Value);

  return Value;
}
