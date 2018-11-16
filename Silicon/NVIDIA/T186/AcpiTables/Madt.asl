/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2000 - 2018 Intel Corporation
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Template for [APIC] ACPI Table (static data table)
 * Format: [ByteLength]  FieldName : HexFieldValue
 */
[0004]                          Signature : "APIC"    [Multiple APIC Description Table (MADT)]
[0004]                       Table Length : 00000000
[0001]                           Revision : 03
[0001]                           Checksum : 00
[0006]                             Oem ID : "NVIDIA"
[0008]                       Oem Table ID : "TEGRA186"
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 00000000

[0004]                 Local Apic Address : 03882000
[0004]              Flags (decoded below) : 00000000
                      PC-AT Compatibility : 0

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 50
[0002]                           Reserved : 0000
[0004]               CPU Interface Number : 00000000
[0004]                      Processor UID : 00000000
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
       Performance Interrupt Trigger Mode : 0
       Virtual GIC Interrupt Trigger Mode : 0
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 00000140
[0008]                     Parked Address : 0000000000000000
[0008]                       Base Address : 0000000003882000
[0008]           Virtual GIC Base Address : 0000000000000000
[0008]        Hypervisor GIC Base Address : 0000000000000000
[0004]              Virtual GIC Interrupt : 00000000
[0008]         Redistributor Base Address : 0000000000000000
[0008]                          ARM MPIDR : 0000000000000000
[0001]                   Efficiency Class : 00
[0003]                           Reserved : 000000

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 50
[0002]                           Reserved : 0000
[0004]               CPU Interface Number : 00000001
[0004]                      Processor UID : 00000001
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
       Performance Interrupt Trigger Mode : 0
       Virtual GIC Interrupt Trigger Mode : 0
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 00000141
[0008]                     Parked Address : 0000000000000000
[0008]                       Base Address : 0000000003882000
[0008]           Virtual GIC Base Address : 0000000000000000
[0008]        Hypervisor GIC Base Address : 0000000000000000
[0004]              Virtual GIC Interrupt : 00000000
[0008]         Redistributor Base Address : 0000000000000000
[0008]                          ARM MPIDR : 0000000000000001
[0001]                   Efficiency Class : 00
[0003]                           Reserved : 000000

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 50
[0002]                           Reserved : 0000
[0004]               CPU Interface Number : 00000002
[0004]                      Processor UID : 00000100
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
       Performance Interrupt Trigger Mode : 0
       Virtual GIC Interrupt Trigger Mode : 0
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 00000000
[0008]                     Parked Address : 0000000000000000
[0008]                       Base Address : 0000000003882000
[0008]           Virtual GIC Base Address : 0000000000000000
[0008]        Hypervisor GIC Base Address : 0000000000000000
[0004]              Virtual GIC Interrupt : 00000128
[0008]         Redistributor Base Address : 0000000000000000
[0008]                          ARM MPIDR : 0000000000000100
[0001]                   Efficiency Class : 00
[0003]                           Reserved : 000000

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 50
[0002]                           Reserved : 0000
[0004]               CPU Interface Number : 00000003
[0004]                      Processor UID : 00000101
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
       Performance Interrupt Trigger Mode : 0
       Virtual GIC Interrupt Trigger Mode : 0
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 00000129
[0008]                     Parked Address : 0000000000000000
[0008]                       Base Address : 0000000003882000
[0008]           Virtual GIC Base Address : 0000000000000000
[0008]        Hypervisor GIC Base Address : 0000000000000000
[0004]              Virtual GIC Interrupt : 00000000
[0008]         Redistributor Base Address : 0000000000000000
[0008]                          ARM MPIDR : 0000000000000101
[0001]                   Efficiency Class : 00
[0003]                           Reserved : 000000

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 50
[0002]                           Reserved : 0000
[0004]               CPU Interface Number : 00000004
[0004]                      Processor UID : 00000102
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
       Performance Interrupt Trigger Mode : 0
       Virtual GIC Interrupt Trigger Mode : 0
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 0000012a
[0008]                     Parked Address : 0000000000000000
[0008]                       Base Address : 0000000003882000
[0008]           Virtual GIC Base Address : 0000000000000000
[0008]        Hypervisor GIC Base Address : 0000000000000000
[0004]              Virtual GIC Interrupt : 00000000
[0008]         Redistributor Base Address : 0000000000000000
[0008]                          ARM MPIDR : 0000000000000102
[0001]                   Efficiency Class : 00
[0003]                           Reserved : 000000

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 50
[0002]                           Reserved : 0000
[0004]               CPU Interface Number : 00000005
[0004]                      Processor UID : 00000103
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
       Performance Interrupt Trigger Mode : 0
       Virtual GIC Interrupt Trigger Mode : 0
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 0000012b
[0008]                     Parked Address : 0000000000000000
[0008]                       Base Address : 0000000003882000
[0008]           Virtual GIC Base Address : 0000000000000000
[0008]        Hypervisor GIC Base Address : 0000000000000000
[0004]              Virtual GIC Interrupt : 00000000
[0008]         Redistributor Base Address : 0000000000000000
[0008]                          ARM MPIDR : 0000000000000103
[0001]                   Efficiency Class : 00
[0003]                           Reserved : 000000

[0001]                      Subtable Type : 0C [Generic Interrupt Distributor]
[0001]                             Length : 18
[0002]                           Reserved : 0000
[0004]              Local GIC Hardware ID : 00000000
[0008]                       Base Address : 0000000003881000
[0004]                     Interrupt Base : 00000000
[0001]                            Version : 02
[0003]                           Reserved : 000000
