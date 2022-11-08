/** @file
  SSDT for TH500 Socket 2 devices

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>

DefinitionBlock("SsdtSocket2.aml", "SSDT", 2, "NVIDIA", "TH500_S2", 0x00000001) {
  Scope(_SB) {
    //---------------------------------------------------------------------
    // MCF Devices
    //---------------------------------------------------------------------
    //MCF NVLINK Chiplet
    Device (MNV2)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x20)
    }
    //MCF C2C Chiplet
    Device (MC22)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x21)
    }
    //GPU SOC-HUB Chiplet
    Device (GSH2)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x22)
    }
    //PCIe SOC-HUB Chiplet
    Device (PSH2)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x23)
    }
  } //Scope(_SB)
}
