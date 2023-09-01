/** @file
  GicIts parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "GicParser.h"
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/NVIDIADebugLib.h>

/** GicIts parser function

  The following structure is populated:
  typedef struct CmArmGicItsInfo {
    /// The GIC ITS ID
    UINT32    GicItsId;

    /// The physical address for the Interrupt Translation Service
    UINT64    PhysicalBaseAddress;

    /// The proximity domain to which the logical processor belongs.
    ///  This field is used to populate the GIC ITS affinity structure
    ///  in the SRAT table.
    UINT32    ProximityDomain;
  } CM_ARM_GIC_ITS_INFO;

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
GicItsParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                        Status;
  TEGRA_GIC_INFO                    *GicInfo;
  CM_OBJ_DESCRIPTOR                 Desc;
  UINT32                            GicItsInfoSize;
  UINT32                            NumberOfItsCtlrs;
  UINT32                            NumberOfItsEntries;
  UINT32                            *ItsHandles;
  CM_ARM_GIC_ITS_INFO               *GicItsInfo;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            Index;
  UINT32                            RegisterSize;
  UINT32                            MaxSocket;

  NumberOfItsCtlrs   = 0;
  NumberOfItsEntries = 0;
  GicInfo            = NULL;
  ItsHandles         = NULL;
  RegisterData       = NULL;
  GicItsInfo         = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  // Get GIC Info
  GicInfo = (TEGRA_GIC_INFO *)AllocatePool (sizeof (TEGRA_GIC_INFO));
  if (!GetGicInfo (GicInfo)) {
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  // Redistributor is only relevant for GICv3 and following and is optional
  if ((GicInfo->Version < 3) || (GicInfo->ItsCompatString == NULL)) {
    Status = EFI_SUCCESS;
    goto CleanupAndReturn;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (GicInfo->ItsCompatString, NULL, &NumberOfItsCtlrs);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto CleanupAndReturn;
  }

  ItsHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfItsCtlrs);
  if (ItsHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (GicInfo->ItsCompatString, ItsHandles, &NumberOfItsCtlrs);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  GicItsInfoSize = sizeof (CM_ARM_GIC_ITS_INFO) * NumberOfItsCtlrs;
  GicItsInfo     = (CM_ARM_GIC_ITS_INFO *)AllocateZeroPool (GicItsInfoSize);
  if (GicItsInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = MpCoreInfoGetPlatformInfo (NULL, &MaxSocket, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r getting PlatformInfo\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  RegisterSize = 0;
  for (Index = 0; Index <= MaxSocket; Index++) {
    // check if socket enabled for this Index
    Status = MpCoreInfoGetSocketInfo (Index, NULL, NULL, NULL, NULL);
    if (EFI_ERROR (Status)) {
      if (Status == EFI_NOT_FOUND) {
        continue;
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Got %r getting info about Socket %u\n", __FUNCTION__, Status, Index));
        goto CleanupAndReturn;
      }
    }

    // Obtain Register Info using the ITS Handle
    Status = GetDeviceTreeRegisters (ItsHandles[Index], RegisterData, &RegisterSize);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      FREE_NON_NULL (RegisterData);

      RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
      if (RegisterData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CleanupAndReturn;
      }

      Status = GetDeviceTreeRegisters (ItsHandles[Index], RegisterData, &RegisterSize);
      if (EFI_ERROR (Status)) {
        goto CleanupAndReturn;
      }
    } else if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    if (RegisterSize < 1) {
      Status = EFI_NOT_FOUND;
      goto CleanupAndReturn;
    }

    GicItsInfo[Index].PhysicalBaseAddress = RegisterData[0].BaseAddress;
    GicItsInfo[Index].GicItsId            = Index;

    // Assign socket number
    GicItsInfo[Index].ProximityDomain = Index;

    NumberOfItsEntries++;
    // Check to ensure space allocated for ITS is enough
    ASSERT (NumberOfItsEntries <= NumberOfItsCtlrs);
  }

  // Add the CmObj to the Configuration Manager.
  Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicItsInfo);
  Desc.Size     = GicItsInfoSize;
  Desc.Count    = NumberOfItsCtlrs;
  Desc.Data     = GicItsInfo;

  Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (ItsHandles);
  FREE_NON_NULL (GicInfo);
  FREE_NON_NULL (RegisterData);
  FREE_NON_NULL (GicItsInfo);
  return Status;
}
