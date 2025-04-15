/** @file
  SSDT for Ethernet

  SPDX-FileCopyrightText: Copyright (c) 2019 - 2025, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

DefinitionBlock("SsdtEth.aml", "SSDT", 2, "NVIDIA", "LNRO ", 0x00000001) {
  Scope(_SB) {
    //---------------------------------------------------------------------
    // ethernet @ 0x03B40000
    //---------------------------------------------------------------------
    Device(ETH0) {
      Name (_HID, "LNRO0003") /* SMC91x */
      Name (_UID, 0)

      Name (_CCA, 0) /* Non-Coherent DMA */

      Name(_CRS, ResourceTemplate() {
        Memory32Fixed(ReadWrite, FixedPcdGet32(PcdEthernetBaseAddr), FixedPcdGet32(PcdEthernetSize))
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { FixedPcdGet32(PcdEthernetIntrId) }
      })

      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () {"reg-io-width", 4},
        }
      })

      //-------------------------------------------------------------
      // _STA(): Report device status (0xF: Present, 0x0: Absent)
      //-------------------------------------------------------------
      Method (_STA) {
        Return (0xF)
      }
    }
  } //Scope(_SB)
}
