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

    Device (I2C0) {
      Name (_HID, "NVDA0301")
      Name (_UID, 0)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0x3160000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x39 }
        FixedDMA (0x15, 0x0001, Width32bit,)
        FixedDMA (0x15, 0x0002, Width32bit,)
      })
    }

    Device (I2C1) {
      Name (_HID, "NVDA0301")
      Name (_UID, 1)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0xc240000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x3a }
        FixedDMA (0x16, 0x0001, Width32bit,)
        FixedDMA (0x16, 0x0002, Width32bit,)
      })
    }

    Device (I2C2) {
      Name (_HID, "NVDA0301")
      Name (_UID, 2)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0x3180000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x3b }
        FixedDMA (0x17, 0x0001, Width32bit,)
        FixedDMA (0x17, 0x0002, Width32bit,)
      })
    }

    Device (I2C3) {
      Name (_HID, "NVDA0301")
      Name (_UID, 3)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0x3190000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x3c }
        FixedDMA (0x1A, 0x0001, Width32bit,)
        FixedDMA (0x1A, 0x0002, Width32bit,)
      })
    }

    Device (I2C5) {
      Name (_HID, "NVDA0301")
      Name (_UID, 4)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0x31b0000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x3e }
        FixedDMA (0x1E, 0x0001, Width32bit,)
        FixedDMA (0x1E, 0x0002, Width32bit,)
      })
    }

    Device (I2C6) {
      Name (_HID, "NVDA0301")
      Name (_UID, 5)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0x31c0000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x3f }
        FixedDMA (0x1C, 0x0001, Width32bit,)
        FixedDMA (0x1C, 0x0002, Width32bit,)
      })
    }

    Device (I2C7) {
      Name (_HID, "NVDA0301")
      Name (_UID, 6)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0xc250000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x40 }
        FixedDMA (0x00, 0x0001, Width32bit,)
        FixedDMA (0x00, 0x0002, Width32bit,)
      })
    }

    Device (I2C8) {
      Name (_HID, "NVDA0301")
      Name (_UID, 7)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0x31e0000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x41 }
        FixedDMA (0x1F, 0x0001, Width32bit,)
        FixedDMA (0x1F, 0x0002, Width32bit,)
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

    Device(SPI0) {
      Name (_HID, "NVDA0513")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name (_CRS, ResourceTemplate () {
        Memory32Fixed(ReadWrite, 0x03210000, 0x10000)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x44 }
        FixedDMA (0x0F, 0x0001, Width32bit, )
        FixedDMA (0x0F, 0x0002, Width32bit, )
      })

      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package (2) {"spi-max-frequency", 65000000},
        }
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

  //---------------------------------------------------------------------
  // ethernet@2490000
  //---------------------------------------------------------------------
  Device(ETH0) {
    Name (_HID, "PRP0001")
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate() {
      Memory32Fixed(ReadWrite, 0x2490000, 0x10000)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0xe2 }
    })

    Name (_DSD, Package () {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () {"mac-address", Package () { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }},
        Package () {"compatible", Package () {"nvidia,tegra186-eqos"}},
        Package () {"nvidia,num-dma-chans", 0x1},
        Package () {"nvidia,dma-chans", 0x0},
        Package () {"nvidia,num-mtl-queues", 0x1},
        Package () {"nvidia,mtl-queues", 0x0},
        Package () {"nvidia,rx-queue-prio", 0x2},
        Package () {"nvidia,dcs-enable", 0},
        Package () {"nvidia,tx-queue-prio", 0x0},
        Package () {"nvidia,rx_riwt", 0x200},
        Package () {"nvidia,ptp_ref_clock_speed", 0x12a05f20},
        Package () {"nvidia,queue_prio", Package () {0x0, 0x1, 0x2, 0x3}},
        Package () {"nvidia,rxq_enable_ctrl", 0x2},
        Package () {"nvidia,pause_frames", 0x0},
        Package () {"phy-mode", "rgmii"},
        Package () {"nvidia,max-platform-mtu", 0x2328},
        Package () {"snps,write-requests", 0x1},
        Package () {"snps,read-requests", 0x3},
        Package () {"snps,burst-map ", 0x7},
        Package () {"snps,txpbl", 0x10},
        Package () {"snps,rxpbl", 0x8}
      }
    })
  }
}

