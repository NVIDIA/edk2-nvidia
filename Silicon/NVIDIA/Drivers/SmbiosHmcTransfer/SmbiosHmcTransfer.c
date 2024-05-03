/** @file

  A driver that sends SMBIOS tables to UEFI variables

  SPDX-FileCopyrightText: copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Guid/SmBios.h>

#include <IndustryStandard/SmBios.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/SmbiosStringTableLib.h>
#include <libfdt.h>

#include <Protocol/Smbios.h>

#include <NVIDIAStatusCodes.h>
#include <OemStatusCodes.h>

#define VAR_SMBIOS_TRANS_NEEDED  L"SmbiosTransNeeded"
#define VAR_PLATFORM_TYPE        L"PlatformType"
#define VAR_POST_SIGNAL_SENT     L"PostSignalSent"
#define VAR_HMC_SMBIOS_BLOB      L"HmcSmbios"

UINT8  *HmcSmbiosTypes;
UINT8  HmcSmbiosTypeCount;

/**
  Check if HMC SMBIOS supported by DTB overlay

**/
BOOLEAN
IsHmcSupport (
  )
{
  EFI_STATUS   Status;
  UINT8        Index;
  VOID         *DtbBase;
  UINTN        DtbSize;
  INT32        NodeOffset;
  CHAR8        SmbiosNodeStr[] = "/firmware/smbios/hmc-smbios";
  CONST INT32  *Property;
  INT32        Length;

  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to load device tree.\n", __FUNCTION__));
    return FALSE;
  }

  NodeOffset = fdt_path_offset (DtbBase, SmbiosNodeStr);
  if (NodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to find SMBIOS overlay\n", __FUNCTION__));
    return FALSE;
  }

  Property = fdt_getprop (DtbBase, NodeOffset, "send-smbios-tables", &Length);
  if ((Property == NULL) || (Length == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: Do not support HMC SMBIOS variables\n", __FUNCTION__));
    return FALSE;
  }

  HmcSmbiosTypeCount = (UINT8)(Length / sizeof (INT32));
  HmcSmbiosTypes     = AllocateZeroPool (HmcSmbiosTypeCount);
  if (HmcSmbiosTypes == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Allocate memory failed\n", __FUNCTION__));
    return FALSE;
  }

  for (Index = 0; Index < HmcSmbiosTypeCount; Index++) {
    HmcSmbiosTypes[Index] = (UINT8)fdt32_to_cpu (Property[Index]);
  }

  return TRUE;
}

/**
  Check if the SMBIOS type is needed to add into HMC blob.

**/
BOOLEAN
IsTypeSupport (
  IN UINT8  Type
  )
{
  UINT8  Index;

  for (Index = 0; Index < HmcSmbiosTypeCount; Index++) {
    if (Type == HmcSmbiosTypes[Index]) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  This function will set HmcSmbios variable for HMC to pick up

  @param  Event    The event of notify protocol.
  @param  Context  Notify event context.
**/
VOID
EFIAPI
SetHmcSmbiosVariable (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                    Status;
  SMBIOS_TABLE_3_0_ENTRY_POINT  *Smbios30Table;
  SMBIOS_TABLE_3_0_ENTRY_POINT  *HmcSmbios;
  EFI_SMBIOS_TABLE_HEADER       *SmbiosHeader;
  UINTN                         TableSize;
  UINTN                         HmcSmbiosSize;
  INT32                         RemainingSize;
  UINT8                         PlatformType;
  UINT8                         PostSigSent;

  gBS->CloseEvent (Event);

  Smbios30Table = NULL;
  Status        = EfiGetSystemConfigurationTable (&gEfiSmbios3TableGuid, (VOID **)&Smbios30Table);
  if (EFI_ERROR (Status) || (Smbios30Table == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: No SMBIOS Table found: %r\n", __FUNCTION__, Status));
    REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
      EFI_ERROR_CODE | EFI_ERROR_MAJOR,
      EFI_CLASS_NV_FIRMWARE | EFI_NV_FW_UEFI_EC_NO_SMBIOS_TABLE,
      OEM_EC_DESC_NO_SMBIOS_TABLE,
      sizeof (OEM_EC_DESC_NO_SMBIOS_TABLE)
      );
    return;
  }

  // Allocate and init 3.0 table entry.
  HmcSmbios = AllocateZeroPool (sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT));
  CopyMem (HmcSmbios, Smbios30Table, sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT));
  HmcSmbios->TableAddress     = sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT);
  HmcSmbios->TableMaximumSize = 0;
  RemainingSize               = Smbios30Table->TableMaximumSize;
  SmbiosHeader                = (EFI_SMBIOS_TABLE_HEADER *)(Smbios30Table->TableAddress);
  HmcSmbiosSize               = sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT);

  while (RemainingSize > 0) {
    TableSize = SmbiosHeader->Length + GetSmbiosTableStringsSize (SmbiosHeader);

    if (IsTypeSupport (SmbiosHeader->Type)) {
      HmcSmbios = ReallocatePool (HmcSmbiosSize, HmcSmbiosSize + TableSize, HmcSmbios);

      CopyMem ((UINT8 *)HmcSmbios + HmcSmbiosSize, (UINT8 *)SmbiosHeader, TableSize);

      HmcSmbios->TableMaximumSize += TableSize;
      HmcSmbiosSize               += TableSize;
    } else if (SmbiosHeader->Type == SMBIOS_TYPE_END_OF_TABLE) {
      break;
    }

    RemainingSize -= TableSize;
    SmbiosHeader   = (EFI_SMBIOS_TABLE_HEADER *)((VOID *)SmbiosHeader + TableSize);
  }

  HmcSmbios->EntryPointStructureChecksum = 0;
  HmcSmbios->EntryPointStructureChecksum = CalculateCheckSum8 ((UINT8 *)HmcSmbios, HmcSmbios->EntryPointLength);

  Status = gRT->SetVariable (
                  VAR_HMC_SMBIOS_BLOB,
                  &gNVIDIAHmcSmbiosVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  HmcSmbiosSize,
                  (VOID *)HmcSmbios
                  );

  FreePool (HmcSmbios);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Set VAR_HMC_SMBIOS_BLOB %r\n", __FUNCTION__, Status));
    return;
  }

  // PLATFORM_TYPE is not consumed by SatMC currently. Set as a dummpy.
  PlatformType = 0x00;
  Status       = gRT->SetVariable (
                        VAR_PLATFORM_TYPE,
                        &gNVIDIAHmcSmbiosVariableGuid,
                        EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                        sizeof (PlatformType),
                        (VOID *)&PlatformType
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Set VAR_PLATFORM_TYPE %r\n", __FUNCTION__, Status));
  }

  PostSigSent = 0x01;
  Status      = gRT->SetVariable (
                       VAR_POST_SIGNAL_SENT,
                       &gNVIDIAHmcSmbiosVariableGuid,
                       EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                       sizeof (PostSigSent),
                       (VOID *)&PostSigSent
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Set VAR_POST_SIGNAL_SENT %r\n", __FUNCTION__, Status));
  }

  return;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
SmbiosHmcTransferEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   Event;
  UINT8       TransNeeded;

  TransNeeded = IsHmcSupport ();
  Status      = gRT->SetVariable (
                       VAR_SMBIOS_TRANS_NEEDED,
                       &gNVIDIAHmcSmbiosVariableGuid,
                       EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                       sizeof (TransNeeded),
                       (VOID *)&TransNeeded
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Set VAR_SMBIOS_TRANS_NEEDED %r\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  if (TransNeeded) {
    //
    // Register event to set the SMBIOS variables
    //
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    SetHmcSmbiosVariable,
                    NULL,
                    &gNVIDIAEndOfPostToBmcGuid,
                    &Event
                    );

    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}
