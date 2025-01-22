/** @file
  SSDT for Live Firmware Activation.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/


// Sourced from ARM's "Live Firmware Activation SMC Interface" spec v1.0ALP3

DefinitionBlock("SsdtLfa_t410.aml", "SSDT", 2, "NVIDIA", "HF_T410", 0x00000001) {
  Scope (_SB)
  {
    Device (FWU0) {
      Name (_HID, "ARMHF000") // Arm HAFW DSDT table identifier.

      Name (_CRS, ResourceTemplate () {
      // Interrupt signaling the updated Firmware Store.
      Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive) { 861 } // Per ATF team Interrupt#885 will be used for LFA Kernal notification.
      })
      // _DSD -- Holds map between field names and objects within the _CRS
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package() {
          // Contains a set of packages of 1 entry,
          // the second entry is an index into the _CRS.
          Package () {"fw-store-updated-interrupt", 0},
        }
      })
    }
  }
}
