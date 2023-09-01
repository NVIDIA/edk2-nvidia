/** @file
  I2C info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "I2cInfoParser.h"
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PrintLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include "I2cTemplate.hex"
#include "I2cTemplate.offset.h"

/** I2C info parser function.

  Adds I2C information to the SSDT ACPI table being generated

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
I2cInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                                     Status;
  UINT32                                         Index;
  NVIDIA_AML_GENERATION_PROTOCOL                 *GenerationProtocol;
  NVIDIA_AML_PATCH_PROTOCOL                      *PatchProtocol;
  UINT32                                         NumberOfI2cPorts;
  UINT32                                         *I2cHandles;
  NVIDIA_DEVICE_TREE_REGISTER_DATA               RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA              InterruptData;
  UINT32                                         Size;
  CHAR8                                          I2cPathString[ACPI_PATCH_MAX_PATH];
  NVIDIA_AML_NODE_INFO                           AcpiNodeInfo;
  EFI_ACPI_32_BIT_FIXED_MEMORY_RANGE_DESCRIPTOR  MemoryDescriptor;
  EFI_ACPI_EXTENDED_INTERRUPT_DESCRIPTOR         InterruptDescriptor;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (TegraGetChipID () != T194_CHIP_ID) {
    return EFI_SUCCESS;
  }

  Status = NvGetCmGenerationProtocol (ParserHandle, &GenerationProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = NvGetCmPatchProtocol (ParserHandle, &PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumberOfI2cPorts = 0;
  Status           = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra194-i2c", NULL, &NumberOfI2cPorts);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  I2cHandles = NULL;
  I2cHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfI2cPorts);
  if (I2cHandles == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra194-i2c", I2cHandles, &NumberOfI2cPorts);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumberOfI2cPorts; Index++) {
    // Only one register space is expected
    Size   = 1;
    Status = GetDeviceTreeRegisters (I2cHandles[Index], &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    // Only one interrupt is expected
    Size   = 1;
    Status = GetDeviceTreeInterrupts (I2cHandles[Index], &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_I2CT_UID, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_I2CT_UID));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &Index, AcpiNodeInfo.Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_I2CT_UID));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_I2CT_REG0, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_I2CT_REG0));
      goto CleanupAndReturn;
    }

    if (AcpiNodeInfo.Size != sizeof (MemoryDescriptor)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, ACPI_I2CT_REG0, AcpiNodeInfo.Size));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->GetNodeData (PatchProtocol, &AcpiNodeInfo, &MemoryDescriptor, sizeof (MemoryDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get data for %a\n", __FUNCTION__, ACPI_I2CT_REG0));
      goto CleanupAndReturn;
    }

    MemoryDescriptor.BaseAddress = RegisterData.BaseAddress;
    MemoryDescriptor.Length      = RegisterData.Size;

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &MemoryDescriptor, sizeof (MemoryDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_I2CT_REG0));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_I2CT_INT0, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_I2CT_INT0));
      goto CleanupAndReturn;
    }

    if (AcpiNodeInfo.Size != sizeof (InterruptDescriptor)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, ACPI_I2CT_INT0, AcpiNodeInfo.Size));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->GetNodeData (PatchProtocol, &AcpiNodeInfo, &InterruptDescriptor, sizeof (InterruptDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get data for %a\n", __FUNCTION__, ACPI_I2CT_INT0));
      goto CleanupAndReturn;
    }

    InterruptDescriptor.InterruptNumber[0] = InterruptData.Interrupt + (InterruptData.Type == INTERRUPT_SPI_TYPE ?
                                                                        DEVICETREE_TO_ACPI_SPI_INTERRUPT_OFFSET :
                                                                        DEVICETREE_TO_ACPI_PPI_INTERRUPT_OFFSET);

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &InterruptDescriptor, sizeof (InterruptDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_I2CT_INT0));
      goto CleanupAndReturn;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, "I2CT", &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, "I2CT"));
      goto CleanupAndReturn;
    }

    AsciiSPrint (I2cPathString, sizeof (I2cPathString), "I2C%u", Index);
    Status = PatchProtocol->UpdateNodeName (PatchProtocol, &AcpiNodeInfo, I2cPathString);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to update name to %a\n", __FUNCTION__, I2cPathString));
      goto CleanupAndReturn;
    }

    Status = GenerationProtocol->AppendDevice (GenerationProtocol, (EFI_ACPI_DESCRIPTION_HEADER *)i2ctemplate_aml_code);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to append device %a\n", __FUNCTION__, I2cPathString));
      goto CleanupAndReturn;
    }
  }

CleanupAndReturn:
  FREE_NON_NULL (I2cHandles);
  return Status;
}
