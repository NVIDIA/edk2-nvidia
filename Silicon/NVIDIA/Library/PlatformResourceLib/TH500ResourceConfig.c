/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <TH500/TH500Definitions.h>
#include "CommonResourceConfig.h"

#include "PlatformResourceConfig.h"
#include "TH500ResourceConfig.h"
#include "TH500ResourceConfigPrivate.h"

#define MAX_CORE_DISABLE_WORDS  3

STATIC UINT64  TH500SocketScratchBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_SCRATCH_BASE_SOCKET_0,
  TH500_SCRATCH_BASE_SOCKET_1,
  TH500_SCRATCH_BASE_SOCKET_2,
  TH500_SCRATCH_BASE_SOCKET_3,
};

STATIC UINT32  TH500CoreDisableScratchOffset[MAX_CORE_DISABLE_WORDS] = {
  TH500_CPU_FLOORSWEEPING_DISABLE_OFFSET_0,
  TH500_CPU_FLOORSWEEPING_DISABLE_OFFSET_1,
  TH500_CPU_FLOORSWEEPING_DISABLE_OFFSET_2,
};

STATIC UINT32  TH500CoreDisableScratchMask[MAX_CORE_DISABLE_WORDS] = {
  TH500_CPU_FLOORSWEEPING_DISABLE_MASK_0,
  TH500_CPU_FLOORSWEEPING_DISABLE_MASK_1,
  TH500_CPU_FLOORSWEEPING_DISABLE_MASK_2,
};

STATIC COMMON_RESOURCE_CONFIG_INFO  TH500CommonResourceConfigInfo = {
  MAX_CORE_DISABLE_WORDS,
  FALSE,
  MAX_UINT32,
  TH500SocketScratchBaseAddr,
  TH500CoreDisableScratchOffset,
  TH500CoreDisableScratchMask,
};

TEGRA_MMIO_INFO  TH500MmioInfo[] = {
  {
    TH500_WDT_CTRL_BASE,
    SIZE_4KB
  },
  {
    TH500_WDT_RFRSH_BASE,
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

#define TH500_CCPLEX_INTERWORLD_SHMEM_MMIO_INFO_INDEX  (ARRAY_SIZE (TH500MmioInfo) - 2)

TEGRA_MMIO_INFO  TH500GicRedistributorMmioInfo[] = {
  {
    TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_0,
    SIZE_256KB *TH500_GIC_REDISTRIBUTOR_INSTANCES
  },
  {
    TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_1,
    SIZE_256KB *TH500_GIC_REDISTRIBUTOR_INSTANCES
  },
  {
    TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_2,
    SIZE_256KB *TH500_GIC_REDISTRIBUTOR_INSTANCES
  },
  {
    TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_3,
    SIZE_256KB *TH500_GIC_REDISTRIBUTOR_INSTANCES
  },
};

TEGRA_MMIO_INFO  TH500GicItsMmioInfo[] = {
  {
    TH500_GIC_ITS_BASE_SOCKET_0,
    SIZE_64KB
  },
  {
    TH500_GIC_ITS_BASE_SOCKET_1,
    SIZE_64KB
  },
  {
    TH500_GIC_ITS_BASE_SOCKET_2,
    SIZE_64KB
  },
  {
    TH500_GIC_ITS_BASE_SOCKET_3,
    SIZE_64KB
  },
};

TEGRA_MMIO_INFO  TH500SocketScratchMmioInfo[] = {
  {
    TH500_SCRATCH_BASE_SOCKET_0,
    TH500_SCRATCH_SIZE
  },
  {
    TH500_SCRATCH_BASE_SOCKET_1,
    TH500_SCRATCH_SIZE
  },
  {
    TH500_SCRATCH_BASE_SOCKET_2,
    TH500_SCRATCH_SIZE
  },
  {
    TH500_SCRATCH_BASE_SOCKET_3,
    TH500_SCRATCH_SIZE
  },
};

TEGRA_MMIO_INFO  TH500SocketCbbMmioInfo[] = {
  {
    TH500_CBB_FABRIC_BASE_SOCKET_0,
    TH500_CBB_FABRIC_SIZE
  },
  {
    TH500_CBB_FABRIC_BASE_SOCKET_1,
    TH500_CBB_FABRIC_SIZE
  },
  {
    TH500_CBB_FABRIC_BASE_SOCKET_2,
    TH500_CBB_FABRIC_SIZE
  },
  {
    TH500_CBB_FABRIC_BASE_SOCKET_3,
    TH500_CBB_FABRIC_SIZE
  },
};

TEGRA_MMIO_INFO  TH500SocketMssMmioInfo[] = {
  {
    TH500_MSS_BASE_SOCKET_0,
    TH500_MSS_SIZE
  },
  {
    TH500_MSS_BASE_SOCKET_1,
    TH500_MSS_SIZE
  },
  {
    TH500_MSS_BASE_SOCKET_2,
    TH500_MSS_SIZE
  },
  {
    TH500_MSS_BASE_SOCKET_3,
    TH500_MSS_SIZE
  },
};

TEGRA_MMIO_INFO  TH500SocketMcfSmmuMmioInfo[] = {
  {
    TH500_MCF_SMMU_SOCKET_0,
    SIZE_4KB
  },
  {
    TH500_MCF_SMMU_SOCKET_1,
    SIZE_4KB
  },
  {
    TH500_MCF_SMMU_SOCKET_2,
    SIZE_4KB
  },
  {
    TH500_MCF_SMMU_SOCKET_3,
    SIZE_4KB
  },
};

NVDA_MEMORY_REGION  TH500DramPageBlacklistInfoAddress[] = {
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

TEGRA_BASE_AND_SIZE_INFO  TH500EgmMemoryInfo[TH500_MAX_SOCKETS]  = { };
TEGRA_DRAM_DEVICE_INFO    TH500DramDeviceInfo[TH500_MAX_SOCKETS] = { };
UINT8                     TH500C2cMode[TH500_MAX_SOCKETS]        = { };

/**
  Get Socket Mask

**/
UINT32
EFIAPI
TH500GetSocketMask (
  IN UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;
  UINT32              SocketMask;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  SocketMask = CpuBootloaderParams->SocketMask;
  ASSERT (SocketMask != 0);
  ASSERT (HighBitSet32 (SocketMask) < TH500_MAX_SOCKETS);

  return SocketMask;
}

/**
  Retrieve UART Instance Info

  This function retrieves the base address of that UART instance, and sets the known UART type
  based on the UART instance number.

  @param[in]  UARTInstanceNumber    UART instance number
  @param[out] UARTInstanceType      UART instance type
  @param[out] UARTInstanceAddress   UART instance address

  @retval TRUE    UART instance info was successfullly retrieved
  @retval FALSE   Retrieval of UART instance info failed

**/
BOOLEAN
TH500UARTInstanceInfo (
  IN  UINT32                UARTInstanceNumber,
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
  )
{
  EFI_PHYSICAL_ADDRESS  UARTBaseAddress[] = {
    0x0,
    TEGRA_UART_ADDRESS_0,
    TEGRA_UART_ADDRESS_1,
  };

  *UARTInstanceAddress = 0;
  *UARTInstanceType    = TEGRA_UART_TYPE_NONE;

  if ((UARTInstanceNumber >= ARRAY_SIZE (UARTBaseAddress)) ||
      ((BIT (UARTInstanceNumber) & TEGRA_UART_SUPPORT_FLAG) == 0x0))
  {
    return FALSE;
  }

  *UARTInstanceType    = TEGRA_UART_TYPE_SBSA;
  *UARTInstanceAddress = UARTBaseAddress[UARTInstanceNumber];

  return TRUE;
}

/**
   Retrieve the TH500 memory mode.

   @param[in] CpuBootloaderParams  CPU BL params.
*/
STATIC
TH500_MEMORY_MODE
TH500GetMemoryMode (
  IN CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams
  )
{
  EFI_PHYSICAL_ADDRESS  EgmBase, HvBase;
  UINT64                EgmSize, HvSize;

  EgmBase = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_EGM].Base;
  EgmSize = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_EGM].Size;

  HvBase = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_HV].Base;
  HvSize = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_HV].Size;

  if ((EgmBase == 0) || (EgmSize == 0)) {
    return Th500MemoryModeNormal;
  } else if ((HvBase == 0) || (HvSize == 0)) {
    return Th500MemoryModeEgmNoHv;
  } else {
    return Th500MemoryModeEgmWithHv;
  }
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
TH500BuildDramRegions (
  IN  CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams,
  OUT NVDA_MEMORY_REGION      **CONST  DramRegions,
  OUT UINTN                    *CONST  DramRegionCount
  )
{
  NVDA_MEMORY_REGION       *Regions;
  UINTN                    RegionCount, RegionCountMax;
  UINTN                    Socket;
  EFI_PHYSICAL_ADDRESS     Base;
  UINT64                   Size;
  CONST UINT32             SocketMask = TH500GetSocketMask ((EFI_PHYSICAL_ADDRESS)CpuBootloaderParams);
  CONST TH500_MEMORY_MODE  MemoryMode = TH500GetMemoryMode (CpuBootloaderParams);
  CONST UINT32             MaxSocket  = HighBitSet32 (SocketMask);

  DEBUG ((DEBUG_ERROR, "SocketMask=0x%x\n", SocketMask));

  switch (MemoryMode) {
    case Th500MemoryModeNormal:
      DEBUG ((DEBUG_ERROR, "Memory Mode: Normal\n"));
      RegionCountMax = MaxSocket + 1;
      break;

    case Th500MemoryModeEgmWithHv:
      DEBUG ((DEBUG_ERROR, "Memory Mode: EGM With HV\n"));
      RegionCountMax = MaxSocket + 1;
      break;

    case Th500MemoryModeEgmNoHv:
      // When egm is enabled without hv, uefi should use only memory in
      // egm carveout on all sockets and rcm, os and uefi carveouts only
      // on primary socket.
      DEBUG ((DEBUG_ERROR, "Memory Mode: EGM No HV\n"));
      RegionCountMax = (MaxSocket + 1) + 3;
      break;

    default:
      UNREACHABLE ();
  }

  Regions = (NVDA_MEMORY_REGION *)AllocatePool (RegionCountMax * sizeof (*Regions));
  NV_ASSERT_RETURN (
    Regions != NULL,
    return EFI_DEVICE_ERROR,
    "%a: Failed to allocate %lu DRAM regions\r\n",
    __FUNCTION__,
    (UINT64)RegionCountMax
    );

  RegionCount = 0;

  for (Socket = TH500_PRIMARY_SOCKET; Socket < TH500_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    switch (MemoryMode) {
      case Th500MemoryModeNormal:
      case Th500MemoryModeEgmWithHv:
        Base = CpuBootloaderParams->SdramInfo[Socket].Base;
        Size = CpuBootloaderParams->SdramInfo[Socket].Size;
        PlatformResourceAddMemoryRegion (Regions, &RegionCount, Base, Size);
        break;

      case Th500MemoryModeEgmNoHv:
        Base = CpuBootloaderParams->CarveoutInfo[Socket][CARVEOUT_EGM].Base;
        Size = CpuBootloaderParams->CarveoutInfo[Socket][CARVEOUT_EGM].Size;
        PlatformResourceAddMemoryRegion (Regions, &RegionCount, Base, Size);

        if (Socket == TH500_PRIMARY_SOCKET) {
          Base = CpuBootloaderParams->CarveoutInfo[Socket][CARVEOUT_RCM_BLOB].Base;
          Size = CpuBootloaderParams->CarveoutInfo[Socket][CARVEOUT_RCM_BLOB].Size;
          PlatformResourceAddMemoryRegion (Regions, &RegionCount, Base, Size);

          Base = CpuBootloaderParams->CarveoutInfo[Socket][CARVEOUT_OS].Base;
          Size = CpuBootloaderParams->CarveoutInfo[Socket][CARVEOUT_OS].Size;
          PlatformResourceAddMemoryRegion (Regions, &RegionCount, Base, Size);

          Base = CpuBootloaderParams->CarveoutInfo[Socket][CARVEOUT_UEFI].Base;
          Size = CpuBootloaderParams->CarveoutInfo[Socket][CARVEOUT_UEFI].Size;
          PlatformResourceAddMemoryRegion (Regions, &RegionCount, Base, Size);
        }

        break;

      default:
        UNREACHABLE ();
    }
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
   @param[in]     MemoryMode         TH500 memory mode.
   @param[in]     Socket             CPU socket index.
   @param[in]     Carveouts          Bootloader carveouts.
   @param[in]     CarveoutCount      Number of bootloader carveouts.
*/
STATIC
VOID
TH500AddBootloaderCarveouts (
  IN     NVDA_MEMORY_REGION          *CONST  Regions,
  IN OUT UINTN                       *CONST  RegionCount,
  IN     NVDA_MEMORY_REGION          *CONST  UsableRegions,
  IN OUT UINTN                       *CONST  UsableRegionCount,
  IN     CONST TH500_MEMORY_MODE             MemoryMode,
  IN     CONST UINTN                         Socket,
  IN     CONST TEGRABL_CARVEOUT_INFO *CONST  Carveouts,
  IN     CONST UINTN                         CarveoutCount
  )
{
  UINTN                 Index;
  EFI_PHYSICAL_ADDRESS  Base;
  UINT64                Size, Pages;

  TEGRA_MMIO_INFO *CONST  CcplexInterworldShmemMmioInfo =
    &TH500MmioInfo[TH500_CCPLEX_INTERWORLD_SHMEM_MMIO_INFO_INDEX];

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
      case CARVEOUT_RCM_BLOB:
      case CARVEOUT_UEFI:
      case CARVEOUT_OS:
        // Leave in memory map but marked as used on socket 0
        if (Socket == TH500_PRIMARY_SOCKET) {
          Pages = EFI_SIZE_TO_PAGES (Size);
          BuildMemoryAllocationHob (Base, EFI_PAGES_TO_SIZE (Pages), EfiReservedMemoryType);
          PlatformResourceAddMemoryRegion (UsableRegions, UsableRegionCount, Base, Size);
        }

        break;

      case CARVEOUT_HV:
        continue;

      case CARVEOUT_EGM:
        if (MemoryMode == Th500MemoryModeEgmNoHv) {
          continue;
        }

        break;

      case CARVEOUT_CCPLEX_INTERWORLD_SHMEM:
        if (Socket == TH500_PRIMARY_SOCKET) {
          // For primary socket, add memory in DRAM CO CARVEOUT_CCPLEX_INTERWORLD_SHMEM in its placeholder
          // in TH500MmioInfo for MMIO mapping.
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
TH500BuildCarveoutRegions (
  IN  CONST TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams,
  OUT NVDA_MEMORY_REGION      **CONST  CarveoutRegions,
  OUT UINTN                    *CONST  CarveoutRegionCount,
  OUT NVDA_MEMORY_REGION      **CONST  UsableCarveoutRegions,
  OUT UINTN                    *CONST  UsableCarveoutRegionCount
  )
{
  NVDA_MEMORY_REGION       *Regions, *UsableRegions;
  UINTN                    RegionCount, UsableRegionCount;
  UINTN                    Socket;
  EFI_PHYSICAL_ADDRESS     *RetiredDramPageList;
  CONST UINT32             SocketMask            = TH500GetSocketMask ((EFI_PHYSICAL_ADDRESS)CpuBootloaderParams);
  CONST TH500_MEMORY_MODE  MemoryMode            = TH500GetMemoryMode (CpuBootloaderParams);
  CONST UINT32             MaxSocket             = HighBitSet32 (SocketMask);
  CONST UINTN              RegionCountMax        = (UINTN)(MaxSocket + 1) * (CARVEOUT_OEM_COUNT + MAX_RETIRED_DRAM_PAGES);
  CONST UINTN              RegionsPagesMax       = EFI_SIZE_TO_PAGES (RegionCountMax * sizeof (*Regions));
  CONST UINTN              UsableRegionCountMax  = (UINTN)(MaxSocket + 1) * CARVEOUT_OEM_COUNT;
  CONST UINTN              UsableRegionsPagesMax = EFI_SIZE_TO_PAGES (UsableRegionCountMax * sizeof (*UsableRegions));

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
    return EFI_DEVICE_ERROR,
    "%a: Failed to allocate %lu pages for usable carveout regions\r\n",
    __FUNCTION__,
    (UINT64)UsableRegionsPagesMax
    );

  RegionCount = UsableRegionCount = 0;

  for (Socket = TH500_PRIMARY_SOCKET; Socket < TH500_MAX_SOCKETS; Socket++) {
    if ((SocketMask & (1 << Socket)) != 0) {
      TH500AddBootloaderCarveouts (
        Regions,
        &RegionCount,
        UsableRegions,
        &UsableRegionCount,
        MemoryMode,
        Socket,
        CpuBootloaderParams->CarveoutInfo[Socket],
        CARVEOUT_OEM_COUNT
        );
    }
  }

  for (Socket = TH500_PRIMARY_SOCKET; Socket < TH500_MAX_SOCKETS; Socket++) {
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
TH500GetResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
  )
{
  EFI_STATUS          Status;
  NVDA_MEMORY_REGION  *DramRegions, *CarveoutRegions, *UsableCarveoutRegions;
  UINTN               DramRegionCount, CarveoutRegionCount, UsableCarveoutRegionCount;

  TEGRA_CPUBL_PARAMS *CONST  CpuBootloaderParams =
    (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  Status = TH500BuildDramRegions (
             CpuBootloaderParams,
             &DramRegions,
             &DramRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = TH500BuildCarveoutRegions (
             CpuBootloaderParams,
             &CarveoutRegions,
             &CarveoutRegionCount,
             &UsableCarveoutRegions,
             &UsableCarveoutRegionCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PlatformInfo->DtbLoadAddress             = TH500GetDTBBaseAddress ((UINTN)CpuBootloaderParams);
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
TH500GetDramPageBlacklistInfoAddress (
  IN  UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;
  UINT32              SocketMask;
  UINTN               Socket;
  UINTN               Index;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  SocketMask          = TH500GetSocketMask (CpuBootloaderAddress);

  Index = 0;
  for (Socket = TH500_PRIMARY_SOCKET; Socket < TH500_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    if (CpuBootloaderParams->RetiredDramPageListAddr[Socket] != 0) {
      TH500DramPageBlacklistInfoAddress[Index].MemoryBaseAddress = CpuBootloaderParams->RetiredDramPageListAddr[Socket] & ~EFI_PAGE_MASK;
      TH500DramPageBlacklistInfoAddress[Index].MemoryLength      = SIZE_64KB;
      Index++;
    }
  }

  return TH500DramPageBlacklistInfoAddress;
}

/**
  Retrieve DTB Address

**/
UINT64
TH500GetDTBBaseAddress (
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
  MemoryBase          = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_UEFI].Base;
  MemorySize          = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_UEFI].Size;
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
TH500GetMmioBaseAndSize (
  IN UINT32  SocketMask
  )
{
  TEGRA_MMIO_INFO  *MmioInfo;
  TEGRA_MMIO_INFO  *MmioInfoEnd;
  UINTN            Socket;

  MmioInfo = AllocateZeroPool (
               sizeof (TH500MmioInfo) +
               (TH500_MAX_SOCKETS * 6 * sizeof (TEGRA_MMIO_INFO))
               );
  CopyMem (MmioInfo, TH500MmioInfo, sizeof (TH500MmioInfo));

  // point to the table terminating entry copied from TH500MmioInfo
  MmioInfoEnd = MmioInfo + (sizeof (TH500MmioInfo) / sizeof (TEGRA_MMIO_INFO)) - 1;

  for (Socket = 0; Socket < TH500_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    CopyMem (MmioInfoEnd++, &TH500GicRedistributorMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &TH500GicItsMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &TH500SocketScratchMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &TH500SocketCbbMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &TH500SocketMssMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
    CopyMem (MmioInfoEnd++, &TH500SocketMcfSmmuMmioInfo[Socket], sizeof (TEGRA_MMIO_INFO));
  }

  return MmioInfo;
}

/**
  Retrieve Active Boot Chain

**/
EFI_STATUS
EFIAPI
TH500GetActiveBootChain (
  IN  UINTN   CpuBootloaderAddress,
  IN  UINTN   Socket,
  OUT UINT32  *BootChain
  )
{
  UINT64  ScratchAddr;

  ScratchAddr = TH500SocketScratchMmioInfo[Socket].Base + TH500_BOOT_CHAIN_SCRATCH_OFFSET;

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
TH500ValidateActiveBootChain (
  IN  UINTN  CpuBootloaderAddress
  )
{
  UINT32      SocketMask;
  UINTN       Socket;
  UINT64      ScratchAddr;
  EFI_STATUS  Status;
  UINT32      BootChain;

  SocketMask = TH500GetSocketMask (CpuBootloaderAddress);
  for (Socket = 0; Socket < TH500_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    Status = TH500GetActiveBootChain (CpuBootloaderAddress, Socket, &BootChain);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: GetActiveBootChain failed socket %u: %r\n", __FUNCTION__, Socket, Status));
      continue;
    }

    ScratchAddr = TH500SocketScratchMmioInfo[Socket].Base + TH500_BOOT_CHAIN_SCRATCH_OFFSET;

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
TH500GetEnabledCoresBitMap (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  UINT32  SatMcCore;
  UINT32  CoresPerSocket;

  // SatMC core is reserved on socket 0.
  CoresPerSocket = PlatformResourceInfo->MaxPossibleCores / PlatformResourceInfo->MaxPossibleSockets;

  SatMcCore = MmioBitFieldRead32 (
                TH500SocketScratchBaseAddr[0] + TH500CoreDisableScratchOffset[MAX_CORE_DISABLE_WORDS-1],
                TH500_CPU_FLOORSWEEPING_SATMC_CORE_BIT_LO,
                TH500_CPU_FLOORSWEEPING_SATMC_CORE_BIT_HI
                );
  if (SatMcCore != TH500_CPU_FLOORSWEEPING_SATMC_CORE_INVALID) {
    ASSERT (SatMcCore <= CoresPerSocket);
    TH500CommonResourceConfigInfo.SatMcSupported = TRUE;
    TH500CommonResourceConfigInfo.SatMcCore      = SatMcCore;
  }

  PlatformResourceInfo->AffinityMpIdrSupported = TRUE;

  return CommonConfigGetEnabledCoresBitMap (&TH500CommonResourceConfigInfo, PlatformResourceInfo);
}

/**
  Get CPU C2C mode from enabled socket. Needs to be called after
  ArmSetMemoryRegionReadOnly to prevent exception.

**/
STATIC
EFI_STATUS
EFIAPI
Th500CpuC2cMode (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  UINTN  Socket;

  if (PlatformResourceInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PlatformResourceInfo->C2cMode = TH500C2cMode;
  for (Socket = 0; Socket < TH500_MAX_SOCKETS; Socket++) {
    if ((PlatformResourceInfo->SocketMask & (1UL << Socket)) == 0) {
      continue;
    }

    TH500C2cMode[Socket] = MmioRead32 (TH500SocketMssMmioInfo[Socket].Base + TH500_MSS_C2C_MODE) & 0x03;
  }

  return EFI_SUCCESS;
}

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
TH500GetPlatformResourceInformation (
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

  SocketMask = TH500GetSocketMask (CpuBootloaderAddress);

  PlatformResourceInfo->SocketMask = SocketMask;

  /* Avoid this step when called from MM */
  if (InMm == FALSE) {
    Status = TH500GetActiveBootChain (CpuBootloaderAddress, 0, &PlatformResourceInfo->ActiveBootChain);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = TH500GetResourceConfig (CpuBootloaderAddress, PlatformResourceInfo->ResourceInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    PlatformResourceInfo->MmioInfo = TH500GetMmioBaseAndSize (SocketMask);
  }

  PlatformResourceInfo->RamdiskOSInfo.Base = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_OS].Base;
  PlatformResourceInfo->RamdiskOSInfo.Size = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_OS].Size;

  PlatformResourceInfo->RcmBlobInfo.Base = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_RCM_BLOB].Base;
  PlatformResourceInfo->RcmBlobInfo.Size = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_RCM_BLOB].Size;

  PlatformResourceInfo->CpublCoInfo.Base = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_UEFI].Base;
  PlatformResourceInfo->CpublCoInfo.Size = CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_UEFI].Size;

  if ((PlatformResourceInfo->RcmBlobInfo.Base != 0) &&
      (PlatformResourceInfo->RcmBlobInfo.Size != 0))
  {
    PlatformResourceInfo->BootType = TegrablBootRcm;
  } else {
    PlatformResourceInfo->BootType = TegrablBootColdBoot;
  }

  if ((CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_EGM].Base != 0) &&
      (CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_EGM].Size != 0))
  {
    if ((CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_HV].Base != 0) &&
        (CpuBootloaderParams->CarveoutInfo[TH500_PRIMARY_SOCKET][CARVEOUT_HV].Size != 0))
    {
      PlatformResourceInfo->HypervisorMode = TRUE;
    }
  }

  PlatformResourceInfo->EgmMemoryInfo = TH500EgmMemoryInfo;
  for (Index = 0; Index < TH500_MAX_SOCKETS; Index++) {
    PlatformResourceInfo->EgmMemoryInfo[Index].Base = CpuBootloaderParams->CarveoutInfo[Index][CARVEOUT_EGM].Base;
    PlatformResourceInfo->EgmMemoryInfo[Index].Size = CpuBootloaderParams->CarveoutInfo[Index][CARVEOUT_EGM].Size;
  }

  PlatformResourceInfo->PhysicalDramSize = 0;
  PlatformResourceInfo->DramDeviceInfo   = TH500DramDeviceInfo;
  // Populate Total Memory.
  for (Index = 0; Index < TH500_MAX_SOCKETS; Index++) {
    if (!(SocketMask & (1UL << Index))) {
      continue;
    }

    PlatformResourceInfo->PhysicalDramSize   += CpuBootloaderParams->SdramInfo[Index].Size;
    TH500DramDeviceInfo[Index].DataWidth      = CpuBootloaderParams->DramInfo[Index].DataWidth;
    TH500DramDeviceInfo[Index].ManufacturerId = CpuBootloaderParams->DramInfo[Index].ManufacturerId;
    TH500DramDeviceInfo[Index].Rank           = CpuBootloaderParams->DramInfo[Index].Rank;
    TH500DramDeviceInfo[Index].SerialNumber   = CpuBootloaderParams->DramInfo[Index].SerialNumber;
    TH500DramDeviceInfo[Index].TotalWidth     = CpuBootloaderParams->DramInfo[Index].TotalWidth;
    TH500DramDeviceInfo[Index].Size           = CpuBootloaderParams->SdramInfo[Index].Size;
    TH500DramDeviceInfo[Index].SpeedKhz       = 0;
  }

  for (Index = 0; Index < TH500_MAX_SOCKETS; Index++) {
    if (!(SocketMask & (1UL << Index))) {
      continue;
    }

    for (Count = 0; Count < UID_NUM_DWORDS; Count++) {
      PlatformResourceInfo->UniqueId[Index][Count] += CpuBootloaderParams->UniqueId[Index][Count];
    }
  }

  BuildGuidDataHob (&gNVIDIATH500MB1DataGuid, &CpuBootloaderParams->EarlyBootVariables, sizeof (CpuBootloaderParams->EarlyBootVariables));
  BuildGuidDataHob (&gNVIDIATH500MB1DefaultDataGuid, &CpuBootloaderParams->EarlyBootVariablesDefaults, sizeof (CpuBootloaderParams->EarlyBootVariablesDefaults));

  Status = TH500BuildTcgEventHob ((UINTN)&CpuBootloaderParams->EarlyTpmCommitLog);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Get Partition information.
**/
EFI_STATUS
EFIAPI
TH500GetPartitionInfo (
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

  return EFI_SUCCESS;
}

/**
 * Check if TPM is requested to be enabled.
**/
BOOLEAN
EFIAPI
TH500IsTpmToBeEnabled (
  IN  UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return CpuBootloaderParams->EarlyBootVariables->Data.Mb1Data.FeatureData.TpmEnable;
}

EFI_STATUS
EFIAPI
TH500UpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  if (PlatformResourceInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return Th500CpuC2cMode (PlatformResourceInfo);
}
