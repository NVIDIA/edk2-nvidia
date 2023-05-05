/** @file
*
*  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DramCarveoutLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/GoldenRegisterLib.h>
#include "T194ResourceConfigPrivate.h"
#include "T194ResourceConfig.h"
#include <T194/T194Definitions.h>
#include <Protocol/Eeprom.h>
#include <Library/IoLib.h>
#include <Library/NvgLib.h>

TEGRA_MMIO_INFO  T194MmioInfo[] = {
  {
    FixedPcdGet64 (PcdTegraCombinedUartTxMailbox),
    SIZE_4KB
  },
  {
    FixedPcdGet64 (PcdTegraMCBBaseAddress),
    SIZE_4KB
  },
  {
    T194_GIC_INTERRUPT_INTERFACE_BASE,
    SIZE_4KB
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
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;
  NVDA_MEMORY_REGION  *DramRegions;
  NVDA_MEMORY_REGION  *CarveoutRegions;
  UINTN               CarveoutRegionsCount = 0;
  NVDA_MEMORY_REGION  *UsableCarveoutRegions;
  UINTN               UsableCarveoutRegionsCount = 0;
  UINTN               Index;
  UINT64              *DramPageBlacklistInfo;

  CpuBootloaderParams          = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  PlatformInfo->DtbLoadAddress = CpuBootloaderParams->BlDtbLoadAddress;

  // Build dram regions
  DramRegions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (NVDA_MEMORY_REGION));
  ASSERT (DramRegions != NULL);
  if (DramRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  DramRegions->MemoryBaseAddress    = TegraGetSystemMemoryBaseAddress (T194_CHIP_ID);
  DramRegions->MemoryLength         = CpuBootloaderParams->SdramSize;
  PlatformInfo->DramRegions         = DramRegions;
  PlatformInfo->DramRegionsCount    = 1;
  PlatformInfo->UefiDramRegionIndex = 0;

  // Build Carveout regions
  CarveoutRegions = (NVDA_MEMORY_REGION *)AllocatePool (
                                            (sizeof (NVDA_MEMORY_REGION) * (CARVEOUT_NUM + NUM_DRAM_BAD_PAGES))
                                            );
  ASSERT (CarveoutRegions != NULL);
  if (CarveoutRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  UsableCarveoutRegions = (NVDA_MEMORY_REGION *)AllocatePool (
                                                  sizeof (NVDA_MEMORY_REGION) * CARVEOUT_NUM
                                                  );
  ASSERT (UsableCarveoutRegions != NULL);
  if (UsableCarveoutRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  for (Index = CARVEOUT_NONE; Index < CARVEOUT_NUM; Index++) {
    if ((CpuBootloaderParams->CarveoutInfo[Index].Base == 0) ||
        (CpuBootloaderParams->CarveoutInfo[Index].Size == 0))
    {
      continue;
    }

    DEBUG ((
      EFI_D_ERROR,
      "Carveout %u Region: Base: 0x%016lx, Size: 0x%016lx\n",
      Index,
      CpuBootloaderParams->CarveoutInfo[Index].Base,
      CpuBootloaderParams->CarveoutInfo[Index].Size
      ));
    if (Index == CARVEOUT_MISC) {
      // Leave in memory map but marked as used
      BuildMemoryAllocationHob (
        CpuBootloaderParams->CarveoutInfo[Index].Base,
        EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CpuBootloaderParams->CarveoutInfo[Index].Size)),
        (ValidateGrBlobHeader (GetGRBlobBaseAddress ()) == EFI_SUCCESS) ? EfiReservedMemoryType : EfiBootServicesData
        );
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryBaseAddress = CpuBootloaderParams->CarveoutInfo[Index].Base;
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryLength      = CpuBootloaderParams->CarveoutInfo[Index].Size;
      UsableCarveoutRegionsCount++;
    } else if ((Index == CARVEOUT_CPUBL) ||
               (Index == CARVEOUT_OS) ||
               (Index == CARVEOUT_MB2) ||
               (Index == CARVEOUT_RCM_BLOB))
    {
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryBaseAddress = CpuBootloaderParams->CarveoutInfo[Index].Base;
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryLength      = CpuBootloaderParams->CarveoutInfo[Index].Size;
      UsableCarveoutRegionsCount++;
    }

    CarveoutRegions[CarveoutRegionsCount].MemoryBaseAddress = CpuBootloaderParams->CarveoutInfo[Index].Base;
    CarveoutRegions[CarveoutRegionsCount].MemoryLength      = CpuBootloaderParams->CarveoutInfo[Index].Size;
    CarveoutRegionsCount++;
  }

  if (CpuBootloaderParams->FeatureFlag.EnableDramPageBlacklisting) {
    DramPageBlacklistInfo = (UINT64 *)CpuBootloaderParams->DramPageBlacklistInfoAddress;
    for (Index = 0; Index < NUM_DRAM_BAD_PAGES; Index++) {
      if (DramPageBlacklistInfo[Index] == 0) {
        break;
      } else {
        CarveoutRegions[CarveoutRegionsCount].MemoryBaseAddress = DramPageBlacklistInfo[Index];
        CarveoutRegions[CarveoutRegionsCount].MemoryLength      = SIZE_4KB;
        CarveoutRegionsCount++;
      }
    }
  }

  PlatformInfo->CarveoutRegions            = CarveoutRegions;
  PlatformInfo->CarveoutRegionsCount       = CarveoutRegionsCount;
  PlatformInfo->UsableCarveoutRegions      = UsableCarveoutRegions;
  PlatformInfo->UsableCarveoutRegionsCount = UsableCarveoutRegionsCount;

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
