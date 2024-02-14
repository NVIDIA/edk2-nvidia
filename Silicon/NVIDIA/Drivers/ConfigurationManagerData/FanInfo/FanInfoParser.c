/** @file
  Fan info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "FanInfoParser.h"
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/NVIDIADebugLib.h>

/** Fan info parser function.

  The ACPI_FAN_FANR table is potentially patched with fan information

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
FanInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                        Status;
  NVIDIA_AML_GENERATION_PROTOCOL    *GenerationProtocol;
  NVIDIA_AML_PATCH_PROTOCOL         *PatchProtocol;
  INT32                             FanOffset;
  INT32                             PwmOffset;
  UINT32                            FanPwmHandle;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterData;
  UINT32                            Size;
  NVIDIA_AML_NODE_INFO              AcpiNodeInfo;
  UINT8                             FanStatus;
  CONST CHAR8                       *CompatibleInfo[] = { "pwm-fan", NULL };

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

  FanOffset = -1;
  Status    = DeviceTreeGetNextCompatibleNode (CompatibleInfo, &FanOffset);
  if (!EFI_ERROR (Status)) {
    Status = DeviceTreeGetNodePropertyValue32 (FanOffset, "pwms", &FanPwmHandle);
    if (EFI_ERROR (Status)) {
      return EFI_SUCCESS;
    }

    Status = DeviceTreeGetNodeByPHandle (FanPwmHandle, &PwmOffset);
    if (EFI_ERROR (Status) || (PwmOffset < 0)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to find the specified pwms node (phandle 0x%x)\n", __FUNCTION__, Status, FanPwmHandle));
      return EFI_UNSUPPORTED;
    }

    // Only one register space is expected
    Size   = 1;
    Status = DeviceTreeGetRegisters (PwmOffset, &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get pwms registers\n", __FUNCTION__, Status));
      return Status;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_FAN_FANR, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      // If fan node isn't in ACPI return success as there is nothing to patch
      return EFI_SUCCESS;
    }

    if (AcpiNodeInfo.Size > sizeof (RegisterData.Size)) {
      DEBUG ((DEBUG_ERROR, "%a: FANR AcpiNodeInfo.Size = %lu, but expected size of %lu\n", __FUNCTION__, AcpiNodeInfo.Size, sizeof (RegisterData.Size)));
      return EFI_DEVICE_ERROR;
    }

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &RegisterData.BaseAddress, AcpiNodeInfo.Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_FAN_FANR, Status));
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_FAN_STA, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      // If fan node isn't in ACPI return success as there is nothing to patch
      return EFI_SUCCESS;
    }

    if (AcpiNodeInfo.Size > sizeof (FanStatus)) {
      DEBUG ((DEBUG_ERROR, "%a: FAN_STA AcpiNodeInfo.Size = %lu, but expected size of %lu\n", __FUNCTION__, AcpiNodeInfo.Size, sizeof (FanStatus)));
      return EFI_DEVICE_ERROR;
    }

    FanStatus = 0xF;
    Status    = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &FanStatus, sizeof (FanStatus));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_FAN_STA, Status));
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get %a node - Ignoring\n", __FUNCTION__, Status, CompatibleInfo[0]));
    return EFI_NOT_FOUND;
  }

  // Warn if we find more than one fan node
  Status = DeviceTreeGetNextCompatibleNode (CompatibleInfo, &FanOffset);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unexpectedly found more than one %a node. Only the first will be used\n", __FUNCTION__, CompatibleInfo[0]));
  }

  return EFI_SUCCESS;
}
