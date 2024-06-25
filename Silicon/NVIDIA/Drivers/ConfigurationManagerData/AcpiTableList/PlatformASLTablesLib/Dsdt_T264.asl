/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

DefinitionBlock ("dsdt_t264.aml", "DSDT", 2, "NVIDIA", "TEGRA264", 0x00000001)
{
  Scope(_SB) {
    Method (_OSC, 4, Serialized)  { // _OSC: Operating System Capabilities
      CreateDWordField (Arg3, 0x00, STS0)
      CreateDWordField (Arg3, 0x04, CAP0)
      If ((Arg0 == ToUUID ("0811b06e-4a27-44f9-8d60-3cbbc22e7b48") /* Platform-wide Capabilities */)) {
        If (!(Arg1 == One)) {
          STS0 &= ~0x1F
          STS0 |= 0x0A
        } Else {
          If ((CAP0 & 0x100)) {
            CAP0 &= ~0x100 /* No support for OS Initiated LPI */
            STS0 &= ~0x1F
            STS0 |= 0x12
          }
        }
      } Else {
        STS0 &= ~0x1F
        STS0 |= 0x06
      }
      Return (Arg3)
    }

    Device(MRQ0) {
      Name (_HID, "NVDA2001")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate() {
        // 1. HSP_TOP0 Registers
        Memory32Fixed (ReadWrite, 0x8800000, 0xd0000)
        // 2. IVC Tx Pool, patched at runtime (using bpmp-shmem base + {0x100-0x1FF, 0x200-0x2FF, 0x300-0x3FF, 0xD00-0xDFF})
        QWordMemory (
          ResourceConsumer,   // ResourceUsage
          PosDecode,          // Decode
          MinFixed,           // IsMinFixed
          MaxFixed,           // IsMaxFixed
          NonCacheable,       // Cacheable
          ReadWrite,          // ReadAndWrite
          0x0,                // AddressGranularity
          0x100,              // AddressMinimum       (+ bpmp-shmem base)
          0xfff,              // AddressMaximum       (+ bpmp-shmem base)
          0,                  // AddressTranslation
          0xf00,              // RangeLength
          ,                   // ResourceSourceIndex
          ,                   // ResourceSource
          TX,                 // DescriptorName
          ,                   // MemoryRangeType
                              // TranslationType
          ) // QWordMemory

        // 3. IVC Rx Pool, patched at runtime (using bpmp-shmem base + 0x1000 + {0x100-0x1FF, 0x200-0x2FF, 0x300-0x3FF, 0xD00-0xDFF})
        QWordMemory (
          ResourceConsumer,   // ResourceUsage
          PosDecode,          // Decode
          MinFixed,           // IsMinFixed
          MaxFixed,           // IsMaxFixed
          NonCacheable,       // Cacheable
          ReadWrite,          // ReadAndWrite
          0x0,                // AddressGranularity
          0x1100,             // AddressMinimum        // + bpmp-shmem base
          0x1fff,             // AddressMaximum        // + bpmp-shmem base
          0,                  // AddressTranslation
          0xf00,              // RangeLength
          ,                   // ResourceSourceIndex
          ,                   // ResourceSource
          RX,                 // DescriptorName
          ,                   // MemoryRangeType
                              // TranslationType
          ) // QWordMemory
        // 4. HSP_TOP0_CCPLEX_DBELL (0x26c + 0x20)
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x28c }
      })
    }
  }
}
