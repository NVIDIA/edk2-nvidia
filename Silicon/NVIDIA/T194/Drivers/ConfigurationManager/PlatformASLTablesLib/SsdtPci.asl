/** @file
  SSDT for Tegra194
**/

#include <T194/T194Definitions.h>

DefinitionBlock("SsdtPci.aml", "SSDT", 1, "NVIDIA", "TEGRA194", 0x00000001) {
  Scope(_SB) {
    Device(PCI0) {
      Name (_HID, EISAID("PNP0A08")) // PCI Express Root Bridge
      Name (_UID, 7)
      Name (_CID, EISAID("PNP0A03")) // Compatible PCI Root Bridge
      Name (_SEG, 0)                 // PCI Segment Group number
      Name (_BBN, T194_PCIE_BUS_MIN) // PCI Base Bus Number
      Name (_CCA, 1)                 // Initially mark the PCI coherent

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
            0,   // AddressGranularity
            T194_PCIE_BUS_MIN, // AddressMinimum - Minimum Bus Number
            T194_PCIE_BUS_MAX, // AddressMaximum - Maximum Bus Number
            0,                 // AddressTranslation - Set to 0
            2                  // RangeLength - Number of Busses
          )

          // 64-bit BAR Window-1
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x40000000,           // Min Base Address
            0xFFFFFFFF,           // Max Base Address
            0x1F00000000,         // Translate
            0xC0000000            // Length
          )

          // 64-bit BAR Window-2
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x1C00000000,         // Min Base Address
            0x1F3FFFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x340000000           // Length
          )
        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)

      // Root complex status
      Method (_STA, 0, Serialized) {
        Return (Zero)
      } // Method(_STA)
    } // PCI0
  } //Scope(_SB)
}
