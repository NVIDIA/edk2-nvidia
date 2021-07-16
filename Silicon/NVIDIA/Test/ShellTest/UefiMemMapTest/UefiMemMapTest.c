/** @file
  UEFI memory map test

  Copyright (c) 2020, NVIDIA Corporation. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UnitTestLib.h>

#define UNIT_TEST_NAME    "UEFI memory map test"
#define UNIT_TEST_VERSION "0.1.0"

/// Structure which wraps context for all tests in the UEFI memory map
/// test suite.
typedef struct {
  /// Pointer to the HOB list.
  VOID                  *HobList;

  /// Pointer to the start of the memory map.
  EFI_MEMORY_DESCRIPTOR *MemoryMap;

  /// Total size of the memory map.
  UINTN                 MemoryMapSize;

  /// Key for the current memory map.
  UINTN                 MapKey;

  /// Size of a single memory map descriptor.
  UINTN                 DescriptorSize;

  /// Version of the memory map descriptors.
  UINT32                DescriptorVersion;
} MEMORY_MAP_TEST_SUITE_CONTEXT;

/// Module-wide test suite context, managed by the test suite setup
/// and teardown functions.
STATIC MEMORY_MAP_TEST_SUITE_CONTEXT mMemoryMapTestSuiteContext;

/**
   Retrieve pointers to the HOB list and the UEFI memory map, and
   store them in the module-wide test suite context.

   If any of the functions fail, then ASSERT().
*/
STATIC
VOID
EFIAPI
TestSuiteSetup (
  VOID
  )
{
  EFI_STATUS                            Status;
  MEMORY_MAP_TEST_SUITE_CONTEXT * CONST Context = &mMemoryMapTestSuiteContext;

  SetMem (Context, sizeof (*Context), 0);

  Context->HobList = GetHobList ();
  ASSERT (Context->HobList != NULL);

  Status = gBS->GetMemoryMap (&Context->MemoryMapSize,
                              Context->MemoryMap,
                              &Context->MapKey,
                              &Context->DescriptorSize,
                              &Context->DescriptorVersion);
  ASSERT (Status == EFI_BUFFER_TOO_SMALL);

  Status = gBS->AllocatePool (EfiBootServicesData,
                              Context->MemoryMapSize,
                              (VOID**) &Context->MemoryMap);
  ASSERT (!EFI_ERROR (Status));

  Status = gBS->GetMemoryMap (&Context->MemoryMapSize,
                              Context->MemoryMap,
                              &Context->MapKey,
                              &Context->DescriptorSize,
                              &Context->DescriptorVersion);
  ASSERT (!EFI_ERROR (Status));
}

/**
   Release all resources acquired during test suite setup.

   If any of the functions fail, then ASSERT().
*/
STATIC
VOID
EFIAPI
TestSuiteTeardown (
  VOID
  )
{
  EFI_STATUS                            Status;
  MEMORY_MAP_TEST_SUITE_CONTEXT * CONST Context = &mMemoryMapTestSuiteContext;

  Status = gBS->FreePool ((VOID*) Context->MemoryMap);
  ASSERT (!EFI_ERROR (Status));
}

/**
   Verifies that all allocations described by
   EFI_HOB_MEMORY_ALLOCATION are present in the UEFI memory map.

   @param [in] Context Test context.

   @retval UNIT_TEST_PASSED            All allocations are present.
   @retval UNIT_TEST_ERROR_TEST_FAILED At least one allocation is
                                       missing.
*/
STATIC
UNIT_TEST_STATUS
EFIAPI
HobMemoryAllocationsPresentTest (
  IN MEMORY_MAP_TEST_SUITE_CONTEXT * CONST Context
  )
{
  UNIT_TEST_STATUS            Status;
  EFI_PEI_HOB_POINTERS        Hob;
  EFI_MEMORY_DESCRIPTOR       *MemDescCur;
  EFI_MEMORY_DESCRIPTOR       *MemDescEnd;
  EFI_MEMORY_TYPE             HobMemoryType;
  EFI_PHYSICAL_ADDRESS        HobStartAddress;
  EFI_PHYSICAL_ADDRESS        HobEndAddress;
  EFI_MEMORY_TYPE             MapMemoryType;
  EFI_PHYSICAL_ADDRESS        MapStartAddress;
  EFI_PHYSICAL_ADDRESS        MapEndAddress;

  Status = UNIT_TEST_PASSED;
  for (Hob.Raw = Context->HobList; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (!(Hob.Header->HobType == EFI_HOB_TYPE_MEMORY_ALLOCATION)) {
      continue;
    }

    HobMemoryType   = Hob.MemoryAllocation->AllocDescriptor.MemoryType;
    HobStartAddress = Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress;
    HobEndAddress   = (Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress
                       + Hob.MemoryAllocation->AllocDescriptor.MemoryLength);

    MemDescCur = Context->MemoryMap;
    MemDescEnd = (EFI_MEMORY_DESCRIPTOR*) ((UINT8*) Context->MemoryMap + Context->MemoryMapSize);
    for (; MemDescCur < MemDescEnd; MemDescCur = NEXT_MEMORY_DESCRIPTOR (MemDescCur, Context->DescriptorSize)) {
      MapMemoryType   = MemDescCur->Type;
      MapStartAddress = MemDescCur->PhysicalStart;
      MapEndAddress   = MemDescCur->PhysicalStart + EFI_PAGES_TO_SIZE (MemDescCur->NumberOfPages);

      if (MapMemoryType == HobMemoryType
          && MapStartAddress <= HobStartAddress
          && HobEndAddress <= MapEndAddress) {
        break;                  // Found it.
      }
    }

    if (!(MemDescCur < MemDescEnd)) {
      UT_LOG_ERROR ("HOB memory allocation not located in memory map:" "\r\n");
      UT_LOG_ERROR ("  GUID          = %g"                             "\r\n", &Hob.MemoryAllocation->AllocDescriptor.Name);
      UT_LOG_ERROR ("  Type          = %d"                             "\r\n", HobMemoryType);
      UT_LOG_ERROR ("  Start address = %016lx"                         "\r\n", HobStartAddress);
      UT_LOG_ERROR ("  End address   = %016lx"                         "\r\n", HobEndAddress);
      Status = UNIT_TEST_ERROR_TEST_FAILED;
    }
  }

  return Status;
}

/**
   Verifies that all memory regions described by the UEFI memory map
   are located in system memory (as described by
   EFI_HOB_RESOURCE_DESCRIPTOR).

   @param [in] Context Test context.

   @retval UNIT_TEST_PASSED            All memory regions are located
                                       in system memory.
   @retval UNIT_TEST_ERROR_TEST_FAILED At least one memory region is
                                       not located in system memory.
*/
STATIC
UNIT_TEST_STATUS
EFIAPI
MemoryMapRegionsInSystemMemoryTest (
  IN MEMORY_MAP_TEST_SUITE_CONTEXT * CONST Context
  )
{
  UNIT_TEST_STATUS            Status;
  EFI_PEI_HOB_POINTERS        Hob;
  EFI_MEMORY_DESCRIPTOR       *MemDescCur;
  EFI_MEMORY_DESCRIPTOR       *MemDescEnd;
  EFI_PHYSICAL_ADDRESS        MapStartAddress;
  EFI_PHYSICAL_ADDRESS        MapEndAddress;
  EFI_PHYSICAL_ADDRESS        HobStartAddress;
  EFI_PHYSICAL_ADDRESS        HobEndAddress;
  BOOLEAN                     HasChanged;

  Status     = UNIT_TEST_PASSED;
  MemDescCur = Context->MemoryMap;
  MemDescEnd = (EFI_MEMORY_DESCRIPTOR*) ((UINT8*) Context->MemoryMap + Context->MemoryMapSize);
  for (; MemDescCur < MemDescEnd; MemDescCur = NEXT_MEMORY_DESCRIPTOR (MemDescCur, Context->DescriptorSize)) {
    MapStartAddress = MemDescCur->PhysicalStart;
    MapEndAddress   = MemDescCur->PhysicalStart + EFI_PAGES_TO_SIZE (MemDescCur->NumberOfPages);

    do {
      HasChanged = FALSE;
      for (Hob.Raw = Context->HobList; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
        if (!(Hob.Header->HobType == EFI_HOB_TYPE_RESOURCE_DESCRIPTOR
              && Hob.ResourceDescriptor->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY)) {
          continue;
        }

        HobStartAddress = Hob.ResourceDescriptor->PhysicalStart;
        HobEndAddress   = Hob.ResourceDescriptor->PhysicalStart + Hob.ResourceDescriptor->ResourceLength;

        if (HobStartAddress <= MapStartAddress && MapStartAddress < HobEndAddress) {
          MapStartAddress = HobEndAddress;
          HasChanged = TRUE;
        }
        if (HobStartAddress < MapEndAddress && MapEndAddress <= HobEndAddress) {
          MapEndAddress = HobStartAddress;
          HasChanged = TRUE;
        }
      }
    } while (HasChanged && MapStartAddress < MapEndAddress);

    if (MapStartAddress < MapEndAddress) {
      UT_LOG_ERROR ("Memory map range not located in system memory:" "\r\n");
      UT_LOG_ERROR ("  Start address = %016lx"                       "\r\n", MapStartAddress);
      UT_LOG_ERROR ("  End address   = %016lx"                       "\r\n", MapEndAddress);
      Status = UNIT_TEST_ERROR_TEST_FAILED;
    }
  }

  return Status;
}

/**
   Verifies that all system memory (as described by
   EFI_HOB_RESOURCE_DESCRIPTOR) is included in the UEFI memory map.

   @param [in] Context Test context.

   @retval UNIT_TEST_PASSED            All memory regions are located
                                       in system memory.
   @retval UNIT_TEST_ERROR_TEST_FAILED At least one memory region is
                                       not located in system memory.
*/
STATIC
UNIT_TEST_STATUS
EFIAPI
SystemMemoryInMemoryMapTest (
  IN MEMORY_MAP_TEST_SUITE_CONTEXT * CONST Context
  )
{
  UNIT_TEST_STATUS            Status;
  EFI_PEI_HOB_POINTERS        Hob;
  EFI_MEMORY_DESCRIPTOR       *MemDescCur;
  EFI_MEMORY_DESCRIPTOR       *MemDescEnd;
  EFI_PHYSICAL_ADDRESS        MapStartAddress;
  EFI_PHYSICAL_ADDRESS        MapEndAddress;
  EFI_PHYSICAL_ADDRESS        HobStartAddress;
  EFI_PHYSICAL_ADDRESS        HobEndAddress;
  BOOLEAN                     HasChanged;

  Status = UNIT_TEST_PASSED;
  for (Hob.Raw = Context->HobList; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (!(Hob.Header->HobType == EFI_HOB_TYPE_RESOURCE_DESCRIPTOR
          && Hob.ResourceDescriptor->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY)) {
      continue;
    }

    HobStartAddress = Hob.ResourceDescriptor->PhysicalStart;
    HobEndAddress   = Hob.ResourceDescriptor->PhysicalStart + Hob.ResourceDescriptor->ResourceLength;

    do {
      HasChanged = FALSE;

      MemDescCur = Context->MemoryMap;
      MemDescEnd = (EFI_MEMORY_DESCRIPTOR*) ((UINT8*) Context->MemoryMap + Context->MemoryMapSize);
      for (; MemDescCur < MemDescEnd; MemDescCur = NEXT_MEMORY_DESCRIPTOR (MemDescCur, Context->DescriptorSize)) {
        MapStartAddress = MemDescCur->PhysicalStart;
        MapEndAddress   = MemDescCur->PhysicalStart + EFI_PAGES_TO_SIZE (MemDescCur->NumberOfPages);

        if (MapStartAddress <= HobStartAddress && HobStartAddress < MapEndAddress) {
          HobStartAddress = MapEndAddress;
          HasChanged = TRUE;
        }
        if (MapStartAddress < HobEndAddress && HobEndAddress <= MapEndAddress) {
          HobEndAddress = MapStartAddress;
          HasChanged = TRUE;
        }
      }
    } while (HasChanged && HobStartAddress < HobEndAddress);

    if (HobStartAddress < HobEndAddress) {
      UT_LOG_ERROR ("System memory range not located in memory map:" "\r\n");
      UT_LOG_ERROR ("  Start address = %016lx"                       "\r\n", HobStartAddress);
      UT_LOG_ERROR ("  End address   = %016lx"                       "\r\n", HobEndAddress);
      Status = UNIT_TEST_ERROR_TEST_FAILED;
    }
  }

  return Status;
}

/**
   Verifies that none of the regions described in the UEFI memory
   map overlap.

   @param [in] Context Test context.

   @retval UNIT_TEST_PASSED            All memory regions are disjoint.
   @retval UNIT_TEST_ERROR_TEST_FAILED At least one pair of memory
                                       regions overlap.
*/
STATIC
EFI_STATUS
EFIAPI
MemoryMapOverlapTest (
  IN MEMORY_MAP_TEST_SUITE_CONTEXT * CONST Context
  )
{
  UNIT_TEST_STATUS            Status;
  EFI_MEMORY_DESCRIPTOR       *MemDescCurA;
  EFI_PHYSICAL_ADDRESS        StartAddressA;
  EFI_PHYSICAL_ADDRESS        EndAddressA;
  EFI_MEMORY_DESCRIPTOR       *MemDescCurB;
  EFI_PHYSICAL_ADDRESS        StartAddressB;
  EFI_PHYSICAL_ADDRESS        EndAddressB;
  EFI_MEMORY_DESCRIPTOR       *MemDescEnd;

  Status      = UNIT_TEST_PASSED;
  MemDescCurA = Context->MemoryMap;
  MemDescEnd  = (EFI_MEMORY_DESCRIPTOR*) ((UINT8*) Context->MemoryMap + Context->MemoryMapSize);
  for (; MemDescCurA < MemDescEnd; MemDescCurA = NEXT_MEMORY_DESCRIPTOR (MemDescCurA, Context->DescriptorSize)) {
    StartAddressA = MemDescCurA->PhysicalStart;
    EndAddressA   = MemDescCurA->PhysicalStart + EFI_PAGES_TO_SIZE (MemDescCurA->NumberOfPages);

    MemDescCurB = NEXT_MEMORY_DESCRIPTOR (MemDescCurA, Context->DescriptorSize);
    for (; MemDescCurB < MemDescEnd; MemDescCurB = NEXT_MEMORY_DESCRIPTOR (MemDescCurB, Context->DescriptorSize)) {
      StartAddressB = MemDescCurB->PhysicalStart;
      EndAddressB   = MemDescCurB->PhysicalStart + EFI_PAGES_TO_SIZE (MemDescCurB->NumberOfPages);

      if (!(EndAddressA <= StartAddressB || EndAddressB <= StartAddressA)) {
        UT_LOG_ERROR ("Distinct regions in UEFI memory map are overlapping:" "\r\n");
        UT_LOG_ERROR ("  Region A: %016lx-%016lx"                            "\r\n", StartAddressA, EndAddressA - 1);
        UT_LOG_ERROR ("  Region B: %016lx-%016lx"                            "\r\n", StartAddressB, EndAddressB - 1);
        Status = UNIT_TEST_ERROR_TEST_FAILED;
      }
    }
  }

  return Status;
}

/**
   Verifies that all memory map entries are aligned on 64 KiB
   boundaries.

   @param [in] Context Test context.

   @retval UNIT_TEST_PASSED            All memory regions are aligned
                                       correctly.
   @retval UNIT_TEST_ERROR_TEST_FAILED At least one memory region is
                                       misaligned.
*/
STATIC
EFI_STATUS
EFIAPI
MemoryMapAlignmentTest (
  IN MEMORY_MAP_TEST_SUITE_CONTEXT * CONST Context
  )
{
  UNIT_TEST_STATUS            Status;
  EFI_MEMORY_DESCRIPTOR       *MemDescCur;
  EFI_MEMORY_DESCRIPTOR       *MemDescEnd;
  EFI_PHYSICAL_ADDRESS        PhysicalStart;
  UINT64                      NumberOfBytes;
  UINT64                      Attribute;

  Status     = UNIT_TEST_PASSED;
  MemDescCur = Context->MemoryMap;
  MemDescEnd = (EFI_MEMORY_DESCRIPTOR*) ((UINT8*) Context->MemoryMap + Context->MemoryMapSize);
  while (MemDescCur < MemDescEnd) {
    PhysicalStart = MemDescCur->PhysicalStart;
    NumberOfBytes = EFI_PAGES_TO_SIZE (MemDescCur->NumberOfPages);
    Attribute     = MemDescCur->Attribute & EFI_MEMORY_RUNTIME;

    while ((MemDescCur = NEXT_MEMORY_DESCRIPTOR (MemDescCur, Context->DescriptorSize)) < MemDescEnd
           && PhysicalStart + NumberOfBytes == MemDescCur->PhysicalStart
           && Attribute == (MemDescCur->Attribute & EFI_MEMORY_RUNTIME)) {
      // This is an adjacent memory region with the same
      // EFI_MEMORY_RUNTIME attribute, join them into one.
      NumberOfBytes += EFI_PAGES_TO_SIZE (MemDescCur->NumberOfPages);
    }

    // Either we have reached end of the memory map, or this is not an
    // adjacent memory region, or the EFI_MEMORY_RUNTIME attribute has
    // changed. Whatever the case may be, we need to perform the
    // alignment check.
    if ((PhysicalStart & (BASE_64KB - 1)) != 0) {
      UT_LOG_ERROR ("Physical address misaligned : %016lx" "\r\n", PhysicalStart);
      Status = UNIT_TEST_ERROR_TEST_FAILED;
    }
    if ((NumberOfBytes & (SIZE_64KB - 1)) != 0) {
      UT_LOG_ERROR ("Region size misaligned      : %016lx" "\r\n", NumberOfBytes);
      Status = UNIT_TEST_ERROR_TEST_FAILED;
    }
  }

  return Status;
}

/**
   Initialize the test suite.

   @param [in] Framework Unit test framework for the suite.

   @retval EFI_SUCCESS Suite initialized successfully.
*/
STATIC
EFI_STATUS
EFIAPI
InitTestSuite (
  IN UNIT_TEST_FRAMEWORK_HANDLE Framework
  )
{
  EFI_STATUS                Status;
  UNIT_TEST_SUITE_HANDLE    TestSuite;

  Status = CreateUnitTestSuite (&TestSuite,
                                Framework,
                                "UEFI Memory Map Tests",
                                "NVIDIA-Internal.UefiMemMap",
                                TestSuiteSetup,
                                TestSuiteTeardown);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Failed to create the test suite."
      " Status = %r\n", __FUNCTION__, Status
    ));
    return Status;
  }

  AddTestCase (TestSuite,
               "Verify all HOB memory allocations present",
               "HobMemoryAllocationsPresentTest",
               (UNIT_TEST_FUNCTION) HobMemoryAllocationsPresentTest,
               NULL, NULL,
               (UNIT_TEST_CONTEXT) &mMemoryMapTestSuiteContext);

  AddTestCase (TestSuite,
               "Verify all memory map regions located in system memory",
               "MemoryMapRegionsInSystemMemoryTest",
               (UNIT_TEST_FUNCTION) MemoryMapRegionsInSystemMemoryTest,
               NULL, NULL,
               (UNIT_TEST_CONTEXT) &mMemoryMapTestSuiteContext);

  AddTestCase (TestSuite,
               "Verify all system memory present in memory map",
               "SystemMemoryInMemoryMapTest",
               (UNIT_TEST_FUNCTION) SystemMemoryInMemoryMapTest,
               NULL, NULL,
               (UNIT_TEST_CONTEXT) &mMemoryMapTestSuiteContext);

  AddTestCase (TestSuite,
               "Verify disjointness of memory map regions",
               "MemoryMapOverlapTest",
               (UNIT_TEST_FUNCTION) MemoryMapOverlapTest,
               NULL, NULL,
               (UNIT_TEST_CONTEXT) &mMemoryMapTestSuiteContext);

  AddTestCase (TestSuite,
               "Verify alignment of memory map regions",
               "MemoryMapAlignmentTest",
               (UNIT_TEST_FUNCTION) MemoryMapAlignmentTest,
               NULL, NULL,
               (UNIT_TEST_CONTEXT) &mMemoryMapTestSuiteContext);

  return Status;
}

/**
   Run the UEFI memory map test in UEFI DXE stage / UEFI shell.

   @param [in] ImageHandle UEFI image handle.
   @param [in] SystemTable UEFI System Table.

   @retval EFI_SUCCESS All tests ran successfully.
*/
EFI_STATUS
EFIAPI
UefiMemMapTestDxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;

  DEBUG ((DEBUG_INFO, "%a v%a" "\r\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  Status = InitUnitTestFramework (&Framework,
                                  UNIT_TEST_NAME,
                                  gEfiCallerBaseName,
                                  UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: InitUnitTestFramework failed."
      " Status = %r\n", __FUNCTION__, Status
    ));
    return Status;
  }

  Status = InitTestSuite (Framework);
  if (!EFI_ERROR (Status)) {
    Status = RunAllTestSuites (Framework);
  }

  FreeUnitTestFramework (Framework);
  return Status;
}
