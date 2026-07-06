/** @file
  L4TShim — release-stable boot shim. Reads the firmware boot-chain variable
  and chain-loads the matching per-chain L4TLauncher from the ESP it booted
  from. When that launcher cannot be loaded, returns to BDS, which advances
  to the next boot option.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Protocol/LoadedImage.h>

// Firmware-published boot chain variable (UINT32: 0 = chain A, 1 = chain B)
#define BOOT_FW_VARIABLE_NAME  L"BootChainFwCurrent"

// LoadOptions token forcing a chain, identical to the L4TLauncher parse
#define BOOTCHAIN_OVERRIDE_STRING  L"bootchain="

// Per-chain launcher locations on the ESP
#define CHAIN_A_PATH  L"\\EFI\\BOOT\\a\\l4t_bootaa64.efi"
#define CHAIN_B_PATH  L"\\EFI\\BOOT\\b\\l4t_bootaa64.efi"

// Highest valid chain number
#define MAX_BOOT_CHAIN  1

/**
  Read the current firmware boot chain from BootChainFwCurrent.

  @return  0 for chain A, 1 for chain B. Missing, wrong-size, or out-of-range
           values are treated as chain A.
**/
STATIC
UINT32
GetBootChain (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT32      Value;
  UINTN       Size;

  // Read BootChainFwCurrent from the NVIDIA variable namespace
  Size   = sizeof (Value);
  Status = gRT->GetVariable (BOOT_FW_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &Size, &Value);

  // Accept only an existing, exactly-4-byte, in-range value
  if (!EFI_ERROR (Status) && (Size == sizeof (Value)) && (Value <= MAX_BOOT_CHAIN)) {
    return Value;
  }

  // Anything else selects chain A
  return 0;
}

/**
  Apply a "bootchain=N" override from LoadOptions when present and valid.

  @param[in]     LoadedImage  This image's loaded-image protocol.
  @param[in,out] Chain        Updated to the override value when present and valid.
**/
STATIC
VOID
GetChainOverride (
  IN     EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage,
  IN OUT UINT32                     *Chain
  )
{
  CHAR16        *Options;
  CONST CHAR16  *Option;
  UINTN         OptionChars;
  UINT64        Value;
  EFI_STATUS    Status;

  // No LoadOptions attached to this image
  if ((LoadedImage->LoadOptions == NULL) || (LoadedImage->LoadOptionsSize < sizeof (CHAR16))) {
    return;
  }

  // Copy LoadOptions into a NUL-terminated buffer; the original is not
  // guaranteed to be NUL-terminated
  OptionChars = LoadedImage->LoadOptionsSize / sizeof (CHAR16);
  Options     = AllocateZeroPool ((OptionChars + 1) * sizeof (CHAR16));
  if (Options == NULL) {
    return;
  }

  CopyMem (Options, LoadedImage->LoadOptions, OptionChars * sizeof (CHAR16));

  // Locate the "bootchain=" token in the option string
  Option = StrStr (Options, BOOTCHAIN_OVERRIDE_STRING);
  if (Option != NULL) {
    // Convert the digits following the token; accept only 0 or 1
    Option += StrLen (BOOTCHAIN_OVERRIDE_STRING);
    Status  = StrDecimalToUint64S (Option, NULL, &Value);
    if (!EFI_ERROR (Status) && (Value <= MAX_BOOT_CHAIN)) {
      *Chain = (UINT32)Value;
      DEBUG ((DEBUG_INFO, "L4TSHIM: bootchain override = %u\r\n", *Chain));
    }
  }

  FreePool (Options);
}

/**
  Load (but do not start) the launcher at ChainPath on the given device.

  @param[in]  ParentImageHandle  This image's handle.
  @param[in]  DeviceHandle       Handle of the ESP this image was loaded from.
  @param[in]  ChainPath          ESP path of the per-chain launcher.
  @param[out] ChildHandle        Handle of the loaded launcher image on success.

  @return  Status of the load. EFI_SECURITY_VIOLATION is returned after the
           mandatory UnloadImage of the unverified image.
**/
STATIC
EFI_STATUS
LoadChainImage (
  IN  EFI_HANDLE    ParentImageHandle,
  IN  EFI_HANDLE    DeviceHandle,
  IN  CONST CHAR16  *ChainPath,
  OUT EFI_HANDLE    *ChildHandle
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevPath;

  // Build the full device path: this ESP + the launcher file path
  DevPath = FileDevicePath (DeviceHandle, (CHAR16 *)ChainPath);
  if (DevPath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Load the launcher image; Secure Boot verification happens here
  *ChildHandle = NULL;
  Status       = gBS->LoadImage (FALSE, ParentImageHandle, DevPath, NULL, 0, ChildHandle);
  FreePool (DevPath);

  if (Status == EFI_SECURITY_VIOLATION) {
    // Per UEFI spec the image is loaded but must not be started; unload it
    DEBUG ((DEBUG_ERROR, "L4TSHIM: %s failed verification\r\n", ChainPath));
    gBS->UnloadImage (*ChildHandle);
    *ChildHandle = NULL;
    return EFI_SECURITY_VIOLATION;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "L4TSHIM: LoadImage(%s) failed: %r\r\n", ChainPath, Status));
  }

  return Status;
}

/**
  Shim entry point. Resolves the boot chain, loads the matching per-chain
  launcher from the same ESP, forwards LoadOptions, and starts it. Each chain
  runs only its own launcher. When the launcher cannot be loaded, returns the
  load error to BDS, which advances to the next boot option.

  @param[in] ImageHandle  This image's handle.
  @param[in] SystemTable  The system table.

  @return  Status of the load when the launcher cannot be loaded.
  @return  Status of the started launcher otherwise.
**/
EFI_STATUS
EFIAPI
L4TShimMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_LOADED_IMAGE_PROTOCOL  *ChildImage;
  EFI_HANDLE                 ChildHandle;
  UINT32                     Chain;
  CONST CHAR16               *ChainPath;

  // Get this image's own info: the ESP it was loaded from and its LoadOptions
  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "L4TSHIM: no LoadedImage protocol: %r\r\n", Status));
    return Status;
  }

  // Resolve the chain: BootChainFwCurrent, then any bootchain= override
  Chain = GetBootChain ();
  GetChainOverride (LoadedImage, &Chain);
  DEBUG ((DEBUG_INFO, "L4TSHIM: boot chain = %u (%a)\r\n", Chain, (Chain == 0) ? "A" : "B"));

  // Each chain runs only its own launcher
  ChainPath = (Chain == 0) ? CHAIN_A_PATH : CHAIN_B_PATH;

  Status = LoadChainImage (ImageHandle, LoadedImage->DeviceHandle, ChainPath, &ChildHandle);
  if (EFI_ERROR (Status)) {
    // Return to BDS, which advances to the next boot option
    DEBUG ((DEBUG_ERROR, "L4TSHIM: launcher load failed, returning to BDS\r\n"));
    return Status;
  }

  // Forward this image's LoadOptions to the launcher
  if ((LoadedImage->LoadOptions != NULL) && (LoadedImage->LoadOptionsSize > 0)) {
    Status = gBS->HandleProtocol (ChildHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&ChildImage);
    if (EFI_ERROR (Status)) {
      // A loaded image must expose EFI_LOADED_IMAGE_PROTOCOL; a failure here is
      // a platform error, so abort rather than start the launcher without its
      // arguments.
      DEBUG ((DEBUG_ERROR, "L4TSHIM: no LoadedImage on launcher: %r\r\n", Status));
      gBS->UnloadImage (ChildHandle);
      return Status;
    }

    ChildImage->LoadOptions     = LoadedImage->LoadOptions;
    ChildImage->LoadOptionsSize = LoadedImage->LoadOptionsSize;
  }

  // Start the launcher; on a successful OS boot this does not return
  DEBUG ((DEBUG_INFO, "L4TSHIM: starting %s\r\n", ChainPath));
  Status = gBS->StartImage (ChildHandle, NULL, NULL);

  // StartImage returned, so the launcher did not boot. Unload it before
  // returning to BDS; this frees the launcher and tears down its LoadOptions
  // alias into this image's buffer.
  gBS->UnloadImage (ChildHandle);
  return Status;
}
