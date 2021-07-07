/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2020 - 2021, NVIDIA Corporation. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <T194/T194Definitions.h>

DefinitionBlock ("dsdt.aml", "DSDT", 2, "NVIDIA", "TEGRA194", 0x00000001)
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

    Device(FAN) {
      Name (_HID, "PNP0C0B")
      Name (_UID, 0)
      Name (_CCA, ZERO)
      Name (_STA, 0)

      Name (_FIF, Package () {
        0,  // revision
        0,  // fine grain control on
        5,  // step size
        0   // no notification on low speed
      })

      /*
       * Fan states. Need to update rpm & noise data in table.
       * TripPoint disabled till we have thermal support
       */
      Name (_FPS, Package () {
        0,  //revision
        Package () { 0x00, 0x0FFFFFFFF,    0, 0xFFFFFFFF, 0xFFFFFFFF },
        Package () { 0x40, 0,           1250, 0xFFFFFFFF, 0xFFFFFFFF },
        Package () { 0x80, 0x0FFFFFFFF, 2500, 0xFFFFFFFF, 0xFFFFFFFF },
        Package () { 0xc0, 0x0FFFFFFFF, 3750, 0xFFFFFFFF, 0xFFFFFFFF },
        Package () { 0xff, 0x0FFFFFFFF, 5000, 0xFFFFFFFF, 0xFFFFFFFF }
      })

      /* PWM FAN register fields */
      OperationRegion(FANR, SystemMemory, 0xFFFFFFFF, 4)
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
          Store (0, PMON)
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
        Return (Package() { 0, PWM0, 0xFFFFFFFF })
      }
    }

    Device (BPMP) {
      OperationRegion (BPTX, SystemMemory, BPMP_TX_MAILBOX, 0x1000)
      Field (BPTX, AnyAcc, NoLock, Preserve) {
        TWCT, 32,
        TSTA, 32,
        Offset (64),
        TRCT, 32,
        Offset (128),
        TMRQ, 32,
        TFLA, 32,
        TDAT, 31680
      }

      OperationRegion (BPRX, SystemMemory, BPMP_RX_MAILBOX, 0x1000)
      Field (BPRX, AnyAcc, NoLock, Preserve) {
        RWCT, 32,
        RSTA, 32,
        Offset (64),
        RRCT, 32,
        Offset (128),
        RERR, 32,
        RFLG, 32,
        RDAT, 31680
      }

      OperationRegion (DRBL, SystemMemory, 0x03C90300, 0x100)
      Field (DRBL, AnyAcc, NoLock, Preserve) {
        TRIG, 4,
        ENA,  4,
        RAW,  4,
        PEND, 4
      }

      Method (BIPC, 2, Serialized, 0, PkgObj, {IntObj, BuffObj}) {
        TMRQ = Arg0
        TFLA = One
        TDAT = Arg1
        Increment (TWCT)
        Store (One, TRIG)

        While (RWCT == RRCT) {
          Sleep (10)
        }
        Increment (RRCT)
        Return (Package() {RERR, RDAT})
      }

      Method (TEMP, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = Buffer(8){}
        CreateDWordField (Local0, 0x00, CMD)
        CreateDWordField (Local0, 0x04, ZONE)
        CMD = ZONE_TEMP
        ZONE = Arg0
        Local1 = \_SB.BPMP.BIPC (MRQ_THERMAL, Local0)
        CreateDWordField (DerefOf (Index (Local1, 1)), 0x00, TEMP)
        Local3 = TEMP / 100
        Local4 = 2732
        Add (Local3, Local4, Local3)
        Return (Local3)
      }
    }

    ThermalZone (TCPU) {
      Method(_TMP) { Return (\_SB.BPMP.TEMP (CPU_TEMP_ZONE) )} // get current temp
      Method(_AC0) { Return (500 + 2732) } // fan active temp 50c
      Name(_AL0, Package(){\_SB.FAN}) // fan is act cool dev
      Method(_CRT) { Return (965 + 2732) } // get critical temp 96.5c
      Method(_HOT) { Return (700 + 2732) } // get hot temp 70c
      Name(_TZP, TEMP_POLL_TIME)
      Name (_STR, Unicode ("System thermal zone"))
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

    Device (GED1) {
      Name (_HID, "ACPI0013")
      Name (_UID, 0)

      Name (_CRS, ResourceTemplate () {
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x58 }
      })
      OperationRegion (GP24, SystemMemory, 0xc2f1080, 0x20)
      Field (GP24, DWordAcc, NoLock, Preserve)
      {
        ENAC,   32,
        DBCT,   32,
        IVAL,   32,
        OCTL,   32,
        OVAL,   32,
        INTC,   32,
      }

      Method (_INI) {
        ENAC = 0x6D //Debounce, Interrupt enabled, both edges, input
        DBCT = 0x0A //Debounce value
      }

      Method (_EVT, 0x1) {
        INTC = 0x1 //Clear Interrupt
        If (ToInteger (IVAL) == 0) {
          Notify (\_SB.PWRB, 0x80)
        }
      }
    }

    Device (PWRB) {
      Name (_HID, "PNP0C0C")
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

    Device(USB0) {
      Name (_HID, "NVDA0214")
      Name (_CID, "PNP0D10")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate () {
        Memory32Fixed(ReadWrite, 0x03610000, 0x40000)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0xc3 }
      })
    }

    //---------------------------------------------------------------------
    // ethernet@2490000
    //---------------------------------------------------------------------
    Device(ETH0) {
      Name (_HID, "NVDA1160")
      Name (_CID, "PRP0001")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate() {
        Memory32Fixed(ReadWrite, T194_ETHERNET_BASE_ADDR, T194_ETHERNET_CAR_SIZE)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { T194_ETHERNET_INTR }
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
          Package () {"phy-mode", "rgmii-id"},
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
}

