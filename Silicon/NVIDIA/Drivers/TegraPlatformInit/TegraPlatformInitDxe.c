/** @file

  Tegra Platform Init Driver.

  Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <libfdt.h>

STATIC
VOID
EFIAPI
SetCpuInfoPcdsFromDtb (
  VOID
  )
{
  CONST VOID    *Dtb;
  UINTN         MaxClusters;
  UINTN         MaxCoresPerCluster;
  INT32         CpuMapOffset;
  INT32         Cluster0Offset;
  INT32         NodeOffset;
  CHAR8         ClusterNodeStr[] = "clusterxx";
  CHAR8         CoreNodeStr[] = "corexx";

  Dtb = (CONST VOID *) GetDTBBaseAddress ();

  CpuMapOffset = fdt_path_offset (Dtb, "/cpus/cpu-map");
  if (CpuMapOffset < 0) {
    DEBUG ((DEBUG_ERROR,
            "/cpus/cpu-map missing in DTB, using Clusters=%u, CoresPerCluster=%u\n",
            PcdGet32 (PcdTegraMaxClusters),
            PcdGet32 (PcdTegraMaxCoresPerCluster)));
    return;
  }

  MaxClusters = 1;
  while (TRUE) {
    AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr), "cluster%u", MaxClusters);
    NodeOffset = fdt_subnode_offset(Dtb, CpuMapOffset, ClusterNodeStr);
    if (NodeOffset < 0) {
      break;
    }

    MaxClusters++;
    ASSERT (MaxClusters < 100);     // "clusterxx" max
  }
  DEBUG ((DEBUG_INFO, "MaxClusters=%u\n", MaxClusters));
  PcdSet32S (PcdTegraMaxClusters, MaxClusters);

  // Use cluster0 node to find max core subnode
  Cluster0Offset = fdt_subnode_offset(Dtb, CpuMapOffset, "cluster0");
  if (Cluster0Offset < 0) {
    DEBUG ((DEBUG_ERROR,
            "No cluster0 in DTB, using Clusters=%u, CoresPerCluster=%u\n",
            PcdGet32 (PcdTegraMaxClusters),
            PcdGet32 (PcdTegraMaxCoresPerCluster)));
    return;
  }

  MaxCoresPerCluster = 1;
  while (TRUE) {
    AsciiSPrint (CoreNodeStr, sizeof (CoreNodeStr), "core%u", MaxCoresPerCluster);
    NodeOffset = fdt_subnode_offset(Dtb, Cluster0Offset, CoreNodeStr);
    if (NodeOffset < 0) {
      break;
    }

    MaxCoresPerCluster++;
    ASSERT (MaxCoresPerCluster < 100);     // "corexx" max
  }
  DEBUG ((DEBUG_INFO, "MaxCoresPerCluster=%u\n", MaxCoresPerCluster));
  PcdSet32S (PcdTegraMaxCoresPerCluster, MaxCoresPerCluster);
}

STATIC
EFI_STATUS
EFIAPI
UseEmulatedVariableStore (
  IN EFI_HANDLE        ImageHandle
  )
{
  EFI_STATUS Status;

  PcdSetBoolS(PcdEmuVariableNvModeEnable, TRUE);
  Status = gBS->InstallMultipleProtocolInterfaces (
             &ImageHandle,
             &gEdkiiNvVarStoreFormattedGuid,
             NULL,
             NULL
             );
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing EmuVariableNvModeEnableProtocol\n", __FUNCTION__));
  }

  return Status;
}

/**
  Set up PCDs for multiple Platforms based on DT info
**/
STATIC
VOID
EFIAPI
SetGicInfoPcdsFromDtb (
  IN UINTN ChipID
  )
{
  UINT32                            NumGicControllers;
  UINT32                            GicHandle;
  TEGRA_GIC_INFO                    *GicInfo;
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            RegisterSize;

  GicHandle = 0;
  Status = EFI_SUCCESS;
  RegisterData = NULL;
  GicInfo = NULL;

  GicInfo = (TEGRA_GIC_INFO *) AllocatePool ( sizeof (TEGRA_GIC_INFO));
  if (GicInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  if (!GetGicInfo (GicInfo)) {
    Status = EFI_D_ERROR;
    goto Exit;
  }

  // To set PCDs, begin with a single GIC controller in the DT
  NumGicControllers = 1;

  // Obtain Gic Handle Info
  Status = GetMatchingEnabledDeviceTreeNodes (GicInfo->GicCompatString, &GicHandle, &NumGicControllers);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "No GIC controllers found %r\r\n", Status));
    goto Exit;
  }

  // Obtain Register Info using the Gic Handle
  RegisterSize = 0;
  Status = GetDeviceTreeRegisters (GicHandle, RegisterData, &RegisterSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    if (RegisterData != NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }
    RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *) AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
    if (RegisterData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }
    Status = GetDeviceTreeRegisters (GicHandle, RegisterData, &RegisterSize);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  } else if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (RegisterData == NULL) {
    ASSERT (FALSE);
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  // Set Pcd values by looping through the RegisterSize for each platform

  if (ChipID == T194_CHIP_ID) {
    // RegisterData[0] has Gic Distributor Base and Size
    PcdSet64S (PcdGicDistributorBase, RegisterData[0].BaseAddress);

    // RegisterData[1] has Interrupt Interface Base and Size
    PcdSet64S (PcdGicInterruptInterfaceBase, RegisterData[1].BaseAddress);

    DEBUG ((EFI_D_INFO, "Found GIC distributor and Interrupt Interface Base@ 0x%Lx (0x%Lx)\n",
       PcdGet64 (PcdGicDistributorBase), PcdGet64 (PcdGicInterruptInterfaceBase)));
  } else {
    // RegisterData[0] has Gic Distributor Base and Size
    PcdSet64S (PcdGicDistributorBase, RegisterData[0].BaseAddress);

    // RegisterData[1] has GIC Redistributor Base and Size
    PcdSet64S (PcdGicRedistributorsBase, RegisterData[1].BaseAddress);

    // RegisterData[2] has GicH Base and Size
    // RegisterData[3] has GicV Base and Size

    DEBUG ((EFI_D_INFO, "Found GIC distributor and (re)distributor Base @ 0x%Lx (0x%Lx)\n",
       PcdGet64 (PcdGicDistributorBase), PcdGet64 (PcdGicRedistributorsBase)));
  }

Exit:
  if (RegisterData != NULL) {
    FreePool (RegisterData);
    RegisterData = NULL;
  }
  if (GicInfo != NULL) {
    FreePool (GicInfo);
    GicInfo = NULL;
  }

  return;

}

/**
  Runtime Configuration Of Tegra Platform.
**/
EFI_STATUS
EFIAPI
TegraPlatformInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS              Status;
  UINTN                   ChipID;
  TEGRA_PLATFORM_TYPE     PlatformType;
  UINT64                  DtbBase;
  CONST VOID              *Property;
  INT32                   Length;
  BOOLEAN                 T234SkuSet;
  UINTN                   EmmcMagic;
  BOOLEAN                 EmulatedVariablesUsed;

  EmulatedVariablesUsed = FALSE;

  ChipID = TegraGetChipID();
  DEBUG ((DEBUG_INFO, "%a: Tegra Chip ID:  0x%x\n", __FUNCTION__, ChipID));

  PlatformType = TegraGetPlatform();

  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    if (ChipID == T194_CHIP_ID) {
      LibPcdSetSku (T194_SKU);
    } else if (ChipID == T234_CHIP_ID) {
      T234SkuSet = FALSE;
      DtbBase = GetDTBBaseAddress ();
      Property = fdt_getprop ((CONST VOID*) DtbBase, 0, "model", &Length);
      if (Property != NULL && Length != 0) {
        if (AsciiStrStr (Property, "SLT") != NULL) {
          LibPcdSetSku (T234SLT_SKU);
          T234SkuSet = TRUE;
        }
      }
      if (T234SkuSet == FALSE) {
        LibPcdSetSku (T234_SKU);
      }
    }
  } else {
    // Override boot timeout for pre-si platforms
    EmmcMagic = * ((UINTN *) (TegraGetSystemMemoryBaseAddress(ChipID) + SYSIMG_EMMC_MAGIC_OFFSET));
    if ((EmmcMagic != SYSIMG_EMMC_MAGIC) && (EmmcMagic == SYSIMG_DEFAULT_MAGIC)) {
      EmulatedVariablesUsed = TRUE;
    }
  }

  SetCpuInfoPcdsFromDtb ();

  if (GetBootType () == TegrablBootRcm) {
    EmulatedVariablesUsed = TRUE;
  }

  if (EmulatedVariablesUsed) {
    // Enable emulated variable NV mode in variable driver when ram loading images and emmc
    // is not enabled.
    Status = UseEmulatedVariableStore (ImageHandle);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  // Set Pcds
  SetGicInfoPcdsFromDtb (ChipID);

  return EFI_SUCCESS;
}
