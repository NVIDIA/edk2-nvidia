/** @file
 * SSDT for PCIe for Tegra234
 *
 * Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 */

#include <T234/T234Definitions.h>

DefinitionBlock("SsdtPci.aml", "SSDT", 2, "NVIDIA", "TEGRA234", 0x00000001) {
  Scope(_SB) {

    Device(PCI1) {
      Name (_HID, EISAID("PNP0A08"))            // PCI Express Root Bridge
      Name (_UID, 1)
      Name (_CID, EISAID("PNP0A03"))            // Compatible PCI Root Bridge
      Name (_SEG, 1)                            // PCI Segment Group number
      Name (_BBN, T234_PCIE_BUS_MIN)            // PCI Base Bus Number
      Name (_CCA, 1)                            // Initially mark the PCI coherent
      Name (_STA, 0)

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

          // MCFG region
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

          // 64-bit Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x2080000000,         // Min Base Address
            0x20AFFFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x30000000            // Length
          )

          // 32-bit Non-Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x30200000,           // Min Base Address
            0x31FFFFFF,           // Max Base Address
            0x00000000,           // Translate
            0x01E00000            // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    } // PCI1

  } //Scope(_SB)
}
