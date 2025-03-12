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
#include <Library/BootChainInfoLib.h>
#include <Library/DramCarveoutLib.h>
#include <Library/GoldenRegisterLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/Eeprom.h>
#include <Library/FloorSweepingLib.h>

#include "PlatformResourceConfig.h"
#include "CommonResourceConfig.h"
#include "SocResourceConfig.h"
#include "T264/T264Definitions.h"

#define T264_MAX_CORE_DISABLE_WORDS  1
#define T264_SOCKET_MASK             0x1

typedef struct {
  CONST CHAR8    *IpName;
  CONST CHAR8    **CompatibilityList;
  CONST CHAR8    *IdProperty;
  UINT64         DisableRegAddr;
  UINT32         DisableRegMask;
  UINT8          DisableRegShift;
  UINT32         DisableRegArray[1];
} T264_FLOOR_SWEEPING_IP_ENTRY;

STATIC TEGRA_FUSE_INFO  T264FuseList[] = {
  { "fuse-prod-mode",  T264_FUSE_PROD_MODE_OFFSET,  T264_FUSE_PROD_MODE_ENABLED },
  { "fuse-ate-priv-1", T264_FUSE_ATE_PRIV_1_OFFSET, BIT0                        },
  { "fuse-ate-priv-2", T264_FUSE_ATE_PRIV_2_OFFSET, BIT0                        },
};

STATIC UINT64  T264FuseBaseAddr[] = {
  T264_FUSE_BASE,
};

STATIC UINT32  T264CoreDisableFuseMask[T264_MAX_CORE_DISABLE_WORDS] = {
  T264_CPU_FLOORSWEEPING_DISABLE_MASK_0,
};

STATIC UINT32  T264CoreDisableFuseOffset[T264_MAX_CORE_DISABLE_WORDS] = {
  T264_CPU_FLOORSWEEPING_DISABLE_OFFSET_0,
};

STATIC COMMON_RESOURCE_CONFIG_INFO  T264CommonResourceConfigInfo = {
  T264_MAX_CORE_DISABLE_WORDS,
  FALSE,
  MAX_UINT32,
  T264FuseBaseAddr,
  T264CoreDisableFuseOffset,
  T264CoreDisableFuseMask,
};

TEGRA_MMIO_INFO  T264MmioInfo[] = {
  {
    T264_GIC_DISTRIBUTOR_BASE,
    SIZE_64KB
  },
  {
    T264_UPHY0_FUSE_BASE,
    SIZE_128KB
  },
  {
    T264_MISC_REG_BASE,
    SIZE_512KB
  },
  // Placeholder for memory in DRAM CO CARVEOUT_CCPLEX_INTERWORLD_SHMEM that would
  // be treated as MMIO memory.
  {
    0,
    0
  },
  {
    0,
    0
  }
};

#define T264_CCPLEX_INTERWORLD_SHMEM_MMIO_INFO_INDEX  (ARRAY_SIZE (T264MmioInfo) - 2)

STATIC TEGRA_MMIO_INFO  T264GicRedistributorMmioInfo[] = {
  {
    T264_GIC_REDISTRIBUTOR_BASE,
    T264_GIC_REDISTRIBUTOR_INSTANCES *SIZE_256KB
  },
};

STATIC TEGRA_MMIO_INFO  T264GicItsMmioInfo[] = {
  {
    T264_GIC_ITS_BASE,
    SIZE_64KB
  },
};

STATIC TEGRA_MMIO_INFO  T264FuseMmioInfo[] = {
  {
    T264_FUSE_BASE,
    SIZE_256KB
  },
};

STATIC TEGRA_MMIO_INFO  T264MemoryControllerMmioInfo[] = {
  {
    T264_MEMORY_CONTROLLER_BASE,
    SIZE_64KB
  },
};

STATIC TEGRA_MMIO_INFO  T264UphyFuseMmioInfo[] = {
  {
    T264_UPHY0_FUSE_BASE,
    SIZE_128KB
  },
};

STATIC TEGRA_MMIO_INFO  T264MiscMmioInfo[] = {
  {
    T264_MISC_REG_BASE,
    SIZE_512KB
  },
};

STATIC TEGRA_MMIO_INFO  T264ScratchMmioInfo[] = {
  {
    T264_SCRATCH_BASE,
    SIZE_64KB
  },
};

STATIC TEGRA_MMIO_INFO  T264FrameBufferMmioInfo[1] = { 0 };

STATIC TEGRA_MMIO_INFO  *T264MmioTables[] = {
  T264GicRedistributorMmioInfo,
  T264GicItsMmioInfo,
  T264FuseMmioInfo,
  T264MemoryControllerMmioInfo,
  T264FrameBufferMmioInfo,
  T264UphyFuseMmioInfo,
  T264MiscMmioInfo,
  T264ScratchMmioInfo,
};

STATIC TEGRA_BASE_AND_SIZE_INFO  mVprInfo[1] = { 0 };

STATIC CONST CHAR8  *T264AudioCompatibility[] = {
  "nvidia,tegra186-audio-graph-card",
  "nvidia,tegra264-aconnect",
  "nvidia,tegra264-hda",
  "nvidia,tegra264-aon",
  NULL
};

STATIC CONST CHAR8  *T264MgbeCompatibility[] = {
  "nvidia,tegra264-mgbe",
  NULL
};
STATIC CONST CHAR8  *T264VicCompatibility[] = {
  "nvidia,tegra264-vic",
  NULL
};
STATIC CONST CHAR8  *T264PvaCompatibility[] = {
  "nvidia,tegra264-pva",
  NULL
};
STATIC CONST CHAR8  *T264DisplayCompatibility[] = {
  "nvidia,tegra264-display",
  "nvidia,tegra264-dce",
  NULL
};

STATIC CONST CHAR8  *T264HwpmCompatibility[] = {
  "nvidia,t264-soc-hwpm",
  NULL
};

STATIC T264_FLOOR_SWEEPING_IP_ENTRY  mT264FloorSweepingIpTable[] = {
  {
    "audio",
    T264AudioCompatibility,
    NULL,
    T264_FUSE_BASE + T264_FUSE_AUDIO_DISABLE_OFFSET,
    T264_FUSE_AUDIO_DISABLE_MASK,
    T264_FUSE_NO_SHIFT,
  },
  {
    "mgbe",
    T264MgbeCompatibility,
    "nvidia,instance_id",
    T264_FUSE_BASE + T264_FUSE_MGBE_DISABLE_OFFSET,
    T264_FUSE_MGBE_DISABLE_MASK,
    T264_FUSE_NO_SHIFT,
  },
  {
    "vic",
    T264VicCompatibility,
    NULL,
    T264_FUSE_BASE + T264_FUSE_VIC_DISABLE_OFFSET,
    T264_FUSE_VIC_DISABLE_MASK,
    T264_FUSE_NO_SHIFT,
  },
  {
    "pva",
    T264PvaCompatibility,
    NULL,
    T264_FUSE_BASE + T264_FUSE_PVA_DISABLE_OFFSET,
    T264_FUSE_PVA_DISABLE_MASK,
    T264_FUSE_NO_SHIFT,
  },
  {
    "display",
    T264DisplayCompatibility,
    NULL,
    T264_FUSE_BASE + T264_FUSE_DISPLAY_DISABLE_OFFSET,
    T264_FUSE_DISPLAY_DISABLE_MASK,
    T264_FUSE_NO_SHIFT,
  },
  {
    NULL,
  }
};

STATIC T264_FLOOR_SWEEPING_IP_ENTRY  mT264FloorSweepingPresilIpTable[] = {
  {
    "hwpm",
    T264HwpmCompatibility,
    NULL,
    T264_FUSE_BASE + T264_FUSE_HWPM_DISABLE_OFFSET,
    T264_FUSE_HWPM_DISABLE_MASK,
    T264_FUSE_NO_SHIFT,
  },
  {
    NULL,
  }
};

NVDA_MEMORY_REGION  T264DramPageBlacklistInfoAddress[] = {
  {
    0,
    0
  },
  // terminating entry
  {
    0,
    0
  }
};

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
EFIAPI
T264BuildDramRegions (
  IN  CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams,
  OUT NVDA_MEMORY_REGION      **CONST  DramRegions,
  OUT UINTN                    *CONST  DramRegionCount
  )
{
  NVDA_MEMORY_REGION  *Regions;

  Regions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (*Regions));
  NV_ASSERT_RETURN (Regions != NULL, return EFI_DEVICE_ERROR, "%a: Failed to allocate DRAM regions\n", __FUNCTION__);

  DEBUG ((
    EFI_D_ERROR,
    "Dram Region: Base: 0x%016lx, Size: 0x%016lx\n",
    CpuBootloaderParams->SdramInfo.Base,
    CpuBootloaderParams->SdramInfo.Size
    ));

  Regions->MemoryBaseAddress = CpuBootloaderParams->SdramInfo.Base;
  Regions->MemoryLength      = CpuBootloaderParams->SdramInfo.Size;

  *DramRegions     = Regions;
  *DramRegionCount = 1;

  return EFI_SUCCESS;
}

/**
  Adds bootloader carveouts to a memory region list.

  @param[in]     Regions             The list of memory regions.
  @param[in,out] RegionCount         Number of regions in the list.
  @param[in]     UsableRegions       The list of usable memory regions.
  @param[in,out] UsableRegionCount   Number of usable regions in the list.
  @param[in]     Carveouts           Bootloader carveouts.
  @param[in]     CarveoutCount       Number of bootloader carveouts.

**/
STATIC
VOID
EFIAPI
T264AddBootloaderCarveouts (
  IN     NVDA_MEMORY_REGION          *CONST  Regions,
  IN OUT UINTN                       *CONST  RegionCount,
  IN     NVDA_MEMORY_REGION          *CONST  UsableRegions,
  IN OUT UINTN                       *CONST  UsableRegionCount,
  IN     CONST TEGRABL_CARVEOUT_INFO *CONST  Carveouts,
  IN     CONST UINTN                         CarveoutCount
  )
{
  UINTN                   Index;
  EFI_MEMORY_TYPE         MemoryType;
  EFI_PHYSICAL_ADDRESS    Base;
  UINT64                  Size, Pages;
  TEGRA_MMIO_INFO *CONST  CcplexInterworldShmemMmioInfo =
    &T264MmioInfo[T264_CCPLEX_INTERWORLD_SHMEM_MMIO_INFO_INDEX];

  for (Index = 0; Index < CarveoutCount; Index++) {
    Base = Carveouts[Index].Base;
    Size = Carveouts[Index].Size;

    if ((Base == 0) || (Size == 0)) {
      continue;
    }

    DEBUG ((DEBUG_ERROR, "Carveout %u Region: Base: 0x%016lx, Size: 0x%016lx\n", Index, Base, Size));

    switch (Index) {
      case CARVEOUT_RCM_BLOB:
      case CARVEOUT_OS:
      case CARVEOUT_GR:
      case CARVEOUT_PROFILING:
      case CARVEOUT_XUSB:
        // Leave in memory map but marked as used
        if ((Index == CARVEOUT_RCM_BLOB)) {
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

      case CARVEOUT_DISP_EARLY_BOOT_FB:
        T264FrameBufferMmioInfo[0].Base = Base;
        T264FrameBufferMmioInfo[0].Size = Size;
        break;

      case CARVEOUT_CCPLEX_INTERWORLD_SHMEM:
        // Add memory in DRAM CO CARVEOUT_CCPLEX_INTERWORLD_SHMEM in its placeholder
        // in T264MmioInfo for MMIO mapping.
        CcplexInterworldShmemMmioInfo->Base = Base;
        CcplexInterworldShmemMmioInfo->Size = Size;
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

**/
STATIC
EFI_STATUS
EFIAPI
T264BuildCarveoutRegions (
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
  CONST BOOLEAN       DramPageRetirementEnabled =
    CpuBootloaderParams->FeatureFlag.EnableDramPageRetirement;

  RegionCountMax = UsableRegionCountMax = CARVEOUT_OEM_COUNT;
  if (DramPageRetirementEnabled) {
    RegionCountMax += TEGRABL_NUM_DRAM_BAD_PAGES;
  }

  Regions = (NVDA_MEMORY_REGION *)AllocatePool (RegionCountMax * sizeof (*Regions));
  NV_ASSERT_RETURN (Regions != NULL, return EFI_DEVICE_ERROR, "%a: Failed to allocate %u carveout regions\n", __FUNCTION__, RegionCountMax);

  UsableRegions = (NVDA_MEMORY_REGION *)AllocatePool (UsableRegionCountMax * sizeof (*UsableRegions));
  NV_ASSERT_RETURN (UsableRegions != NULL, return EFI_DEVICE_ERROR, "%a: Failed to allocate %lu usable carveout regions\n", __FUNCTION__, UsableRegionCountMax);

  RegionCount = UsableRegionCount = 0;
  T264AddBootloaderCarveouts (
    Regions,
    &RegionCount,
    UsableRegions,
    &UsableRegionCount,
    CpuBootloaderParams->CarveoutInfo,
    CARVEOUT_OEM_COUNT
    );

  if (DramPageRetirementEnabled) {
    PlatformResourceAddRetiredDramPageIndices (
      Regions,
      &RegionCount,
      (UINT32 *)(CpuBootloaderParams->DramPageRetirementAddress),
      TEGRABL_NUM_DRAM_BAD_PAGES,
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

  @param  NumberOfMemoryRegions Number of regions installed into HOB list.

  @retval EFI_SUCCESS           Resources have been installed
  @retval EFI_DEVICE_ERROR      Error setting up memory

**/

/**
  Get resource configuration and fill in the TEGRA_RESOURCE_INFO structure.
  Installs some carveouts into HOB lists.

  @param [in]  CpuBootloaderAddress  Address of CPUBL params.
  @param [out] ResourceInfo          Pointer to TEGRA_RESOURCE_INFO structure.

  @retval EFI_SUCCESS           Resources have been installed
  @retval EFI_DEVICE_ERROR      Error setting up structure

**/
STATIC
EFI_STATUS
EFIAPI
T264GetResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *ResourceInfo
  )
{
  EFI_STATUS                 Status;
  NVDA_MEMORY_REGION         *BpmpIpcRegions;
  NVDA_MEMORY_REGION         *DramRegions, *CarveoutRegions, *UsableCarveoutRegions;
  UINTN                      DramRegionCount, CarveoutRegionCount, UsableCarveoutRegionCount;
  TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams =
    (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  Status = T264BuildDramRegions (
             CpuBootloaderParams,
             &DramRegions,
             &DramRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = T264BuildCarveoutRegions (
             CpuBootloaderParams,
             &CarveoutRegions,
             &CarveoutRegionCount,
             &UsableCarveoutRegions,
             &UsableCarveoutRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ResourceInfo->DtbLoadAddress             = GetDTBBaseAddress ();
  ResourceInfo->DramRegions                = DramRegions;
  ResourceInfo->DramRegionsCount           = DramRegionCount;
  ResourceInfo->UefiDramRegionIndex        = 0;
  ResourceInfo->CarveoutRegions            = CarveoutRegions;
  ResourceInfo->CarveoutRegionsCount       = CarveoutRegionCount;
  ResourceInfo->UsableCarveoutRegions      = UsableCarveoutRegions;
  ResourceInfo->UsableCarveoutRegionsCount = UsableCarveoutRegionCount;

  BpmpIpcRegions               = (NVDA_MEMORY_REGION *)AllocateZeroPool (sizeof (NVDA_MEMORY_REGION));
  ResourceInfo->BpmpIpcRegions = BpmpIpcRegions;

  BpmpIpcRegions->MemoryBaseAddress = CpuBootloaderParams->CarveoutInfo[CARVEOUT_BPMP_CPU_NS].Base;
  BpmpIpcRegions->MemoryLength      = CpuBootloaderParams->CarveoutInfo[CARVEOUT_BPMP_CPU_NS].Size;

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
  CpuBootloaderParams  = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  T264DramPageBlacklistInfoAddress[0].MemoryBaseAddress = CpuBootloaderParams->DramPageRetirementAddress & ~EFI_PAGE_MASK;
  T264DramPageBlacklistInfoAddress[0].MemoryLength      = SIZE_64KB;

  return T264DramPageBlacklistInfoAddress;
}

/**
  Retrieve DTB Address

**/
UINT64
GetDTBBaseAddress (
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
  CpuBootloaderParams  = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  MemoryBase           = CpuBootloaderParams->CarveoutInfo[CARVEOUT_UEFI].Base;
  MemorySize           = CpuBootloaderParams->CarveoutInfo[CARVEOUT_UEFI].Size;
  FvHeader             = NULL;
  FvOffset             = 0;

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
T264GetMmioBaseAndSize (
  VOID
  )
{
  TEGRA_MMIO_INFO  *MmioInfo;
  TEGRA_MMIO_INFO  *MmioInfoEnd;
  UINTN            Index;

  MmioInfo = AllocateZeroPool (
               sizeof (T264MmioInfo) +
               (ARRAY_SIZE (T264MmioTables) * sizeof (TEGRA_MMIO_INFO))
               );
  CopyMem (MmioInfo, T264MmioInfo, sizeof (T264MmioInfo));

  // point to the table terminating entry copied from T264MmioInfo
  MmioInfoEnd = MmioInfo + (sizeof (T264MmioInfo) / sizeof (TEGRA_MMIO_INFO)) - 1;

  for (Index = 0; Index < ARRAY_SIZE (T264MmioTables); Index++) {
    if (T264MmioTables[Index]->Size != 0) {
      CopyMem (MmioInfoEnd++, T264MmioTables[Index], sizeof (TEGRA_MMIO_INFO));
    }
  }

  return MmioInfo;
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

  return CommonConfigGetEnabledCoresBitMap (&T264CommonResourceConfigInfo, PlatformResourceInfo);
}

/**
  Retrieve Board Information

**/
EFI_STATUS
EFIAPI
T264GetBoardInfo (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  TEGRABL_EEPROM_DATA  *EepromData;
  T264_EEPROM_DATA     *T264EepromData;
  TEGRA_BOARD_INFO     *BoardInfo;

  BoardInfo      = PlatformResourceInfo->BoardInfo;
  EepromData     = PlatformResourceInfo->EepromData;
  T264EepromData = (T264_EEPROM_DATA *)EepromData->CvmEepromData;

  BoardInfo->FuseBaseAddr = T264_FUSE_BASE;
  BoardInfo->FuseList     = T264FuseList;
  BoardInfo->FuseCount    = ARRAY_SIZE (T264FuseList);

  CopyMem ((VOID *)&BoardInfo->CvmProductId, (VOID *)&T264EepromData->PartNumber, sizeof (T264EepromData->PartNumber));
  CopyMem ((VOID *)BoardInfo->SerialNumber, (VOID *)&T264EepromData->SerialNumber, sizeof (T264EepromData->SerialNumber));

  if ((CompareMem (T264EepromData->CustomerBlockSignature, EEPROM_CUSTOMER_BLOCK_SIGNATURE, sizeof (T264EepromData->CustomerBlockSignature)) == 0) &&
      (CompareMem (T264EepromData->CustomerTypeSignature, EEPROM_CUSTOMER_TYPE_SIGNATURE, sizeof (T264EepromData->CustomerTypeSignature)) == 0))
  {
    CopyMem ((VOID *)BoardInfo->MacAddr, (VOID *)T264EepromData->CustomerEthernetMacAddress, NET_ETHER_ADDR_LEN);
    BoardInfo->NumMacs = T264EepromData->CustomerNumEthernetMacs;
  } else {
    CopyMem ((VOID *)BoardInfo->MacAddr, (VOID *)T264EepromData->EthernetMacAddress, NET_ETHER_ADDR_LEN);
    BoardInfo->NumMacs = T264EepromData->NumEthernetMacs;
  }

  T264EepromData = (T264_EEPROM_DATA *)EepromData->CvbEepromData;
  CopyMem ((VOID *)&BoardInfo->CvbProductId, (VOID *)&T264EepromData->PartNumber, sizeof (T264EepromData->PartNumber));

  return EFI_SUCCESS;
}

/**
  Get UpdateBrBct flag

**/
STATIC
BOOLEAN
T264GetUpdateBrBct (
  VOID
  )
{
  BOOLEAN  UpdateBrBct;

  UpdateBrBct = MmioBitFieldRead32 (
                  T264_BOOT_CHAIN_REGISTER,
                  T264_BOOT_CHAIN_LAST_BOOT_CHAIN_FAILED_BIT,
                  T264_BOOT_CHAIN_LAST_BOOT_CHAIN_FAILED_BIT
                  );

  DEBUG ((DEBUG_INFO, "UpdateBrBct=%u\n", UpdateBrBct));

  return UpdateBrBct;
}

VOID
EFIAPI
ClearUpdateBrBctFlag (
  VOID
  )
{
  MmioBitFieldWrite32 (
    T264_BOOT_CHAIN_REGISTER,
    T264_BOOT_CHAIN_LAST_BOOT_CHAIN_FAILED_BIT,
    T264_BOOT_CHAIN_LAST_BOOT_CHAIN_FAILED_BIT,
    FALSE
    );
}

/**
  Retrieve Active Boot Chain Information

**/
STATIC
EFI_STATUS
EFIAPI
T264GetActiveBootChain (
  OUT UINT32  *BootChain
  )
{
  *BootChain = MmioBitFieldRead32 (
                 T264_BOOT_CHAIN_REGISTER,
                 T264_BOOT_CHAIN_MB1_BOOT_CHAIN_FIELD_LO,
                 T264_BOOT_CHAIN_MB1_BOOT_CHAIN_FIELD_HI
                 );

  DEBUG ((DEBUG_INFO, "%a: bootchain=0x%x\n", __FUNCTION__, *BootChain));

  if (*BootChain >= T264_BOOT_CHAIN_MAX) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
ValidateActiveBootChain (
  VOID
  )
{
  EFI_STATUS          Status;
  UINT32              BootChain;
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;
  UINTN               CpuBootloaderAddress;

  CpuBootloaderAddress = GetCPUBLBaseAddress ();
  CpuBootloaderParams  = (TEGRA_CPUBL_PARAMS *)CpuBootloaderAddress;
  Status               = T264GetActiveBootChain (&BootChain);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetActiveBootChain failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // set all BR chain status bits to GOOD
  MmioBitFieldWrite32 (
    T264_BOOT_CHAIN_REGISTER,
    T264_BOOT_CHAIN_BR_FAIL_BITMAP_FIELD_LO,
    T264_BOOT_CHAIN_BR_FAIL_BITMAP_FIELD_HI,
    T264_BOOT_CHAIN_STATUS_GOOD
    );

  // Set active boot chain mb1 status to GOOD
  MmioBitFieldWrite32 (
    T264_BOOT_CHAIN_REGISTER,
    T264_BOOT_CHAIN_MB1_FAIL_BITMAP_FIELD_LO + BootChain,
    T264_BOOT_CHAIN_MB1_FAIL_BITMAP_FIELD_LO + BootChain,
    T264_BOOT_CHAIN_STATUS_GOOD
    );

  return EFI_SUCCESS;
}

/**
  Initialize IP floorsweeping table

**/
EFI_STATUS
EFIAPI
T264InitFloorSweepingIpTable (
  IN T264_FLOOR_SWEEPING_IP_ENTRY    *T264IpTable,
  OUT TEGRA_FLOOR_SWEEPING_IP_ENTRY  **TegraIpTable
  )
{
  TEGRA_FLOOR_SWEEPING_IP_ENTRY  *TegraIpEntry;

  TegraIpEntry = *TegraIpTable;

  while (T264IpTable->IpName != NULL) {
    GetDisableRegArray (
      T264_SOCKET_MASK,
      0,
      T264IpTable->DisableRegAddr,
      T264IpTable->DisableRegMask,
      T264IpTable->DisableRegShift,
      T264IpTable->DisableRegArray
      );

    TegraIpEntry->IpName            = T264IpTable->IpName;
    TegraIpEntry->CompatibilityList = T264IpTable->CompatibilityList;
    TegraIpEntry->IdProperty        = T264IpTable->IdProperty;
    TegraIpEntry->DisableReg        = T264IpTable->DisableRegArray;

    T264IpTable++;
    TegraIpEntry++;
  }

  *TegraIpTable = TegraIpEntry;

  return EFI_SUCCESS;
}

/**
  Initialize floorsweeping info

**/
EFI_STATUS
EFIAPI
T264InitFloorSweepingInfo (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  TEGRA_FLOOR_SWEEPING_INFO      *Info;
  EFI_STATUS                     Status;
  TEGRA_PLATFORM_TYPE            Platform;
  UINT32                         GpuEnable;
  TEGRA_FLOOR_SWEEPING_IP_ENTRY  *TegraIpTable;
  UINT32                         *PcieDisableRegArray;
  TEGRA_FLOOR_SWEEPING_IP_ENTRY  *TegraIpTableNextEntry;

  Platform = TegraGetPlatform ();

  // Get PCIe disable reg
  PcieDisableRegArray = AllocateZeroPool (sizeof (*PcieDisableRegArray));
  Status              = GetDisableRegArray (
                          T264_SOCKET_MASK,
                          0,
                          T264_UPHY0_FUSE_BASE + T264_PCIE_FLOORSWEEPING_DISABLE_OFFSET,
                          ~T264_PCIE_FLOORSWEEPING_DISABLE_MASK,
                          T264_FUSE_NO_SHIFT,
                          PcieDisableRegArray
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: PcieDisableRegArray failed: %r\n", __FUNCTION__, Status));
  }

  // C0 always present, disable register only has bits for C1-C4
  *PcieDisableRegArray <<= 1;

  if (Platform != TEGRA_PLATFORM_SILICON) {
    GpuEnable = MmioRead32 (T264_PRESIL_GPU_ENABLE_REG);
    if ((GpuEnable & BIT31) == 0) {
      // if gpu is disabled, disable PCIe C0
      *PcieDisableRegArray |= 0x1;
      DEBUG ((DEBUG_ERROR, "%a: GpuEnable=0x%x, PcieDisable=0x%x\n", __FUNCTION__, GpuEnable, *PcieDisableRegArray));
    }
  }

  // Create IP floorsweeping table
  TegraIpTable = AllocateZeroPool (
                   sizeof (*TegraIpTable) *
                   (ARRAY_SIZE (mT264FloorSweepingIpTable) +
                    ARRAY_SIZE (mT264FloorSweepingPresilIpTable))
                   );
  TegraIpTableNextEntry = TegraIpTable;
  Status                = T264InitFloorSweepingIpTable (
                            mT264FloorSweepingIpTable,
                            &TegraIpTableNextEntry
                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: IpTable failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Add IPs that only require floorsweeping for pre-sil
  if (Platform != TEGRA_PLATFORM_SILICON) {
    Status = T264InitFloorSweepingIpTable (
               mT264FloorSweepingPresilIpTable,
               &TegraIpTableNextEntry
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: presil IpTable failed: %r\n", __FUNCTION__, Status));
    }
  }

  Info                       = AllocateZeroPool (sizeof (TEGRA_FLOOR_SWEEPING_INFO));
  Info->SocketAddressMask    = 0x1;
  Info->AddressToSocketShift = 63;     // not used
  Info->PcieEpCompatibility  = "nvidia,tegra264-pcie-ep";
  Info->PcieDisableRegArray  = PcieDisableRegArray;
  Info->PcieParentNameFormat = "/bus@0";
  Info->PcieNumParentNodes   = 1;
  Info->IpTable              = TegraIpTable;

  PlatformResourceInfo->FloorSweepingInfo = Info;

  return EFI_SUCCESS;
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
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)CpuBootloaderAddress;

  PlatformResourceInfo->SocketMask = T264_SOCKET_MASK;
  PlatformResourceInfo->BootType   = (UINT32)CpuBootloaderParams->BootType;

  if (InMm == FALSE) {
    Status = T264GetActiveBootChain (&PlatformResourceInfo->ActiveBootChain);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    DEBUG ((DEBUG_ERROR, "Boot chain: %u\n", PlatformResourceInfo->ActiveBootChain));

    PlatformResourceInfo->BrBctUpdateFlag = T264GetUpdateBrBct ();

    Status = T264GetResourceConfig (CpuBootloaderAddress, PlatformResourceInfo->ResourceInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    PlatformResourceInfo->MmioInfo   = T264GetMmioBaseAndSize ();
    PlatformResourceInfo->EepromData = &CpuBootloaderParams->Eeprom;

    Status = T264GetBoardInfo (PlatformResourceInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  PlatformResourceInfo->RamdiskOSInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_OS].Base;
  PlatformResourceInfo->RamdiskOSInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_OS].Size;

  PlatformResourceInfo->CpublCoInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_UEFI].Base;
  PlatformResourceInfo->CpublCoInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_UEFI].Size;

  // Populate RcmBlobInfo
  PlatformResourceInfo->RcmBlobInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_RCM_BLOB].Base;
  PlatformResourceInfo->RcmBlobInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_RCM_BLOB].Size;

  PlatformResourceInfo->FsiNsInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_FSI_CPU_NS].Base;
  PlatformResourceInfo->FsiNsInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_FSI_CPU_NS].Size;

  if (InMm == FALSE) {
    PlatformResourceInfo->ResourceInfo->RamOopsRegion.MemoryBaseAddress =
      CpuBootloaderParams->CarveoutInfo[CARVEOUT_RAM_OOPS].Base;
    PlatformResourceInfo->ResourceInfo->RamOopsRegion.MemoryLength =
      CpuBootloaderParams->CarveoutInfo[CARVEOUT_RAM_OOPS].Size;
  }

  PlatformResourceInfo->PhysicalDramSize = CpuBootloaderParams->SdramInfo.Size;

  PlatformResourceInfo->GrOutputInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_GR].Base;
  PlatformResourceInfo->GrOutputInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_GR].Size;

  PlatformResourceInfo->PvaFwInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_PVA].Base;
  PlatformResourceInfo->PvaFwInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_PVA].Size;

  PlatformResourceInfo->FrameBufferInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_DISP_EARLY_BOOT_FB].Base;
  PlatformResourceInfo->FrameBufferInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_DISP_EARLY_BOOT_FB].Size;

  PlatformResourceInfo->ProfilerInfo.Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_PROFILING].Base;
  PlatformResourceInfo->ProfilerInfo.Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_PROFILING].Size;

  if (InMm == FALSE) {
    PlatformResourceInfo->ResourceInfo->XusbRegion.MemoryBaseAddress = CpuBootloaderParams->CarveoutInfo[CARVEOUT_XUSB].Base;
    PlatformResourceInfo->ResourceInfo->XusbRegion.MemoryLength      = CpuBootloaderParams->CarveoutInfo[CARVEOUT_XUSB].Size;
  }

  PlatformResourceInfo->PcieAddressBits = T264_PCIE_ADDRESS_BITS;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
T264GetVprInfo (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  UINT64  McBase;

  if (PlatformResourceInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PlatformResourceInfo->VprInfo = mVprInfo;
  McBase                        = T264_MEMORY_CONTROLLER_BASE;

  PlatformResourceInfo->VprInfo[0].Base =
    ((UINT64)MmioRead32 (McBase + T264_MC_VIDEO_PROTECT_BOM_ADR_HI_0) << 32) |
    MmioRead32 (McBase + T264_MC_VIDEO_PROTECT_BOM_0);

  PlatformResourceInfo->VprInfo[0].Size =
    (UINT64)MmioRead32 (McBase + T264_MC_VIDEO_PROTECT_SIZE_MB_0) << 20;

  DEBUG ((DEBUG_INFO, "%a: VPR base=0x%llx size=0x%llx\n", __FUNCTION__, PlatformResourceInfo->VprInfo[0].Base, PlatformResourceInfo->VprInfo[0].Size));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SocUpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  EFI_STATUS  Status;

  if (PlatformResourceInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = T264GetVprInfo (PlatformResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = T264InitFloorSweepingInfo (PlatformResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

UINT32
EFIAPI
PcieIdToInterface (
  IN UINT32  PcieId
  )
{
  return PcieId;
}

UINT32
EFIAPI
PcieIdToSocket (
  IN UINT32  PcieId
  )
{
  return 0;
}

BOOLEAN
EFIAPI
BootChainIsFailed (
  IN UINT32  BootChain
  )
{
  UINT32  Failed;

  NV_ASSERT_RETURN ((BootChain < T264_BOOT_CHAIN_MAX), return TRUE, "%a: invalid boot chain %u\n", __FUNCTION__, BootChain);

  Failed = MmioBitFieldRead32 (
             T264_BOOT_CHAIN_REGISTER,
             T264_BOOT_CHAIN_MB1_FAIL_BITMAP_FIELD_LO + BootChain,
             T264_BOOT_CHAIN_MB1_FAIL_BITMAP_FIELD_LO + BootChain
             );

  DEBUG ((DEBUG_INFO, "%a: chain %u failed=%u\n", __FUNCTION__, BootChain, Failed));

  return (Failed == T264_BOOT_CHAIN_STATUS_BAD);
}

EFI_STATUS
EFIAPI
SetInactiveBootChainStatus (
  IN BOOLEAN  SetGoodStatus
  )
{
  UINT32      BootChain;
  EFI_STATUS  Status;

  Status = T264GetActiveBootChain (&BootChain);
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  BootChain = OTHER_BOOT_CHAIN (BootChain);

  MmioBitFieldWrite32 (
    T264_BOOT_CHAIN_REGISTER,
    T264_BOOT_CHAIN_MB1_FAIL_BITMAP_FIELD_LO + BootChain,
    T264_BOOT_CHAIN_MB1_FAIL_BITMAP_FIELD_LO + BootChain,
    SetGoodStatus ? T264_BOOT_CHAIN_STATUS_GOOD : T264_BOOT_CHAIN_STATUS_BAD
    );

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
  if (BootChain >= T264_BOOT_CHAIN_MAX) {
    return EFI_INVALID_PARAMETER;
  }

  // nothing to do for marker-based boot chain
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
T264GetActiveBootChainStMm (
  IN  UINTN   ScratchBase,
  OUT UINT32  *BootChain
  )
{
  *BootChain = MmioBitFieldRead32 (
                 ScratchBase + T264_BOOT_CHAIN_REGISTER_OFFSET,
                 T264_BOOT_CHAIN_MB1_BOOT_CHAIN_FIELD_LO,
                 T264_BOOT_CHAIN_MB1_BOOT_CHAIN_FIELD_HI
                 );

  DEBUG ((DEBUG_INFO, "%a: addr=0x%llx bootchain=%u\n", __FUNCTION__, ScratchBase, *BootChain));

  if (*BootChain >= T264_BOOT_CHAIN_MAX) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

UINT32
EFIAPI
SocGetSocketMask (
  IN UINTN  CpuBootloaderAddress
  )
{
  return T264_SOCKET_MASK;
}

BOOLEAN
EFIAPI
GetGicInfo (
  OUT TEGRA_GIC_INFO  *GicInfo
  )
{
  GicInfo->GicCompatString = "arm,gic-v3";
  GicInfo->ItsCompatString = "arm,gic-v3-its";
  GicInfo->Version         = 4;

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

EFI_STATUS
EFIAPI
GetRootfsStatusReg (
  IN UINT32  *RegisterValue
  )
{
  *RegisterValue = MmioRead32 (T264_ROOTFS_REGISTER);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SetRootfsStatusReg (
  IN UINT32  RegisterValue
  )
{
  MmioWrite32 (T264_ROOTFS_REGISTER, RegisterValue);

  return EFI_SUCCESS;
}

VOID
EFIAPI
SetNextBootRecovery (
  IN  VOID
  )
{
  MmioBitFieldWrite32 (
    T264_RECOVERY_BOOT_REGISTER,
    T264_RECOVERY_BOOT_BIT,
    T264_RECOVERY_BOOT_BIT,
    1
    );
}

UINT64
EFIAPI
GetGRBlobBaseAddress (
  VOID
  )
{
  return 0;
}

EFI_STATUS
EFIAPI
InValidateActiveBootChain (
  VOID
  )
{
  return EFI_UNSUPPORTED;
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

BOOLEAN
EFIAPI
IsTpmToBeEnabled (
  VOID
  )
{
  return FALSE;
}
