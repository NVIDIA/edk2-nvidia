/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DramCarveoutLib.h>
#include <Library/GoldenRegisterLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/NvgLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include <Protocol/Eeprom.h>

#include <T194/T194Definitions.h>

#include "PlatformResourceConfig.h"
#include "T194ResourceConfig.h"
#include "T194ResourceConfigPrivate.h"

TEGRA_MMIO_INFO  T194MmioInfo[] = {
  {
    FixedPcdGet64 (PcdTegraCombinedUartTxMailbox),
    SIZE_4KB
  },
  {
    T194_MEMORY_CONTROLLER_BASE,
    SIZE_4KB
  },
  {
    T194_GIC_INTERRUPT_INTERFACE_BASE,
    SIZE_4KB
  },
  {
    T194_SCRATCH_BASE,
    SIZE_64KB
  },
  {
    0,
    0
  }
};

TEGRA_FUSE_INFO  T194FloorsweepingFuseList[] = {
};

NVDA_MEMORY_REGION  T194DramPageBlacklistInfoAddress[] = {
  {
    0,
    0
  },
  {
    0,
    0
  }
};

STATIC TEGRA_BASE_AND_SIZE_INFO  mVprInfo;

/**
   Builds a list of DRAM memory regions.

   @param[in]  CpuBootloaderParams  CPU BL params.
   @param[out] DramRegions          The list of DRAM regions.
   @param[out] DramRegionCount      Number of DRAM regions in the list.

   @retval EFI_SUCCESS    The list was built successfully.
   @retval !=EFI_SUCCESS  Errors occurred.
*/
STATIC
EFI_STATUS
T194BuildDramRegions (
  IN  CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams,
  OUT NVDA_MEMORY_REGION      **CONST  DramRegions,
  OUT UINTN                    *CONST  DramRegionCount
  )
{
  NVDA_MEMORY_REGION  *Regions;
  CONST UINTN         RegionCount = 1;

  Regions = (NVDA_MEMORY_REGION *)AllocatePool (RegionCount * sizeof (*Regions));
  NV_ASSERT_RETURN (
    Regions != NULL,
    return EFI_DEVICE_ERROR,
    "%a: Failed to allocate %lu DRAM regions\r\n",
    __FUNCTION__,
    (UINT64)RegionCount
    );

  Regions[0].MemoryBaseAddress = TegraGetSystemMemoryBaseAddress (T194_CHIP_ID);
  Regions[0].MemoryLength      = CpuBootloaderParams->SdramSize;

  *DramRegions     = Regions;
  *DramRegionCount = RegionCount;
  return EFI_SUCCESS;
}

/**
   Adds bootloader carveouts to a memory region list.

   @param[in]     Regions            The list of memory regions.
   @param[in,out] RegionCount        Number of regions in the list.
   @param[in]     UsableRegions      The list of usable memory regions.
   @param[in,out] UsableRegionCount  Number of usable regions in the list.
   @param[in]     Carveouts          Bootloader carveouts.
   @param[in]     CarveoutCount      Number of bootloader carveouts.
*/
STATIC
VOID
T194AddBootloaderCarveouts (
  IN     NVDA_MEMORY_REGION          *CONST  Regions,
  IN OUT UINTN                       *CONST  RegionCount,
  IN     NVDA_MEMORY_REGION          *CONST  UsableRegions,
  IN OUT UINTN                       *CONST  UsableRegionCount,
  IN     CONST TEGRABL_CARVEOUT_INFO *CONST  Carveouts,
  IN     CONST UINTN                         CarveoutCount
  )
{
  UINTN                 Index;
  EFI_PHYSICAL_ADDRESS  Base;
  UINT64                Size, Pages;
  EFI_MEMORY_TYPE       MemoryType;

  for (Index = 0; Index < CarveoutCount; Index++) {
    Base = Carveouts[Index].Base;
    Size = Carveouts[Index].Size;

    if ((Base == 0) || (Size == 0)) {
      continue;
    }

    DEBUG ((
      DEBUG_ERROR,
      "Carveout %u Region: Base: 0x%016lx, Size: 0x%016lx\n",
      Index,
      Base,
      Size
      ));

    switch (Index) {
      case CARVEOUT_MISC:
        // Leave in memory map but marked as used
        if (!EFI_ERROR (ValidateGrBlobHeader (GetGRBlobBaseAddress ()))) {
          MemoryType = EfiReservedMemoryType;
        } else {
          MemoryType = EfiBootServicesData;
        }

        Pages = EFI_SIZE_TO_PAGES (Size);
        BuildMemoryAllocationHob (Base, EFI_PAGES_TO_SIZE (Pages), MemoryType);
        PlatformResourceAddMemoryRegion (UsableRegions, UsableRegionCount, Base, Size);
        break;

      case CARVEOUT_CPUBL:
      case CARVEOUT_OS:
      case CARVEOUT_MB2:
      case CARVEOUT_RCM_BLOB:
        PlatformResourceAddMemoryRegion (UsableRegions, UsableRegionCount, Base, Size);
        break;

      default:
        break;
    }

    PlatformResourceAddMemoryRegion (Regions, RegionCount, Base, Size);
  }
}

/**
   Builds a list of carveout memory regions.

   @param[in]  CpuBootloaderParams        CPU BL params.
   @param[out] CarveoutRegions            The list of carveout regions.
   @param[out] CarveoutRegionCount        Number of carveout regions in the list.
   @param[out] UsableCarveoutRegions      The list of usable carveout regions.
   @param[out] UsableCarveoutRegionCount  Number of usable carveout regions in the list.

   @retval EFI_SUCCESS    The list was built successfully.
   @retval !=EFI_SUCCESS  Errors occurred.
*/
STATIC
EFI_STATUS
T194BuildCarveoutRegions (
  IN  CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams,
  OUT NVDA_MEMORY_REGION      **CONST  CarveoutRegions,
  OUT UINTN                    *CONST  CarveoutRegionCount,
  OUT NVDA_MEMORY_REGION      **CONST  UsableCarveoutRegions,
  OUT UINTN                    *CONST  UsableCarveoutRegionCount
  )
{
  NVDA_MEMORY_REGION  *Regions;
  UINTN               RegionCount, RegionCountMax;
  NVDA_MEMORY_REGION  *UsableRegions;
  UINTN               UsableRegionCount, UsableRegionCountMax;

  CONST BOOLEAN  DramPageRetirementEnabled =
    CpuBootloaderParams->FeatureFlag.EnableDramPageBlacklisting;

  RegionCountMax = UsableRegionCountMax = CARVEOUT_NUM;
  if (DramPageRetirementEnabled) {
    RegionCountMax += NUM_DRAM_BAD_PAGES;
  }

  Regions = (NVDA_MEMORY_REGION *)AllocatePool (RegionCountMax * sizeof (*Regions));
  NV_ASSERT_RETURN (
    Regions != NULL,
    return EFI_DEVICE_ERROR,
    "%a: Failed to allocate %lu carveout regions\r\n",
    __FUNCTION__,
    (UINT64)RegionCountMax
    );

  UsableRegions = (NVDA_MEMORY_REGION *)AllocatePool (UsableRegionCountMax * sizeof (*UsableRegions));
  NV_ASSERT_RETURN (
    UsableRegions != NULL,
    return EFI_DEVICE_ERROR,
    "%a: Failed to allocate %lu usable carveout regions\r\n",
    __FUNCTION__,
    (UINT64)UsableRegionCountMax
    );

  RegionCount = UsableRegionCount = 0;

  T194AddBootloaderCarveouts (
    Regions,
    &RegionCount,
    UsableRegions,
    &UsableRegionCount,
    CpuBootloaderParams->CarveoutInfo,
    CARVEOUT_NUM
    );

  if (DramPageRetirementEnabled) {
    PlatformResourceAddRetiredDramPages (
      Regions,
      &RegionCount,
      (EFI_PHYSICAL_ADDRESS *)CpuBootloaderParams->DramPageBlacklistInfoAddress,
      NUM_DRAM_BAD_PAGES,
      SIZE_4KB
      );
  }

  *CarveoutRegions           = Regions;
  *CarveoutRegionCount       = RegionCount;
  *UsableCarveoutRegions     = UsableRegions;
  *UsableCarveoutRegionCount = UsableRegionCount;
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
T194GetResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
  )
{
  EFI_STATUS          Status;
  NVDA_MEMORY_REGION  *DramRegions, *CarveoutRegions, *UsableCarveoutRegions;
  UINTN               DramRegionCount, CarveoutRegionCount, UsableCarveoutRegionCount;

  TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams =
    (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  Status = T194BuildDramRegions (
             CpuBootloaderParams,
             &DramRegions,
             &DramRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = T194BuildCarveoutRegions (
             CpuBootloaderParams,
             &CarveoutRegions,
             &CarveoutRegionCount,
             &UsableCarveoutRegions,
             &UsableCarveoutRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PlatformInfo->DtbLoadAddress             = CpuBootloaderParams->BlDtbLoadAddress;
  PlatformInfo->DramRegions                = DramRegions;
  PlatformInfo->DramRegionsCount           = DramRegionCount;
  PlatformInfo->UefiDramRegionIndex        = 0;
  PlatformInfo->CarveoutRegions            = CarveoutRegions;
  PlatformInfo->CarveoutRegionsCount       = CarveoutRegionCount;
  PlatformInfo->UsableCarveoutRegions      = UsableCarveoutRegions;
  PlatformInfo->UsableCarveoutRegionsCount = UsableCarveoutRegionCount;

  return EFI_SUCCESS;
}

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
T194GetDramPageBlacklistInfoAddress (
  IN  UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  T194DramPageBlacklistInfoAddress[0].MemoryBaseAddress = CpuBootloaderParams->DramPageBlacklistInfoAddress & ~EFI_PAGE_MASK;
  T194DramPageBlacklistInfoAddress[0].MemoryLength      = SIZE_64KB;

  return T194DramPageBlacklistInfoAddress;
}

/**
  Retrieve DTB Address

**/
UINT64
T194GetDTBBaseAddress (
  IN UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return CpuBootloaderParams->BlDtbLoadAddress;
}

/**
  Retrieve GR Blob Address

**/
UINT64
T194GetGRBlobBaseAddress (
  IN UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS          *CpuBootloaderParams;
  UINT64                      MemoryBase;
  UINT64                      MemorySize;
  EFI_FIRMWARE_VOLUME_HEADER  *FvHeader;
  UINT64                      FvOffset;
  UINT64                      FvSize;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  MemoryBase          = CpuBootloaderParams->CarveoutInfo[CARVEOUT_CPUBL].Base;
  MemorySize          = CpuBootloaderParams->CarveoutInfo[CARVEOUT_CPUBL].Size;
  FvHeader            = NULL;
  FvOffset            = 0;

  while (FvOffset < MemorySize) {
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER *)(VOID *)(MemoryBase + FvOffset);
    if (FvHeader->Signature == EFI_FVH_SIGNATURE) {
      break;
    }

    FvOffset += SIZE_64KB;
  }

  ASSERT (FvOffset < MemorySize);
  ASSERT (FvHeader != NULL);
  FvSize = FvHeader->FvLength;
  // Make UEFI FV size aligned to 64KB.
  FvSize = ALIGN_VALUE (FvSize, SIZE_64KB);

  return (UINT64)FvHeader + FvSize;
}

/**
  Retrieve MMIO Base and Size

**/
TEGRA_MMIO_INFO *
EFIAPI
T194GetMmioBaseAndSize (
  VOID
  )
{
  return T194MmioInfo;
}

/**
  Retrieve EEPROM Data

**/
TEGRABL_EEPROM_DATA *
EFIAPI
T194GetEepromData (
  IN  UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return &CpuBootloaderParams->Eeprom;
}

/**
  Retrieve Board Information

**/
BOOLEAN
T194GetBoardInfo (
  IN  UINTN             CpuBootloaderAddress,
  OUT TEGRA_BOARD_INFO  *BoardInfo
  )
{
  TEGRABL_EEPROM_DATA  *EepromData;
  T194_EEPROM_DATA     *T194EepromData;

  EepromData     = T194GetEepromData (CpuBootloaderAddress);
  T194EepromData = (T194_EEPROM_DATA *)EepromData->CvmEepromData;

  BoardInfo->FuseBaseAddr = T194_FUSE_BASE_ADDRESS;
  BoardInfo->FuseList     = T194FloorsweepingFuseList;
  BoardInfo->FuseCount    = sizeof (T194FloorsweepingFuseList) / sizeof (T194FloorsweepingFuseList[0]);
  CopyMem ((VOID *)&BoardInfo->CvmProductId, (VOID *)&T194EepromData->PartNumber, sizeof (T194EepromData->PartNumber));
  CopyMem ((VOID *)BoardInfo->SerialNumber, (VOID *)&T194EepromData->SerialNumber, sizeof (T194EepromData->SerialNumber));

  T194EepromData = (T194_EEPROM_DATA *)EepromData->CvbEepromData;
  CopyMem ((VOID *)&BoardInfo->CvbProductId, (VOID *)&T194EepromData->PartNumber, sizeof (T194EepromData->PartNumber));

  return TRUE;
}

/**
  Validate Boot Chain

**/
BOOLEAN
T194BootChainIsValid (
  IN UINTN  CpuBootloaderAddress
  )
{
  UINT32  RegisterValue;

  RegisterValue = MmioRead32 (FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT194));
  if ((SR_BL_MAGIC_GET (RegisterValue) != SR_BL_MAGIC) ||
      (SR_BL_MAX_SLOTS_GET (RegisterValue) < BOOT_CHAIN_MAX))
  {
    DEBUG ((DEBUG_ERROR, "Invalid SR_BL=0x%x\n", RegisterValue));
    return FALSE;
  }

  return TRUE;
}

/**
  Retrieve Active Boot Chain Information

**/
EFI_STATUS
T194GetActiveBootChain (
  IN  UINTN   CpuBootloaderAddress,
  OUT UINT32  *BootChain
  )
{
  if (T194BootChainIsValid (CpuBootloaderAddress) != TRUE) {
    // No valid slot number is found in scratch register. Return default slot
    *BootChain = BOOT_CHAIN_A;
  } else {
    *BootChain = MmioBitFieldRead32 (
                   FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT194),
                   BL_CURRENT_BOOT_CHAIN_BIT_FIELD_LO,
                   BL_CURRENT_BOOT_CHAIN_BIT_FIELD_HI
                   );
  }

  return EFI_SUCCESS;
}

/**
  Validate Active Boot Chain

**/
EFI_STATUS
T194ValidateActiveBootChain (
  IN  UINTN  CpuBootloaderAddress
  )
{
  EFI_STATUS  Status;
  UINT32      BootChain;

  if (T194BootChainIsValid (CpuBootloaderAddress) != TRUE) {
    // Default case. No need to modify SR register
    return EFI_SUCCESS;
  }

  Status = T194GetActiveBootChain (CpuBootloaderAddress, &BootChain);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmioBitFieldWrite32 (
    FixedPcdGet64 (PcdBootROMRegisterBaseAddressT194),
    BR_CURRENT_BOOT_CHAIN_BIT_FIELD,
    BR_CURRENT_BOOT_CHAIN_BIT_FIELD,
    BootChain
    );

  if (BootChain == BOOT_CHAIN_A) {
    MmioBitFieldWrite32 (
      FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT194),
      BL_BOOT_CHAIN_STATUS_A_BIT_FIELD,
      BL_BOOT_CHAIN_STATUS_A_BIT_FIELD,
      BOOT_CHAIN_GOOD
      );
  } else {
    MmioBitFieldWrite32 (
      FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT194),
      BL_BOOT_CHAIN_STATUS_B_BIT_FIELD,
      BL_BOOT_CHAIN_STATUS_B_BIT_FIELD,
      BOOT_CHAIN_GOOD
      );
  }

  return EFI_SUCCESS;
}

/**
  Get UpdateBrBct flag

**/
BOOLEAN
T194GetUpdateBrBct (
  IN UINTN  CpuBootloaderAddress
  )
{
  if (!T194BootChainIsValid (CpuBootloaderAddress)) {
    return FALSE;
  }

  return MmioBitFieldRead32 (
           FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT194),
           BL_UPDATE_BR_BCT_BIT_FIELD,
           BL_UPDATE_BR_BCT_BIT_FIELD
           );
}

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
T194GetEnabledCoresBitMap (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  PlatformResourceInfo->AffinityMpIdrSupported = FALSE;
  return NvgGetEnabledCoresBitMap (PlatformResourceInfo->EnabledCoresBitMap);
}

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
T194GetPlatformResourceInformation (
  IN UINTN                         CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  EFI_STATUS          Status;
  BOOLEAN             Result;
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  PlatformResourceInfo->SocketMask      = 0x1;
  PlatformResourceInfo->BrBctUpdateFlag = T194GetUpdateBrBct (CpuBootloaderAddress);

  Status = T194GetActiveBootChain (CpuBootloaderAddress, &PlatformResourceInfo->ActiveBootChain);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = T194GetResourceConfig (CpuBootloaderAddress, PlatformResourceInfo->ResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PlatformResourceInfo->MmioInfo = T194GetMmioBaseAndSize ();

  PlatformResourceInfo->EepromData = T194GetEepromData (CpuBootloaderAddress);

  Result = T194GetBoardInfo (CpuBootloaderAddress, PlatformResourceInfo->BoardInfo);
  if (!Result) {
    return EFI_DEVICE_ERROR;
  }

  // Populate RamOops Memory Information
  PlatformResourceInfo->ResourceInfo->RamOopsRegion.MemoryBaseAddress = CpuBootloaderParams->CarveoutInfo[CARVEOUT_RAM_OOPS].Base;
  PlatformResourceInfo->ResourceInfo->RamOopsRegion.MemoryLength      = CpuBootloaderParams->CarveoutInfo[CARVEOUT_RAM_OOPS].Size;

  // Populate GrOutputInfo
  PlatformResourceInfo->GrOutputInfo.Base = CpuBootloaderParams->GoldenRegisterAddress;
  PlatformResourceInfo->GrOutputInfo.Size = CpuBootloaderParams->GoldenRegisterSize;

  // Populate Total Memory.
  PlatformResourceInfo->PhysicalDramSize =  CpuBootloaderParams->SdramSize;

  // Populate RcmBlobInfo
  PlatformResourceInfo->RcmBlobInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_RCM_BLOB].Base;
  PlatformResourceInfo->RcmBlobInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_RCM_BLOB].Size;

  PlatformResourceInfo->BootType = TegrablBootColdBoot;

  return EFI_SUCCESS;
}

/**
  Get Rootfs Status Register Value

**/
EFI_STATUS
EFIAPI
T194GetRootfsStatusReg (
  IN  UINTN   CpuBootloaderAddress,
  OUT UINT32  *RegisterValue
  )
{
  *RegisterValue = MmioRead32 (FixedPcdGet64 (PcdRootfsRegisterBaseAddressT194));

  return EFI_SUCCESS;
}

/**
  Set Rootfs Status Register

**/
EFI_STATUS
EFIAPI
T194SetRootfsStatusReg (
  IN  UINTN   CpuBootloaderAddress,
  IN  UINT32  RegisterValue
  )
{
  MmioWrite32 (FixedPcdGet64 (PcdRootfsRegisterBaseAddressT194), RegisterValue);

  return EFI_SUCCESS;
}

/**
  Set next boot chain

**/
EFI_STATUS
EFIAPI
T194SetNextBootChain (
  IN  UINT32  BootChain
  )
{
  if (BootChain >= BOOT_CHAIN_MAX) {
    return EFI_INVALID_PARAMETER;
  }

  MmioBitFieldWrite32 (
    FixedPcdGet64 (PcdBootROMRegisterBaseAddressT194),
    BR_CURRENT_BOOT_CHAIN_BIT_FIELD,
    BR_CURRENT_BOOT_CHAIN_BIT_FIELD,
    BootChain
    );

  if (BootChain == BOOT_CHAIN_A) {
    MmioBitFieldWrite32 (
      FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT194),
      BL_BOOT_CHAIN_STATUS_A_BIT_FIELD,
      BL_BOOT_CHAIN_STATUS_A_BIT_FIELD,
      BOOT_CHAIN_GOOD
      );
  } else {
    MmioBitFieldWrite32 (
      FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT194),
      BL_BOOT_CHAIN_STATUS_B_BIT_FIELD,
      BL_BOOT_CHAIN_STATUS_B_BIT_FIELD,
      BOOT_CHAIN_GOOD
      );
  }

  return EFI_SUCCESS;
}

/**
  Set next boot into recovery

**/
VOID
EFIAPI
T194SetNextBootRecovery (
  IN  VOID
  )
{
  MmioBitFieldWrite32 (
    T194_SCRATCH_BASE + SCRATCH_RECOVERY_BOOT_OFFSET,
    RECOVERY_BOOT_BIT,
    RECOVERY_BOOT_BIT,
    1
    );
}

EFI_STATUS
EFIAPI
T194UpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  if (PlatformResourceInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PlatformResourceInfo->VprInfo = &mVprInfo;
  mVprInfo.Base                 =
    ((UINT64)MmioRead32 (T194_MEMORY_CONTROLLER_BASE + MC_VIDEO_PROTECT_BOM_ADR_HI_0) << 32) |
    MmioRead32 (T194_MEMORY_CONTROLLER_BASE + MC_VIDEO_PROTECT_BOM_0);
  mVprInfo.Size =
    (UINT64)MmioRead32 (T194_MEMORY_CONTROLLER_BASE + MC_VIDEO_PROTECT_SIZE_MB_0) << 20;

  return EFI_SUCCESS;
}
