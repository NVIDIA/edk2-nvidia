/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include <T410/T410Definitions.h>
#include "CommonResourceConfig.h"

#include "PlatformResourceConfig.h"
#include "T410ResourceConfigPrivate.h"

#define T410_MAX_CORE_DISABLE_WORDS       4
#define T410_MAX_SCF_CACHE_DISABLE_WORDS  4

STATIC UINT64  T410SocketScratchBaseAddr[T410_MAX_SOCKETS] = {
  T410_SCRATCH_BASE_SOCKET_0,
  T410_SCRATCH_BASE_SOCKET_1,
};

STATIC UINT32  T410CoreDisableScratchOffset[T410_MAX_CORE_DISABLE_WORDS] = {
  T410_CPU_FLOORSWEEPING_DISABLE_OFFSET_0,
  T410_CPU_FLOORSWEEPING_DISABLE_OFFSET_1,
  T410_CPU_FLOORSWEEPING_DISABLE_OFFSET_2,
  T410_CPU_FLOORSWEEPING_DISABLE_OFFSET_3,
};

STATIC UINT32  T410CoreDisableScratchMask[T410_MAX_CORE_DISABLE_WORDS] = {
  T410_CPU_FLOORSWEEPING_DISABLE_MASK_0,
  T410_CPU_FLOORSWEEPING_DISABLE_MASK_1,
  T410_CPU_FLOORSWEEPING_DISABLE_MASK_2,
  T410_CPU_FLOORSWEEPING_DISABLE_MASK_3,
};

STATIC UINT32  T410CoreDisableScratchShift[T410_MAX_CORE_DISABLE_WORDS] = {
  T410_CPU_FLOORSWEEPING_DISABLE_SHFT_0,
  T410_CPU_FLOORSWEEPING_DISABLE_SHFT_1,
  T410_CPU_FLOORSWEEPING_DISABLE_SHFT_2,
  T410_CPU_FLOORSWEEPING_DISABLE_SHFT_3,
};

STATIC COMMON_RESOURCE_CONFIG_INFO  T410CommonResourceConfigInfo = {
  T410_MAX_SOCKETS,
  T410_MAX_CORE_DISABLE_WORDS,
  FALSE,
  MAX_UINT32,
  T410SocketScratchBaseAddr,
  T410CoreDisableScratchOffset,
  T410CoreDisableScratchMask,
  T410CoreDisableScratchShift
};

STATIC UINT32  T410ScfCacheDisableScratchOffset[T410_MAX_SCF_CACHE_DISABLE_WORDS] = {
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_0,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_1,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_2,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_3,
};

STATIC UINT32  T410ScfCacheDisableScratchMask[T410_MAX_SCF_CACHE_DISABLE_WORDS] = {
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_0,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_1,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_2,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_3,
};

STATIC UINT32  T410ScfCacheDisableScratchShift[T410_MAX_SCF_CACHE_DISABLE_WORDS] = {
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_SHFT_0,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_SHFT_1,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_SHFT_2,
  T410_SCF_CACHE_FLOORSWEEPING_DISABLE_SHFT_3,
};

STATIC TEGRA_FLOOR_SWEEPING_SCF_CACHE  T410ScfCacheInfo = {
  .MaxScfCacheCountPerSocket = T410_SCF_CACHE_INSTANCES_PER_SOCKET,
  .ScfDisableWords           = T410_MAX_SCF_CACHE_DISABLE_WORDS,
  .ScfDisableSocketBase      = T410SocketScratchBaseAddr,
  .ScfDisableOffset          = T410ScfCacheDisableScratchOffset,
  .ScfDisableMask            = T410ScfCacheDisableScratchMask,
  .ScfDisableShift           = T410ScfCacheDisableScratchShift,
  .ScfSliceSize              = T410_SCF_CACHE_SLICE_SIZE,
  .ScfSliceSets              = T410_SCF_CACHE_SLICE_SETS,
};

TEGRA_MMIO_INFO  T410MmioInfo[] = {
  {
    T410_WDT_CTRL_BASE,
    SIZE_4KB
  },
  {
    T410_WDT_RFRSH_BASE,
    SIZE_4KB
  },
  {
    T410_HSP_SHRD_MBOX_RAS_UEFI_SOCKET_0,
    SIZE_4KB
  },
  {
    T410_HSP_SHRD_MBOX_RAS_UEFI_SOCKET_1,
    SIZE_4KB
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

#define T410_CCPLEX_INTERWORLD_SHMEM_MMIO_INFO_INDEX  (ARRAY_SIZE (T410MmioInfo) - 2)

TEGRA_MMIO_INFO  T410GicRedistributorMmioInfo[] = {
  {
    T410_GIC_REDISTRIBUTOR_BASE_SOCKET_0,
    SIZE_256KB *T410_GIC_REDISTRIBUTOR_INSTANCES
  },
  {
    T410_GIC_REDISTRIBUTOR_BASE_SOCKET_1,
    SIZE_256KB *T410_GIC_REDISTRIBUTOR_INSTANCES
  },
};

TEGRA_MMIO_INFO  T410GicItsMmioInfo[] = {
  {
    T410_GIC_ITS_BASE_SOCKET_0,
    SIZE_64KB
  },
  {
    T410_GIC_ITS_BASE_SOCKET_1,
    SIZE_64KB
  },
};

TEGRA_MMIO_INFO  T410SocketScratchMmioInfo[] = {
  {
    T410_SCRATCH_BASE_SOCKET_0,
    T410_SCRATCH_SIZE
  },
  {
    T410_SCRATCH_BASE_SOCKET_1,
    T410_SCRATCH_SIZE
  },
};

TEGRA_MMIO_INFO  T410SocketCbbMmioInfo[] = {
  {
    T410_CBB_FABRIC_BASE_SOCKET_0,
    T410_CBB_FABRIC_SIZE
  },
  {
    T410_CBB_FABRIC_BASE_SOCKET_1,
    T410_CBB_FABRIC_SIZE
  },
};

TEGRA_MMIO_INFO  T410SocketMssMmioInfo[] = {
  {
    T410_MSS_BASE_SOCKET_0,
    T410_MSS_SIZE
  },
  {
    T410_MSS_BASE_SOCKET_1,
    T410_MSS_SIZE
  },
};

TEGRA_MMIO_INFO  T410SocketMcfSmmuMmioInfo[] = {
  {
    T410_MCF_SMMU_SOCKET_0,
    SIZE_4KB
  },
  {
    T410_MCF_SMMU_SOCKET_1,
    SIZE_4KB
  },
};

TEGRA_MMIO_INFO  T410SocketCxlBrgMmioInfo[][T410_MAX_SEGMENT_CTLR_PER_SOCKET] = {
  {
    {
      T410_CXL_BRG_0_SOCKET_0,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_1_SOCKET_0,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_2_SOCKET_0,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_3_SOCKET_0,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_4_SOCKET_0,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_5_SOCKET_0,
      SIZE_256KB
    },
  },
  {
    {
      T410_CXL_BRG_0_SOCKET_1,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_1_SOCKET_1,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_2_SOCKET_1,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_3_SOCKET_1,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_4_SOCKET_1,
      SIZE_256KB
    },
    {
      T410_CXL_BRG_5_SOCKET_1,
      SIZE_256KB
    },
  },
};

NVDA_MEMORY_REGION  T410DramPageBlacklistInfoAddress[] = {
  {
    0,
    0
  },
  {
    0,
    0
  },
  {
    0,
    0
  },
  {
    0,
    0
  },
  {
    0,
    0
  }
};

TEGRA_DRAM_DEVICE_INFO  T410DramDeviceInfo[T410_MAX_SOCKETS] = { };
UINT8                   T410C2cMode[T410_MAX_SOCKETS]        = { };

/**
  Get Socket Mask

**/
UINT32
EFIAPI
SocGetSocketMask (
  IN UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;
  UINT32              SocketMask;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  SocketMask = CpuBootloaderParams->SocketMask;
  ASSERT (SocketMask != 0);
  ASSERT (HighBitSet32 (SocketMask) < T410_MAX_SOCKETS);

  return SocketMask;
}

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
T410BuildDramRegions (
  IN  CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams,
  OUT NVDA_MEMORY_REGION      **CONST  DramRegions,
  OUT UINTN                    *CONST  DramRegionCount
  )
{
  NVDA_MEMORY_REGION    *Regions;
  UINTN                 RegionCount, RegionCountMax;
  UINTN                 Socket;
  EFI_PHYSICAL_ADDRESS  Base;
  UINT64                Size;
  CONST UINT32          SocketMask = SocGetSocketMask ((EFI_PHYSICAL_ADDRESS)CpuBootloaderParams);
  CONST UINT32          MaxSocket  = HighBitSet32 (SocketMask);

  RegionCountMax = MaxSocket + 1;

  Regions = (NVDA_MEMORY_REGION *)AllocatePool (RegionCountMax * sizeof (*Regions));
  NV_ASSERT_RETURN (
    Regions != NULL,
    return EFI_DEVICE_ERROR,
    "%a: Failed to allocate %lu DRAM regions\r\n",
    __FUNCTION__,
    (UINT64)RegionCountMax
    );

  RegionCount = 0;

  for (Socket = T410_PRIMARY_SOCKET; Socket < T410_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    Base = CpuBootloaderParams->SdramInfo[Socket].Base;
    Size = CpuBootloaderParams->SdramInfo[Socket].Size;

    PlatformResourceAddMemoryRegion (Regions, &RegionCount, Base, Size);
  }

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
   @param[in]     Socket             CPU socket index.
   @param[in]     Carveouts          Bootloader carveouts.
   @param[in]     CarveoutCount      Number of bootloader carveouts.
*/
STATIC
VOID
T410AddBootloaderCarveouts (
  IN     NVDA_MEMORY_REGION          *CONST  Regions,
  IN OUT UINTN                       *CONST  RegionCount,
  IN     NVDA_MEMORY_REGION          *CONST  UsableRegions,
  IN OUT UINTN                       *CONST  UsableRegionCount,
  IN     CONST UINTN                         Socket,
  IN     CONST TEGRABL_CARVEOUT_INFO *CONST  Carveouts,
  IN     CONST UINTN                         CarveoutCount
  )
{
  UINTN                 Index;
  EFI_PHYSICAL_ADDRESS  Base;
  UINT64                Size, Pages;

  TEGRA_MMIO_INFO *CONST  CcplexInterworldShmemMmioInfo =
    &T410MmioInfo[T410_CCPLEX_INTERWORLD_SHMEM_MMIO_INFO_INDEX];

  for (Index = 0; Index < CarveoutCount; ++Index) {
    Base = Carveouts[Index].Base;
    Size = Carveouts[Index].Size;

    if ((Base == 0) || (Size == 0)) {
      continue;
    }

    DEBUG ((
      DEBUG_ERROR,
      "Socket: %u Carveout %u Region: Base: 0x%016lx, Size: 0x%016lx\n",
      Socket,
      Index,
      Base,
      Size
      ));

    switch (Index) {
      case CARVEOUT_UEFI:
      case CARVEOUT_OS:
        // Leave in memory map but marked as used on socket 0
        if (Socket == T410_PRIMARY_SOCKET) {
          Pages = EFI_SIZE_TO_PAGES (Size);
          BuildMemoryAllocationHob (Base, EFI_PAGES_TO_SIZE (Pages), EfiReservedMemoryType);
          PlatformResourceAddMemoryRegion (UsableRegions, UsableRegionCount, Base, Size);
        }

        break;

      case CARVEOUT_CCPLEX_INTERWORLD_SHMEM:
        if (Socket == T410_PRIMARY_SOCKET) {
          // For primary socket, add memory in DRAM CO CARVEOUT_CCPLEX_INTERWORLD_SHMEM in its placeholder
          // in T410MmioInfo for MMIO mapping.
          CcplexInterworldShmemMmioInfo->Base = Base;
          CcplexInterworldShmemMmioInfo->Size = Size;
        }

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
T410BuildCarveoutRegions (
  IN  CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams,
  OUT NVDA_MEMORY_REGION      **CONST  CarveoutRegions,
  OUT UINTN                    *CONST  CarveoutRegionCount,
  OUT NVDA_MEMORY_REGION      **CONST  UsableCarveoutRegions,
  OUT UINTN                    *CONST  UsableCarveoutRegionCount
  )
{
  NVDA_MEMORY_REGION    *Regions, *UsableRegions;
  UINTN                 RegionCount, UsableRegionCount;
  UINTN                 Socket;
  EFI_PHYSICAL_ADDRESS  *RetiredDramPageList;
  CONST UINT32          SocketMask            = SocGetSocketMask ((EFI_PHYSICAL_ADDRESS)CpuBootloaderParams);
  CONST UINT32          MaxSocket             = HighBitSet32 (SocketMask);
  CONST UINTN           RegionCountMax        = (UINTN)(MaxSocket + 1) * (CARVEOUT_OEM_COUNT + MAX_RETIRED_DRAM_PAGES);
  CONST UINTN           RegionsPagesMax       = EFI_SIZE_TO_PAGES (RegionCountMax * sizeof (*Regions));
  CONST UINTN           UsableRegionCountMax  = (UINTN)(MaxSocket + 1) * CARVEOUT_OEM_COUNT;
  CONST UINTN           UsableRegionsPagesMax = EFI_SIZE_TO_PAGES (UsableRegionCountMax * sizeof (*UsableRegions));

  Regions = (NVDA_MEMORY_REGION *)AllocatePages (RegionsPagesMax);
  NV_ASSERT_RETURN (
    Regions != NULL,
    return EFI_DEVICE_ERROR,
    "%a: Failed to allocate %lu pages for carveout regions\r\n",
    __FUNCTION__,
    (UINT64)RegionsPagesMax
    );

  UsableRegions = (NVDA_MEMORY_REGION *)AllocatePages (UsableRegionsPagesMax);
  NV_ASSERT_RETURN (
    UsableRegions != NULL,
    { FreePages (Regions, RegionsPagesMax);
      return EFI_DEVICE_ERROR;
    },
    "%a: Failed to allocate %lu pages for usable carveout regions\r\n",
    __FUNCTION__,
    (UINT64)UsableRegionsPagesMax
    );

  RegionCount = UsableRegionCount = 0;

  for (Socket = T410_PRIMARY_SOCKET; Socket < T410_MAX_SOCKETS; Socket++) {
    if ((SocketMask & (1 << Socket)) != 0) {
      T410AddBootloaderCarveouts (
        Regions,
        &RegionCount,
        UsableRegions,
        &UsableRegionCount,
        Socket,
        CpuBootloaderParams->CarveoutInfo[Socket],
        CARVEOUT_OEM_COUNT
        );
    }
  }

  for (Socket = T410_PRIMARY_SOCKET; Socket < T410_MAX_SOCKETS; Socket++) {
    if ((SocketMask & (1 << Socket)) != 0) {
      RetiredDramPageList = (EFI_PHYSICAL_ADDRESS *)CpuBootloaderParams->RetiredDramPageListAddr[Socket];

      if (RetiredDramPageList != NULL) {
        PlatformResourceAddRetiredDramPages (
          Regions,
          &RegionCount,
          RetiredDramPageList,
          MAX_RETIRED_DRAM_PAGES,
          SIZE_64KB
          );
      }
    }
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
T410GetResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
  )
{
  EFI_STATUS          Status;
  NVDA_MEMORY_REGION  *DramRegions, *CarveoutRegions, *UsableCarveoutRegions;
  UINTN               DramRegionCount, CarveoutRegionCount, UsableCarveoutRegionCount;

  TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams =
    (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  Status = T410BuildDramRegions (
             CpuBootloaderParams,
             &DramRegions,
             &DramRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = T410BuildCarveoutRegions (
             CpuBootloaderParams,
             &CarveoutRegions,
             &CarveoutRegionCount,
             &UsableCarveoutRegions,
             &UsableCarveoutRegionCount
             );
  if (EFI_ERROR (Status)) {
    FreePool (DramRegions);
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

  if (CpuBootloaderParams->EarlyBootVariables->Data.Mb1Data.UefiDebugLevel == 0) {
    CpuBootloaderParams->EarlyBootVariables->Data.Mb1Data.UefiDebugLevel = PcdGet32 (PcdDebugPrintErrorLevel);
  }

  return EFI_SUCCESS;
}

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
EFIAPI
GetDramPageBlacklistInfoAddress (
  VOID
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;
  UINTN               CpuBootloaderAddress;
  UINT32              SocketMask;
  UINTN               Socket;
  UINTN               Index;

  CpuBootloaderAddress = GetCPUBLBaseAddress ();
  CpuBootloaderParams  = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  SocketMask           = SocGetSocketMask (CpuBootloaderAddress);

  Index = 0;
  for (Socket = T410_PRIMARY_SOCKET; Socket < T410_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    if (CpuBootloaderParams->RetiredDramPageListAddr[Socket] != 0) {
      T410DramPageBlacklistInfoAddress[Index].MemoryBaseAddress = CpuBootloaderParams->RetiredDramPageListAddr[Socket] & ~EFI_PAGE_MASK;
      T410DramPageBlacklistInfoAddress[Index].MemoryLength      = SIZE_64KB;
      Index++;
    }
  }

  return T410DramPageBlacklistInfoAddress;
}

/**
  Retrieve DTB Address

**/
UINT64
EFIAPI
GetDTBBaseAddress (
  VOID
  )
{
  TEGRA_CPUBL_PARAMS          *CpuBootloaderParams;
  UINTN                       CpuBootloaderAddress;
  UINT64                      MemoryBase;
  UINT64                      MemorySize;
  EFI_FIRMWARE_VOLUME_HEADER  *FvHeader;
  UINT64                      FvOffset;
  UINT64                      FvSize;

  CpuBootloaderAddress = GetCPUBLBaseAddress ();
  CpuBootloaderParams  = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  MemoryBase           = CpuBootloaderParams->CarveoutInfo[T410_PRIMARY_SOCKET][CARVEOUT_UEFI].Base;
  MemorySize           = CpuBootloaderParams->CarveoutInfo[T410_PRIMARY_SOCKET][CARVEOUT_UEFI].Size;
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
T410GetMmioBaseAndSize (
  IN UINT32  SocketMask
  )
{
  TEGRA_MMIO_INFO  *MmioInfo;
  TEGRA_MMIO_INFO  *MmioInfoEnd;
  UINTN            Socket;
  UINTN            SegmentCtlr;

  MmioInfo = AllocateZeroPool (
               sizeof (T410MmioInfo) +
               (T410_MAX_SOCKETS * (6 + T410_MAX_SEGMENT_CTLR_PER_SOCKET) * sizeof (TEGRA_MMIO_INFO))
               );
  CopyMem (MmioInfo, T410MmioInfo, sizeof (T410MmioInfo));

  // point to the table terminating entry copied from T410MmioInfo
  MmioInfoEnd = MmioInfo + (sizeof (T410MmioInfo) / sizeof (TEGRA_MMIO_INFO)) - 1;

  for (Socket = 0; Socket < T410_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    CopyMem (MmioInfoEnd++, &T410GicRedistributorMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &T410GicItsMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &T410SocketScratchMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &T410SocketCbbMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &T410SocketMssMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &T410SocketMcfSmmuMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    for (SegmentCtlr = 0; SegmentCtlr < T410_MAX_SEGMENT_CTLR_PER_SOCKET; SegmentCtlr++) {
      CopyMem (MmioInfoEnd++, &T410SocketCxlBrgMmioInfo[Socket][SegmentCtlr], sizeof (TEGRA_MMIO_INFO));
    }
  }

  return MmioInfo;
}

/**
  Retrieve Active Boot Chain

**/
EFI_STATUS
EFIAPI
T410GetActiveBootChain (
  IN  UINTN   CpuBootloaderAddress,
  IN  UINTN   Socket,
  OUT UINT32  *BootChain
  )
{
  UINT64  ScratchAddr;

  ScratchAddr = T410SocketScratchMmioInfo[Socket].Base + T410_BOOT_CHAIN_SCRATCH_OFFSET;

  *BootChain = MmioBitFieldRead32 (
                 ScratchAddr,
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
EFIAPI
ValidateActiveBootChain (
  VOID
  )
{
  UINTN       CpuBootloaderAddress;
  UINT32      SocketMask;
  UINTN       Socket;
  UINT64      ScratchAddr;
  EFI_STATUS  Status;
  UINT32      BootChain;

  CpuBootloaderAddress = GetCPUBLBaseAddress ();
  SocketMask           = SocGetSocketMask (CpuBootloaderAddress);
  for (Socket = 0; Socket < T410_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    Status = T410GetActiveBootChain (CpuBootloaderAddress, Socket, &BootChain);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: GetActiveBootChain failed socket %u: %r\n", __FUNCTION__, Socket, Status));
      continue;
    }

    ScratchAddr = T410SocketScratchMmioInfo[Socket].Base + T410_BOOT_CHAIN_SCRATCH_OFFSET;

    MmioBitFieldWrite32 (
      ScratchAddr,
      BOOT_CHAIN_STATUS_LO + BootChain,
      BOOT_CHAIN_STATUS_LO + BootChain,
      BOOT_CHAIN_GOOD
      );
  }

  return EFI_SUCCESS;
}

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
SocGetEnabledCoresBitMap (
  IN UINTN                     CpuBootloaderAddress,
  IN OUT SOC_CORE_BITMAP_INFO  *SocCoreBitmapInfo
  )
{
  SocCoreBitmapInfo->ThreadsPerCore       = 1;
  T410CommonResourceConfigInfo.SocketMask = SocGetSocketMask (CpuBootloaderAddress);

  return CommonConfigGetEnabledCoresBitMap (&T410CommonResourceConfigInfo, SocCoreBitmapInfo);
}

/**
  Get CPU C2C mode from enabled socket. Needs to be called after
  ArmSetMemoryRegionReadOnly to prevent exception.

**/
STATIC
EFI_STATUS
EFIAPI
T410CpuC2cMode (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  /*
  UINTN  Socket;

  if (PlatformResourceInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  PlatformResourceInfo->C2cMode = T410C2cMode;

  for (Socket = 0; Socket < T410_MAX_SOCKETS; Socket++) {
    if ((PlatformResourceInfo->SocketMask & (1UL << Socket)) == 0) {
      continue;
    }
    T410C2cMode[Socket] = MmioRead32 (T410SocketMssMmioInfo[Socket].Base + T410_MSS_C2C_MODE) & 0x03;
  }
  */
  return EFI_SUCCESS;
}

/**
  Initialize floorsweeping info

**/
EFI_STATUS
EFIAPI
T410InitFloorSweepingInfo (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  TEGRA_FLOOR_SWEEPING_INFO  *Info;
  EFI_STATUS                 Status;
  TEGRA_PLATFORM_TYPE        Platform;
  UINT32                     *PcieDisableRegArray;

  Platform = TegraGetPlatform ();

  // Get PCIe disable reg
  PcieDisableRegArray = AllocateZeroPool (T410_MAX_SOCKETS * sizeof (*PcieDisableRegArray));
  if (PcieDisableRegArray == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate PcieDisableRegArray\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetDisableRegArray (
             PlatformResourceInfo->SocketMask,
             (1ULL << T410_SOCKET_SHFT),
             T410_SCRATCH_BASE_SOCKET_0 + T410_PCIE_FLOORSWEEPING_DISABLE_OFFSET,
             ~T410_PCIE_FLOORSWEEPING_DISABLE_MASK,
             T410_PCIE_FLOORSWEEPING_DISABLE_SHIFT,
             PcieDisableRegArray
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: PcieDisableRegArray failed: %r\n", __FUNCTION__, Status));
  }

  Info = AllocateZeroPool (sizeof (TEGRA_FLOOR_SWEEPING_INFO));
  if (Info == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate floor sweeping info\n", __FUNCTION__));
    FreePool (PcieDisableRegArray);
    return EFI_OUT_OF_RESOURCES;
  }

  Info->SocketAddressMask    = T410_SOCKET_MASK;
  Info->AddressToSocketShift = T410_SOCKET_SHFT;
  Info->PcieDisableRegArray  = PcieDisableRegArray;
  Info->PcieParentNameFormat = "/socket@%u";
  Info->PcieNumParentNodes   = T410_MAX_SOCKETS;
  Info->ScfCacheInfo         = &T410ScfCacheInfo;

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
  UINT32              SocketMask;
  UINTN               Index;
  UINTN               Count;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  SocketMask = SocGetSocketMask (CpuBootloaderAddress);

  PlatformResourceInfo->SocketMask = SocketMask;

  /* Avoid this step when called from MM */
  if (InMm == FALSE) {
    Status = T410GetActiveBootChain (CpuBootloaderAddress, 0, &PlatformResourceInfo->ActiveBootChain);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = T410GetResourceConfig (CpuBootloaderAddress, PlatformResourceInfo->ResourceInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    PlatformResourceInfo->MmioInfo = T410GetMmioBaseAndSize (SocketMask);
  }

  PlatformResourceInfo->RamdiskOSInfo.Base = CpuBootloaderParams->CarveoutInfo[T410_PRIMARY_SOCKET][CARVEOUT_OS].Base;
  PlatformResourceInfo->RamdiskOSInfo.Size = CpuBootloaderParams->CarveoutInfo[T410_PRIMARY_SOCKET][CARVEOUT_OS].Size;

  PlatformResourceInfo->CpublCoInfo.Base = CpuBootloaderParams->CarveoutInfo[T410_PRIMARY_SOCKET][CARVEOUT_UEFI].Base;
  PlatformResourceInfo->CpublCoInfo.Size = CpuBootloaderParams->CarveoutInfo[T410_PRIMARY_SOCKET][CARVEOUT_UEFI].Size;

  // T410 has no RCM carveout; keep the blob info defined so BootType does not
  // depend on whatever the caller left in the structure.
  PlatformResourceInfo->RcmBlobInfo.Base = 0;
  PlatformResourceInfo->RcmBlobInfo.Size = 0;

  if ((PlatformResourceInfo->RcmBlobInfo.Base != 0) &&
      (PlatformResourceInfo->RcmBlobInfo.Size != 0))
  {
    PlatformResourceInfo->BootType = TegrablBootRcm;
  } else {
    PlatformResourceInfo->BootType = TegrablBootColdBoot;
  }

  PlatformResourceInfo->PhysicalDramSize = 0;
  PlatformResourceInfo->DramDeviceInfo   = T410DramDeviceInfo;
  // Populate Total Memory.
  for (Index = 0; Index < T410_MAX_SOCKETS; Index++) {
    if (!(SocketMask & (1UL << Index))) {
      continue;
    }

    PlatformResourceInfo->PhysicalDramSize  += CpuBootloaderParams->SdramInfo[Index].Size;
    T410DramDeviceInfo[Index].DataWidth      = CpuBootloaderParams->DramInfo[Index].DataWidth;
    T410DramDeviceInfo[Index].ManufacturerId = CpuBootloaderParams->DramInfo[Index].ManufacturerId;
    T410DramDeviceInfo[Index].Rank           = CpuBootloaderParams->DramInfo[Index].Rank;
    T410DramDeviceInfo[Index].SerialNumber   = CpuBootloaderParams->DramInfo[Index].SerialNumber;
    T410DramDeviceInfo[Index].TotalWidth     = CpuBootloaderParams->DramInfo[Index].TotalWidth;
    T410DramDeviceInfo[Index].Size           = CpuBootloaderParams->SdramInfo[Index].Size;
    T410DramDeviceInfo[Index].SpeedKhz       = 0;
  }

  PlatformResourceInfo->IsSMTAvailable = CpuBootloaderParams->IsSMT2Enabled;

  for (Index = 0; Index < T410_MAX_SOCKETS; Index++) {
    if (!(SocketMask & (1UL << Index))) {
      continue;
    }

    for (Count = 0; Count < UID_NUM_DWORDS; Count++) {
      PlatformResourceInfo->UniqueId[Index][Count] += CpuBootloaderParams->UniqueId[Index][Count];
    }
  }

  BuildGuidDataHob (&gNVIDIAT410MB1DataGuid, &CpuBootloaderParams->EarlyBootVariables, sizeof (CpuBootloaderParams->EarlyBootVariables));
  BuildGuidDataHob (&gNVIDIAT410MB1DefaultDataGuid, &CpuBootloaderParams->EarlyBootVariablesDefaults, sizeof (CpuBootloaderParams->EarlyBootVariablesDefaults));

  /*
    Status = T410BuildTcgEventHob ((UINTN)&CpuBootloaderParams->EarlyTpmCommitLog);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  */

  PlatformResourceInfo->PcieAddressBits = T410_PCIE_ADDRESS_BITS;

  return EFI_SUCCESS;
}

/**
 * Get Partition information.
**/
STATIC
EFI_STATUS
EFIAPI
T410GetPartitionInfo (
  IN  UINTN   CpuBootloaderAddress,
  IN  UINT32  PartitionIndex,
  OUT UINT16  *DeviceInstance,
  OUT UINT64  *PartitionStartByte,
  OUT UINT64  *PartitionSizeBytes
  )
{
  TEGRA_CPUBL_PARAMS      *CpuBootloaderParams;
  TEGRABL_PARTITION_DESC  *PartitionDesc;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  if (PcdGetBool (PcdCapsulePartitionEnabled)) {
    if (PartitionIndex == TEGRAUEFI_CAPSULE) {
      *PartitionSizeBytes =  PcdGet64 (PcdCapsulePartitionSize);
      PartitionDesc       = &CpuBootloaderParams->PartitionInfo[TEGRABL_RAS_ERROR_LOGS][PRIMARY_COPY];
      *PartitionStartByte = PartitionDesc->StartBlock * BLOCK_SIZE;
      return EFI_SUCCESS;
    }
  }

  if (PartitionIndex >= TEGRABL_BINARY_MAX) {
    DEBUG ((
      DEBUG_ERROR,
      "%a, Partition Index is invalid %u (Max %u)\n",
      __FUNCTION__,
      PartitionIndex,
      TEGRABL_BINARY_MAX
      ));
    return EFI_INVALID_PARAMETER;
  }

  PartitionDesc       = &CpuBootloaderParams->PartitionInfo[PartitionIndex][PRIMARY_COPY];
  *DeviceInstance     = PartitionDesc->DeviceInstance;
  *PartitionStartByte = PartitionDesc->StartBlock * BLOCK_SIZE;
  *PartitionSizeBytes = PartitionDesc->Size;
  if (PcdGetBool (PcdCapsulePartitionEnabled)) {
    if (PartitionIndex == TEGRABL_RAS_ERROR_LOGS) {
      if (PcdGet64 (PcdCapsulePartitionSize) < *PartitionSizeBytes) {
        *PartitionStartByte += PcdGet64 (PcdCapsulePartitionSize);
        *PartitionSizeBytes -= PcdGet64 (PcdCapsulePartitionSize);
        DEBUG ((DEBUG_ERROR, "%a: capsule partition allocated 0x%x\n", __FUNCTION__, PcdGet64 (PcdCapsulePartitionSize)));
      }
    }
  }

  return EFI_SUCCESS;
}

/**
 * Check if TPM is requested to be enabled.
**/
BOOLEAN
EFIAPI
IsTpmToBeEnabled (
  VOID
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;
  UINTN               CpuBootloaderAddress;

  CpuBootloaderAddress = GetCPUBLBaseAddress ();
  CpuBootloaderParams  = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return CpuBootloaderParams->EarlyBootVariables->Data.Mb1Data.FeatureData.TpmEnable;
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

  Status = T410CpuC2cMode (PlatformResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = T410InitFloorSweepingInfo (PlatformResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

/**
  Invalidate Active Boot Chain

**/
EFI_STATUS
EFIAPI
InValidateActiveBootChain (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Get Total Core Count in case system supports software core disable

**/
EFI_STATUS
EFIAPI
SocSupportsSoftwareCoreDisable (
  IN UINT32   Socket,
  OUT UINT32  *TotalCoreCount
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Check if given boot chain is marked failed

**/
BOOLEAN
EFIAPI
BootChainIsFailed (
  IN UINT32  BootChain
  )
{
  return FALSE;
}

/**
  Set the status of the inactive boot chain

**/
EFI_STATUS
EFIAPI
SetInactiveBootChainStatus (
  IN BOOLEAN  SetGoodStatus
  )
{
  return EFI_SUCCESS;
}

/**
  Clear the UpdateBrBct flag

**/
VOID
EFIAPI
ClearUpdateBrBctFlag (
  VOID
  )
{
}

/**
  Get Partition information.

**/
EFI_STATUS
EFIAPI
GetPartitionInfo (
  IN  UINT32  PartitionIndex,
  OUT UINT16  *DeviceInstance,
  OUT UINT64  *PartitionStartByte,
  OUT UINT64  *PartitionSizeBytes
  )
{
  return T410GetPartitionInfo (
           GetCPUBLBaseAddress (),
           PartitionIndex,
           DeviceInstance,
           PartitionStartByte,
           PartitionSizeBytes
           );
}

/**
  Get Partition information in Standalone MM image.

**/
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
  return T410GetPartitionInfo (
           CpuBlAddress,
           PartitionIndex,
           DeviceInstance,
           PartitionStartByte,
           PartitionSizeBytes
           );
}

/**
  Return PCIe interface number from ID

**/
UINT32
EFIAPI
PcieIdToInterface (
  IN UINT32  PcieId
  )
{
  return PcieId & T410_PCIE_ID_TO_INTERFACE_MASK;
}

/**
  Return socket from PCIe ID

**/
UINT32
EFIAPI
PcieIdToSocket (
  IN UINT32  PcieId
  )
{
  return (PcieId >> T410_PCIE_ID_TO_SOCKET_SHIFT) & T410_SOCKET_MASK;
}

/**
  Get GIC information

**/
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

/**
  Retrieve GR Blob Address

**/
UINT64
EFIAPI
GetGRBlobBaseAddress (
  VOID
  )
{
  return 0x0;
}

/**
  Get the active boot chain from Standalone MM image

**/
EFI_STATUS
EFIAPI
GetActiveBootChainStMm (
  IN  UINTN   ScratchBase,
  OUT UINT32  *BootChain
  )
{
  return EFI_UNSUPPORTED;
}

UINT32
EFIAPI
DramAddrToSocket (
  IN EFI_PHYSICAL_ADDRESS  DramAddr
  )
{
  return (((UINT64)DramAddr & T410_DRAM_START_MASK) >> T410_DRAM_START_SHFT);
}

/**
  Set the next boot chain

**/
EFI_STATUS
EFIAPI
SetNextBootChain (
  IN  UINT32  BootChain
  )
{
  return EFI_UNSUPPORTED;
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
}

/**
  Get the rootfs status register

**/
EFI_STATUS
EFIAPI
GetRootfsStatusReg (
  IN UINT32  *RegisterValue
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Set the rootfs status register

**/
EFI_STATUS
EFIAPI
SetRootfsStatusReg (
  IN UINT32  RegisterValue
  )
{
  return EFI_UNSUPPORTED;
}
