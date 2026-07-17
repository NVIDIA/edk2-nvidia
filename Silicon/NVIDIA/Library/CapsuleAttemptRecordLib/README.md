# Capsule Attempt Record Library

## Overview

`CapsuleAttemptRecordLib` is the stateless client library used by executable
modules to access the singleton capsule attempt record DXE driver. The library
does not parse records, gather firmware information, or access storage. It
checks the feature PCD, locates
`NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL`, and synchronously forwards the
requested operation.

Each UEFI executable receives its own instance of the library. No state is
stored in that instance, so every call locates the driver protocol again and
all persistent state remains owned by `CapsuleAttemptRecordDxe`.

## Build Support

The library class supports `DXE_DRIVER` and `UEFI_APPLICATION` consumers. It
depends on:

- `PcdCapsuleAttemptRecordSupported`
- `UefiBootServicesTableLib`
- `gNVIDIACapsuleAttemptRecordProtocolGuid`

When `PcdCapsuleAttemptRecordSupported` is `FALSE`, every public operation
returns `EFI_SUCCESS` as a no-op before attempting protocol discovery. The
platform sets the PCD to `TRUE` when `CONFIG_CAPSULE_ATTEMPT_RECORD` is
enabled. The option depends on firmware management, is implied by the common
Android build configuration, and may be enabled independently for other
platforms.

The platform build uses `CONFIG_CAPSULE_ATTEMPT_RECORD` for the feature PCD,
the `CapsuleAttemptRecordDxe` DSC component, and the firmware-volume contents.
The driver publishes the protocol used by this library and must be dispatched
before a client operation is called. If the PCD is enabled but the driver has
not dispatched, the client reports the integration or dispatch problem by
returning `EFI_NOT_READY`.

## Public Operations

The public declarations are in
[`CapsuleAttemptRecordLib.h`](../../Include/Library/CapsuleAttemptRecordLib.h).

### `CapsuleAttemptRecordUpdateFmpImageInfo`

Requests that the driver gather the current FMP image information for
`PcdSystemFmpCapsuleImageTypeIdGuid` and commit a new record. The caller does
not pass an FMP descriptor or storage information; the driver owns both
operations.

Current callers invoke this function after their capsule operation returns:

- `AndroidFastbootApp` calls it immediately after `UpdateCapsule`.
- `PlatformBootManagerLib` calls it after capsule processing has requested a
  reset and before performing that reset. This covers capsule HOBs,
  Capsule-on-Disk, and saved capsules such as an Android OTA capsule.

These supported capsule flows do not use persist-across-reset delivery and do
not request that `UpdateCapsule` initiate a reset. The capsule operation
therefore returns before the Phase 1 snapshot is requested. The committed
record describes the FMP state available after that returned operation; it is
not an in-progress marker written before the operation starts.

The record status is independent of the capsule operation status. A record
failure is never substituted for the original capsule result.

### `CapsuleAttemptRecordDumpLatest`

Requests that the driver read and print the newest valid record without
modifying storage. `PlatformBootManagerLib` calls this before capsule handling
during boot, allowing the previously committed attempt to be reported before
a new Capsule-on-Disk operation can replace it.

## Call Flow

```text
Caller
  -> CapsuleAttemptRecordLib
    -> check PcdCapsuleAttemptRecordSupported
    -> LocateProtocol(NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL)
      -> CapsuleAttemptRecordDxe protocol operation
```

Protocol lookup is not cached. If the driver is not yet available, the current
call returns `EFI_NOT_READY`; a later call performs a new lookup.

## Status Handling

The library returns:

- `EFI_SUCCESS` without protocol discovery when the feature PCD is disabled.
- `EFI_NOT_READY` when the driver protocol cannot be located.
- The status returned by the selected protocol operation after successful
  discovery.

The library emits `DEBUG_INFO` tracing when an operation is skipped because the
feature PCD is disabled. When the feature is enabled, an update request and any
protocol discovery failure are logged at `DEBUG_ERROR`. This makes a reached
update call visible even when `DEBUG_INFO` is filtered and identifies a missing
or undispatched driver as a platform integration error. The library does not
log an error returned by a successfully located driver protocol. Callers log
those operation errors; this includes `EFI_UNSUPPORTED` when the driver
encounters an unsupported configuration or record format.

## Execution Context

Both public operations are synchronous and must be called at
`TPL_APPLICATION`. The driver may allocate memory, enumerate protocols, parse
DT data, and perform Block I/O before returning.

## Related Component

See the
[`CapsuleAttemptRecordDxe` documentation](../../Drivers/CapsuleAttemptRecordDxe/README.md)
for FMP collection, storage discovery, record format, and dump behavior.
