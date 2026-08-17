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

#define CAPSULE_3701_AGX             L"TEGRA_BL_3701_agx.Cap"
#define CAPSULE_3701_AGX_SUPER       L"TEGRA_BL_3701_agx_super.Cap"
#define CAPSULE_3701_AGX_IND         L"TEGRA_BL_3701_agx_ind.Cap"
#define CAPSULE_3701_000             L"TEGRA_BL_3701_000.Cap"
#define CAPSULE_3767                 L"TEGRA_BL_3767.Cap"
#define CAPSULE_3767_SUPER           L"TEGRA_BL_3767_super.Cap"
#define CAPSULE_3767_NANOE8GB        L"TEGRA_BL_3767_nanoe8gb.Cap"
#define CAPSULE_3767_NANOE8GB_SUPER  L"TEGRA_BL_3767_nanoe8gb_super.Cap"
#define CAPSULE_3834_AGX             L"TEGRA_BL_3834_agx.Cap"
#define CAPSULE_3834_IGX             L"TEGRA_BL_3834_igx.Cap"

#define MAX_CAPSULE_PATH_CHARS  128
#define MAX_CAPSULE_PATH_SIZE   (MAX_CAPSULE_PATH_CHARS * sizeof (CHAR16))

#define VERSION_FILE_PATH       L"EFI\\version"
#define ISO_ID_FILE_PATH        L"EFI\\ISO_ID"
#define INSTALLER_ID_FILE_PATH  L"EFI\\INSTALLER_ID"
#define LOG_FILE_PATH           L"EFI\\PreIsoInstaller.log"
#define LOG_FILE_MAX_SIZE       (10 * 1024 * 1024)

#define SHIM_PATH  L"EFI\\BOOT\\shimaa64.efi"

#define FILE_COPY_BUFFER_SIZE   0x10000
#define MAX_READ_FILE_SIZE      0x100000
#define SHA256_DIGEST_SIZE      32
#define SHA256_HEX_STRING_SIZE  (SHA256_DIGEST_SIZE * 2)

#define TEGRA_PLATFORM_SPEC_VARIABLE_NAME         L"TegraPlatformSpec"
#define TEGRA_PLATFORM_COMPAT_SPEC_VARIABLE_NAME  L"TegraPlatformCompatSpec"
#define PREISO_CAPSULE_STAGED_VARIABLE_NAME       L"PreIsoCapsuleStaged"

#define MAX_SPEC_STRING_LEN  256

#define BOARD_ID_AGX_ORIN   3701
#define BOARD_ID_ORIN_NANO  3767
#define BOARD_ID_THOR       3834

#define COMPAT_FAB_000  "000"
#define COMPAT_FAB_300  "300"
#define COMPAT_FAB_401  "401"

#define BOARD_NAME_AGX_ORIN_DEVKIT             "jetson-agx-orin-devkit"
#define BOARD_NAME_AGX_ORIN_DEVKIT_SUPER       "jetson-agx-orin-devkit-super"
#define BOARD_NAME_AGX_ORIN_DEVKIT_INDUSTRIAL  "jetson-agx-orin-devkit-industrial"
#define BOARD_NAME_ORIN_NANO_DEVKIT            "jetson-orin-nano-devkit"
#define BOARD_NAME_ORIN_NANO_DEVKIT_SUPER      "jetson-orin-nano-devkit-super"
#define BOARD_NAME_ORIN_NANOE8GB_DEVKIT        "jetson-orin-nanoe8gb-devkit"
#define BOARD_NAME_ORIN_NANOE8GB_DEVKIT_SUPER  "jetson-orin-nanoe8gb-devkit-super"
#define BOARD_NAME_AGX_THOR_DEVKIT             "jetson-agx-thor-devkit"
#define BOARD_NAME_AGX_THOR_T4000              "jetson-agx-thor-t4000"
#define BOARD_NAME_IGX_THOR_DEVKIT             "igx-thor-devkit"
#define BOARD_NAME_IGX_THOR_DEVKIT_MINI        "igx-thor-devkit-mini"

/**
  Check if EFI\ISO_ID or EFI\INSTALLER_ID exists and its content matches
  the SHA256 hash of the EFI\version file content concatenated with "L4T".

  @param[in]  DeviceHandle   Device handle for ESP partition.

  @retval TRUE   An installer ID file matches the version file SHA256.
  @retval FALSE  Files are missing, hash protocol unavailable, or neither ID matches.

**/
BOOLEAN
EFIAPI
IsInstallerIdFileValid (
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

  @retval EFI_SUCCESS        PreIsoInstaller completed; continue the selected boot flow.
  @retval EFI_NOT_READY      Update not needed; continue the selected boot flow.
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
  If the started image returns, this function unloads it and returns its status.

  @param[in]  ImageHandle    The image handle of the calling application.
  @param[in]  DeviceHandle   Device handle for ESP.

  @retval EFI_SUCCESS        Shim returned successfully.
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
  Detect and handle installer boot media.

  Checks whether the boot media contains a valid installer ID file. If so,
  clears the rootfs status register and runs the PreIsoInstaller capsule update
  flow. Afterward, continue boot by loading the shim bootloader from ISO_ID
  media, or proceed with normal L4TLauncher boot for INSTALLER_ID media.

  @param[in]  ImageHandle    The image handle of this application.
  @param[in]  DeviceHandle   Device handle for ESP.

  @retval EFI_SUCCESS        Boot media handled (or not an installation medium).
  @retval EFI_ABORTED        PreIsoInstaller blocked boot; caller must halt.
  @retval Other              Fatal error from shim load for ISO media.

**/
EFI_STATUS
EFIAPI
HandleInstallerBootMedia (
  IN  EFI_HANDLE  ImageHandle,
  IN  EFI_HANDLE  DeviceHandle
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
  Flush cached data to the PreIsoInstaller log file.

**/
VOID
EFIAPI
PreIsoLogFlush (
  VOID
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
