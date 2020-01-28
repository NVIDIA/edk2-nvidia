/** @file
*
*  Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <Uefi.h>
#include <libfdt.h>
#include <Uefi/UefiBaseType.h>
#include <Library/DramCarveoutLib.h>
#include <Pi/PiMultiPhase.h>
#include <Pi/PiPeiCis.h>
#include <Library/PrePiLib.h>
#include <Library/DramCarveoutLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Pi/PiHob.h>
#include <Library/SystemResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include "SystemResourceLibPrivate.h"
#include "T234ResourceConfig.h"
#include "TH500ResourceConfig.h"
#include "T194ResourceConfig.h"
#include "T186ResourceConfig.h"


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
  //Register Device Tree
  if (0 != BlDtbLoadAddress) {
    if (fdt_check_header ((VOID *)BlDtbLoadAddress) == 0) {
      UINTN DtbSize = fdt_totalsize ((VOID *)BlDtbLoadAddress);
      EFI_PHYSICAL_ADDRESS DtbCopy = (EFI_PHYSICAL_ADDRESS)AllocatePages (EFI_SIZE_TO_PAGES (DtbSize));
      CopyMem ((VOID *)DtbCopy, (VOID *)BlDtbLoadAddress, DtbSize);

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
  UINTN SerialRegisterBase=0;

  *MmioRegionsCount += InstallMmioRegion(
                         FixedPcdGet64(PcdTegraCombinedUartRxMailbox), SIZE_4KB);
  *MmioRegionsCount += InstallMmioRegion(
                         FixedPcdGet64(PcdTegraCombinedUartTxMailbox), SIZE_4KB);
  *MmioRegionsCount += InstallMmioRegion(
                         FixedPcdGet64(PcdMiscRegBaseAddress), SIZE_4KB);
  *MmioRegionsCount += InstallMmioRegion(
                         TegraGetGicDistributorBaseAddress(ChipID), SIZE_64KB);
  *MmioRegionsCount += InstallMmioRegion(
                         TegraGetGicInterruptInterfaceBaseAddress(ChipID), SIZE_4KB);
  *MmioRegionsCount += InstallMmioRegion(
                         TegraGetGicRedistributorBaseAddress(ChipID), SIZE_128KB);

  switch (ChipID) {
  case T186_CHIP_ID:
    SerialRegisterBase = FixedPcdGet64(PcdTegra16550UartBaseT186);
    break;
  case T194_CHIP_ID:
    SerialRegisterBase = FixedPcdGet64(PcdTegra16550UartBaseT194);
    break;
  case T234_CHIP_ID:
    SerialRegisterBase = FixedPcdGet64(PcdTegra16550UartBaseT234);
    break;
  case TH500_CHIP_ID:
    SerialRegisterBase = FixedPcdGet64(PcdTegra16550UartBaseTH500);
    break;
  default:
    break;
  }
  *MmioRegionsCount += InstallMmioRegion(SerialRegisterBase, SIZE_4KB);
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
  UINTN                FinalDramRegionsCount = 0;
  UINTN                CpuBootloaderAddress;
  UINTN                TegraSystemMemoryBase;

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

  TegraSystemMemoryBase = TegraGetSystemMemoryBaseAddress(ChipID);
  CpuBootloaderAddress = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress(ChipID));
  //Address may be encoded as number of 64KiB pages from 0.
  if (CpuBootloaderAddress < TegraSystemMemoryBase) {
    CpuBootloaderAddress <<= 16;
  }
  ASSERT (((VOID *) CpuBootloaderAddress) != NULL);
  if (((VOID *) CpuBootloaderAddress) == NULL) {
    return EFI_DEVICE_ERROR;
  }

  switch (ChipID) {
  case T186_CHIP_ID:
    Status = T186ResourceConfig(CpuBootloaderAddress, &PlatformInfo);
    break;
  case T194_CHIP_ID:
    Status = T194ResourceConfig(CpuBootloaderAddress, &PlatformInfo);
    break;
  case T234_CHIP_ID:
    Status = T234ResourceConfig(CpuBootloaderAddress, &PlatformInfo);
    break;
  case TH500_CHIP_ID:
    Status = TH500ResourceConfig(CpuBootloaderAddress,&PlatformInfo);
    break;
  default:
    break;
  }

  //Build DRAM regions
  DramRegion.MemoryBaseAddress = TegraSystemMemoryBase;
  DramRegion.MemoryLength = PlatformInfo.SdramSize;
  ASSERT (DramRegion.MemoryLength != 0);

  AlignCarveoutRegions64KiB(PlatformInfo.CarveoutRegions, PlatformInfo.CarveoutRegionsCount);

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

  if (ChipID != TH500_CHIP_ID)
      RegisterDeviceTree(PlatformInfo.DtbLoadAddress);

  return Status;
}
