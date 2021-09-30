/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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
*  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
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
#include <Protocol/CvmEeprom.h>

#define T234_MAX_CPUS       12

TEGRA_MMIO_INFO T234MmioInfo[] = {
  {
    FixedPcdGet64(PcdTegraCombinedUartTxMailbox),
    SIZE_4KB
  },
  {
    FixedPcdGet64(PcdTegraCombinedUartRxMailbox),
    SIZE_4KB
  },
  {
    FixedPcdGet64(PcdTegraMCBBaseAddress),
    SIZE_4KB
  },
  {
    T234_GIC_REDISTRIBUTOR_BASE,
    T234_GIC_REDISTRIBUTOR_INSTANCES * SIZE_128KB
  },
  {
    FixedPcdGet64(PcdTegraMceAriApertureBaseAddress),
    MCE_ARI_APERTURE_OFFSET (T234_MAX_CPUS)
  },
  {
    0,
    0
  }
};

TEGRA_FUSE_INFO T234FloorsweepingFuseList[] = {
  {"fuse-disable-isp",   FUSE_OPT_ISP_DISABLE,   BIT(0) },
  {"fuse-disable-nvenc", FUSE_OPT_NVENC_DISABLE, BIT(0)|BIT(1) },
  {"fuse-disable-pva",   FUSE_OPT_PVA_DISABLE,   BIT(0)|BIT(1) },
  {"fuse-disable-dla",   FUSE_OPT_DLA_DISABLE,   BIT(0)|BIT(1) },
  {"fuse-disable-cv",    FUSE_OPT_CV_DISABLE,    BIT(0) },
  {"fuse-disable-nvdec", FUSE_OPT_NVDEC_DISABLE, BIT(0)|BIT(1) }
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
T234UARTInstanceInfo(
  IN  UINT32                UARTInstanceNumber,
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
)
{
  EFI_PHYSICAL_ADDRESS UARTBaseAddress[] = {
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
  *UARTInstanceType = TEGRA_UART_TYPE_NONE;

  if (UARTInstanceNumber == TEGRA_UART_TYPE_TCU) {
    *UARTInstanceType = TEGRA_UART_TYPE_TCU;
    return TRUE;
  }

  if ((UARTInstanceNumber >= ARRAY_SIZE(UARTBaseAddress)) ||
     ((BIT(UARTInstanceNumber) & TEGRA_UART_SUPPORT_FLAG) == 0x0)) {
    return FALSE;
  }

  *UARTInstanceAddress = UARTBaseAddress[UARTInstanceNumber];
  *UARTInstanceType = TEGRA_UART_TYPE_16550;
  if ((BIT(UARTInstanceNumber) & TEGRA_UART_SUPPORT_SBSA) != 0x0) {
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
T234ResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
  )
{
  TEGRA_CPUBL_PARAMS    *CpuBootloaderParams;
  NVDA_MEMORY_REGION    *CarveoutRegions;
  UINTN                 CarveoutRegionsCount=0;
  EFI_MEMORY_DESCRIPTOR Descriptor;
  UINTN                 Index;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  PlatformInfo->SdramSize = CpuBootloaderParams->SdramSize;
  PlatformInfo->DtbLoadAddress = T234GetDTBBaseAddress ((UINTN)CpuBootloaderParams);

  //Build Carveout regions
  CarveoutRegions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (NVDA_MEMORY_REGION) * (CARVEOUT_OEM_COUNT));
  ASSERT (CarveoutRegions != NULL);
  if (CarveoutRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  for (Index = CARVEOUT_NONE; Index < CARVEOUT_OEM_COUNT; Index++) {
    if (CpuBootloaderParams->CarveoutInfo[Index].Size != 0) {
      if (Index == CARVEOUT_CCPLEX_INTERWORLD_SHMEM) {
        //Leave in memory map but marked as used
        EFI_MEMORY_TYPE MemoryType;

        if (FixedPcdGetBool(PcdExposeCcplexInterworldShmem)) {
          if (ValidateGrBlobHeader(GetGRBlobBaseAddress ()) == EFI_SUCCESS) {
            MemoryType = EfiReservedMemoryType;
          } else {
            MemoryType = EfiBootServicesData;
          }
        } else {
          MemoryType = EfiReservedMemoryType;
        }

        BuildMemoryAllocationHob (
          CpuBootloaderParams->CarveoutInfo[Index].Base,
          EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CpuBootloaderParams->CarveoutInfo[Index].Size)),
          MemoryType
        );
      } else if (Index == CARVEOUT_RCM_BLOB) {
        //Leave in memory map but marked as used
        BuildMemoryAllocationHob (
          CpuBootloaderParams->CarveoutInfo[Index].Base,
          EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CpuBootloaderParams->CarveoutInfo[Index].Size)),
          EfiBootServicesData
        );
      } else if (Index == CARVEOUT_OS) {
        //Leave in memory map but marked as used
        BuildMemoryAllocationHob (
          CpuBootloaderParams->CarveoutInfo[Index].Base,
          EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CpuBootloaderParams->CarveoutInfo[Index].Size)),
          EfiReservedMemoryType
        );

        Descriptor.Type = EfiReservedMemoryType;
        Descriptor.PhysicalStart = CpuBootloaderParams->CarveoutInfo[Index].Base;
        Descriptor.VirtualStart = CpuBootloaderParams->CarveoutInfo[Index].Base;
        Descriptor.NumberOfPages = EFI_SIZE_TO_PAGES (CpuBootloaderParams->CarveoutInfo[Index].Size);
        Descriptor.Attribute = 0;
        BuildGuidDataHob (&gNVIDIAOSCarveoutHob, &Descriptor, sizeof (Descriptor));
      } else if ((Index != CARVEOUT_UEFI) &&
                 (Index != CARVEOUT_TEMP_MB2_LOAD) &&
                 (Index != CARVEOUT_TEMP_MB2_IO_BUFFERS) &&
                 (CpuBootloaderParams->CarveoutInfo[Index].Size != 0)) {
        CarveoutRegions[CarveoutRegionsCount].MemoryBaseAddress = CpuBootloaderParams->CarveoutInfo[Index].Base;
        CarveoutRegions[CarveoutRegionsCount].MemoryLength      = CpuBootloaderParams->CarveoutInfo[Index].Size;
        CarveoutRegionsCount++;
      }
    }
  }

  PlatformInfo->CarveoutRegions = CarveoutRegions;
  PlatformInfo->CarveoutRegionsCount = CarveoutRegionsCount;

  return EFI_SUCCESS;
}

/**
  Retrieve DTB Address

**/
UINT64
T234GetDTBBaseAddress (
  IN UINTN CpuBootloaderAddress
  )
{
  UINT64 GrBlobBase;

  GrBlobBase = T234GetGRBlobBaseAddress (CpuBootloaderAddress);

  if (ValidateGrBlobHeader (GrBlobBase) == EFI_SUCCESS) {
    return GrBlobBase + GrBlobBinarySize (GrBlobBase);
  }

  return GrBlobBase;
}

/**
  Retrieve RCM Blob Address

**/
UINT64
T234GetRCMBaseAddress (
  IN UINTN CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return CpuBootloaderParams->CarveoutInfo[CARVEOUT_RCM_BLOB].Base;
}

/**
  Retrieve Boot Type

**/
TEGRA_BOOT_TYPE
T234GetBootType (
  IN UINTN CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return CpuBootloaderParams->BootType;
}

/**
  Retrieve GR Blob Address

**/
UINT64
T234GetGRBlobBaseAddress (
  IN UINTN CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS         *CpuBootloaderParams;
  UINT64                     MemoryBase;
  UINT64                     MemorySize;
  EFI_FIRMWARE_VOLUME_HEADER *FvHeader;
  UINT64                     FvOffset;
  UINT64                     FvSize;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  MemoryBase = CpuBootloaderParams->CarveoutInfo[CARVEOUT_UEFI].Base;
  MemorySize = CpuBootloaderParams->CarveoutInfo[CARVEOUT_UEFI].Size;
  FvOffset = 0;

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
  Retrieve GR Output Base and Size

**/
BOOLEAN
T234GetGROutputBaseAndSize (
  IN  UINTN CpuBootloaderAddress,
  OUT UINTN *Base,
  OUT UINTN *Size
  )
{
  TEGRA_CPUBL_PARAMS *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  *Base = CpuBootloaderParams->GoldenRegisterAddress;
  *Size = CpuBootloaderParams->GoldenRegisterSize;

  return TRUE;
}

/**
  Retrieve FSI NS Base and Size

**/
BOOLEAN
T234GetFsiNsBaseAndSize (
  IN  UINTN CpuBootloaderAddress,
  OUT UINTN *Base,
  OUT UINTN *Size
  )
{
  TEGRA_CPUBL_PARAMS *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  *Base = CpuBootloaderParams->CarveoutInfo[CARVEOUT_FSI_CPU_NS].Base;
  *Size = CpuBootloaderParams->CarveoutInfo[CARVEOUT_FSI_CPU_NS].Size;

  return TRUE;
}

/**
  Retrieve MMIO Base and Size

**/
TEGRA_MMIO_INFO*
EFIAPI
T234GetMmioBaseAndSize (
  VOID
)
{
  return T234MmioInfo;
}

/**
  Retrieve CVM EEPROM Data

**/
UINT32
EFIAPI
T234GetCvmEepromData (
  IN  UINTN CpuBootloaderAddress,
  OUT UINT8 **Data
)
{
  TEGRA_CPUBL_PARAMS *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  *Data = CpuBootloaderParams->Eeprom.CvmEepromData;

  return CpuBootloaderParams->Eeprom.CvmEepromDataSize;
}

/**
  Retrieve Board Information

**/
BOOLEAN
T234GetBoardInfo(
  IN  UINTN            CpuBootloaderAddress,
  OUT TEGRA_BOARD_INFO *BoardInfo
)
{
  T234_CVM_EEPROM_DATA *EepromData;
  UINT32               DataLen;

  DataLen = T234GetCvmEepromData (CpuBootloaderAddress, (UINT8 **)&EepromData);

  BoardInfo->FuseBaseAddr = TEGRA_FUSE_BASE_ADDRESS;
  BoardInfo->FuseList = T234FloorsweepingFuseList;
  BoardInfo->FuseCount = sizeof(T234FloorsweepingFuseList) / sizeof(T234FloorsweepingFuseList[0]);
  CopyMem ((VOID *) BoardInfo->BoardId, (VOID *) EepromData->PartNumber.Id, TEGRA_BOARD_ID_LEN);
  CopyMem ((VOID *) BoardInfo->ProductId, (VOID *) &EepromData->PartNumber, sizeof (BoardInfo->ProductId));

  return TRUE;
}

/**
  Retrieve Active Boot Chain Information

**/
EFI_STATUS
T234GetActiveBootChain(
  IN  UINTN   CpuBootloaderAddress,
  OUT UINT32  *BootChain
)
{
  *BootChain = MmioBitFieldRead32 (FixedPcdGet64(PcdBootChainRegisterBaseAddressT234),
                                   BOOT_CHAIN_BIT_FIELD_LO,
                                   BOOT_CHAIN_BIT_FIELD_HI);

  if (*BootChain >= BOOT_CHAIN_MAX) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Validate Active Boot Chain

**/
EFI_STATUS
T234ValidateActiveBootChain(
  IN  UINTN   CpuBootloaderAddress
)
{
  EFI_STATUS Status;
  UINT32     BootChain;

  Status = T234GetActiveBootChain (CpuBootloaderAddress, &BootChain);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmioBitFieldWrite32 (FixedPcdGet64(PcdBootChainRegisterBaseAddressT234),
                       BootChain,
                       BootChain,
                       BOOT_CHAIN_GOOD);

  return EFI_SUCCESS;
}
