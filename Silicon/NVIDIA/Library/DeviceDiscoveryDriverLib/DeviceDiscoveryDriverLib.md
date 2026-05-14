# DeviceDiscoveryDriverLib Design Document

## Overview

`DeviceDiscoveryDriverLib` is a shared DXE library for NVIDIA drivers that
discover non-discoverable devices from the platform device tree. A consuming
driver provides compatibility mappings, configuration flags, and a
`DeviceDiscoveryNotify()` callback. The library handles common device tree
enumeration, non-discoverable device protocol creation, optional clock/reset
and power-gate sequencing, optional threaded device start, exit-boot-services
cleanup, Component Name 2, and Driver Health registration.

## Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | The library shall enumerate device tree nodes that match the driver's compatibility table. |
| FR-02 | The library shall create non-discoverable device handles for matching nodes. |
| FR-03 | The library shall notify the consuming driver at defined discovery and driver-binding phases. |
| FR-04 | The library shall optionally deassert power gates, enable clocks, reset modules, or deassert resets before driver start. |
| FR-05 | The library shall optionally undo clock/reset/power-gate state at ExitBootServices. |
| FR-06 | The library shall support delayed enumeration for drivers that wait for another protocol notification. |
| FR-07 | The library shall support threaded start for slow device initialization. |
| FR-08 | The library shall publish Driver Health for the consuming driver image. |

## Configuration Model

The consuming module defines `gDeviceCompatibilityMap` and
`gDeviceDiscoverDriverConfig`.

`gDeviceCompatibilityMap` maps device tree compatible strings to
non-discoverable device type GUIDs. A mapping with a `NULL` device type rejects
the match after compatibility detection.

`gDeviceDiscoverDriverConfig` controls automatic setup:

| Field | Effect |
|-------|--------|
| `DriverName` | Component Name 2 driver string. |
| `AutoEnableClocks` | Enables all clock nodes before driver start. |
| `AutoDeassertReset` | Deasserts all reset nodes before driver start. |
| `AutoResetModule` | Performs module reset before driver start. |
| `AutoDeassertPg` | Deasserts all power gates before driver start. |
| `SkipEdkiiNondiscoverableInstall` | Suppresses installation of the EDK II non-discoverable protocol alias. |
| `SkipAutoDeinitControllerOnExitBootServices` | Suppresses automatic cleanup callback registration. |
| `DelayEnumeration` | Defers `DeviceDiscoveryEnumerateDevices()` until the driver calls it. |
| `ThreadedDeviceStart` | Starts each device on a cooperative thread context. |
| `DisableInRcm` | Prevents initialization during RCM boot. |

## Discovery Flow

1. `DeviceDiscoveryDriverInitialize()` reads platform resource data from the
   platform resource HOB.
2. RCM mode is rejected when `DisableInRcm` is set.
3. The library locates SCMI clock and NVIDIA clock-parent protocols.
4. Component Name 2 and Driver Health are installed on the image handle.
5. `DeviceDiscoveryNotify(DeviceDiscoveryDriverStart, ...)` lets the driver do
   module-level setup.
6. Unless `DelayEnumeration` is set, `DeviceDiscoveryEnumerateDevices()` walks
   supported device tree nodes and creates controller handles.
7. Each controller is passed through `DeviceDiscoveryStart()`, which runs
   supported/start notifications and optional resource sequencing.
8. Enumeration completion is reported with
   `DeviceDiscoveryNotify(DeviceDiscoveryEnumerationCompleted, ...)` after all
   synchronous starts, or after all threaded starts complete.

## Controller Start Flow

For each supported controller, `DeviceDiscoveryStart()` opens the NVIDIA
non-discoverable device protocol, locates the device tree node protocol, calls
the consumer's supported-phase notification, performs configured automatic
power/clock/reset operations, allocates a discovery context, optionally creates
an ExitBootServices cleanup event, starts the consumer driver directly or in a
thread, and installs `gNVIDIADeviceDiscoveryContextGuid`.

When EDK II non-discoverable protocol aliasing is enabled, the same
non-discoverable protocol instance is also installed under
`gEdkiiNonDiscoverableDeviceProtocolGuid`.

## ExitBootServices Cleanup Flow

`DeviceDiscoveryOnExitBootServices()` notifies the consumer, optionally asserts
power gates, disables clocks, resets or asserts resets, and hides MMIO resource
descriptors when ACPI is not installed. This prevents OS-visible resources from
remaining exposed after firmware-only devices are deinitialized.

## Threaded Start

Threaded mode uses `SystemContextLib` to switch to a per-device stack and
returns to the main context when `DeviceDiscoveryThreadMicroSecondDelay()` is
called or when start completes. The optional async status protocol reports
whether any subthreads are still running.

## Health Reporting

The library installs Driver Health during initialization. Health is updated as
follows:

| Status | Trigger |
|--------|---------|
| `Healthy` | Delayed enumeration is selected, or enumeration and completion notifications finish without errors. |
| `Failed` | Supported-node discovery, node processing, memory allocation, resource sequencing, start notification, or completion notification fails. |

Errors during delayed enumeration are reported when
`DeviceDiscoveryEnumerateDevices()` is eventually called. Threaded start errors
are logged in the thread body; completion status is reflected when the library
reaches enumeration completion.

## Testing

Host-based unit coverage is provided by
`UnitTest/DeviceDiscoveryDriverLibUnitTests.c`. The tests exercise the shared
library behavior that can regress without target hardware:

- Device-tree compatibility mapping returns the configured device type GUID and
  invokes the consumer compatibility notification.
- Enumeration reports healthy Driver Health when no supported nodes are found
  or when a matching controller is processed and started successfully.
- Supported-node discovery failures, node-info allocation failure, node
  processing failure, controller start failure, and completion notification
  failure report failed Driver Health.
- Failure paths that occur before a controller handle owns the device structure
  are covered so cleanup and health reporting stay aligned.

Consumer-specific callback behavior is still validated in the consuming driver
tests and on target hardware.

## Related Files

- `Silicon/NVIDIA/Library/DeviceDiscoveryDriverLib/DeviceDiscoveryDriverLib.c`
- `Silicon/NVIDIA/Library/DeviceDiscoveryDriverLib/DeviceDiscoveryDriverLibServices.c`
- `Silicon/NVIDIA/Include/Library/DeviceDiscoveryDriverLib.h`
- `Silicon/NVIDIA/Library/DeviceDiscoveryDriverLib/DeviceDiscoveryDriverLib.inf`

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
