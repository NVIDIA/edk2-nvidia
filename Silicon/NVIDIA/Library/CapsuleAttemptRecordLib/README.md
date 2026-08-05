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

### `CapsuleAttemptRecordBegin`

Commits a durable `in-progress` record before capsule processing starts. The
caller supplies the coarse producer context: Android fastboot,
PlatformBootManager `HandleCapsules`, or PlatformBootManager
`HandleSavedCapsules`. The driver assigns an attempt ID and retains it for the
matching finalization request.

### `CapsuleAttemptRecordFinalize`

Gathers the post-operation FMP image information and commits a `completed`
record. When a matching begin succeeded, the completed record uses the same
attempt ID. A finalization without a matching begin remains valid and receives
a new attempt ID, preserving the existing PlatformBootManager reset-path
snapshot behavior. A matching finalization consumes the active attempt identity
even if the completed record cannot be gathered or committed, so a later
capsule operation cannot inherit the earlier attempt ID.

### `CapsuleAttemptRecordUpdateFmpImageInfo`

Requests that the driver gather the current FMP image information for
`PcdSystemFmpCapsuleImageTypeIdGuid` and commit a new record. The caller does
not pass an FMP descriptor or storage information; the driver owns both
operations.

This operation remains available for compatibility and commits a completed
record with unknown source. Current call sites use the lifecycle operations:

- `AndroidFastbootApp` begins immediately before `UpdateCapsule` and finalizes
  immediately after it returns.
- `PlatformBootManagerLib` begins immediately before Capsule-on-Disk relocation
  or saved-capsule `UpdateCapsule` processing and finalizes immediately after
  the operation returns. Capsule-on-Disk uses the `capsule-on-disk`
  source, while saved-capsule processing uses `saved-capsule`.

Capsule attempt records are not supported for HOB-delivered capsule updates.
That path intentionally performs no Begin, Finalize, or compatibility update
operation.

These supported capsule flows do not use persist-across-reset delivery and do
not request that `UpdateCapsule` initiate a reset. The completed record
describes the FMP state available after the returned operation. If reset or
power loss prevents finalization, the durable in-progress record remains as
evidence that processing began.

The record status is independent of the capsule operation status. A record
failure is never substituted for the original capsule result.

### `CapsuleAttemptRecordDumpLatest`

Requests that the driver read and print the newest valid record without
modifying storage. `PlatformBootManagerLib` calls this before capsule handling
during boot, allowing the previously committed attempt to be reported before
a new capsule operation can replace it.

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

## Execution Context

All public operations are synchronous and must be called at
`TPL_APPLICATION`. The driver may allocate memory, enumerate protocols, parse
DT data, and perform Block I/O before returning.

## Related Component

See the
[`CapsuleAttemptRecordDxe` documentation](../../Drivers/CapsuleAttemptRecordDxe/README.md)
for FMP collection, storage discovery, record format, and dump behavior.
