/** @file
  BootConfig Protocol Library

  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/NctLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/AndroidBcbLib.h>
#include <Library/BootConfigProtocolLib.h>
#include <Protocol/BootConfigUpdateProtocol.h>
#include <Protocol/Eeprom.h>

#define BOOTCONFIG_DUMMY_SERIALNO    "DummySN"
#define BOOTCONFIG_DEFAULT_SERIALNO  "0123456789ABCDEF"
#define MAX_NCT_SN_LEN               30

#define MAX_SLOT_SUFFIX_LEN          3
#define MAX_BOOT_CHAIN_INFO_MAPPING  2
CHAR8  *SlotSuffixNameId[MAX_BOOT_CHAIN_INFO_MAPPING] = {
  "_a",
  "_b",
};
CHAR8  AndroidbootDtboIdx[MAX_ANDROID_BOOT_DTBO_IDX] = "";

//
// Standalone handle on which the bootconfig update protocol instance
// is installed. Using a private handle (instead of gImageHandle) makes
// the protocol globally locatable across UEFI images, so that callers
// in different modules (e.g. AndroidFastbootApp and AndroidBootDxe)
// share the same in-memory accumulator (NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL
// .BootConfigs). The handle and its protocol survive image unload
// because the pool memory is EfiBootServicesData, valid until
// ExitBootServices.
//
STATIC EFI_HANDLE  mBootConfigHandle = NULL;

/**
 * Appends androidboot.newArgs=newValue to the bootconfig string.
 * Note: This function allocates space for the bootconfig string if needed.
 *
 * @param[in] This A pointer to the NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL.
 * @param[in] NewArgs A pointer to the new bootconfig string argument.
 * @param[in] NewValue A pointer to the new value string for the argument.
 *
 * @retval EFI_SUCCESS if the boot configuration was updated successfully.
 * @retval EFI_INVALID_PARAMETER if the This parameter is NULL.
 * @retval EFI_OUT_OF_RESOURCES if memory allocation failed.
 *
 *
 */
STATIC
EFI_STATUS
EFIAPI
UpdateBootConfig (
  IN NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *This,
  IN CONST CHAR8                        *NewArgs,
  IN CONST CHAR8                        *NewValue
  )
{
  CONST UINTN  BootConfigMaxLength = sizeof (CHAR8) * BOOTCONFIG_MAX_LEN;

  if (This == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Called with a NULL This pointer\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (This->BootConfigs == NULL) {
    This->BootConfigs = AllocateZeroPool (BootConfigMaxLength);
    if (This->BootConfigs == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  AsciiSPrint (
    This->BootConfigs,
    BootConfigMaxLength,
    "%aandroidboot.%a=%a\n",
    This->BootConfigs,
    NewArgs,
    NewValue
    );

  return EFI_SUCCESS;
}

/**
 * Installs the BootConfigProtocol protocol.
 *
 * @param BootConfigProtocol A pointer to the NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL.
 *
 * @retval EFI_SUCCESS if the installation is successful.
 * @retval EFI_OUT_OF_RESOURCES if memory allocation fails.
 * @retval Other The operation failed with an error status.
 */
STATIC
EFI_STATUS
EFIAPI
BootConfigProtocolInit (
  OUT NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  **BootConfigProtocol OPTIONAL
  )
{
  EFI_STATUS                         Status;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigUpdate = NULL;

  BootConfigUpdate = (NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL *)AllocateZeroPool (sizeof (NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL));
  if (BootConfigUpdate == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  BootConfigUpdate->UpdateBootConfigs = UpdateBootConfig;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mBootConfigHandle,
                  &gNVIDIABootConfigUpdateProtocol,
                  BootConfigUpdate,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to install bootconfig update protocol\n", __FUNCTION__, Status));
    return Status;
  }

  if (BootConfigProtocol != NULL) {
    *BootConfigProtocol = BootConfigUpdate;
  }

  return EFI_SUCCESS;
}

/**
 * Retrieves the boot configuration update protocol, creating it if it's missing.
 * Code should use this API rather than getting the protocol directly to ensure
 * that only one common copy of the protocol is created.
 *
 * @param BootConfigProtocol A pointer to where to put the protocol pointer.
 *
 * @return The status of the operation.
 *
 * @retval EFI_SUCCESS The protocol was retrieved successfully.
 * @retval EFI_INVALID_PARAMETER The input parameter is NULL.
 * @retval Other The operation failed with an error status.
 */
EFI_STATUS
EFIAPI
GetBootConfigUpdateProtocol (
  OUT NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  **BootConfigProtocol
  )
{
  EFI_STATUS  Status;

  if (BootConfigProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // LocateProtocol walks the system protocol database, so it sees the
  // single shared instance regardless of which image originally
  // installed it (see mBootConfigHandle).
  //
  Status = gBS->LocateProtocol (&gNVIDIABootConfigUpdateProtocol, NULL, (VOID **)BootConfigProtocol);
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  if (Status == EFI_NOT_FOUND) {
    return BootConfigProtocolInit (BootConfigProtocol);
  } else {
    return Status;
  }
}

/**
 * Adds a serial number to the boot configuration.
 *
 * @param NewValue       The new serial number to add. If NULL, the function will
 *                       retrieve the serial number from the CVM protocol.
 *
 * @param OutStrSn       Pass final serial number buffer to caller, if not NULL.
 * @param OutStrSnLen    Buffer size of OutStrSn.
 *
 * @retval EFI_SUCCESS   The serial number was added successfully.
 * @retval Other         The operation failed with an error status.
 */
EFI_STATUS
EFIAPI
BootConfigAddSerialNumber (
  CONST CHAR8  *NewValue OPTIONAL,
  CHAR8        *OutStrSn OPTIONAL,
  UINT32       OutStrSnLen
  )
{
  EFI_STATUS                         Status;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigProtocol;
  TEGRA_EEPROM_BOARD_INFO            *CvmBoardInfo = NULL;
  CHAR8                              NctSn[MAX_NCT_SN_LEN];

  Status = GetBootConfigUpdateProtocol (&BootConfigProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get bootconfig update protocol\n", __FUNCTION__, Status));
    return Status;
  }

  if (NewValue == NULL) {
    Status = gBS->LocateProtocol (&gNVIDIACvmEepromProtocolGuid, NULL, (VOID **)&CvmBoardInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get CVM protocol\n", __FUNCTION__, Status));
      return Status;
    }

    NewValue = CvmBoardInfo->SerialNumber;
  }

  if (AsciiStrCmp (NewValue, BOOTCONFIG_DUMMY_SERIALNO) == 0) {
    Status = NctGetSerialNumber (NctSn, MAX_NCT_SN_LEN);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get NCT Serial Number\n", __FUNCTION__, Status));
      NewValue = BOOTCONFIG_DEFAULT_SERIALNO;
    } else {
      NewValue = NctSn;
    }
  }

  Status = BootConfigProtocol->UpdateBootConfigs (BootConfigProtocol, "serialno", NewValue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add Serial Number to bootconfig\n", __FUNCTION__, Status));
    return Status;
  }

  if (OutStrSn != NULL) {
    AsciiStrCpyS (OutStrSn, OutStrSnLen, NewValue);
  }

  return EFI_SUCCESS;
}

/**
 * Adds a slot_suffix to the boot configuration.
 *
 * @retval EFI_SUCCESS The slot_suffix was added successfully.
 * @retval Other The operation failed with an error status.
 */
EFI_STATUS
EFIAPI
BootConfigAddSlotSuffix (
  VOID
  )
{
  EFI_STATUS                         Status;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigProtocol;
  UINT32                             SlotIndex                       = 0;
  CHAR8                              SlotSuffix[MAX_SLOT_SUFFIX_LEN] = { 0 };

  Status = GetBootConfigUpdateProtocol (&BootConfigProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get bootconfig update protocol\n", __FUNCTION__, Status));
    return Status;
  }

  SlotIndex = BcbGetActiveFwBootChain ();

  AsciiStrCpyS (SlotSuffix, MAX_SLOT_SUFFIX_LEN, SlotSuffixNameId[SlotIndex]);
  Status = BootConfigProtocol->UpdateBootConfigs (BootConfigProtocol, "slot_suffix", SlotSuffix);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add slot_suffix to bootconfig\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
 * Adds androidboot.in_OTA to the boot configuration.
 *
 * @retval EFI_SUCCESS The androidboot.in_OTA was added successfully.
 * @retval Other The operation failed with an error status.
 */
STATIC
EFI_STATUS
EFIAPI
BootConfigAddIfInOta (
  VOID
  )
{
  EFI_STATUS                         Status;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigProtocol;

  Status = GetBootConfigUpdateProtocol (&BootConfigProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get bootconfig update protocol\n", __FUNCTION__, Status));
    return Status;
  }

  Status = BootConfigProtocol->UpdateBootConfigs (BootConfigProtocol, "in_OTA", BcbIsInOta () == TRUE ? "1" : "0");
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add androidboot.in_OTA to bootconfig\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
 * Adds quiescent boot info to the boot configuration.
 *
 * @retval EFI_SUCCESS The slot_suffix was added successfully.
 * @retval Other The operation failed with an error status.
 */
EFI_STATUS
EFIAPI
BootConfigAddQuiescentBootInfo (
  VOID
  )
{
  EFI_STATUS                         Status;
  MiscCmdType                        MiscCmd;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigProtocol;

  Status = GetCmdFromMiscPartition (NULL, &MiscCmd, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get Bcb Cmd from Misc\n", __FUNCTION__, Status));
    return Status;
  }

  if (MiscCmd != MISC_CMD_TYPE_BOOT_QUIESCENT) {
    return EFI_SUCCESS;
  }

  Status = GetBootConfigUpdateProtocol (&BootConfigProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get bootconfig update protocol\n", __FUNCTION__, Status));
    return Status;
  }

  Status = BootConfigProtocol->UpdateBootConfigs (BootConfigProtocol, "quiescent", "1");
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add quiescent boot to bootconfig\n", __FUNCTION__, Status));
  }

  return Status;
}

EFI_STATUS
EFIAPI
BootConfigSetDtboIdx (
  CONST CHAR8  *NewDtboIdx
  )
{
  return AsciiStrCpyS (AndroidbootDtboIdx, MAX_ANDROID_BOOT_DTBO_IDX, NewDtboIdx);
}

/**
 * Adds dtbo idx to the boot configuration.
 *
 * @param NewValue The new dtbo idx to add.
 *
 * @retval EFI_SUCCESS The serial number was added successfully.
 * @retval Other The operation failed with an error status.
 */
EFI_STATUS
EFIAPI
BootConfigAddDtboIdx (
  VOID
  )
{
  EFI_STATUS                         Status;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigProtocol;

  Status = GetBootConfigUpdateProtocol (&BootConfigProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get bootconfig update protocol\n", __FUNCTION__, Status));
    return Status;
  }

  Status = BootConfigProtocol->UpdateBootConfigs (BootConfigProtocol, "dtbo_idx", AndroidbootDtboIdx);

  return Status;
}

/**
 * Adds boot time boot configuration.
 *
 * @retval EFI_SUCCESS The slot_suffix was added successfully.
 * @retval Other The operation failed with an error status.
 */
EFI_STATUS
EFIAPI
BootConfigPrepareBootTimeArgs (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = BootConfigAddSlotSuffix ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = BootConfigAddIfInOta ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = BootConfigAddQuiescentBootInfo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = BootConfigAddDtboIdx ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Test whether the shared bootconfig accumulator already contains an
  androidboot.<Key>=<ExpectedValue> entry, with proper line-boundary
  matching so e.g. "mode=safe" does not match "mode=safety".

  See the public header for the full contract.
**/
BOOLEAN
EFIAPI
BootConfigHasAndroidbootValue (
  IN CONST CHAR8  *Key,
  IN CONST CHAR8  *ExpectedValue
  )
{
  EFI_STATUS                         Status;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigProtocol;
  CONST CHAR8                        *Accumulator;
  CHAR8                              *Needle;
  UINTN                              KeyLen;
  UINTN                              ExpectedValueLen;
  UINTN                              NeedleCapacity;
  UINTN                              NeedleLen;
  CONST CHAR8                        *Cursor;
  CHAR8                              AfterChar;
  BOOLEAN                            Found;

  if ((Key == NULL) || (ExpectedValue == NULL)) {
    return FALSE;
  }

  Status = GetBootConfigUpdateProtocol (&BootConfigProtocol);
  if (EFI_ERROR (Status) || (BootConfigProtocol->BootConfigs == NULL)) {
    return FALSE;
  }

  Accumulator = BootConfigProtocol->BootConfigs;

  KeyLen           = AsciiStrLen (Key);
  ExpectedValueLen = AsciiStrLen (ExpectedValue);
  NeedleCapacity   = (sizeof ("androidboot.") - 1) + KeyLen + (sizeof ("=") - 1) + ExpectedValueLen + 1;
  Needle           = AllocateZeroPool (NeedleCapacity);
  if (Needle == NULL) {
    return FALSE;
  }

  AsciiSPrint (Needle, NeedleCapacity, "androidboot.%a=%a", Key, ExpectedValue);
  NeedleLen = AsciiStrLen (Needle);
  Found     = FALSE;

  Cursor = Accumulator;
  while ((Cursor = AsciiStrStr (Cursor, Needle)) != NULL) {
    //
    // Accept both LF and CR as the leading/trailing boundary. The
    // accumulator is currently produced via EDK2's PrintLib, whose
    // SPrintMarker translates "\n" in the format string to "\r\n"
    // -- so entries are de facto separated by "\r\n". Be liberal in
    // what we accept here so a future fix of that translation does
    // not require touching this helper again.
    //
    if ((Cursor == Accumulator) ||
        (*(Cursor - 1) == '\n') || (*(Cursor - 1) == '\r'))
    {
      AfterChar = *(Cursor + NeedleLen);
      if ((AfterChar == '\n') || (AfterChar == '\r') || (AfterChar == '\0')) {
        Found = TRUE;
        goto Exit;
      }
    }

    Cursor++;
  }

Exit:
  FreePool (Needle);
  return Found;
}
