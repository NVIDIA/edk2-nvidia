# BpmpI2cDxe Design Document

## Overview

`BpmpI2cDxe` exposes BPMP-managed I2C buses through standard EDK II I2C
protocols. The driver binds to a controller that provides
`NVIDIA_BPMP_IPC_PROTOCOL`, finds the BPMP I2C node in the platform DTB, builds
an I2C device list from child nodes, and translates I2C requests into
`MRQ_I2C` messages sent through BPMP IPC.

## Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | The driver shall bind to BPMP IPC controller handles. |
| FR-02 | The driver shall discover BPMP I2C bus ID and child devices from the device tree. |
| FR-03 | The driver shall publish `EFI_I2C_MASTER_PROTOCOL`, `EFI_I2C_ENUMERATE_PROTOCOL`, and `EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL` on a child handle. |
| FR-04 | The driver shall translate write, read, and SMBus PEC write operations into BPMP I2C request payloads. |
| FR-05 | The driver shall support synchronous and asynchronous I2C request completion. |
| FR-06 | The driver shall enumerate known NVIDIA I2C device GUIDs from device tree compatibility strings. |
| FR-07 | The driver shall initialize supported VRS PSEQ devices using device tree register programming data. |
| FR-08 | The driver shall install Driver Binding, Component Name, Component Name 2, and Driver Health. |

## Device Discovery

`BpmpI2cStart()` opens BPMP IPC, obtains the parent device path, loads the DTB,
finds `nvidia,tegra186-bpmp-i2c`, reads `nvidia,bpmp-bus-id` or `adapter`,
and derives the parent BPMP phandle from the node's parent.

`BuildI2cDevices()` walks direct child nodes under the BPMP I2C node. Known
compatible strings map to NVIDIA I2C device GUIDs. Some devices have additional
slave addresses derived with mask/OR rules. Unsupported or disabled children
are left as unknown devices.

For `nvidia,vrs-pseq`, `InitializeVrsPseq()` reads
`nvidia,uefi-vrs-pseq-init`, detects whether SMBus PEC is enabled, reads the
target register, applies the requested OR or AND operation, and writes the
updated value back through the same BPMP I2C path.

## I2C Transaction Flow

`BpmpI2cStartRequest()` records the slave address, request packet, completion
event, and completion status pointer, then calls `BpmpIpcProcess()`.

`BpmpIpcProcess()` builds a `BPMP_I2C_CMD_TRANSFER` request. Write operations
copy caller data into the BPMP payload. Read operations reserve response space
and copy response bytes back into the caller buffers after BPMP completion.
SMBus PEC writes append the calculated PEC byte inside the BPMP request. The
last operation is marked with `BPMP_I2C_STOP`.

When the caller supplies an event, the request is asynchronous and the BPMP IPC
token event eventually returns to `BpmpIpcProcess()`. When the caller passes a
`NULL` event, the function recursively completes the synchronous path before
returning.

## Protocols

| Protocol | Purpose |
|----------|---------|
| `EFI_DRIVER_BINDING_PROTOCOL` | Binds the driver to BPMP IPC handles. |
| `EFI_I2C_MASTER_PROTOCOL` | Performs I2C transfers through BPMP. |
| `EFI_I2C_ENUMERATE_PROTOCOL` | Enumerates child I2C devices found in DTB. |
| `EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL` | Enables the single supported bus configuration. |
| `EFI_DEVICE_PATH_PROTOCOL` | Child device path for the I2C controller. |
| `EFI_DRIVER_HEALTH_PROTOCOL` | Reports BPMP I2C transaction health. |

## Health Reporting

| Status | Trigger |
|--------|---------|
| `Healthy` | A BPMP I2C transfer completes successfully and read data is copied back to callers. |
| `Failed` | BPMP IPC returns a synchronous error, asynchronous token completion reports an error, or a critical transaction setup path fails. |

Unsupported I2C operation flags return `EFI_UNSUPPORTED` without changing
health, because that is a caller request issue rather than a BPMP I2C transport
failure.

## Testing

`BpmpI2cDxeUnitTestsHost` covers driver-model helper stubbing and health state
updates for successful, failed, and unsupported transaction paths.

## Related Files

- `Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.c`
- `Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2c.h`
- `Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.inf`
- `Silicon/NVIDIA/Drivers/BpmpI2c/UnitTest/BpmpI2cDxeUnitTests.c`

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
