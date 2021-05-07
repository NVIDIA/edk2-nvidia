DefinitionBlock("SdcTemplate.aml", "SSDT", 2, "NVIDIA", "SDCTEMP", 0x00000001) {
  Device(SDCT) {
    Name (_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, 0xFFFFFFFF, 0xFFFFFFFF, REG0)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive, , , INT0) { 0xFF }
    })
  }
}
