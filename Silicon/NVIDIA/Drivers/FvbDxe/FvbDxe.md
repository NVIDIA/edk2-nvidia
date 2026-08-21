# FvbDxe Driver

## Overview

`FvbDxe` exposes the UEFI variable partition on block devices as
`EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL` and provides the
`EFI_FAULT_TOLERANT_WRITE_PROTOCOL` shim used by variable services. The driver
discovers the GPT partition named by `PcdUEFIVariablesPartitionName`, validates
that the partition is backed by eMMC or UFS media, caches the full partition in
runtime memory, and keeps the backing block device synchronized when firmware
volume blocks are written or erased.

## Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | The driver shall skip initialization when emulated nonvolatile variable mode is enabled. |
| FR-02 | The driver shall locate a GPT variable partition on eMMC or UFS-backed media. |
| FR-03 | The driver shall read the variable partition into runtime memory before exposing FVB services. |
| FR-04 | The driver shall validate the firmware volume and variable-store headers and initialize them when they are missing or invalid. |
| FR-05 | The driver shall expose firmware volume block read, write, erase, geometry, and physical-address services. |
| FR-06 | The driver shall expose a fault-tolerant write protocol that splits writes on block boundaries. |
| FR-07 | The driver shall update runtime pointers on virtual address changes. |
| FR-08 | The driver shall publish Component Name, Component Name 2, and Driver Health after FVB and FTW protocols are installed. |

## Initialization Flow

```
FVBInitialize()
    |
    +-- Skip if PcdEmuVariableNvModeEnable is TRUE
    |
    +-- Locate handles with EFI_PARTITION_INFO_PROTOCOL
    |
    +-- Select the configured UEFI variable partition on eMMC or UFS
    |
    +-- Locate parent EFI_BLOCK_IO_PROTOCOL
    |
    +-- Allocate runtime cache for the full partition
    |
    +-- Read the partition into memory
    |
    +-- Validate or create FV and variable-store headers
    |
    +-- Install gEdkiiNvVarStoreFormattedGuid marker
    |
    +-- Register virtual address change callback
    |
    +-- Install EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL and FTW protocol
    |
    +-- Install Component Name and Driver Health protocols
```

If initialization fails after allocations or event creation, the driver frees
the runtime cache, closes the virtual address event, and frees private data
before returning the error.

## Firmware Volume Operations

Reads are served from the runtime partition cache. Writes update the cache first
and then write the containing block back through `EFI_BLOCK_IO_PROTOCOL`.
Erase requests prevalidate all LBA ranges, set the matching cache bytes to
`0xFF`, and flush each erased block back to the block device. If a write or
erase flush fails, the driver reads the affected block back from media so the
cache follows the device state as closely as possible.

The FTW write service repeatedly calls the FVB write service, clipping each
operation at a block boundary. A partial write or short write is reported as
`EFI_BAD_BUFFER_SIZE`.

## Protocols

| Protocol | Direction | Purpose |
|----------|-----------|---------|
| `gEfiPartitionInfoProtocolGuid` | Consumes | Finds the configured GPT variable partition. |
| `gEfiBlockIoProtocolGuid` | Consumes | Reads and writes the backing eMMC or UFS media. |
| `gEfiDevicePathProtocolGuid` | Consumes | Validates media type and locates the parent block device. |
| `gEfiFirmwareVolumeBlockProtocolGuid` | Produces | Exposes firmware volume block services. |
| `gEfiFaultTolerantWriteProtocolGuid` | Produces | Provides variable-service FTW write entry points. |
| `gEfiComponentNameProtocolGuid` | Produces | User-readable driver name. |
| `gEfiComponentName2ProtocolGuid` | Produces | ISO 639-2 successor component name protocol. |
| `gEfiDriverHealthProtocolGuid` | Produces | Reports block-backed FVB health. |

## Driver Health

Driver Health is published on the driver image handle after the FVB and FTW
protocols are installed. The health state reflects the most recent backing
media operation observed by the driver.

| Path | Health Result |
|------|---------------|
| Partition read during initialization succeeds | Healthy |
| Partition read during initialization fails | Failed, but no health protocol is published because initialization aborts |
| Header creation or resize write succeeds | Healthy |
| Header creation or resize write fails | Failed |
| FVB write flush succeeds | Healthy |
| FVB write flush fails | Failed |
| FVB erase block flush succeeds | Healthy |
| FVB erase block flush fails | Failed |
| FVB/FTW protocol installation and Driver Health publication succeed | Healthy |

Parameter validation failures such as NULL pointers, zero-byte requests,
out-of-range LBAs, and block-boundary clipping are reported through the FVB
return status and do not by themselves represent media health failures.

## Testing

`FvbDxeUnitTestsHost` exercises FVB geometry, read, write, erase, FV header,
faulty flash, and FTW write behavior using host block-device stubs. The tests
cover Driver Health updates for successful header writes and failed write,
erase, and header initialization media paths.

## Related Files

- `Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.c`
- `Silicon/NVIDIA/Drivers/FvbDxe/FvbPrivate.h`
- `Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf`
- `Silicon/NVIDIA/Drivers/FvbDxe/UnitTest/FvbDxeUnitTests.c`

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
