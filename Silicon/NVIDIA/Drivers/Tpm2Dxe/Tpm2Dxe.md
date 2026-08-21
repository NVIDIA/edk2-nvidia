# Tpm2Dxe Design Document

## Overview

`Tpm2Dxe` binds to NVIDIA QSPI controller handles that contain a supported TPM
device node and exposes `NVIDIA_TPM2_PROTOCOL` on a child handle. The protocol
implements TPM register read and write transactions over QSPI using the TPM TIS
SPI framing expected by supported TPM devices.

## Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | The driver shall bind only to QSPI controllers with a supported TPM-compatible child node. |
| FR-02 | The driver shall recognize `tcg,tpm_tis-spi` and `infineon,slb9670` compatible strings. |
| FR-03 | The driver shall read the TPM chip-select from the child node `reg` property. |
| FR-04 | The driver shall reject TPM chip-select values outside the QSPI controller chip-select count. |
| FR-05 | The driver shall expose `NVIDIA_TPM2_PROTOCOL` and a child device path. |
| FR-06 | The driver shall perform TPM reads and writes through QSPI combined sequence mode with 3-byte addresses. |
| FR-07 | The driver shall update `PcdTpmPresent` and `PcdTpmEnable` based on detected TPM compatibility. |
| FR-08 | The driver shall publish Driver Binding, Component Name, Component Name 2, and Driver Health. |

## Discovery Flow

`Tpm2DxeDriverBindingSupported()` opens the QSPI controller protocol and calls
`CheckTpmCompatibility()`. Compatibility scanning walks the QSPI device tree
children and checks each node against the supported TPM compatible strings.
When no TPM is found, the driver sets TPM-present and TPM-enable PCDs to false.
When a TPM is found, it sets TPM-present true and derives TPM-enable from
`IsTpmToBeEnabled()`.

## Start Flow

`Tpm2DxeDriverBindingStart()` opens the QSPI controller by driver, allocates
private data, reads TPM properties from the device tree, appends a vendor device
path node, installs `gNVIDIATpm2ProtocolGuid` and the device path on a child
handle, installs a caller ID marker on the parent controller, and opens the
caller ID protocol by child controller.

The stop path reverses those steps: it closes the parent-child relationship,
uninstalls the TPM protocol and device path, frees the device path and private
data, removes the caller ID marker, and closes QSPI.

## TPM Transfer Flow

`Tpm2Transfer()` accepts a TPM register address, data buffer, transfer size, and
read/write direction. It builds a QSPI transaction with TPM SPI command bits and
address prefix, enables QSPI wait-state settings, performs the transaction, and
then disables wait-state settings. Transfer sizes larger than
`TPM_MAX_TRANSFER_SIZE` are rejected.

## Protocols

| Protocol | Purpose |
|----------|---------|
| `gNVIDIAQspiControllerProtocolGuid` | Consumed parent QSPI transaction interface. |
| `gNVIDIATpm2ProtocolGuid` | Produced TPM2 transfer interface. |
| `EFI_DEVICE_PATH_PROTOCOL` | Child path for the TPM instance. |
| `EFI_DRIVER_HEALTH_PROTOCOL` | Reports TPM driver binding health. |

## Health Reporting

| Status | Trigger |
|--------|---------|
| `Healthy` | Driver Binding Start completes successfully, stop with no children is requested, or stop cleanup completes successfully. |
| `Failed` | Start or stop returns an EFI error after a supported TPM controller is being managed. |

Unsupported controllers and invalid caller requests return their normal
`EFI_STATUS` values and are not treated as driver health failures.

## Testing

`Tpm2DxeUnitTestsHost` covers driver binding start and stop health transitions
with host stubs. TPM transfer behavior requires QSPI and TPM hardware or a
hardware model.

## Related Files

- `Silicon/NVIDIA/Drivers/Tpm2Dxe/Tpm2Dxe.c`
- `Silicon/NVIDIA/Drivers/Tpm2Dxe/Tpm2Internal.h`
- `Silicon/NVIDIA/Drivers/Tpm2Dxe/Tpm2Dxe.inf`
- `Silicon/NVIDIA/Drivers/Tpm2Dxe/UnitTest/Tpm2DxeUnitTests.c`

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
