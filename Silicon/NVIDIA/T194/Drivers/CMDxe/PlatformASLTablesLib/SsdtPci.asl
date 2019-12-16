/** @file
  SSDT for Tegra194
**/

DefinitionBlock("SsdtPci.aml", "SSDT", 1, "NVIDIA", "TEGRA194", 0x00000001) {
  Scope(_SB) {

    Device(PCI0) {
      Name (_HID, EISAID("PNP0A08"))            // PCI Express Root Bridge
      Name (_UID, 7)
      Name (_CID, EISAID("PNP0A03"))            // Compatible PCI Root Bridge
      Name (_SEG, 0)                            // PCI Segment Group number
      Name (_BBN, FixedPcdGet32 (PcdPciBusMin)) // PCI Base Bus Number
      Name (_CCA, 1)                            // Initially mark the PCI coherent

      Name (_PRT, Package (){
        Package () {0xFFFF,0,0,104}, // INT_A
        Package () {0xFFFF,1,0,104}, // INT_B
        Package () {0xFFFF,2,0,104}, // INT_C
        Package () {0xFFFF,3,0,104}, // INT_D
      })

      // Root complex resources
      Method (_CRS, 0, Serialized) {
        Name (RBUF, ResourceTemplate () {
          // Bus numbers assigned to this root
          WordBusNumber (
            ResourceProducer,
            MinFixed, MaxFixed, PosDecode,
            0,                             // AddressGranularity
            FixedPcdGet32 (PcdPciBusMin),  // AddressMinimum - Minimum Bus Number
            FixedPcdGet32 (PcdPciBusMax),  // AddressMaximum - Maximum Bus Number
            0,                             // AddressTranslation - Set to 0
            32                             // RangeLength - Number of Busses
          )

          // MCFG region
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x38000000,           // Min Base Address
            0x39FFFFFF,           // Max Base Address
            0x00000000,           // Translate
            0x02000000            // Length
          )

          // 64-bit Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x1800000000,         // Min Base Address
            0x1B3FFFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x340000000           // Length
          )

          // 32-bit Non-Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x40000000,           // Min Base Address
            0xFFFEFFFF,           // Max Base Address
            0x1B00000000,         // Translate
            0xBFFF0000            // Length
          )

          // IO BAR window
          QWordIO (
            ResourceProducer,
            MinFixed,
            MaxFixed,
            PosDecode,
            EntireRange,
            0x0,                   // Granularity
            0x0,                   // Min Base Address
            0xFFFF,                // Max Base Address
            0x1BFFFF0000,          // Translate
            0x10000                // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    } // PCI0

    Device(PCI1) {
      Name (_HID, EISAID("PNP0A08"))            // PCI Express Root Bridge
      Name (_UID, 7)
      Name (_CID, EISAID("PNP0A03"))            // Compatible PCI Root Bridge
      Name (_SEG, 1)                            // PCI Segment Group number
      Name (_BBN, FixedPcdGet32 (PcdPciBusMin)) // PCI Base Bus Number
      Name (_CCA, 1)                            // Initially mark the PCI coherent

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
            0,                             // AddressGranularity
            FixedPcdGet32 (PcdPciBusMin),  // AddressMinimum - Minimum Bus Number
            FixedPcdGet32 (PcdPciBusMax),  // AddressMaximum - Maximum Bus Number
            0,                             // AddressTranslation - Set to 0
            32                             // RangeLength - Number of Busses
          )

          // MCFG region
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x30000000,           // Min Base Address
            0x31FFFFFF,           // Max Base Address
            0x00000000,           // Translate
            0x02000000            // Length
          )

          // 64-bit Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x1200000000,         // Min Base Address
            0x122FFFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x30000000            // Length
          )

          // 32-bit Non-Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x40000000,           // Min Base Address
            0x4FFEFFFF,           // Max Base Address
            0x11F0000000,         // Translate
            0x0FFF0000            // Length
          )

          // IO BAR window
          QWordIO (
            ResourceProducer,
            MinFixed,
            MaxFixed,
            PosDecode,
            EntireRange,
            0x0,                  // Granularity
            0x0,                  // Min Base Address
            0xFFFF,               // Max Base Address
            0x123FFF0000,         // Translate
            0x10000               // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    } // PCI1

    Device(PCI3) {
      Name (_HID, EISAID("PNP0A08"))            // PCI Express Root Bridge
      Name (_UID, 7)
      Name (_CID, EISAID("PNP0A03"))            // Compatible PCI Root Bridge
      Name (_SEG, 3)                            // PCI Segment Group number
      Name (_BBN, FixedPcdGet32 (PcdPciBusMin)) // PCI Base Bus Number
      Name (_CCA, 1)                            // Initially mark the PCI coherent

      Name (_PRT, Package (){
        Package () {0xFFFF,0,0,81}, // INT_A
        Package () {0xFFFF,1,0,81}, // INT_B
        Package () {0xFFFF,2,0,81}, // INT_C
        Package () {0xFFFF,3,0,81}, // INT_D
      })

      // Root complex resources
      Method (_CRS, 0, Serialized) {
        Name (RBUF, ResourceTemplate () {
          // Bus numbers assigned to this root
          WordBusNumber (
            ResourceProducer,
            MinFixed, MaxFixed, PosDecode,
            0,                             // AddressGranularity
            FixedPcdGet32 (PcdPciBusMin),  // AddressMinimum - Minimum Bus Number
            FixedPcdGet32 (PcdPciBusMax),  // AddressMaximum - Maximum Bus Number
            0,                             // AddressTranslation - Set to 0
            32                             // RangeLength - Number of Busses
          )

          // MCFG region
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x34000000,           // Min Base Address
            0x35FFFFFF,           // Max Base Address
            0x00000000,           // Translate
            0x02000000            // Length
          )

          // 64-bit Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x1280000000,         // Min Base Address
            0x12AFFFFFFF,         // Max Base Address
            0x00000000,           // Translate
            0x30000000            // Length
          )

          // 32-bit Non-Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x40000000,           // Min Base Address
            0x4FFEFFFF,           // Max Base Address
            0x1270000000,         // Translate
            0x0FFF0000            // Length
          )

          // IO BAR window
          QWordIO (
            ResourceProducer,
            MinFixed,
            MaxFixed,
            PosDecode,
            EntireRange,
            0x0,                  // Granularity
            0x0,                  // Min Base Address
            0xFFFF,               // Max Base Address
            0x12BFFF0000,         // Translate
            0x10000               // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)
    } // PCI3

    Device(PCI5) {
      Name (_HID, EISAID("PNP0A08"))            // PCI Express Root Bridge
      Name (_UID, 7)
      Name (_CID, EISAID("PNP0A03"))            // Compatible PCI Root Bridge
      Name (_SEG, 5)                            // PCI Segment Group number
      Name (_BBN, FixedPcdGet32 (PcdPciBusMin)) // PCI Base Bus Number
      Name (_CCA, 1)                            // Initially mark the PCI coherent

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
            0,                             // AddressGranularity
            FixedPcdGet32 (PcdPciBusMin),  // AddressMinimum - Minimum Bus Number
            FixedPcdGet32 (PcdPciBusMax),  // AddressMaximum - Maximum Bus Number
            0,                             // AddressTranslation - Set to 0
            32                             // RangeLength - Number of Busses
          )

          // MCFG region
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x3A000000,           // Min Base Address
            0x3BFFFFFF,           // Max Base Address
            0x00000000,           // Translate
            0x02000000            // Length
          )

          // 64-bit Prefetchable BAR window
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

          // 32-bit Non-Prefetchable BAR window
          QWordMemory (
            ResourceProducer, PosDecode,
            MinFixed, MaxFixed,
            Cacheable, ReadWrite,
            0x00000000,           // Granularity
            0x40000000,           // Min Base Address
            0xFFFEFFFF,           // Max Base Address
            0x1F00000000,         // Translate
            0xBFFF0000            // Length
          )

          // IO BAR window
          QWordIO (
            ResourceProducer,
            MinFixed,
            MaxFixed,
            PosDecode,
            EntireRange,
            0x0,                  // Granularity
            0x0,                  // Min Base Address
            0xFFFF,               // Max Base Address
            0x1FFFFF0000,         // Translate
            0x10000               // Length
          )

        }) // Name(RBUF)
        Return (RBUF)
      } // Method(_CRS)

      // Root complex status
      Method (_STA, 0, Serialized) {
        Return (Zero)
      } // Method(_STA)

    } // PCI5

  } //Scope(_SB)
}
