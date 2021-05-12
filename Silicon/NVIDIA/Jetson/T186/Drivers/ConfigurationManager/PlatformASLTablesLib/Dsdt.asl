/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2020 - 2021, NVIDIA Corporation. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <T186/T186Definitions.h>

DefinitionBlock ("dsdt.aml", "DSDT", 2, "NVIDIA", "TEGRA186", 0x00000001)
{
  Device(CPU0) {
    Name(_HID, "ACPI0007")
    Name(_UID, Zero)
  }

  Device(CPU1) {
    Name(_HID, "ACPI0007")
    Name(_UID, One)
  }

  Device(CPU2) {
    Name(_HID, "ACPI0007")
    Name(_UID, 2)
  }

  Device(CPU3) {
    Name(_HID, "ACPI0007")
    Name(_UID, 3)
  }

  Device(CPU4) {
    Name(_HID, "ACPI0007")
    Name(_UID, 4)
  }

  Device(CPU5) {
    Name(_HID, "ACPI0007")
    Name(_UID, 5)
  }

  Device(COM0) {
    Name (_HID, "NVDA0100")
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate() {
      Memory32Fixed(ReadWrite, T186_UARTA_BASE_ADDR, T186_UARTA_CAR_SIZE)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { T186_UARTA_INTR }
    })

    Name (_DSD, Package () {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () {"clock-frequency", 408000000},
        Package () {"reg-shift", 2},
        Package () {"reg-io-width", 4},
      }
    })
  }

  Device(SDC0) {
    Name(_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
    Name(_UID, 0)
    Name (_CCA, ZERO)
    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, T186_SDMMC4_BASE_ADDR, T186_SDMMC4_CAR_SIZE)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { T186_SDMMC4_INTR }
    })
  }

  Device(SDC3) {
    Name(_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
    Name(_UID, 0)
    Name (_CCA, ZERO)
    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, T186_SDMMC1_BASE_ADDR, T186_SDMMC1_CAR_SIZE)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { T186_SDMMC1_INTR }
    })
  }
}

