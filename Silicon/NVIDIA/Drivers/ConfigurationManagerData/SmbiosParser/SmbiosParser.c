/** @file
  Configuration Manager Data of SMBIOS tables.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/FruLib.h>
#include <Library/MemoryAllocationLib.h>
#include <libfdt.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosParser.h"

#include "../ConfigurationManagerDataRepoLib.h"

#include "ConfigurationSmbiosType0/SmbiosType0Parser.h"
#include "ConfigurationSmbiosType1/SmbiosType1Parser.h"
#include "ConfigurationSmbiosType2/SmbiosType2Parser.h"
#include "ConfigurationSmbiosType3/SmbiosType3Parser.h"
#include "ConfigurationSmbiosProcSub/SmbiosProcSubParser.h"
#include "ConfigurationSmbiosType8/SmbiosType8Parser.h"
#include "ConfigurationSmbiosType9/SmbiosType9Parser.h"
#include "ConfigurationSmbiosType11/SmbiosType11Parser.h"
#include "ConfigurationSmbiosType13/SmbiosType13Parser.h"
#include "ConfigurationSmbiosMem/SmbiosMemParser.h"
#include "ConfigurationSmbiosType32/SmbiosType32Parser.h"
#include "ConfigurationSmbiosType38/SmbiosType38Parser.h"
#include "ConfigurationSmbiosType39/SmbiosType39Parser.h"
#include "ConfigurationSmbiosType41/SmbiosType41Parser.h"
#include "ConfigurationSmbiosType43/SmbiosType43Parser.h"
#include "ConfigurationSmbiosType45/SmbiosType45Parser.h"

/**
  Find FRU by FRU description

  @param[in] Private          Pointer to the private data of SMBIOS creators
  @param[in] FruDescPattern   FRU description pattern to look for.
                              Wildcard '?' can be used to match any characters.

  @return A pointer to the FRU record if found, or NULL if not found.

**/
FRU_DEVICE_INFO *
FindFruByDescription (
  IN  CM_SMBIOS_PRIVATE_DATA  *Private,
  IN  CHAR8                   *FruDescPattern
  )
{
  FRU_DEVICE_INFO  **FruInfo = Private->FruInfo;
  UINT8            FruCount  = Private->FruCount;
  UINT8            Index;
  CHAR8            *FruDesc;
  CHAR8            *Pattern;

  for (Index = 0; Index < FruCount; Index++) {
    FruDesc = FruInfo[Index]->FruDeviceDescription;
    Pattern = FruDescPattern;

    while ((*FruDesc != '\0') &&
           ((*Pattern == '?') || (*FruDesc == *Pattern)))
    {
      FruDesc++;
      Pattern++;
    }

    if (*FruDesc == *Pattern) {
      return FruInfo[Index];
    }
  }

  return NULL;
}

/**
  Find and get FRU extra string that has a certain prefix

  @param[in] FruExtra  Pointer to the array of FRU (chassis/board/product) extra
  @param[in] Prefix    FRU extra prefix to search for

  @return A pointer to an allocated string

**/
CHAR8 *
GetFruExtraStr (
  IN CHAR8        **FruExtra,
  IN CONST CHAR8  *Prefix
  )
{
  UINT32  Index;
  UINTN   PrefixLen;

  ASSERT (FruExtra != NULL);
  ASSERT (Prefix != NULL);

  PrefixLen = AsciiStrLen (Prefix);

  for (Index = 0; Index < MAX_EXTRA_FRU_AREA_ENTRIES; Index++) {
    if (FruExtra[Index] == NULL) {
      break;
    }

    if (AsciiStrnCmp (FruExtra[Index], Prefix, PrefixLen) == 0) {
      return AllocateCopyString (FruExtra[Index] + PrefixLen);
    }
  }

  return NULL;
}

/**
  Allocate and copy string

  @param[in] String     String to be copied

  @return A pointer to the copied string if succeeds, or NULL if fails.

**/
CHAR8 *
AllocateCopyString (
  IN  CHAR8  *String
  )
{
  if (String == NULL) {
    return NULL;
  }

  return (CHAR8 *)AllocateCopyPool (AsciiStrLen (String) + 1, String);
}

/** Install the SMBIOS tables to Configuration Manager Data driver

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
SmbiosParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  CM_SMBIOS_PRIVATE_DATA       *Private;
  EFI_STATUS                   Status;
  UINTN                        Index;
  CM_OBJ_DESCRIPTOR            Desc;
  CM_SMBIOS_RECORD_POPULATION  CmInstallSmbiosRecords[] = {
    { EFI_SMBIOS_TYPE_BIOS_INFORMATION,                     InstallSmbiosType0Cm   },
    { EFI_SMBIOS_TYPE_SYSTEM_INFORMATION,                   InstallSmbiosType1Cm   },
    { EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE,                     InstallSmbiosType3Cm   },
    { EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION,                InstallSmbiosProcSubCm },
    { EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION,           InstallSmbiosType8Cm   },
    { EFI_SMBIOS_TYPE_SYSTEM_SLOTS,                         InstallSmbiosType9Cm   },
    { EFI_SMBIOS_TYPE_OEM_STRINGS,                          InstallSmbiosType11Cm  },
    { EFI_SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION,            InstallSmbiosType13Cm  },
    { EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY,                InstallSmbiosTypeMemCm },
    { EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION,                InstallSmbiosType2Cm   },
    { EFI_SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION,              InstallSmbiosType32Cm  },
    { EFI_SMBIOS_TYPE_IPMI_DEVICE_INFORMATION,              InstallSmbiosType38Cm  },
    { EFI_SMBIOS_TYPE_SYSTEM_POWER_SUPPLY,                  InstallSmbiosType39Cm  },
    { EFI_SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION, InstallSmbiosType41Cm  },
    { SMBIOS_TYPE_TPM_DEVICE,                               InstallSmbiosType43Cm  },
    { SMBIOS_TYPE_FIRMWARE_INVENTORY_INFORMATION,           InstallSmbiosType45Cm  }
  };

  Private = NULL;

  Private = AllocatePool (sizeof (CM_SMBIOS_PRIVATE_DATA));
  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for Smbios private data\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Private->CmSmbiosTableCount              = 0;
  Private->EnclosureBaseboardBinding.Count = 0;
  Private->EnclosureBaseboardBinding.Info  = NULL;

  //
  // Load device tree SMBIOS node
  //
  Status = DtPlatformLoadDtb (&Private->DtbBase, &Private->DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to load device tree.\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  Private->DtbSmbiosOffset = fdt_path_offset (Private->DtbBase, "/firmware/smbios");
  if (Private->DtbSmbiosOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS not found.\n", __FUNCTION__));
    // Continue anyway to install SMBIOS tables that do not need DTB
  }

  //
  // Read all FRUs
  //
  Status = ReadAllFrus (&Private->FruInfo, &Private->FruCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to read FRUs.\n", __FUNCTION__));
    // Continue anyway to install SMBIOS tables that do not need FRU info
    Private->FruCount = 0;
  }

  //
  // Install CM object for each SMBIOS table
  //
  for (Index = 0; Index < ARRAY_SIZE (CmInstallSmbiosRecords); Index++) {
    Status = CmInstallSmbiosRecords[Index].Function (ParserHandle, Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Install CM object of SMBIOS Type %u, Status = %r.\n", __FUNCTION__, CmInstallSmbiosRecords[Index].Type, Status));
    }
  }

  //
  // Free all FRUs
  //
  Status = FreeAllFruRecords ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r from FreeAllFruRecords\n", __FUNCTION__, Status));
  }

  //
  // Install CM object for SMBIOS table list
  //
  if (Private->CmSmbiosTableCount > 0) {
    Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjSmbiosTableList);
    Desc.Size     = sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO) * Private->CmSmbiosTableCount;
    Desc.Count    = Private->CmSmbiosTableCount;
    Desc.Data     = &Private->CmSmbiosTableList;
    DEBUG ((DEBUG_ERROR, "%a: Private->CmSmbiosTableCount = %d\n", __FUNCTION__, Private->CmSmbiosTableCount));
    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }
  }

CleanupAndReturn:
  FREE_NON_NULL (Private);
  return EFI_SUCCESS;
}

REGISTER_PARSER_FUNCTION (SmbiosParser, "skip-smbios-table")
