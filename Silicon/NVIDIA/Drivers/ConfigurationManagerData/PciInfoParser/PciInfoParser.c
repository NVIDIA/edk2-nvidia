/** @file
  PCI info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "PciInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"
#include "Uefi/UefiBaseType.h"

#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/PrintLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/SortLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

/**
  Compare config space by segment number.

  @param[in] Buffer1                  The pointer to first buffer.
  @param[in] Buffer2                  The pointer to second buffer.

  @retval 0                           Buffer1 equal to Buffer2.
  @return <0                          Buffer1 is less than Buffer2.
  @return >0                          Buffer1 is greater than Buffer2.
**/
INTN
ConfigSpaceCompare (
  IN CONST VOID  *Buffer1,
  IN CONST VOID  *Buffer2
  )
{
  CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO  *ConfigSpaceInfo1;
  CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO  *ConfigSpaceInfo2;

  ConfigSpaceInfo1 = (CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO *)Buffer1;
  ConfigSpaceInfo2 = (CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO *)Buffer2;

  return (INTN)ConfigSpaceInfo1->PciSegmentGroupNumber - (INTN)ConfigSpaceInfo2->PciSegmentGroupNumber;
}

/** PCI info parser function.

  Updates PCI information in the SSDT/MCFG ACPI

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
PciInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                            Status;
  CM_STD_OBJ_ACPI_TABLE_INFO            AcpiTableHeader;
  CM_OBJ_DESCRIPTOR                     Desc;
  UINTN                                 Index;
  UINTN                                 NumberOfHandles;
  EFI_HANDLE                            *HandleBuffer;
  CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO  *ConfigSpaceInfo;
  CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO  *ConfigSpaceInfoArray;
  UINTN                                 ConfigSpaceInfoSize;

  ConfigSpaceInfoArray = NULL;
  Status               = gBS->LocateHandleBuffer (
                                ByProtocol,
                                &gNVIDIAPciConfigurationDataProtocolGuid,
                                NULL,
                                &NumberOfHandles,
                                &HandleBuffer
                                );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to find PciConfigurationDataProtocol\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  ConfigSpaceInfoSize  = sizeof (CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO) * NumberOfHandles;
  ConfigSpaceInfoArray = (CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO *)AllocatePool (ConfigSpaceInfoSize);
  if (ConfigSpaceInfoArray == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate ConfigSpaceInfoArray\r\n", __func__));
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gNVIDIAPciConfigurationDataProtocolGuid,
                    (VOID **)&ConfigSpaceInfo
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get PCI config space info - %r\r\n", __func__, Status));
      goto CleanupAndReturn;
    }

    CopyMem (&ConfigSpaceInfoArray[Index], ConfigSpaceInfo, sizeof (CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO));
  }

  PerformQuickSort (ConfigSpaceInfoArray, NumberOfHandles, sizeof (CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO), ConfigSpaceCompare);

  Desc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjPciConfigSpaceInfo);
  Desc.Size     = ConfigSpaceInfoSize;
  Desc.Count    = NumberOfHandles;
  Desc.Data     = ConfigSpaceInfoArray;
  Status        = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add ConfigSpaceInfoArray to CM\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  // Create ACPI Table Entries
  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_3_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg);
  AcpiTableHeader.AcpiTableData      = NULL;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to add PCI MCFG ACPI table - %r\r\n", __func__, Status));
    goto CleanupAndReturn;
  }

  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtPciExpress);
  AcpiTableHeader.AcpiTableData      = NULL;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to add PCI SSDT ACPI table - %r\r\n", __func__, Status));
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (ConfigSpaceInfoArray);
  return Status;
}

REGISTER_PARSER_FUNCTION (PciInfoParser, NULL)
