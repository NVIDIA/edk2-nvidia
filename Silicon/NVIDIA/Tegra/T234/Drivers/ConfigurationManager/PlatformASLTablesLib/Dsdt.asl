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
}
