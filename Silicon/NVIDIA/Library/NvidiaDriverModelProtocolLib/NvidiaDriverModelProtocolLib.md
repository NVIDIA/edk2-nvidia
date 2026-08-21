# NvidiaDriverModelProtocolLib

## Overview

`NvidiaDriverModelProtocolLib` provides shared helpers for NVIDIA DXE drivers
that need to publish standard UEFI driver model protocols. The library keeps
driver binding, Component Name, Component Name 2, and Driver Health protocol
installation consistent across drivers that do not need custom protocol
implementations.

## Purpose

The library is intended for drivers that can report a static driver name, an
optional static controller name, and a module-owned Driver Health state. It
reduces duplicate Component Name and Driver Health boilerplate while allowing
each driver to keep its existing binding, start, stop, health transition, and
controller ownership logic.

## Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | The library shall install standard UEFI driver model protocols for NVIDIA DXE drivers. |
| FR-02 | The library shall support drivers that already install Driver Binding and only need Component Name 2 and Driver Health helpers. |
| FR-03 | The library shall support drivers that already provide custom Component Name protocols and only need Driver Health installation. |
| FR-04 | The library shall return configured driver and controller names through Component Name and Component Name 2. |
| FR-05 | The library shall keep Driver Health state private to each linked driver image. |
| FR-06 | The library shall allow modules to set health status directly or map an `EFI_STATUS` to health status. |
| FR-07 | The library shall report `EFI_UNSUPPORTED` for Driver Health repair operations until a module provides a repair path. |

## Protocol Support

`EFI_DRIVER_BINDING_PROTOCOL` is installed by
`NvidiaInstallDriverModelProtocols()` through the EDK II UefiLib helper.

`EFI_COMPONENT_NAME_PROTOCOL` and `EFI_COMPONENT_NAME2_PROTOCOL` return the
configured driver name and, when configured, a managed controller name.

`EFI_DRIVER_HEALTH_PROTOCOL` reports the module-owned health state and returns
`EFI_UNSUPPORTED` for repair. Drivers publish state changes through
`NvidiaSetDriverHealthStatus()` or
`NvidiaSetDriverHealthStatusFromEfiStatus()`.

## Architecture

Each consuming DXE module links its own instance of the library. The global
protocol context and stored health state therefore belong to that driver image,
not to a system-wide singleton. This lets several drivers use the same helper
without overwriting each other's Component Name strings or Driver Health state.

The common flow is:

1. The driver entry point calls one of the install helpers.
2. The helper records the driver name, optional controller name, optional
   controller protocol GUID, and optional Driver Binding pointer.
3. The helper installs the requested protocols on the driver handle.
4. Protocol installation resets the module health status to
   `EfiDriverHealthStatusHealthy`.
5. The driver updates health when start, stop, asynchronous work, or runtime
   transactions observe real failures or degraded configuration.

When a controller protocol GUID is provided, Component Name controller queries
and Driver Health controller queries first check that the driver currently
manages the controller with `EFI_OPEN_PROTOCOL_BY_DRIVER`. Drivers pass `NULL`
when the health state is driver-wide rather than tied to one controller
protocol.

## API Reference

### NvidiaInstallDriverModelProtocols

```c
EFI_STATUS
EFIAPI
NvidiaInstallDriverModelProtocols (
  IN EFI_HANDLE                   ImageHandle,
  IN EFI_SYSTEM_TABLE             *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding,
  IN EFI_HANDLE                   DriverBindingHandle,
  IN CONST CHAR16                 *DriverName,
  IN CONST CHAR16                 *ControllerName OPTIONAL,
  IN EFI_GUID                     *ControllerProtocolGuid OPTIONAL
  );
```

Installs Driver Binding, Component Name, Component Name 2, and Driver Health
protocols on a driver binding handle. If `ControllerProtocolGuid` is provided,
controller name and health queries are limited to controllers currently managed
by the driver for that protocol.

### NvidiaInstallComponentName2AndDriverHealth

```c
EFI_STATUS
EFIAPI
NvidiaInstallComponentName2AndDriverHealth (
  IN EFI_HANDLE    DriverHandle,
  IN CONST CHAR16  *DriverName,
  IN CONST CHAR16  *ControllerName OPTIONAL,
  IN EFI_GUID      *ControllerProtocolGuid OPTIONAL
  );
```

Installs Component Name, Component Name 2, and Driver Health protocols for a
driver that does not install an `EFI_DRIVER_BINDING_PROTOCOL`.

### NvidiaInstallDriverHealthProtocol

```c
EFI_STATUS
EFIAPI
NvidiaInstallDriverHealthProtocol (
  IN EFI_HANDLE                   DriverHandle,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding OPTIONAL,
  IN EFI_GUID                     *ControllerProtocolGuid OPTIONAL
  );
```

Installs only Driver Health on a driver handle. This is useful for drivers that
already provide custom Component Name implementations.

### NvidiaUninstallDriverHealthProtocol

```c
EFI_STATUS
EFIAPI
NvidiaUninstallDriverHealthProtocol (
  IN EFI_HANDLE  DriverHandle
  );
```

Uninstalls Driver Health from a driver handle.

### NvidiaSetDriverHealthStatus

```c
EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatus (
  IN EFI_DRIVER_HEALTH_STATUS  HealthStatus
  );
```

Updates the Driver Health status reported by the module.

### NvidiaSetDriverHealthStatusFromEfiStatus

```c
EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatusFromEfiStatus (
  IN EFI_STATUS  Status
  );
```

Maps `EFI_SUCCESS` to `EfiDriverHealthStatusHealthy` and EFI errors to
`EfiDriverHealthStatusFailed`.

### NvidiaGetDriverHealthStatus

```c
EFI_STATUS
EFIAPI
NvidiaGetDriverHealthStatus (
  OUT EFI_DRIVER_HEALTH_STATUS  *HealthStatus
  );
```

Returns the current module-owned Driver Health status.

## Usage Notes

Drivers should pass a controller protocol GUID only when it reflects a stable
protocol opened by driver binding with `EFI_OPEN_PROTOCOL_BY_DRIVER`. Passing
`NULL` limits the generic implementation to driver-level names and cumulative
health queries.

The helper stores protocol context in module-global state. Because EDK II links
libraries into each consuming module, that state is private to each driver
image.

Protocol installation resets the module health state to
`EfiDriverHealthStatusHealthy`. Drivers should set a more specific status after
the operation that detects degraded hardware, missing configuration, or a reboot
requirement.

## Health Status Semantics

| Status | Meaning |
|--------|---------|
| `EfiDriverHealthStatusHealthy` | The driver has completed its most recent observable operation successfully. |
| `EfiDriverHealthStatusFailed` | The driver detected a hardware, protocol, allocation, transaction, or cleanup failure that can affect service. |
| `EfiDriverHealthStatusConfigurationRequired` | The driver can continue with fallback data, but platform configuration or persistent data should be corrected. |
| `EfiDriverHealthStatusRebootRequired` | The driver made a change that requires another boot before it can be fully reflected. |

The helper intentionally does not infer these states from all return values.
Each module owns the policy because `EFI_UNSUPPORTED`, `EFI_NOT_FOUND`, or
`EFI_ALREADY_STARTED` can be normal in some driver binding paths.

## Related Files

- `Silicon/NVIDIA/Include/Library/NvidiaDriverModelProtocolLib.h` - Public
  library interface.
- `Silicon/NVIDIA/Library/NvidiaDriverModelProtocolLib/NvidiaDriverModelProtocolLib.c` -
  Shared protocol implementations and install helpers.
- `Silicon/NVIDIA/Library/NvidiaDriverModelProtocolLib/NvidiaDriverModelProtocolLib.inf` -
  Library instance metadata.
- `Silicon/NVIDIA/Library/NvidiaDriverModelProtocolLib/UnitTest/NvidiaDriverModelProtocolLibUnitTests.c` -
  Host-based tests for health state reporting.

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
