/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2020 - 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <T234/T234Definitions.h>

DefinitionBlock ("dsdt.aml", "DSDT", 2, "NVIDIA", "TEGRA234", 0x00000001)
{
  Device(USB0) {
    Name (_HID, "NVDA0214")
    Name (_CID, "PNP0D10")
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, 0x03610000, 0x40000)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0xc3 }
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
}
