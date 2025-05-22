/** @file
  Configuration Manager Data of SMBIOS tables.

  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/FruLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <libfdt.h>

#include <Protocol/PciIo.h>

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

#define PCIE_SBDF(s, b, d, f)  (((s) << 16) | ((b) << 8) | ((d) << 3) | (f))

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

/**
  Search for a string or substring in a FRU record.

  @param[in] FruRecord   Pointer to the FRU_DEVICE_INFO to search.
  @param[in] SearchStr   The string to search for.

  @retval TRUE   If a match or substring is found in any field.
  @retval FALSE  Otherwise.
**/
BOOLEAN
FruRecordSearchStr (
  IN FRU_DEVICE_INFO  *FruRecord,
  IN CONST CHAR8      *SearchStr
  )
{
  UINT32  Idx;

  if ((FruRecord == NULL) || (SearchStr == NULL)) {
    return FALSE;
  }

  // Check main string fields in the specified fru record
  if (((FruRecord->ChassisPartNum != NULL) && (AsciiStrStr (FruRecord->ChassisPartNum, SearchStr) != NULL)) ||
      ((FruRecord->ChassisSerial != NULL) && (AsciiStrStr (FruRecord->ChassisSerial, SearchStr) != NULL)) ||
      ((FruRecord->BoardManufacturer != NULL) && (AsciiStrStr (FruRecord->BoardManufacturer, SearchStr) != NULL)) ||
      ((FruRecord->BoardProduct != NULL) && (AsciiStrStr (FruRecord->BoardProduct, SearchStr) != NULL)) ||
      ((FruRecord->BoardSerial != NULL) && (AsciiStrStr (FruRecord->BoardSerial, SearchStr) != NULL)) ||
      ((FruRecord->BoardPartNum != NULL) && (AsciiStrStr (FruRecord->BoardPartNum, SearchStr) != NULL)) ||
      ((FruRecord->ProductManufacturer != NULL) && (AsciiStrStr (FruRecord->ProductManufacturer, SearchStr) != NULL)) ||
      ((FruRecord->ProductName != NULL) && (AsciiStrStr (FruRecord->ProductName, SearchStr) != NULL)) ||
      ((FruRecord->ProductPartNum != NULL) && (AsciiStrStr (FruRecord->ProductPartNum, SearchStr) != NULL)) ||
      ((FruRecord->ProductVersion != NULL) && (AsciiStrStr (FruRecord->ProductVersion, SearchStr) != NULL)) ||
      ((FruRecord->ProductSerial != NULL) && (AsciiStrStr (FruRecord->ProductSerial, SearchStr) != NULL)) ||
      ((FruRecord->ProductAssetTag != NULL) && (AsciiStrStr (FruRecord->ProductAssetTag, SearchStr) != NULL)))
  {
    return TRUE;
  }

  // Check extra fields (arrays)
  for (Idx = 0; Idx < MAX_EXTRA_FRU_AREA_ENTRIES; Idx++) {
    if (((FruRecord->ChassisExtra[Idx] != NULL) && (AsciiStrStr (FruRecord->ChassisExtra[Idx], SearchStr) != NULL)) ||
        ((FruRecord->BoardExtra[Idx] != NULL) && (AsciiStrStr (FruRecord->BoardExtra[Idx], SearchStr) != NULL)) ||
        ((FruRecord->ProductExtra[Idx] != NULL) && (AsciiStrStr (FruRecord->ProductExtra[Idx], SearchStr) != NULL)))
    {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Check if the input DTB node has condition and if the condition is satisfied.
  Examples:

    type9@1 {
      condition {
        type = "pcie";
        address = <PCIE_SBDF(9,1,0,0) 0x00>;
        value = <0x102315B3 0xFFFFFFFF>;
      };
      ...
    };

    type39@0 {
      condition {
        type = "fru-desc";
        value = "PDU_FRU0";
      };
      ...
    };

  @param[in] Private     Pointer to the private data of SMBIOS creators
  @param[in] NodeOffset  Offset to DTB node to check

  @retval EFI_SUCCESS             The condition is satisfied.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No condition found.
  @retval EFI_UNSUPPORTED         The condition is not satisfied.
**/
EFI_STATUS
EvaluateDtbNodeCondition (
  IN  CM_SMBIOS_PRIVATE_DATA  *Private,
  IN  INTN                    NodeOffset
  )
{
  EFI_STATUS           Status;
  VOID                 *DtbBase;
  CONST VOID           *Property;
  INT32                Length;
  INT32                DtbOffset;
  FRU_DEVICE_INFO      *Fru;
  CHAR8                *TypeStr;
  CHAR8                *FruDesc;
  CHAR8                *Pattern;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  UINTN                Index;
  UINTN                HandleCount;
  EFI_HANDLE           *Handles;
  UINT32               PcieSBDF;
  UINT32               PcieRegOffset;
  UINT32               Value;
  UINT32               ExpectedValue;
  UINT32               ExpectedMask = 0xFFFFFFFF;
  BOOLEAN              Unexpected   = FALSE;
  BOOLEAN              StrExist     = FALSE;
  UINTN                Segment;
  UINTN                Bus;
  UINTN                Device;
  UINTN                Function;
  UINT64               MmioAddr;

  if ((Private == NULL) || (NodeOffset < 0)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DtbBase = Private->DtbBase;

  //
  // Check if the node has "condition"
  //
  DtbOffset = fdt_subnode_offset (DtbBase, NodeOffset, "condition");
  if (DtbOffset < 0) {
    return EFI_NOT_FOUND;
  }

  //
  // Supported condition types: fru-desc, pci, mmio
  //
  Property = fdt_getprop (DtbBase, DtbOffset, "type", &Length);
  if ((Property == NULL) || (Length == 0)) {
    DEBUG ((DEBUG_ERROR, "'condition/type' not found.\n"));
    return EFI_INVALID_PARAMETER;
  }

  TypeStr = (CHAR8 *)Property;

  //
  // If 'unexpected' is defined, negate the condition
  //
  Property = fdt_getprop (DtbBase, DtbOffset, "unexpected", &Length);
  if (Property != NULL) {
    Unexpected = TRUE;
  }

  //
  // Check if FRU is present
  //
  if (AsciiStriCmp (TypeStr, "fru-desc") == 0) {
    Property = fdt_getprop (DtbBase, DtbOffset, "value", &Length);
    if ((Property == NULL) || (Length == 0)) {
      DEBUG ((DEBUG_ERROR, "'condition/value' not found.\n"));
      return EFI_INVALID_PARAMETER;
    }

    FruDesc = (CHAR8 *)Property;
    Fru     = FindFruByDescription (Private, FruDesc);
    if (Fru != NULL) {
      Property = fdt_getprop (DtbBase, DtbOffset, "pattern", &Length);
      if ((Property == NULL) || (Length == 0)) {
        DEBUG ((DEBUG_ERROR, "'condition/pattern' not found.\n"));
        return EFI_INVALID_PARAMETER;
      }

      if (Length > 1) {
        Pattern  = (CHAR8 *)Property;
        StrExist = FruRecordSearchStr (Fru, Pattern);
        if (StrExist) {
          return Unexpected ? EFI_UNSUPPORTED : EFI_SUCCESS;
        }
      }
    }

    return Unexpected ? EFI_SUCCESS : EFI_UNSUPPORTED;
  }

  //
  // Read PCI config space register and check if it matches 'value'
  //
  if (AsciiStriCmp (TypeStr, "pcie") == 0) {
    Property = fdt_getprop (DtbBase, DtbOffset, "address", &Length);
    if ((Property == NULL) || (Length < 8)) {
      DEBUG ((DEBUG_ERROR, "'condition/address' not found.\n"));
      return EFI_INVALID_PARAMETER;
    }

    PcieSBDF      = fdt32_to_cpu (*(UINT32 *)Property);
    PcieRegOffset = fdt32_to_cpu (*((UINT32 *)Property + 1));

    Property = fdt_getprop (DtbBase, DtbOffset, "value", &Length);
    if ((Property == NULL) || (Length < 4)) {
      DEBUG ((DEBUG_ERROR, "'condition/value' not found.\n"));
      return EFI_INVALID_PARAMETER;
    }

    ExpectedValue = fdt32_to_cpu (*(UINT32 *)Property);
    if (Length == 8) {
      ExpectedMask = fdt32_to_cpu (*((UINT32 *)Property + 1));
    }

    //
    // Locate protocol based on SBDF and read the CSR
    //
    Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleCount, &Handles);
    ASSERT_EFI_ERROR (Status);

    for (Index = 0; Index < HandleCount; Index++) {
      Status = gBS->HandleProtocol (Handles[Index], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
      ASSERT_EFI_ERROR (Status);

      Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
      ASSERT_EFI_ERROR (Status);

      if (PcieSBDF == PCIE_SBDF (Segment, Bus, Device, Function)) {
        Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, PcieRegOffset, 1, &Value);
        if (!EFI_ERROR (Status) && ((Value & ExpectedMask) == (ExpectedValue & ExpectedMask))) {
          return Unexpected ? EFI_UNSUPPORTED : EFI_SUCCESS;
        }

        return Unexpected ? EFI_SUCCESS : EFI_UNSUPPORTED;
      }
    }

    DEBUG ((DEBUG_ERROR, "%a: SBDF %08x not found\n", __FUNCTION__, PcieSBDF));
    return Unexpected ? EFI_SUCCESS : EFI_UNSUPPORTED;
  }

  //
  // Read MMIO register and matches it against 'value'
  //
  if (AsciiStriCmp (TypeStr, "mmio") == 0) {
    Property = fdt_getprop (DtbBase, DtbOffset, "address", &Length);
    if ((Property == NULL) || (Length < 8)) {
      DEBUG ((DEBUG_ERROR, "'condition/address' not found.\n"));
      return EFI_INVALID_PARAMETER;
    }

    MmioAddr = fdt64_to_cpu (*(UINT32 *)Property);

    Property = fdt_getprop (DtbBase, DtbOffset, "value", &Length);
    if ((Property == NULL) || (Length < 4)) {
      DEBUG ((DEBUG_ERROR, "'condition/value' not found.\n"));
      return EFI_INVALID_PARAMETER;
    }

    ExpectedValue = fdt32_to_cpu (*(UINT32 *)Property);
    if (Length == 8) {
      ExpectedMask = fdt32_to_cpu (*((UINT32 *)Property + 1));
    }

    Value = MmioRead32 (MmioAddr);
    if ((Value & ExpectedMask) == (ExpectedValue & ExpectedMask)) {
      return Unexpected ? EFI_UNSUPPORTED : EFI_SUCCESS;
    }

    return Unexpected ? EFI_SUCCESS : EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_ERROR, "%a: condition/type='%a' not supported\n", __FUNCTION__, TypeStr));
  return EFI_INVALID_PARAMETER;
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
