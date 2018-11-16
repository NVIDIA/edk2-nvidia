/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2000 - 2018 Intel Corporation
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Template for [GTDT] ACPI Table (static data table)
 * Format: [ByteLength]  FieldName : HexFieldValue
 */
[0004]                          Signature : "GTDT"    [Generic Timer Description Table]
[0004]                       Table Length : 00000000
[0001]                           Revision : 02
[0001]                           Checksum : 00
[0006]                             Oem ID : "NVIDIA"
[0008]                       Oem Table ID : "TEGRA186"
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 00000000

[0008]              Counter Block Address : 0000000000000000
[0004]                           Reserved : 00000000

[0004]               Secure EL1 Interrupt : 0000001D
[0004]          EL1 Flags (decoded below) : 00000000
                             Trigger Mode : 0
                                 Polarity : 1
                                Always On : 0

[0004]           Non-Secure EL1 Interrupt : 0000001E
[0004]         NEL1 Flags (decoded below) : 00000000
                             Trigger Mode : 0
                                 Polarity : 1
                                Always On : 0

[0004]            Virtual Timer Interrupt : 0000001B
[0004]           VT Flags (decoded below) : 00000000
                             Trigger Mode : 0
                                 Polarity : 1
                                Always On : 0

[0004]           Non-Secure EL2 Interrupt : 0000001A
[0004]         NEL2 Flags (decoded below) : 00000000
                             Trigger Mode : 0
                                 Polarity : 1
                                Always On : 0
[0008]         Counter Read Block Address : 0000000000000000

[0004]               Platform Timer Count : 00000000
[0004]              Platform Timer Offset : 00000000
