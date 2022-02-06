/** @file
 * SSDT for PCIe for Tegra234
 *
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <T234/T234Definitions.h>

DefinitionBlock("SsdtPci.aml", "SSDT", 2, "NVIDIA", "TEGRA234", 0x00000001) {
  Scope(_SB) {

    //MCFG resources
    Device(RES0) {
      Name (_HID, EISAID("PNP0C02")) // Motherboard resources
      Name (_UID, 1)
      Method (_CRS, 0, Serialized) {
        Name (RBUF, ResourceTemplate () {
          // MCFG PCI1 region
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x20B0000000,         // Min Base Address
            0x20BFFFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x10000000            // Length
          )
          // MCFG PCI4 region
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x2430000000,         // Min Base Address
            0x243FFFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x10000000            // Length
          )
          // MCFG PCI5 region
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x2B30000000,         // Min Base Address
            0x2B3FFFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x10000000            // Length
          )
        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    }

    Device(PCI1) {
      Name (_HID, EISAID("PNP0A08"))            // PCI Express Root Bridge
      Name (_UID, 1)
      Name (_CID, EISAID("PNP0A03"))            // Compatible PCI Root Bridge
      Name (_SEG, 1)                            // PCI Segment Group number
      Name (_BBN, T234_PCIE_BUS_MIN)            // PCI Base Bus Number
      Name (_CCA, 1)                            // Initially mark the PCI coherent
      Name (_STA, 0xF)

      Name (_PRT, Package (){
        Package () {0xFFFF,0,0,77}, // INT_A
        Package () {0xFFFF,1,0,77}, // INT_B
        Package () {0xFFFF,2,0,77}, // INT_C
        Package () {0xFFFF,3,0,77}, // INT_D
      })

      // Root complex resources
      Method (_CRS, 0, Serialized) {
        Name (RBUF, ResourceTemplate () {
          // Bus numbers assigned to this root
          WordBusNumber (
            ResourceProducer,
            MinFixed, MaxFixed, PosDecode,
            0,                  // AddressGranularity
            T234_PCIE_BUS_MIN,  // AddressMinimum - Minimum Bus Number
            T234_PCIE_BUS_MAX,  // AddressMaximum - Maximum Bus Number
            0,                  // AddressTranslation - Set to 0
            256                 // RangeLength - Number of Busses
          )

          // 64-bit Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x2080000000,         // Min Base Address
            0x20A7FFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x28000000            // Length
          )

          // 32-bit Non-Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x40000000,           // Min Base Address
            0x47FFFFFF,           // Max Base Address
            0x2068000000,         // Translate
            0x08000000            // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    } // PCI1

    Device(PCI4) {
      Name (_HID, EISAID("PNP0A08"))            // PCI Express Root Bridge
      Name (_UID, 4)
      Name (_CID, EISAID("PNP0A03"))            // Compatible PCI Root Bridge
      Name (_SEG, 4)                            // PCI Segment Group number
      Name (_BBN, T234_PCIE_BUS_MIN)            // PCI Base Bus Number
      Name (_CCA, 1)                            // Initially mark the PCI coherent
      Name (_STA, 0xF)

      Name (_PRT, Package (){
        Package () {0xFFFF,0,0,83}, // INT_A
        Package () {0xFFFF,1,0,83}, // INT_B
        Package () {0xFFFF,2,0,83}, // INT_C
        Package () {0xFFFF,3,0,83}, // INT_D
      })

      // Root complex resources
      Method (_CRS, 0, Serialized) {
        Name (RBUF, ResourceTemplate () {
          // Bus numbers assigned to this root
          WordBusNumber (
            ResourceProducer,
            MinFixed, MaxFixed, PosDecode,
            0,                  // AddressGranularity
            T234_PCIE_BUS_MIN,  // AddressMinimum - Minimum Bus Number
            T234_PCIE_BUS_MAX,  // AddressMaximum - Maximum Bus Number
            0,                  // AddressTranslation - Set to 0
            256                 // RangeLength - Number of Busses
          )

          // 64-bit Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x2140000000,         // Min Base Address
            0x2427FFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x2E8000000           // Length
          )

          // 32-bit Non-Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x40000000,           // Min Base Address
            0x47FFFFFF,           // Max Base Address
            0x23E8000000,         // Translate
            0x08000000            // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    } // PCI4

    Device(PCI5) {
      Name (_HID, EISAID("PNP0A08"))            // PCI Express Root Bridge
      Name (_UID, 5)
      Name (_CID, EISAID("PNP0A03"))            // Compatible PCI Root Bridge
      Name (_SEG, 5)                            // PCI Segment Group number
      Name (_BBN, T234_PCIE_BUS_MIN)            // PCI Base Bus Number
      Name (_CCA, 1)                            // Initially mark the PCI coherent
      Name (_STA, 0xF)

      Name (_PRT, Package (){
        Package () {0xFFFF,0,0,85}, // INT_A
        Package () {0xFFFF,1,0,85}, // INT_B
        Package () {0xFFFF,2,0,85}, // INT_C
        Package () {0xFFFF,3,0,85}, // INT_D
      })

      // Root complex resources
      Method (_CRS, 0, Serialized) {
        Name (RBUF, ResourceTemplate () {
          // Bus numbers assigned to this root
          WordBusNumber (
            ResourceProducer,
            MinFixed, MaxFixed, PosDecode,
            0,                  // AddressGranularity
            T234_PCIE_BUS_MIN,  // AddressMinimum - Minimum Bus Number
            T234_PCIE_BUS_MAX,  // AddressMaximum - Maximum Bus Number
            0,                  // AddressTranslation - Set to 0
            256                 // RangeLength - Number of Busses
          )

          // 64-bit Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x2740000000,         // Min Base Address
            0x2B27FFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x3E8000000           // Length
          )

          // 32-bit Non-Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x40000000,           // Min Base Address
            0x47FFFFFF,           // Max Base Address
            0x2AE8000000,         // Translate
            0x08000000            // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    } // PCI5

  } //Scope(_SB)
}
