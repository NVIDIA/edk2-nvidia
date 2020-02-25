/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */
DefinitionBlock ("dsdt.aml", "DSDT", 1, "NVIDIA", "TEGRA194", 0x00000001)
{
  Scope(_SB) {
    Device(CPU0) {
      Name(_HID, "ACPI0007")
      Name(_UID, 0)
    }

    Device(CPU1) {
      Name(_HID, "ACPI0007")
      Name(_UID, 1)
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
    }

    Device(CPU6) {
      Name(_HID, "ACPI0007")
      Name(_UID, 6)
    }

    Device(CPU7) {
      Name(_HID, "ACPI0007")
      Name(_UID, 7)
    }

    Device(COM0) {
      Name (_HID, "NVDA0100")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate() {
        Memory32Fixed(ReadWrite, FixedPcdGet64 (PcdTegra16550UartBaseT194), 0x10000)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x92 }
      })
      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () {"clock-frequency", 1843200}
        }
      })
    }

    Device(FAN) {
      Name (_HID, "PNP0C0B")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name (_FIF, Package () {
        0,  // revision
        0,  // fine grain control off
        5,  // step size
        0   // no notification on low speed
      })

      /*
       * Fan states. Need to update rpm & noise data in table.
       * TripPoint disabled till we have thermal support
       */
      Name (_FPS, Package () {
        0,  //revision
        Package () {   0, 0x0FFFFFFFF,    0, 0xFFFFFFFF, 0xFFFFFFFF },
        Package () {  64, 0x0FFFFFFFF, 1250, 0xFFFFFFFF, 0xFFFFFFFF },
        Package () { 128, 0x0FFFFFFFF, 2500, 0xFFFFFFFF, 0xFFFFFFFF },
        Package () { 192, 0x0FFFFFFFF, 3750, 0xFFFFFFFF, 0xFFFFFFFF },
        Package () { 256, 0x0FFFFFFFF, 5000, 0xFFFFFFFF, 0xFFFFFFFF }
      })

      /* PWM4 FAN register fields */
      OperationRegion(FANR, SystemMemory, 0xC340000, 4)
      Field(FANR, DWordAcc, NoLock, Preserve) {
        PFM0, 13,
        , 3,
        PWM0, 9,
        , 6,
        PMON, 1,
      }

      Method (_FSL, 1) {
        If (Arg0)
        {
          Store (Arg0, PWM0)
          Store (1, PMON)
        }
        Else
        {
          Store (0, PWM0)
          Store (0, PMON)
        }
      }

      Method (_FST) {
        Name (PCTR, 0xff)
        Store(PWM0, PCTR)
        Name (FST0, Package() { 0, PCTR, 0xFFFFFFFF })
        Return (FST0)
      }
    }

    Device (DMA0) {
        Name (_HID, "NVDA0209")
        Name (_UID, 0)

        Name (_CRS, ResourceTemplate () {
            Memory32Fixed(ReadWrite, 0x2600000, 0x210000)
            Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x6B,
                                                                        0x6C,
                                                                        0x6D,
                                                                        0x6E,
                                                                        0x6F,
                                                                        0x70,
                                                                        0x71,
                                                                        0x72,
                                                                        0x73,
                                                                        0x74,
                                                                        0x75,
                                                                        0x76,
                                                                        0x77,
                                                                        0x78,
                                                                        0x79,
                                                                        0x7A,
                                                                        0x7B,
                                                                        0x7C,
                                                                        0x7D,
                                                                        0x7E,
                                                                        0x7F,
                                                                        0x80,
                                                                        0x81,
                                                                        0x82,
                                                                        0x83,
                                                                        0x84,
                                                                        0x85,
                                                                        0x87,
                                                                        0x88,
                                                                        0x89,
                                                                        0x8A,
                                                                        0x8B}
        })

        Name (_DSD, Package () {
          ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
          Package () {
            Package (2) {"nvidia,start-dma-channel-index", 1},
            Package (2) {"dma-channels", 31}
        }
      })
    }


    Device (GPI0) {
        Name (_HID, "NVDA0308")
        Name (_UID, 0)

        Name (_CRS, ResourceTemplate () {
            Memory32Fixed(ReadWrite, 0x2210000, 0x10000)
            Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x140,
                                                                        0x141,
                                                                        0x142,
                                                                        0x143,
                                                                        0x144,
                                                                        0x145,
                                                                        0x146,
                                                                        0x147,
                                                                        0x148,
                                                                        0x149,
                                                                        0x14a,
                                                                        0x14b,
                                                                        0x14c,
                                                                        0x14e,
                                                                        0x14f,
                                                                        0x150,
                                                                        0x151,
                                                                        0x152,
                                                                        0x153,
                                                                        0x154,
                                                                        0x155,
                                                                        0x156,
                                                                        0x157,
                                                                        0x158,
                                                                        0x159,
                                                                        0x15a,
                                                                        0x15b,
                                                                        0x15c,
                                                                        0x15e,
                                                                        0x15f,
                                                                        0x160,
                                                                        0x161,
                                                                        0x162,
                                                                        0x163,
                                                                        0x164,
                                                                        0x165,
                                                                        0x166,
                                                                        0x167,
                                                                        0x168,
                                                                        0x169,
                                                                        0x16a,
                                                                        0x16b,
                                                                        0x16c,
                                                                        0x16e,
                                                                        0x16f}
        })
    }

    Device (GPI1) {
        Name (_HID, "NVDA0408")
        Name (_UID, 0)

        Name (_CRS, ResourceTemplate () {
            Memory32Fixed(ReadWrite, 0xc2f1000, 0x1000)
            Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x58, 0x59, 0x5a, 0x5b}
        })
    }

    Device(SDC0) {
      Name (_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate () {
        Memory32Fixed(ReadWrite, 0x03460000, 0x20000)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x61 }
      })
    }

    Device(SDC3) {
      Name (_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate () {
        Memory32Fixed(ReadWrite, 0x03400000, 0x20000)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x5e }
      })
    }

    Device(USB0) {
      Name (_HID, "ARMH0D10")
      Name (_CID, "PNP0D10")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate () {
        Memory32Fixed(ReadWrite, 0x03610000, 0x40000)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0xc3 }
      })
    }
  }
}

