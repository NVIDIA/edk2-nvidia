/** @file

  Tegra Platform Init Driver.

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/HobLib.h>
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
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/FloorSweepingLib.h>
#include <libfdt.h>
#include <Guid/ImageAuthentication.h>
#include <UefiSecureBoot.h>
#include <Library/SecureBootVariableLib.h>
#include <Library/TegraPlatformInfoLib.h>

/**
  Check if the Device is an AGX Xavier Device type.

  @retval TRUE  Device is an AGX Xavier.
  @retval FALSE Not an AGX Xavier Device.

**/
STATIC
BOOLEAN
IsAgxXavier (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT32      NumberOfPlatformNodes;

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,p2972-0000", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,galen", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,e3360_1099", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  return FALSE;
}

STATIC
VOID
SetPhysicalPresencePcd (
  VOID
  )
{
  if ((IsAgxXavier () == TRUE)) {
    DEBUG ((DEBUG_ERROR, "Setting Physical Presence to TRUE\n"));
    PcdSetBoolS (PcdUserPhysicalPresence, TRUE);
  }
}

STATIC
VOID
EFIAPI
SetCpuInfoPcdsFromDtb (
  VOID
  )
{
  VOID        *Dtb;
  UINTN       DtbSize;
  UINTN       MaxClusters;
  UINTN       MaxCoresPerCluster;
  UINTN       MaxSockets;
  INT32       CpuMapOffset;
  INT32       Cluster0Offset;
  INT32       NodeOffset;
  CHAR8       ClusterNodeStr[] = "clusterxxx";
  CHAR8       CoreNodeStr[]    = "corexx";
  EFI_STATUS  Status;
  CHAR8       SocketNodeStr[] = "/socket@xx";
  INT32       SocketOffset;
  CHAR8       CpuMapPathStr[] = "/socket@xx/cpus/cpu-map";
  CHAR8       *CpuMapPathFormat;
  UINTN       Socket;

  Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    return;
  }

  // count number of socket nodes, 100 limit due to socket@xx string
  for (MaxSockets = 0; MaxSockets < 100; MaxSockets++) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", MaxSockets);
    SocketOffset = fdt_path_offset (Dtb, SocketNodeStr);
    if (SocketOffset < 0) {
      break;
    }
  }

  // handle global vs per-socket cpu map
  if (MaxSockets == 0) {
    MaxSockets       = 1;
    CpuMapPathFormat = "/cpus/cpu-map";
  } else {
    CpuMapPathFormat = "/socket@%u/cpus/cpu-map";
  }

  DEBUG ((DEBUG_INFO, "MaxSockets=%u\n", MaxSockets));
  PcdSet32S (PcdTegraMaxSockets, MaxSockets);

  // count clusters across all sockets
  MaxClusters = 0;
  for (Socket = 0; Socket < MaxSockets; Socket++) {
    UINTN  Cluster;

    AsciiSPrint (CpuMapPathStr, sizeof (CpuMapPathStr), CpuMapPathFormat, Socket);
    CpuMapOffset = fdt_path_offset (Dtb, CpuMapPathStr);
    if (CpuMapOffset < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: %a missing in DTB, using Clusters=%u, CoresPerCluster=%u\n",
        __FUNCTION__,
        CpuMapPathStr,
        PcdGet32 (PcdTegraMaxClusters),
        PcdGet32 (PcdTegraMaxCoresPerCluster)
        ));
      return;
    }

    Cluster = 0;
    while (TRUE) {
      AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr), "cluster%u", Cluster);
      NodeOffset = fdt_subnode_offset (Dtb, CpuMapOffset, ClusterNodeStr);
      if (NodeOffset < 0) {
        break;
      }

      MaxClusters++;
      Cluster++;
      ASSERT (Cluster < 1000);    // "clusterxxx" max
    }

    DEBUG ((DEBUG_INFO, "Socket=%u MaxClusters=%u\n", Socket, MaxClusters));
  }

  DEBUG ((DEBUG_INFO, "MaxClusters=%u\n", MaxClusters));
  PcdSet32S (PcdTegraMaxClusters, MaxClusters);

  // Use cluster0 node to find max core subnode
  Cluster0Offset = fdt_subnode_offset (Dtb, CpuMapOffset, "cluster0");
  if (Cluster0Offset < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "No cluster0 in %a, using CoresPerCluster=%u\n",
      CpuMapPathStr,
      PcdGet32 (PcdTegraMaxCoresPerCluster)
      ));
    return;
  }

  MaxCoresPerCluster = 1;
  while (TRUE) {
    AsciiSPrint (CoreNodeStr, sizeof (CoreNodeStr), "core%u", MaxCoresPerCluster);
    NodeOffset = fdt_subnode_offset (Dtb, Cluster0Offset, CoreNodeStr);
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
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  PcdSetBoolS (PcdEmuVariableNvModeEnable, TRUE);
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEdkiiNvVarStoreFormattedGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing EmuVariableNvModeEnableProtocol\n", __FUNCTION__));
  }

  return Status;
}

/**
  Setup PCDs for CPU and GPU domain distance info based on DT
**/
STATIC
VOID
EFIAPI
SetCpuGpuDistanceInfoPcdsFromDtb (
  IN VOID  *Dtb
  )
{
  CONST UINT32  *Property;
  UINT32        CpuToCpuDistance;
  UINT32        GpuToGpuDistance;
  UINT32        CpuToOwnGpuDistance;
  UINT32        CpuToOtherGpuDistance;
  UINT32        GpuToOwnCpuDistance;
  UINT32        GpuToOtherCpuDistance;
  INTN          AcpiNode;

  AcpiNode = fdt_path_offset (Dtb, "/firmware/acpi");
  if (AcpiNode >= 0) {
    // Obtain Distance info
    Property = fdt_getprop (Dtb, AcpiNode, "cpu-distance-cpu", NULL);
    if (Property != NULL) {
      CpuToCpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToCpuDistance, CpuToCpuDistance);
      DEBUG ((EFI_D_INFO, "Cpu To Cpu Distance = 0x%X\n", PcdGet32 (PcdCpuToCpuDistance)));
    } else {
      DEBUG ((DEBUG_ERROR, "Cpu To Cpu Distance not found, using 0x%X\n", PcdGet32 (PcdCpuToCpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-distance-gpu", NULL);
    if (Property != NULL) {
      GpuToGpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToGpuDistance, GpuToGpuDistance);
      DEBUG ((EFI_D_INFO, "Gpu To Gpu Distance = 0x%X\n", PcdGet32 (PcdGpuToGpuDistance)));
    } else {
      DEBUG ((DEBUG_ERROR, "Gpu To Gpu Distance not found, using 0x%X\n", PcdGet32 (PcdGpuToGpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-distance-other-gpu", NULL);
    if (Property != NULL) {
      CpuToOtherGpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToOtherGpuDistance, CpuToOtherGpuDistance);
      DEBUG ((EFI_D_INFO, "Cpu To Other Gpu Distance = 0x%X\n", PcdGet32 (PcdCpuToOtherGpuDistance)));
    } else {
      DEBUG ((DEBUG_ERROR, "Cpu To Other Gpu Distance not found, using 0x%X\n", PcdGet32 (PcdCpuToOtherGpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-distance-own-gpu", NULL);
    if (Property != NULL) {
      CpuToOwnGpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToOwnGpuDistance, CpuToOwnGpuDistance);
      DEBUG ((EFI_D_INFO, "Cpu To Own Gpu Distance = 0x%X\n", PcdGet32 (PcdCpuToOwnGpuDistance)));
    } else {
      DEBUG ((DEBUG_ERROR, "Cpu To Own Gpu Distance not found, using 0x%X\n", PcdGet32 (PcdCpuToOwnGpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-distance-other-cpu", NULL);
    if (Property != NULL) {
      GpuToOtherCpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToOtherCpuDistance, GpuToOtherCpuDistance);
      DEBUG ((EFI_D_INFO, "Gpu To Other Cpu Distance = 0x%X\n", PcdGet32 (PcdGpuToOtherCpuDistance)));
    } else {
      DEBUG ((DEBUG_ERROR, "Gpu To Other Cpu Distance not found, using 0x%X\n", PcdGet32 (PcdGpuToOtherCpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-distance-own-cpu", NULL);
    if (Property != NULL) {
      GpuToOwnCpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToOwnCpuDistance, GpuToOwnCpuDistance);
      DEBUG ((EFI_D_INFO, "Gpu To Own Cpu Distance = 0x%X\n", PcdGet32 (PcdGpuToOwnCpuDistance)));
    } else {
      DEBUG ((DEBUG_ERROR, "Gpu To Own Cpu Distance not found, using 0x%X\n", PcdGet32 (PcdGpuToOwnCpuDistance)));
    }
  }
}

/**
  Set up PCDs for multiple Platforms based on DT info
**/
STATIC
VOID
EFIAPI
SetGicInfoPcdsFromDtb (
  IN UINTN  ChipID
  )
{
  UINT32                            NumGicControllers;
  UINT32                            GicHandle;
  TEGRA_GIC_INFO                    *GicInfo;
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            RegisterSize;

  GicHandle    = 0;
  Status       = EFI_SUCCESS;
  RegisterData = NULL;
  GicInfo      = NULL;

  GicInfo = (TEGRA_GIC_INFO *)AllocatePool (sizeof (TEGRA_GIC_INFO));
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
  Status       = GetDeviceTreeRegisters (GicHandle, RegisterData, &RegisterSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    if (RegisterData != NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
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

    DEBUG ((
      EFI_D_INFO,
      "Found GIC distributor and Interrupt Interface Base@ 0x%Lx (0x%Lx)\n",
      PcdGet64 (PcdGicDistributorBase),
      PcdGet64 (PcdGicInterruptInterfaceBase)
      ));
  } else {
    // RegisterData[0] has Gic Distributor Base and Size
    PcdSet64S (PcdGicDistributorBase, RegisterData[0].BaseAddress);

    // RegisterData[1] has GIC Redistributor Base and Size
    PcdSet64S (PcdGicRedistributorsBase, RegisterData[1].BaseAddress);

    // RegisterData[2] has GicH Base and Size
    // RegisterData[3] has GicV Base and Size

    DEBUG ((
      EFI_D_INFO,
      "Found GIC distributor and (re)distributor Base @ 0x%Lx (0x%Lx)\n",
      PcdGet64 (PcdGicDistributorBase),
      PcdGet64 (PcdGicRedistributorsBase)
      ));
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
  EFI_STATUS                    Status;
  UINTN                         ChipID;
  TEGRA_PLATFORM_TYPE           PlatformType;
  VOID                          *DtbBase;
  UINTN                         DtbSize;
  CONST VOID                    *Property;
  INT32                         Length;
  BOOLEAN                       T234SkuSet;
  UINTN                         EmmcMagic;
  BOOLEAN                       EmulatedVariablesUsed;
  INTN                          UefiNode;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  EmulatedVariablesUsed = FALSE;

  ChipID = TegraGetChipID ();
  DEBUG ((DEBUG_INFO, "%a: Tegra Chip ID:  0x%x\n", __FUNCTION__, ChipID));

  PlatformType = TegraGetPlatform ();
  Status       = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    if (ChipID == T194_CHIP_ID) {
      LibPcdSetSku (T194_SKU);
    } else if (ChipID == T234_CHIP_ID) {
      T234SkuSet = FALSE;
      Property   = fdt_getprop (DtbBase, 0, "model", &Length);
      if ((Property != NULL) && (Length != 0)) {
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
    if (ChipID == T234_CHIP_ID) {
      LibPcdSetSku (T234_PRESIL_SKU);
    }

    // Override boot timeout for pre-si platforms
    EmmcMagic = *((UINTN *)(TegraGetSystemMemoryBaseAddress (ChipID) + SYSIMG_EMMC_MAGIC_OFFSET));
    if ((EmmcMagic != SYSIMG_EMMC_MAGIC) && (EmmcMagic == SYSIMG_DEFAULT_MAGIC)) {
      EmulatedVariablesUsed = TRUE;
    }
  }

  /*TODO: Retaining above logic for backward compatibility. Remove once all DTBs are updated.*/
  UefiNode = fdt_path_offset (DtbBase, "/firmware/uefi");
  if (UefiNode >= 0) {
    if (NULL != fdt_get_property (DtbBase, UefiNode, "use-emulated-variables", NULL)) {
      EmulatedVariablesUsed = TRUE;
    }
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get PlatformResourceInfo\n"));
    return EFI_NOT_FOUND;
  }

  if ((PlatformResourceInfo->BootType == TegrablBootRcm) ||
      (PcdGetBool (PcdEmuVariableNvModeEnable) == TRUE))
  {
    EmulatedVariablesUsed = TRUE;
  }

  if (EmulatedVariablesUsed) {
    // Enable emulated variable NV mode in variable driver when ram loading images and emmc
    // is not enabled.
    Status = UseEmulatedVariableStore (ImageHandle);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (PlatformType == TEGRA_PLATFORM_SILICON) {
      PcdSetBoolS (PcdTegraStmmEnabled, FALSE);
    }
  }

  // Set Pcds
  SetCpuInfoPcdsFromDtb ();
  SetGicInfoPcdsFromDtb (ChipID);
  SetPhysicalPresencePcd ();

  Status = FloorSweepDtb (DtbBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "DTB floorsweeping failed.\n"));
    return Status;
  }

  SetCpuGpuDistanceInfoPcdsFromDtb (DtbBase);

  return EFI_SUCCESS;
}
