/** @file
*
*  AML generation protocol implementation.
*
*  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SmbiosMiscLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PlatformResourceLib.h>

#include "SmbiosMiscOem.h"

#define EXTENDED_SIZE_THRESHOLD  (SIZE_2TB)

STATIC
UINTN
GetNumDevices (
  VOID
  )
{
  UINTN   NumDevices = 0;
  VOID    *Hob;
  UINT32  SocketMask;
  UINTN   Index;
  UINTN   MaxSockets;

  MaxSockets = PcdGet32 (PcdTegraMaxSockets);
  Hob        = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    SocketMask = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->SocketMask;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Platform Resource Info\n",
      __FUNCTION__
      ));
    goto ExitGetNumDevices;
  }

  for (Index = 0; Index < MaxSockets; Index++) {
    if (!(SocketMask & (1UL << Index))) {
      continue;
    }

    NumDevices++;
  }

  DEBUG ((DEBUG_INFO, "%a: NumDevices = %u\n", __FUNCTION__, NumDevices));
ExitGetNumDevices:
  return NumDevices;
}

STATIC
UINT64
GetTotalDram (
  VOID
  )
{
  UINT64  TotalDram = 0;
  VOID    *Hob;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    TotalDram = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->PhysicalDramSize;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Platform Resource Info\n",
      __FUNCTION__
      ));
  }

  DEBUG ((DEBUG_INFO, "%a: Total Dram = %lu\n", __FUNCTION__, TotalDram));
  return TotalDram;
}

/**
  This function makes boot time changes to the contents of the
  MiscBootInformation (Type 32) record.

  @param  RecordData                 Pointer to SMBIOS table with default values.
  @param  Smbios                     SMBIOS protocol.

  @retval EFI_SUCCESS                The SMBIOS table was successfully added.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.
  @retval EFI_OUT_OF_RESOURCES       Failed to allocate required memory.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscPhysMemArray) {
  EFI_STATUS           Status;
  SMBIOS_TABLE_TYPE16  *SmbiosRecord;
  SMBIOS_TABLE_TYPE16  *Input;
  UINT64               TotalDram;
  UINT64               SizeKb;

  if (RecordData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Input = (SMBIOS_TABLE_TYPE16 *)RecordData;

  SmbiosRecord = AllocateZeroPool (sizeof (SMBIOS_TABLE_TYPE16) + 1 + 1);
  if (SmbiosRecord == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  (VOID)CopyMem (SmbiosRecord, Input, sizeof (SMBIOS_TABLE_TYPE16));
  SmbiosRecord->Hdr.Length                   = sizeof (SMBIOS_TABLE_TYPE16);
  SmbiosRecord->Location                     = OemGetPhysMemArrayLocation ();
  SmbiosRecord->Use                          = OemGetPhysMemArrayUse ();
  SmbiosRecord->MemoryErrorCorrection        = OemGetPhysMemErrCorrection ();
  SmbiosRecord->MemoryErrorInformationHandle = OemGetPhysMemErrInfoHandle ();
  SmbiosRecord->NumberOfMemoryDevices        = GetNumDevices ();

  TotalDram = GetTotalDram ();
  SizeKb    = TotalDram / 1024;

  if (TotalDram < EXTENDED_SIZE_THRESHOLD) {
    SmbiosRecord->MaximumCapacity         = SizeKb;
    SmbiosRecord->ExtendedMaximumCapacity = 0;
  } else {
    SmbiosRecord->MaximumCapacity         = 0x80000000;
    SmbiosRecord->ExtendedMaximumCapacity = TotalDram;
  }

  Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[%a]:[%dL] Smbios Type16 Table Log Failed! %r \n",
      __FUNCTION__,
      DEBUG_LINE_NUMBER,
      Status
      ));
  }

  FreePool (SmbiosRecord);
  return Status;
}
