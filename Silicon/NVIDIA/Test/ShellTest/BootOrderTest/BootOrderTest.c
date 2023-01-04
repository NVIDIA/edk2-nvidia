/** @file
  Boot order test

  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Guid/GlobalVariable.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UnitTestLib.h>
#include <Protocol/LoadedImage.h>

#define UNIT_TEST_NAME     "Boot order test"
#define UNIT_TEST_VERSION  "0.1.0"

/// Name of the variable used to persist the boot order test context.
#define NVDA_TEST_CONTEXT_VARIABLE_NAME  L"BootOrderTestContext"

/// The number of non-existent boot options to include in the test
/// permutation.
#define NONEXISTENT_OPTION_COUNT  1

/// The number of failing boot options to include in the test
/// permutation.
#define FAILING_OPTION_COUNT  2

/// Total number of boot options to include in the test
/// permutation. Note that the extra one option is the successful boot
/// option, which must always be present to actually verify the
/// recorded boot sequence.
#define TOTAL_OPTION_COUNT              \
  (NONEXISTENT_OPTION_COUNT             \
   + FAILING_OPTION_COUNT + 1)

#pragma pack (1)
typedef struct {
  /// TRUE if we are running BootNext test, FALSE otherwise.
  BOOLEAN    UseBootNext;

  /// Size of the data originally stored in the BootOrder variable.
  UINT32     OriginalBootOrderSize;

  /// An array mapping internal boot option ids used in the test
  /// permutation (i.e. numbers between 1 and TOTAL_OPTION_COUNT) to
  /// the boot option numbers used for BootOrder and Boot####
  /// variables.
  UINT16     BootOptionNumber[TOTAL_OPTION_COUNT];

  /// The permutation of boot options currently under test.
  UINT16     TestPermutation[TOTAL_OPTION_COUNT];
} BOOT_ORDER_TEST_CONTEXT_HEADER;

typedef struct {
  /// The fixed-size test context header.
  BOOT_ORDER_TEST_CONTEXT_HEADER    Hdr;

  /// The boot sequence as recorded during the last boot.
  UINT16                            RecordedBootSequence[TOTAL_OPTION_COUNT];

  /// The number of valid entries in the RecordedBootSequence array.
  UINT16                            RecordedBootSequenceLength;
} BOOT_ORDER_TEST_CONTEXT;
#pragma pack ()

/**
   Determines if the given option id corresponds to a non-existent
   boot option.

   @param[in] OptionId Boot option id to check.
*/
STATIC
BOOLEAN
EFIAPI
IsNonexistentBootOption (
  IN CONST UINT16  OptionId
  )
{
  return OptionId < NONEXISTENT_OPTION_COUNT;
}

/**
   Determines if the given option id corresponds to a failing boot
   option.

   @param[in] OptionId Boot option id to check.
*/
STATIC
BOOLEAN
EFIAPI
IsFailingBootOption (
  IN CONST UINT16  OptionId
  )
{
  return NONEXISTENT_OPTION_COUNT <= OptionId
         && OptionId < NONEXISTENT_OPTION_COUNT + FAILING_OPTION_COUNT;
}

/**
   Determines if the given option id corresponds to a successful boot
   option.

   @param[in] OptionId Boot option id to check.
*/
STATIC
BOOLEAN
EFIAPI
IsSuccessfulBootOption (
  IN CONST UINT16  OptionId
  )
{
  return NONEXISTENT_OPTION_COUNT + FAILING_OPTION_COUNT <= OptionId
         && OptionId < TOTAL_OPTION_COUNT;
}

/**
   Saves the boot order test context into persistent storage for later
   retrieval.

   @param [in] Context The test context to save.

   @retval EFI_SUCCESS Context saved successfully.
*/
STATIC
EFI_STATUS
EFIAPI
SaveTestContext (
  IN CONST BOOT_ORDER_TEST_CONTEXT *CONST  Context
  )
{
  EFI_STATUS  Status;

  CONST UINTN  ContextSize =
    sizeof (Context->Hdr)
    + (Context->RecordedBootSequenceLength
       * sizeof (*Context->RecordedBootSequence));

  if (!((ContextSize
         + sizeof (Context->RecordedBootSequenceLength)) <= sizeof (*Context)))
  {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: recorded boot sequence too long\r\n",
      __FUNCTION__
      ));
    return EFI_INVALID_PARAMETER;
  }

  Status = gRT->SetVariable (
                  NVDA_TEST_CONTEXT_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS
                  | EFI_VARIABLE_NON_VOLATILE,
                  ContextSize,
                  (VOID *)Context
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Could not persist context."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}

/**
   Loads the boot order test context from persistent storage.

   @param [in] Context Where to save the loaded context.

   @retval EFI_SUCCESS   Context loaded successfully.
   @retval EFI_NOT_FOUND No context has been previously saved.
*/
STATIC
EFI_STATUS
EFIAPI
LoadTestContext (
  IN BOOT_ORDER_TEST_CONTEXT *CONST  Context
  )
{
  EFI_STATUS  Status;
  UINTN       ContextSize;

  ContextSize = sizeof (*Context);
  Status      = gRT->GetVariable (
                       NVDA_TEST_CONTEXT_VARIABLE_NAME,
                       &gNVIDIATokenSpaceGuid,
                       NULL,
                       &ContextSize,
                       (VOID *)Context
                       );
  if (EFI_ERROR (Status)) {
    if (!(Status == EFI_NOT_FOUND)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: %a: Could not retrieve context."
        " Status = %r\r\n",
        __FUNCTION__,
        Status
        ));
    }

    return Status;
  }

  if (!(ContextSize >= sizeof (Context->Hdr))) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Buffer too short."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  ContextSize -= sizeof (Context->Hdr);

  Context->RecordedBootSequenceLength =
    ContextSize / sizeof (*Context->RecordedBootSequence);
  if (!(  (ContextSize == (Context->RecordedBootSequenceLength
                           * sizeof (*Context->RecordedBootSequence)))
       && (ContextSize <= sizeof (Context->RecordedBootSequence))))
  {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Unexpected data at the end of buffer\r\n",
      __FUNCTION__
      ));
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
   Appends a boot option id to the recorded boot sequence of a
   persisted boot test context.

   @param [in] BootOptionId Boot option id to append.
*/
STATIC
EFI_STATUS
EFIAPI
RecordTestContextBootSequence (
  IN CONST UINT16  BootOptionId
  )
{
  EFI_STATUS  Status;

  Status = gRT->SetVariable (
                  NVDA_TEST_CONTEXT_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS
                  | EFI_VARIABLE_NON_VOLATILE
                  | EFI_VARIABLE_APPEND_WRITE,
                  sizeof (BootOptionId),
                  (VOID *)&BootOptionId
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Could not append boot option id."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}

/**
   Finds and stores boot option numbers that are available to use for
   the boot order test.

   @param [in] Context The boot order test context to update.
*/
STATIC
EFI_STATUS
EFIAPI
GetBootOptionNumbers (
  IN BOOT_ORDER_TEST_CONTEXT *CONST  Context
  )
{
  EFI_STATUS  Status;
  UINTN       BootOptionId     = 0;
  UINT16      BootOptionNumber = 0;
  CHAR16      BootOptionName[sizeof ("Boot####")];
  UINTN       DataSize;

  do {
    UnicodeSPrint (
      BootOptionName,
      sizeof (BootOptionName),
      L"Boot%04x",
      BootOptionNumber
      );

    DataSize = 0;
    Status   = gRT->GetVariable (
                      BootOptionName,
                      &gEfiGlobalVariableGuid,
                      NULL,
                      &DataSize,
                      NULL
                      );
    if (Status == EFI_NOT_FOUND) {
      // We found a free boot option number, store it.
      Context->Hdr.BootOptionNumber[BootOptionId++] = BootOptionNumber;
    } else if (EFI_ERROR (Status) && !(Status == EFI_BUFFER_TOO_SMALL)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: %a: Could not determine if variable %s exists."
        " Status = %r\r\n",
        __FUNCTION__,
        BootOptionName,
        Status
        ));
      return Status;
    }

    if (++BootOptionNumber == 0) {
      // BootOptionNumber wrapped around to zero, meaning that we have
      // tried all possibilities, but have not found enough free boot
      // option numbers.
      return EFI_OUT_OF_RESOURCES;
    }
  } while (BootOptionId < ARRAY_SIZE (Context->Hdr.BootOptionNumber));

  return EFI_SUCCESS;
}

/**
   Creates a test boot option (i.e. Boot#### variable) for the given
   boot option id.

   @param [in] FilePath File path to this image.
   @param [in] Context  Boot order test context.
   @param [in] OptionId Boot option id to create the variable for.
*/
STATIC
EFI_STATUS
EFIAPI
CreateBootOption (
  IN EFI_DEVICE_PATH_PROTOCOL *CONST  FilePath,
  IN BOOT_ORDER_TEST_CONTEXT  *CONST  Context,
  IN CONST UINT16                     OptionId
  )
{
  EFI_STATUS                    Status;
  EFI_STATUS                    Status2;
  EFI_BOOT_MANAGER_LOAD_OPTION  LoadOption;
  CHAR16                        Description[sizeof ("Test boot option ##")];
  UINT8                         *OptionalData;
  UINT32                        OptionalDataSize;

  if (IsNonexistentBootOption (OptionId)) {
    // Non-existent boot options do not have the corresponding
    // Boot#### variable.
    return EFI_SUCCESS;
  }

  UnicodeSPrint (
    Description,
    sizeof (Description),
    L"Test boot option %02u",
    OptionId
    );

  if (IsFailingBootOption (OptionId)) {
    OptionalData     = (UINT8 *)&OptionId;
    OptionalDataSize = sizeof (OptionId);
  } else {
    /* IsSuccessfulBootOption (OptionId) == TRUE */
    OptionalData     = NULL;
    OptionalDataSize = 0;
  }

  Status = EfiBootManagerInitializeLoadOption (
             &LoadOption,
             Context->Hdr.BootOptionNumber[OptionId],
             LoadOptionTypeBoot,
             LOAD_OPTION_ACTIVE,
             Description,
             FilePath,
             OptionalData,
             OptionalDataSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Cannot initialize load option."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = EfiBootManagerLoadOptionToVariable (&LoadOption);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Cannot create boot option variable."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
  }

  Status2 = EfiBootManagerFreeLoadOption (&LoadOption);
  if (EFI_ERROR (Status2)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Cannot free load option."
      " Status = %r\r\n",
      __FUNCTION__,
      Status2
      ));
  }

  return EFI_ERROR (Status) ? Status : Status2;
}

/**
   Creates the necessary test boot options (i.e. Boot#### variables).

   @param [in] Context Boot order test context.
*/
STATIC
EFI_STATUS
EFIAPI
CreateBootOptions (
  IN BOOT_ORDER_TEST_CONTEXT *CONST  Context
  )
{
  EFI_STATUS                 Status;
  EFI_STATUS                 Status2;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL   *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL   *FilePath = NULL;
  UINT16                     OptionId;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Failed to retrieve loaded image protocol from the image handle."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  if (DevicePath == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Could not retrieve device path\r\n",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    goto error_handler;
  }

  FilePath = AppendDevicePath (DevicePath, LoadedImage->FilePath);
  if (FilePath == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Could not append file path to device path\r\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto error_handler;
  }

  Status = EFI_SUCCESS;
  for (OptionId = 0; OptionId < ARRAY_SIZE (Context->Hdr.BootOptionNumber); ++OptionId) {
    Status = CreateBootOption (FilePath, Context, OptionId);
    if (EFI_ERROR (Status)) {
      goto error_handler;
    }
  }

error_handler:
  Status2 = EFI_SUCCESS;
  if (FilePath != NULL) {
    Status2 = gBS->FreePool (FilePath);
    if (EFI_ERROR (Status2)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: %a: Could not free file path."
        " Status = %r\r\n",
        __FUNCTION__,
        Status2
        ));
    }
  }

  return EFI_ERROR (Status) ? Status : Status2;
}

/**
   Deletes all the test boot options (i.e. Boot#### variables).

   @param [in] Context Boot order test context.
*/
STATIC
EFI_STATUS
EFIAPI
DeleteBootOptions (
  IN BOOT_ORDER_TEST_CONTEXT  *CONST  Context
  )
{
  EFI_STATUS  Status;
  UINT16      OptionId;
  UINT16      OptionNumber;

  for (OptionId = 0; OptionId < ARRAY_SIZE (Context->Hdr.BootOptionNumber); ++OptionId) {
    OptionNumber = Context->Hdr.BootOptionNumber[OptionId];
    Status       = EfiBootManagerDeleteLoadOptionVariable (
                     OptionNumber,
                     LoadOptionTypeBoot
                     );
    if (EFI_ERROR (Status) && !(Status == EFI_NOT_FOUND)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: %a: Failed to delete load option variable Boot%04x."
        " Status = %r\r\n",
        __FUNCTION__,
        OptionNumber,
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
   Swaps two UINT16 values.

   @param [in] First  Pointer to the first value.
   @param [in] Second Pointer to the second value.
*/
STATIC
VOID
EFIAPI
Swap16 (
  IN UINT16 *CONST  First,
  IN UINT16 *CONST  Second
  )
{
  UINT16  Temp;

  Temp    = *First;
  *First  = *Second;
  *Second = Temp;
}

/**
   Reverses a range of UINT16 values.

   @param [in] First Pointer to the first element of the range.
   @param [in] Last  Pointer past the last element of the range.
*/
STATIC
VOID
EFIAPI
Reverse16 (
  IN UINT16 *CONST  First,
  IN UINT16 *CONST  Last
  )
{
  UINT16  *Left  = First;
  UINT16  *Right = Last - 1;

  for ( ; Left < Right; ++Left, --Right) {
    Swap16 (Left, Right);
  }
}

/**
   Prints the given permutation range.

   @param [in] First  Pointer to the first element of the range.
   @param [in] Last   Pointer past the last element of the range.
*/
STATIC
VOID
EFIAPI
PrintRange16 (
  IN CONST UINT16 *CONST  First,
  IN CONST UINT16 *CONST  Last
  )
{
  CONST UINT16  *Current = First;

  if (Current < Last) {
    Print (L"%04x", *Current++);
    for ( ; Current < Last; ++Current) {
      Print (L" %04x", *Current);
    }
  }
}

/**
   Initialize a range to the identity permutation.

   @param [in] First Pointer to the first element of the range.
   @param [in] Last  Pointer past the last element of the range.

   @retval EFI_SUCCESS Permutation initialized successfully.
*/
STATIC
EFI_STATUS
EFIAPI
InitPermutation (
  IN UINT16 *CONST  First,
  IN UINT16 *CONST  Last
  )
{
  UINT16  Index;

  for (Index = 0; First + Index < Last; ++Index) {
    First[Index] = Index;
  }

  return EFI_SUCCESS;
}

/**
   Rearrange elements in a range into the next lexicographically
   greater permutation.

   @param [in] First Pointer to the first element of the range.
   @param [in] Last  Pointer past the last element of the range.

   @retval EFI_SUCCESS   Next permutation successfully generated.
   @retval EFI_NOT_FOUND No lexicographically greater permutation
                         exists.
*/
STATIC
EFI_STATUS
EFIAPI
NextPermutation (
  IN UINT16 *CONST  First,
  IN UINT16 *CONST  Last
  )
{
  UINT16  *Current = Last - 1;
  UINT16  *Next;
  UINT16  *Other;

  if (!(First < Current)) {
    return EFI_NOT_FOUND;
  }

  while (1) {
    Next = Current--;
    if (*Current < *Next) {
      Other = Last;
      while (!(*Current < *--Other)) {
      }

      Swap16 (Current, Other);
      Reverse16 (Next, Last);
      return EFI_SUCCESS;
    }

    if (Current == First) {
      Reverse16 (First, Last);
      return EFI_NOT_FOUND;
    }
  }
}

/**
   Verifies the recorded boot sequence against the expectation.

   @param [in] Context Boot order test context.

   @retval UNIT_TEST_PASSED            Verification successful.
   @retval UNIT_TEST_ERROR_TEST_FAILED Verification failed.
*/
STATIC
UNIT_TEST_STATUS
EFIAPI
VerifyRecordedBootSequence (
  IN BOOT_ORDER_TEST_CONTEXT *CONST  Context
  )
{
  UINT16  PermIdx;
  UINT16  SeqIdx;
  UINT16  OptionId;

  SeqIdx = 0;
  for (PermIdx = 0; PermIdx < ARRAY_SIZE (Context->Hdr.TestPermutation); ++PermIdx) {
    OptionId = Context->Hdr.TestPermutation[PermIdx];

    if (IsFailingBootOption (OptionId)) {
      UT_ASSERT_EQUAL (OptionId, Context->RecordedBootSequence[SeqIdx++]);
    } else if (IsSuccessfulBootOption (OptionId)) {
      UT_ASSERT_EQUAL (SeqIdx, Context->RecordedBootSequenceLength);
      return UNIT_TEST_PASSED;
    }

    // ... else non-existent boot option, which we just skip over.
  }

  return UNIT_TEST_ERROR_TEST_FAILED;
}

/**
   Writes the test permutation into the BootOrder variable.

   @param [in] Context Boot order test context.
*/
STATIC
EFI_STATUS
EFIAPI
WriteBootOrder (
  IN BOOT_ORDER_TEST_CONTEXT *CONST  Context
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  Status2;
  UINT8       *BootOrderValue  = NULL;
  UINT8       *BootOrderBuffer = NULL;
  UINTN       BootOrderSize;
  UINTN       TestPermutationSize;
  UINT16      Idx;
  UINT16      OptionId;

  Status = GetEfiGlobalVariable2 (
             EFI_BOOT_ORDER_VARIABLE_NAME,
             (VOID **)&BootOrderValue,
             &BootOrderSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Cannot read boot order."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto error_handler;
  }

  if (!(Context->Hdr.OriginalBootOrderSize <= BootOrderSize)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Invalid boot order size\r\n",
      __FUNCTION__
      ));
    goto error_handler;
  }

  TestPermutationSize = sizeof (Context->Hdr.TestPermutation);
  if (Context->Hdr.UseBootNext) {
    TestPermutationSize -= sizeof (*Context->Hdr.TestPermutation);
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  TestPermutationSize + Context->Hdr.OriginalBootOrderSize,
                  (VOID **)&BootOrderBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Cannot allocate boot order buffer."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto error_handler;
  }

  if (Context->Hdr.UseBootNext) {
    OptionId = Context->Hdr.TestPermutation[0];
    Status   = gRT->SetVariable (
                      EFI_BOOT_NEXT_VARIABLE_NAME,
                      &gEfiGlobalVariableGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS
                      | EFI_VARIABLE_RUNTIME_ACCESS
                      | EFI_VARIABLE_NON_VOLATILE,
                      sizeof (Context->Hdr.BootOptionNumber[OptionId]),
                      &Context->Hdr.BootOptionNumber[OptionId]
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: %a: Cannot update variable BootNext."
        " Status = %r\r\n",
        __FUNCTION__,
        Status
        ));
      goto error_handler;
    }

    for (Idx = 1; Idx < ARRAY_SIZE (Context->Hdr.TestPermutation); ++Idx) {
      OptionId                             = Context->Hdr.TestPermutation[Idx];
      ((UINT16 *)BootOrderBuffer)[Idx - 1] = Context->Hdr.BootOptionNumber[OptionId];
    }
  } else {
    for (Idx = 0; Idx < ARRAY_SIZE (Context->Hdr.TestPermutation); ++Idx) {
      OptionId                         = Context->Hdr.TestPermutation[Idx];
      ((UINT16 *)BootOrderBuffer)[Idx] = Context->Hdr.BootOptionNumber[OptionId];
    }
  }

  // Append the last Context->Hdr.OriginalBootOrderSize bytes from the
  // previous boot order value. This corresponds to the original value
  // (before testing) of the BootOrder variable.
  gBS->CopyMem (
         BootOrderBuffer + TestPermutationSize,
         BootOrderValue + BootOrderSize - Context->Hdr.OriginalBootOrderSize,
         Context->Hdr.OriginalBootOrderSize
         );

  Status = gRT->SetVariable (
                  EFI_BOOT_ORDER_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS
                  | EFI_VARIABLE_RUNTIME_ACCESS
                  | EFI_VARIABLE_NON_VOLATILE,
                  TestPermutationSize + Context->Hdr.OriginalBootOrderSize,
                  BootOrderBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Cannot write boot order."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto error_handler;
  }

error_handler:
  Status2 = EFI_SUCCESS;
  if (BootOrderBuffer != NULL) {
    Status2 = gBS->FreePool (BootOrderBuffer);
    if (EFI_ERROR (Status2)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: %a: Cannot free boot order buffer."
        " Status = %r\r\n",
        __FUNCTION__,
        Status2
        ));
    }
  }

  if (BootOrderValue != NULL) {
    Status2 = gBS->FreePool (BootOrderValue);
    if (EFI_ERROR (Status2)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: %a: Cannot free boot order value."
        " Status = %r\r\n",
        __FUNCTION__,
        Status2
        ));
    }
  }

  return EFI_ERROR (Status) ? Status : Status2;
}

/**
   Entry point of the boot order test.

   @param [in] UseBootNext Whether to run BootNext test; only used if
                           boot order test context does not exist yet.

   @retval UNIT_TEST_PASSED The test passed.
*/
STATIC
UNIT_TEST_STATUS
EFIAPI
BootOrderTest (
  IN CONST BOOLEAN  UseBootNext
  )
{
  EFI_STATUS               Status;
  UNIT_TEST_STATUS         TestStatus;
  BOOT_ORDER_TEST_CONTEXT  Context;
  UINTN                    OriginalBootOrderSize;

  gBS->SetMem (&Context, sizeof (Context), 0);

  Status = LoadTestContext (&Context);
  if (Status == EFI_NOT_FOUND) {
    // Initialization run
    Context.Hdr.UseBootNext = UseBootNext;

    OriginalBootOrderSize = 0;
    Status                = gRT->GetVariable (
                                   EFI_BOOT_ORDER_VARIABLE_NAME,
                                   &gEfiGlobalVariableGuid,
                                   NULL,
                                   &OriginalBootOrderSize,
                                   NULL
                                   );
    UT_ASSERT_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
    Context.Hdr.OriginalBootOrderSize = OriginalBootOrderSize;

    Status = GetBootOptionNumbers (&Context);
    UT_ASSERT_NOT_EFI_ERROR (Status);

    Status = CreateBootOptions (&Context);
    UT_ASSERT_NOT_EFI_ERROR (Status);

    Status = InitPermutation (
               Context.Hdr.TestPermutation,
               Context.Hdr.TestPermutation
               + ARRAY_SIZE (Context.Hdr.TestPermutation)
               );
    UT_ASSERT_NOT_EFI_ERROR (Status);
  } else if (!EFI_ERROR (Status)) {
    // Verification run
    TestStatus = VerifyRecordedBootSequence (&Context);
    if (!(TestStatus == UNIT_TEST_PASSED)) {
      return TestStatus;
    }

    Status = NextPermutation (
               Context.Hdr.TestPermutation,
               Context.Hdr.TestPermutation
               + ARRAY_SIZE (Context.Hdr.TestPermutation)
               );
    if (Status == EFI_NOT_FOUND) {
      return UNIT_TEST_PASSED;
    }

    UT_ASSERT_NOT_EFI_ERROR (Status);
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Could not load test context."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    return UNIT_TEST_ERROR_TEST_FAILED;
  }

  Print (L"UseBootNext     = %u\r\n", (UINTN)Context.Hdr.UseBootNext);
  Print (L"TestPermutation = ");
  PrintRange16 (
    Context.Hdr.TestPermutation,
    Context.Hdr.TestPermutation
    + ARRAY_SIZE (Context.Hdr.TestPermutation)
    );
  Print (L"\r\n");

  Status = WriteBootOrder (&Context);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Context.RecordedBootSequenceLength = 0;
  Status                             = SaveTestContext (&Context);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);
  // ResetSystem should never return
  return UNIT_TEST_ERROR_TEST_FAILED;
}

/**
   Boot order test cleanup entry point.

   @param [in] UseBootNext Whether to run BootNext test (unused).
*/
STATIC
VOID
EFIAPI
BootOrderTestCleanup (
  IN CONST BOOLEAN  UseBootNext
  )
{
  EFI_STATUS               Status;
  BOOT_ORDER_TEST_CONTEXT  Context;

  (VOID)UseBootNext;            // Unused

  Status = LoadTestContext (&Context);
  ASSERT (!EFI_ERROR (Status) || Status == EFI_NOT_FOUND);

  if (!EFI_ERROR (Status)) {
    if (Context.Hdr.UseBootNext) {
      Status = gRT->SetVariable (
                      EFI_BOOT_NEXT_VARIABLE_NAME,
                      &gEfiGlobalVariableGuid,
                      0,
                      0,
                      NULL
                      );
      ASSERT (!EFI_ERROR (Status) || Status == EFI_NOT_FOUND);
    }

    Status = DeleteBootOptions (&Context);
    ASSERT (!EFI_ERROR (Status));

    Status = gRT->SetVariable (
                    NVDA_TEST_CONTEXT_VARIABLE_NAME,
                    &gNVIDIATokenSpaceGuid,
                    0,
                    0,
                    NULL
                    );
    ASSERT (!EFI_ERROR (Status));
  }
}

/**
   Initializes the boot order test suite.

   @param [in] Framework   Handle to the unit test framework.
   @param [in] UseBootNext Whether to run BootNext test.

   @retval EFI_SUCCESS All tests ran successfully.
*/
STATIC
EFI_STATUS
EFIAPI
InitTestSuite (
  IN CONST UNIT_TEST_FRAMEWORK_HANDLE  Framework,
  IN CONST BOOLEAN                     UseBootNext
  )
{
  EFI_STATUS              Status;
  UNIT_TEST_SUITE_HANDLE  TestSuite;

  Status = CreateUnitTestSuite (
             &TestSuite,
             Framework,
             "Boot Order Tests",
             "NVIDIA-Internal.BootOrder",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Failed to create a unit test suite."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  AddTestCase (
    TestSuite,
    "Test boot order",
    "BootOrderTest",
    (UNIT_TEST_FUNCTION)BootOrderTest,
    NULL,
    (UNIT_TEST_CLEANUP)BootOrderTestCleanup,
    (UNIT_TEST_CONTEXT)(UINTN)UseBootNext
    );

  return Status;
}

/**
   Run the boot order test in UEFI DXE stage / UEFI shell.

   @param [in] ImageHandle UEFI image handle.
   @param [in] SystemTable UEFI System Table.

   @retval EFI_SUCCESS All tests ran successfully.
*/
EFI_STATUS
EFIAPI
BootOrderTestDxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;
  UINT16                      BootOptionId;
  CHAR16                      *CmdLine;
  UINT32                      CmdLineSize;
  BOOLEAN                     UseBootNext;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: %a: Failed to retrieve loaded image protocol from the image handle."
      " Status = %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  if (LoadedImage->LoadOptionsSize == sizeof (BootOptionId)) {
    // A boot option id has been passed in the load options, which
    // means we are in a middle of a boot order test. Record the
    // passed boot option id to persistent test context, then return
    // an error to continue booting the next boot option.

    BootOptionId = *(UINT16 *)LoadedImage->LoadOptions;

    // Write the just-booted option id into the persistent test
    // context.
    Status = RecordTestContextBootSequence (BootOptionId);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    // Return a fake error, so that the boot manager moves on to the
    // next boot option in the boot order.
    return EFI_VOLUME_CORRUPTED;
  } else {
    // Otherwise, this is either initial test invocation or a boot
    // sequence verification test run. Set up the test framework and
    // run the test code; initial test invocation and boot sequence
    // verification run are differentiated by presence of a persistent
    // test context.

    UseBootNext = FALSE;

    CmdLine     = (CHAR16 *)LoadedImage->LoadOptions;
    CmdLineSize = LoadedImage->LoadOptionsSize / sizeof (*CmdLine);
    if (CmdLineSize > 0) {
      // This is most likely initial test invocation from the UEFI
      // shell. Scan the command line (passed as load options by the
      // shell) for -BootNext flag, which indicates we should run the
      // BootNext boot order test.

      ASSERT (CmdLine[CmdLineSize - 1] == L'\0');
      UseBootNext = (StrStr (CmdLine, L"-BootNext") != NULL);
    }

    DEBUG ((DEBUG_INFO | DEBUG_INIT, "%a v%a\r\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

    Status = InitUnitTestFramework (
               &Framework,
               UNIT_TEST_NAME,
               gEfiCallerBaseName,
               UNIT_TEST_VERSION
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: %a: InitUnitTestFramework failed."
        " Status = %r\r\n",
        __FUNCTION__,
        Status
        ));
      return Status;
    }

    Status = InitTestSuite (Framework, UseBootNext);
    if (!EFI_ERROR (Status)) {
      Status = RunAllTestSuites (Framework);
    }

    FreeUnitTestFramework (Framework);
    return Status;
  }
}
