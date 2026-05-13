# UsbRndisDxe Design Document

## Overview

`UsbRndisDxe` binds to USB RNDIS control and data interfaces and exposes a
UEFI Simple Network Protocol interface for BMC USB networking. The driver
initializes the RNDIS control channel, discovers USB endpoints, queries link and
address information, installs a child network device path, and exposes SNP plus
NVIDIA USB NIC information protocols after the data interface is attached.

## Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | The driver shall bind only to USB interfaces that match RNDIS control or data interface descriptors. |
| FR-02 | The driver shall wait for the RNDIS control interface before accepting the data interface. |
| FR-03 | The driver shall initialize the RNDIS device and query MAC, link, packet, and transfer parameters. |
| FR-04 | The driver shall publish `EFI_SIMPLE_NETWORK_PROTOCOL` after both control and data interfaces are ready. |
| FR-05 | The driver shall publish `NVIDIA_USB_NIC_INFO_PROTOCOL` with USB NIC metadata. |
| FR-06 | The driver shall create a MAC-address device path for the network child handle. |
| FR-07 | The driver shall support SNP start, stop, initialize, reset, shutdown, receive filter, transmit, receive, and status operations. |
| FR-08 | The driver shall publish Driver Binding, Component Name, Component Name 2, and Driver Health. |

## Binding Flow

`UsbRndisDriverSupported()` opens `EFI_USB_IO_PROTOCOL` and checks whether the
interface is an RNDIS control or data interface. Data interfaces are rejected
until `GetRndisPrivateData()` can find the already-started control interface.

`UsbRndisDriverStart()` has two paths. The data-interface path attaches the data
USB I/O protocol to the existing private data, configures bulk and interrupt
endpoints, installs SNP and USB NIC info protocols on the child handle, and
opens USB I/O by child controller.

The control-interface path allocates private data, creates the receive-control
timer, initializes SNP service and USB NIC info state, opens the parent device
path, appends a MAC-address node, installs a caller ID marker on the controller,
and installs the child device path. SNP itself is installed later when the data
interface arrives.

## RNDIS And SNP Flow

`Rndis.c` implements the RNDIS control messages used by SNP. Initialization
resets the RNDIS device, sends an initialize message, queries media state,
current and permanent MAC addresses, maximum frame and transfer sizes, packet
alignment, and link speed.

`Snp.c` implements the SNP operations. Transmit wraps Ethernet frames in RNDIS
packet messages and sends them over USB bulk OUT. Receive collects RNDIS packet
messages from USB bulk IN, queues them, and copies frame payloads to the SNP
caller. A timer slows receive polling when no packets are available.

## Stop Flow

`UsbRndisDriverStop()` marks the USB NIC as lost, uninstalls the caller ID
marker, uninstalls child device path, SNP, and USB NIC info protocols, cancels
and closes the receive-control timer, closes consumed USB I/O and device path
protocols, and frees private data.

## Protocols

| Protocol | Purpose |
|----------|---------|
| `EFI_USB_IO_PROTOCOL` | Consumed USB control and data interface access. |
| `EFI_SIMPLE_NETWORK_PROTOCOL` | Produced network interface. |
| `NVIDIA_USB_NIC_INFO_PROTOCOL` | Produced NVIDIA USB NIC metadata. |
| `EFI_DEVICE_PATH_PROTOCOL` | Child MAC-address device path. |
| `EFI_DRIVER_HEALTH_PROTOCOL` | Reports USB RNDIS binding and cleanup health. |

## Health Reporting

| Status | Trigger |
|--------|---------|
| `Healthy` | Control interface start completes, data interface start completes, or stop cleanup completes. |
| `Failed` | Private allocation, RNDIS/SNP initialization, USB NIC info initialization, device path handling, protocol installation, child open, protocol uninstall, or cleanup fails. |

Unsupported interfaces and a data interface that arrives before the control
interface are normal binding outcomes and do not indicate driver failure.

## Testing

There is no current host-based unit test for this driver. Validation is through
platform builds, USB device enumeration, and network stack consumers of SNP.

## Related Files

- `Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/UsbRndisDxe.c`
- `Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/Rndis.c`
- `Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/Snp.c`
- `Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/UsbNicInfo.c`
- `Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/UsbRndisDxe.inf`

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
