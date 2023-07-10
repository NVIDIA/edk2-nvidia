/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "T234ResourceConfig.h"

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/DramCarveoutLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/GoldenRegisterLib.h>
#include <Library/MceAriLib.h>
#include <Library/IoLib.h>
#include "T234ResourceConfigPrivate.h"
#include <T234/T234Definitions.h>
#include <Protocol/Eeprom.h>

#define T234_MAX_CPUS  12

TEGRA_MMIO_INFO  T234MmioInfo[] = {
  {
    FixedPcdGet64 (PcdTegraCombinedUartTxMailbox),
    SIZE_4KB
  },
  {
    FixedPcdGet64 (PcdTegraMCBBaseAddress),
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
T234UARTInstanceInfo (
  IN  UINT32                UARTInstanceNumber,
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
  )
{
  EFI_PHYSICAL_ADDRESS  UARTBaseAddress[] = {
    0x0,
    TEGRA_UART_ADDRESS_A,
    TEGRA_UART_ADDRESS_B,
    TEGRA_UART_ADDRESS_C,
    TEGRA_UART_ADDRESS_D,
    TEGRA_UART_ADDRESS_E,
    TEGRA_UART_ADDRESS_F,
    0x0,
    TEGRA_UART_ADDRESS_H,
    TEGRA_UART_ADDRESS_I,
    TEGRA_UART_ADDRESS_J,
  };

  *UARTInstanceAddress = 0;
  *UARTInstanceType    = TEGRA_UART_TYPE_NONE;

  if (UARTInstanceNumber == TEGRA_UART_TYPE_TCU) {
    *UARTInstanceType = TEGRA_UART_TYPE_TCU;
    return TRUE;
  }

  if ((UARTInstanceNumber >= ARRAY_SIZE (UARTBaseAddress)) ||
      ((BIT (UARTInstanceNumber) & TEGRA_UART_SUPPORT_FLAG) == 0x0))
  {
    return FALSE;
  }

  *UARTInstanceAddress = UARTBaseAddress[UARTInstanceNumber];
  *UARTInstanceType    = TEGRA_UART_TYPE_16550;
  if ((BIT (UARTInstanceNumber) & TEGRA_UART_SUPPORT_SBSA) != 0x0) {
    *UARTInstanceType = TEGRA_UART_TYPE_SBSA;
  }

  return TRUE;
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
T234GetResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
  )
{
  TEGRA_CPUBL_PARAMS      *CpuBootloaderParams;
  NVDA_MEMORY_REGION      *DramRegions;
  NVDA_MEMORY_REGION      *CarveoutRegions;
  UINTN                   CarveoutRegionsCount = 0;
  NVDA_MEMORY_REGION      *UsableCarveoutRegions;
  UINTN                   UsableCarveoutRegionsCount = 0;
  EFI_MEMORY_DESCRIPTOR   Descriptor;
  UINTN                   Index;
  BOOLEAN                 BanketDramEnabled;
  UINT32                  *DramPageRetirementInfo;
  TEGRA_MMIO_INFO *CONST  FrameBufferMmioInfo = &T234MmioInfo[T234_FRAME_BUFFER_MMIO_INFO_INDEX];

  CpuBootloaderParams          = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  PlatformInfo->DtbLoadAddress = T234GetDTBBaseAddress ((UINTN)CpuBootloaderParams);

  BanketDramEnabled = CPUBL_PARAMS (CpuBootloaderParams, FeatureFlagData.EnableBlanketNsdramCarveout);

  // Build dram regions
  if (BanketDramEnabled) {
    DEBUG ((EFI_D_ERROR, "DRAM Encryption Enabled\n"));
    // When blanket dram is enabled, uefi should use only memory in nsdram carveout
    // and interworld shmem carveout.
    DramRegions = (NVDA_MEMORY_REGION *)AllocatePool (2 * sizeof (NVDA_MEMORY_REGION));
    ASSERT (DramRegions != NULL);
    if (DramRegions == NULL) {
      return EFI_DEVICE_ERROR;
    }

    DramRegions[0].MemoryBaseAddress   = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_BLANKET_NSDRAM].Base);
    DramRegions[0].MemoryLength        = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_BLANKET_NSDRAM].Size);
    DramRegions[1].MemoryBaseAddress   = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_CCPLEX_INTERWORLD_SHMEM].Base);
    DramRegions[1].MemoryLength        = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_CCPLEX_INTERWORLD_SHMEM].Size);
    PlatformInfo->DramRegions          = DramRegions;
    PlatformInfo->DramRegionsCount     = 2;
    PlatformInfo->UefiDramRegionsCount = 2;
  } else {
    DEBUG ((EFI_D_ERROR, "DRAM Encryption Disabled\n"));
    DramRegions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (NVDA_MEMORY_REGION));
    ASSERT (DramRegions != NULL);
    if (DramRegions == NULL) {
      return EFI_DEVICE_ERROR;
    }

    DramRegions->MemoryBaseAddress     = TegraGetSystemMemoryBaseAddress (T234_CHIP_ID);
    DramRegions->MemoryLength          = CPUBL_PARAMS (CpuBootloaderParams, SdramSize);
    PlatformInfo->DramRegions          = DramRegions;
    PlatformInfo->DramRegionsCount     = 1;
    PlatformInfo->UefiDramRegionsCount = 1;
  }

  // Build Carveout regions
  CarveoutRegions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (NVDA_MEMORY_REGION) * (CARVEOUT_OEM_COUNT + NUM_DRAM_BAD_PAGES));
  ASSERT (CarveoutRegions != NULL);
  if (CarveoutRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  UsableCarveoutRegions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (NVDA_MEMORY_REGION) * CARVEOUT_OEM_COUNT);
  ASSERT (UsableCarveoutRegions != NULL);
  if (UsableCarveoutRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  for (Index = CARVEOUT_NONE; Index < CARVEOUT_OEM_COUNT; Index++) {
    if ((CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base) == 0) ||
        (CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size) == 0))
    {
      continue;
    }

    DEBUG ((
      EFI_D_ERROR,
      "Carveout %d Region: Base: 0x%016lx, Size: 0x%016lx\n",
      Index,
      CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base),
      CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size)
      ));
    if (Index == CARVEOUT_CCPLEX_INTERWORLD_SHMEM) {
      // Leave in memory map but marked as used
      EFI_MEMORY_TYPE  MemoryType;

      if (FixedPcdGetBool (PcdExposeCcplexInterworldShmem)) {
        MemoryType = EfiBootServicesData;
      } else {
        MemoryType = EfiReservedMemoryType;
      }

      if (BanketDramEnabled) {
        MemoryType = EfiReservedMemoryType;
      }

      BuildMemoryAllocationHob (
        CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base),
        EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size))),
        MemoryType
        );

      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size);
      UsableCarveoutRegionsCount++;
    } else if (Index == CARVEOUT_BLANKET_NSDRAM) {
      // Skip CARVEOUT_BLANKET_NSDRAM if blanket dram is enabled as this is a placeholder
      // for BL carveout for BL to program GSC for usable DRAM.
      if (BanketDramEnabled) {
        continue;
      }
    } else if (Index == CARVEOUT_RCM_BLOB) {
      // Leave in memory map but marked as used
      BuildMemoryAllocationHob (
        CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base),
        EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size))),
        EfiBootServicesData
        );

      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size);
      UsableCarveoutRegionsCount++;
    } else if (Index == CARVEOUT_UEFI) {
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size);
      UsableCarveoutRegionsCount++;
    } else if (Index == CARVEOUT_OS) {
      // Leave in memory map but marked as used
      BuildMemoryAllocationHob (
        CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base),
        EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size))),
        EfiReservedMemoryType
        );

      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size);
      UsableCarveoutRegionsCount++;

      Descriptor.Type          = EfiReservedMemoryType;
      Descriptor.PhysicalStart = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
      Descriptor.VirtualStart  = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
      Descriptor.NumberOfPages = EFI_SIZE_TO_PAGES (CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size));
      Descriptor.Attribute     = 0;
      BuildGuidDataHob (&gNVIDIAOSCarveoutHob, &Descriptor, sizeof (Descriptor));
    } else if ((Index == CARVEOUT_GR) || (Index == CARVEOUT_PROFILING)) {
      // Leave in memory map but marked as used
      BuildMemoryAllocationHob (
        CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base),
        EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size))),
        EfiReservedMemoryType
        );

      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
      UsableCarveoutRegions[UsableCarveoutRegionsCount].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size);
      UsableCarveoutRegionsCount++;
    } else if (Index == CARVEOUT_DISP_EARLY_BOOT_FB) {
      FrameBufferMmioInfo->Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
      FrameBufferMmioInfo->Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size);
    }

    CarveoutRegions[CarveoutRegionsCount].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base);
    CarveoutRegions[CarveoutRegionsCount].MemoryLength      = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size);
    CarveoutRegionsCount++;
  }

  if (CPUBL_PARAMS (CpuBootloaderParams, FeatureFlagData.EnableDramPageRetirement)) {
    DramPageRetirementInfo = (UINT32 *)CPUBL_PARAMS (CpuBootloaderParams, DramPageRetirementInfoAddress);
    for (Index = 0; Index < NUM_DRAM_BAD_PAGES; Index++) {
      if (DramPageRetirementInfo[Index] == 0) {
        break;
      } else {
        /* Convert badpage index to 64K badpage address */
        CarveoutRegions[CarveoutRegionsCount].MemoryBaseAddress = DramPageRetirementInfo[Index] * ((UINT64)SIZE_64KB);
        CarveoutRegions[CarveoutRegionsCount].MemoryLength      = SIZE_64KB;
        DEBUG ((
          EFI_D_ERROR,
          "Retired DRAM Region %u: Base: 0x%016lx, Size: 0x%016lx\n",
          Index,
          CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Base),
          CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[Index].Size)
          ));
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
T234GetDramPageBlacklistInfoAddress (
  IN  UINTN  CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS  *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  T234DramPageBlacklistInfoAddress[0].MemoryBaseAddress = CPUBL_PARAMS (CpuBootloaderParams, DramPageRetirementInfoAddress) & ~EFI_PAGE_MASK;
  T234DramPageBlacklistInfoAddress[0].MemoryLength      = SIZE_64KB;

  return T234DramPageBlacklistInfoAddress;
}

/**
  Retrieve DTB Address

**/
UINT64
T234GetDTBBaseAddress (
  IN UINTN  CpuBootloaderAddress
  )
{
  UINT64  GrBlobBase;

  GrBlobBase = T234GetGRBlobBaseAddress (CpuBootloaderAddress);

  if (ValidateGrBlobHeader (GrBlobBase) == EFI_SUCCESS) {
    return GrBlobBase + GrBlobBinarySize (GrBlobBase);
  }

  return GrBlobBase;
}

/**
  Retrieve GR Blob Address

**/
UINT64
T234GetGRBlobBaseAddress (
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
  MemoryBase          = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_UEFI].Base);
  MemorySize          = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_UEFI].Size);
  FvOffset            = 0;

  while (FvOffset < MemorySize) {
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER *)(VOID *)(MemoryBase + FvOffset);
    if (FvHeader->Signature == EFI_FVH_SIGNATURE) {
      break;
    }

    FvOffset += SIZE_64KB;
  }

  ASSERT (FvOffset < MemorySize);
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
T234GetMmioBaseAndSize (
  VOID
  )
{
  return T234MmioInfo;
}

/**
  Retrieve EEPROM Data

**/
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

  T234EepromData = (T234_EEPROM_DATA *)EepromData->CvbEepromData;
  CopyMem ((VOID *)&BoardInfo->CvbProductId, (VOID *)&T234EepromData->PartNumber, sizeof (T234EepromData->PartNumber));

  return TRUE;
}

/**
  Retrieve Active Boot Chain Information

**/
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
T234ValidateActiveBootChain (
  IN  UINTN  CpuBootloaderAddress
  )
{
  EFI_STATUS  Status;
  UINT32      BootChain;

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
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
T234GetPlatformResourceInformation (
  IN UINTN                         CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
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

  // Populate FrameBufferInfo
  PlatformResourceInfo->FrameBufferInfo.Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_DISP_EARLY_BOOT_FB].Base);
  PlatformResourceInfo->FrameBufferInfo.Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_DISP_EARLY_BOOT_FB].Size);

  // Populate ProfilerInfo
  PlatformResourceInfo->ProfilerInfo.Base = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_PROFILING].Base);
  PlatformResourceInfo->ProfilerInfo.Size = CPUBL_PARAMS (CpuBootloaderParams, CarveoutInfo[CARVEOUT_PROFILING].Size);

  PlatformResourceInfo->BootType = CPUBL_PARAMS (CpuBootloaderParams, BootType);

  return EFI_SUCCESS;
}

/**
  Get Rootfs Status Register

**/
EFI_STATUS
EFIAPI
T234GetRootfsStatusReg (
  IN  UINTN   CpuBootloaderAddress,
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
T234SetRootfsStatusReg (
  IN  UINTN   CpuBootloaderAddress,
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
T234SetNextBootChain (
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
