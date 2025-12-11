# MmioUtil - MMIO Read/Write Utility

## Overview

MmioUtil is a UEFI shell application that safely performs MMIO (Memory-Mapped I/O) reads and writes, even when the target address is not in the UEFI memory map. Unlike the standard shell `mm` command which crashes on unmapped addresses or bus errors, MmioUtil:

1. **Automatically maps** unmapped addresses to the GCD memory map
2. **Catches exceptions** (translation faults, permission faults, bus errors) and reports them gracefully instead of crashing

## Problem Statement

The UEFI shell provides an `mm` command for memory access:

```
Shell> mm 0x12340000
```

However, on ARM64 systems with MMU enabled:
- Accessing an **unmapped address** causes a **Translation Fault** (Data Abort), crashing the shell
- Accessing an address where **no hardware exists** causes a **Synchronous External Abort** (bus error), also crashing the shell

This makes debugging hardware registers difficult when you're not sure if an address is valid.

## Solution

MmioUtil solves this by:

1. Querying the GCD (Global Coherency Domain) memory map
2. If the address is unmapped (`EfiGcdMemoryTypeNonExistent`), adding it as MMIO space
3. Setting uncached (UC) memory attributes for device memory access
4. Installing an exception handler to catch any Data Aborts during access
5. Reporting errors gracefully instead of crashing

## Usage

```
MmioUtil <Address> [Value] [-w 1|2|4|8] [-r <count>] [-v]
```

### Options

| Option | Description |
|--------|-------------|
| `Address` | Physical address in hexadecimal (e.g., `0x12340000`) |
| `Value` | Value to write (if omitted, performs read only) |
| `-w <width>` | Access width in bytes: 1, 2, 4, or 8 (default: 4) |
| `-r <count>` | Number of consecutive reads (default: 1) |
| `-v` | Verbose mode - shows mapping information |
| `-?` | Display help |

### Examples

```bash
# Read a 32-bit register
Shell> MmioUtil 0x12340000
0x0000000012340000: 0x00000001

# Write 0xDEADBEEF to a 32-bit register
Shell> MmioUtil 0x12340000 0xDEADBEEF
0x0000000012340000: 0xDEADBEEF

# Read 16 consecutive 32-bit values (dump 64 bytes)
Shell> MmioUtil 0x12340000 -r 16
0x0000000012340000: 0x00000001
0x0000000012340004: 0x00000002
...

# Read an 8-bit register with verbose output
Shell> MmioUtil 0x12340000 -w 1 -v
[INFO] Mapping 0x12340000 - 0x12340FFF as UC (Uncached Device Memory)
0x0000000012340000: 0x01

# Write a 64-bit value
Shell> MmioUtil 0x12340000 0x123456789ABCDEF0 -w 8
0x0000000012340000: 0x123456789ABCDEF0
```

## How It Works

### Exception Handling

MmioUtil installs a custom exception handler before each MMIO access. If any Data Abort occurs (translation fault, permission fault, external abort, etc.):

1. The handler catches the exception
2. Records the fault address and ESR (Exception Syndrome Register)
3. Advances the program counter past the faulting instruction
4. Returns control to MmioUtil which reports the error gracefully with a descriptive message

**Handled Fault Types:**
| Fault Type | Description |
|------------|-------------|
| Translation fault | Address not mapped in page tables |
| Access flag fault | Page table access flag not set |
| Permission fault | Insufficient permissions |
| Synchronous External Abort | Bus error - device not responding |
| External abort on PTW | Bus error during page table walk |
| Alignment fault | Misaligned access |

**Example output:**
```
Shell> MmioUtil 0xDEAD0000
MmioUtil: Synchronous external abort (bus error) at 0xDEAD0000 (ESR=0x96000010)

Shell> MmioUtil 0x1234
MmioUtil: Translation fault (address not mapped) at 0x1234 (ESR=0x96000006)
```

### Memory Mapping Flow

```
┌─────────────────────────────────────────────────────────────┐
│                    MmioUtil Access Flow                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. User provides address                                   │
│                    ↓                                        │
│  2. Query GCD: gDS->GetMemorySpaceDescriptor()              │
│                    ↓                                        │
│  ┌─────────────────┴─────────────────┐                      │
│  │ GcdMemoryType == NonExistent?     │                      │
│  └─────────────────┬─────────────────┘                      │
│         YES ↓               ↓ NO                            │
│  ┌──────────────────┐  ┌─────────────────┐                  │
│  │ AddMemorySpace() │  │ Already mapped  │                  │
│  │ (MMIO, UC attrs) │  │                 │                  │
│  └────────┬─────────┘  └────────┬────────┘                  │
│           ↓                     ↓                           │
│  ┌──────────────────┐           │                           │
│  │ SetMemorySpace   │           │                           │
│  │ Attributes(UC)   │           │                           │
│  └────────┬─────────┘           │                           │
│           └──────────┬──────────┘                           │
│                      ↓                                      │
│  ┌───────────────────────────────────────────┐              │
│  │ Install exception handler                 │              │
│  │ Perform MMIO access using MmioRead/Write  │              │
│  │ If exception → catch, record, report      │              │
│  │ Uninstall exception handler               │              │
│  └───────────────────────────────────────────┘              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Key Implementation Details

1. **4KB Page Alignment**: ARM MMU operates on 4KB page granularity. Addresses are automatically aligned to page boundaries for mapping.

2. **Uncached Memory**: Mapped regions use `EFI_MEMORY_UC` (Uncached) attributes, which is required for device MMIO:
   - No caching (essential for hardware registers)
   - Strongly-ordered memory access
   - Proper memory barriers

3. **Persistent Mappings**: Once a region is mapped, it remains mapped until reboot. This is intentional:
   - GCD doesn't support clean removal of memory spaces
   - Beneficial for iterative debugging (no re-mapping overhead)
   - Matches how platform drivers handle MMIO regions

4. **Access Width Validation**: The application validates that the address is aligned to the requested access width (e.g., 4-byte aligned for 32-bit access).

5. **Exception Handler**: Uses `EFI_CPU_ARCH_PROTOCOL` to register a custom handler for synchronous exceptions. The handler checks ESR (Exception Syndrome Register) to identify the fault type.

## Implementation Files

| File | Description |
|------|-------------|
| `MmioUtil.c` | Main application source code |
| `MmioUtil.inf` | EDK2 module definition file |
| `MmioUtil.uni` | Module description strings |
| `MmioUtilExtra.uni` | Extra module properties |
| `MmioUtilStrings.uni` | Application strings (help, errors) |

## Build Information

MmioUtil is built as part of the NVIDIA UEFI firmware for non-RELEASE builds.

### Dependencies

- `MdePkg` - Core UEFI definitions
- `MdeModulePkg` - DXE services
- `ShellPkg` - Shell library support

### Key Libraries Used

- `IoLib` - `MmioRead*`/`MmioWrite*` functions
- `DxeServicesTableLib` - Access to `gDS` (DXE Services Table)
- `ShellLib` - Command line parsing

### Key Protocols Used

- `EFI_CPU_ARCH_PROTOCOL` - For registering exception handlers

## Comparison with Shell mm Command

| Feature | Shell `mm` | MmioUtil |
|---------|-----------|----------|
| Unmapped address handling | Crashes | Auto-maps |
| Bus error handling | Crashes | Reports gracefully |
| MMIO flag required | Yes (`-MMIO`) | No (auto-detected) |
| Range dump | No | Yes (`-r` option) |
| Verbose mode | No | Yes (`-v` option) |
| PCI config space | Yes | No (MMIO only) |

## Limitations

1. **MMIO Only**: Does not support PCI configuration space access (use shell `mm -PCI` for that)
2. **No Cleanup**: Mappings persist until reboot
3. **Page Granularity**: Cannot map less than 4KB at a time
4. **DXE Phase Only**: Requires DXE services (not available in PEI/SEC)
5. **AArch64 Only**: Exception handling is ARM64-specific

## Troubleshooting

### "Failed to add memory space" Error

This can occur if:
- The region overlaps with an existing mapping with different attributes
- System is out of GCD map entries (rare)

### "Address not aligned" Error

Ensure the address is aligned to the access width:
- 1-byte access: any address
- 2-byte access: even addresses (0x...0, 0x...2, etc.)
- 4-byte access: 4-byte aligned (0x...0, 0x...4, 0x...8, 0x...C)
- 8-byte access: 8-byte aligned (0x...0, 0x...8)

### Bus Error Messages

If you see a bus error, check:
- Is the hardware powered on?
- Are clocks enabled to the peripheral?
- Is the address correct (check TRM/datasheet)?
- Is the region firewall-protected?

## License

SPDX-License-Identifier: BSD-2-Clause-Patent

Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

