/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Template for [SSDT] HDA ACPI Table (AML byte code table)
 */

#define HDA_CONFIGURATION_BAR0_INIT_PROGRAM      MAX_UINT32
#define HDA_CONFIGURATION_BAR0_FINAL_PROGRAM     BIT14
#define HDA_FPCI_BAR0_START                      0x40
/**
  High Definition Audio registers Cf Intel High Defination Audio specification:
   - s3.3.7 "GCTl - Global Control".
**/
// Accept Unsolicited Response Enable (UNSOL)
#define HDA_GLOBAL_CONTROL_UNSOL  BIT8
// Controller Reset (CRST)
#define HDA_GLOBAL_CONTROL_CRST  BIT0

DefinitionBlock ("SsdtHda.aml", "SSDT", 2, "NVIDIA", "TEGRAHDA", 0x00000001)
{
  Scope(_SB) {
    Device(HDA0) {
      Name (_HID, "NVDA010F")
      Name (_UID, 0)
      Name (_CCA, ZERO)
      Name(_CLS, Package (3)
      {
        0x04, // Base Class (04h == Multimedia Controller)
        0x03, // Sub-Class (03h == Multimedia Device)
        0x00, // Programming Interface
      })

      Name (BASE, 0xFFFFFFFF)

      OperationRegion (HDAC, SystemMemory, BASE, 0x8000)
      Field (HDAC, AnyAcc, NoLock, Preserve) {
        Offset (0x80),
        FPCI, 32,
        Offset (0x180),
        ENFP, 32,
        Offset (0x1004),
        ENIO, 1,
        ENME, 1,
        ENBM, 1,
        RES0, 5,
        ENSR, 1,
        RES1, 1,
        DINT, 1,
        Offset (0x1010),
        CFB0, 32
      }

      OperationRegion (HDAG, SystemMemory, BASE+0x8080, 4)
      Field (HDAG, AnyAcc, NoLock, Preserve) {
        GCTL, 32
      }

      Method(_INI, 0) {
        ENFP = One
        DINT = Zero
        ENIO = One
        ENME = One
        ENBM = One
        ENSR = One
        CFB0 = Ones
        CFB0 = HDA_CONFIGURATION_BAR0_FINAL_PROGRAM
        FPCI = HDA_FPCI_BAR0_START
        GCTL = HDA_GLOBAL_CONTROL_UNSOL | HDA_GLOBAL_CONTROL_CRST
      }
    }
  }
}
