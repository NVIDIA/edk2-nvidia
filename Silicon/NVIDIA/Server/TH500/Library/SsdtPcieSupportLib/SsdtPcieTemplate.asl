/** @file
  SSDT Pci Osc (Operating System Capabilities)

  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2021, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Reference(s):
  - PCI Firmware Specification - Revision 3.3
  - ACPI 6.4 specification:
   - s6.2.13 "_PRT (PCI Routing Table)"
   - s6.1.1 "_ADR (Address)"
  - linux kernel code
**/

#include <TH500/TH500Definitions.h>

DefinitionBlock ("SsdtPciOsc.aml", "SSDT", 2, "NVIDIA", "PCI-OSC", 1) {

  // This table is just a template and is never installed as a table.
  // Pci devices are dynamically created at runtime as:
  // ASL:
  // Device (PCIx) {
  //   ...
  // }
  // and the _OSC method available below is appended to the PCIx device as:
  // ASL:
  // Device (PCIx) {
  //   ...
  //   Method (_OSC, 4 {
  //    ...
  //   })
  // }
  Method (_OSC, 4) {
    //
    // OS Control Handoff
    //
    Name (SUPP, Zero) // PCI _OSC Support Field value
    Name (CTRL, Zero) // PCI _OSC Control Field value

    // Create DWord-addressable fields from the Capabilities Buffer
    CreateDWordField (Arg3, 0, CDW1)
    CreateDWordField (Arg3, 4, CDW2)
    CreateDWordField (Arg3, 8, CDW3)

    // Check for proper UUID
    If (LEqual (Arg0,ToUUID ("33DB4D5B-1FF7-401C-9657-7441C03DD766"))) {

      // Save Capabilities DWord2 & 3
      Store (CDW2, SUPP)
      Store (CDW3, CTRL)

      /* Do not allow Native Hotplug */
      /* Do not allow SHPC (No SHPC controller in this system) */
      /* Do not allow Native PME (TODO: Confirm it) */
      /* Do not allow Native AER (RAS-FW handles it) */
      /* Allow Native PCIe capability */
      /* Allow Native LTR control */
      And(CTRL,0x30,CTRL)

      If (LNotEqual (Arg1, One)) {  // Unknown revision
        Or (CDW1, 0x08, CDW1)
      }

      If (LNotEqual (CDW3, CTRL)) {  // Capabilities bits were masked
        Or (CDW1, 0x10, CDW1)
      }

      // Update DWORD3 in the buffer
      Store (CTRL,CDW3)
      Return (Arg3)
    } Else {
      Or (CDW1, 4, CDW1) // Unrecognized UUID
      Return (Arg3)
    } // If
  } // _OSC

  Device (RP00)
  {
      Name (_ADR, 0x0000)  // _ADR: Address

    // The "ADDR" named object would be patched by UEFI to have the correct
    // address of the NS shared memory region of this particular instance.
    Name (ADDR, 0xFFFFFFFFFFFFFFFF)
    // The "LICA" named object would be patched by UEFI to have the correct
    // address of the LIC region of this particular instance.
    Name (LICA, 0xFFFFFFFFFFFFFFFF)

    // The "_SEG " named object would be added dynamically by UEFI at the
    // time of generating the PCIe node.
    External (_SEG)

    OperationRegion (SMEM, SystemMemory, ADDR, 0x10)
    Field (SMEM, DWordAcc, NoLock, Preserve) {
      DPC0, 32, SOC0, 32, SEG0, 32, ESR0, 32
    }

    Method (_DSM, 4, Serialized) {
      If (LEqual (Arg0, ToUUID ("E5C937D0-3553-4D7A-9117-EA4D19C3434D"))) {
        If (LEqual (Arg1, 0x5)) {
          // Check for Revision ID
          If (LEqual (Arg2, 0xD)) {
            // Check for Function Index
            // return BDF of port that experienced containment
            Local0                   = 0xFFFFFFFF
                              Local1 = 0xFFFFFFFF
                                       Store (DPC0, Local0)
                                       If (LEqual (Local0, 0x1)) {
              // Only if an active DPC is going on
              Store (ESR0, Local1)
              Return (Local1)
            }
            Return (Local1)
          }   // end Check for Function Index
        }   // end Check for Revision ID
      }   // end Check UUID
    }   // end _DSM

    OperationRegion (LIC4, SystemMemory, LICA, TH500_SW_IO4_SIZE)
    Field (LIC4, DWordAcc, NoLock, Preserve) {
      STS4, 32,
      SET4, 32,
      CLR4, 32,
      RVD4, 32,
      DAL4, 32,
      DAH4, 32,
    }

    Method (_OST, 3, Serialized) {
      // OSPM calls this method after processing ErrorDisconnectRecover
      // notification from firmware
      If (LEqual (And (Arg0, 0xFF), 0xF)) {
        // Mask to retain low byte

        // Invoke RAS-FW for both cases i.e
        // OST_SUCCESS (Arg1=0x80) and
        // OST_FAILURE (Arg1=0x81)

        /* Save BDF to DAH4 */
        Local0 = (Arg1 >> 16)

        /* Embed Segment number also in DAH4 */
        Store (_SEG, Local1)
        Local0 |= (Local1 << 16)

        /* Make sure DAH4 is zero to avoid overwriting the value of
           an ongoing RAS-FW handling of _OST() call */
        Local1 = Zero
        While ((Local1 < 60000) && (LNotEqual (DAH4, 0))) {
          Local1 += 2;
          Sleep (2)
        }

        Store (Local0, DAH4)
        Store (0x1, SET4)
      }
    }   // End _OST
  }

  Device(GPU0) {
    Name(_ADR, 0x0000)
    Name(_DSD, Package() {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package(2) { "gpu_mem_base_pa", 0x400000000000 },
        Package(2) { "gpu_mem_pxm_start", TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID(0) },
        Package(2) { "gpu_mem_pxm_count", TH500_GPU_MAX_NR_MEM_PARTITIONS },
      }
    })

    // The "LICA" named object would be patched by UEFI to have the correct
    // address of the LIC region of this particular instance.
    Name (LICA, 0xFFFFFFFFFFFFFFFF)

    OperationRegion (LIC1, SystemMemory, LICA, TH500_SW_IO1_SIZE)
    Field (LIC1, DWordAcc, NoLock, Preserve)
    {
      STAT, 32,
      SET, 32,
      CLR, 32,
      RSV0, 32,
      DALO, 32,
      DAHI, 32,
    }
    Method(_RST, 0) {
      /* Issue GPU reset request via LIC IO1 interrupt */
      Store (0x1, DALO)
      Store (0x1, SET)

      /* Wait for reset to complete, poll for 6sec (as per from Linux) */
      Local0 = Zero
      While ((Local0 < 60000) && (LNotEqual (DALO, 0))) {
        Local0 += 2;
        Sleep(2)
      }
    }
  }
}
