/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2020 - 2021, NVIDIA Corporation. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * Portions provided under the following terms:
 * Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <T234/T234Definitions.h>

DefinitionBlock ("dsdt.aml", "DSDT", 2, "NVIDIA", "TEGRA234", 0x00000001)
{
  Device(CPU0) {
    Name(_HID, "ACPI0007")
    Name(_UID, Zero)
  }

  /*Device(CPU1) {
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
  }*/

  Device(SDC0) {
    Name(_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
    Name(_UID, 0)
    Name (_CCA, ZERO)
    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, T234_SDMMC4_BASE_ADDR, T234_SDMMC4_CAR_SIZE)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { T234_SDMMC4_INTR }
    })
  }

  Device(SDC3) {
    Name(_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
    Name(_UID, 0)
    Name (_CCA, ZERO)
    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, T234_SDMMC1_BASE_ADDR, T234_SDMMC1_CAR_SIZE)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { T234_SDMMC1_INTR }
    })
  }
}

