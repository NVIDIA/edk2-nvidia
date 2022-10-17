/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2020 - 2022, NVIDIA Corporation. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <TH500/TH500Definitions.h>

DefinitionBlock ("dsdt.aml", "DSDT", 2, "NVIDIA", "TH500", 0x00000001)
{
  Scope(_SB) {

    //---------------------------------------------------------------------
    // GED to receive RAS events
    //---------------------------------------------------------------------
    Device(GED0) {
      Name(_HID, "ACPI0013") /* Generic Event Device */
      Name(_UID, 0)

      Name (_CRS, ResourceTemplate () {
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {
          TH500_SW_IO2_INTR
        }
      })

      OperationRegion (LIC2, SystemMemory, TH500_SW_IO2_BASE, TH500_SW_IO2_SIZE)
      Field (LIC2, DWordAcc, NoLock, Preserve)
      {
        STAT, 32,
        SET, 32,
        CLR, 32,
        RSVD, 32,
        DALO, 32,
        DAHI, 32,
      }

      Method (_INI)
      {
        Store (0x1, CLR)
      }

      Method (_EVT, 1) {
        Switch(Arg0) {
          Case(TH500_SW_IO2_INTR) {
            Store (0x1, CLR)
            Notify (\_SB.RASD, 0x80)
          }
        }
      }

      Method (_STA) {
        Return (0xF)
      }
    }

    //---------------------------------------------------------------------
    // HWPM - HW Performance Monitoring
    //---------------------------------------------------------------------
    Device(HWP0) {
      Name (_HID, "NVDA2006")
      Name (_UID, 0)
      Name (_CCA, ZERO)

      Name(_CRS, ResourceTemplate() {
        Memory32Fixed(ReadWrite, 0x13E00000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E01000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E02000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E03000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E04000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E05000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E06000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E07000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E08000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E09000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E0A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E0B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E0C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E0D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E0E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E0F000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E10000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E11000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E12000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E13000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E14000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E15000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E16000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E17000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E18000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E19000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E1A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E1B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E1C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E1D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E1E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E1F000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E20000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E21000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E22000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E23000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E24000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E25000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E26000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E27000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E28000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E29000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E2A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E2B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E2C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E2D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E2E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E2F000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E30000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E31000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E32000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E33000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E34000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E35000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E36000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E37000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E38000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E39000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E3A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E3B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E3C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E3D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E3E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E3F000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E40000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E41000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E42000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E43000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E44000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E45000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E46000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E47000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E48000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E49000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E4A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E4B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E4C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E4D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E4E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E4F000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E50000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E51000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E52000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E53000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E54000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E55000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E56000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E57000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E58000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E59000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E5A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E5B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E5C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E5D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E5E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E5F000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E60000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E61000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E62000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E63000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E64000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E65000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E66000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E67000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E68000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E69000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E6A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E6B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E6C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E6D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E6E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E6F000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E70000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E71000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E72000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E73000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E74000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E75000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E76000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E77000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E78000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E79000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E7A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E7B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E7C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E7D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E7E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E7F000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E80000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E81000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E82000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E83000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E84000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E85000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E86000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E87000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E88000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E89000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E8A000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E8B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E8C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E8D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E8E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13EF0000, 0x2000)
        Memory32Fixed(ReadWrite, 0x13EF2000, 0x1000)
      })
    }

    //---------------------------------------------------------------------
    // PSC Device for PSCFW testing
    // Creates e.g. /sys/devices/LNXSYSTM\:00/subsystem/devices/NVDA2003\:00
    //---------------------------------------------------------------------
    Device (PSC0)
    {
      Name (_HID, "NVDA2003") /* ACPI PSC Device */
      Name (_UID, 0) // optional; required if there are other _HIDs with NVDA2003

      Name (_CCA, 1) // coherent DMA, explicit setting

      Name(_CRS, ResourceTemplate() {
        Memory32Fixed(ReadWrite,0x0e860000, 0x200000) // mbox-regs in DT: PSC_MBOX_VM* address space
        // PSC MBOX Interrupts
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) {
            215, // NV_ADDRESS_MAP_PSC_MB5_GIC_ID, VM0
            216,
            217,
            218,
            219,
            220,
            221,
            222 // NV_ADDRESS_MAP_PSC_MB12_GIC_ID, VM7
        }
        Memory32Fixed(ReadWrite,0x0e80201c, 8) // PSC_EXT_CFG_SIDTABLE_VM0_0, PSC_EXT_CFG_SIDCONFIG_VM0_0 (extcfg in DT)
      })

      // _STA(): Report device status (0xF: Present, 0x0: Absent)
      Method (_STA) {
        Return (0xF)
      }
    }

    //---------------------------------------------------------------------
    // Standard error device
    //---------------------------------------------------------------------
    Device (\_SB.RASD)
    {
      Name (_HID, "PNP0C33") /* ACPI Error Device */
      Name (_UID, 0)

      Method (_STA) {
        Return (0xF)
      }
    }

    //-------------------------------------------------------------
    // _OSC(): Operating System Capabilities
    //   Arg0 - UUID (Buffer)
    //   Arg1 - Revision ID (Integer): 1
    //   Arg2 - Count of Entries in Arg3 (Integer): 2
    //   Arg3 - DWORD capabilities (Buffer):
    //     First DWORD  (DWD1): status
    //     Second DWORD (DWD2): capabilities bitmap
    //-------------------------------------------------------------
    Method (_OSC, 4) {
      CreateDWordField (Arg3, 0x0, DWD1)
      CreateDWordField (Arg3, 0x4, DWD2)

      // Verify revision (Arg1)
      If (LNotEqual (Arg1, 0x1)) {
        Store (0x8, DWD1) //Bit [3] - Unrecognized Revision
        Return (Arg3)
      }

      // ACPI UUID for "Platform-Wide OSPM Capabilities"
      If (LEqual (Arg0, ToUUID("0811B06E-4A27-44F9-8D60-3CBBC22E7B48"))) {
        Or (DWD2, 0x10, DWD2) // Bit [4] - APEI Support
        Return (Arg3)
      }
      // WHEA UUID for APEI
      ElseIf (LEqual (Arg0, ToUUID("ED855E0C-6C90-47BF-A62A-26DE0FC5AD5C"))) {
        Or (DWD2, 0x10, DWD2) // Bit [4] - APEI Support
        Return (Arg3)
      }
      // Unsupported UUID
      Else {
        Store (0x4, DWD1) //Bit [2] - Unrecognized UUID
        Return (Arg3)
      }
    }

    //---------------------------------------------------------------------
    // QSPI Device
    //---------------------------------------------------------------------
    Device (QSP1)
    {
      Name (_HID, "NVDA1513")
      Name (_UID, 1)
      Name (_CCA, 1) // coherent DMA, explicit setting

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0x3250000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x3a }
      })
      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () {"spi-max-frequency", 10000000},
        }
      })
    }

    //---------------------------------------------------------------------
    // SMMU Test Device
    //---------------------------------------------------------------------
    Device (TEST)
    {
      Name (_HID, "NVDA200B")
    }
  }
}
