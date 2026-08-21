# NOR Flash Driver

Feature Name: NOR Flash Driver

PI Phase supported: DXE, Standalone MM

SMM Required: No (DXE variant); Yes (Standalone MM variants)

## References

- JEDEC JESD216 — Serial Flash Discoverable Parameters (SFDP)
- JEDEC JESD216B Annex A — 4-Byte Address Instruction Table (4BI)

## Purpose

This driver exposes NOR flash devices connected over QSPI as
`NVIDIA_NOR_FLASH_PROTOCOL` instances.  The protocol provides
read, write, and erase operations used by the firmware-partition
and FVB (Firmware Volume Block) layers to store UEFI variables,
UEFI capsule payloads, and other persistent data.

## Driver Variants

| Module | Type | Use |
|---|---|---|
| `NorFlashDxe.inf` | `DXE_RUNTIME_DRIVER` | Normal-world UEFI; discovers flash via Device Tree node under the QSPI controller |
| `NorFlashStandaloneMm.inf` | `MM_STANDALONE` | Secure-world StandaloneMM; used for variable storage |
| `NorFlashStandaloneMmBlob.inf` | `MM_STANDALONE` | StandaloneMM variant for TH500 that forces chip-select 0 and installs `gNVIDIANorFlash2ProtocolGuid` |
| `NorFlashStandaloneMmDice.inf` | `MM_STANDALONE` | StandaloneMM variant with DICE-specific initialisation |

All variants share the same core I/O logic in `NorFlashCommon.c`.

## Architecture

```
┌─────────────────────────────────────┐
│  NVIDIA_NOR_FLASH_PROTOCOL consumer │
│  (FwPartitionNorFlashDxe, FvbDxe …) │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│         NorFlashCommon.c            │
│  ReadNorFlashSFDP                   │  ← SFDP parse & address-mode select
│  NorFlashRead / Write / Erase       │  ← I/O with runtime AddrSize
│  NorFlashGetAttributes              │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│      NVIDIA_QSPI_CONTROLLER_PROTOCOL │
└─────────────────────────────────────┘
```

Each driver variant links `NorFlashCommon.c` and adds only its own
initialisation, protocol-install, and (for DXE) driver-binding code.

## SFDP and Address Mode Selection

On startup, the driver reads the flash's Serial Flash Discoverable
Parameters (SFDP) to determine geometry, erase sizes, timing, and
address mode.  No hard-coded flash IDs are required.

### Address mode

The SFDP Basic Flash Parameter table DWORD 1 contains an `AddressBytes`
field (bits 17:16) defined by JEDEC JESD216:

| `AddressBytes` | Meaning | Driver behaviour |
|---|---|---|
| `0b00` | 3-byte addressing only | Use 3-byte opcodes; skip 4BI table |
| `0b01` | 3-byte or 4-byte | Use 3-byte if density ≤ 16 MB, otherwise 4-byte |
| `0b10` | 4-byte addressing only | Use 4-byte opcodes; 4BI table required |

The threshold of 16 MB (0xFFFFFF) is the maximum address reachable
with a 24-bit address; any larger flash requires 4-byte addressing.

The resolved address size is stored in
`NOR_FLASH_PRIVATE_ATTRIBUTES.AddrSize` at SFDP parse time and used
by all subsequent I/O operations.

### 4-Byte Instruction (4BI) table

When `AddrSize == 4`, the driver also reads the SFDP 4BI parameter
table (ID `0xFF84`, JESD216 Annex A) to discover the specific 4-byte
opcodes the flash supports for read, write, and each erase type.
Initialization fails if this table is absent or if the required
opcodes are not advertised.

When `AddrSize == 3`, the 4BI table is skipped entirely; standard
3-byte opcodes (`0x03` slow read, `0x0B` fast read, `0x02` page
program) are used, and erase commands are taken from the Basic Flash
Parameter table directly.

### Opcode summary

| Operation | 3-byte opcode | 4-byte opcode |
|---|---|---|
| Read (slow) | `0x03` | `0x13` |
| Read (fast) | `0x0B` | `0x0C` |
| Page Program | `0x02` | `0x12` |
| Erase | From BFP `EraseType[n].Command` | From 4BI `EraseInstruction[n]` |

## Fast Read

Fast read behaviour differs by address mode:

**3-byte mode** — fast read uses opcode `0x0B` and is enabled when:
- The QSPI clock speed exceeds `NOR_FAST_CMD_THRESH_FREQ` (100 MHz), **and**
- The platform is silicon (not pre-silicon emulation).

The 4BI table is not consulted in this path; `0x0B` is universally
supported by SPI flash devices that implement 3-byte addressing.

**4-byte mode** — fast read uses opcode `0x0C` and is enabled when the
same clock and platform conditions hold, **and** the flash's 4BI table
advertises `ReadCmd0C` support.  If `ReadCmd0C` is absent from the 4BI
table, `FastReadSupport` is forced off and `0x13` (slow read) is used.

For Standalone MM, `PcdSecureQspiUseFastRead` is the primary control,
but fast read can still be disabled in 4-byte mode when the 4BI table
does not advertise `ReadCmd0C`.

## Erase Granularity

The driver supports hybrid-sector flash layouts (e.g. a 4 KB parameter
region at the bottom followed by uniform 64 KB sectors).  Block size and
erase commands for both the uniform and hybrid regions are discovered from
the SFDP Sector Map parameter table (ID `0xFF81`) when 4 KB uniform erase
is not advertised in the Basic table.

## NVIDIA_NOR_FLASH_PROTOCOL

```c
struct _NVIDIA_NOR_FLASH_PROTOCOL {
  EFI_FVB_ATTRIBUTES_2     FvbAttributes;
  NOR_FLASH_GET_ATTRIBUTES GetAttributes;  // density, block size, timing
  NOR_FLASH_READ           Read;           // byte-addressed read
  NOR_FLASH_WRITE          Write;          // byte-addressed write (page-split internally)
  NOR_FLASH_ERASE          Erase;          // LBA-addressed uniform erase
};
```

The DXE variant additionally installs `EFI_BLOCK_IO_PROTOCOL` and
`EFI_ERASE_BLOCK_PROTOCOL` when `PcdTegraNorBlockProtocols` is set.

## Adding Support for a New Flash Device

No flash-specific code is required.  The driver is fully self-configuring
via SFDP.  A new device is supported automatically provided it:

1. Implements JEDEC JESD216 SFDP.
2. Advertises its address mode via the `AddressBytes` field.
3. If using 4-byte addressing, implements the 4BI parameter table with
   the required read (`0x0C` or `0x13`), write (`0x12`), and erase
   opcodes.

For devices with vendor-specific write-protect or Advanced Sector
Protection (ASP), see `MacronixAsp.c` which is called from the
Standalone MM DICE variant.
