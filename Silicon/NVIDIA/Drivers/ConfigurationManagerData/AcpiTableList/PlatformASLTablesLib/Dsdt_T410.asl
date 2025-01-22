/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026, NVIDIA Corporation. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <T410/T410Definitions.h>
#include <DsdtDefinitions.h>

DefinitionBlock ("dsdt_t410.aml", "DSDT", 2, "NVIDIA", "T410", 0x00000001)
{
  Scope(_SB)
  {
    //-------------------------------------------------------------
    // _OSC(): Operating System Capabilities
    //   Arg0 - UUID (Buffer)
    //   Arg1 - Revision ID (Integer): 1
    //   Arg2 - Count of Entries in Arg3 (Integer): 2
    //   Arg3 - DWORD capabilities (Buffer):
    //     First DWORD  (DWD1): status
    //     Second DWORD (DWD2): capabilities bitmap
    //-------------------------------------------------------------
    /* _OSC: Operating System Capabilities */
    Method (_OSC, 4, Serialized) {
      CreateDWordField (Arg3, 0x00, STS0)
      CreateDWordField (Arg3, 0x04, CAP0)

      /* Platform-wide Capabilities */
      If (LEqual (Arg0, ToUUID("0811b06e-4a27-44f9-8d60-3cbbc22e7b48"))) {
        /* OSC rev 1 supported, for other version, return failure */
        If (LEqual (Arg1, One)) {
          And (STS0, Not (OSC_STS_MASK), STS0)

          If (And (CAP0, OSC_CAP_OS_INITIATED_LPI)) {
            /* OS initiated LPI not supported */
            And (CAP0, Not (OSC_CAP_OS_INITIATED_LPI), CAP0)
            Or (STS0, OSC_STS_CAPABILITY_MASKED, STS0)
          }
        } Else {
          And (STS0, Not (OSC_STS_MASK), STS0)
          Or (STS0, Or (OSC_STS_FAILURE, OSC_STS_UNRECOGNIZED_REV), STS0)
        }
      } Else {
        And (STS0, Not (OSC_STS_MASK), STS0)
        Or (STS0, Or (OSC_STS_FAILURE, OSC_STS_UNRECOGNIZED_UUID), STS0)
      }

      Return (Arg3)
    }

  }
}
