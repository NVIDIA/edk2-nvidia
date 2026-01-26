# Redfish HTTP Boot Configuration Driver

## Overview

This DXE driver enables remote HTTP boot configuration through Redfish. It
exposes a single HTTP boot URI attribute via Redfish OEM extensions, allowing
BMC administrators to configure flexible HTTP boot options remotely without
physical access to the system.

## Features

- **HII-based Configuration**: Uses HII (Human Interface Infrastructure) with
  config language mapping for Redfish discovery
- **Flexible URI Format**: Supports three boot modes via `[MAC||][once|]URI` format:
  - **First NIC, persistent**: `http://example.com/boot.efi`
  - **Specific NIC, persistent**: `00:0a:1b:2c:3d:4e||http://example.com/boot.efi`
  - **Specific NIC, one-time**: `00:0a:1b:2c:3d:4e|once|http://example.com/boot.efi`
- **BootNext for Transient Boot**: Uses UEFI BootNext variable for one-time
  boot instead of manipulating boot order
- **Automatic Boot Option Creation**: Creates Boot8C7D in `RouteConfig`
  callback when Redfish pushes configuration
- **Intelligent Cleanup**: Removes HTTP boot option and variable after
  one-time boot execution; persistent boots remain until cleared
- **Timing-Aware**: Handles BdsDxe caching by creating boot options on
  boot N, executing on boot N+1, and cleaning up on boot N+2

## URI Format

The driver supports a flexible URI format with optional components:

```
[MAC_ADDRESS||][once|]URI
```

### Components

- **MAC_ADDRESS** (optional): Target NIC MAC address in colon-separated format
  (e.g., `00:0a:1b:2c:3d:4e`). If omitted, first available NIC is used.
- **once** (optional): One-time boot flag. If present, boot option is
  automatically removed after execution.
- **URI** (required): HTTP or HTTPS URI to boot from.

### Examples

```
# Boot from URI on first NIC (persistent)
http://192.168.1.100/bootfile.efi

# Boot from URI on specific NIC (persistent)
00:0a:1b:2c:3d:4e||http://192.168.1.100/bootfile.efi

# One-time boot from URI on specific NIC
00:0a:1b:2c:3d:4e|once|http://192.168.1.100/bootfile.efi

# IPv6 example with one-time boot
aa:bb:cc:dd:ee:ff|once|http://[2001:db8::1]/bootfile.efi
```

## Architecture

### Boot Flow

```
Boot N (Initial)
    ↓
BMC (Redfish Client)
    ↓ PATCH /redfish/v1/Systems/{1}/Bios/Settings
    ↓ {"Attributes": {"HTTP_BOOT_URI": "00:0a:1b:2c:3d:4e|once|http://..."}}
    ↓
Redfish Service (BiosDxe Feature Driver)
    ↓ Discovers via x-UEFI-redfish-Bios.v1_2_0
    ↓ Calls RedfishPlatformConfigDxe → RouteConfig(<ConfigResp>)
    ↓
RedfishHttpBootConfigDxe (this driver)
    ↓ RouteConfig callback
    ├─→ Parse URI format (MAC, once flag, URI)
    ├─→ Find NIC with matching MAC (or first NIC if no MAC)
    ├─→ Build device path: MAC + IPv4/IPv6 + URI
    ├─→ Create Boot8C7D with HTTP boot device path
    ├─→ Set BootNext = 0x8C7D
    └─→ Write HttpBootUri variable
    ↓ System reboot
    ↓
Boot N+1 (HTTP Boot)
    ↓ Driver entry point
    ├─→ Read HttpBootUri variable
    ├─→ Check BootNext (still 0x8C7D, so don't delete yet)
    └─→ Continue boot
    ↓
BDS Phase
    ↓ BdsDxe reads BootNext (0x8C7D)
    ↓ Boot from Boot8C7D
    └─→ HTTP boot executes, BootNext cleared by firmware
    ↓
Boot N+2 (Cleanup)
    ↓ Driver entry point
    ├─→ Read HttpBootUri variable
    ├─→ Check BootNext (cleared, so cleanup can proceed)
    ├─→ If "once" flag: delete HttpBootUri variable and Boot8C7D
    └─→ If persistent: keep Boot8C7D (already created)
```

### Key Implementation Details

1. **Fixed Boot Number**: Uses Boot8C7D (0x8C7D) derived from
   NVIDIA_HTTP_BOOT_CONFIG_GUID first word (0x8c7d9a1e). This provides
   uniqueness and avoids collisions with other boot options

2. **BootNext Timing**: Boot option is created on boot N, used on boot N+1,
   and cleaned up on boot N+2. This handles BdsDxe caching BootNext early
   in boot.

3. **Cleanup Detection**: Driver checks if BootNext is still set to 0x8C7D
   to distinguish between boot N+1 (before HTTP boot) and boot N+2 (after).

4. **varstore vs efivarstore**: Uses HII varstore (not efivarstore) with
   manual ExtractConfig/RouteConfig implementation for full control over
   variable lifecycle.

## Configuration

### Build-Time Configuration

Enable via Kconfig:

```bash
CONFIG_REDFISH=y
CONFIG_REDFISH_HTTP_BOOT_CONFIG=y
CONFIG_NETWORKING_HTTP=y
```

This option is implied for datacenter builds (`BUILD_DATACENTER`).

### Runtime Configuration

1. **BMC sets Redfish attribute**:
   ```bash
   curl -X PATCH https://bmc/redfish/v1/Systems/1/Bios/Settings \
     -d '{"Attributes": {"HTTP_BOOT_URI": "00:0a:1b:2c:3d:4e|once|http://192.168.1.100/bootfile.efi"}}'
   ```

2. **Reboot system**: UEFI boots and Redfish service pushes configuration
   - RouteConfig creates Boot8C7D and sets BootNext

3. **Second reboot**: System boots from Boot8C7D via BootNext
   - HTTP boot executes and downloads/boots from URI

4. **Cleanup**: On third boot, if "once" flag was set, Boot8C7D and
   HttpBootUri variable are automatically removed

### Clearing Configuration

To clear persistent HTTP boot:

```bash
curl -X PATCH https://bmc/redfish/v1/Systems/1/Bios/Settings \
  -d '{"Attributes": {"HTTP_BOOT_URI": ""}}'
```

Empty string triggers cleanup on next boot.

## Redfish API

### Attribute

The driver exposes a single Redfish attribute:

```
/Systems/{1}/Bios/Attributes/HTTP_BOOT_URI
```

### Example

```json
{
  "@odata.type": "#Bios.v1_2_0.Bios",
  "Id": "BIOS",
  "Name": "BIOS Configuration",
  "Attributes": {
    "HTTP_BOOT_URI": "00:0a:1b:2c:3d:4e|once|http://192.168.1.100/bootfile.efi"
  }
}
```

### Supported URI Schemes

- `http://` - HTTP (unencrypted)
- `https://` - HTTPS (encrypted)

### IPv4 vs IPv6

The driver automatically detects IPv4 or IPv6 from the URI:
- IPv4: `http://192.168.1.100/...`
- IPv6: `http://[2001:db8::1]/...`

## Device Path Structure

The driver constructs HTTP boot device paths as follows:

```
MAC(00:0a:1b:2c:3d:4e) / IPv4(DHCP) / URI(http://192.168.1.100/bootfile.efi)
```

Or for IPv6:

```
MAC(00:0a:1b:2c:3d:4e) / IPv6(DHCP) / URI(http://[2001:db8::1]/bootfile.efi)
```

## Variables

### UEFI Variables

- **HttpBootUri** (Configuration storage):
  - GUID: `gNvidiaHttpBootConfigGuid`
  - Attributes: `EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE`
  - Type: `HTTP_BOOT_URI_STORAGE` structure with URI string
  - Contents: Full URI string in format `[MAC||][once|]URI`
  - Lifecycle:
    - Created by RouteConfig when Redfish pushes configuration
    - Read by ExtractConfig when Redfish queries configuration
    - Deleted on cleanup if "once" flag is set or URI is empty

- **Boot8C7D** (HTTP boot option):
  - GUID: `gEfiGlobalVariableGuid`
  - Attributes: `EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE`
  - Type: `EFI_LOAD_OPTION` with HTTP boot device path
  - Lifecycle:
    - Created by RouteConfig with HTTP boot device path
    - Used by BdsDxe when BootNext points to it
    - Deleted on cleanup if "once" flag is set or URI is empty

- **BootNext** (Transient boot):
  - GUID: `gEfiGlobalVariableGuid`
  - Attributes: `EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE`
  - Type: `UINT16` (boot option number)
  - Value: `0x8C7D` (Boot8C7D)
  - Lifecycle:
    - Set by RouteConfig to 0x8C7D
    - Read by BdsDxe on next boot
    - Automatically cleared by firmware after boot

## HII Integration

### Config Access Protocol

The driver implements `EFI_HII_CONFIG_ACCESS_PROTOCOL`:

- **ExtractConfig**: Reads HttpBootUri variable and returns HII config string
  - Called by Redfish when querying current configuration
  - Uses `BlockToConfig()` from `EFI_HII_CONFIG_ROUTING_PROTOCOL` to convert variable to config string

- **RouteConfig**: Parses HII config string and creates HTTP boot option
  - Called by Redfish when applying new configuration
  - Uses `ConfigToBlock()` from `EFI_HII_CONFIG_ROUTING_PROTOCOL` to parse config string
  - Creates Boot8C7D and sets BootNext on same boot (boot N)

- **Callback**: Not used (returns EFI_UNSUPPORTED)

### VFR Declaration

The driver uses a static string question in VFR:

```vfr
varstore HTTP_BOOT_URI_STORAGE,
  varid = HTTP_BOOT_URI_VARSTORE_ID,
  name  = HttpBootUri,
  guid  = NVIDIA_HTTP_BOOT_CONFIG_GUID;

string varid   = HttpBootUri.HttpBootUri,
       questionid = KEY_HTTP_BOOT_URI,
       prompt = STRING_TOKEN(STR_HTTP_BOOT_URI_PROMPT),
       help   = STRING_TOKEN(STR_HTTP_BOOT_URI_HELP),
       minsize = 0,
       maxsize = HTTP_BOOT_URI_MAX_SIZE,
endstring;
```

### Redfish Mapping

Redfish attribute mapping is defined in `RedfishHttpBootConfigMap.uni`:

```
#langdef x-UEFI-redfish-Bios.v1_2_0 "x-UEFI-redfish-Bios.v1_2_0"
#string STR_HTTP_BOOT_URI_PROMPT #language x-UEFI-redfish-Bios.v1_2_0 "/Bios/Attributes/HTTP_BOOT_URI"
```

This allows RedfishPlatformConfigDxe to discover the attribute and route
configuration changes to this driver.

## Limitations

### Current Implementation

- **Single Boot Option**: Only one HTTP boot URI at a time
- **No Certificate Validation**: HTTPS connections do not validate server certificates
- **IPv6 Detection**: Simple heuristic (checks for `[` in URI)
- **First NIC Fallback**: If no MAC specified, uses first available NIC

### Future Enhancements

1. **Certificate Management**: Support for trusted certificate provisioning
2. **Authentication**: HTTP basic auth or bearer token support
3. **Multiple URIs**: Priority list of URIs to try
4. **Progress Reporting**: Report download progress to BMC
5. **Retry Logic**: Automatic retry on failure
6. **Boot Option Conflicts**: Detect and handle existing Boot8C7D

## Security Considerations

1. **URI Validation**: Driver validates URI format to prevent injection attacks
2. **HTTPS Support**: Encrypted transport for sensitive boot images
3. **Access Control**: Redfish access control limits who can set boot URIs
4. **Cleanup**: Automatic cleanup prevents persistent unauthorized boot options (with "once" flag)
5. **Fixed Boot Number**: Using Boot8C7D prevents conflicts with firmware boot options

## Dependencies

### Required Protocols

- `gEfiHiiConfigAccessProtocolGuid` (produces)
- `gEfiHiiConfigRoutingProtocolGuid` (consumes)
- `gEfiSimpleNetworkProtocolGuid` (consumes)

### Required Libraries

- `UefiBootServicesTableLib`
- `UefiRuntimeServicesTableLib`
- `HiiLib`
- `UefiHiiServicesLib`
- `DevicePathLib`
- `BaseMemoryLib`
- `MemoryAllocationLib`
- `PrintLib`
- `DebugLib`

### Required Components

- `CONFIG_REDFISH=y` - Redfish support
- `CONFIG_NETWORKING_HTTP=y` - HTTP/HTTPS networking stack
- `CONFIG_IPMI_BMC=y` - BMC communication
- `RedfishPlatformConfigDxe` - EDK2 Redfish platform config driver
- `HttpBootDxe` - EDK2 HTTP boot driver

## Files

### Driver Files

- `RedfishHttpBootConfigDxe.c` - Main driver implementation (entry point, HII callbacks)
- `RedfishHttpBootConfigDxe.h` - Main driver header
- `RedfishHttpBootConfigDxe.inf` - Module information
- `RedfishHttpBootConfigUtils.c` - Utility functions implementation
- `RedfishHttpBootConfigUtils.h` - Utility functions header
- `CreateHttpBootOptionImpl.c` - Boot option creation implementation
- `CompareAndSyncBootOptionsImpl.c` - Boot option sync/cleanup implementation
- `RedfishHttpBootConfigVfrDefs.h` - VFR-safe definitions (GUID, structures)
- `RedfishHttpBootConfigFormset.h` - VFR formset constants
- `RedfishHttpBootConfigVfr.vfr` - HII form definition
- `RedfishHttpBootConfigStrings.uni` - Display strings (English)
- `RedfishHttpBootConfigMap.uni` - Redfish attribute mapping
- `README.md` - This file

### Test Files

- `GoogleTest/` - Google Test unit tests
  - `RedfishHttpBootConfigTest.cpp` - ParseHttpBootUri tests
  - `CompareAndSyncBootOptionsTest.cpp` - Sync/cleanup tests
  - `CompareAndSyncBootOptionsStub.c` - Test stubs

## Troubleshooting

### Boot Option Not Created

- Check that HttpBootUri variable was written by RouteConfig
- Verify NIC with specified MAC address exists
- Check debug logs for parsing errors

### HTTP Boot Not Executing

- Verify BootNext was set to 0x8C7D on previous boot
- Check that Boot8C7D exists and has correct device path
- Ensure network is configured and URI is accessible

### Cleanup Not Working

- One-time boot requires "once" flag in URI format
- Cleanup happens on boot N+2, not immediately after HTTP boot
- Check that BootNext was cleared by firmware

### Redfish Attribute Not Visible

- Verify `RedfishHttpBootConfigMap.uni` contains Redfish mapping
- Check that RedfishPlatformConfigDxe is loaded
- Ensure x-UEFI-redfish-Bios.v1_2_0 language is defined

## References

- [EDK2 Redfish Overview](https://github.com/tianocore/edk2/tree/master/RedfishPkg)
- [HII Configuration Access Protocol](https://uefi.org/specs/UEFI/2.10/32_HII_Configuration_Access_Protocol.html)
- [HTTP Boot Specification](https://uefi.org/specs/UEFI/2.10/24_Network_Protocols.html#http-boot)
- [Redfish Bios Schema](https://redfish.dmtf.org/schemas/v1/Bios.v1_2_0.json)

## Maintainer

NVIDIA CORPORATION & AFFILIATES

## License

SPDX-License-Identifier: BSD-2-Clause-Patent
