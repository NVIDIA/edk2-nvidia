/** @file
  SSDT Pci Osc (Operating System Capabilities)

  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
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
#include <Guid/NVIDIADsmApi.h>

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
    Local1 = Zero // PCI _OSC Control Field value

    // Create DWord-addressable fields from the Capabilities Buffer
    CreateDWordField (Arg3, 0, CDW1)
    CreateDWordField (Arg3, 8, CDW3)

    // Check for proper UUID
    If (LEqual (Arg0,ToUUID ("33DB4D5B-1FF7-401C-9657-7441C03DD766"))) {

      // Save Capabilities DWord3
      Store (CDW3, Local1)

      /* Do not allow SHPC (No SHPC controller in this system) */
      /* Do not allow Native PME (TODO: Confirm it) */
      /* Do not allow Native AER (RAS-FW handles it) */
      /* Allow Native PCIe capability */
      /* Allow Native LTR control */
      And(Local1,0x31,Local1)

      If (LNotEqual (Arg1, One)) {  // Unknown revision
        Or (CDW1, 0x08, CDW1)
      }

      If (LNotEqual (CDW3, Local1)) {  // Capabilities bits were masked
        Or (CDW1, 0x10, CDW1)
      }

      // Update DWORD3 in the buffer
      Store (Local1,CDW3)
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
        // Check for Revision ID
        If (LEqual (Arg1, 0x5)) {
          Switch(ToInteger(Arg2)) {
            //
            // Function Index:0
            // Standard query - A bitmask of functions supported
            //
            Case (0) {
              Local0 = Buffer(2) {0, 0}
              CreateBitField(Local0, 0, FUN0)
              CreateBitField(Local0, 13, FUND)

              Store(1, FUN0)
              Store(1, FUND)
              Return(Local0)
            }
            //
            // Function Index: D
            // Downstream Port Containment Device Location
            //
            Case(13) {
              Local0 = 0xFFFFFFFF
              Local1 = 0xFFFFFFFF
              Store (DPC0, Local0)
              // Only if an active DPC is going on
              If (LEqual (Local0, 0x1)) {
                Store (ESR0, Local1)
                Return (Local1)
              }
              Return (Local1)
            }
          } // End of switch(Arg2)
        } // end Check for Revision ID
      } // end Check UUID
      //
      // If not one of the UUIDs we recognize, then return a buffer
      // with bit 0 set to 0 indicating no functions supported.
      //
      Return (Buffer () {0})
    } // end _DSM

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
    Name (_ADR, 0x0000)

    // The "_SEG " named object would be added dynamically by UEFI at the
    // time of generating the PCIe node.
    External (_SEG)

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
      C9RS, 1,
      C8RS, 1,
      RSV1, 30,
      C9CO, 2,
      C8CO, 2,
      RSV2, 28
    }

    // The "FSPA" named object is patched by UEFI to provide the correct location
    // this is the address of the FSP Boot partition in VDM space
    Name (FSPA, 0xFFFFFFFFFFFFFFFF)

    OperationRegion (FSPB, SystemMemory, FSPA, 4)
    Field (FSPB, DWordAcc, NoLock, Preserve)
    {
      TI2S, 32, // < Nv_Therm_I2Cs_Scratch
    }

    // The "UVAR" named object is added by UEFI when generating the GPU0
    // node and has the value of the McfSmmuBypassEnable UEFI option
    // (0=disable 1=enable)
    External (UVAR)

    OperationRegion (MS0, SystemMemory, TH500_MCF_SMMU_SOCKET_0, 8)
    Field (MS0, DWordAcc, NoLock, Preserve) {
      Offset (TH500_MCF_SMMU_BYPASS_0_OFFSET),
      MS0B, 32,
    }
    OperationRegion (MS1, SystemMemory, TH500_MCF_SMMU_SOCKET_1, 8)
    Field (MS1, DWordAcc, NoLock, Preserve) {
      Offset (TH500_MCF_SMMU_BYPASS_0_OFFSET),
      MS1B, 32,
    }
    OperationRegion (MS2, SystemMemory, TH500_MCF_SMMU_SOCKET_2, 8)
    Field (MS2, DWordAcc, NoLock, Preserve) {
      Offset (TH500_MCF_SMMU_BYPASS_0_OFFSET),
      MS2B, 32,
    }
    OperationRegion (MS3, SystemMemory, TH500_MCF_SMMU_SOCKET_3, 8)
    Field (MS3, DWordAcc, NoLock, Preserve) {
      Offset (TH500_MCF_SMMU_BYPASS_0_OFFSET),
      MS3B, 32,
    }

    Method(_RST, 0) {
      /* Issue GPU reset request via LIC IO1 interrupt */
      If ((_SEG & 0xF) == 8) {
        C8RS = 1
      } ElseIf ((_SEG & 0xF) == 9) {
        C9RS = 1
      } Else {
        Return
      }

      SET = 1

      /* Wait for reset to complete, poll for 6sec (as per from Linux) */
      For (Local0 = 0, Local0 < 60000, Local0 +=2) {
        If (((_SEG & 0xF) == 8) && (C8RS == 0)) {
          Break
        } ElseIf (((_SEG & 0xF) == 9) && (C9RS == 0)){
          Break
        }
        Sleep(2)
      }

      /* Wait for reset to complete, poll for 6sec (as per from Linux) */
      For (Local0 = 0, Local0 < 60000, Local0 +=2) {
        If (TI2S == 0xFF) {
          Break
        }
        Sleep(2)
      }
    }

    Method (_DSM, 4, Serialized) {
      If (LEqual (Arg0, ToUUID (NVIDIA_GPU_STATUS_DSM_GUID_STR))) {
        // Check for Revision ID
        If (Arg1 >= NVIDIA_GPU_STATUS_DSM_REV) {
          Switch(ToInteger(Arg2)) {
        //
        // Function Index:0
        // Standard query - A bitmask of functions supported
        //
        Case (0) {
          Local0 = Buffer(2) {0, 0}
          CreateBitField(Local0, 0, FUN0)
          CreateBitField(Local0, 1, FUN1)

          Store(1, FUN0)
          Store(1, FUN1)
          Return(Local0)
        }
        //
        // Function Index: 1
        // Get GPU Containment status
        //
        Case(1) {
          If ((_SEG & 0xF) == 8) {
            Return (C8CO)
          } ElseIf ((_SEG & 0xF) == 9) {
            Return (C9CO)
          } Else {
            Return (0)
          }
        }
          } // End of switch(Arg2)
        } // end Check for Revision ID
      } // end Check UUID

      // GPU SMMU Bypass
      If (LEqual (Arg0, ToUUID (NVIDIA_GPU_SMMU_BYPASS_DSM_GUID_STR))) {
        // Check for Revision ID
        If (Arg1 >= NVIDIA_GPU_SMMU_BYPASS_DSM_REV) {
          Switch(ToInteger(Arg2)) {
        //
        // Function Index:0
        // Standard query - A bitmask of functions supported
        //
        Case (0) {
          Return (Buffer () {0x3})
        }
        //
        // Function Index: 1
        // Enable/Disable GPU SMMU Bypass
        //
        Case(1) {
          // if UEFI variable disabled SMMU bypass, return error
          If (LEqual (UVAR, 0x0)) {
            Return (1)
          }

          Local0 = (_SEG & 0xF0) >> 4
          Local1 = Arg3 & 0x1

          If (LEqual (Local0, 0)) {
            Store(Local1, MS0B)
          } ElseIf (LEqual (Local0, 1)) {
            Store(Local1, MS1B)
          } ElseIf (LEqual (Local0, 2)) {
            Store(Local1, MS2B)
          } ElseIf (LEqual (Local0, 3)) {
            Store(Local1, MS3B)
          } Else {
            Return (2)
          }

          Return (0)
        }
          } // End of switch(Arg2)
        } // end Check for Revision ID
      } // end GPU SMMU Bypass UUID

      //
      // If not one of the UUIDs we recognize, then return a buffer
      // with bit 0 set to 0 indicating no functions supported.
      //
      Return (Buffer () {0})
    } // end _DSM
  }
}
