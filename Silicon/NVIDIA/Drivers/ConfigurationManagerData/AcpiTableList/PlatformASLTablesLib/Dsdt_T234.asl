/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <T234/T234Definitions.h>

DefinitionBlock ("dsdt_t234.aml", "DSDT", 2, "NVIDIA", "TEGRA234", 0x00000001)
{
  Scope(_SB) {
    Method (_OSC, 4, Serialized)  { // _OSC: Operating System Capabilities
      CreateDWordField (Arg3, 0x00, STS0)
      CreateDWordField (Arg3, 0x04, CAP0)
      If ((Arg0 == ToUUID ("0811b06e-4a27-44f9-8d60-3cbbc22e7b48") /* Platform-wide Capabilities */)) {
        If (!(Arg1 == One)) {
          STS0 &= ~0x1F
          STS0 |= 0x0A
        } Else {
          If ((CAP0 & 0x100)) {
            CAP0 &= ~0x100 /* No support for OS Initiated LPI */
            STS0 &= ~0x1F
            STS0 |= 0x12
          }
        }
      } Else {
        STS0 &= ~0x1F
        STS0 |= 0x06
      }
      Return (Arg3)
    }

    //---------------------------------------------------------------------
    // dla0 @ 15880000
    //---------------------------------------------------------------------
    Device (DLA0) {
      Name (_HID, "NVDA200A")
      Name (_UID, 0)
      Name (_CCA, ONE)

      Name (_CRS, ResourceTemplate() {
              Memory32Fixed(ReadWrite, 0x15880000, 0x40000)
              Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x10c }
              })
    }

    //---------------------------------------------------------------------
    // dla1 @ 158c0000
    //---------------------------------------------------------------------
    Device (DLA1) {
      Name (_HID, "NVDA200A")
      Name (_UID, 1)
      Name (_CCA, ONE)

      Name (_CRS, ResourceTemplate() {
              Memory32Fixed(ReadWrite, 0x158c0000, 0x40000)
              Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x10d }
              })
    }

    //---------------------------------------------------------------------
    // host1x
    //---------------------------------------------------------------------
    // Description: host1x
    Device(HOST) {
      Name (_HID, "NVDA200D")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed(ReadWrite, 0x13e00000, 0x10000)
        Memory32Fixed(ReadWrite, 0x13e10000, 0x10000)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x127 }
      })
    }

    //---------------------------------------------------------------------
    // ga10b
    //---------------------------------------------------------------------
    Device(GPU0) {
      Name (_HID, "NVDA1081")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate() {
        Memory32Fixed(ReadWrite, 0x17000000, 0x1000000)
        Memory32Fixed(ReadWrite, 0x18000000, 0x1000000)
        Memory32Fixed(ReadWrite, 0x03b41000, 0x1000)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x64, 0x66, 0x67, 0x63 }
      })
    }
  }

  Device(MRQ0) {
    Name (_HID, "NVDA2001")
    Name (_UID, 0)
    Name (_CCA, ZERO)
    // Name (CDIS, ZERO)

    // Method (_HID, 0, NotSerialized) {
    //   Store (ZERO, CDIS)
    //   Return ("NVDA2001")
    // }

    // Method (_STA, 0, NotSerialized) {
    //   If (LEqual(CDIS, One)) {
    //     Return (0x0D)
    //   }
    //   Return (0x0F)
    // }

    // Method (_DIS, 0, NotSerialized) {
    //   Store (One, CDIS)
    // }

    Name(_CRS, ResourceTemplate() {
      // 1. HSP_TOP0 Registers
      Memory32Fixed (ReadWrite, 0x03C00000, 0xA0000)
      // 2. IVC Tx Pool (using 0x40070000 + {0x100-0x1FF, 0x200-0x2FF, 0x300-0x3FF, 0xD00-0xDFF})
      Memory32Fixed (ReadWrite, 0x40070100, 0xF00)
      // 3. IVC Rx Pool (using 0x40071000 + {0x100-0x1FF, 0x200-0x2FF, 0x300-0x3FF, 0xD00-0xDFF})
      Memory32Fixed (ReadWrite, 0x40071100, 0xF00)
      // 4. HSP_TOP0_CCPLEX_DBELL (0xB0 + 0x20)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0xD0 }
    })
  }

  //---------------------------------------------------------------------
  // rce@bc00000
  //---------------------------------------------------------------------
  Device(RCE0) {
    Name (_HID, "NVDA2007")
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate() {
      // HSP_RCE Registers
      Memory32Fixed (ReadWrite, 0x0b950000, 0x90000)
      // RCE_PM Registers
      Memory32Fixed (ReadWrite, 0x0b9f0000, 0x40000)
      // RCE_WDT_REMOTE (0x13 + 0x20)
      // HSP_RCE_SHARED_MAILBOX (0xB6 + 0x20)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x33, 0xD6 }
    })
  }

  //---------------------------------------------------------------------
  // vi0@15c00000
  //---------------------------------------------------------------------
  Device(VI00) {
    Name (_HID, "NVDA2008")
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate() {
      // VI MMIO apertures are programmed by RTCPU and firewalls prevent
      // access from CCPLEX.
    })
  }

  //---------------------------------------------------------------------
  // vi1@14c00000
  //---------------------------------------------------------------------
  Device(VI01) {
    Name (_HID, "NVDA2008")
    Name (_UID, 1)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate() {
      // VI MMIO apertures are programmed by RTCPU and firewalls prevent
      // access from CCPLEX.
    })
  }

  //---------------------------------------------------------------------
  // isp@14800000
  //---------------------------------------------------------------------
  Device(ISP0) {
    Name (_HID, "NVDA2009")
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate() {
      // ISP MMIO apertures are programmed by RTCPU and firewalls prevent
      // access from CCPLEX.
    })
  }
}
