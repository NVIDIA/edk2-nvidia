/** @file

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __L4T_PRE_ISO_INSTALLER_H_
#define __L4T_PRE_ISO_INSTALLER_H_

#include <Uefi.h>
#include <Protocol/L4TLauncherSupportProtocol.h>

#define CAPSULE_SOURCE_DIR    L"EFI\\"
#define CAPSULE_DEST_DIR      L"EFI\\UpdateCapsule\\"
#define CAPSULE_DEFAULT_NAME  L"TEGRA_BL_3701_agx.Cap"

#define CAPSULE_3701_AGX      L"TEGRA_BL_3701_agx.Cap"
#define CAPSULE_3701_AGX_IND  L"TEGRA_BL_3701_agx_ind.Cap"
#define CAPSULE_3701_000      L"TEGRA_BL_3701_000.Cap"
#define CAPSULE_3767          L"TEGRA_BL_3767.Cap"
#define CAPSULE_3767_SUPER    L"TEGRA_BL_3767_super.Cap"
#define CAPSULE_3834_AGX      L"TEGRA_BL_3834_agx.Cap"

#define MAX_CAPSULE_PATH_CHARS  128
#define MAX_CAPSULE_PATH_SIZE   (MAX_CAPSULE_PATH_CHARS * sizeof (CHAR16))

#define VERSION_FILE_PATH  L"EFI\\version"
#define ISO_ID_FILE_PATH   L"EFI\\ISO_ID"
#define LOG_FILE_PATH      L"EFI\\PreIsoInstaller.log"
#define LOG_FILE_MAX_SIZE  (10 * 1024 * 1024)

#define SHIM_PATH  L"EFI\\BOOT\\shimaa64.efi"

#define FILE_COPY_BUFFER_SIZE   0x10000
#define MAX_READ_FILE_SIZE      0x100000
#define SHA256_DIGEST_SIZE      32
#define SHA256_HEX_STRING_SIZE  (SHA256_DIGEST_SIZE * 2)

#define CAPSULE_CONFIRM_TIMEOUT_SEC  30

#define ISO9660_PVD_OFFSET     32768    // Sector 16 in 2048-byte sectors
#define ISO9660_VOLDESC_MAGIC  "CD001"
#define ISO9660_MAGIC_LEN      5

#define TEGRA_PLATFORM_SPEC_VARIABLE_NAME         L"TegraPlatformSpec"
#define TEGRA_PLATFORM_COMPAT_SPEC_VARIABLE_NAME  L"TegraPlatformCompatSpec"
#define PREISO_CAPSULE_STAGED_VARIABLE_NAME       L"PreIsoCapsuleStaged"

#define MAX_SPEC_STRING_LEN  256

#define BOARD_ID_AGX_ORIN   3701
#define BOARD_ID_ORIN_NANO  3767
#define BOARD_ID_AGX_THOR   3834

#define COMPAT_FAB_000  "000"
#define COMPAT_FAB_300  "300"
#define COMPAT_FAB_401  "401"

#define BOARD_NAME_AGX_ORIN_DEVKIT             "jetson-agx-orin-devkit"
#define BOARD_NAME_AGX_ORIN_DEVKIT_INDUSTRIAL  "jetson-agx-orin-devkit-industrial"
#define BOARD_NAME_ORIN_NANO_DEVKIT            "jetson-orin-nano-devkit"
#define BOARD_NAME_ORIN_NANO_DEVKIT_SUPER      "jetson-orin-nano-devkit-super"
#define BOARD_NAME_AGX_THOR_DEVKIT             "jetson-agx-thor-devkit"
#define BOARD_NAME_AGX_THOR_T4000              "jetson-agx-thor-t4000"

/**
  Check if the boot media contains an ISO9660 filesystem by reading the
  Primary Volume Descriptor (PVD) at sector 16 and verifying the "CD001"
  magic signature.  Finds the parent whole-disk of DeviceHandle, then
  checks partition 1 (or falls back to the parent disk).

  @param[in]  DeviceHandle   Device handle for a partition on the boot media.

  @retval TRUE   Boot media has an ISO9660 filesystem.
  @retval FALSE  Not ISO9660, or detection failed.

**/
BOOLEAN
EFIAPI
IsIso9660BootMedia (
  IN EFI_HANDLE  DeviceHandle
  );

/**
  Check if EFI\ISO_ID file exists and its content matches the SHA256
  hash of the EFI\version file content concatenated with "L4T".

  @param[in]  DeviceHandle   Device handle for ESP partition.

  @retval TRUE   ISO_ID file matches the version file SHA256.
  @retval FALSE  Files missing, hash protocol unavailable, or hash mismatch.

**/
BOOLEAN
EFIAPI
IsIsoIdFileValid (
  IN EFI_HANDLE  DeviceHandle
  );

/**
  Execute PreIsoInstaller logic.

  Checks firmware versions, determines if capsule update is needed,
  and performs the update if required. If update is performed, this
  function triggers a warm reset and does not return.

  @param[in]  ImageHandle    The image handle of this application.
  @param[in]  DeviceHandle   Device handle for ESP.
  @param[in]  BootChain      Current boot chain.

  @retval EFI_SUCCESS        No update needed, continue with normal boot.
  @retval EFI_NOT_READY      Update not needed, continue with normal boot.
  @retval Other              Error occurred during PreIsoInstaller execution.

**/
EFI_STATUS
EFIAPI
RunPreIsoInstaller (
  IN EFI_HANDLE  ImageHandle,
  IN EFI_HANDLE  DeviceHandle,
  IN UINT32      BootChain
  );

/**
  Load and start shim bootloader from ESP.

  This function loads EFI\BOOT\shimaa64.efi and starts it.
  If successful, this function does not return.

  @param[in]  ImageHandle    The image handle of the calling application.
  @param[in]  DeviceHandle   Device handle for ESP.

  @retval EFI_NOT_FOUND      Shim file not found.
  @retval Other              Error occurred loading or starting shim.

**/
EFI_STATUS
EFIAPI
LoadAndStartShim (
  IN EFI_HANDLE  ImageHandle,
  IN EFI_HANDLE  DeviceHandle
  );

/**
  Detect ISO installation medium and run the full ISO boot path.

  Checks whether the boot media is an ISO installation medium.  If so,
  clears the rootfs status register, runs the PreIsoInstaller capsule
  update flow, and loads the shim bootloader.

  @param[in]  ImageHandle    The image handle of this application.
  @param[in]  DeviceHandle   Device handle for ESP.
  @param[in]  BootChain      Current boot chain.

  @retval EFI_SUCCESS        ISO medium handled (or not an ISO medium).
  @retval EFI_ABORTED        Capsule boot loop detected — caller must halt.
  @retval Other              Fatal error from shim load.

**/
EFI_STATUS
EFIAPI
HandleIsoBootMedia (
  IN  EFI_HANDLE  ImageHandle,
  IN  EFI_HANDLE  DeviceHandle,
  IN  UINT32      BootChain
  );

/**
  Initialize PreIsoInstaller log file on the ESP.

  Opens (or creates) EFI\PreIsoInstaller.log in append mode and writes
  a timestamped header.  Subsequent PreIsoLogPrint() calls write to this
  file as well as to the console.

  @param[in]  DeviceHandle   Device handle for the ESP.

**/
VOID
EFIAPI
PreIsoLogInit (
  IN EFI_HANDLE  DeviceHandle
  );

/**
  Write a formatted message to both the console (ErrorPrint) and the
  PreIsoInstaller log file.  If the log file has not been initialized,
  writes to the console only.

  @param[in]  Fmt   Printf-style wide format string.
  @param[in]  ...   Variable arguments.

**/
VOID
EFIAPI
PreIsoLogPrint (
  IN CONST CHAR16  *Fmt,
  ...
  );

/**
  Close the PreIsoInstaller log file.

**/
VOID
EFIAPI
PreIsoLogClose (
  VOID
  );

#endif /* __L4T_PRE_ISO_INSTALLER_H_ */
