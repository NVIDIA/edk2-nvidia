/** @file

  Tegra Platform Init Driver.

  SPDX-FileCopyrightText: Copyright (c) 2018-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/DtbUpdateLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/FdtLib.h>
#include <Guid/ImageAuthentication.h>
#include <Guid/RtPropertiesTable.h>
#include <UefiSecureBoot.h>
#include <Library/SecureBootVariableLib.h>
#include <Library/TegraPlatformInfoLib.h>

#define SET_PCD_FROM_DTB(Node, DtbName, Variable) \
do { \
  UINT32  Variable; \
  Status = DeviceTreeGetNodePropertyValue32 (Node, DtbName, &Variable);\
  if (EFI_ERROR (Status)) {\
    DEBUG ((DEBUG_INFO, "%a: Failed to get " #DtbName ": %r\r\n", __FUNCTION__, Status));\
    DEBUG ((DEBUG_INFO, "Pcd: " #Variable " not found, using 0x%X\n", PcdGet32 (Pcd##Variable)));\
  } else {\
    PcdSet32S (Pcd##Variable, Variable); \
    DEBUG ((DEBUG_INFO, "Pcd: " #Variable " = 0x%X\n", PcdGet32 (Pcd##Variable))); \
  }\
} while (0)

#define TH500_PCI_COMPAT  "nvidia,th500-pcie"

STATIC
EFI_STATUS
EFIAPI
UseEmulatedVariableStore (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS               Status;
  EFI_RT_PROPERTIES_TABLE  *RtProperties;

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

  RtProperties = (EFI_RT_PROPERTIES_TABLE *)AllocateRuntimePool (sizeof (EFI_RT_PROPERTIES_TABLE));
  if (RtProperties == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate RT properties table\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  RtProperties->Version                  = EFI_RT_PROPERTIES_TABLE_VERSION;
  RtProperties->Length                   = sizeof (EFI_RT_PROPERTIES_TABLE);
  RtProperties->RuntimeServicesSupported = PcdGet32 (PcdNoVariableRtProperties);
  gBS->InstallConfigurationTable (&gEfiRtPropertiesTableGuid, RtProperties);

  return Status;
}

/**
  Setup PCDs for Oem Table Id based on dtb info
  for Th500
**/
STATIC
EFI_STATUS
EFIAPI
SetOemTableIdPcdForTh500 (
  VOID
  )
{
  UINT32      SocketIndex;
  CHAR8       OemTableIdStr[] = "T241xxx";
  BOOLEAN     GpuPresent;
  EFI_STATUS  Status;
  UINT32      NumberOfPciHandles;
  UINT32      *PciHandles;
  VOID        *DeviceTreeBase;
  INT32       NodeOffset;
  UINT32      SocketCount;
  UINT32      Index;

  GpuPresent         = FALSE;
  NumberOfPciHandles = 0;
  SocketCount        = 0;
  PciHandles         = NULL;

  // Identify if a GPU is present or not
  Status = GetMatchingEnabledDeviceTreeNodes (TH500_PCI_COMPAT, NULL, &NumberOfPciHandles);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    NumberOfPciHandles = 0;
  } else {
    PciHandles = AllocateZeroPool (sizeof (UINT32) * NumberOfPciHandles);
    if (PciHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for cpuidle cores\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    Status = GetMatchingEnabledDeviceTreeNodes (TH500_PCI_COMPAT, PciHandles, &NumberOfPciHandles);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get PCI handles %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  for (Index = 0; Index < NumberOfPciHandles; Index++) {
    GetDeviceTreeNode (PciHandles[Index], &DeviceTreeBase, &NodeOffset);
    if ((NULL != FdtGetProp (DeviceTreeBase, NodeOffset, "c2c-partitions", NULL)) ||
        (NULL != FdtGetProp (DeviceTreeBase, NodeOffset, "hbm-ranges", NULL)))
    {
      GpuPresent = TRUE;
      break;
    }
  }

  // Find socket count in the system
  MPCORE_FOR_EACH_ENABLED_SOCKET (SocketIndex) {
    SocketCount++;
  }

  // Set Pcd for Oem Table ID
  if (GpuPresent) {
    AsciiSPrint (OemTableIdStr, sizeof (OemTableIdStr), "T241x%u", SocketCount);
  } else {
    AsciiSPrint (OemTableIdStr, sizeof (OemTableIdStr), "T241c%u", SocketCount);
  }

  PcdSet64S (PcdAcpiDefaultOemTableId, *(UINT64 *)(OemTableIdStr));

  return EFI_SUCCESS;
}

STATIC
VOID
EFIAPI
SetDistanceLatencyBandwidthInfoPcdsFromDtb (
  IN VOID  *Dtb
  )
{
  EFI_STATUS  Status;
  INT32       AcpiNode;

  Status = DeviceTreeGetNodeByPath ("/firmware/acpi", &AcpiNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get ACPI node: %r\r\n", __FUNCTION__, Status));
    return;
  }

  if (AcpiNode >= 0) {
    // Obtain Distance info
    SET_PCD_FROM_DTB (AcpiNode, "cpu-distance-remotecpu", CpuToRemoteCpuDistance);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-distance-remotegpu", GpuToRemoteGpuDistance);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-distance-localhbm", CpuToLocalHbmDistance);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-distance-remotehbm", CpuToRemoteHbmDistance);
    SET_PCD_FROM_DTB (AcpiNode, "hbm-distance-localcpu", HbmToLocalCpuDistance);
    SET_PCD_FROM_DTB (AcpiNode, "hbm-distance-remotecpu", HbmToRemoteCpuDistance);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-distance-localhbm", GpuToLocalHbmDistance);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-distance-remotehbm", GpuToRemoteHbmDistance);

    // Obtain Latency info
    SET_PCD_FROM_DTB (AcpiNode, "cpu-localcpu-read-latency", CpuToLocalCpuReadLatency);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-localcpu-write-latency", CpuToLocalCpuWriteLatency);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-localhbm-read-latency", CpuToLocalHbmReadLatency);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-localhbm-write-latency", CpuToLocalHbmWriteLatency);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-remotehbm-read-latency", CpuToRemoteHbmReadLatency);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-remotehbm-write-latency", CpuToRemoteHbmWriteLatency);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-remotecpu-read-latency", CpuToRemoteCpuReadLatency);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-remotecpu-write-latency", CpuToRemoteCpuWriteLatency);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-localhbm-read-latency", GpuToLocalHbmReadLatency);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-localhbm-write-latency", GpuToLocalHbmWriteLatency);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-localcpu-read-latency", GpuToLocalCpuReadLatency);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-localcpu-write-latency", GpuToLocalCpuWriteLatency);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-remotehbm-read-latency", GpuToRemoteHbmReadLatency);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-remotehbm-write-latency", GpuToRemoteHbmWriteLatency);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-remotecpu-read-latency", GpuToRemoteCpuReadLatency);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-remotecpu-write-latency", GpuToRemoteCpuWriteLatency);

    // Obtain Bandwidth info
    SET_PCD_FROM_DTB (AcpiNode, "cpu-localcpu-accessbandwidth", CpuToLocalCpuAccessBandwidth);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-localhbm-accessbandwidth", CpuToLocalHbmAccessBandwidth);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-remotecpu-accessbandwidth", CpuToRemoteCpuAccessBandwidth);
    SET_PCD_FROM_DTB (AcpiNode, "cpu-remotehbm-accessbandwidth", CpuToRemoteHbmAccessBandwidth);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-localhbm-accessbandwidth", GpuToLocalHbmAccessBandwidth);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-remotehbm-accessbandwidth", GpuToRemoteHbmAccessBandwidth);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-localcpu-accessbandwidth", GpuToLocalCpuAccessBandwidth);
    SET_PCD_FROM_DTB (AcpiNode, "gpu-remotecpu-accessbandwidth", GpuToRemoteCpuAccessBandwidth);
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
    Status = DEBUG_ERROR;
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

  // RegisterData[0] has Gic Distributor Base and Size
  PcdSet64S (PcdGicDistributorBase, RegisterData[0].BaseAddress);

  // RegisterData[1] has GIC Redistributor Base and Size
  PcdSet64S (PcdGicRedistributorsBase, RegisterData[1].BaseAddress);

  // RegisterData[2] has GicH Base and Size
  // RegisterData[3] has GicV Base and Size

  DEBUG ((
    DEBUG_INFO,
    "Found GIC distributor and (re)distributor Base @ 0x%Lx (0x%Lx)\n",
    PcdGet64 (PcdGicDistributorBase),
    PcdGet64 (PcdGicRedistributorsBase)
    ));

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
  BOOLEAN                       EmulatedVariablesUsed;
  INTN                          NodeOffset;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  EmulatedVariablesUsed = FALSE;

  ChipID = TegraGetChipID ();
  DEBUG ((DEBUG_ERROR, "%a: Tegra Chip ID:  0x%x\n", __FUNCTION__, ChipID));

  PlatformType = TegraGetPlatform ();
  Status       = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    if (ChipID == T234_CHIP_ID) {
      T234SkuSet = FALSE;
      Property   = FdtGetProp (DtbBase, 0, "model", &Length);
      if ((Property != NULL) && (Length != 0)) {
        if (AsciiStrStr (Property, "SLT") != NULL) {
          LibPcdSetSku (T234SLT_SKU);
          T234SkuSet = TRUE;
        }
      }

      if (T234SkuSet == FALSE) {
        LibPcdSetSku (T234_SKU);
      }
    } else if (ChipID == TH500_CHIP_ID) {
      LibPcdSetSku (TH500_SKU);
    } else if (ChipID == T264_CHIP_ID) {
      LibPcdSetSku (T264_SKU);
    } else if (ChipID == T410_CHIP_ID) {
      LibPcdSetSku (T410_SKU);
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ImageHandle,
                    &gNVIDIAIsSiliconDeviceGuid,
                    NULL,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error installing gNVIDIAIsSiliconDeviceGuid\n", __FUNCTION__));
    }
  } else {
    if (ChipID == T234_CHIP_ID) {
      LibPcdSetSku (T234_PRESIL_SKU);
    } else if (ChipID == TH500_CHIP_ID) {
      LibPcdSetSku (TH500_PRESIL_SKU);
    } else if (ChipID == T264_CHIP_ID) {
      LibPcdSetSku (T264_PRESIL_SKU);
    } else if (ChipID == T410_CHIP_ID) {
      LibPcdSetSku (T410_PRESIL_SKU);
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ImageHandle,
                    &gNVIDIAIsPresiliconDeviceGuid,
                    NULL,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error installing gNVIDIAIsPresiliconDeviceGuid\n", __FUNCTION__));
    }
  }

  NodeOffset = FdtPathOffset (DtbBase, "/firmware/uefi");
  if (NodeOffset >= 0) {
    if (NULL != FdtGetProperty (DtbBase, NodeOffset, "use-emulated-variables", NULL)) {
      DEBUG ((DEBUG_ERROR, "Platform Override To Use Emulated Variable Store\n"));
      EmulatedVariablesUsed = TRUE;
    }

    if (NULL != FdtGetProperty (DtbBase, NodeOffset, "use-partition-name-suffixes", NULL)) {
      DEBUG ((DEBUG_INFO, "Using partition name suffixes\n"));

      PcdSetBoolS (PcdPartitionNamesHaveSuffixes, TRUE);
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
      DEBUG ((DEBUG_ERROR, "Do not use STMM as emulated variables are being used.\n"));
      PcdSetBoolS (PcdTegraStmmEnabled, FALSE);
    }
  }

  // Set Pcds
  PcdSet32S (PcdTegraMaxSockets, PlatformResourceInfo->MaxPossibleSockets);
  SetGicInfoPcdsFromDtb (ChipID);

  Status = FloorSweepDtb (DtbBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "DTB floorsweeping failed.\n"));
    ASSERT (FALSE);
    return Status;
  }

  DtbUpdateForUefi (DtbBase);

  SetDistanceLatencyBandwidthInfoPcdsFromDtb (DtbBase);

  if (ChipID == TH500_CHIP_ID) {
    SetOemTableIdPcdForTh500 ();
  }

  return EFI_SUCCESS;
}
