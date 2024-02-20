/** @file
  Resource token utility functions.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/PrintLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/NvCmObjectDescUtility.h>

/** Creates Memory Range CM Object.

  Creates and registers a memory region CM object for a device.

  @param [in]  ParserHandle       A handle to the parser instance.
  @param [in]  NodeOffset         Offset of the node in device tree.
  @param [in]  ResourceMax        Maximum number of resources to add to CM object.
                                  0 for unlimited.
  @param [out] MemoryRanges       Optional pointer to return the rangess in the device.
  @param [out] MemoryRangeCount   Optional pointer to return number of ranges
  @param [out] Token              Optional pointer to return the token of the CM object

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
CreateMemoryRangesObject (
  IN  CONST HW_INFO_PARSER_HANDLE           ParserHandle,
  IN        INT32                           NodeOffset,
  IN        INT32                           ResourceMax,
  OUT       CM_ARM_MEMORY_RANGE_DESCRIPTOR  **MemoryRanges OPTIONAL,
  OUT       UINT32                          *MemoryRangeCount OPTIONAL,
  OUT       CM_OBJECT_TOKEN                 *Token OPTIONAL
  )
{
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray;
  UINT32                            NumberOfRegisters;
  UINT32                            ObjectSize;
  UINT32                            Index;
  CM_ARM_MEMORY_RANGE_DESCRIPTOR    *LocalMemoryRanges;
  CM_OBJ_DESCRIPTOR                 *CmObjDesc;

  RegisterArray     = NULL;
  NumberOfRegisters = 0;
  CmObjDesc         = NULL;
  LocalMemoryRanges = NULL;

  Status = DeviceTreeGetRegisters (NodeOffset, RegisterArray, &NumberOfRegisters);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    FREE_NON_NULL (RegisterArray);
    RegisterArray = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * NumberOfRegisters);
    if (RegisterArray == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = DeviceTreeGetRegisters (NodeOffset, RegisterArray, &NumberOfRegisters);
  }

  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (ResourceMax != 0) {
    NumberOfRegisters = MIN (NumberOfRegisters, ResourceMax);
  }

  ObjectSize        = sizeof (CM_ARM_MEMORY_RANGE_DESCRIPTOR) * NumberOfRegisters;
  LocalMemoryRanges = (CM_ARM_MEMORY_RANGE_DESCRIPTOR *)AllocatePool (ObjectSize);
  if (LocalMemoryRanges == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  for (Index = 0; Index < NumberOfRegisters; Index++) {
    LocalMemoryRanges[Index].BaseAddress = RegisterArray[Index].BaseAddress;
    LocalMemoryRanges[Index].Length      = RegisterArray[Index].Size;
  }

  Status = NvCreateCmObjDesc (
             CREATE_CM_ARM_OBJECT_ID (EArmObjMemoryRangeDescriptor),
             NumberOfRegisters,
             LocalMemoryRanges,
             ObjectSize,
             &CmObjDesc
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = NvAddMultipleCmObjGetTokens (
             ParserHandle,
             CmObjDesc,
             NULL,
             Token
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (MemoryRanges != NULL) {
    *MemoryRanges     = LocalMemoryRanges;
    LocalMemoryRanges = NULL;
  }

  if (MemoryRangeCount != NULL) {
    *MemoryRangeCount = NumberOfRegisters;
  }

Exit:
  FREE_NON_NULL (RegisterArray);
  FREE_NON_NULL (LocalMemoryRanges);

  if (CmObjDesc != NULL) {
    NvFreeCmObjDesc (CmObjDesc);
  }

  return Status;
}

/** Creates Interrupts CM Object.

  Creates and registers a interrupts CM object for a device.

  @param [in]  ParserHandle       A handle to the parser instance.
  @param [in]  NodeOffset         Offset of the node in device tree.
  @param [in]  ResourceMax        Maximum number of resources to add to CM object.
                                  0 for unlimited.
  @param [out] Interrupts         Optional pointer to return the interrupts in the device.
  @param [out] InterruptCount     Optional pointer to return number of interrupts
  @param [out] Token              Optional pointer to return the token of the CM object

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
CreateInterruptsObject (
  IN  CONST HW_INFO_PARSER_HANDLE     ParserHandle,
  IN        INT32                     NodeOffset,
  IN        INT32                     ResourceMax,
  OUT       CM_ARM_GENERIC_INTERRUPT  **Interrupts OPTIONAL,
  OUT       UINT32                    *InterruptCount OPTIONAL,
  OUT       CM_OBJECT_TOKEN           *Token OPTIONAL
  )
{
  EFI_STATUS                         Status;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptArray;
  UINT32                             NumberOfInterrupts;
  UINT32                             ObjectSize;
  UINT32                             Index;
  CM_ARM_GENERIC_INTERRUPT           *LocalInterrupts;
  CM_OBJ_DESCRIPTOR                  *CmObjDesc;

  InterruptArray     = NULL;
  NumberOfInterrupts = 0;
  CmObjDesc          = NULL;
  LocalInterrupts    = NULL;

  Status = DeviceTreeGetInterrupts (NodeOffset, InterruptArray, &NumberOfInterrupts);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    FREE_NON_NULL (InterruptArray);
    InterruptArray = (NVIDIA_DEVICE_TREE_INTERRUPT_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_INTERRUPT_DATA) * NumberOfInterrupts);
    if (InterruptArray == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = DeviceTreeGetInterrupts (NodeOffset, InterruptArray, &NumberOfInterrupts);
  }

  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (ResourceMax != 0) {
    NumberOfInterrupts = MIN (NumberOfInterrupts, ResourceMax);
  }

  ObjectSize      = sizeof (CM_ARM_GENERIC_INTERRUPT) * NumberOfInterrupts;
  LocalInterrupts = (CM_ARM_GENERIC_INTERRUPT *)AllocatePool (ObjectSize);
  if (LocalInterrupts == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  for (Index = 0; Index < NumberOfInterrupts; Index++) {
    LocalInterrupts[Index].Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptArray[Index]);
    LocalInterrupts[Index].Flags     = 0;
    if ((InterruptArray[Index].Flag == INTERRUPT_LO_TO_HI_EDGE) ||
        (InterruptArray[Index].Flag == INTERRUPT_HI_TO_LO_EDGE))
    {
      LocalInterrupts[Index].Flags |= BIT0;
    }

    if ((InterruptArray[Index].Flag == INTERRUPT_LO_LEVEL) ||
        (InterruptArray[Index].Flag == INTERRUPT_HI_TO_LO_EDGE))
    {
      LocalInterrupts[Index].Flags |= BIT1;
    }
  }

  Status = NvCreateCmObjDesc (
             CREATE_CM_ARM_OBJECT_ID (EArmObjGenericInterrupt),
             NumberOfInterrupts,
             LocalInterrupts,
             ObjectSize,
             &CmObjDesc
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = NvAddMultipleCmObjGetTokens (
             ParserHandle,
             CmObjDesc,
             NULL,
             Token
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (Interrupts != NULL) {
    *Interrupts     = LocalInterrupts;
    LocalInterrupts = NULL;
  }

  if (InterruptCount != NULL) {
    *InterruptCount = NumberOfInterrupts;
  }

Exit:
  FREE_NON_NULL (InterruptArray);
  FREE_NON_NULL (LocalInterrupts);

  if (CmObjDesc != NULL) {
    NvFreeCmObjDesc (CmObjDesc);
  }

  return Status;
}
