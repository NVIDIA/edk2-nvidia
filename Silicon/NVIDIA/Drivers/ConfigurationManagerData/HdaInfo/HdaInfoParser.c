/** @file
  Hda info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "HdaInfoParser.h"
#include <Library/AmlLib/AmlLib.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/PrintLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>

#include "SsdtHda.hex"
#define HDA_REG_OFFSET  0x8000

/** HDA info parser function.

  Updates SDHCI information in the DSDT ACPI

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
HdaInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                         Status;
  AML_ROOT_NODE_HANDLE               RootNode;
  AML_NODE_HANDLE                    SbNode;
  AML_NODE_HANDLE                    HdaNode;
  AML_NODE_HANDLE                    HdaNewNode;
  AML_NODE_HANDLE                    BaseNode;
  AML_NODE_HANDLE                    UidNode;
  AML_NODE_HANDLE                    ResourceNode;
  AML_NODE_HANDLE                    MemoryNode;
  INT32                              NodeOffset;
  CONST CHAR8                        *CompatibleInfo[] = { "nvidia,tegra234-hda", "nvidia,tegra23x-hda", NULL };
  CHAR8                              HdaNodeName[]     = "HDAx";
  UINT32                             InterruptId;
  UINT32                             NumberOfHda;
  NVIDIA_DEVICE_TREE_REGISTER_DATA   RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  InterruptData;
  UINT32                             Size;
  EFI_ACPI_DESCRIPTION_HEADER        *NewTable;
  CM_OBJ_DESCRIPTOR                  Desc;
  CM_STD_OBJ_ACPI_TABLE_INFO         AcpiTableHeader;

  Status = AmlParseDefinitionBlock ((const EFI_ACPI_DESCRIPTION_HEADER *)ssdthda_aml_code, &RootNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to parse hda ssdt - %r\r\n", __func__, Status));
    return Status;
  }

  Status = AmlFindNode ((AML_NODE_HANDLE)RootNode, "\\_SB_", &SbNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to find SB node - %r\r\n", __func__, Status));
    return Status;
  }

  Status = AmlFindNode (SbNode, "HDA0", &HdaNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to find hda node - %r\r\n", __func__, Status));
    return Status;
  }

  Status = AmlDetachNode (HdaNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to detach hda node - %r\r\n", __func__, Status));
    return Status;
  }

  NumberOfHda = 0;
  NodeOffset  = -1;
  while (EFI_SUCCESS == DeviceTreeGetNextCompatibleNode (CompatibleInfo, &NodeOffset)) {
    // Only one register space is expected
    Size   = 1;
    Status = DeviceTreeGetRegisters (NodeOffset, &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to get registers - %r\r\n", __func__, Status));
      break;
    }

    // Only one interrupt is expected
    Size   = 1;
    Status = DeviceTreeGetInterrupts (NodeOffset, &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to get interrupts - %r\r\n", __func__, Status));
      break;
    }

    Status = AmlCloneTree (HdaNode, &HdaNewNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to clone node - %r\r\n", __func__, Status));
      break;
    }

    Status = AmlAttachNode (SbNode, HdaNewNode);
    if (EFI_ERROR (Status)) {
      AmlDeleteTree (HdaNewNode);
      DEBUG ((DEBUG_ERROR, "%a: Unable to attach hda node - %r\r\n", __func__, Status));
      break;
    }

    AsciiSPrint (HdaNodeName, sizeof (HdaNodeName), "HDA%u", NumberOfHda);
    Status = AmlDeviceOpUpdateName (HdaNewNode, HdaNodeName);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to update node name - %r\r\n", __func__, Status));
      break;
    }

    Status = AmlFindNode (HdaNewNode, "_UID", &UidNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to find Uid node - %r\r\n", __func__, Status));
      break;
    }

    Status = AmlNameOpUpdateInteger (UidNode, NumberOfHda);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to update Uid node - %r\r\n", __func__, Status));
      break;
    }

    Status = AmlFindNode (HdaNewNode, "BASE", &BaseNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to find base node - %r\r\n", __func__, Status));
      break;
    }

    Status = AmlNameOpUpdateInteger (BaseNode, RegisterData.BaseAddress);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to update base node - %r\r\n", __func__, Status));
      break;
    }

    Status = AmlCodeGenNameResourceTemplate ("_CRS", HdaNewNode, &ResourceNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to create _CRS node - %r\r\n", __func__, Status));
      break;
    }

    Status = AmlCodeGenRdMemory32Fixed (
               TRUE,
               RegisterData.BaseAddress + HDA_REG_OFFSET,
               RegisterData.Size - HDA_REG_OFFSET,
               ResourceNode,
               &MemoryNode
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to create memory node - %r\r\n", __func__, Status));
      break;
    }

    InterruptId = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData);
    Status      = AmlCodeGenRdInterrupt (
                    TRUE,
                    FALSE,
                    FALSE,
                    FALSE,
                    &InterruptId,
                    1,
                    ResourceNode,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to create memory node - %r\r\n", __func__, Status));
      break;
    }

    NumberOfHda++;
  }

  if (!EFI_ERROR (Status) && (NumberOfHda != 0)) {
    // Install new table

    Status = AmlSerializeDefinitionBlock (RootNode, &NewTable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to serialize table - %r\r\n", __func__, Status));
      return Status;
    }

    // Create a ACPI Table Entry
    AcpiTableHeader.AcpiTableSignature = NewTable->Signature;
    AcpiTableHeader.AcpiTableRevision  = NewTable->Revision;
    AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
    AcpiTableHeader.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)NewTable;
    AcpiTableHeader.OemTableId         = NewTable->OemTableId;
    AcpiTableHeader.OemRevision        = NewTable->OemRevision;
    AcpiTableHeader.MinorRevision      = 0;

    Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
    Desc.Size     = sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
    Desc.Count    = 1;
    Desc.Data     = &AcpiTableHeader;

    Status = NvExtendCmObj (ParserHandle, &Desc, CM_NULL_TOKEN, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add ACPI table - %r\r\n", __func__, Status));
      return Status;
    }
  }

  AmlDeleteTree (RootNode);
  return Status;
}
