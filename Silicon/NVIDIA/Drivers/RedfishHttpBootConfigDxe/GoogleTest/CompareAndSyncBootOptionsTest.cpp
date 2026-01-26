/** @file
  Unit tests for CompareAndSyncBootOptions function.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <gtest/gtest.h>
#include <Library/GoogleTestLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/DebugLib.h>
  #include <Guid/GlobalVariable.h>
  #include "CompareAndSyncBootOptionsStub.h"

  // Function under test
  EFI_STATUS
  CompareAndSyncBootOptions (
    VOID
    );
}

using namespace testing;

#define CHAR16_STRING(x)  (reinterpret_cast<CHAR16*>(const_cast<char16_t*>(u##x)))

//
// Test fixture for CompareAndSyncBootOptions tests
//
class CompareAndSyncBootOptionsTest : public Test {
protected:
  CHAR16 UriBuffer[512];

  void
  SetUp (
    ) override
  {
    SetupMocks ();
    ZeroMem (UriBuffer, sizeof (UriBuffer));
  }

  void
  TearDown (
    ) override
  {
    TeardownMocks ();
  }
};

//
// Test: HttpBootUri not found, Boot8C7D doesn't exist
//
TEST_F (CompareAndSyncBootOptionsTest, HttpBootUri_NotFound_BootOption_NotFound) {
  // Setup: HttpBootUri doesn't exist
  SetMockGetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_NOT_FOUND, NULL, 0);

  // Setup: Boot8C7D deletion returns NOT_FOUND (doesn't exist)
  SetMockSetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_NOT_FOUND);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (WasGetVariableCalled (CHAR16_STRING ("HttpBootUri")));
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
}

//
// Test: HttpBootUri not found, Boot8C7D exists and is deleted
//
TEST_F (CompareAndSyncBootOptionsTest, HttpBootUri_NotFound_BootOption_Exists) {
  // Setup: HttpBootUri doesn't exist
  SetMockGetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_NOT_FOUND, NULL, 0);

  // Setup: BootNext points to our boot option (should be cleared)
  UINT16  BootNextValue = 0x8C7D;

  SetMockGetVariableResult (
    CHAR16_STRING ("BootNext"),
    EFI_SUCCESS,
    &BootNextValue,
    sizeof (BootNextValue)
    );

  // Setup: Boot8C7D deletion succeeds (existed)
  SetMockSetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_SUCCESS);

  // Setup: BootNext deletion succeeds
  SetMockSetVariableResult (CHAR16_STRING ("BootNext"), EFI_SUCCESS);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("BootNext"), 0));
}

//
// Test: HttpBootUri not found, deletion fails
//
TEST_F (CompareAndSyncBootOptionsTest, HttpBootUri_NotFound_DeleteFails) {
  // Setup: HttpBootUri doesn't exist
  SetMockGetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_NOT_FOUND, NULL, 0);

  // Setup: Boot8C7D deletion fails
  SetMockSetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_ACCESS_DENIED);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  // Function still returns SUCCESS (error is logged but not fatal)
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
}

//
// Test: HttpBootUri read fails with access denied
//
TEST_F (CompareAndSyncBootOptionsTest, HttpBootUri_ReadFails_AccessDenied) {
  // Setup: HttpBootUri read fails
  SetMockGetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_ACCESS_DENIED, NULL, 0);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_ACCESS_DENIED);
  EXPECT_TRUE (WasGetVariableCalled (CHAR16_STRING ("HttpBootUri")));
}

//
// Test: HttpBootUri read fails with buffer too small
//
TEST_F (CompareAndSyncBootOptionsTest, HttpBootUri_ReadFails_BufferTooSmall) {
  // Setup: HttpBootUri read fails with BUFFER_TOO_SMALL
  SetMockGetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_BUFFER_TOO_SMALL, NULL, 1000);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_BUFFER_TOO_SMALL);
}

//
// Test: ParseHttpBootUri fails
//
TEST_F (CompareAndSyncBootOptionsTest, ParseHttpBootUri_Fails) {
  // Setup: HttpBootUri exists
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("invalid::format"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse fails
  SetMockParseResult (EFI_INVALID_PARAMETER, NULL, FALSE, NULL);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: Persistent boot, boot option missing, create succeeds
//
TEST_F (CompareAndSyncBootOptionsTest, Persistent_BootOption_Missing_CreateSucceeds) {
  // Setup: HttpBootUri exists (persistent - no "once")
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("http://192.168.1.1/boot.img"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, no MAC, no "once"
  EFI_MAC_ADDRESS  ZeroMac;

  ZeroMem (&ZeroMac, sizeof (ZeroMac));
  SetMockParseResult (EFI_SUCCESS, &ZeroMac, FALSE, UriBuffer);

  // Setup: Boot8C7D doesn't exist
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_NOT_FOUND, NULL, 0);

  // Setup: CreateHttpBootOption succeeds
  SetMockCreateBootOptionResult (EFI_SUCCESS, 0x8C7D);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
}

//
// Test: Persistent boot, boot option missing, create fails
//
TEST_F (CompareAndSyncBootOptionsTest, Persistent_BootOption_Missing_CreateFails) {
  // Setup: HttpBootUri exists (persistent)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF||http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, no "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, FALSE, UriPtr);

  // Setup: Boot8C7D doesn't exist
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_NOT_FOUND, NULL, 0);

  // Setup: CreateHttpBootOption fails (NIC not found)
  SetMockCreateBootOptionResult (EFI_NOT_FOUND, 0);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

//
// Test: One-time boot, boot option missing, create succeeds
//
TEST_F (CompareAndSyncBootOptionsTest, OneTime_BootOption_Missing_CreateSucceeds) {
  // Setup: HttpBootUri exists (one-time)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF|once|http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, TRUE, UriPtr);

  // Setup: Boot8C7D doesn't exist
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_NOT_FOUND, NULL, 0);

  // Setup: CreateHttpBootOption succeeds
  SetMockCreateBootOptionResult (EFI_SUCCESS, 0x8C7D);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
}

//
// Test: One-time boot, BootNext still set, no cleanup
//
TEST_F (CompareAndSyncBootOptionsTest, OneTime_BootNext_StillSet_NoCleanup) {
  // Setup: HttpBootUri exists (one-time)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF|once|http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, TRUE, UriPtr);

  // Setup: Boot8C7D exists
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_BUFFER_TOO_SMALL, NULL, 100);

  // Setup: BootNext is set to 0x8C7D
  UINT16  BootNextValue = 0x8C7D;

  SetMockGetVariableResult (
    CHAR16_STRING ("BootNext"),
    EFI_SUCCESS,
    &BootNextValue,
    sizeof (BootNextValue)
    );

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
  // No cleanup should happen
  EXPECT_FALSE (WasSetVariableCalled (CHAR16_STRING ("HttpBootUri"), 0));
  EXPECT_FALSE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
}

//
// Test: One-time boot, BootNext cleared, cleanup succeeds
//
TEST_F (CompareAndSyncBootOptionsTest, OneTime_BootNext_Cleared_CleanupSucceeds) {
  // Setup: HttpBootUri exists (one-time)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF|once|http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, TRUE, UriPtr);

  // Setup: Boot8C7D exists
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_BUFFER_TOO_SMALL, NULL, 100);

  // Setup: BootNext is cleared (NOT_FOUND)
  SetMockGetVariableResult (CHAR16_STRING ("BootNext"), EFI_NOT_FOUND, NULL, 0);

  // Setup: Cleanup succeeds
  SetMockSetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_SUCCESS);
  SetMockSetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_SUCCESS);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("HttpBootUri"), 0));
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
  // BootNext was NOT_FOUND, so it should not be cleared
  EXPECT_FALSE (WasSetVariableCalled (CHAR16_STRING ("BootNext"), 0));
}

//
// Test: One-time boot, BootNext different value, cleanup succeeds
//
TEST_F (CompareAndSyncBootOptionsTest, OneTime_BootNext_DifferentValue_CleanupSucceeds) {
  // Setup: HttpBootUri exists (one-time)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF|once|http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, TRUE, UriPtr);

  // Setup: Boot8C7D exists
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_BUFFER_TOO_SMALL, NULL, 100);

  // Setup: BootNext is set to different value
  UINT16  BootNextValue = 0x0001;

  SetMockGetVariableResult (
    CHAR16_STRING ("BootNext"),
    EFI_SUCCESS,
    &BootNextValue,
    sizeof (BootNextValue)
    );

  // Setup: Cleanup succeeds
  SetMockSetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_SUCCESS);
  SetMockSetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_SUCCESS);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("HttpBootUri"), 0));
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
  // BootNext was set to different value (0x0001), so it should not be cleared
  EXPECT_FALSE (WasSetVariableCalled (CHAR16_STRING ("BootNext"), 0));
}

//
// Test: One-time boot, BootNext read fails, cleanup succeeds
//
TEST_F (CompareAndSyncBootOptionsTest, OneTime_BootNext_ReadFails_CleanupSucceeds) {
  // Setup: HttpBootUri exists (one-time)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF|once|http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, TRUE, UriPtr);

  // Setup: Boot8C7D exists
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_BUFFER_TOO_SMALL, NULL, 100);

  // Setup: BootNext read fails (treated as "not set to RedfishBootNum")
  SetMockGetVariableResult (CHAR16_STRING ("BootNext"), EFI_ACCESS_DENIED, NULL, 0);

  // Setup: Cleanup succeeds
  SetMockSetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_SUCCESS);
  SetMockSetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_SUCCESS);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("HttpBootUri"), 0));
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
}

//
// Test: One-time boot, variable delete fails, continues with boot option delete
//
TEST_F (CompareAndSyncBootOptionsTest, OneTime_VariableDeleteFails_ContinuesWithBootOption) {
  // Setup: HttpBootUri exists (one-time)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF|once|http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, TRUE, UriPtr);

  // Setup: Boot8C7D exists
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_BUFFER_TOO_SMALL, NULL, 100);

  // Setup: BootNext cleared
  SetMockGetVariableResult (CHAR16_STRING ("BootNext"), EFI_NOT_FOUND, NULL, 0);

  // Setup: HttpBootUri deletion fails, Boot8C7D deletion succeeds
  SetMockSetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_ACCESS_DENIED);
  SetMockSetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_SUCCESS);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("HttpBootUri"), 0));
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
}

//
// Test: One-time boot, boot option delete fails, returns success
//
TEST_F (CompareAndSyncBootOptionsTest, OneTime_BootOptionDeleteFails_ReturnsSuccess) {
  // Setup: HttpBootUri exists (one-time)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF|once|http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, TRUE, UriPtr);

  // Setup: Boot8C7D exists
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_BUFFER_TOO_SMALL, NULL, 100);

  // Setup: BootNext cleared
  SetMockGetVariableResult (CHAR16_STRING ("BootNext"), EFI_NOT_FOUND, NULL, 0);

  // Setup: HttpBootUri deletion succeeds, Boot8C7D deletion fails
  SetMockSetVariableResult (CHAR16_STRING ("HttpBootUri"), EFI_SUCCESS);
  SetMockSetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_ACCESS_DENIED);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  // Function returns SUCCESS (error is logged but not fatal)
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("HttpBootUri"), 0));
  EXPECT_TRUE (WasSetVariableCalled (CHAR16_STRING ("Boot8C7D"), 0));
}

//
// Test: Persistent boot, boot option exists, recreates and sets BootNext
//
TEST_F (CompareAndSyncBootOptionsTest, Persistent_BootOption_Exists_RecreatesBootOption) {
  // Setup: HttpBootUri exists (persistent - no "once")
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("http://192.168.1.1/boot.img"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, no MAC, no "once"
  EFI_MAC_ADDRESS  ZeroMac;

  ZeroMem (&ZeroMac, sizeof (ZeroMac));
  SetMockParseResult (EFI_SUCCESS, &ZeroMac, FALSE, UriBuffer);

  // Setup: Boot8C7D exists
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_BUFFER_TOO_SMALL, NULL, 100);

  // Setup: CreateHttpBootOption succeeds
  SetMockCreateBootOptionResult (EFI_SUCCESS, 0x8C7D);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);

  // Verify CreateHttpBootOption was called with correct parameters
  EFI_MAC_ADDRESS  CalledMacAddr;
  CHAR16           *CalledUri;

  EXPECT_TRUE (WasCreateHttpBootOptionCalled (&CalledMacAddr, &CalledUri));

  // Verify MAC address is all zeros (first NIC mode)
  EFI_MAC_ADDRESS  ExpectedMac;

  ZeroMem (&ExpectedMac, sizeof (ExpectedMac));
  EXPECT_EQ (CompareMem (&CalledMacAddr, &ExpectedMac, sizeof (EFI_MAC_ADDRESS)), 0);

  // Verify URI matches
  EXPECT_THAT (CalledUri, Char16StrEq (CHAR16_STRING ("http://192.168.1.1/boot.img")));
}

//
// Test: Persistent boot with MAC, boot option exists, recreates and sets BootNext
//
TEST_F (CompareAndSyncBootOptionsTest, Persistent_WithMac_BootOption_Exists_RecreatesBootOption) {
  // Setup: HttpBootUri exists (persistent with MAC)
  StrCpyS (UriBuffer, 512, CHAR16_STRING ("AA:BB:CC:DD:EE:FF||http://example.com/file"));
  SetMockGetVariableResult (
    CHAR16_STRING ("HttpBootUri"),
    EFI_SUCCESS,
    UriBuffer,
    StrSize (UriBuffer)
    );

  // Setup: Parse succeeds, MAC, no "once"
  EFI_MAC_ADDRESS  Mac = {
    { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
  };
  CHAR16           *UriPtr = CHAR16_STRING ("http://example.com/file");

  SetMockParseResult (EFI_SUCCESS, &Mac, FALSE, UriPtr);

  // Setup: Boot8C7D exists
  SetMockGetVariableResult (CHAR16_STRING ("Boot8C7D"), EFI_BUFFER_TOO_SMALL, NULL, 100);

  // Setup: CreateHttpBootOption succeeds
  SetMockCreateBootOptionResult (EFI_SUCCESS, 0x8C7D);

  EFI_STATUS  Status = CompareAndSyncBootOptions ();

  EXPECT_EQ (Status, EFI_SUCCESS);

  // Verify CreateHttpBootOption was called with correct parameters
  EFI_MAC_ADDRESS  CalledMacAddr;
  CHAR16           *CalledUri;

  EXPECT_TRUE (WasCreateHttpBootOptionCalled (&CalledMacAddr, &CalledUri));

  // Verify MAC address matches
  EXPECT_EQ (CalledMacAddr.Addr[0], 0xAA);
  EXPECT_EQ (CalledMacAddr.Addr[1], 0xBB);
  EXPECT_EQ (CalledMacAddr.Addr[2], 0xCC);
  EXPECT_EQ (CalledMacAddr.Addr[3], 0xDD);
  EXPECT_EQ (CalledMacAddr.Addr[4], 0xEE);
  EXPECT_EQ (CalledMacAddr.Addr[5], 0xFF);

  // Verify URI matches
  EXPECT_THAT (CalledUri, Char16StrEq (CHAR16_STRING ("http://example.com/file")));
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
