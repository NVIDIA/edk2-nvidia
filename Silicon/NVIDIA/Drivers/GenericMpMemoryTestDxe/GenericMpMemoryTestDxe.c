/** @file

  Copyright (c) 2006 - 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "GenericMpMemoryTestDxe.h"

/**
  Construct the system base memory range through GCD service.

  @param[in] Private  Point to generic memory test driver's private data.

  @retval EFI_SUCCESS          Successful construct the base memory range through GCD service.
  @retval EFI_OUT_OF_RESOURCE  Could not allocate needed resource from base memory.
  @retval Others               Failed to construct base memory range through GCD service.

**/
EFI_STATUS
ConstructBaseMemoryRange (
  IN  GENERIC_MEMORY_TEST_PRIVATE  *Private
  )
{
  UINTN                            NumberOfDescriptors;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *MemorySpaceMap;
  UINTN                            Index;

  gDS->GetMemorySpaceMap (&NumberOfDescriptors, &MemorySpaceMap);

  for (Index = 0; Index < NumberOfDescriptors; Index++) {
    if ((MemorySpaceMap[Index].GcdMemoryType == EfiGcdMemoryTypeSystemMemory) ||
        (MemorySpaceMap[Index].GcdMemoryType == EfiGcdMemoryTypeMoreReliable))
    {
      Private->BaseMemorySize += MemorySpaceMap[Index].Length;
    }
  }

  return EFI_SUCCESS;
}

/**
  Destroy the link list base on the correspond link list type.

  @param[in] Private  Point to generic memory test driver's private data.

**/
VOID
DestroyLinkList (
  IN  GENERIC_MEMORY_TEST_PRIVATE  *Private
  )
{
  NONTESTED_MEMORY_RANGE  *NontestedRange;
  MEMORY_TEST_RANGE       *MemoryTestRange;

  while (!IsListEmpty (&Private->NonTestedMemList)) {
    NontestedRange = NONTESTED_MEMORY_RANGE_FROM_LINK (GetFirstNode (&Private->NonTestedMemList));
    RemoveEntryList (&NontestedRange->Link);
    gBS->FreePool (NontestedRange);
  }

  while (!IsListEmpty (&Private->MemoryTestList)) {
    MemoryTestRange = MEMORY_TEST_RANGE_FROM_LINK (GetFirstNode (&Private->MemoryTestList));
    if (MemoryTestRange->Thread) {
      Private->ThreadingProtocol->AbortThread (MemoryTestRange->Thread);
      Private->ThreadingProtocol->CleanupThread (MemoryTestRange->Thread);
    }

    RemoveEntryList (&MemoryTestRange->Link);
    gBS->FreePool (MemoryTestRange);
  }
}

/**
  Convert the memory range to tested.

  @param BaseAddress  Base address of the memory range.
  @param Length       Length of the memory range.
  @param Capabilities Capabilities of the memory range.

  @retval EFI_SUCCESS The memory range is converted to tested.
  @retval others      Error happens.
**/
EFI_STATUS
ConvertToTestedMemory (
  IN UINT64  BaseAddress,
  IN UINT64  Length,
  IN UINT64  Capabilities
  )
{
  EFI_STATUS  Status;

  Status = gDS->RemoveMemorySpace (
                  BaseAddress,
                  Length
                  );
  if (!EFI_ERROR (Status)) {
    Status = gDS->AddMemorySpace (
                    ((Capabilities & EFI_MEMORY_MORE_RELIABLE) == EFI_MEMORY_MORE_RELIABLE) ?
                    EfiGcdMemoryTypeMoreReliable : EfiGcdMemoryTypeSystemMemory,
                    BaseAddress,
                    Length,
                    Capabilities &~
                    (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED | EFI_MEMORY_RUNTIME)
                    );
  }

  return Status;
}

/**
  Add the extened memory to whole system memory map.

  @param[in] Private  Point to generic memory test driver's private data.

  @retval EFI_SUCCESS Successful add all the extended memory to system memory map.
  @retval Others      Failed to add the tested extended memory.

**/
EFI_STATUS
UpdateMemoryMap (
  IN  GENERIC_MEMORY_TEST_PRIVATE  *Private
  )
{
  LIST_ENTRY              *Link;
  NONTESTED_MEMORY_RANGE  *Range;

  Link = GetFirstNode (&Private->NonTestedMemList);

  while (Link != &Private->NonTestedMemList) {
    Range = NONTESTED_MEMORY_RANGE_FROM_LINK (Link);

    ConvertToTestedMemory (
      Range->StartAddress,
      Range->Length,
      Range->Capabilities &~
      (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED | EFI_MEMORY_RUNTIME)
      );
    Link = GetNextNode (&Private->NonTestedMemList, Link);
  }

  return EFI_SUCCESS;
}

/**
  Test a range of the memory directly .

  @param[in] Private       Point to generic memory test driver's private data.
  @param[in] StartAddress  Starting address of the memory range to be tested.
  @param[in] Length        Length in bytes of the memory range to be tested.
  @param[in] Capabilities  The bit mask of attributes that the memory range supports.

  @retval EFI_SUCCESS      Successful test the range of memory.
  @retval Others           Failed to test the range of memory.

**/
EFI_STATUS
DirectRangeTest (
  IN  GENERIC_MEMORY_TEST_PRIVATE  *Private,
  IN  EFI_PHYSICAL_ADDRESS         StartAddress,
  IN  UINT64                       Length,
  IN  UINT64                       Capabilities
  )
{
  EFI_STATUS  Status;

  //
  // Verify the memory range
  //
  Status = MemoryVerificationTestRegion (
             Private->MemoryTestConfig.TestMode,
             Private->MemoryTestConfig.Parameter1,
             Private->MemoryTestConfig.Parameter2,
             StartAddress,
             Length,
             Private->CoverageSpan,
             NULL
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Add the tested compatible memory to system memory using GCD service
  //
  ConvertToTestedMemory (
    StartAddress,
    Length,
    Capabilities &~
    (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED | EFI_MEMORY_RUNTIME)
    );

  return EFI_SUCCESS;
}

/**
  Construct the memory ranges to test.

  @param[in] Private  Point to generic memory test driver's private data.

  @retval EFI_SUCCESS          Successful construct the memory test ranges.
  @retval EFI_OUT_OF_RESOURCE  Could not allocate needed resource from base memory.
  @retval Others               Failed to construct the memory test ranges.

**/
EFI_STATUS
ConstructMemoryTestRanges (
  IN  GENERIC_MEMORY_TEST_PRIVATE  *Private
  )
{
  LIST_ENTRY              *NonTestedRangeNode;
  NONTESTED_MEMORY_RANGE  *NonTestedRange;
  MEMORY_TEST_RANGE       *MemoryTestRange;
  UINTN                   Offset;

  NonTestedRangeNode = GetFirstNode (&Private->NonTestedMemList);

  while (NonTestedRangeNode != &Private->NonTestedMemList) {
    NonTestedRange = NONTESTED_MEMORY_RANGE_FROM_LINK (NonTestedRangeNode);

    Offset = 0;
    while (Offset < NonTestedRange->Length) {
      MemoryTestRange = AllocatePool (sizeof (MEMORY_TEST_RANGE));
      if (MemoryTestRange == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      MemoryTestRange->Signature    = EFI_MEMORY_TEST_RANGE_SIGNATURE;
      MemoryTestRange->StartAddress = NonTestedRange->StartAddress + Offset;
      MemoryTestRange->Length       = MIN (Private->BdsBlockSize, NonTestedRange->Length - Offset);
      MemoryTestRange->CoverageSpan = Private->CoverageSpan;
      MemoryTestRange->BadAddress   = 0;
      MemoryTestRange->TestDone     = FALSE;
      MemoryTestRange->TestStatus   = EFI_SUCCESS;
      MemoryTestRange->Thread       = NULL;
      MemoryTestRange->MemoryError  = &Private->MemoryError;
      MemoryTestRange->TestedMemory = &Private->TestedMemory;
      MemoryTestRange->TestConfig   = &Private->MemoryTestConfig;
      InsertTailList (&Private->MemoryTestList, &MemoryTestRange->Link);
      Offset += Private->BdsBlockSize;
    }

    NonTestedRangeNode = GetNextNode (&Private->NonTestedMemList, NonTestedRangeNode);
  }

  return EFI_SUCCESS;
}

/**
  Construct the system non-tested memory range through GCD service.

  @param[in] Private  Point to generic memory test driver's private data.

  @retval EFI_SUCCESS          Successful construct the non-tested memory range through GCD service.
  @retval EFI_OUT_OF_RESOURCE  Could not allocate needed resource from base memory.
  @retval Others               Failed to construct non-tested memory range through GCD service.

**/
EFI_STATUS
ConstructNonTestedMemoryRange (
  IN  GENERIC_MEMORY_TEST_PRIVATE  *Private
  )
{
  NONTESTED_MEMORY_RANGE           *Range;
  BOOLEAN                          NoFound;
  UINTN                            NumberOfDescriptors;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *MemorySpaceMap;
  UINTN                            Index;

  NoFound = TRUE;

  gDS->GetMemorySpaceMap (&NumberOfDescriptors, &MemorySpaceMap);
  Private->NonTestedSystemMemory = 0;

  for (Index = 0; Index < NumberOfDescriptors; Index++) {
    if ((MemorySpaceMap[Index].GcdMemoryType == EfiGcdMemoryTypeReserved) &&
        ((MemorySpaceMap[Index].Capabilities & (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED)) ==
         (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED))
        )
    {
      NoFound = FALSE;
      gBS->AllocatePool (
             EfiBootServicesData,
             sizeof (NONTESTED_MEMORY_RANGE),
             (VOID **)&Range
             );

      Range->Signature    = EFI_NONTESTED_MEMORY_RANGE_SIGNATURE;
      Range->StartAddress = MemorySpaceMap[Index].BaseAddress;
      Range->Length       = MemorySpaceMap[Index].Length;
      Range->Capabilities = MemorySpaceMap[Index].Capabilities;

      Private->NonTestedSystemMemory += MemorySpaceMap[Index].Length;
      InsertTailList (&Private->NonTestedMemList, &Range->Link);
    }
  }

  if (NoFound) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Initialize the generic memory test.

  @param[in]  This                The protocol instance pointer.
  @param[in]  Level               The coverage level of the memory test.
  @param[out] RequireSoftECCInit  Indicate if the memory need software ECC init.

  @retval EFI_SUCCESS         The generic memory test is initialized correctly.
  @retval EFI_NO_MEDIA        The system had no memory to be tested.

**/
EFI_STATUS
EFIAPI
InitializeMemoryTest (
  IN EFI_GENERIC_MEMORY_TEST_PROTOCOL  *This,
  IN  EXTENDMEM_COVERAGE_LEVEL         Level,
  OUT BOOLEAN                          *RequireSoftECCInit
  )
{
  EFI_STATUS                   Status;
  GENERIC_MEMORY_TEST_PRIVATE  *Private;
  LIST_ENTRY                   *MemoryTestRangeNode;
  MEMORY_TEST_RANGE            *MemoryTestRange;

  Private             = GENERIC_MEMORY_TEST_PRIVATE_FROM_THIS (This);
  *RequireSoftECCInit = FALSE;

  //
  // This is initialize for default value, but some value may be reset base on
  // platform memory test driver.
  //
  Private->CoverLevel = Level;

  //
  // Create the CoverageSpan of the memory test base on the coverage level
  //
  switch (Private->CoverLevel) {
    case EXTENSIVE:
      Private->CoverageSpan = MemoryVerificationGetCacheLineLength ();
      break;

    case SPARSE:
      Private->CoverageSpan = SPARSE_SPAN_SIZE;
      break;

    //
    // Even the BDS do not need to test any memory, but in some case it
    // still need to init ECC memory.
    //
    default:
      Private->CoverageSpan = QUICK_SPAN_SIZE;
      break;
  }

  // Initialize test parameters
  switch (Private->MemoryTestConfig.TestMode) {
    case MemoryTestMovingInversions01:
      Private->MemoryTestConfig.Parameter1 = 0;
      break;
    case MemoryTestMovingInversions8Bit:
      Private->MemoryTestConfig.Parameter1 = 0x8080808080808080;
      break;
    case MemoryTestMovingInversionsRandom:
    case MemoryTestRandomNumberSequence:
    case MemoryTestModulo20Random:
      GetRandomNumber64 (&Private->MemoryTestConfig.Parameter1);
      break;
    default:
      break;
  }

  if (!IsListEmpty (&Private->NonTestedMemList)) {
    // Re-init the memory test
    MemoryTestRangeNode = GetFirstNode (&Private->MemoryTestList);

    while (MemoryTestRangeNode != &Private->MemoryTestList) {
      MemoryTestRange = MEMORY_TEST_RANGE_FROM_LINK (MemoryTestRangeNode);

      if (MemoryTestRange->Thread) {
        Private->ThreadingProtocol->AbortThread (MemoryTestRange->Thread);
        Private->ThreadingProtocol->CleanupThread (MemoryTestRange->Thread);
      }

      MemoryTestRange->CoverageSpan = Private->CoverageSpan;
      MemoryTestRange->BadAddress   = 0;
      MemoryTestRange->TestDone     = FALSE;
      MemoryTestRange->TestStatus   = EFI_SUCCESS;
      MemoryTestRange->Thread       = NULL;
      MemoryTestRangeNode           = GetNextNode (&Private->MemoryTestList, MemoryTestRangeNode);
    }

    Private->MemoryError    = FALSE;
    Private->TestedMemory   = 0;
    Private->ThreadsSpawned = FALSE;
    Private->TestDone       = FALSE;

    return EFI_SUCCESS;
  }

  //
  // This is the first time we construct the non-tested memory range, if no
  // extended memory found, we know the system have not any extended memory
  // need to be test
  //
  Status = ConstructNonTestedMemoryRange (Private);
  if (Status == EFI_NOT_FOUND) {
    return EFI_NO_MEDIA;
  }

  Status = gBS->LocateProtocol (&gEfiThreadingProtocolGuid, NULL, (VOID **)&Private->ThreadingProtocol);
  if (EFI_ERROR (Status)) {
    Private->ThreadingProtocol = NULL;
  } else {
    Private->BdsBlockSize <<= 4;
  }

  Status = ConstructMemoryTestRanges (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Private->MemoryError    = FALSE;
  Private->TestedMemory   = 0;
  Private->ThreadsSpawned = FALSE;
  Private->TestDone       = FALSE;

  return EFI_SUCCESS;
}

VOID
TestMemoryThread (
  VOID  *Parameter
  )
{
  MEMORY_TEST_RANGE  *MemoryTestRange = (MEMORY_TEST_RANGE *)Parameter;

  MemoryTestRange->TestStatus = MemoryVerificationTestRegion (
                                  MemoryTestRange->TestConfig->TestMode,
                                  MemoryTestRange->TestConfig->Parameter1,
                                  MemoryTestRange->TestConfig->Parameter2,
                                  MemoryTestRange->StartAddress,
                                  MemoryTestRange->Length,
                                  MemoryTestRange->CoverageSpan,
                                  &MemoryTestRange->BadAddress
                                  );
  if (MemoryTestRange->TestStatus == EFI_UNSUPPORTED) {
    // Mask unsupported test requests
    MemoryTestRange->TestStatus = EFI_SUCCESS;
  }
}

VOID
TestMemoryThreadDone (
  VOID  *Parameter
  )
{
  MEMORY_TEST_RANGE               *MemoryTestRange = (MEMORY_TEST_RANGE *)Parameter;
  EFI_MEMORY_RANGE_EXTENDED_DATA  RangeData;
  EFI_MEMORY_EXTENDED_ERROR_DATA  ExtendedErrorData;
  UINT64                          OldTested;
  UINT64                          NewTested;
  UINT64                          Value;

  RangeData.DataHeader.HeaderSize = (UINT16)sizeof (EFI_STATUS_CODE_DATA);
  RangeData.DataHeader.Size       = (UINT16)(sizeof (EFI_MEMORY_RANGE_EXTENDED_DATA) - sizeof (EFI_STATUS_CODE_DATA));
  RangeData.Start                 = MemoryTestRange->StartAddress;
  RangeData.Length                = MemoryTestRange->Length;

  REPORT_STATUS_CODE_EX (
    EFI_PROGRESS_CODE,
    EFI_COMPUTING_UNIT_MEMORY | EFI_CU_MEMORY_PC_TEST,
    0,
    &gEfiGenericMemTestProtocolGuid,
    NULL,
    (UINT8 *)&RangeData + sizeof (EFI_STATUS_CODE_DATA),
    RangeData.DataHeader.Size
    );
  if (EFI_ERROR (MemoryTestRange->TestStatus)) {
    ExtendedErrorData.DataHeader.HeaderSize = (UINT16)sizeof (EFI_STATUS_CODE_DATA);
    ExtendedErrorData.DataHeader.Size       = (UINT16)(sizeof (EFI_MEMORY_EXTENDED_ERROR_DATA) - sizeof (EFI_STATUS_CODE_DATA));
    ExtendedErrorData.Granularity           = EFI_MEMORY_ERROR_DEVICE;
    ExtendedErrorData.Operation             = EFI_MEMORY_OPERATION_READ;
    ExtendedErrorData.Syndrome              = 0x0;
    ExtendedErrorData.Address               = MemoryTestRange->BadAddress;
    ExtendedErrorData.Resolution            = MemoryVerificationGetCacheLineLength ();

    REPORT_STATUS_CODE_EX (
      EFI_ERROR_CODE,
      EFI_COMPUTING_UNIT_MEMORY | EFI_CU_MEMORY_EC_UNCORRECTABLE,
      0,
      &gEfiGenericMemTestProtocolGuid,
      NULL,
      (UINT8 *)&ExtendedErrorData + sizeof (EFI_STATUS_CODE_DATA),
      ExtendedErrorData.DataHeader.Size
      );
    *MemoryTestRange->MemoryError = TRUE;
    DEBUG ((DEBUG_ERROR, "\r\nMemory Error detected at 0x%llx\r\n", MemoryTestRange->BadAddress));
    ASSERT (0);
  }

  do {
    OldTested = *MemoryTestRange->TestedMemory;
    NewTested = OldTested + MemoryTestRange->Length;
    Value     = InterlockedCompareExchange64 (MemoryTestRange->TestedMemory, OldTested, NewTested);
  } while (Value != OldTested);

  MemoryTestRange->TestDone = TRUE;
}

MEMORY_TEST_RANGE *
EFIAPI
GetFirstPendingTest (
  IN   GENERIC_MEMORY_TEST_PRIVATE  *Private,
  IN LIST_ENTRY                     *MemoryTestList
  )
{
  MEMORY_TEST_RANGE  *MemoryTestRange;
  LIST_ENTRY         *Link;
  EFI_TPL            OldTpl;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  Link   = GetFirstNode (MemoryTestList);
  while (Link != MemoryTestList) {
    MemoryTestRange = MEMORY_TEST_RANGE_FROM_LINK (Link);
    if (!MemoryTestRange->TestDone) {
      break;
    } else {
      if (MemoryTestRange->Thread != NULL) {
        Private->ThreadingProtocol->CleanupThread (MemoryTestRange->Thread);
        MemoryTestRange->Thread = NULL;
      }
    }

    Link = GetNextNode (MemoryTestList, Link);
  }

  gBS->RestoreTPL (OldTpl);
  if (Link == MemoryTestList) {
    return NULL;
  } else {
    return MemoryTestRange;
  }
}

/**
  Perform the memory test.

  @param[in]  This              The protocol instance pointer.
  @param[out] TestedMemorySize  Return the tested extended memory size.
  @param[out] TotalMemorySize   Return the whole system physical memory size.
                                The total memory size does not include memory in a slot with a disabled DIMM.
  @param[out] ErrorOut          TRUE if the memory error occurred.
  @param[in]  IfTestAbort       Indicates that the user pressed "ESC" to skip the memory test.

  @retval EFI_SUCCESS         One block of memory passed the test.
  @retval EFI_NOT_FOUND       All memory blocks have already been tested.
  @retval EFI_DEVICE_ERROR    Memory device error occurred, and no agent can handle it.

**/
EFI_STATUS
EFIAPI
GenPerformMemoryTest (
  IN EFI_GENERIC_MEMORY_TEST_PROTOCOL  *This,
  OUT UINT64                           *TestedMemorySize,
  OUT UINT64                           *TotalMemorySize,
  OUT BOOLEAN                          *ErrorOut,
  IN BOOLEAN                           TestAbort
  )
{
  EFI_STATUS                   Status;
  GENERIC_MEMORY_TEST_PRIVATE  *Private;
  MEMORY_TEST_RANGE            *MemoryTestRange;
  LIST_ENTRY                   *Link;
  EFI_TPL                      OldTpl;

  Private   = GENERIC_MEMORY_TEST_PRIVATE_FROM_THIS (This);
  *ErrorOut = FALSE;

  if (Private->TestDone) {
    return EFI_NOT_FOUND;
  }

  if ((Private->CoverLevel == IGNORE) || TestAbort) {
    if (TestAbort) {
      if (Private->ThreadingProtocol != NULL) {
        // Cancel all threads
        OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
        Link   = GetFirstNode (&Private->MemoryTestList);
        while (Link != &Private->MemoryTestList) {
          MemoryTestRange = MEMORY_TEST_RANGE_FROM_LINK (Link);
          Private->ThreadingProtocol->AbortThread (MemoryTestRange->Thread);
          Private->ThreadingProtocol->CleanupThread (MemoryTestRange->Thread);
          Link = GetNextNode (&Private->MemoryTestList, Link);
        }

        gBS->RestoreTPL (OldTpl);
      }
    }

    Private->TestDone = TRUE;
    *TotalMemorySize  = Private->BaseMemorySize + Private->NonTestedSystemMemory;
    *TestedMemorySize = Private->BaseMemorySize + Private->NonTestedSystemMemory;
    *ErrorOut         = Private->MemoryError;
    return EFI_SUCCESS;
  }

  if (Private->ThreadingProtocol != NULL) {
    if (!Private->ThreadsSpawned) {
      OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
      Link   = GetFirstNode (&Private->MemoryTestList);
      while (Link != &Private->MemoryTestList) {
        MemoryTestRange = MEMORY_TEST_RANGE_FROM_LINK (Link);
        Status          = Private->ThreadingProtocol->SpawnThread (
                                                        TestMemoryThread,
                                                        (VOID *)MemoryTestRange,
                                                        TestMemoryThreadDone,
                                                        (VOID *)MemoryTestRange,
                                                        0,
                                                        &MemoryTestRange->Thread
                                                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to spawn thread - %r\r\n", __FUNCTION__, Status));
          gBS->RestoreTPL (OldTpl);
          return Status;
        }

        Link = GetNextNode (&Private->MemoryTestList, Link);
      }

      gBS->RestoreTPL (OldTpl);
      Private->ThreadsSpawned = TRUE;
    }
  } else {
    MemoryTestRange = GetFirstPendingTest (Private, &Private->MemoryTestList);
    TestMemoryThread (MemoryTestRange);
    TestMemoryThreadDone (MemoryTestRange);
  }

  *TestedMemorySize = Private->BaseMemorySize + Private->TestedMemory;
  *TotalMemorySize  = Private->BaseMemorySize + Private->NonTestedSystemMemory;
  *ErrorOut         = Private->MemoryError;

  if (GetFirstPendingTest (Private, &Private->MemoryTestList) == NULL) {
    Private->TestDone = TRUE;
  }

  if (Private->MemoryError) {
    return EFI_DEVICE_ERROR;
  } else {
    return EFI_SUCCESS;
  }
}

/**
  Finish the memory test.

  @param[in] This             The protocol instance pointer.

  @retval EFI_SUCCESS         Success. All resources used in the memory test are freed.

**/
EFI_STATUS
EFIAPI
GenMemoryTestFinished (
  IN EFI_GENERIC_MEMORY_TEST_PROTOCOL  *This
  )
{
  GENERIC_MEMORY_TEST_PRIVATE  *Private;

  Private = GENERIC_MEMORY_TEST_PRIVATE_FROM_THIS (This);

  //
  // Add the non tested memory range to system memory map through GCD service
  //
  UpdateMemoryMap (Private);

  //
  // we need to free all the memory allocate
  //
  DestroyLinkList (Private);

  return EFI_SUCCESS;
}

/**
  Provides the capability to test the compatible range used by some special drivers.

  @param[in]  This              The protocol instance pointer.
  @param[in]  StartAddress      The start address of the compatible memory range that
                                must be below 16M.
  @param[in]  Length            The compatible memory range's length.

  @retval EFI_SUCCESS           The compatible memory range pass the memory test.
  @retval EFI_INVALID_PARAMETER The compatible memory range are not below Low 16M.

**/
EFI_STATUS
EFIAPI
GenCompatibleRangeTest (
  IN EFI_GENERIC_MEMORY_TEST_PROTOCOL  *This,
  IN EFI_PHYSICAL_ADDRESS              StartAddress,
  IN UINT64                            Length
  )
{
  EFI_STATUS                       Status;
  GENERIC_MEMORY_TEST_PRIVATE      *Private;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor;
  EFI_PHYSICAL_ADDRESS             CurrentBase;
  UINT64                           CurrentLength;

  Private = GENERIC_MEMORY_TEST_PRIVATE_FROM_THIS (This);

  CurrentBase = StartAddress;
  do {
    //
    // Check the required memory range status; if the required memory range span
    // the different GCD memory descriptor, it may be cause different action.
    //
    Status = gDS->GetMemorySpaceDescriptor (
                    CurrentBase,
                    &Descriptor
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if ((Descriptor.GcdMemoryType == EfiGcdMemoryTypeReserved) &&
        ((Descriptor.Capabilities & (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED)) ==
         (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED))
        )
    {
      CurrentLength = Descriptor.BaseAddress + Descriptor.Length - CurrentBase;
      if (CurrentBase + CurrentLength > StartAddress + Length) {
        CurrentLength = StartAddress + Length - CurrentBase;
      }

      Status = DirectRangeTest (
                 Private,
                 CurrentBase,
                 CurrentLength,
                 Descriptor.Capabilities
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    CurrentBase = Descriptor.BaseAddress + Descriptor.Length;
  } while (CurrentBase < StartAddress + Length);

  //
  // Here means the required range already be tested, so just return success.
  //
  return EFI_SUCCESS;
}

//
// Driver entry here
//
GENERIC_MEMORY_TEST_PRIVATE  mGenericMemoryTestPrivate;

/**
  The generic memory test driver's entry point.

  It initializes private data to default value.

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS     The entry point is executed successfully.
  @retval EFI_NOT_FOUND   Can't find HandOff Hob in HobList.
  @retval other           Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
GenericMemoryTestEntryPoint (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  mGenericMemoryTestPrivate.Signature                             = EFI_GENERIC_MEMORY_TEST_PRIVATE_SIGNATURE;
  mGenericMemoryTestPrivate.GenericMemoryTest.MemoryTestInit      = InitializeMemoryTest;
  mGenericMemoryTestPrivate.GenericMemoryTest.PerformMemoryTest   = GenPerformMemoryTest;
  mGenericMemoryTestPrivate.GenericMemoryTest.Finished            = GenMemoryTestFinished;
  mGenericMemoryTestPrivate.GenericMemoryTest.CompatibleRangeTest = GenCompatibleRangeTest;
  mGenericMemoryTestPrivate.CoverLevel                            = IGNORE;
  mGenericMemoryTestPrivate.CoverageSpan                          = SPARSE_SPAN_SIZE;
  mGenericMemoryTestPrivate.BdsBlockSize                          = TEST_BLOCK_SIZE;
  Status                                                          = ConstructBaseMemoryRange (&mGenericMemoryTestPrivate);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Initialize several internal link lists
  //
  InitializeListHead (&mGenericMemoryTestPrivate.NonTestedMemList);
  InitializeListHead (&mGenericMemoryTestPrivate.MemoryTestList);

  //
  // Install the protocol
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiGenericMemTestProtocolGuid,
                  &mGenericMemoryTestPrivate.GenericMemoryTest,
                  &gNVIDIAMemoryTestConfig,
                  &mGenericMemoryTestPrivate.MemoryTestConfig,
                  NULL
                  );
  return Status;
}
