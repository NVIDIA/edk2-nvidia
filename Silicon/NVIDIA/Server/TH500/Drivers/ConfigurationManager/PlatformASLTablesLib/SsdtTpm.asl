/*
 * SSDT for TPM
 *
 * Copyright (c) 2023, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <TH500/TH500Definitions.h>

DefinitionBlock("SsdtTpm.aml", "SSDT", 2, "NVIDIA", "TH500", 0x00000001)
{
  Scope(_SB)
  {
    Device (TPM1)
    {
      Name (_HID, "PRP0001")
      Name (_UID, 1)
      Name (_STA, 0xF)

      Name(RBUF, ResourceTemplate() {
        SPISerialBus(0, PolarityLow, FourWireMode, 8,
                     ControllerInitiated, 1000000, ClockPolarityLow,
                     ClockPhaseFirst, "\\_SB.QSP1",)
      })

      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", "tpm_tis_spi" },
        }
      })

      Method (_CRS, 0, NotSerialized) {
        Return(RBUF)
      }
    }
  }
}
