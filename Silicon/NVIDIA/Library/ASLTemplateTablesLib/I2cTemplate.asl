/** @file
 * SSDT for I2C
 *
 * Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

DefinitionBlock("I2cTemplate.aml", "SSDT", 2, "NVIDIA", "I2CTEMP", 0x00000001) {
  Device (I2CT) {
    Name (_HID, "NVDA0301")
    Name (_UID, 0)

    Name (_CRS, ResourceTemplate() {
      Memory32Fixed(ReadWrite, 0xFFFFFFFF, 0xFFFFFFFF, REG0)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive, , , INT0) { 0xFF }
      //FixedDMA (0xFF, 0x0001, Width32bit,DMA1)
      //FixedDMA (0xFF, 0x0002, Width32bit,DMA2)
    })

    //Upstream driver prints warning if _RST method is not defined
    Method (_RST) {
    }
  }
}
