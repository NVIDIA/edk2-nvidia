#!/usr/bin/python3

# Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

# Uefi Sub-directories to search for included JSON files in
UEFI_SUBDIRS = [
    "edk2",
    "edk2-nvidia", "edk2-nvidia-private", "edk2-non-osi",
    # TODO: Remove the following line after we've transitioned our directory structure.
    "edk2-platforms", "edk2-platforms-private"
]

# List of supported integer types
INT_TYPES = ["UINT8", "UINT16", "UINT32", "UINT64"]

# List of the starts of supported types. This is used with
# parse_json_pairs to make sure to make type strings unique in JSON objects.
SUPPORTED_TYPE_STARTS = INT_TYPES + ["ARRAY:", "STRUCT", "FILE"]

# Max values for the supported integer types
MAX_UINT8 = 255
MAX_UINT16 = 65535
MAX_UINT32 = 4294967295
MAX_UINT64 = 18446744073709551615

# Vendor GUID names and values
GUIDS = {
    "gDtPlatformFormSetGuid":[
        0x2b7a240d,
        0xd5ad,
        0x4fd6,
        [0xbe, 0x1c, 0xdf, 0xa4, 0x41, 0x5f, 0x55, 0x26]
    ],
    "gEfiAuthenticatedVariableGuid":[
        0xAAF32C78,
        0x947B,
        0x439A,
        [0xA1, 0x80, 0x2E, 0x14, 0x4E, 0xC3, 0x77, 0x92]
    ],
    "gEfiSystemNvDataFvGuid":[
        0xFFF12B8D,
        0x7696,
        0x4C8B,
        [0xA9, 0x85, 0x27, 0x47, 0x07, 0x5B, 0x4F, 0x50]
    ],
    "gNVIDIATokenSpaceGuid":[
        0xed3374ef,
        0x767b,
        0x42fa,
        [0xaf, 0x70, 0xdb, 0x52, 0x0a, 0x39, 0x28, 0x22]
    ],
    "gEfiGlobalVariableGuid":[
        0x8BE4DF61,
        0x93CA,
        0x11D2,
        [0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C]
    ]
}

# Attribute option names and their corresponding values
ATTRIBUTES = {
    "EFI_VARIABLE_NON_VOLATILE":0x1,
    "EFI_VARIABLE_BOOTSERVICE_ACCESS":0x2,
    "EFI_VARIABLE_RUNTIME_ACCESS":0x4,
    "EFI_VARIABLE_HARDWARE_ERROR_RECORD":0x8,
    "EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS":0x20,
    "EFI_VARIABLE_APPEND_WRITE":0x40,
    "EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS":0x10
}

# Default attributes that are given to every variable
DEFAULT_ATTRIBUTES = [
    "EFI_VARIABLE_NON_VOLATILE",
    "EFI_VARIABLE_BOOTSERVICE_ACCESS"
]

# Firmware Volume Header
FVH_FV_LENGTH = 0x20000
FVH_SIGNATURE = "_FVH"
FVH_ATTRIBUTES = 0x0E36
FVH_HEADER_LENGTH = 0x48
FVH_HEADER_OFFSET = 0x0
FVH_RESERVED = 0x0
FVH_REVISION = 0x2
FVH_BLOCKMAP0_BLOCK_SIZE = 0x10000
FVH_BLOCKMAP1_NUM_BLOCKS = 0x0
FVH_BLOCKMAP1_BLOCK_SIZE = 0x0

# Variable Store Header
VSH_FORMATTED = 0x5A
VSH_HEALTHY = 0xFE
VSH_RESERVED = 0x0
VSH_RESERVED1 = 0x0

# Variable Header
VH_START_ID = 0x55AA
VH_STATE = 0x3F
VH_RESERVED = 0x0
VH_MONOTONIC_COUNT = 0x0
VH_PUBLIC_KEY_INDEX = 0x0
