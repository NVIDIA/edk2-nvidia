/** @file
  Creates HOB during Standalone MM Foundation entry point
  on ARM platforms.

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <PiPei.h>
#include <Guid/MmramMemoryReserve.h>
#include <Guid/MpInformation.h>

#include <Library/NvMmStandaloneMmCoreEntryPoint.h>
#include <Library/ArmMmuLib.h>
#include <Library/ArmSvcLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SerialPortLib.h>

#include <IndustryStandard/ArmStdSmc.h>

extern EFI_HOB_HANDOFF_INFO_TABLE *
HobConstructor (
  IN VOID   *EfiMemoryBegin,
  IN UINTN  EfiMemoryLength,
  IN VOID   *EfiFreeMemoryBottom,
  IN VOID   *EfiFreeMemoryTop
  );

// GUID to identify HOB with whereabouts of communication buffer with Normal
// World
extern EFI_GUID  gEfiStandaloneMmNonSecureBufferGuid;

// GUID to identify HOB where the entry point of the CPU driver will be
// populated to allow this entry point driver to invoke it upon receipt of an
// event
extern EFI_GUID  gEfiMmCpuDriverEpDescriptorGuid;

/**
  Use the boot information passed by privileged firmware to populate a HOB list
  suitable for consumption by the MM Core and drivers.

  @param  [in, out] CpuDriverEntryPoint   Address of MM CPU driver entrypoint
  @param  [in]      PayloadBootInfo       Boot information passed by privileged
                                          firmware

**/
VOID *
CreateHobListFromBootInfo (
  IN  OUT  PI_MM_CPU_DRIVER_ENTRYPOINT     *CpuDriverEntryPoint,
  IN       EFI_SECURE_PARTITION_BOOT_INFO  *PayloadBootInfo
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE      *HobStart;
  EFI_RESOURCE_ATTRIBUTE_TYPE     Attributes;
  UINT32                          Index;
  UINT32                          BufferSize;
  UINT32                          Flags;
  EFI_MMRAM_HOB_DESCRIPTOR_BLOCK  *MmramRangesHob;
  EFI_MMRAM_DESCRIPTOR            *MmramRanges;
  EFI_MMRAM_DESCRIPTOR            *NsCommBufMmramRange;
  MP_INFORMATION_HOB_DATA         *MpInformationHobData;
  EFI_PROCESSOR_INFORMATION       *ProcInfoBuffer;
  EFI_SECURE_PARTITION_CPU_INFO   *CpuInfo;
  MM_CPU_DRIVER_EP_DESCRIPTOR     *CpuDriverEntryPointDesc;
  UINTN                           MmRangeIndex;

  // Create a hoblist with a PHIT and EOH
  HobStart = HobConstructor (
               (VOID *)PayloadBootInfo->SpMemBase,
               (UINTN)PayloadBootInfo->SpMemLimit - PayloadBootInfo->SpMemBase,
               (VOID *)PayloadBootInfo->SpHeapBase,
               (VOID *)(PayloadBootInfo->SpHeapBase + PayloadBootInfo->SpHeapSize)
               );

  // Check that the Hoblist starts at the bottom of the Heap
  ASSERT (HobStart == (VOID *)PayloadBootInfo->SpHeapBase);

  // Build a Boot Firmware Volume HOB
  BuildFvHob (PayloadBootInfo->SpImageBase, PayloadBootInfo->SpImageSize);

  // Build a resource descriptor Hob that describes the available physical
  // memory range
  Attributes = (
                EFI_RESOURCE_ATTRIBUTE_PRESENT |
                EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
                EFI_RESOURCE_ATTRIBUTE_TESTED |
                EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
                EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
                EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
                EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE
                );

  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    Attributes,
    (UINTN)PayloadBootInfo->SpMemBase,
    PayloadBootInfo->SpMemLimit - PayloadBootInfo->SpMemBase
    );

  // Find the size of the GUIDed HOB with MP information
  BufferSize  = sizeof (MP_INFORMATION_HOB_DATA);
  BufferSize += sizeof (EFI_PROCESSOR_INFORMATION) * PayloadBootInfo->NumCpus;

  // Create a Guided MP information HOB to enable the ARM TF CPU driver to
  // perform per-cpu allocations.
  MpInformationHobData = BuildGuidHob (&gMpInformationHobGuid, BufferSize);

  // Populate the MP information HOB with the topology information passed by
  // privileged firmware
  MpInformationHobData->NumberOfProcessors        = PayloadBootInfo->NumCpus;
  MpInformationHobData->NumberOfEnabledProcessors = PayloadBootInfo->NumCpus;
  ProcInfoBuffer                                  = MpInformationHobData->ProcessorInfoBuffer;
  CpuInfo                                         = PayloadBootInfo->CpuInfo;

  for (Index = 0; Index < PayloadBootInfo->NumCpus; Index++) {
    ProcInfoBuffer[Index].ProcessorId      = CpuInfo[Index].Mpidr;
    ProcInfoBuffer[Index].Location.Package = GET_CLUSTER_ID (CpuInfo[Index].Mpidr);
    ProcInfoBuffer[Index].Location.Core    = GET_CORE_ID (CpuInfo[Index].Mpidr);
    ProcInfoBuffer[Index].Location.Thread  = GET_CORE_ID (CpuInfo[Index].Mpidr);

    Flags = PROCESSOR_ENABLED_BIT | PROCESSOR_HEALTH_STATUS_BIT;
    if (CpuInfo[Index].Flags & CPU_INFO_FLAG_PRIMARY_CPU) {
      Flags |= PROCESSOR_AS_BSP_BIT;
    }

    ProcInfoBuffer[Index].StatusFlag = Flags;
  }

  // Create a Guided HOB to tell the ARM TF CPU driver the location and length
  // of the communication buffer shared with the Normal world.
  NsCommBufMmramRange = (EFI_MMRAM_DESCRIPTOR *)BuildGuidHob (
                                                  &gEfiStandaloneMmNonSecureBufferGuid,
                                                  (NS_MAX_REGIONS * sizeof (EFI_MM_DEVICE_REGION))
                                                  );
  for (Index = 0; Index < NS_MAX_REGIONS; Index++) {
    NsCommBufMmramRange[Index].PhysicalStart = PayloadBootInfo->SpNsRegions[Index].DeviceRegionStart;
    NsCommBufMmramRange[Index].CpuStart      = PayloadBootInfo->SpNsRegions[Index].DeviceRegionStart;
    NsCommBufMmramRange[Index].PhysicalSize  = PayloadBootInfo->SpNsRegions[Index].DeviceRegionSize;
    NsCommBufMmramRange[Index].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;
  }

  // Create a Guided HOB to enable the ARM TF CPU driver to share its entry
  // point and populate it with the address of the shared buffer
  CpuDriverEntryPointDesc = (MM_CPU_DRIVER_EP_DESCRIPTOR *)BuildGuidHob (
                                                             &gEfiMmCpuDriverEpDescriptorGuid,
                                                             sizeof (MM_CPU_DRIVER_EP_DESCRIPTOR)
                                                             );

  *CpuDriverEntryPoint                      = NULL;
  CpuDriverEntryPointDesc->MmCpuDriverEpPtr = CpuDriverEntryPoint;

  // Find the size of the GUIDed HOB with SRAM ranges
  BufferSize  = sizeof (EFI_MMRAM_HOB_DESCRIPTOR_BLOCK);
  BufferSize += PayloadBootInfo->NumSpMemRegions * sizeof (EFI_MMRAM_DESCRIPTOR);

  // Create a GUIDed HOB with SRAM ranges
  MmramRangesHob = BuildGuidHob (&gEfiMmPeiMmramMemoryReserveGuid, BufferSize);

  // Fill up the number of MMRAM memory regions
  MmramRangesHob->NumberOfMmReservedRegions = PayloadBootInfo->NumSpMemRegions;
  // Fill up the MMRAM ranges
  MmramRanges = &MmramRangesHob->Descriptor[0];

  MmRangeIndex = 0;
  // Base and size of memory occupied by the Standalone MM image
  MmramRanges[MmRangeIndex].PhysicalStart = PayloadBootInfo->SpImageBase;
  MmramRanges[MmRangeIndex].CpuStart      = PayloadBootInfo->SpImageBase;
  MmramRanges[MmRangeIndex].PhysicalSize  = PayloadBootInfo->SpImageSize;
  MmramRanges[MmRangeIndex].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;
  MmRangeIndex++;

  // Base and size of buffer shared with privileged Secure world software
  MmramRanges[MmRangeIndex].PhysicalStart = PayloadBootInfo->SpSharedBufBase;
  MmramRanges[MmRangeIndex].CpuStart      = PayloadBootInfo->SpSharedBufBase;
  MmramRanges[MmRangeIndex].PhysicalSize  = PayloadBootInfo->SpSharedBufSize;
  MmramRanges[MmRangeIndex].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;
  MmRangeIndex++;

  // Base and size of buffer used for synchronous communication with Normal
  // world software
  for (Index = 0; Index < NS_MAX_REGIONS; Index++) {
    if (PayloadBootInfo->SpNsRegions[Index].DeviceRegionStart) {
      MmramRanges[MmRangeIndex].PhysicalStart = PayloadBootInfo->SpNsRegions[Index].DeviceRegionStart;
      MmramRanges[MmRangeIndex].CpuStart      = PayloadBootInfo->SpNsRegions[Index].DeviceRegionStart;
      MmramRanges[MmRangeIndex].PhysicalSize  = PayloadBootInfo->SpNsRegions[Index].DeviceRegionSize;
      MmramRanges[MmRangeIndex].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;
      MmRangeIndex++;
    }
  }

  // Base and size of memory allocated for stacks for all cpus
  MmramRanges[MmRangeIndex].PhysicalStart = PayloadBootInfo->SpStackBase;
  MmramRanges[MmRangeIndex].CpuStart      = PayloadBootInfo->SpStackBase;
  MmramRanges[MmRangeIndex].PhysicalSize  = PayloadBootInfo->SpPcpuStackSize * PayloadBootInfo->NumCpus;
  MmramRanges[MmRangeIndex].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;
  MmRangeIndex++;

  // Base and size of heap memory shared by all cpus
  MmramRanges[MmRangeIndex].PhysicalStart = (EFI_PHYSICAL_ADDRESS)HobStart;
  MmramRanges[MmRangeIndex].CpuStart      = (EFI_PHYSICAL_ADDRESS)HobStart;
  MmramRanges[MmRangeIndex].PhysicalSize  = HobStart->EfiFreeMemoryBottom - (EFI_PHYSICAL_ADDRESS)HobStart;
  MmramRanges[MmRangeIndex].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;
  MmRangeIndex++;

  // Base and size of heap memory shared by all cpus
  MmramRanges[MmRangeIndex].PhysicalStart = HobStart->EfiFreeMemoryBottom;
  MmramRanges[MmRangeIndex].CpuStart      = HobStart->EfiFreeMemoryBottom;
  MmramRanges[MmRangeIndex].PhysicalSize  = HobStart->EfiFreeMemoryTop - HobStart->EfiFreeMemoryBottom;
  MmramRanges[MmRangeIndex].RegionState   = EFI_CACHEABLE;
  MmRangeIndex++;

  return HobStart;
}
