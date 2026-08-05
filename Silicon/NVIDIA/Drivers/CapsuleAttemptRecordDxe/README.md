# Capsule Attempt Record DXE Driver

## Overview

`CapsuleAttemptRecordDxe` is the singleton owner of the capsule attempt record.
It produces `NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL`, gathers the configured
system firmware descriptor from FMP, discovers a DT-described region in an
OS-media GPT partition, and reads or commits records through Block I/O.

The driver contains two implementation source modules:

- `CapsuleAttemptRecordDxe.c` implements feature gating, protocol publication,
  FMP discovery, and protocol request handling.
- `CapsuleAttemptRecordDxeInternal.c` implements DT parsing, partition
  discovery, Block I/O, slot validation and selection, DTB payload generation,
  record commits, and debug dumping.

The internal source is also compiled directly into the host-test executable so
that format and storage behavior are tested without exposing a separate
library class.

## Driver Initialization

`CONFIG_CAPSULE_ATTEMPT_RECORD` controls the feature PCD and includes the
driver in both the DSC component list and firmware volume. The option depends
on firmware management, is implied by the common Android build configuration,
and may be enabled independently for other platforms.

The driver has a `TRUE` dependency expression and may load before the target
OS-media partition is connected. Its entry point performs only these actions:

1. If `PcdCapsuleAttemptRecordSupported` is `FALSE`, return `EFI_UNSUPPORTED`
   without installing the protocol or accessing the platform DTB.  The driver
   should be removed from the platform build.
2. If the feature is enabled, install
   `NVIDIA_CAPSULE_ATTEMPT_RECORD_PROTOCOL`.

Storage initialization is lazy. The first protocol request loads the platform
DTB, parses the storage description, locates the partition, and caches the
resulting record context. If initialization fails, the context remains `NULL`
and a later request retries the complete operation.

## Execution Model

Protocol operations are synchronous and must be called at `TPL_APPLICATION`.
Capsule processing and boot-time dumping execute serially in the cooperative
UEFI execution model, so the driver does not raise TPL or maintain a lock.

## Protocol Operations

### `Begin`

Commits an `in-progress` record containing an assigned attempt ID and the
caller's source context. The driver retains that identity for a matching
finalization request. The begin record intentionally has no ESRT entry because
the post-operation FMP result does not yet exist.

### `Finalize`

Gathers the current system FMP descriptor and commits a `completed` record. It
uses the active attempt ID when the source matches a successful begin. If no
matching begin exists, it assigns a new attempt ID so the final snapshot is
still durable. A matching finalization request consumes the active attempt
identity even if gathering or committing the completed record fails. This
prevents a later capsule operation from reusing the earlier attempt ID.

### `UpdateFmpImageInfo`

The update operation performs these steps:

1. Read `PcdSystemFmpCapsuleImageTypeIdGuid`.
2. Enumerate handles that expose `EFI_FIRMWARE_MANAGEMENT_PROTOCOL`.
3. Call `GetImageInfo` on each candidate with one allocated
   `EFI_FIRMWARE_IMAGE_DESCRIPTOR`.
4. Select the first valid result containing the configured image type GUID.
5. Initialize the record context if necessary.
6. Normalize the ESRT-visible scalar fields from the matching descriptor.
7. Build a standalone diagnostic DTB.
8. Select a target slot, write the complete slot, flush it, read it back, and
   validate the result.

This compatibility operation commits a completed record with unknown source.
Current callers use `Begin` and `Finalize`.

The system FMP is produced by `FmpDevicePkg/FmpDxe`. That implementation
exposes exactly one fixed-size firmware image descriptor, and the driver asserts
that an FMP result containing `PcdSystemFmpCapsuleImageTypeIdGuid` has a
descriptor count of one. Other FMP instances may expose multiple descriptors,
but they must not use the configured system firmware image type GUID.
Candidates that fail `GetImageInfo`, return invalid descriptor metadata, need a
larger buffer, or do not contain the configured GUID are skipped. Descriptor
pointer fields are never persisted.

### `DumpLatest`

The dump operation initializes the record context if necessary, selects the
newest valid slot, validates its envelope and payload, and prints it. It does
not write, clear, or otherwise modify the record.

The payload root `compatible` value and format version are validated
explicitly. Immediate subnodes and their properties are then dumped without a
compiled list of property names. Printable NUL-terminated values are displayed
as strings, values with lengths divisible by four are displayed as big-endian
32-bit cells, and other values are displayed as byte arrays.

## Platform DT Storage Contract

The platform system DTB supplies this node:

```dts
/firmware/uefi/capsule-update-record {
    compatible = "nvidia,capsule-update-record-storage-v1";
    nvidia,format = "nvidia,capsule-attempt-dtb-v1";
    nvidia,partition-name = "<OS-media GPT partition>";
    nvidia,offset = <...>;
    nvidia,size = <...>;
    nvidia,slot-count = <1 or 2>;
};
```

`nvidia,offset`, `nvidia,size`, and `nvidia,slot-count` may be encoded as one
32-bit cell or one 64-bit value. The parser validates the DTB header and total
size, requires the compatible string-list to contain the supported value,
validates the exact format string, partition-name length and policy, slot count,
region division, and allocation bounds. A platform that configures one slot
accepts the power-failure risk described under Slot Envelope; two slots are
required to preserve the previous valid record while committing its
replacement.

## Partition Discovery and Validation

The driver enumerates handles with `EFI_PARTITION_INFO_PROTOCOL` and selects a
GPT partition whose name exactly matches `nvidia,partition-name`. The same
handle must expose `EFI_BLOCK_IO_PROTOCOL`. Exactly one usable match is
required; no match returns `EFI_NOT_FOUND`, while multiple usable matches
return `EFI_UNSUPPORTED`.

The selected media must:

- Be present and identify a logical partition.
- Have a nonzero block size.
- Contain the complete configured region without arithmetic overflow.
- Have block-aligned region and slot boundaries.
- Be writable for an update operation.

Compiled partition allow and deny lists are available to control which
partitions the driver may use. List entries support exact case-sensitive names
and trailing-`*` prefix patterns. The deny list takes precedence, and a
nonempty allow list requires at least one match.

The driver uses Block I/O directly because the record resides on OS media and
has no firmware backup-copy semantics. It does not use
`NVIDIA_FW_PARTITION_PROTOCOL` or byte-granular Disk I/O.

## Slot Envelope

The configured region is divided evenly into one or two slots. Each slot starts
with a 48-byte `CAPSULE_ATTEMPT_RECORD_HEADER` containing:

- Magic `NVCAR001`
- Header version and size
- Sequence number
- Payload offset and length
- Payload format identifier
- Payload CRC32
- Header CRC32
- Reserved fields

All header fields are naturally aligned, and the complete header is copied to
and from the slot buffer rather than accessed through an unaligned pointer.
Slot validation checks the magic, version, header size, reserved values,
payload bounds, header CRC, payload CRC, and the embedded DTB when the format
identifier is known.
Invalid slots are ignored independently.

Sequence comparison uses serial-number arithmetic so selection continues to
work across `UINT64` wrap. When committing:

- If no valid slot exists, slot 0 is written with sequence 1.
- Otherwise, the first invalid slot is preferred.
- If every slot is valid, a one-slot configuration overwrites slot 0 and a
  two-slot configuration writes the slot following the newest slot.

With one slot, the platform explicitly accepts a small power-failure window:
the only valid record is overwritten in place, so power loss after overwriting
begins and before the complete replacement has been successfully flushed can
leave no valid record. With two slots, the newest valid record is retained
while its replacement is written to the other slot.

The driver writes one zero-filled, block-aligned slot image, calls
`FlushBlocks`, reads the slot back, compares every byte with the written image,
and validates the stored envelope again.

## Diagnostic DTB Payload

The current payload is a standalone DTB with this schema:

```dts
/ {
    compatible = "nvidia,capsule-attempt-record-v1";
    nvidia,format-version = <1>;
    nvidia,attempt-id = /bits/ 64 <...>;
    nvidia,attempt-source = "android-fastboot";
    nvidia,attempt-state = "completed";

    esrt-entry-0 {
        nvidia,firmware-class =
            "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
        nvidia,firmware-version = <...>;
        nvidia,lowest-supported-firmware-version = <...>;
        nvidia,last-attempt-version = <...>;
        nvidia,last-attempt-status = <...>;
    };
};
```

`nvidia,attempt-source` is `android-fastboot`, `capsule-on-disk`,
`saved-capsule`, or `unknown`. PlatformBootManager uses `capsule-on-disk` for
Capsule-on-Disk processing and `saved-capsule` for `HandleSavedCapsules`
processing. The compatibility update operation uses `unknown` because it does
not identify a lifecycle producer.
`nvidia,attempt-state` is `in-progress` or `completed`. A begin record has the
three lifecycle properties but no ESRT subnode. Its completed replacement has
the same attempt ID and includes the ESRT snapshot. Existing records without
lifecycle properties remain readable.

The firmware-class GUID is stored as a lowercase canonical string. Firmware
version is always copied from the descriptor. Lowest-supported firmware version
is copied for descriptor version 2 or later and otherwise stored as zero.
Last-attempt version and status are copied for descriptor version 3 or later;
older descriptors store zero and `LAST_ATTEMPT_STATUS_SUCCESS`. Other
descriptor fields, package metadata, and pointer-referenced data are not
stored.

## Error Behavior

- Feature-disabled client calls return `EFI_SUCCESS` as no-ops without reaching
  the driver.
- Missing or invalid DT configuration prevents storage access for that
  request.
- A slot too small to contain the record envelope and a DTB header, or with a
  payload capacity larger than `UINT32`, returns `EFI_BAD_BUFFER_SIZE`.
- Missing, ambiguous, unavailable, unaligned, out-of-bounds, or read-only media
  prevents a record update.
- Invalid slots do not prevent another valid slot from being selected.
- If no valid slot exists, `DumpLatest` returns `EFI_NOT_FOUND`.
- An unsupported envelope or payload format returns `EFI_UNSUPPORTED`.
- Record failures are returned to the caller but do not replace capsule update
  status or alter boot flow.

## Host Tests

The host test is located in `UnitTest/` and compiles
`CapsuleAttemptRecordDxeInternal.c` directly. Its current cases cover:

- Trailing-wildcard partition-name matching.
- Storage DT parsing.
- Diagnostic DTB construction and GUID-string encoding.
- Block I/O commits, slot advancement, corruption fallback, and dumping.
- Read-only media, unaligned regions, and nonmatching FMP image types.
- Sequence comparison across wrap.

## Related Component

See the
[`CapsuleAttemptRecordLib` documentation](../../Library/CapsuleAttemptRecordLib/README.md)
for client-side feature gating, protocol lookup, current call sites, and status
forwarding.
