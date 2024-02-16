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
  UINT32                            FanHandle;
  INT32                             PwmOffset;
  UINT32                            FanPwmHandle;
  VOID                              *DeviceTreeBase;
  UINT32                            PwmHandle;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterData;
  UINT32                            Size;
  NVIDIA_AML_NODE_INFO              AcpiNodeInfo;
  UINT8                             FanStatus;

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

  Size   = 1;
  Status = GetMatchingEnabledDeviceTreeNodes ("pwm-fan", &FanHandle, &Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetDeviceTreeNode (FanHandle, &DeviceTreeBase, &FanOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue32 (FanOffset, "pwms", &FanPwmHandle);
  if (EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  Status = DeviceTreeGetNodeByPHandle (FanPwmHandle, &PwmOffset);
  if (EFI_ERROR (Status) || (PwmOffset < 0)) {
    return EFI_UNSUPPORTED;
  }

  Status = GetDeviceTreeHandle (DeviceTreeBase, PwmOffset, &PwmHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Only one register space is expected
  Size   = 1;
  Status = GetDeviceTreeRegisters (PwmHandle, &RegisterData, &Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, ACPI_FAN_FANR, &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    // If fan node isn't in ACPI return success as there is nothing to patch
    return EFI_SUCCESS;
  }

  if (AcpiNodeInfo.Size > sizeof (RegisterData.Size)) {
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
    return EFI_DEVICE_ERROR;
  }

  FanStatus = 0xF;
  Status    = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &FanStatus, sizeof (FanStatus));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_FAN_STA, Status));
  }

  return Status;
}
