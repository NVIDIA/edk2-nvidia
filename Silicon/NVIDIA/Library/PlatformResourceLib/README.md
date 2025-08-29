# PlatformResourceLib Documentation

## Overview

The PlatformResourceLib is a collection of libraries that provide platform-specific resource configuration and management for NVIDIA Tegra-based systems. These libraries handle hardware resource discovery, memory layout configuration, and platform-specific initialization during the UEFI boot process.

## Data Source Architecture

The platform resource libraries use two primary data sources for hardware configuration:

1. **FDT (Flattened Device Tree)** - Device tree-based configuration
2. **CPU BL Parameters** - Bootloader parameter data structure

## Library Classification

### FDT-Based Libraries

These libraries rely on Flattened Device Tree (FDT) for hardware resource discovery and configuration:

#### T41xFdtPlatformResourceLib
- **File**: `T41xFdtPlatformResourceLib.inf`
- **Source**: `T410FdtResourceConfig.c`
- **Dependencies**:
  - `DeviceTreeFdtBootHelperLib`
  - `libfdt.h` (direct FDT parsing)
- **Key Features**:
  - FDT blob parsing for DRAM regions
  - Carveout region management from device tree
  - Socket and core configuration from FDT
  - Boot chain management via FDT
- **PCD Configuration**:
  - `PcdNvFdtBlobBase` - FDT blob base address
  - `PcdUefiCoBase` - UEFI carveout base
  - `PcdUefiCoSize` - UEFI carveout size

### CPU BL Parameter-Based Libraries

These libraries rely on CPU Bootloader (BL) parameter data structures for hardware configuration:

#### T23xPlatformResourceLib (T234)
- **File**: `T23xPlatformResourceLib.inf`
- **Source**: `T234ResourceConfig.c`
- **Dependencies**:
  - `GoldenRegisterLib`
  - `MceAriLib`
  - `FloorSweepingLib`
- **Key Features**:
  - CPU BL parameter parsing
  - Golden register-based configuration
  - MCE ARI aperture management
  - Floor sweeping support
- **PCD Configuration**:
  - `PcdTegraCombinedUartTxMailbox`
  - `PcdTegraMceAriApertureBaseAddress`
  - `PcdBootChainRegisterBaseAddressT234`
  - `PcdBootLoaderRegisterBaseAddressT234`

#### T24xPlatformResourceLib (TH500)
- **File**: `T24xPlatformResourceLib.inf`
- **Source**: `TH500ResourceConfig.c`
- **Dependencies**:
  - `GoldenRegisterLib`
  - `ErotLib`
- **Key Features**:
  - CPU BL parameter-based configuration
  - ERoT (Embedded Root of Trust) integration
  - TCG2 event log support
  - Multi-socket support (up to 4 sockets)
- **PCD Configuration**:
  - `PcdSbsaUartBaseTH500`
  - `PcdGenericWatchdogControlBase`
  - `PcdGenericWatchdogRefreshBase`

#### T26xPlatformResourceLib (T264)
- **File**: `T26xPlatformResourceLib.inf`
- **Source**: `T264ResourceConfig.c`
- **Dependencies**:
  - `GoldenRegisterLib`
  - `BootChainInfoLib`
  - `FloorSweepingLib`
- **Key Features**:
  - CPU BL parameter parsing
  - Boot chain information management
  - Floor sweeping configuration
  - Fuse-based configuration
- **PCD Configuration**:
  - `PcdExposeCcplexInterworldShmem`
  - `PcdCapsulePartitionEnabled`
  - `PcdCapsulePartitionSize`

#### T41xPlatformResourceLib (T410 - Non-FDT)
- **File**: `T41xPlatformResourceLib.inf`
- **Source**: `T410ResourceConfig.c`
- **Dependencies**:
  - `GoldenRegisterLib`
  - `ErotLib`
- **Key Features**:
  - CPU BL parameter-based configuration
  - ERoT integration
  - Multi-socket support (up to 2 sockets)
  - Core floor sweeping
- **PCD Configuration**:
  - `PcdGenericWatchdogControlBase`
  - `PcdGenericWatchdogRefreshBase`

## Key Differences

### FDT-Based Approach
- **Data Source**: Device tree blob provided by bootloader
- **Parsing**: Uses `libfdt` library for device tree parsing
- **Configuration**: Dynamic configuration based on device tree content
- **Memory Management**: Carveout regions defined in device tree
- **Advantages**: 
  - Standardized hardware description
  - Dynamic configuration
  - OS-compatible hardware description
- **Use Cases**: Systems requiring device tree compatibility

### CPU BL Parameter-Based Approach
- **Data Source**: Bootloader parameter data structures
- **Parsing**: Direct memory access to bootloader parameters
- **Configuration**: Static configuration based on platform definitions
- **Memory Management**: Predefined carveout regions
- **Advantages**:
  - Faster initialization
  - Platform-optimized configuration
  - Direct hardware register access
- **Use Cases**: Legacy systems and platform-specific optimizations

## Common Components

All platform resource libraries share common components:

### Core Files
- `PlatformResourceLib.c` - Main library implementation
- `PlatformResourceConfig.c` - Common configuration functions
- `CommonResourceConfig.c` - Shared resource management

### Shared Dependencies
- `TegraPlatformInfoLib` - Platform identification
- `BaseMemoryLib` - Memory operations
- `HobLib` - Hand-Off Block management
- `BaseLib` - Basic library functions

### Common Features
- Memory region management
- MMIO resource allocation
- Platform resource information HOB creation
- Core and socket configuration
- Boot chain management

## Usage Guidelines

### Selecting the Appropriate Library

1. **Use FDT-based libraries when**:
   - System requires device tree compatibility
   - Dynamic hardware configuration is needed
   - OS expects device tree hardware description
   - Bootloader provides FDT blob

2. **Use CPU BL parameter-based libraries when**:
   - Platform-specific optimization is required
   - Faster boot time is critical
   - Legacy system compatibility is needed
   - Bootloader provides parameter data structures

### Build Configuration

Include the appropriate library in your platform DSC file:

```dsc
# For FDT-based configuration
PlatformResourceLib|Silicon/NVIDIA/Library/PlatformResourceLib/T41xFdtPlatformResourceLib.inf

# For CPU BL parameter-based configuration
PlatformResourceLib|Silicon/NVIDIA/Library/PlatformResourceLib/T23xPlatformResourceLib.inf
```

### PCD Configuration

Configure the required PCDs based on your chosen library:

```dsc
# FDT-based PCDs
gNVIDIATokenSpaceGuid.PcdNvFdtBlobBase|0x80000000
gNVIDIATokenSpaceGuid.PcdUefiCoBase|0x90000000
gNVIDIATokenSpaceGuid.PcdUefiCoSize|0x10000000

# CPU BL parameter-based PCDs
gNVIDIATokenSpaceGuid.PcdTegraCombinedUartTxMailbox|0x03100000
gNVIDIATokenSpaceGuid.PcdTegraMceAriApertureBaseAddress|0x40000000
```

## Implementation Notes

### FDT Integration
- FDT libraries use `libfdt` for device tree parsing
- Device tree validation is performed during initialization
- Memory layout is calculated based on FDT carveouts
- Platform information is extracted from device tree nodes

### CPU BL Parameter Integration
- Parameters are accessed via memory-mapped registers
- Configuration is based on platform-specific definitions
- Golden registers provide hardware configuration
- Bootloader parameters are parsed during initialization

### Error Handling
- Both approaches include comprehensive error handling
- Invalid configurations are detected and reported
- Fallback mechanisms are provided where possible
- Debug output is available for troubleshooting

## Testing and Validation

### FDT Libraries
- Validate FDT blob integrity
- Test device tree parsing functions
- Verify memory layout calculation
- Test platform information extraction

### CPU BL Parameter Libraries
- Validate bootloader parameter structures
- Test golden register access
- Verify platform-specific configuration
- Test floor sweeping functionality

## Future Considerations

- Migration path from CPU BL parameters to FDT
- Enhanced device tree support for newer platforms
- Improved error handling and recovery mechanisms
- Standardization of configuration interfaces

## Related Documentation

- [FDT PrePi Feature Documentation](../FDT_PREPI_FEATURE_DOCUMENTATION.md)
- [Platform Resource Library API Reference](../Include/Library/PlatformResourceLib.h)
- [Tegra Platform Information Library](../TegraPlatformInfoLib/README.md)

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
