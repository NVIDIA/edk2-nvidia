/** @file
*
*  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#include <libfdt.h>
#include <Library/DramCarveoutLib.h>
#include <Pi/PiPeiCis.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
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
STATIC
VOID
RegisterDeviceTree (
  IN UINTN BlDtbLoadAddress
  )
{
  EFI_PHYSICAL_ADDRESS *DeviceTreeHobData = NULL;
  UINT64                        DtbNext;
  EFI_STATUS                    Status;
  CHAR8                         SWModule[] = "uefi";

  //Register Device Tree
  if (0 != BlDtbLoadAddress) {
    if (fdt_check_header ((VOID *)BlDtbLoadAddress) == 0) {
      UINTN DtbSize = fdt_totalsize ((VOID *)BlDtbLoadAddress);

      EFI_PHYSICAL_ADDRESS DtbCopy = (EFI_PHYSICAL_ADDRESS)AllocatePages (EFI_SIZE_TO_PAGES (DtbSize * 2));
      if (fdt_open_into ( (VOID *)BlDtbLoadAddress, (VOID *)DtbCopy, 2 * DtbSize) != 0) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to increase device tree size\r\n", __FUNCTION__));
        return;
      }

      DtbNext = ALIGN_VALUE(BlDtbLoadAddress + DtbSize, SIZE_4KB);
      if (fdt_check_header((VOID *)DtbNext) == 0) {
        Status = ApplyTegraDeviceTreeOverlay((VOID *)DtbCopy, (VOID *)DtbNext, SWModule);
        if (EFI_ERROR (Status)) {
          DEBUG ((EFI_D_ERROR, "DTB Overlay failed. Using base DTB.\n"));
          fdt_open_into ( (VOID *)BlDtbLoadAddress, (VOID *)DtbCopy, 2 * DtbSize);
        }
      }

      DeviceTreeHobData = (EFI_PHYSICAL_ADDRESS *)BuildGuidHob ( &gFdtHobGuid, sizeof (EFI_PHYSICAL_ADDRESS));
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
  IN CONST UINTN MemoryBaseAddress,
  IN UINTN MemoryLength
  )
{
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttribute = (EFI_RESOURCE_ATTRIBUTE_PRESENT |
                                                    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
                                                    EFI_RESOURCE_ATTRIBUTE_TESTED
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
  NVDA_MEMORY_REGION *CarveoutRegions,
  UINTN CarveoutRegionsCount
  )
{
  UINTN Index;

  for (Index = 0; Index < CarveoutRegionsCount; Index++) {
    UINTN AddressShift = (CarveoutRegions[Index].MemoryBaseAddress & (SIZE_64KB-1));
    CarveoutRegions[Index].MemoryBaseAddress -= AddressShift;
    CarveoutRegions[Index].MemoryLength = ALIGN_VALUE (CarveoutRegions[Index].MemoryLength + AddressShift, SIZE_64KB);
  }

  return;
}

STATIC
EFI_STATUS
InstallMmioRegions (
  IN UINTN  ChipID,
  OUT UINTN *MmioRegionsCount
)
{
  TEGRA_MMIO_INFO *MmioInfo;

  *MmioRegionsCount += InstallMmioRegion(
                         (TegraGetBLInfoLocationAddress(ChipID) & ~EFI_PAGE_MASK), SIZE_4KB);
  *MmioRegionsCount += InstallMmioRegion(
                         FixedPcdGet64(PcdMiscRegBaseAddress), SIZE_4KB);
  *MmioRegionsCount += InstallMmioRegion(
                         TegraGetGicDistributorBaseAddress(ChipID), SIZE_64KB);
  *MmioRegionsCount += InstallMmioRegion(
                         TegraGetGicInterruptInterfaceBaseAddress(ChipID), SIZE_4KB);
  *MmioRegionsCount += InstallMmioRegion(
                         TegraGetGicRedistributorBaseAddress(ChipID), SIZE_128KB);
  *MmioRegionsCount += InstallMmioRegion(GetTegraUARTBaseAddress (), SIZE_4KB);

  MmioInfo = GetMmioBaseAndSize ();
  if (MmioInfo != NULL) {
    while (MmioInfo->Base != 0 &&
           MmioInfo->Size != 0) {
      *MmioRegionsCount += InstallMmioRegion(
                             MmioInfo->Base, MmioInfo->Size);
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
  OUT UINTN *MemoryRegionsCount
  )
{
  EFI_STATUS           Status;
  UINTN                ChipID;
  NVDA_MEMORY_REGION   DramRegion;
  TEGRA_RESOURCE_INFO  PlatformInfo;
  UINTN                FinalDramRegionsCount;

  if (NULL == MemoryRegionsCount) {
    return EFI_INVALID_PARAMETER;
  }

  *MemoryRegionsCount = 0;

  ChipID = TegraGetChipID();

  //Install MMIO regions
  Status = InstallMmioRegions (ChipID, MemoryRegionsCount);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetResourceConfig (&PlatformInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //Build DRAM regions
  DramRegion.MemoryBaseAddress = TegraGetSystemMemoryBaseAddress(ChipID);
  DramRegion.MemoryLength = PlatformInfo.SdramSize;
  ASSERT (DramRegion.MemoryLength != 0);

  AlignCarveoutRegions64KiB(PlatformInfo.CarveoutRegions, PlatformInfo.CarveoutRegionsCount);

  FinalDramRegionsCount = 0;
  Status = InstallDramWithCarveouts (
             &DramRegion,
             1,
             PlatformInfo.CarveoutRegions,
             PlatformInfo.CarveoutRegionsCount,
             &FinalDramRegionsCount
           );

  if (!EFI_ERROR (Status)) {
    *MemoryRegionsCount += FinalDramRegionsCount;
  }
  FreePool (PlatformInfo.CarveoutRegions);

  RegisterDeviceTree(PlatformInfo.DtbLoadAddress);

  return Status;
}
