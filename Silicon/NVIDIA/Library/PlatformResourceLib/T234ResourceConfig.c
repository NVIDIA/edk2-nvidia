/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DramCarveoutLib.h>
#include <Library/GoldenRegisterLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MceAriLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/FloorSweepingLib.h>

#include <Protocol/Eeprom.h>

#include <T234/T234Definitions.h>

#include "PlatformResourceConfig.h"
#include "T234ResourceConfigPrivate.h"

#define T234_MAX_CPUS  12

TEGRA_MMIO_INFO  T234MmioInfo[] = {
  {
    T234_GIC_DISTRIBUTOR_BASE,
    SIZE_64KB
  },
  {
    FixedPcdGet64 (PcdTegraCombinedUartTxMailbox),
    SIZE_4KB
  },
  {
    T234_MEMORY_CONTROLLER_BASE,
    SIZE_4KB
  },
  {
    T234_GIC_REDISTRIBUTOR_BASE,
    T234_GIC_REDISTRIBUTOR_INSTANCES *SIZE_128KB
  },
  {
    FixedPcdGet64 (PcdTegraMceAriApertureBaseAddress),
    MCE_ARI_APERTURE_OFFSET (T234_MAX_CPUS)
  },
  {
    T234_FUSE_BASE_ADDRESS,
    SIZE_128KB
  },
  {
    T234_SCRATCH_BASE,
    SIZE_64KB
  },
  {
    FixedPcdGet64 (PcdTegra16550UartBaseT234),
    SIZE_4KB
  },
  // Placeholder for memory in DRAM CO CARVEOUT_DISP_EARLY_BOOT_FB
  // that would be treated as MMIO memory.
  {
    0,
    0
  },
  {
    0,
    0
  }
};

#define T234_FRAME_BUFFER_MMIO_INFO_INDEX  (ARRAY_SIZE (T234MmioInfo) - 2)

TEGRA_FUSE_INFO  T234FloorsweepingFuseList[] = {
  { "fuse-disable-isp",   FUSE_OPT_ISP_DISABLE,   BIT (0)         },
  { "fuse-disable-nvenc", FUSE_OPT_NVENC_DISABLE, BIT (0)|BIT (1) },
  { "fuse-disable-pva",   FUSE_OPT_PVA_DISABLE,   BIT (0)|BIT (1) },
  { "fuse-disable-dla0",  FUSE_OPT_DLA_DISABLE,   BIT (0)         },
  { "fuse-disable-dla1",  FUSE_OPT_DLA_DISABLE,   BIT (1)         },
  { "fuse-disable-cv",    FUSE_OPT_CV_DISABLE,    BIT (0)         },
  { "fuse-disable-nvdec", FUSE_OPT_NVDEC_DISABLE, BIT (0)|BIT (1) }
};

NVDA_MEMORY_REGION  T234DramPageBlacklistInfoAddress[] = {
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
T234BuildDramRegions (
  IN  CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams,
  OUT NVDA_MEMORY_REGION      **CONST  DramRegions,
  OUT UINTN                    *CONST  DramRegionCount
  )
{
  NVDA_MEMORY_REGION  *Regions;
  UINTN               RegionCount;

  CONST BOOLEAN  BlanketDramEnabled =
    CPUBL_PARAMS (CpuBootloaderParams, FeatureFlagData.EnableBlanketNsdramCarveout);

  if (BlanketDramEnabled) {
    DEBUG ((DEBUG_ERROR, "DRAM Encryption Enabled\n"));
    // When blanket dram is enabled, uefi should use only memory in nsdram carveout
    // and interworld shmem carveout.
    RegionCount = 2;
  } else {
    DEBUG ((DEBUG_ERROR, "DRAM Encryption Disabled\n"));
    RegionCount = 1;
  }

  Regions = (NVDA_MEMORY_REGION *)AllocatePool (RegionCount * sizeof (*Regions));
  NV_ASSERT_RETURN (
    Regions != NULL,
    return EFI_DEVICE_ERROR,
    "%a: Failed to allocate %lu DRAM regions\r\n",
    __FUNCTION__,
    (UINT64)RegionCount
    );

  if (BlanketDramEnabled) {
    Regions[0].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_BLANKET_NSDRAM].Base);
    Regions[0].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_BLANKET_NSDRAM].Size);
    Regions[1].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_CCPLEX_INTERWORLD_SHMEM].Base);
    Regions[1].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_CCPLEX_INTERWORLD_SHMEM].Size);
  } else {
    Regions[0].MemoryBaseAddress = TegraGetSystemMemoryBaseAddress (T234_CHIP_ID);
    Regions[0].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, SdramSize);
  }

  *DramRegions     = Regions;
  *DramRegionCount = RegionCount;
  return EFI_SUCCESS;
}

/**
   Adds bootloader carveouts to a memory region list.

   @param[in]     Regions             The list of memory regions.
   @param[in,out] RegionCount         Number of regions in the list.
   @param[in]     UsableRegions       The list of usable memory regions.
   @param[in,out] UsableRegionCount   Number of usable regions in the list.
   @param[in]     BlanketDramEnabled  DRAM encryption enabled?
   @param[in]     Carveouts           Bootloader carveouts.
   @param[in]     CarveoutCount       Number of bootloader carveouts.
*/
STATIC
VOID
T234AddBootloaderCarveouts (
  IN     NVDA_MEMORY_REGION          *CONST  Regions,
  IN OUT UINTN                       *CONST  RegionCount,
  IN     NVDA_MEMORY_REGION          *CONST  UsableRegions,
  IN OUT UINTN                       *CONST  UsableRegionCount,
  IN     CONST BOOLEAN                       BlanketDramEnabled,
  IN     CONST TEGRABL_CARVEOUT_INFO *CONST  Carveouts,
  IN     CONST UINTN                         CarveoutCount
  )
{
  UINTN                 Index;
  EFI_MEMORY_TYPE       MemoryType;
  EFI_PHYSICAL_ADDRESS  Base;
  UINT64                Size, Pages;

  TEGRA_MMIO_INFO *CONST  FrameBufferMmioInfo =
    &T234MmioInfo[T234_FRAME_BUFFER_MMIO_INFO_INDEX];

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
      case CARVEOUT_CCPLEX_INTERWORLD_SHMEM:
      case CARVEOUT_RCM_BLOB:
      case CARVEOUT_OS:
      case CARVEOUT_GR:
      case CARVEOUT_PROFILING:
        // Leave in memory map but marked as used
        if (  (  (Index == CARVEOUT_CCPLEX_INTERWORLD_SHMEM)
              && FixedPcdGetBool (PcdExposeCcplexInterworldShmem)
              && !BlanketDramEnabled)
           || (Index == CARVEOUT_RCM_BLOB))
        {
          MemoryType = EfiBootServicesData;
        } else {
          MemoryType = EfiReservedMemoryType;
        }

        Pages = EFI_SIZE_TO_PAGES (Size);
        BuildMemoryAllocationHob (Base, EFI_PAGES_TO_SIZE (Pages), MemoryType);
        PlatformResourceAddMemoryRegion (UsableRegions, UsableRegionCount, Base, Size);

        break;

      case CARVEOUT_UEFI:
        PlatformResourceAddMemoryRegion (UsableRegions, UsableRegionCount, Base, Size);
        break;

      case CARVEOUT_BLANKET_NSDRAM:
        // Skip CARVEOUT_BLANKET_NSDRAM if blanket dram is enabled as this is a placeholder
        // for BL carveout for BL to program GSC for usable DRAM.
        if (BlanketDramEnabled) {
          continue;
        }

        break;

      case CARVEOUT_DISP_EARLY_BOOT_FB:
        FrameBufferMmioInfo->Base = Base;
        FrameBufferMmioInfo->Size = Size;
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
T234BuildCarveoutRegions (
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

  CONST BOOLEAN  BlanketDramEnabled =
    CPUBL_PARAMS (CpuBootloaderParams, FeatureFlagData.EnableBlanketNsdramCarveout);
  CONST BOOLEAN  DramPageRetirementEnabled =
    CPUBL_PARAMS (CpuBootloaderParams, FeatureFlagData.EnableDramPageRetirement);

  RegionCountMax = UsableRegionCountMax = CARVEOUT_OEM_COUNT;
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

  T234AddBootloaderCarveouts (
    Regions,
    &RegionCount,
    UsableRegions,
    &UsableRegionCount,
    BlanketDramEnabled,
    CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo),
    CARVEOUT_OEM_COUNT
    );

  if (DramPageRetirementEnabled) {
    PlatformResourceAddRetiredDramPageIndices (
      Regions,
      &RegionCount,
      (UINT32 *)CPUBL_PARAMS (CpuBootloaderParams, DramPageRetirementInfoAddress),
      NUM_DRAM_BAD_PAGES,
      SIZE_64KB
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
STATIC
EFI_STATUS
T234GetResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
  )
{
  EFI_STATUS          Status;
  NVDA_MEMORY_REGION  *DramRegions, *CarveoutRegions, *UsableCarveoutRegions;
  UINTN               DramRegionCount, CarveoutRegionCount, UsableCarveoutRegionCount;

  TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams =
    (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  Status = T234BuildDramRegions (
             CpuBootloaderParams,
             &DramRegions,
             &DramRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = T234BuildCarveoutRegions (
             CpuBootloaderParams,
             &CarveoutRegions,
             &CarveoutRegionCount,
             &UsableCarveoutRegions,
             &UsableCarveoutRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PlatformInfo->DtbLoadAddress             = GetDTBBaseAddress ();
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
GetDramPageBlacklistInfoAddress (
  VOID
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;
  UINTN               CpuBootloaderAddress;

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  T234DramPageBlacklistInfoAddress[0].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, DramPageRetirementInfoAddress) & ~EFI_PAGE_MASK;
  T234DramPageBlacklistInfoAddress[0].MemoryLength      = SIZE_64KB;

  return T234DramPageBlacklistInfoAddress;
}

/**
  Retrieve DTB Address

**/
UINT64
GetDTBBaseAddress (
  VOID
  )
{
  UINT64  GrBlobBase;

  GrBlobBase = GetGRBlobBaseAddress ();

  if (ValidateGrBlobHeader (GrBlobBase) == EFI_SUCCESS) {
    return GrBlobBase + GrBlobBinarySize (GrBlobBase);
  }

  return GrBlobBase;
}

/**
  Retrieve GR Blob Address

**/
UINT64
GetGRBlobBaseAddress (
  VOID
  )
{
  TEGRA_CPUBL_PARAMS          *CpuBootloaderParams;
  UINT64                      MemoryBase;
  UINT64                      MemorySize;
  EFI_FIRMWARE_VOLUME_HEADER  *FvHeader;
  UINT64                      FvOffset;
  UINT64                      FvSize;
  UINTN                       CpuBootloaderAddress;

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  MemoryBase          = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_UEFI].Base);
  MemorySize          = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_UEFI].Size);
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
STATIC
TEGRA_MMIO_INFO *
EFIAPI
T234GetMmioBaseAndSize (
  VOID
  )
{
  return T234MmioInfo;
}

/**
  Retrieve EEPROM Data

**/
STATIC
TEGRABL_EEPROM_DATA *
EFIAPI
T234GetEepromData (
  IN  UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return ADDR_OF_CPUBL_PARAMS (CpuBootloaderParams, Eeprom);
}

/**
  Retrieve Board Information

**/
STATIC
BOOLEAN
T234GetBoardInfo (
  IN  UINTN             CpuBootloaderAddress,
  OUT TEGRA_BOARD_INFO  *BoardInfo
  )
{
  TEGRABL_EEPROM_DATA  *EepromData;
  T234_EEPROM_DATA     *T234EepromData;

  EepromData     = T234GetEepromData (CpuBootloaderAddress);
  T234EepromData = (T234_EEPROM_DATA *)EepromData->CvmEepromData;

  BoardInfo->FuseBaseAddr = T234_FUSE_BASE_ADDRESS;
  BoardInfo->FuseList     = T234FloorsweepingFuseList;
  BoardInfo->FuseCount    = sizeof (T234FloorsweepingFuseList) / sizeof (T234FloorsweepingFuseList[0]);
  CopyMem ((VOID *)&BoardInfo->CvmProductId, (VOID *)&T234EepromData->PartNumber, sizeof (T234EepromData->PartNumber));
  CopyMem ((VOID *)BoardInfo->SerialNumber, (VOID *)&T234EepromData->SerialNumber, sizeof (T234EepromData->SerialNumber));

  if ((CompareMem (T234EepromData->CustomerBlockSignature, EEPROM_CUSTOMER_BLOCK_SIGNATURE, sizeof (T234EepromData->CustomerBlockSignature)) == 0) &&
      (CompareMem (T234EepromData->CustomerTypeSignature, EEPROM_CUSTOMER_TYPE_SIGNATURE, sizeof (T234EepromData->CustomerTypeSignature)) == 0))
  {
    CopyMem ((VOID *)BoardInfo->MacAddr, (VOID *)T234EepromData->CustomerEthernetMacAddress, NET_ETHER_ADDR_LEN);
    BoardInfo->NumMacs = T234EepromData->CustomerNumEthernetMacs;
  } else {
    CopyMem ((VOID *)BoardInfo->MacAddr, (VOID *)T234EepromData->EthernetMacAddress, NET_ETHER_ADDR_LEN);
    BoardInfo->NumMacs = T234EepromData->NumEthernetMacs;
  }

  T234EepromData = (T234_EEPROM_DATA *)EepromData->CvbEepromData;
  CopyMem ((VOID *)&BoardInfo->CvbProductId, (VOID *)&T234EepromData->PartNumber, sizeof (T234EepromData->PartNumber));

  return TRUE;
}

/**
  Retrieve Active Boot Chain Information

**/
STATIC
EFI_STATUS
T234GetActiveBootChain (
  IN  UINTN   CpuBootloaderAddress,
  OUT UINT32  *BootChain
  )
{
  *BootChain = MmioBitFieldRead32 (
                 FixedPcdGet64 (PcdBootChainRegisterBaseAddressT234),
                 BOOT_CHAIN_BIT_FIELD_LO,
                 BOOT_CHAIN_BIT_FIELD_HI
                 );

  if (*BootChain >= BOOT_CHAIN_MAX) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Validate Active Boot Chain

**/
EFI_STATUS
ValidateActiveBootChain (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT32      BootChain;
  UINTN       CpuBootloaderAddress;

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  Status = T234GetActiveBootChain (CpuBootloaderAddress, &BootChain);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmioBitFieldWrite32 (
    FixedPcdGet64 (PcdBootChainRegisterBaseAddressT234),
    BootChain,
    BootChain,
    BOOT_CHAIN_GOOD
    );

  return EFI_SUCCESS;
}

/**
  Get UpdateBrBct flag

**/
STATIC
BOOLEAN
T234GetUpdateBrBct (
  IN UINTN  CpuBootloaderAddress
  )
{
  UINT32   Magic;
  BOOLEAN  UpdateBrBct;

  Magic = MmioBitFieldRead32 (
            FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT234),
            BL_MAGIC_BIT_FIELD_LO,
            BL_MAGIC_BIT_FIELD_HI
            );
  if (Magic != SR_BL_MAGIC) {
    DEBUG ((DEBUG_ERROR, "Invalid SR_BL magic=0x%x\n", Magic));
    return FALSE;
  }

  UpdateBrBct = MmioBitFieldRead32 (
                  FixedPcdGet64 (PcdBootLoaderRegisterBaseAddressT234),
                  BL_UPDATE_BR_BCT_BIT_FIELD,
                  BL_UPDATE_BR_BCT_BIT_FIELD
                  );

  DEBUG ((
    DEBUG_INFO,
    "SR_BL Magic=0x%x UpdateBrBct=%u\n",
    Magic,
    UpdateBrBct
    ));

  return UpdateBrBct;
}

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
SocGetEnabledCoresBitMap (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  PlatformResourceInfo->AffinityMpIdrSupported = TRUE;
  return MceAriGetEnabledCoresBitMap (PlatformResourceInfo->EnabledCoresBitMap);
}

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
SocGetPlatformResourceInformation (
  IN UINTN                         CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN BOOLEAN                       InMm
  )
{
  EFI_STATUS          Status;
  BOOLEAN             Result;
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)CpuBootloaderAddress;

  PlatformResourceInfo->SocketMask      = 0x1;
  PlatformResourceInfo->BrBctUpdateFlag = T234GetUpdateBrBct (CpuBootloaderAddress);

  Status = T234GetActiveBootChain (CpuBootloaderAddress, &PlatformResourceInfo->ActiveBootChain);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = T234GetResourceConfig (CpuBootloaderAddress, PlatformResourceInfo->ResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PlatformResourceInfo->MmioInfo = T234GetMmioBaseAndSize ();

  PlatformResourceInfo->EepromData = T234GetEepromData (CpuBootloaderAddress);

  Result = T234GetBoardInfo (CpuBootloaderAddress, PlatformResourceInfo->BoardInfo);
  if (!Result) {
    return EFI_DEVICE_ERROR;
  }

  // Populate RamOops Memory Information
  PlatformResourceInfo->ResourceInfo->RamOopsRegion.MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_RAM_OOPS].Base);
  PlatformResourceInfo->ResourceInfo->RamOopsRegion.MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_RAM_OOPS].Size);

  // Populate Total Memory.
  PlatformResourceInfo->PhysicalDramSize =  CPUBL_PARAMS (CpuBootloaderParams, SdramSize);

  // Populate GrOutputInfo
  PlatformResourceInfo->GrOutputInfo.Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_GR].Base);
  PlatformResourceInfo->GrOutputInfo.Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_GR].Size);

  // Populate FsiNsInfo
  PlatformResourceInfo->FsiNsInfo.Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_FSI_CPU_NS].Base);
  PlatformResourceInfo->FsiNsInfo.Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_FSI_CPU_NS].Size);

  // Populate RcmBlobInfo
  PlatformResourceInfo->RcmBlobInfo.Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_RCM_BLOB].Base);
  PlatformResourceInfo->RcmBlobInfo.Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_RCM_BLOB].Size);

  // Populate PvaFwInfo
  PlatformResourceInfo->PvaFwInfo.Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_PVA_FW].Base);
  PlatformResourceInfo->PvaFwInfo.Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_PVA_FW].Size);

  // Populate FrameBufferInfo
  PlatformResourceInfo->FrameBufferInfo.Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_DISP_EARLY_BOOT_FB].Base);
  PlatformResourceInfo->FrameBufferInfo.Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_DISP_EARLY_BOOT_FB].Size);

  // Populate ProfilerInfo
  PlatformResourceInfo->ProfilerInfo.Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_PROFILING].Base);
  PlatformResourceInfo->ProfilerInfo.Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_PROFILING].Size);

  PlatformResourceInfo->ResourceInfo->XusbRegion.MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_XUSB].Base);
  PlatformResourceInfo->ResourceInfo->XusbRegion.MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_XUSB].Size);

  PlatformResourceInfo->BootType        = CPUBL_PARAMS (CpuBootloaderParams, BootType);
  PlatformResourceInfo->PcieAddressBits = T234_PCIE_ADDRESS_BITS;

  return EFI_SUCCESS;
}

/**
  Get Rootfs Status Register

**/
EFI_STATUS
EFIAPI
GetRootfsStatusReg (
  OUT UINT32  *RegisterValue
  )
{
  *RegisterValue = MmioRead32 (FixedPcdGet64 (PcdRootfsRegisterBaseAddressT234));

  return EFI_SUCCESS;
}

/**
  Set Rootfs Status Register

**/
EFI_STATUS
EFIAPI
SetRootfsStatusReg (
  IN  UINT32  RegisterValue
  )
{
  MmioWrite32 (FixedPcdGet64 (PcdRootfsRegisterBaseAddressT234), RegisterValue);

  return EFI_SUCCESS;
}

/**
  Set next boot chain

**/
EFI_STATUS
EFIAPI
SetNextBootChain (
  IN  UINT32  BootChain
  )
{
  if (BootChain >= BOOT_CHAIN_MAX) {
    return EFI_INVALID_PARAMETER;
  }

  MmioBitFieldWrite32 (
    FixedPcdGet64 (PcdBootChainRegisterBaseAddressT234),
    BOOT_CHAIN_BIT_FIELD_LO,
    BOOT_CHAIN_BIT_FIELD_HI,
    BootChain
    );

  MmioBitFieldWrite32 (
    FixedPcdGet64 (PcdBootChainRegisterBaseAddressT234),
    BootChain,
    BootChain,
    BOOT_CHAIN_GOOD
    );

  return EFI_SUCCESS;
}

/**
  Set next boot into recovery

**/
VOID
EFIAPI
SetNextBootRecovery (
  IN  VOID
  )
{
  MmioBitFieldWrite32 (
    T234_SCRATCH_BASE + SCRATCH_RECOVERY_BOOT_OFFSET,
    RECOVERY_BOOT_BIT,
    RECOVERY_BOOT_BIT,
    1
    );
}

EFI_STATUS
EFIAPI
SocUpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  if (PlatformResourceInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PlatformResourceInfo->VprInfo = &mVprInfo;
  mVprInfo.Base                 =
    ((UINT64)MmioRead32 (T234_MEMORY_CONTROLLER_BASE + MC_VIDEO_PROTECT_BOM_ADR_HI_0) << 32) |
    MmioRead32 (T234_MEMORY_CONTROLLER_BASE + MC_VIDEO_PROTECT_BOM_0);
  mVprInfo.Size =
    (UINT64)MmioRead32 (T234_MEMORY_CONTROLLER_BASE + MC_VIDEO_PROTECT_SIZE_MB_0) << 20;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetActiveBootChainStMm (
  IN  UINTN   ScratchBase,
  OUT UINT32  *BootChain
  )
{
  *BootChain = MmioBitFieldRead32 (
                 ScratchBase + BOOT_CHAIN_SCRATCH_OFFSET,
                 BOOT_CHAIN_BIT_FIELD_LO,
                 BOOT_CHAIN_BIT_FIELD_HI
                 );

  DEBUG ((DEBUG_INFO, "%a: addr=0x%llx bootchain=%u\n", __FUNCTION__, ScratchBase, *BootChain));

  if (*BootChain >= BOOT_CHAIN_MAX) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
GetGicInfo (
  OUT TEGRA_GIC_INFO  *GicInfo
  )
{
  GicInfo->GicCompatString = "arm,gic-v3";
  GicInfo->ItsCompatString = "arm,gic-v3-its";
  GicInfo->Version         = 3;

  return TRUE;
}

UINTN
EFIAPI
TegraGetMaxCoreCount (
  IN UINTN  Socket
  )
{
  UINTN       CoreCount;
  EFI_STATUS  Status;

  Status = GetNumEnabledCoresOnSocket (Socket, &CoreCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:Failed to get Enabled Core Count for Socket %u %r\n", __FUNCTION__, Socket, Status));
  }

  return CoreCount;
}

UINT32
EFIAPI
SocGetSocketMask (
  IN UINTN  CpuBootloaderAddress
  )
{
  return 0x1;
}

EFI_STATUS
EFIAPI
GetPartitionInfo (
  IN  UINT32  PartitionIndex,
  OUT UINT16  *DeviceInstance,
  OUT UINT64  *PartitionStartByte,
  OUT UINT64  *PartitionSizeBytes
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
GetPartitionInfoStMm (
  IN  UINTN   CpuBlAddress,
  IN  UINT32  PartitionIndex,
  OUT UINT16  *DeviceInstance,
  OUT UINT64  *PartitionStartByte,
  OUT UINT64  *PartitionSizeBytes
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
InValidateActiveBootChain (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

UINT32
EFIAPI
PcieIdToInterface (
  IN UINT32  PcieId
  )
{
  NV_ASSERT_RETURN (FALSE, return 0, "%a: not implemented!!!\n", __FUNCTION__);
  return 0;
}

UINT32
EFIAPI
PcieIdToSocket (
  IN UINT32  PcieId
  )
{
  NV_ASSERT_RETURN (FALSE, return 0, "%a: not implemented!!!\n", __FUNCTION__);
  return 0;
}

BOOLEAN
EFIAPI
IsTpmToBeEnabled (
  VOID
  )
{
  return FALSE;
}
