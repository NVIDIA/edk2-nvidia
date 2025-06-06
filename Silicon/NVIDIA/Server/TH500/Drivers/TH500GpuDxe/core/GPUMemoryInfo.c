/** @file

  NVIDIA GPU Memory information support functions.
    Placeholder until PCD, post devinit scratch, fsp query
    or CXL information available

  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

///
/// Libraries
///

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Server/TH500/TH500Definitions.h>

///
/// Protocol(s)
///

#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#include "GPUMemoryInfo.h"

///
/// GPU Memory Information template
///

STATIC CM_ARM_NVDA_GPU_MEMORY_INFO  mGpuMemInfoTemplate =
{
  /* .SegmentNumber */ 0,
  /* .PropertyEntryCount */ MAX_GPU_MEMORY_INFO_PROPERTY_ENTRIES,
  /* .Entry[] */
  {
    /* .PropertyName          , .PropertyValue                                    */
    { "nvidia,gpu-mem-base-pa",                                  0x400000000000    },
    { "nvidia,gpu-mem-pxm-start",                                (16 + ((0) * 8))  },
    { "nvidia,gpu-mem-pxm-count",                                8                 },
    { "nvidia,gpu-mem-size",                                     0x10000000        },
    { "nvidia,egm-base-pa",                                      0                 },
    { "nvidia,egm-size",                                         0                 },
    { "nvidia,egm-pxm",                                          0                 },
    { "nvidia,egm-retired-pages-data-base",                      0                 }
  }
};

/** Returns the PCI Location infromation for the controller

  @param ControllerHandle Controller handle to get PCI Location Information for.
  @param PciLocationInfo  Handle for PciLocationInfo
  @retval Status
            EFI_SUCCESS
            EFI_INVALID_PARAMETER
            (passthrough OpenProtocol)
            (passthrough PciIo->GetLocation)
*/
EFI_STATUS
EFIAPI
GetGPUPciLocation (
  IN EFI_HANDLE          ControllerHandle,
  OUT PCI_LOCATION_INFO  **PciLocationInfo
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo       = NULL;
  PCI_LOCATION_INFO    *PciLocation = NULL;

  DEBUG ((DEBUG_INFO, "%a: ControllerHandle: '%p'\n", __FUNCTION__, ControllerHandle));

  if (NULL == PciLocationInfo) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == ControllerHandle) {
    *PciLocationInfo = NULL;
    return EFI_INVALID_PARAMETER;
  }

  /* Check for installed PciIo Protocol to retrieve PCI Location Information */
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    /* PciIo not present */
    ASSERT (0);
    *PciLocationInfo = NULL;
    return Status;
  }

  PciLocation = AllocatePool (sizeof (PCI_LOCATION_INFO));
  if (NULL == PciLocation) {
    *PciLocationInfo = NULL;
    return EFI_OUT_OF_RESOURCES;
  }

  Status = PciIo->GetLocation (
                    PciIo,
                    &PciLocation->Segment,
                    &PciLocation->Bus,
                    &PciLocation->Device,
                    &PciLocation->Function
                    );

  if (EFI_ERROR (Status)) {
    *PciLocationInfo = NULL;
    FreePool (PciLocation);
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: [PCI S:%04x,B:%02x,D:%02x,F:%02x]\n", __FUNCTION__, (UINT16)PciLocation->Segment, (UINT8)PciLocation->Bus, (UINT8)PciLocation->Device, (UINT8)PciLocation->Function));

  *PciLocationInfo = PciLocation;
  return Status;
}

/* Disable standard SHIM mapping to execute Patched version for testing on tarball TH500 Sim */
#if 1
#define SHIM_GET_GUID_HOB_DATA(Hob)  GET_GUID_HOB_DATA(Hob)
#else
/* Test shim code for grabbing Hob and then adding the EgmMemoryInfo manually. */
STATIC
TEGRA_PLATFORM_RESOURCE_INFO *
EFIAPI
PatchLegacySimPlatformResourceHobData (
  VOID  *Hob
  )
{
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  TEGRA_PLATFORM_RESOURCE_INFO  *FixedPlatformResourceInfo;
  TEGRA_PLATFORM_RESOURCE_INFO  StagingPlatformResourceInfo;
  UINT32                        OldPlatformResourceInfoSize = sizeof (TEGRA_PLATFORM_RESOURCE_INFO) - sizeof (TEGRA_BASE_AND_SIZE_INFO *);
  // Single socke instance for testing on SIM
  TEGRA_BASE_AND_SIZE_INFO  EgmMemoryInfoTemplate[] /*num sockets*/ = {
    { 0x40000000ULL, 0x20000000ULL },
    { 0x60000000ULL, 0x20000000ULL },
    { 0x80000000ULL, 0x20000000ULL },
    { 0xa0000000ULL, 0x20000000ULL }
  };

  DEBUG ((DEBUG_INFO, "%a: Shim EgmMemoryInfo assigned.\n", __FUNCTION__));
  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  CopyMem (&StagingPlatformResourceInfo, PlatformResourceInfo, OldPlatformResourceInfoSize);
  /* Note: these allocations will leak as system side allocation will not be freed and these are imitating those. */
  StagingPlatformResourceInfo.EgmMemoryInfo = AllocateCopyPool (sizeof (TEGRA_BASE_AND_SIZE_INFO), &EgmMemoryInfoTemplate);

  FixedPlatformResourceInfo = AllocateCopyPool (sizeof (TEGRA_PLATFORM_RESOURCE_INFO), &StagingPlatformResourceInfo);
  return FixedPlatformResourceInfo;
}

#define SHIM_GET_GUID_HOB_DATA(Hob)  PatchLegacySimPlatformResourceHobData(Hob)
#endif

/** Allocate and configure GPU Memory Info structure
    @param  ControllerHandle  Controller Handle to retrieve memory information for
    @param  MemInfo           Memory Information structure for the GPU
    @retval Status
            EFI_SUCCESS  Memory Information successful
            EFI_NOT_FOUND Controller information invalid
            EFI_OUT_OF_RESORUCES
*/
EFI_STATUS
EFIAPI
GetGPUMemoryInfo (
  IN EFI_HANDLE        ControllerHandle,
  OUT GPU_MEMORY_INFO  **MemInfo
  )
{
  EFI_STATUS                    Status           = EFI_SUCCESS;
  GPU_MEMORY_INFO               *GpuMemInfo      = NULL;
  PCI_LOCATION_INFO             *PciLocationInfo = NULL;
  UINT8                         index;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  UINT32                        Socket;
  EFI_PHYSICAL_ADDRESS          EgmRetiredPageList;

  if (NULL == MemInfo) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == ControllerHandle) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Status = GetGPUPciLocation (ControllerHandle, &PciLocationInfo);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto errorHandler;
  }

  GpuMemInfo = AllocateCopyPool (
                 sizeof (GPU_MEMORY_INFO),
                 &mGpuMemInfoTemplate
                 );

  if (NULL == GpuMemInfo) {
    Status = EFI_OUT_OF_RESOURCES;
    goto errorHandler;
  }

  /* Fill in GPU specific information */
  GpuMemInfo->SegmentNumber = PciLocationInfo->Segment;
  /* ASCII strings in initial AllocateCopyPool reference data in the static template */
  /* Individually Allocate the ASCII strings and assign */
  for (index = 0; index < MAX_GPU_MEMORY_INFO_PROPERTY_ENTRIES; index++) {
    CONST CHAR8  *PropertyName      = mGpuMemInfoTemplate.Entry[index].PropertyName;
    UINTN        PropertyStringSize = AsciiStrSize (PropertyName);

    GpuMemInfo->Entry[index].PropertyName = AllocateCopyPool (PropertyStringSize, PropertyName);
    if (NULL == GpuMemInfo->Entry[index].PropertyName) {
      Status = EFI_OUT_OF_RESOURCES;
      goto errorHandler;
    }
  }

  if (NULL != ControllerHandle) {
    ATS_RANGE_INFO  ATSRangeInfo;
    EFI_STATUS      StatusLocal;

    StatusLocal = GetControllerATSRangeInfo (ControllerHandle, &ATSRangeInfo);
    if (EFI_ERROR (StatusLocal)) {
      DEBUG ((DEBUG_ERROR, "INFO: 'GetControllerATSRangeInfo' on Handle [%p]. Status = %r.\n", ControllerHandle, StatusLocal));
    } else {
      GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_BASE_PA].PropertyValue = (UINT64)ATSRangeInfo.HbmRangeStart;
      GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_SIZE].PropertyValue    = (UINT64)ATSRangeInfo.HbmRangeSize;
      if (PcdGetBool (PcdGenerateGpuPxmInfoDsd)) {
        GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_START].PropertyValue = ATSRangeInfo.ProximityDomainStart;
        GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_COUNT].PropertyValue = ATSRangeInfo.NumProximityDomains;
      }

      DEBUG_CODE_BEGIN ();
      DEBUG ((DEBUG_INFO, "%a: [%p] ATSRangeInfo.HbmRangeStart: '%lX'\n", __FUNCTION__, ControllerHandle, ATSRangeInfo.HbmRangeStart));
      DEBUG ((DEBUG_INFO, "%a: [%p] ATSRangeInfo.HbmRangeSize: '%lX'\n", __FUNCTION__, ControllerHandle, ATSRangeInfo.HbmRangeSize));
      if (PcdGetBool (PcdGenerateGpuPxmInfoDsd)) {
        DEBUG ((DEBUG_INFO, "%a: [%p] ATSRangeInfo.ProximityDomainStart: '%d'\n", __FUNCTION__, ControllerHandle, ATSRangeInfo.ProximityDomainStart));
        DEBUG ((DEBUG_INFO, "%a: [%p] ATSRangeInfo.NumProximityDomains: '%d'\n", __FUNCTION__, ControllerHandle, ATSRangeInfo.NumProximityDomains));
      }

      DEBUG ((DEBUG_INFO, "%a: [%p] '%a': %lX\n", __FUNCTION__, ControllerHandle, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_BASE_PA].PropertyName, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_BASE_PA].PropertyValue));
      DEBUG ((DEBUG_INFO, "%a: [%p] '%a': %lX\n", __FUNCTION__, ControllerHandle, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_SIZE].PropertyName, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_SIZE].PropertyValue));
      if (PcdGetBool (PcdGenerateGpuPxmInfoDsd)) {
        DEBUG ((DEBUG_INFO, "%a: [%p] '%a': %d\n", __FUNCTION__, ControllerHandle, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_START].PropertyName, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_START].PropertyValue));
        DEBUG ((DEBUG_INFO, "%a: [%p] '%a': %d\n", __FUNCTION__, ControllerHandle, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_COUNT].PropertyName, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_COUNT].PropertyValue));
      }

      DEBUG_CODE_END ();
    }
  }

  /* Adjust size based upon Firmware Initialization detection */
  GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_SIZE].PropertyValue = GetGPUMemSize (ControllerHandle);
  DEBUG ((DEBUG_INFO, "%a: Memsize assigned.\n", __FUNCTION__));

  DEBUG ((DEBUG_INFO, "%a: Hob GUID: '%g'\n", __FUNCTION__, &gNVIDIAPlatformResourceDataGuid));
  // Get EGM info from HOB
  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  DEBUG ((DEBUG_INFO, "%a: Hob: '%p'\n", __FUNCTION__, Hob));

  if (Hob != NULL) {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)SHIM_GET_GUID_HOB_DATA (Hob);
    DEBUG ((DEBUG_INFO, "%a: PlatformResourceInfo: '%p'\n", __FUNCTION__, PlatformResourceInfo));
    if (PlatformResourceInfo->EgmMemoryInfo != NULL) {
      DEBUG ((DEBUG_INFO, "%a: Socket: '%x'\n", __FUNCTION__, ((PciLocationInfo->Segment >> 4) & 0xF)));
      Socket = (PciLocationInfo->Segment >> 4) & 0xF;
      DEBUG_CODE_BEGIN ();
      ASSERT (Socket < 4);
      DEBUG_CODE_END ();
      GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_BASE_PA].PropertyValue = PlatformResourceInfo->EgmMemoryInfo[Socket].Base;
      GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE].PropertyValue    = PlatformResourceInfo->EgmMemoryInfo[Socket].Size;
      GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_PXM].PropertyValue     = Socket;
      if (PlatformResourceInfo->HypervisorMode) {
        GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_PXM].PropertyValue += TH500_HV_EGM_PXM_DOMAIN_START;
      }

      EgmRetiredPageList = 0;
      if ((PlatformResourceInfo->EgmRetiredPages[Socket].Base != 0) &&
          (PlatformResourceInfo->EgmRetiredPages[Socket].Size != 0))
      {
        EgmRetiredPageList = (EFI_PHYSICAL_ADDRESS)AllocateReservedPages (EFI_SIZE_TO_PAGES (PlatformResourceInfo->EgmRetiredPages[Socket].Size));
        CopyMem ((VOID *)EgmRetiredPageList, (CONST VOID *)PlatformResourceInfo->EgmRetiredPages[Socket].Base, PlatformResourceInfo->EgmRetiredPages[Socket].Size);
      }

      if (EgmRetiredPageList == 0) {
        EgmRetiredPageList = (EFI_PHYSICAL_ADDRESS)AllocateReservedPages (EFI_SIZE_TO_PAGES (SIZE_4KB));
        ZeroMem ((VOID *)EgmRetiredPageList, SIZE_4KB);
      }

      GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_RETIRED_PAGES_ADDR].PropertyValue = EgmRetiredPageList;

      DEBUG ((DEBUG_INFO, "%a: [%p] '%a': %lX\n", __FUNCTION__, ControllerHandle, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_BASE_PA].PropertyName, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_BASE_PA].PropertyValue));
      DEBUG ((DEBUG_INFO, "%a: [%p] '%a': %lX\n", __FUNCTION__, ControllerHandle, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE].PropertyName, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE].PropertyValue));
      DEBUG ((DEBUG_INFO, "%a: [%p] '%a': %d\n", __FUNCTION__, ControllerHandle, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_PXM].PropertyName, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_PXM].PropertyValue));
      DEBUG ((DEBUG_INFO, "%a: [%p] '%a': %lX\n", __FUNCTION__, ControllerHandle, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_RETIRED_PAGES_ADDR].PropertyName, GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_RETIRED_PAGES_ADDR].PropertyValue));
    }
  } else {
    // Testing Dummy values values [WIP]
    GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_BASE_PA].PropertyValue = 0x41000000ULL;
    GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE].PropertyValue    = 0x10000000ULL;
    GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_PXM].PropertyValue     = 4;
  }

  *MemInfo = GpuMemInfo;
  return Status;

errorHandler:
  MemInfo = NULL;
  if (NULL != GpuMemInfo) {
    for (index = 0; index < MAX_GPU_MEMORY_INFO_PROPERTY_ENTRIES; index++) {
      /* Free any string allocation that may have been done on error */
      if (NULL != GpuMemInfo->Entry[index].PropertyName) {
        FreePool (GpuMemInfo->Entry[index].PropertyName);
      }
    }

    FreePool (GpuMemInfo);
  }

  return Status;
}

/* Retrieve the ATS infromation from the plaform
    @param[in]    ControllerHandle  Controller Handle to retrieve memory information for
    @param[out]   ATSRangeInfoData  Pointer to ATS Memory Information structure for the GPU
    @retval Status
            EFI_SUCCESS  Memory Information successful
            EFI_NOT_FOUND Controller information invalid
            EFI_OUT_OF_RESORUCES
*/
EFI_STATUS
EFIAPI
GetControllerATSRangeInfo (
  IN EFI_HANDLE       ControllerHandle,
  OUT ATS_RANGE_INFO  *ATSRangeInfoData
  )
{
  EFI_STATUS                                        Status;
  EFI_DEVICE_PATH_PROTOCOL                          *DevicePath;
  EFI_HANDLE                                        ParentHandle;
  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *PciRootBridgeConfigurationIo;

  if (NULL == ATSRangeInfoData) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath,
                  gImageHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Locate a parent controller that supports the NVIDIA Pci Root Bridge Configuration IO Protocol
  Status = gBS->LocateDevicePath (
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  &DevicePath,
                  &ParentHandle
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  ParentHandle,
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  (VOID **)&PciRootBridgeConfigurationIo,
                  gImageHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.Read: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->Read));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.Write: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->Write));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.SegmentNumber: '%08x'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->SegmentNumber));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.MinBusNumber: '%02x'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->MinBusNumber));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.MaxBusNumber: '%02x'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->MaxBusNumber));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.IsExternalFacing: '%d'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->IsExternalFacingPort));
 #if 0 // These are not live on stock TH500 SIM tarball
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.HbmRangeStart: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->HbmRangeStart));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.HbmRangeSize: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->HbmRangeSize));
  if (PcdGetBool (PcdGenerateGpuPxmInfoDsd)) {
    DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.ProximityDomainStart: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->ProximityDomainStart));
    DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.NumProximityDomains: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->NumProximityDomains));
  }

 #endif
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.Read: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->Read));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.Write: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->Write));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.SegmentNumber: '%08x'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->SegmentNumber));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.MinBusNumber: '%02x'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->MinBusNumber));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.MaxBusNumber: '%02x'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->MaxBusNumber));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.IsExternalFacing: '%d'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->IsExternalFacingPort));
 #if 1  // These are not live on stock TH500 SIM tarball, but should be patched by the above call if patching path is enabled.
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.HbmRangeStart: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->HbmRangeStart));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.HbmRangeSize: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->HbmRangeSize));
  if (PcdGetBool (PcdGenerateGpuPxmInfoDsd)) {
    DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.ProximityDomainStart: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->ProximityDomainStart));
    DEBUG ((DEBUG_INFO, "%a: [%p] PciRootBridgeConfigurationIo.NumProximityDomains: '%p'\n", __FUNCTION__, PciRootBridgeConfigurationIo, PciRootBridgeConfigurationIo->NumProximityDomains));
  }

 #endif
  DEBUG_CODE_END ();

  // Retrieve HBM configuration data
  ATSRangeInfoData->HbmRangeStart = PciRootBridgeConfigurationIo->HbmRangeStart;
  ATSRangeInfoData->HbmRangeSize  = PciRootBridgeConfigurationIo->HbmRangeSize;
  if (PcdGetBool (PcdGenerateGpuPxmInfoDsd)) {
    ATSRangeInfoData->ProximityDomainStart = PciRootBridgeConfigurationIo->ProximityDomainStart;
    ATSRangeInfoData->NumProximityDomains  = PciRootBridgeConfigurationIo->NumProximityDomains;
  }

  return EFI_SUCCESS;
}
