# AndroidBootDxe Design Document

## Overview

`AndroidBootDxe` exposes Android boot images to the UEFI boot manager through
Load File protocols. It can boot from Android boot partitions, vendor boot
partitions, init boot partitions, a preloaded kernel address from DTB, or an RCM
kernel base PCD. The driver validates Android boot headers, builds kernel
arguments, installs boot image load handles, applies DTB/DTBO updates, and
updates the kernel command line before loading the kernel.

## Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | The driver shall detect valid Android boot images on GPT partitions. |
| FR-02 | The driver shall reject Android boot partition handling when an EFI System Partition is present on the same disk. |
| FR-03 | The driver shall support Android boot header versions handled by `AndroidBootImgHeader.h`. |
| FR-04 | The driver shall support Android vendor boot version 4 metadata. |
| FR-05 | The driver shall install `EFI_LOAD_FILE_PROTOCOL` and kernel-args protocol on child handles for bootable Android partitions. |
| FR-06 | The driver shall install `EFI_LOAD_FILE2_PROTOCOL` for initrd delivery when needed. |
| FR-07 | The driver shall select active, recovery, and fastboot userspace partitions based on boot-chain and misc partition state. |
| FR-08 | The driver shall load and patch kernel DTB data, apply DTBO overlays, and update bootconfig when applicable. |
| FR-09 | The driver shall support memory boot and RCM boot paths without requiring partition binding. |
| FR-10 | The driver shall publish Driver Binding, Component Name, Component Name 2, and Driver Health for the partition binding path. |

## Partition Binding Flow

`AndroidBootDriverBindingSupported()` checks that the controller has GPT
partition information, Block I/O, Disk I/O, and no existing caller ID marker. It
rejects invalid GPT ranges and rejects Android boot handling when an EFI System
Partition is found on the same parent disk. It then calls
`AndroidBootGetVerify()` to validate that the partition contains an Android boot
image.

`AndroidBootDriverBindingStart()` opens partition, block, and disk protocols,
allocates the kernel argument buffer, validates the boot image again, appends a
URI device path node, allocates private data, installs Load File and kernel-args
protocols on a child handle, creates the connect-complete event, installs the
caller ID marker, and opens the parent-child relationship.

The connect-complete callback checks misc partition state and active slot
selection. Non-selected boot partitions are disconnected by uninstalling their
Load File protocols so the boot manager sees only the active Android boot
candidate.

## Boot Image Flow

`AndroidBootGetVerify()` reads Android boot image headers either from partition
Block I/O and Disk I/O or from a memory boot source. It validates magic, header
version, page size, signature header offset, image size, and partition bounds.
When requested, it records kernel, ramdisk, and page-size information and
extracts kernel arguments.

`AndroidBootDxeLoadFile()` appends AVB and Android boot arguments, loads DTB
content, updates `/chosen/bootargs`, appends Android boot image protocol
arguments, and loads the kernel into the caller-provided buffer.

`AndroidBootDxeLoadFile2()` serves the initrd buffer through Load File 2 when an
initrd has been staged.

## DTB And Bootconfig Flow

`AndroidBootDxeLoadDtb()` locates sibling DTB and DTBO partitions based on the
boot partition name and recovery state. It validates DTB and DTBO containers,
applies overlays, installs the resulting DTB as the firmware FDT configuration
table, and patches selected `/chosen` and reserved-memory data from the UEFI
DTB when hypervisor mode is present.

The function also prepares boot-time bootconfig data, copies NCT and TNSPEC
data into the kernel DTB, populates MAC addresses, and adds RPMB information for
Android image boot when available.

## Memory Boot And RCM Flow

At entry, `AndroidBootDxeDriverEntryPoint()` checks `GetKernelAddress()` and RCM
kernel PCDs. If a kernel is already loaded, it installs an RCM-style Load File
handle on the image handle and, for the DTB-address path, installs a relocated
kernel DTB configuration table. In memory boot mode the partition driver model
protocols are not installed because the boot source is not a partition
controller.

## Protocols

| Protocol | Purpose |
|----------|---------|
| `EFI_LOAD_FILE_PROTOCOL` | Presents Android kernel image loading to the boot manager. |
| `EFI_LOAD_FILE2_PROTOCOL` | Presents initrd loading. |
| `gNVIDIALoadfileKernelArgsProtocol` | Stores mutable kernel command line state. |
| `gAndroidBootImgProtocolGuid` | Appends Android boot image arguments. |
| `EFI_DRIVER_HEALTH_PROTOCOL` | Reports partition binding health. |

## Health Reporting

| Status | Trigger |
|--------|---------|
| `Healthy` | Partition Driver Binding Start completes successfully or Stop cleanup completes successfully. |
| `Failed` | Start fails while opening required protocols, allocating state, validating the boot image, installing protocols, creating events, or opening parent-child relationships. |

The memory boot and RCM paths run before driver model protocol installation and
therefore do not publish Driver Health state. Load-time kernel image failures
are returned to the boot manager through the Load File status.

## Testing

`AndroidBootDxeUnitTest` covers Android boot header parsing, kernel argument
update behavior, and selected Driver Binding health paths. The health tests
verify that early Start failures are reported through Driver Health and that
unsupported child-handle Stop requests do not mutate the current health state.

## Related Files

- `Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.c`
- `Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootConfig.c`
- `Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDtbImgParser.c`
- `Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf`
- `Silicon/NVIDIA/Drivers/AndroidBootDxe/UnitTest/`

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
