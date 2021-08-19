/** @file
  Configuration Manager Library

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.

*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2020-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#include <Uefi/UefiBaseType.h>
#include <ConfigurationManagerObject.h>
#include <Library/FloorSweepingLib.h>
#include <Library/PcdLib.h>
#include <Library/NvgLib.h>
#include <Library/ArmGicLib.h>
#include <Library/ConfigurationManagerLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <libfdt.h>

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_CPUS               (PLATFORM_MAX_CLUSTERS * \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)

#define ITSID_FROM_PHYS_ADDR(phys) (((phys) >> 40) & 0x3)

// GiC variable
CM_ARM_GICC_INFO                  *GicCInfo;

// GicC Token for Processor Hierarchy structure

CM_OBJECT_TOKEN
EFIAPI
GetGicCToken (
  UINTN Index
)
{
  return REFERENCE_TOKEN (GicCInfo[Index]);
}

EFI_STATUS
EFIAPI
GetPmuBaseInterrupt (
  OUT HARDWARE_INTERRUPT_SOURCE* PmuBaseInterrupt
)
{
  EFI_STATUS                        Status;
  UINT32                            PmuHandle;
  UINT32                            NumPmuHandles;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA InterruptData;
  UINT32                            Size;

  NumPmuHandles = 1;
  Status = GetMatchingEnabledDeviceTreeNodes ("arm,armv8-pmuv3", &PmuHandle, &NumPmuHandles);
  if (EFI_ERROR (Status)) {
    NumPmuHandles = 0;
    *PmuBaseInterrupt = 0;
    return Status;
  }

  //Only one interrupt is expected
  Size = 1;
  Status = GetDeviceTreeInterrupts (PmuHandle, &InterruptData, &Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *PmuBaseInterrupt = InterruptData.Interrupt + DEVICETREE_TO_ACPI_INTERRUPT_OFFSET;
  return Status;
}


/** Initialize the GIC ITS entry in the platform configuration repository and patch MADT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
EFI_STATUS
EFIAPI
UpdateGicItsInfo (EDKII_PLATFORM_REPOSITORY_INFO **PlatformRepositoryInfo, CHAR8 *ItsCompatString)
{
  EFI_STATUS                        Status;
  UINT32                            NumberOfItsCtlrs;
  UINT32                            NumberOfItsEntries;
  UINT32                            *ItsHandles;
  EDKII_PLATFORM_REPOSITORY_INFO    *Repo;
  CM_ARM_GIC_ITS_INFO               *GicItsInfo;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            Index;
  UINT32                            RegisterSize;

  NumberOfItsCtlrs = 0;
  NumberOfItsEntries = 0;
  RegisterData = NULL;
  GicItsInfo = NULL;

  Status = GetMatchingEnabledDeviceTreeNodes (ItsCompatString, NULL, &NumberOfItsCtlrs);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto Exit;
  }

  ItsHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfItsCtlrs);
  if (ItsHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (ItsCompatString, ItsHandles, &NumberOfItsCtlrs);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  GicItsInfo = (CM_ARM_GIC_ITS_INFO *)AllocateZeroPool (sizeof (CM_ARM_GIC_ITS_INFO) * NumberOfItsCtlrs);
  if (GicItsInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  RegisterSize = 0;
  for (Index = 0; Index < NumberOfItsCtlrs; Index++) {
    // Obtain Register Info using the ITS Handle
    Status = GetDeviceTreeRegisters (ItsHandles[Index], RegisterData, &RegisterSize);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      if (RegisterData != NULL) {
        FreePool (RegisterData);
        RegisterData = NULL;
      }
      RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *) AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
      if (RegisterData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }
      Status = GetDeviceTreeRegisters (ItsHandles[Index], RegisterData, &RegisterSize);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    } else if (EFI_ERROR (Status)) {
      goto Exit;
    }

    GicItsInfo[Index].PhysicalBaseAddress = RegisterData[0].BaseAddress;
    GicItsInfo[Index].GicItsId = ITSID_FROM_PHYS_ADDR (RegisterData[0].BaseAddress);

    NumberOfItsEntries++;
  }

  Repo = *PlatformRepositoryInfo;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicItsInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CM_ARM_GIC_ITS_INFO) * NumberOfItsEntries;
  Repo->CmObjectCount = NumberOfItsEntries;
  Repo->CmObjectPtr = GicItsInfo;
  Repo++;

  *PlatformRepositoryInfo = Repo;

  Exit:
  if (EFI_ERROR (Status)) {
    if (GicItsInfo != NULL) {
      FreePool (GicItsInfo);
    }
  }
  if (ItsHandles != NULL) {
    FreePool (ItsHandles);
  }
  if (RegisterData != NULL) {
    FreePool (RegisterData);
  }
  return Status;
}


/** Initialize the GIC entries in the platform configuration repository and patch MADT.
 *  This function updates GIC structure for all supporting Tegra platforms using the
 *  Device Tree information.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
EFI_STATUS
EFIAPI
UpdateGicInfo (EDKII_PLATFORM_REPOSITORY_INFO **PlatformRepositoryInfo)
{
  EFI_STATUS                        Status;
  UINT32                            NumberOfGicCtlrs;
  UINT32                            NumberOfGicEntries;
  UINT32                            *GicHandles;
  EDKII_PLATFORM_REPOSITORY_INFO    *Repo;
  TEGRA_GIC_INFO                    *GicInfo;
  CM_ARM_GICD_INFO                  *GicDInfo;
  CM_ARM_GIC_REDIST_INFO            *GicRedistInfo;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA InterruptData;
  UINT32                            Index;
  UINT32                            CpuIndex;
  UINT32                            RegisterSize;
  UINT32                            RedistStride;
  CONST UINT64*                     Prop;
  VOID                              *DeviceTree;
  INT32                             NodeOffset;
  INT32                             PropertySize;
  UINT64                            MpIdr;
  UINT64                            PmuBaseInterrupt;
  UINT32                            Size;
  UINTN                             VGicMaintenanceInterrupt;
  UINT32                            NumCpus;

  NumberOfGicCtlrs = 0;
  NumberOfGicEntries = 0;
  RegisterData = NULL;
  GicInfo = NULL;
  GicDInfo = NULL;
  GicCInfo = NULL;
  GicRedistInfo = NULL;
  RedistStride = 0;
  PmuBaseInterrupt = 0;

  NumCpus = GetNumberOfEnabledCpuCores ();

  GicInfo = (TEGRA_GIC_INFO *) AllocatePool ( sizeof (TEGRA_GIC_INFO));

  if (!GetGicInfo (GicInfo)) {
    Status = EFI_D_ERROR;
    goto Exit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (GicInfo->GicCompatString, NULL, &NumberOfGicCtlrs);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto Exit;
  }

  GicHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfGicCtlrs);
  if (GicHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (GicInfo->GicCompatString, GicHandles, &NumberOfGicCtlrs);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  GicCInfo = (CM_ARM_GICC_INFO *)AllocateZeroPool (sizeof (CM_ARM_GICC_INFO) * NumberOfGicCtlrs * NumCpus);
  if (GicCInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  GicDInfo = (CM_ARM_GICD_INFO *)AllocateZeroPool (sizeof (CM_ARM_GICD_INFO) * NumberOfGicCtlrs);
  if (GicDInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  //For Gic v3/v4 allocate space for Redistributor
  if (GicInfo->Version >= 3) {
    GicRedistInfo = (CM_ARM_GIC_REDIST_INFO *)AllocateZeroPool (sizeof (CM_ARM_GIC_REDIST_INFO) * NumberOfGicCtlrs);
    if (GicRedistInfo == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }
  }

  //PMU
  Status = GetPmuBaseInterrupt (&PmuBaseInterrupt);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  RegisterSize = 0;
  for (Index = 0; Index < NumberOfGicCtlrs; Index++) {
    // Obtain Register Info using the Gic Handle
    Status = GetDeviceTreeRegisters (GicHandles[Index], RegisterData, &RegisterSize);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      if (RegisterData != NULL) {
        FreePool (RegisterData);
        RegisterData = NULL;
      }
      RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *) AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
      if (RegisterData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }
      Status = GetDeviceTreeRegisters (GicHandles[Index], RegisterData, &RegisterSize);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    } else if (EFI_ERROR (Status)) {
      goto Exit;
    }

    // GICD structure entries
    GicDInfo[Index].PhysicalBaseAddress = RegisterData[0].BaseAddress;
    GicDInfo[Index].SystemVectorBase = 0;
    GicDInfo[Index].GicVersion = GicInfo->Version;

    // GICR structure entries
    if (GicInfo->Version >= 3) {
      // Get redistributor stride
      Status = GetDeviceTreeNode (GicHandles[Index], &DeviceTree, &NodeOffset);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
      Prop = fdt_getprop (DeviceTree,
                          NodeOffset,
                          "redistributor-stride",
                          &PropertySize);
      if (Prop != NULL) {
        RedistStride = SwapBytes64 (Prop[0]);
      }

      GicRedistInfo[Index].DiscoveryRangeBaseAddress = RegisterData[1].BaseAddress;
      GicRedistInfo[Index].DiscoveryRangeLength = RedistStride * PLATFORM_MAX_CPUS;
    }

    //VGIC Interrupt info for GICC per GIC Ctlr
    //Only one interrupt is expected
    Size = 1;
    Status = GetDeviceTreeInterrupts (GicHandles[Index], &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
    VGicMaintenanceInterrupt = InterruptData.Interrupt + DEVICETREE_TO_ACPI_INTERRUPT_OFFSET;

    //GICC structure entries
    for (CpuIndex = 0; CpuIndex < NumCpus; CpuIndex++) {

      // Get Mpidr
      MpIdr = GetMpidrFromLinearCoreID (CpuIndex);

      GicCInfo[CpuIndex + Index * NumCpus].CPUInterfaceNumber = CpuIndex;
      GicCInfo[CpuIndex + Index * NumCpus].AcpiProcessorUid = CpuIndex;
      GicCInfo[CpuIndex + Index * NumCpus].Flags = EFI_ACPI_6_3_GIC_ENABLED;
      GicCInfo[CpuIndex + Index * NumCpus].ParkingProtocolVersion = 0;
      GicCInfo[CpuIndex + Index * NumCpus].PerformanceInterruptGsiv = PmuBaseInterrupt + CpuIndex;
      GicCInfo[CpuIndex + Index * NumCpus].ParkedAddress = 0;

      if (GicInfo->Version < 3) {
        GicCInfo[CpuIndex + Index * NumCpus].PhysicalBaseAddress = RegisterData[1].BaseAddress;
      }

      if (GicInfo->Version < 3) {
        // GICV and GICH for v2
        GicCInfo[CpuIndex + Index * NumCpus].GICV = RegisterData[2].BaseAddress;
        GicCInfo[CpuIndex + Index * NumCpus].GICH = RegisterData[3].BaseAddress;
      }

      // VGIC info
      GicCInfo[CpuIndex + Index * NumCpus].VGICMaintenanceInterrupt = VGicMaintenanceInterrupt;

      if (GicInfo->Version >= 3) {
        GicCInfo[CpuIndex + Index * NumCpus].GICRBaseAddress = RegisterData[1].BaseAddress;
      }
      GicCInfo[CpuIndex + Index * NumCpus].MPIDR = MpIdr & 0xFFFFFF;
      GicCInfo[CpuIndex + Index * NumCpus].ProcessorPowerEfficiencyClass = 0;

      // TODO: check for compat string "arm,statistical-profiling-extension-v1"
      GicCInfo[CpuIndex + Index * NumCpus].SpeOverflowInterrupt = 0;

      GicCInfo[CpuIndex + Index * NumCpus].ProximityDomain = 0;
      GicCInfo[CpuIndex + Index * NumCpus].ClockDomain = 0;
      GicCInfo[CpuIndex + Index * NumCpus].AffinityFlags = EFI_ACPI_6_3_GICC_ENABLED;
    }

    NumberOfGicEntries++;
  }

  Repo = *PlatformRepositoryInfo;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CM_ARM_GICD_INFO) * NumberOfGicEntries;
  Repo->CmObjectCount = NumberOfGicEntries;
  Repo->CmObjectPtr = GicDInfo;
  Repo++;

  if (GicInfo->Version >= 3) {
    Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicRedistributorInfo);
    Repo->CmObjectToken = CM_NULL_TOKEN;
    Repo->CmObjectSize = sizeof (CM_ARM_GIC_REDIST_INFO) * NumberOfGicEntries;
    Repo->CmObjectCount = NumberOfGicEntries;
    Repo->CmObjectPtr = GicRedistInfo;
    Repo++;
  }

  //optional ITS
  if ((GicInfo->Version >= 3) && (GicInfo->ItsCompatString)) {
    UpdateGicItsInfo (&Repo,GicInfo->ItsCompatString);
  }

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicCInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CM_ARM_GICC_INFO) * NumberOfGicEntries * NumCpus;
  Repo->CmObjectCount = NumberOfGicEntries * NumCpus;
  Repo->CmObjectPtr = GicCInfo;

  Repo++;

  *PlatformRepositoryInfo = Repo;

  Exit:
  if (EFI_ERROR (Status)) {
    if (GicDInfo != NULL) {
      FreePool (GicDInfo);
    }
    if (GicRedistInfo != NULL) {
      FreePool (GicRedistInfo);
    }
    if (GicCInfo != NULL) {
      FreePool (GicCInfo);
    }
  }
  if (GicHandles != NULL) {
    FreePool (GicHandles);
  }
  if (RegisterData != NULL) {
    FreePool (RegisterData);
  }
  if (GicInfo != NULL) {
    FreePool (GicInfo);
  }

  return Status;
}
