/** @file
  GIC MSI Frame parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/NvCmObjectDescUtility.h>
#include "GicParser.h"
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/NVIDIADebugLib.h>

/** GIC MSI Frame parser function

  The following structure is populated:
  typedef struct CmArmGicMsiFrameInfo {
    /// The GIC MSI Frame ID
    UINT32    GicMsiFrameId;

    /// The Physical base address for the MSI Frame
    UINT64    PhysicalBaseAddress;

    /// The GIC MSI Frame flags
    /// as described by the GIC MSI frame
    /// structure in the ACPI Specification.
    UINT32    Flags;

    /// SPI Count used by this frame
    UINT16    SPICount;

    /// SPI Base used by this frame
    UINT16    SPIBase;
  } CM_ARM_GIC_MSI_FRAME_INFO;

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
GicMsiFrameParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                        Status;
  UINT32                            Count;
  UINT32                            Index;
  UINT32                            *Handles;
  CM_ARM_GIC_MSI_FRAME_INFO         *MsiInfo;
  UINT32                            MsiInfoSize;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  Registers[2];
  UINT32                            NumberOfRegisters;
  VOID                              *DeviceTreeBase;
  INT32                             NodeOffset;
  UINT32                            Property;
  CM_OBJ_DESCRIPTOR                 Desc;

  Handles = NULL;
  MsiInfo = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Count  = 0;
  Status = GetMatchingEnabledDeviceTreeNodes ("arm,gic-v2m-frame", NULL, &Count);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return EFI_SUCCESS;
  }

  Handles = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * Count);
  if (Handles == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate device handle array!\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("arm,gic-v2m-frame", Handles, &Count);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to GetMatchingEnabledDeviceTreeNodes - %r!\r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  MsiInfoSize = sizeof (CM_ARM_GIC_MSI_FRAME_INFO) * Count;
  MsiInfo     = (CM_ARM_GIC_MSI_FRAME_INFO *)AllocateZeroPool (MsiInfoSize);
  if (MsiInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate MSI Info array!\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < Count; Index++) {
    NumberOfRegisters = 2;
    Status            = GetDeviceTreeRegisters (Handles[Index], Registers, &NumberOfRegisters);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get registers - %r. Ignoring MSI support\r\n", __FUNCTION__, Status));
      Status = EFI_SUCCESS;
      goto CleanupAndReturn;
    }

    MsiInfo[Index].GicMsiFrameId       = Index;
    MsiInfo[Index].PhysicalBaseAddress = Registers[0].BaseAddress;

    Status = GetDeviceTreeNode (Handles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get device node info - %r. Ignoring MSI support\r\n", __FUNCTION__, Status));
      Status = EFI_SUCCESS;
      goto CleanupAndReturn;
    }

    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "arm,msi-base-spi", &Property);
    if (!EFI_ERROR (Status)) {
      if (Property <= MAX_UINT16) {
        MsiInfo[Index].SPIBase = (UINT16)Property;
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Got invalid MSI SPI base value %u. Ignoring MSI support\n", __FUNCTION__, Property));
        Status = EFI_SUCCESS;
        goto CleanupAndReturn;
      }

      Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "arm,msi-num-spis", &Property);
      if (!EFI_ERROR (Status)) {
        if (Property <= MAX_UINT16) {
          MsiInfo[Index].SPICount = (UINT16)Property;
        } else {
          DEBUG ((DEBUG_ERROR, "%a: Got invalid MSI SPI count value %u. Ignoring MSI support\n", __FUNCTION__, Property));
          Status = EFI_SUCCESS;
          goto CleanupAndReturn;
        }

        MsiInfo[Index].Flags = BIT0;
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Got %r getting \"arm,msi-num-spis\" property for index %u. Ignoring MSI support\n", __FUNCTION__, Status, Index));
        Status = EFI_SUCCESS;
        goto CleanupAndReturn;
      }
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Got %r getting \"arm,msi-base-spi\" property for index %u. Ignoring MSI support\n", __FUNCTION__, Status, Index));
      Status = EFI_SUCCESS;
      goto CleanupAndReturn;
    }
  }

  // Add the CmObj to the Configuration Manager.
  Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicMsiFrameInfo);
  Desc.Size     = MsiInfoSize;
  Desc.Count    = Count;
  Desc.Data     = MsiInfo;

  Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (Handles);
  FREE_NON_NULL (MsiInfo);

  return Status;
}
