/** @file
*
*  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <libfdt.h>
#include <Library/DramCarveoutLib.h>
#include <Pi/PiPeiCis.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/HobLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/SystemResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>

/**
  Register device tree.

  This function copies and registers device tree into the GUID HOB list.

  @param  Physical address of device tree location.
**/
VOID
RegisterDeviceTree (
  IN UINTN  BlDtbLoadAddress
  )
{
  EFI_PHYSICAL_ADDRESS  *DeviceTreeHobData = NULL;
  UINT64                DtbNext;
  EFI_STATUS            Status;
  CHAR8                 SWModule[] = "uefi";
  INT32                 NodeOffset;

  // Register Device Tree
  if (0 != BlDtbLoadAddress) {
    if (fdt_check_header ((VOID *)BlDtbLoadAddress) == 0) {
      UINTN  DtbSize = fdt_totalsize ((VOID *)BlDtbLoadAddress);

      EFI_PHYSICAL_ADDRESS  DtbCopy = (EFI_PHYSICAL_ADDRESS)AllocatePages (EFI_SIZE_TO_PAGES (DtbSize * 4));
      if (fdt_open_into ((VOID *)BlDtbLoadAddress, (VOID *)DtbCopy, 4 * DtbSize) != 0) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to increase device tree size\r\n", __FUNCTION__));
        return;
      }

      DtbNext = ALIGN_VALUE (BlDtbLoadAddress + DtbSize, SIZE_4KB);
      if (fdt_check_header ((VOID *)DtbNext) == 0) {
        Status = ApplyTegraDeviceTreeOverlay ((VOID *)DtbCopy, (VOID *)DtbNext, SWModule);
        if (EFI_ERROR (Status)) {
          DEBUG ((EFI_D_ERROR, "DTB Overlay failed. Using base DTB.\n"));
          fdt_open_into ((VOID *)BlDtbLoadAddress, (VOID *)DtbCopy, 4 * DtbSize);
        }
      }

      NodeOffset = fdt_path_offset ((VOID *)DtbCopy, "/plugin-manager");
      if (NodeOffset >= 0) {
        fdt_del_node ((VOID *)DtbCopy, NodeOffset);
      }

      DeviceTreeHobData = (EFI_PHYSICAL_ADDRESS *)BuildGuidHob (&gFdtHobGuid, sizeof (EFI_PHYSICAL_ADDRESS));
      if (NULL != DeviceTreeHobData) {
        *DeviceTreeHobData = DtbCopy;
      } else {
        DEBUG ((EFI_D_ERROR, "Failed to build guid hob\r\n"));
      }
    }
  }

  return;
}

/**
  Installs MMIO region into the HOB list

  This function install MMIO region into the HOB list.

  @param  Base address of MMIO region to be installed into the Hob list.
  @param  Size of the MMIO region to be installed.

  @retval 0           MMIO region was invalid or not installed
  @retval 1           MMIO region was installed

**/
STATIC
UINTN
InstallMmioRegion (
  IN CONST UINTN  MemoryBaseAddress,
  IN UINTN        MemoryLength
  )
{
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttribute = (EFI_RESOURCE_ATTRIBUTE_PRESENT |
                                                    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
                                                    EFI_RESOURCE_ATTRIBUTE_TESTED    |
                                                    EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE
                                                    );

  if (MemoryBaseAddress == 0) {
    return 0;
  }

  BuildResourceDescriptorHob (
    EFI_RESOURCE_FIRMWARE_DEVICE,
    ResourceAttribute,
    MemoryBaseAddress,
    MemoryLength
    );

  return 1;
}

/**
   Align carveout regions to 64KiB

   This function adjusts carveout to be 64KiB aligned and sized
   to meet UEFI memory map requirements.

   @param  Address of carveout region array.
   @param  Number of carveout regions in the array.
**/
STATIC
VOID
AlignCarveoutRegions64KiB (
  NVDA_MEMORY_REGION  *CarveoutRegions,
  UINTN               CarveoutRegionsCount
  )
{
  UINTN  Index;

  for (Index = 0; Index < CarveoutRegionsCount; Index++) {
    UINTN  AddressShift = (CarveoutRegions[Index].MemoryBaseAddress & (SIZE_64KB-1));
    CarveoutRegions[Index].MemoryBaseAddress -= AddressShift;
    CarveoutRegions[Index].MemoryLength       = ALIGN_VALUE (CarveoutRegions[Index].MemoryLength + AddressShift, SIZE_64KB);
  }

  return;
}

STATIC
EFI_STATUS
InstallMmioRegions (
  IN UINTN   ChipID,
  OUT UINTN  *MmioRegionsCount
  )
{
  VOID             *Hob;
  TEGRA_MMIO_INFO  *MmioInfo;

  *MmioRegionsCount += InstallMmioRegion (
                         (TegraGetBLInfoLocationAddress (ChipID) & ~EFI_PAGE_MASK),
                         SIZE_4KB
                         );
  *MmioRegionsCount += InstallMmioRegion (
                         FixedPcdGet64 (PcdMiscRegBaseAddress),
                         SIZE_4KB
                         );
  *MmioRegionsCount += InstallMmioRegion (
                         TegraGetGicDistributorBaseAddress (ChipID),
                         SIZE_64KB
                         );
  *MmioRegionsCount += InstallMmioRegion (GetTegraUARTBaseAddress (), SIZE_4KB);

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    MmioInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->MmioInfo;
  } else {
    return EFI_DEVICE_ERROR;
  }

  if (MmioInfo != NULL) {
    while (MmioInfo->Base != 0 &&
           MmioInfo->Size != 0)
    {
      *MmioRegionsCount += InstallMmioRegion (
                             MmioInfo->Base,
                             MmioInfo->Size
                             );
      MmioInfo++;
    }
  }

  return EFI_SUCCESS;
}

/**
  Installs resources into the HOB list

  This function install all memory regions into the HOB list.
  This function is called by the platform memory initialization library.

  @param  NumberOfMemoryRegions Number of regions installed into HOB list.

  @retval EFI_SUCCESS           Resources have been installed
  @retval EFI_DEVICE_ERROR      Error setting up memory

**/
EFI_STATUS
InstallSystemResources (
  OUT UINTN  *MemoryRegionsCount
  )
{
  EFI_STATUS           Status;
  UINTN                ChipID;
  TEGRA_RESOURCE_INFO  *PlatformInfo;
  UINTN                FinalDramRegionsCount;
  VOID                 *Hob;
  UINTN                CarveoutSize;

  if (NULL == MemoryRegionsCount) {
    return EFI_INVALID_PARAMETER;
  }

  *MemoryRegionsCount = 0;

  ChipID = TegraGetChipID ();

  // Install MMIO regions
  Status = InstallMmioRegions (ChipID, MemoryRegionsCount);
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ResourceInfo;
  } else {
    return EFI_DEVICE_ERROR;
  }

  PlatformInfo->InputDramRegions = AllocatePool (sizeof (NVDA_MEMORY_REGION) * PlatformInfo->DramRegionsCount);
  NV_ASSERT_RETURN (PlatformInfo->InputDramRegions != NULL, return EFI_DEVICE_ERROR);

  CopyMem (
    PlatformInfo->InputDramRegions,
    PlatformInfo->DramRegions,
    sizeof (NVDA_MEMORY_REGION) * PlatformInfo->DramRegionsCount
    );

  CarveoutSize                       = sizeof (NVDA_MEMORY_REGION) * PlatformInfo->CarveoutRegionsCount;
  PlatformInfo->InputCarveoutRegions = AllocatePages (EFI_SIZE_TO_PAGES (CarveoutSize));
  NV_ASSERT_RETURN (PlatformInfo->InputCarveoutRegions != NULL, return EFI_DEVICE_ERROR);

  CopyMem (
    PlatformInfo->InputCarveoutRegions,
    PlatformInfo->CarveoutRegions,
    sizeof (NVDA_MEMORY_REGION) * PlatformInfo->CarveoutRegionsCount
    );

  AlignCarveoutRegions64KiB (PlatformInfo->CarveoutRegions, PlatformInfo->CarveoutRegionsCount);

  FinalDramRegionsCount = 0;
  Status                = InstallDramWithCarveouts (
                            PlatformInfo->DramRegions,
                            PlatformInfo->DramRegionsCount,
                            PlatformInfo->UefiDramRegionIndex,
                            PlatformInfo->CarveoutRegions,
                            PlatformInfo->CarveoutRegionsCount,
                            &FinalDramRegionsCount
                            );

  if (!EFI_ERROR (Status)) {
    *MemoryRegionsCount += FinalDramRegionsCount;
  }

  return Status;
}
