/** @file
  SSDT for TH500
**/

#include <TH500/TH500Definitions.h>

DefinitionBlock("SsdtPci.aml", "SSDT", 1, "NVIDIA", "TH500", 0x00000001) {
  Scope(_SB) {
    Device(PCI0) {
      Name (_HID, EISAID("PNP0A08"))  // PCI Express Root Bridge
      Name (_UID, 7)
      Name (_CID, EISAID("PNP0A03"))  // Compatible PCI Root Bridge
      Name (_SEG, 0)                  // PCI Segment Group number
      Name (_BBN, TH500_PCIE_BUS_MIN) // PCI Base Bus Number
      Name (_CCA, 1)                  // Initially mark the PCI coherent

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
            TH500_PCIE_BUS_MIN, // AddressMinimum - Minimum Bus Number
            TH500_PCIE_BUS_MAX, // AddressMaximum - Maximum Bus Number
            0,                  // AddressTranslation - Set to 0
            2                   // RangeLength - Number of Busses
          )

          // 64-bit Prefetchable BAR Window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,               // Granularity
            TH500_PCIE_C0_64BIT_BASE, // Min Base Address
            TH500_PCIE_C0_64BIT_END,  // Max Base Address
            0x00000000,               // Translate
            TH500_PCIE_C0_64BIT_SIZE  // Length
          )

          // 32-bit Non-prefetchable BAR Window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x2CA00000,           // Min Base Address
            0x349FFFFF,           // Max Base Address
            0x00000000,           // Translate
            0x08000000            // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    } // PCI0
  } //Scope(_SB)
}
