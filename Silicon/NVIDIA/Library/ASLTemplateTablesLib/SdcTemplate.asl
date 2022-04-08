/** @file
 * SSDT for SDHCI
 *
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

DefinitionBlock("SdcTemplate.aml", "SSDT", 2, "NVIDIA", "SDCTEMP", 0x00000001) {
  Device(SDCT) {
    Name (_HID, "NVDA2002")
    Name (_CID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
    Name (_UID, 0)
    Name (_CCA, ZERO)
    Name (_RMV, 0)

    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, 0xFFFFFFFF, 0xFFFFFFFF, REG0)
      Interrupt(ResourceConsumer, Level, ActiveHigh, ExclusiveAndWake, , , INT0) { 0xFF }
    })
  }
}
