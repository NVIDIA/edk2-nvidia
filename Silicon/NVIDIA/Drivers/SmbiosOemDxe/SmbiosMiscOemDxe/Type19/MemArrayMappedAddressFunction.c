/** @file
*
*  Smbios Type 19 Table generation.
*
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#define EXTENDED_ADDRESS_THRESHOLD  (0xFFFFFFFFL)

/**
  This function makes boot time changes to the contents of the
  Memory Array Mapped Address table (Type19)

  @param  RecordData                 Pointer to SMBIOS table with default values.
  @param  Smbios                     SMBIOS protocol.

  @retval EFI_SUCCESS                The SMBIOS table was successfully added.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.
  @retval EFI_OUT_OF_RESOURCES       Failed to allocate required memory.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscMemArrayMap) {
  EFI_STATUS           Status;
  VOID                 *Hob;
  TEGRA_RESOURCE_INFO  *ResInfo = NULL;
  SMBIOS_TABLE_TYPE19  *SmbiosRecord;
  SMBIOS_TABLE_TYPE19  *Input;
  UINT64               StartAddress;
  UINT64               EndAddress;
  UINT64               Size;
  UINT64               StartAddressKb;
  UINT64               EndAddressKb;
  UINTN                HandleCount;
  UINT16               *HandleArray;
  UINTN                Count;

  Status = EFI_INVALID_PARAMETER;
  Hob    = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    ResInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ResourceInfo;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Platform Resource Info\n",
      __FUNCTION__
      ));
    return EFI_DEVICE_ERROR;
  }

  for (Count = 0; Count < ResInfo->DramRegionsCount; Count++) {
    StartAddress   = ResInfo->DramRegions[Count].MemoryBaseAddress;
    Size           = ResInfo->DramRegions[Count].MemoryLength;
    EndAddress     = StartAddress + Size;
    StartAddressKb = StartAddress / SIZE_1KB;
    EndAddressKb   = EndAddress / SIZE_1KB;

    Input = (SMBIOS_TABLE_TYPE19 *)RecordData;

    SmbiosRecord = AllocateZeroPool (sizeof (SMBIOS_TABLE_TYPE19) + 1 + 1);
    if (SmbiosRecord == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    (VOID)CopyMem (SmbiosRecord, Input, sizeof (SMBIOS_TABLE_TYPE19));
    SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE19);

    if (EndAddressKb >= EXTENDED_ADDRESS_THRESHOLD) {
      SmbiosRecord->StartingAddress         = EXTENDED_ADDRESS_THRESHOLD;
      SmbiosRecord->EndingAddress           = EXTENDED_ADDRESS_THRESHOLD;
      SmbiosRecord->ExtendedStartingAddress = StartAddress;
      SmbiosRecord->ExtendedEndingAddress   = EndAddress - 1;
    } else {
      SmbiosRecord->StartingAddress = StartAddressKb;
      SmbiosRecord->EndingAddress   = EndAddressKb - 1;
    }

    HandleCount = 0;
    HandleArray = NULL;

    // Add the reference to the physical memory array.
    SmbiosMiscGetLinkTypeHandle (
      EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY,
      &HandleArray,
      &HandleCount
      );

    // We can't handle multiple tables/references in this
    // driver. This is handled in DynamicTablesPkg which will
    // eventually be used and obsolete this driver.
    // Not a boot critical error so install the table without the reference.
    if (HandleCount > 1) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: %u PhysicalArray Handles found.\n",
        __FUNCTION__,
        HandleCount
        ));
    } else {
      SmbiosRecord->MemoryArrayHandle = HandleArray[0];
    }

    Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "[%a]:[%dL] Smbios Type19 Table Log Failed! %r \n",
        __FUNCTION__,
        DEBUG_LINE_NUMBER,
        Status
        ));
    }

    FreePool (SmbiosRecord);
  }

  return Status;
}
