/** @file
  SSDT for TH500 Socket 3 devices

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>

DefinitionBlock("SsdtSocket3.aml", "SSDT", 2, "NVIDIA", "TH500_S3", 0x00000001) {
  Scope(_SB) {
    //---------------------------------------------------------------------
    // MCF Devices
    //---------------------------------------------------------------------
    //MCF NVLINK Chiplet
    Device (MNV3)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x30)
    }
    //MCF C2C Chiplet
    Device (MC23)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x31)
    }
    //GPU SOC-HUB Chiplet
    Device (GSH3)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x32)
    }
    //PCIe SOC-HUB Chiplet
    Device (PSH3)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x33)
    }
  } //Scope(_SB)
}
