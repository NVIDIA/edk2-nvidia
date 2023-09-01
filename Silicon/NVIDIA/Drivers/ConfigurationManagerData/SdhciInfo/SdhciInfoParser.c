/** @file
  Sdhci info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "SdhciInfoParser.h"
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/PrintLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>

#include "SdcTemplate.hex"
#include "SdcTemplate.offset.h"

/** Sdhci info parser function.

  Adds SDHCI information to the SSDT ACPI table being generated

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
SdhciInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                                     Status;
  UINT32                                         Index;
  NVIDIA_AML_GENERATION_PROTOCOL                 *GenerationProtocol;
  NVIDIA_AML_PATCH_PROTOCOL                      *PatchProtocol;
  UINT32                                         NumberOfSdhciPorts;
  UINT32                                         *SdhciHandles;
  NVIDIA_DEVICE_TREE_REGISTER_DATA               RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA              InterruptData;
  UINT32                                         Size;
  CHAR8                                          SdcPathString[ACPI_PATCH_MAX_PATH];
  NVIDIA_AML_NODE_INFO                           AcpiNodeInfo;
  EFI_ACPI_32_BIT_FIXED_MEMORY_RANGE_DESCRIPTOR  MemoryDescriptor;
  EFI_ACPI_EXTENDED_INTERRUPT_DESCRIPTOR         InterruptDescriptor;
  VOID                                           *DeviceTreeBase;
  INT32                                          NodeOffset;
  UINT32                                         Removable;
  CHAR8                                          *CompatStr;
  UINTN                                          ChipID;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Status = NvGetCmGenerationProtocol (ParserHandle, &GenerationProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = NvGetCmPatchProtocol (ParserHandle, &PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumberOfSdhciPorts = 0;
  ChipID             = TegraGetChipID ();
  if (ChipID == T194_CHIP_ID) {
    CompatStr = "nvidia,tegra194-sdhci";
  } else if (ChipID == T234_CHIP_ID) {
    CompatStr = "nvidia,tegra234-sdhci";
  } else {
    DEBUG ((DEBUG_ERROR, "Unsupported ChipID for SdhciInfoParser\n"));
    return EFI_UNSUPPORTED;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (CompatStr, NULL, &NumberOfSdhciPorts);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  SdhciHandles = NULL;
  SdhciHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfSdhciPorts);
  if (SdhciHandles == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (CompatStr, SdhciHandles, &NumberOfSdhciPorts);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumberOfSdhciPorts; Index++) {
    // Only one register space is expected
    Size   = 1;
    Status = GetDeviceTreeRegisters (SdhciHandles[Index], &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    // Only one interrupt is expected
    Size   = 1;
    Status = GetDeviceTreeInterrupts (SdhciHandles[Index], &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SDCT_UID, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_UID));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &Index, AcpiNodeInfo.Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_UID));
      goto CleanupAndReturn;
    }

    GetDeviceTreeNode (SdhciHandles[Index], &DeviceTreeBase, &NodeOffset);
    Status = DeviceTreeGetNodeProperty (NodeOffset, "non-removable", NULL, NULL);
    if (!EFI_ERROR (Status)) {
      Removable = 0;
    } else {
      Removable = 1;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SDCT_RMV, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_RMV));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &Removable, AcpiNodeInfo.Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_RMV));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SDCT_REG0, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto CleanupAndReturn;
    }

    if (AcpiNodeInfo.Size != sizeof (MemoryDescriptor)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, ACPI_SDCT_REG0, AcpiNodeInfo.Size));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->GetNodeData (PatchProtocol, &AcpiNodeInfo, &MemoryDescriptor, sizeof (MemoryDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get data for %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto CleanupAndReturn;
    }

    MemoryDescriptor.BaseAddress = RegisterData.BaseAddress;
    MemoryDescriptor.Length      = RegisterData.Size;

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &MemoryDescriptor, sizeof (MemoryDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SDCT_INT0, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto CleanupAndReturn;
    }

    if (AcpiNodeInfo.Size != sizeof (InterruptDescriptor)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, ACPI_SDCT_INT0, AcpiNodeInfo.Size));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->GetNodeData (PatchProtocol, &AcpiNodeInfo, &InterruptDescriptor, sizeof (InterruptDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get data for %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto CleanupAndReturn;
    }

    InterruptDescriptor.InterruptNumber[0] = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData);

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &InterruptDescriptor, sizeof (InterruptDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, "SDCT", &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, "SDCT"));
      goto CleanupAndReturn;
    }

    AsciiSPrint (SdcPathString, sizeof (SdcPathString), "SDC%u", Index);
    Status = PatchProtocol->UpdateNodeName (PatchProtocol, &AcpiNodeInfo, SdcPathString);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to update name to %a\n", __FUNCTION__, SdcPathString));
      goto CleanupAndReturn;
    }

    Status = GenerationProtocol->AppendDevice (GenerationProtocol, (EFI_ACPI_DESCRIPTION_HEADER *)sdctemplate_aml_code);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to append device %a\n", __FUNCTION__, SdcPathString));
      goto CleanupAndReturn;
    }
  }

CleanupAndReturn:
  FREE_NON_NULL (SdhciHandles);
  return Status;
}
