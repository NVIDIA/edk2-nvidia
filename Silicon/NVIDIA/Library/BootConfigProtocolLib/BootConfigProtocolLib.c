/** @file
  BootConfig Protocol Library

  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/NctLib.h>
#include <Library/BootChainInfoLib.h>
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
                  &gImageHandle,
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

  Status = gBS->HandleProtocol (gImageHandle, &gNVIDIABootConfigUpdateProtocol, (VOID **)BootConfigProtocol);
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  if (Status == EFI_UNSUPPORTED) {
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

  SlotIndex = GetBootChainForGpt ();

  AsciiStrCpyS (SlotSuffix, MAX_SLOT_SUFFIX_LEN, SlotSuffixNameId[SlotIndex]);
  Status = BootConfigProtocol->UpdateBootConfigs (BootConfigProtocol, "slot_suffix", SlotSuffix);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add slot_suffix to bootconfig\n", __FUNCTION__, Status));
  }

  return Status;
}
