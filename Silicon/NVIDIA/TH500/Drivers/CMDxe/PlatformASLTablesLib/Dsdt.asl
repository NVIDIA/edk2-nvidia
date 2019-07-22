/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */
DefinitionBlock ("dsdt.aml", "DSDT", 2, "NVIDIA", "TH500", 0x00000001)
{
  Device(CPU0) {
    Name(_HID, "ACPI0007")
    Name(_UID, Zero)
  }

  /*Device(CPU1) {
    Name(_HID, "ACPI0007")
    Name(_UID, One)
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
  }*/

  Device(COM0) {
    Name (_HID, "NVDA0100")
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate() {
      Memory32Fixed(ReadWrite, 0x0C280000, 0x1000)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x92 }
    })

    Name (_DSD, Package () {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () {"clock-frequency", 1843200},
        Package () {"reg-shift", 2},
        Package () {"reg-io-width", 4},
      }
    })
  }

  Device(SDC0) {
    Name(_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
    Name(_UID, 0)
    Name (_CCA, ONE)
    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, 0x03460000, 0x210)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x61 }
    })
  }

	//---------------------------------------------------------------------
	// ethernet @ 0x03B40000
	//---------------------------------------------------------------------
	Device(ETH0) {
		Name (_HID, "LNRO0003") /* SMC91x */
		Name (_UID, 0)

		Name (_CCA, 0) /* Non-Coherent DMA */

		Name(_CRS, ResourceTemplate() {
			Memory32Fixed(ReadWrite, 0x03B40000, 0x100)
			Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0xFA } /* SPI[218] == 250 */
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

}

