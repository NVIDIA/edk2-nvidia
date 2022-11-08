/** @file
  SSDT for TH500 Socket 1 devices

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>

DefinitionBlock("SsdtSocket1.aml", "SSDT", 2, "NVIDIA", "TH500_S1", 0x00000001) {
  Scope(_SB) {
    //---------------------------------------------------------------------
    // MCF Devices
    //---------------------------------------------------------------------
    //MCF NVLINK Chiplet
    Device (MNV1)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x10)
    }
    //MCF C2C Chiplet
    Device (MC21)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x11)
    }
    //GPU SOC-HUB Chiplet
    Device (GSH1)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x12)
    }
    //PCIe SOC-HUB Chiplet
    Device (PSH1)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x13)
    }
  } //Scope(_SB)
}
