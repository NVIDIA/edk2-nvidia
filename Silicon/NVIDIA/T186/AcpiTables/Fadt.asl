/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2000 - 2018 Intel Corporation
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Template for [FACP] ACPI Table (static data table)
 * Format: [ByteLength]  FieldName : HexFieldValue
 */
[0004]                          Signature : "FACP"    [Fixed ACPI Description Table (FADT)]
[0004]                       Table Length : 00000000
[0001]                           Revision : 06
[0001]                           Checksum : 00
[0006]                             Oem ID : "NVIDIA"
[0008]                       Oem Table ID : "TEGRA186"
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 00000000

[0004]                       FACS Address : 00000001
[0004]                       DSDT Address : 00000001
[0001]                              Model : 00
[0001]                         PM Profile : 00 [Unspecified]
[0002]                      SCI Interrupt : 0000
[0004]                   SMI Command Port : 00000000
[0001]                  ACPI Enable Value : 00
[0001]                 ACPI Disable Value : 00
[0001]                     S4BIOS Command : 00
[0001]                    P-State Control : 00
[0004]           PM1A Event Block Address : 00000001
[0004]           PM1B Event Block Address : 00000000
[0004]         PM1A Control Block Address : 00000001
[0004]         PM1B Control Block Address : 00000000
[0004]          PM2 Control Block Address : 00000001
[0004]             PM Timer Block Address : 00000001
[0004]                 GPE0 Block Address : 00000001
[0004]                 GPE1 Block Address : 00000000
[0001]             PM1 Event Block Length : 04
[0001]           PM1 Control Block Length : 02
[0001]           PM2 Control Block Length : 01
[0001]              PM Timer Block Length : 04
[0001]                  GPE0 Block Length : 08
[0001]                  GPE1 Block Length : 00
[0001]                   GPE1 Base Offset : 00
[0001]                       _CST Support : 00
[0002]                         C2 Latency : 0000
[0002]                         C3 Latency : 0000
[0002]                     CPU Cache Size : 0000
[0002]                 Cache Flush Stride : 0000
[0001]                  Duty Cycle Offset : 00
[0001]                   Duty Cycle Width : 00
[0001]                RTC Day Alarm Index : 00
[0001]              RTC Month Alarm Index : 00
[0001]                  RTC Century Index : 00
[0002]         Boot Flags (decoded below) : 0000
            Legacy Devices Supported (V2) : 0
         8042 Present on ports 60/64 (V2) : 0
                     VGA Not Present (V4) : 0
                   MSI Not Supported (V4) : 0
             PCIe ASPM Not Supported (V4) : 0
                CMOS RTC Not Present (V5) : 0
[0001]                           Reserved : 00
[0004]              Flags (decoded below) : 00000000
   WBINVD instruction is operational (V1) : 0
           WBINVD flushes all caches (V1) : 0
                 All CPUs support C1 (V1) : 0
               C2 works on MP system (V1) : 0
         Control Method Power Button (V1) : 0
         Control Method Sleep Button (V1) : 0
     RTC wake not in fixed reg space (V1) : 0
         RTC can wake system from S4 (V1) : 0
                     32-bit PM Timer (V1) : 0
                   Docking Supported (V1) : 0
            Reset Register Supported (V2) : 0
                         Sealed Case (V3) : 0
                 Headless - No Video (V3) : 0
     Use native instr after SLP_TYPx (V3) : 0
           PCIEXP_WAK Bits Supported (V4) : 0
                  Use Platform Timer (V4) : 0
            RTC_STS valid on S4 wake (V4) : 0
             Remote Power-on capable (V4) : 0
              Use APIC Cluster Model (V4) : 0
  Use APIC Physical Destination Mode (V4) : 0
                    Hardware Reduced (V5) : 1
                   Low Power S0 Idle (V5) : 1

[0012]                     Reset Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 01 [Byte Access:8]
[0008]                            Address : 0000000000000001

[0001]               Value to cause reset : 00
[0002]          ARM Flags (decoded below) : 0000
                           PSCI Compliant : 1
                    Must use HVC for PSCI : 0

[0001]                FADT Minor Revision : 01
[0008]                       FACS Address : 0000000000000001
[0008]                       DSDT Address : 0000000000000001
[0012]                   PM1A Event Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 20
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 02 [Word Access:16]
[0008]                            Address : 0000000000000001

[0012]                   PM1B Event Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                 PM1A Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 10
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 02 [Word Access:16]
[0008]                            Address : 0000000000000001

[0012]                 PM1B Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                  PM2 Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000001

[0012]                     PM Timer Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 20
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 03 [DWord Access:32]
[0008]                            Address : 0000000000000001

[0012]                         GPE0 Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 40
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 01 [Byte Access:8]
[0008]                            Address : 0000000000000001

[0012]                         GPE1 Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000


[0012]             Sleep Control Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 01 [Byte Access:8]
[0008]                            Address : 0000000000000000

[0012]              Sleep Status Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 01 [Byte Access:8]
[0008]                            Address : 0000000000000000

[0008]                      Hypervisor ID : 0000000000000000
