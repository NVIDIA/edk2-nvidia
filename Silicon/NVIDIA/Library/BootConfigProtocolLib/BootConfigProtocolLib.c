/** @file
  BootConfig Protocol Library

  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/NctLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/AndroidBcbLib.h>
#include <Library/BootConfigProtocolLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/BootConfigUpdateProtocol.h>
#include <Protocol/Eeprom.h>
#include <Protocol/EFuse.h>
#include "T23xPmcBootReason.h"

#define BOOTCONFIG_DUMMY_SERIALNO    "DummySN"
#define BOOTCONFIG_DEFAULT_SERIALNO  "0123456789ABCDEF"
#define MAX_NCT_SN_LEN               30

#define MAX_SLOT_SUFFIX_LEN          3
#define MAX_BOOT_CHAIN_INFO_MAPPING  2

#define T23X_FUSE_SECURITY_MODE_OFFSET                 0x1a0
#define T23X_FUSE_SECURITY_MODE_BIT                    BIT0
#define T23X_FUSE_BOOT_SECURITY_INFO_OFFSET            0x268
#define T23X_FUSE_BOOT_SECURITY_INFO_AUTH_SCHEME_MASK  0x7

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
    FreePool (BootConfigUpdate);
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

// SHLD2-1095: hardware boot reason (androidboot.bootreason).
//
// Decoded from the PMC reset source, which UEFI reads directly from
// PMC_IMPL_RST_STATUS_0: per bug 6488007 the firewall grants CCPLEX read
// access, and GetPmcBootReason() maps the register page on demand (see below).
// The platform DSC supplies the address via PcdBootReasonPmcRstStatusAddress.
// Field layout (T23x PMC_IMPL_RST_STATUS_0):
//   bits[1:0] = RST_LEVEL  (L0/L1/L2/WARM)
//   bits[7:2] = RST_SOURCE (the 6-bit source enum decoded in T23xPmcBootReason.c)
//
// The PMIC-specific cause is not recoverable on this platform: the max77851
// reset-cause registers (read-clear RLOGIC_INT and latched TOP_STAT2/IO_STAT)
// are already cleared/reloaded from OTP before even MB1 runs (verified on
// target HW), so no PMIC decode is attempted here. PMIC over-temperature is
// instead reported through the orderly-shutdown path (thermal HAL ->
// "shutdown,thermal" via bootstat).

// T23xPmcSourceToReason() lives in T23xPmcBootReason.c (pure decode,
// host-unit-tested). The RST_SOURCE enum is T23x-specific; the PCD that drives
// this read is only set under CONFIG_SOC_T23X, so other SoCs never reach here.

/**
  Read the PMC reset source directly from PMC_IMPL_RST_STATUS_0 and map it to a
  canonical boot reason.

  @retval EFI_SUCCESS    *Reason set from the PMC RST_STATUS register.
  @retval EFI_NOT_FOUND  Feature disabled (PCD 0) or register unreadable.
**/
STATIC
EFI_STATUS
GetPmcBootReason (
  OUT CONST CHAR8  **Reason
  )
{
  UINT32                           Val;
  UINT8                            Source;
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             Addr;
  EFI_PHYSICAL_ADDRESS             PageBase;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Desc;
  BOOLEAN                          Mapped;
  UINT64                           OriginalAttributes;
  EFI_STATUS                       RestoreStatus;

  if (Reason == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // The register address is platform-specific and supplied via PCD by the
  // platform DSC; the default 0 disables the decode (this library is shared).
  if (FixedPcdGet64 (PcdBootReasonPmcRstStatusAddress) == 0) {
    return EFI_NOT_FOUND;
  }

  // PMC_IMPL lives in a register block that UEFI does not map by default (NS
  // code normally avoids PMC core MMIO), so an unguarded read faults with a
  // translation fault. Map the containing page uncached on demand, but only if
  // it is not already in the GCD, and release it again after the read so the
  // default "PMC core not NS-mapped" state is preserved.
  Addr               = (EFI_PHYSICAL_ADDRESS)FixedPcdGet64 (PcdBootReasonPmcRstStatusAddress);
  PageBase           = Addr & ~(EFI_PHYSICAL_ADDRESS)EFI_PAGE_MASK;
  Mapped             = FALSE;
  OriginalAttributes = 0;

  // gDS (DXE Services) is only valid in DXE-phase modules. This library is BASE
  // (it is consumed by BASE/UEFI_APPLICATION modules too), so guard against a
  // NULL gDS rather than dereferencing it in a non-DXE context.
  if (gDS == NULL) {
    return EFI_NOT_FOUND;
  }

  // Query the current GCD state: EFI_NOT_FOUND (or a described-but-nonexistent
  // region) means the page is unmapped and we must add it; a successful lookup
  // of a real region means it is owned elsewhere and we only tweak attributes;
  // any other error is unexpected, so do not guess at the mapping state.
  Status = gDS->GetMemorySpaceDescriptor (PageBase, &Desc);
  if (!EFI_ERROR (Status)) {
    if (Desc.GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
      Mapped = TRUE;
    } else {
      // Page already in the GCD (owned elsewhere): remember its attributes so we
      // can restore them after temporarily forcing UC for the read.
      OriginalAttributes = Desc.Attributes;
    }
  } else if (Status == EFI_NOT_FOUND) {
    Mapped = TRUE;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: GetMemorySpaceDescriptor(0x%llx) failed: %r\n", __FUNCTION__, PageBase, Status));
    return EFI_NOT_FOUND;
  }

  if (Mapped) {
    // Map the PMC page uncached. We only ever read RST_STATUS, so no
    // EFI_MEMORY_RO is requested: RO is not a portable GCD MMIO capability and
    // can be rejected by the CPU arch protocol on some implementations.
    Status = gDS->AddMemorySpace (
                    EfiGcdMemoryTypeMemoryMappedIo,
                    PageBase,
                    SIZE_4KB,
                    EFI_MEMORY_UC
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: AddMemorySpace(0x%llx) failed: %r\n", __FUNCTION__, PageBase, Status));
      return EFI_NOT_FOUND;
    }
  }

  Status = gDS->SetMemorySpaceAttributes (PageBase, SIZE_4KB, EFI_MEMORY_UC);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: SetMemorySpaceAttributes(0x%llx) failed: %r\n", __FUNCTION__, PageBase, Status));
    if (Mapped) {
      gDS->RemoveMemorySpace (PageBase, SIZE_4KB);
    } else {
      // Undo any partial attribute change on a pre-existing mapping.
      RestoreStatus = gDS->SetMemorySpaceAttributes (PageBase, SIZE_4KB, OriginalAttributes);
      if (EFI_ERROR (RestoreStatus)) {
        DEBUG ((DEBUG_ERROR, "%a: restore attributes for 0x%llx failed: %r\n", __FUNCTION__, PageBase, RestoreStatus));
      }
    }

    return EFI_NOT_FOUND;
  }

  Val = MmioRead32 ((UINTN)Addr);

  // Restore the pre-read GCD state: remove a mapping we added, or put back the
  // original attributes of a mapping that already existed - so PMC core is not
  // left NS-accessible and another owner's mapping is not altered. Best-effort,
  // but log a cleanup failure rather than silently leaving the region changed.
  if (Mapped) {
    Status = gDS->RemoveMemorySpace (PageBase, SIZE_4KB);
  } else {
    Status = gDS->SetMemorySpaceAttributes (PageBase, SIZE_4KB, OriginalAttributes);
  }

  if (EFI_ERROR (Status)) {
    // Cleanup failed: the PMC page may be left NS-accessible / an owner's
    // mapping altered. Suppress the boot reason rather than publishing it while
    // the GCD state is indeterminate.
    DEBUG ((DEBUG_ERROR, "%a: GCD cleanup for 0x%llx failed: %r\n", __FUNCTION__, PageBase, Status));
    return EFI_NOT_FOUND;
  }

  // 0xFFFFFFFF means the register is firewalled/unreadable; do not emit a
  // bogus reason. Otherwise RST_STATUS always holds a real value (0 ==
  // SYS_RESET_N/L0 is a legitimate cold-boot status).
  if (Val == MAX_UINT32) {
    return EFI_NOT_FOUND;
  }

  Source  = (UINT8)((Val >> 2) & 0x3FU);
  *Reason = T23xPmcSourceToReason (Source);
  DEBUG ((DEBUG_INFO, "%a: PMC RST_STATUS=0x%08x source=%u -> %a\n", __FUNCTION__, Val, Source, *Reason));
  return EFI_SUCCESS;
}

/**
  Publish androidboot.bootreason into the bootconfig accumulator. Best-effort:
  never blocks Android boot on failure.

  The reason comes from the PMC reset source (PMC_IMPL_RST_STATUS_0).
**/
STATIC
EFI_STATUS
BootConfigAddBootReason (
  VOID
  )
{
  EFI_STATUS                         Status;
  CONST CHAR8                        *Reason     = NULL;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfig = NULL;

  if (EFI_ERROR (GetPmcBootReason (&Reason)) || (Reason == NULL)) {
    DEBUG ((DEBUG_INFO, "%a: no HW boot reason (PMC RST_STATUS unavailable)\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  Status = GetBootConfigUpdateProtocol (&BootConfig);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: no BootConfig protocol: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = BootConfig->UpdateBootConfigs (BootConfig, "bootreason", Reason);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to add bootreason: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: androidboot.bootreason=%a\n", __FUNCTION__, Reason));

  return EFI_SUCCESS;
}

#if FixedPcdGetBool (PcdBootConfigEnableSecurityModeFuse)

STATIC
EFI_STATUS
EFIAPI
BootConfigGetSecurityModeT23x (
  OUT CONST CHAR8  **SecurityValue
  )
{
  EFI_STATUS             Status;
  NVIDIA_EFUSE_PROTOCOL  *EFuse;
  UINT32                 BootSecurityInfo;
  UINT32                 SecurityMode;
  BOOLEAN                AuthSchemeFused;
  BOOLEAN                SecurityModeFused;

  *SecurityValue = NULL;

  Status = gBS->LocateProtocol (&gNVIDIAEFuseProtocolGuid, NULL, (VOID **)&EFuse);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "%a: EFuse protocol unavailable (%r); skipping androidboot.security\n",
      __FUNCTION__,
      Status
      ));
    return EFI_SUCCESS;
  }

  Status = EFuse->ReadReg (EFuse, T23X_FUSE_BOOT_SECURITY_INFO_OFFSET, &BootSecurityInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read FUSE_BOOT_SECURITY_INFO_0 (%r)\n", __FUNCTION__, Status));
    return Status;
  }

  Status = EFuse->ReadReg (EFuse, T23X_FUSE_SECURITY_MODE_OFFSET, &SecurityMode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read FUSE_SECURITY_MODE_0 (%r)\n", __FUNCTION__, Status));
    return Status;
  }

  AuthSchemeFused   = (BOOLEAN)((BootSecurityInfo & T23X_FUSE_BOOT_SECURITY_INFO_AUTH_SCHEME_MASK) != 0);
  SecurityModeFused = (BOOLEAN)((SecurityMode & T23X_FUSE_SECURITY_MODE_BIT) != 0);

  *SecurityValue = (AuthSchemeFused && SecurityModeFused) ? "1" : "0";

  DEBUG ((
    DEBUG_INFO,
    "%a: BOOT_SECURITY_INFO=0x%08x SECURITY_MODE=0x%08x -> androidboot.security=%a\n",
    __FUNCTION__,
    BootSecurityInfo,
    SecurityMode,
    *SecurityValue
    ));

  return EFI_SUCCESS;
}

#endif // FixedPcdGetBool (PcdBootConfigEnableSecurityModeFuse)

STATIC
EFI_STATUS
EFIAPI
BootConfigAddSecurityMode (
  VOID
  )
{
 #if FixedPcdGetBool (PcdBootConfigEnableSecurityModeFuse)
  EFI_STATUS                         Status;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigProtocol;
  CONST CHAR8                        *SecurityValue;

  SecurityValue = NULL;
  switch (TegraGetChipID ()) {
    case T234_CHIP_ID:
      Status = BootConfigGetSecurityModeT23x (&SecurityValue);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      break;
    default:
      return EFI_SUCCESS;
  }

  if (SecurityValue == NULL) {
    return EFI_SUCCESS;
  }

  Status = GetBootConfigUpdateProtocol (&BootConfigProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get bootconfig update protocol\n", __FUNCTION__, Status));
    return Status;
  }

  Status = BootConfigProtocol->UpdateBootConfigs (BootConfigProtocol, "security", SecurityValue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add androidboot.security to bootconfig\n", __FUNCTION__, Status));
  }

  return Status;
 #else
  return EFI_SUCCESS;
 #endif
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

  // SHLD2-1095: publish androidboot.bootreason from the HW reset cause.
  // Best-effort - intentionally not gated on status so it never blocks boot.
  Status = BootConfigAddBootReason ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add androidboot.bootreason\n", __FUNCTION__, Status));
  }

  Status = BootConfigAddSecurityMode ();
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
