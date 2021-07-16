/*
 * Portions provided under the following terms:
 * Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 */

#include <T194/T194Definitions.h>

DefinitionBlock("SsdtAhci.aml", "SSDT", 2, "NVIDIA", "ACHI194", 0x00000001) {
  Scope(_SB) {
    Device(SATA) //AHCI- compatible SATA controller
    {
      Name(_HID, "1B4B9171")
      Name (_CCA, 1)
      Name(_CLS, Package (3)
      {
        0x01, // Base Class (01h == Mass Storage)
        0x06, // Sub-Class (06h == SATA)
        0x01, // Programming Interface (01h == AHCI)
      })
      Name(_CRS, ResourceTemplate()
      {
        QWordMemory (
          ResourceConsumer, PosDecode,
          MinFixed, MaxFixed,
          Cacheable, ReadWrite,
          0x00000000,           // Granularity
          0x1230000000,         // Min Base Address
          0x12300001ff,         // Max Base Address
          0x00000000,           // Translate
          0x00000200            // Length
        )
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 77 }
      })
    }
  } //Scope(_SB)
}
