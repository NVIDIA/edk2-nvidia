/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2020 - 2023, NVIDIA Corporation. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <TH500/TH500Definitions.h>

DefinitionBlock ("dsdt.aml", "DSDT", 2, "NVIDIA", "TH500", 0x00000001)
{
  Scope(_SB)
  {
    //---------------------------------------------------------------------
    // GED to receive RAS events
    //---------------------------------------------------------------------
    Device(GED0)
    {
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

    // ---------------------------------------------------------------------
    // GED for PCIe DPC
    // ---------------------------------------------------------------------
    Device (GED1)
    {
      Name (_HID, "ACPI0013") /* Generic Event Device */
      Name (_UID, 1)

      // In the case of Socket-0, PCIe controller node names take the shape of
      // "PCIx" where 'x' represent the controller/instance number.

      // Socket - 0
      External (\_SB.PCI0.RP00)
      External (\_SB.PCI1.RP00)
      External (\_SB.PCI2.RP00)
      External (\_SB.PCI3.RP00)
      External (\_SB.PCI4.RP00)
      External (\_SB.PCI5.RP00)
      External (\_SB.PCI6.RP00)
      External (\_SB.PCI7.RP00)
      External (\_SB.PCI8.RP00)
      External (\_SB.PCI9.RP00)

      // Starting from Socket-1, PCIe controller node names take the shape of
      // "PCyx" where 'x' represent the controller/instance number and 'y'
      // represents the socket ID.

      // Socket - 1
      External (\_SB.PC10.RP00)
      External (\_SB.PC11.RP00)
      External (\_SB.PC12.RP00)
      External (\_SB.PC13.RP00)
      External (\_SB.PC14.RP00)
      External (\_SB.PC15.RP00)
      External (\_SB.PC16.RP00)
      External (\_SB.PC17.RP00)
      External (\_SB.PC18.RP00)
      External (\_SB.PC19.RP00)

      // Socket - 2
      External (\_SB.PC20.RP00)
      External (\_SB.PC21.RP00)
      External (\_SB.PC22.RP00)
      External (\_SB.PC23.RP00)
      External (\_SB.PC24.RP00)
      External (\_SB.PC25.RP00)
      External (\_SB.PC26.RP00)
      External (\_SB.PC27.RP00)
      External (\_SB.PC28.RP00)
      External (\_SB.PC29.RP00)

      // Socket - 3
      External (\_SB.PC30.RP00)
      External (\_SB.PC31.RP00)
      External (\_SB.PC32.RP00)
      External (\_SB.PC33.RP00)
      External (\_SB.PC34.RP00)
      External (\_SB.PC35.RP00)
      External (\_SB.PC36.RP00)
      External (\_SB.PC37.RP00)
      External (\_SB.PC38.RP00)
      External (\_SB.PC39.RP00)

      Name (
        _CRS,
        ResourceTemplate () {
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {
          TH500_SW_IO3_INTR_SOCKET_0
        }
      }
        )

      // The "SMR1" named object in the OperationRegion would be patched
      // by UEFI to have the correct address of the NS shared memory region.
      OperationRegion (SMR1, SystemMemory, 0xFFFFFFFFFFFFFFFF, 0x280)
      Field (SMR1, DWordAcc, NoLock, Preserve) {
        // DP = IsInDPC
        // SK = Socket ID
        // SG = Segment ID
        // ES = Error Source

        // Socket-0
        DP00, 32, SK00, 32, SG00, 32, ES00, 32,
        DP01, 32, SK01, 32, SG01, 32, ES01, 32,
        DP02, 32, SK02, 32, SG02, 32, ES02, 32,
        DP03, 32, SK03, 32, SG03, 32, ES03, 32,
        DP04, 32, SK04, 32, SG04, 32, ES04, 32,
        DP05, 32, SK05, 32, SG05, 32, ES05, 32,
        DP06, 32, SK06, 32, SG06, 32, ES06, 32,
        DP07, 32, SK07, 32, SG07, 32, ES07, 32,
        DP08, 32, SK08, 32, SG08, 32, ES08, 32,
        DP09, 32, SK09, 32, SG09, 32, ES09, 32,

        // Socket-1
        DP10, 32, SK10, 32, SG10, 32, ES10, 32,
        DP11, 32, SK11, 32, SG11, 32, ES11, 32,
        DP12, 32, SK12, 32, SG12, 32, ES12, 32,
        DP13, 32, SK13, 32, SG13, 32, ES13, 32,
        DP14, 32, SK14, 32, SG14, 32, ES14, 32,
        DP15, 32, SK15, 32, SG15, 32, ES15, 32,
        DP16, 32, SK16, 32, SG16, 32, ES16, 32,
        DP17, 32, SK17, 32, SG17, 32, ES17, 32,
        DP18, 32, SK18, 32, SG18, 32, ES18, 32,
        DP19, 32, SK19, 32, SG19, 32, ES19, 32,

        // Socket-2
        DP20, 32, SK20, 32, SG20, 32, ES20, 32,
        DP21, 32, SK21, 32, SG21, 32, ES21, 32,
        DP22, 32, SK22, 32, SG22, 32, ES22, 32,
        DP23, 32, SK23, 32, SG23, 32, ES23, 32,
        DP24, 32, SK24, 32, SG24, 32, ES24, 32,
        DP25, 32, SK25, 32, SG25, 32, ES25, 32,
        DP26, 32, SK26, 32, SG26, 32, ES26, 32,
        DP27, 32, SK27, 32, SG27, 32, ES27, 32,
        DP28, 32, SK28, 32, SG28, 32, ES28, 32,
        DP29, 32, SK29, 32, SG29, 32, ES29, 32,

        // Socket-3
        DP30, 32, SK30, 32, SG30, 32, ES30, 32,
        DP31, 32, SK31, 32, SG31, 32, ES31, 32,
        DP32, 32, SK32, 32, SG32, 32, ES32, 32,
        DP33, 32, SK33, 32, SG33, 32, ES33, 32,
        DP34, 32, SK34, 32, SG34, 32, ES34, 32,
        DP35, 32, SK35, 32, SG35, 32, ES35, 32,
        DP36, 32, SK36, 32, SG36, 32, ES36, 32,
        DP37, 32, SK37, 32, SG37, 32, ES37, 32,
        DP38, 32, SK38, 32, SG38, 32, ES38, 32,
        DP39, 32, SK39, 32, SG39, 32, ES39, 32
      }

      OperationRegion (LIC3, SystemMemory, TH500_SW_IO3_BASE_SOCKET_0, TH500_SW_IO3_SIZE)
      Field (LIC3, DWordAcc, NoLock, Preserve) {
        STAT, 32,
        SET, 32,
        CLR, 32,
        RSVD, 32,
        DALO, 32,
        DAHI, 32,
      }

      Method (_EVT, 1) {
        Switch (Arg0) {
          Case (TH500_SW_IO3_INTR_SOCKET_0) {
            // Socket-0
            If (LEqual (DP00, 0x1)) {
              Notify (\_SB.PCI0.RP00, 0xF)
            }
            If (LEqual (DP01, 0x1)) {
              Notify (\_SB.PCI1.RP00, 0xF)
            }
            If (LEqual (DP02, 0x1)) {
              Notify (\_SB.PCI2.RP00, 0xF)
            }
            If (LEqual (DP03, 0x1)) {
              Notify (\_SB.PCI3.RP00, 0xF)
            }
            If (LEqual (DP04, 0x1)) {
              Notify (\_SB.PCI4.RP00, 0xF)
            }
            If (LEqual (DP05, 0x1)) {
              Notify (\_SB.PCI5.RP00, 0xF)
            }
            If (LEqual (DP06, 0x1)) {
              Notify (\_SB.PCI6.RP00, 0xF)
            }
            If (LEqual (DP07, 0x1)) {
              Notify (\_SB.PCI7.RP00, 0xF)
            }
            If (LEqual (DP08, 0x1)) {
              Notify (\_SB.PCI8.RP00, 0xF)
            }
            If (LEqual (DP09, 0x1)) {
              Notify (\_SB.PCI9.RP00, 0xF)
            }

            // Socket-1
            If (LEqual (DP10, 0x1)) {
              Notify (\_SB.PC10.RP00, 0xF)
            }
            If (LEqual (DP11, 0x1)) {
              Notify (\_SB.PC11.RP00, 0xF)
            }
            If (LEqual (DP12, 0x1)) {
              Notify (\_SB.PC12.RP00, 0xF)
            }
            If (LEqual (DP13, 0x1)) {
              Notify (\_SB.PC13.RP00, 0xF)
            }
            If (LEqual (DP14, 0x1)) {
              Notify (\_SB.PC14.RP00, 0xF)
            }
            If (LEqual (DP15, 0x1)) {
              Notify (\_SB.PC15.RP00, 0xF)
            }
            If (LEqual (DP16, 0x1)) {
              Notify (\_SB.PC16.RP00, 0xF)
            }
            If (LEqual (DP17, 0x1)) {
              Notify (\_SB.PC17.RP00, 0xF)
            }
            If (LEqual (DP18, 0x1)) {
              Notify (\_SB.PC18.RP00, 0xF)
            }
            If (LEqual (DP19, 0x1)) {
              Notify (\_SB.PC19.RP00, 0xF)
            }

            // Socket-2
            If (LEqual (DP20, 0x1)) {
              Notify (\_SB.PC20.RP00, 0xF)
            }
            If (LEqual (DP21, 0x1)) {
              Notify (\_SB.PC21.RP00, 0xF)
            }
            If (LEqual (DP22, 0x1)) {
              Notify (\_SB.PC22.RP00, 0xF)
            }
            If (LEqual (DP23, 0x1)) {
              Notify (\_SB.PC23.RP00, 0xF)
            }
            If (LEqual (DP24, 0x1)) {
              Notify (\_SB.PC24.RP00, 0xF)
            }
            If (LEqual (DP25, 0x1)) {
              Notify (\_SB.PC25.RP00, 0xF)
            }
            If (LEqual (DP26, 0x1)) {
              Notify (\_SB.PC26.RP00, 0xF)
            }
            If (LEqual (DP27, 0x1)) {
              Notify (\_SB.PC27.RP00, 0xF)
            }
            If (LEqual (DP28, 0x1)) {
              Notify (\_SB.PC28.RP00, 0xF)
            }
            If (LEqual (DP29, 0x1)) {
              Notify (\_SB.PC29.RP00, 0xF)
            }

            // Socket-3
            If (LEqual (DP30, 0x1)) {
              Notify (\_SB.PC30.RP00, 0xF)
            }
            If (LEqual (DP31, 0x1)) {
              Notify (\_SB.PC31.RP00, 0xF)
            }
            If (LEqual (DP32, 0x1)) {
              Notify (\_SB.PC32.RP00, 0xF)
            }
            If (LEqual (DP33, 0x1)) {
              Notify (\_SB.PC33.RP00, 0xF)
            }
            If (LEqual (DP34, 0x1)) {
              Notify (\_SB.PC34.RP00, 0xF)
            }
            If (LEqual (DP35, 0x1)) {
              Notify (\_SB.PC35.RP00, 0xF)
            }
            If (LEqual (DP36, 0x1)) {
              Notify (\_SB.PC36.RP00, 0xF)
            }
            If (LEqual (DP37, 0x1)) {
              Notify (\_SB.PC37.RP00, 0xF)
            }
            If (LEqual (DP38, 0x1)) {
              Notify (\_SB.PC38.RP00, 0xF)
            }
            If (LEqual (DP39, 0x1)) {
              Notify (\_SB.PC39.RP00, 0xF)
            }

            Store (0x1, CLR)
          }
        }
      }
    }

    // ---------------------------------------------------------------------
    // HWPM - HW Performance Monitoring
    //---------------------------------------------------------------------
    Device(HWP0)
    {
      Name (_HID, "NVDA2006")
      Name (_UID, 0)
      Name (_CCA, 1)

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
        Memory32Fixed(ReadWrite, 0x13E8B000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E8C000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E8D000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13E8E000, 0x1000)
        Memory32Fixed(ReadWrite, 0x13EF0000, 0x2000)
        Memory32Fixed(ReadWrite, 0x13EF2000, 0x1000)
      })

      // _STA(): Report device status (0xF: Present, 0x0: Absent)
      Method (_STA) {
        Return (0xF)
      }
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

      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package (2) {"nvidia,sidtable", Package {9, 9, 9, 9}}, // to put in PSC_EXT_CFG_SIDTABLE_VM0_0 for SMMU config.
        }
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
    // CLINK Device
    //---------------------------------------------------------------------
    Device (CLNK)
    {
      Name (_HID, "NVDA2004")
      Name (_UID, 0)
    }

    //---------------------------------------------------------------------
    // C2C Device
    //---------------------------------------------------------------------
    Device (C2C)
    {
      Name (_HID, "NVDA2005")
      Name (_UID, 0)
    }

    //---------------------------------------------------------------------
    // QSPI Device
    //---------------------------------------------------------------------
    Device (QSP1)
    {
      Name (_HID, "NVDA1513")
      Name (_UID, 1)
      Name (_CCA, 1) // coherent DMA, explicit setting
      Name (_STA, 0)

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

    Device (FLS1) {
      Name (_HID, "PRP0001")
      Name (_UID, 0)
      Name (_STA, 0)
      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package (2) { "compatible", "jedec,spi-nor" },
        }
      })
      Name (_CRS, ResourceTemplate() {
        SPISerialBus(0, PolarityLow, FourWireMode, 8,
                     ControllerInitiated, 1000000, ClockPolarityLow,
                     ClockPhaseFirst, "\\_SB.QSP1",)
      })
    }

    Device (TPM1) {
      Name (_HID, "PRP0001")
      Name (_UID, 0)
      Name (_STA, 0)
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

    //---------------------------------------------------------------------
    // GPIO Device
    //---------------------------------------------------------------------
    Device (GPI0) {
        Name (_HID, "NVDA0508")
        Name (_UID, 0)
        Name (_STA, 0xF)

        Name (_CRS, ResourceTemplate () {
            Memory32Fixed(ReadWrite, 0x2200000, 0x10000)
            Memory32Fixed(ReadWrite, 0x2210000, 0x10000)
            Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 0xad,
                                                                        0xae,
                                                                        0xaf,
                                                                        0xb0,
                                                                        0xb1,
                                                                        0xb2,
                                                                        0xb3,
                                                                        0xb4,
                                                                        0xb5,
                                                                        0xb6,
                                                                        0xb7,
                                                                        0xb8,
                                                                        0xb9,
                                                                        0xba,
                                                                        0xbb,
                                                                        0xbc }
        })
    }

    //---------------------------------------------------------------------
    // I2C Device
    //---------------------------------------------------------------------
    Device (I2C3) {
      Name (_HID, "NVDA0301")
      Name (_UID, 3)
      Name (_STA, 0)

      Name (_CRS, ResourceTemplate() {
        Memory32Fixed (ReadWrite, 0xc250000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x3e }
        Interrupt (ResourceConsumer, Edge, ActiveLow, Exclusive, 0, "\\_SB.GPI0") { 0xa }
      })

      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package (2) {"interrupt-names", Package () { "default", "smbus_alert" } },
        },
      })

      Method (_RST) { }

      Device (SSIF) {
        Name (_HID, "IPI0001")
        Name (_UID, 0)
        Name (_STA, 0)

        Name (_STR, Unicode("IPMI_SSIF"))

        // Return interface type
        Method (_IFT) {
          Return(0x04)
        }

        // Return the SSIF slave address
        Method (_ADR) {
          Return(0x10)
        }

        // Return interface specification version
        Method (_SRV) {
          Return(0x0200)
        }

        Name (_CRS, ResourceTemplate () {
                 I2cSerialBusV2 (
                   0x10,
                   ControllerInitiated,
                   100000,
                   AddressingMode7Bit,
                   "\\_SB.I2C3",
                   0,
                   ResourceConsumer
                 )
        })
      }
    }

    //---------------------------------------------------------------------
    // SMMU Test Device
    //---------------------------------------------------------------------
    Device (TEST)
    {
      Name (_HID, "NVDA200B")
    }

    //---------------------------------------------------------------------
    // MCF Devices
    //---------------------------------------------------------------------
    //MCF NVLINK Chiplet
    Device (MNV0)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x00)
    }
    //MCF C2C Chiplet
    Device (MC20)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x01)
    }
    //GPU SOC-HUB Chiplet
    Device (GSH0)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x02)
    }
    //PCIe SOC-HUB Chiplet
    Device (PSH0)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x03)
    }

    //---------------------------------------------------------------------
    // Coresight Devices
    //---------------------------------------------------------------------
    // Coresight STM
    Device (STM0)
    {
      Name (_CID, "ARMHC502")

      Name (_CRS, ResourceTemplate () {
        //STM control register
        QWordMemory (
          ResourceProducer, // ResourceUsage
          PosDecode, // Decode
          MinFixed, // Min range is fixed
          MaxFixed, // Max range is fixed
          NonCacheable, // Cacheable
          ReadWrite, // Read And Write
          0x0000000000000000, // Address Granularity - GRA
          0x0000670000070000, // Address Minimum - MIN
          0x0000670000070FFF, // Address Maximum - MAX
          0x0000000000000000, // Address Translation - TRA
          0x0000000000001000, // Range Length - LEN
          , // Resource Source Index
          , // Resource Source
          // Descriptor Name
        )

        //STM stimulus register
        QWordMemory (
          ResourceProducer, // ResourceUsage
          PosDecode, // Decode
          MinFixed, // Min range is fixed
          MaxFixed, // Max range is fixed
          NonCacheable, // Cacheable
          ReadWrite, // Read And Write
          0x0000000000000000, // Address Granularity - GRA
          0x0000670001000000, // Address Minimum - MIN
          0x0000670001FFFFFF, // Address Maximum - MAX
          0x0000000000000000, // Address Translation - TRA
          0x0000000001000000, // Range Length - LEN
          , // Resource Source Index
          , // Resource Source
          // Descriptor Name
        )
      })

      Name (_DSD, Package () {
        ToUUID ("ab02a46b-74c7-45a2-bd68-f7d344ef2153"), Package () {
          0, // Revision
          1, // Number of graphs
          Package () {
            1, // GraphID // CoreSight graphs UUID
            ToUUID ("3ecbc8b6-1d0e-4fb3-8107-e627f805c6cd") ,
            1, // Number of links
            Package () { // output port 0 connected to input port 1 FUN0.
              0, // source port
              1, // destination port
              \_SB.FUN0, // destination device
              1 // flow (source as output port)
            }
          }
        }
      })
    }

    //Coresight Major Funnel
    Device (FUN0)
    {
      Name (_HID , "ARMHC9FF")
      Name (_UID , 0x00)
      Name (_CID , "ARMHC500")

      Name (_CRS , ResourceTemplate () {
        QWordMemory (
          ResourceProducer, // ResourceUsage
          PosDecode, // Decode
          MinFixed, // Min range is fixed
          MaxFixed, // Max range is fixed
          NonCacheable, // Cacheable
          ReadWrite, // Read And Write
          0x0000000000000000, // Address Granularity - GRA
          0x0000670000030000, // Address Minimum - MIN
          0x0000670000030FFF, // Address Maximum - MAX
          0x0000000000000000, // Address Translation - TRA
          0x0000000000001000, // Range Length - LEN
          , // Resource Source Index
          , // Resource Source
          // Descriptor Name
        )
      })

      Name (_DSD , Package () {
        ToUUID ("ab02a46b-74c7-45a2-bd68-f7d344ef2153") ,
        Package () {
          0, // Revision
          1, // Number of graphs
          Package () {
            1, // GraphID // CoreSight graphs UUID
            ToUUID ("3ecbc8b6-1d0e-4fb3-8107-e627f805c6cd") ,
            2, // Number of links
            Package () { // input port 1 to connected to output port 0 STM0.
              1, // source port
              0, // destination port
              \_SB.STM0, // destination device
              0 // flow (source as input port)
            },
            Package () { // output port 0 connected to input port 0 on ETF0.
              0, // source port
              0, // destination port
              \_SB.ETF0, // destination device
              1 // flow (source as output port)
            }
          }
        }
      })
    }

    // Coresight ETF
    Device (ETF0) {
      Name (_HID , "ARMHC97C")
      Name (_UID , 0x00)
      Name (_CID , "ARMHC500")

      Name (_CRS , ResourceTemplate () {
        QWordMemory (
          ResourceProducer, // ResourceUsage
          PosDecode, // Decode
          MinFixed, // Min range is fixed
          MaxFixed, // Max range is fixed
          NonCacheable, // Cacheable
          ReadWrite, // Read And Write
          0x0000000000000000, // Address Granularity - GRA
          0x0000670000040000, // Address Minimum - MIN
          0x0000670000040FFF, // Address Maximum - MAX
          0x0000000000000000, // Address Translation - TRA
          0x0000000000001000, // Range Length - LEN
          , // Resource Source Index
          , // Resource Source
          // Descriptor Name
        )
      })

      Name (_DSD , Package () {
        ToUUID ("ab02a46b-74c7-45a2-bd68-f7d344ef2153") ,
        Package () {
          0, // Revision
          1, // Number of graphs
          Package () {
            1, // GraphID // CoreSight graphs UUID
            ToUUID ("3ecbc8b6-1d0e-4fb3-8107-e627f805c6cd") ,
            1, // Number of links
            Package () { // input port 0 connected to output port 1 FUN0.
              0, // source port
              0, // destination port
              \_SB.FUN0, // destination device
              0 // flow (source as input port)
            },
          }
        }
      })
    }

    //---------------------------------------------------------------------
    // SMMU0_CMDQV @ 0x000011200000
    //---------------------------------------------------------------------
    Device (SQ00) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_SMMU0_BASE_SOCKET_0)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU0_CMDQV_BASE_SOCKET_0,  // Range Minimum
                     TH500_SMMU0_CMDQV_LIMIT_SOCKET_0, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU0_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU0_CMDQV_INTR_SOCKET_0 }
      })
    }

    //---------------------------------------------------------------------
    // SMMU1_CMDQV @ 0x000012200000
    //---------------------------------------------------------------------
    Device (SQ01) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_SMMU1_BASE_SOCKET_0)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU1_CMDQV_BASE_SOCKET_0,  // Range Minimum
                     TH500_SMMU1_CMDQV_LIMIT_SOCKET_0, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU1_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU1_CMDQV_INTR_SOCKET_0 }
      })
    }

    //---------------------------------------------------------------------
    // SMMU2_CMDQV @ 0x000015200000
    //---------------------------------------------------------------------
    Device (SQ02) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_SMMU2_BASE_SOCKET_0)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU2_CMDQV_BASE_SOCKET_0,  // Range Minimum
                     TH500_SMMU2_CMDQV_LIMIT_SOCKET_0, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU2_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU2_CMDQV_INTR_SOCKET_0 }
      })
    }

    //---------------------------------------------------------------------
    // GSMMU0_CMDQV @ 0x000016200000
    //---------------------------------------------------------------------
    Device (GQ00) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_GSMMU0_BASE_SOCKET_0)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                               // Granularity
                     TH500_GSMMU0_CMDQV_BASE_SOCKET_0,  // Range Minimum
                     TH500_GSMMU0_CMDQV_LIMIT_SOCKET_0, // Range Maximum
                     0x0,                               // Translation Offset
                     TH500_GSMMU0_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_GSMMU0_CMDQV_INTR_SOCKET_0 }
      })
    }

    //---------------------------------------------------------------------
    // GSMMU1_CMDQV @ 0x000005200000
    //---------------------------------------------------------------------
    Device (GQ01) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_GSMMU1_BASE_SOCKET_0)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                               // Granularity
                     TH500_GSMMU1_CMDQV_BASE_SOCKET_0,  // Range Minimum
                     TH500_GSMMU1_CMDQV_LIMIT_SOCKET_0, // Range Maximum
                     0x0,                               // Translation Offset
                     TH500_GSMMU1_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_GSMMU1_CMDQV_INTR_SOCKET_0 }
      })
    }
  }
}
