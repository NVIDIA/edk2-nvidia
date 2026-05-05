/** @file

  Host-based unit tests for BootChainInfoLib.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <PiDxe.h>
#include <HostBasedTestStubLib/HobStubLib.h>
#include <HostBasedTestStubLib/PcdStubLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>

#define UNIT_TEST_NAME     "BootChainInfoLib Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

typedef struct {
  EFI_HOB_GUID_TYPE               Hob;
  TEGRA_PLATFORM_RESOURCE_INFO    Data;
} TEST_PLATFORM_RESOURCE_HOB;

STATIC TEST_PLATFORM_RESOURCE_HOB  mResourceHob;
STATIC TEGRA_PLATFORM_TYPE         mPlatform;
STATIC UINT32                      mChipId;
STATIC BOOLEAN                     mPcdGptIsPerBootChain;
STATIC BOOLEAN                     mPcdPartitionNamesHaveSuffixes;

TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  )
{
  return mPlatform;
}

UINT32
TegraGetChipID (
  VOID
  )
{
  return mChipId;
}

STATIC
VOID
InitPlatformResourceHob (
  IN UINT32  ActiveBootChain
  )
{
  ZeroMem (&mResourceHob, sizeof (mResourceHob));
  mResourceHob.Hob.Header.HobType   = EFI_HOB_TYPE_GUID_EXTENSION;
  mResourceHob.Hob.Header.HobLength = sizeof (mResourceHob);
  mResourceHob.Hob.Name             = gNVIDIAPlatformResourceDataGuid;
  mResourceHob.Data.ActiveBootChain = ActiveBootChain;
}

STATIC
VOID
ExpectPartitionName (
  IN CONST CHAR16  *ActualName,
  IN CONST CHAR16  *ExpectedName
  )
{
  assert_int_equal (StrCmp (ActualName, ExpectedName), 0);
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ZeroMem (&mResourceHob, sizeof (mResourceHob));
  mPlatform                      = TEGRA_PLATFORM_SILICON;
  mChipId                        = T264_CHIP_ID;
  mPcdGptIsPerBootChain          = FALSE;
  mPcdPartitionNamesHaveSuffixes = FALSE;

  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
TestTeardown (
  IN UNIT_TEST_CONTEXT  Context
  )
{
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetBootChainPartitionNameRejectsBadParameters (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  PartitionName[MAX_PARTITION_NAME_LEN];

  UT_ASSERT_STATUS_EQUAL (
    GetBootChainPartitionName (NULL, BOOT_CHAIN_A, PartitionName),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_STATUS_EQUAL (
    GetBootChainPartitionName (L"kernel", BOOT_CHAIN_A, NULL),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_STATUS_EQUAL (
    GetBootChainPartitionName (L"kernel", BOOT_CHAIN_COUNT, PartitionName),
    EFI_INVALID_PARAMETER
    );

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestNonSiliconCopiesBasePartitionName (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  PartitionName[MAX_PARTITION_NAME_LEN];

  mPlatform = TEGRA_PLATFORM_VDK;
  ZeroMem (PartitionName, sizeof (PartitionName));

  UT_ASSERT_NOT_EFI_ERROR (
    GetBootChainPartitionName (L"kernel", BOOT_CHAIN_B, PartitionName)
    );
  ExpectPartitionName (PartitionName, L"kernel");

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSiliconUsesSuffixPartitionNames (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  PartitionName[MAX_PARTITION_NAME_LEN];

  mPcdPartitionNamesHaveSuffixes = TRUE;
  ZeroMem (PartitionName, sizeof (PartitionName));

  UT_ASSERT_NOT_EFI_ERROR (
    GetBootChainPartitionName (L"kernel", BOOT_CHAIN_B, PartitionName)
    );
  ExpectPartitionName (PartitionName, L"kernel_b");

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSiliconUsesPrefixPartitionNames (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  PartitionName[MAX_PARTITION_NAME_LEN];

  mPcdPartitionNamesHaveSuffixes = FALSE;
  mChipId                        = T234_CHIP_ID;
  ZeroMem (PartitionName, sizeof (PartitionName));

  UT_ASSERT_NOT_EFI_ERROR (
    GetBootChainPartitionName (L"kernel", BOOT_CHAIN_B, PartitionName)
    );
  ExpectPartitionName (PartitionName, L"B_kernel");

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSiliconRejectsUnsupportedPrefixChip (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  PartitionName[MAX_PARTITION_NAME_LEN];

  mPcdPartitionNamesHaveSuffixes = FALSE;
  mChipId                        = 0x41;
  ZeroMem (PartitionName, sizeof (PartitionName));

  UT_ASSERT_STATUS_EQUAL (
    GetBootChainPartitionName (L"kernel", BOOT_CHAIN_A, PartitionName),
    EFI_UNSUPPORTED
    );

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetActivePartitionNameUsesBootChainHob (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  PartitionName[MAX_PARTITION_NAME_LEN];

  InitPlatformResourceHob (BOOT_CHAIN_B);
  MockGetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid, &mResourceHob.Hob);
  mPcdPartitionNamesHaveSuffixes = TRUE;
  ZeroMem (PartitionName, sizeof (PartitionName));

  UT_ASSERT_NOT_EFI_ERROR (GetActivePartitionName (L"boot", PartitionName));
  ExpectPartitionName (PartitionName, L"boot_b");

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetActivePartitionNameCopiesBaseWhenHobMissing (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  PartitionName[MAX_PARTITION_NAME_LEN];

  MockGetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid, NULL);
  ZeroMem (PartitionName, sizeof (PartitionName));

  UT_ASSERT_NOT_EFI_ERROR (GetActivePartitionName (L"boot", PartitionName));
  ExpectPartitionName (PartitionName, L"boot");

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetPartitionBaseNameAndBootChainAny (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  BaseName[MAX_PARTITION_NAME_LEN];
  UINTN   BootChain;

  ZeroMem (BaseName, sizeof (BaseName));
  BootChain = MAX_UINTN;
  UT_ASSERT_NOT_EFI_ERROR (
    GetPartitionBaseNameAndBootChainAny (L"A_kernel", BaseName, &BootChain)
    );
  ExpectPartitionName (BaseName, L"kernel");
  UT_ASSERT_EQUAL (BootChain, BOOT_CHAIN_A);

  ZeroMem (BaseName, sizeof (BaseName));
  BootChain = MAX_UINTN;
  UT_ASSERT_NOT_EFI_ERROR (
    GetPartitionBaseNameAndBootChainAny (L"kernel_b", BaseName, &BootChain)
    );
  ExpectPartitionName (BaseName, L"kernel");
  UT_ASSERT_EQUAL (BootChain, BOOT_CHAIN_B);

  ZeroMem (BaseName, sizeof (BaseName));
  BootChain = MAX_UINTN;
  UT_ASSERT_NOT_EFI_ERROR (
    GetPartitionBaseNameAndBootChainAny (L"kernel", BaseName, &BootChain)
    );
  ExpectPartitionName (BaseName, L"kernel");
  UT_ASSERT_EQUAL (BootChain, BOOT_CHAIN_A);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetPartitionBaseNameAndBootChainAnyRejectsBadParameters (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16  BaseName[MAX_PARTITION_NAME_LEN];
  UINTN   BootChain;

  UT_ASSERT_STATUS_EQUAL (
    GetPartitionBaseNameAndBootChainAny (NULL, BaseName, &BootChain),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_STATUS_EQUAL (
    GetPartitionBaseNameAndBootChainAny (L"kernel", NULL, &BootChain),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_STATUS_EQUAL (
    GetPartitionBaseNameAndBootChainAny (L"kernel", BaseName, NULL),
    EFI_INVALID_PARAMETER
    );

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetBootChainForGptUsesDefaultWhenPcdDisabled (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mPcdGptIsPerBootChain = FALSE;

  UT_ASSERT_EQUAL (GetBootChainForGpt (), BOOT_CHAIN_A);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetBootChainForGptReadsActiveBootChainHob (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  InitPlatformResourceHob (BOOT_CHAIN_B);
  mPcdGptIsPerBootChain = TRUE;
  MockGetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid, &mResourceHob.Hob);

  UT_ASSERT_EQUAL (GetBootChainForGpt (), BOOT_CHAIN_B);

  return UNIT_TEST_PASSED;
}

STATIC
BOOLEAN
UnitTestPcdGetBool_PcdGptIsPerBootChain (
  VOID
  )
{
  return mPcdGptIsPerBootChain;
}

STATIC
BOOLEAN
UnitTestPcdGetBool_PcdPartitionNamesHaveSuffixes (
  VOID
  )
{
  return mPcdPartitionNamesHaveSuffixes;
}

#undef PcdGetBool
#define PcdGetBool(TokenName)  UnitTestPcdGetBool_##TokenName ()

#include "../BootChainInfoLib.c"

#undef PcdGetBool

EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      Suite;

  Framework = NULL;
  Suite     = NULL;

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CreateUnitTestSuite (&Suite, Framework, "Boot chain helpers", "BootChainInfoLib", NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "Reject invalid partition-name parameters", "RejectBadPartitionNameParams", TestGetBootChainPartitionNameRejectsBadParameters, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Copy base name for non-silicon platform", "NonSiliconBaseName", TestNonSiliconCopiesBasePartitionName, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Append suffix for silicon partition names", "SiliconSuffixNames", TestSiliconUsesSuffixPartitionNames, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Prepend prefix for silicon partition names", "SiliconPrefixNames", TestSiliconUsesPrefixPartitionNames, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Reject unsupported prefix chip", "UnsupportedPrefixChip", TestSiliconRejectsUnsupportedPrefixChip, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Use HOB active boot chain for partition name", "ActivePartitionFromHob", TestGetActivePartitionNameUsesBootChainHob, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Copy base name when boot-chain HOB is missing", "ActivePartitionNoHob", TestGetActivePartitionNameCopiesBaseWhenHobMissing, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Parse prefixed, suffixed, and base names", "ParseAnyBootChainName", TestGetPartitionBaseNameAndBootChainAny, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Reject invalid partition parser parameters", "RejectBadParserParams", TestGetPartitionBaseNameAndBootChainAnyRejectsBadParameters, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Use GPT chain A when per-chain GPT is disabled", "GptDefaultChain", TestGetBootChainForGptUsesDefaultWhenPcdDisabled, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Read GPT boot chain from resource HOB", "GptChainFromHob", TestGetBootChainForGptReadsActiveBootChainHob, TestSetup, TestTeardown, NULL);

  Status = RunAllTestSuites (Framework);

Done:
  if (Framework != NULL) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

int
main (
  void
  )
{
  return (int)UefiTestMain ();
}
