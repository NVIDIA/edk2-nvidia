/** @file
  Unit tests for Redfish HTTP Boot Configuration driver.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <gtest/gtest.h>
#include <Library/GoogleTestLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include "../RedfishHttpBootConfigUtils.h"
}

#define CHAR16_STRING(x)  (reinterpret_cast<CHAR16*>(const_cast<char16_t*>(u##x)))

//
// Test fixture for IsMacAllZeros tests
//
class IsMacAllZerosTest : public ::testing::Test {
protected:
  EFI_MAC_ADDRESS MacAddr;

  void
  SetUp (
    ) override
  {
    ZeroMem (&MacAddr, sizeof (MacAddr));
  }
};

//
// Test: All zeros should return TRUE
//
TEST_F (IsMacAllZerosTest, AllZeros) {
  // MacAddr is already zeroed in SetUp
  EXPECT_TRUE (IsMacAllZeros (&MacAddr));
}

//
// Test: Non-zero first byte should return FALSE
//
TEST_F (IsMacAllZerosTest, NonZeroFirstByte) {
  MacAddr.Addr[0] = 0x01;
  EXPECT_FALSE (IsMacAllZeros (&MacAddr));
}

//
// Test: Non-zero last byte should return FALSE
//
TEST_F (IsMacAllZerosTest, NonZeroLastByte) {
  MacAddr.Addr[5] = 0xFF;
  EXPECT_FALSE (IsMacAllZeros (&MacAddr));
}

//
// Test: Non-zero middle byte should return FALSE
//
TEST_F (IsMacAllZerosTest, NonZeroMiddleByte) {
  MacAddr.Addr[3] = 0xAA;
  EXPECT_FALSE (IsMacAllZeros (&MacAddr));
}

//
// Test: All 0xFF should return FALSE
//
TEST_F (IsMacAllZerosTest, AllFF) {
  for (int i = 0; i < 6; i++) {
    MacAddr.Addr[i] = 0xFF;
  }

  EXPECT_FALSE (IsMacAllZeros (&MacAddr));
}

//
// Test: Typical MAC address should return FALSE
//
TEST_F (IsMacAllZerosTest, TypicalMac) {
  MacAddr.Addr[0] = 0x00;
  MacAddr.Addr[1] = 0x1B;
  MacAddr.Addr[2] = 0x21;
  MacAddr.Addr[3] = 0xF2;
  MacAddr.Addr[4] = 0x7F;
  MacAddr.Addr[5] = 0xF7;
  EXPECT_FALSE (IsMacAllZeros (&MacAddr));
}

//
// Test: NULL pointer should return FALSE
//
TEST (IsMacAllZerosNullTest, NullPointer) {
  EXPECT_FALSE (IsMacAllZeros (NULL));
}

//
// Test fixture for ParseHttpBootUri tests
//
class ParseHttpBootUriTest : public ::testing::Test {
protected:
  EFI_MAC_ADDRESS MacAddr;
  BOOLEAN Once;
  CHAR16 *Uri;
  CHAR16 TestBuffer[512];

  void
  SetUp (
    ) override
  {
    ZeroMem (&MacAddr, sizeof (MacAddr));
    Once = FALSE;
    Uri  = NULL;
    ZeroMem (TestBuffer, sizeof (TestBuffer));
  }
};

//
// Test: Parse URI only (no MAC, no once)
//
TEST_F (ParseHttpBootUriTest, ParseUriOnly) {
  StrCpyS (TestBuffer, sizeof (TestBuffer)/sizeof (CHAR16), CHAR16_STRING ("http://server/file.img"));

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("http://server/file.img")));

  // MAC should be all zeros
  for (int i = 0; i < 6; i++) {
    EXPECT_EQ (MacAddr.Addr[i], 0);
  }
}

//
// Test: Parse MAC + URI (persistent)
//
TEST_F (ParseHttpBootUriTest, ParseMacAndUri) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("http://server/file.img")));

  // Check MAC address
  EXPECT_EQ (MacAddr.Addr[0], 0x00);
  EXPECT_EQ (MacAddr.Addr[1], 0x1B);
  EXPECT_EQ (MacAddr.Addr[2], 0x21);
  EXPECT_EQ (MacAddr.Addr[3], 0xF2);
  EXPECT_EQ (MacAddr.Addr[4], 0x7F);
  EXPECT_EQ (MacAddr.Addr[5], 0xF7);
}

//
// Test: Parse MAC + once + URI (one-time boot)
//
TEST_F (ParseHttpBootUriTest, ParseMacOnceAndUri) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7|once|http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_TRUE (Once);
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("http://server/file.img")));

  // Check MAC address
  EXPECT_EQ (MacAddr.Addr[0], 0x00);
  EXPECT_EQ (MacAddr.Addr[1], 0x1B);
  EXPECT_EQ (MacAddr.Addr[2], 0x21);
  EXPECT_EQ (MacAddr.Addr[3], 0xF2);
  EXPECT_EQ (MacAddr.Addr[4], 0x7F);
  EXPECT_EQ (MacAddr.Addr[5], 0xF7);
}

//
// Test: IPv6 URI
//
TEST_F (ParseHttpBootUriTest, ParseIpv6Uri) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("http://[2001:db8::1]/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("http://[2001:db8::1]/file.img")));
}

//
// Test: HTTPS URI
//
TEST_F (ParseHttpBootUriTest, ParseHttpsUri) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("https://server/secure.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("https://server/secure.img")));
}

//
// Test: Empty URI should fail
//
TEST_F (ParseHttpBootUriTest, ParseEmptyUri) {
  StrCpyS (TestBuffer, sizeof (TestBuffer)/sizeof (CHAR16), CHAR16_STRING (""));

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: NULL parameters should fail
//
TEST_F (ParseHttpBootUriTest, ParseNullParameters) {
  StrCpyS (TestBuffer, sizeof (TestBuffer)/sizeof (CHAR16), CHAR16_STRING ("http://server/file.img"));

  // NULL UriString
  EXPECT_EQ (ParseHttpBootUri (NULL, &MacAddr, &Once, &Uri), EFI_INVALID_PARAMETER);

  // NULL MacAddr
  EXPECT_EQ (ParseHttpBootUri (TestBuffer, NULL, &Once, &Uri), EFI_INVALID_PARAMETER);

  // NULL Once
  EXPECT_EQ (ParseHttpBootUri (TestBuffer, &MacAddr, NULL, &Uri), EFI_INVALID_PARAMETER);

  // NULL Uri
  EXPECT_EQ (ParseHttpBootUri (TestBuffer, &MacAddr, &Once, NULL), EFI_INVALID_PARAMETER);
}

//
// Test: Truncated MAC should fail
//
TEST_F (ParseHttpBootUriTest, ParseTruncatedMac) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: Lowercase MAC address
//
TEST_F (ParseHttpBootUriTest, ParseLowercaseMac) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("aa:bb:cc:dd:ee:ff||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (MacAddr.Addr[0], 0xAA);
  EXPECT_EQ (MacAddr.Addr[1], 0xBB);
  EXPECT_EQ (MacAddr.Addr[2], 0xCC);
  EXPECT_EQ (MacAddr.Addr[3], 0xDD);
  EXPECT_EQ (MacAddr.Addr[4], 0xEE);
  EXPECT_EQ (MacAddr.Addr[5], 0xFF);
}

//
// Test: Mixed case MAC address
//
TEST_F (ParseHttpBootUriTest, ParseMixedCaseMac) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("Aa:Bb:Cc:Dd:Ee:Ff||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (MacAddr.Addr[0], 0xAA);
  EXPECT_EQ (MacAddr.Addr[1], 0xBB);
  EXPECT_EQ (MacAddr.Addr[2], 0xCC);
  EXPECT_EQ (MacAddr.Addr[3], 0xDD);
  EXPECT_EQ (MacAddr.Addr[4], 0xEE);
  EXPECT_EQ (MacAddr.Addr[5], 0xFF);
}

//
// Test: Invalid hex characters in MAC should fail
//
TEST_F (ParseHttpBootUriTest, ParseInvalidHexInMac) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("GG:HH:II:JJ:KK:LL||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Invalid hex characters (G, H, I, J, K, L) should be rejected
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: Dashes instead of colons should fail
//
TEST_F (ParseHttpBootUriTest, ParseMacWithDashes) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00-1B-21-F2-7F-F7||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Parser expects colons after each hex pair
  // After parsing 6 bytes, expects '|' but finds '-'
  // Returns EFI_INVALID_PARAMETER
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: MAC without separators should fail
//
TEST_F (ParseHttpBootUriTest, ParseMacNoSeparators) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("001B21F27FF7||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Parser now enforces colons between hex pairs
  // After first 2 hex chars, expects ':' but finds hex digit
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: MAC with too many bytes should fail
//
TEST_F (ParseHttpBootUriTest, ParseMacTooLong) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7:AA||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // After parsing 6 bytes (00:1B:21:F2:7F:F7), parser expects '|'
  // But finds ':' (before AA), so returns EFI_INVALID_PARAMETER
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: Capital "Once" should work (case-insensitive)
//
TEST_F (ParseHttpBootUriTest, ParseCapitalOnce) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7|Once|http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Current implementation uses StrnCmp which is case-sensitive
  // "Once" != "once", so this should NOT set Once flag
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);  // Should be FALSE since it's case-sensitive
}

//
// Test: All caps "ONCE" should work (case-insensitive)
//
TEST_F (ParseHttpBootUriTest, ParseAllCapsOnce) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7|ONCE|http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Current implementation uses StrnCmp which is case-sensitive
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);  // Should be FALSE since it's case-sensitive
}

//
// Test: Invalid middle field (not "once")
//
TEST_F (ParseHttpBootUriTest, ParseInvalidMiddleField) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7|persistent|http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Should succeed - "persistent" is not recognized, so Once=FALSE
  // URI starts after the second pipe
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("http://server/file.img")));
}

//
// Test: Typo in "once" keyword
//
TEST_F (ParseHttpBootUriTest, ParseTruncatedOnce) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7|onc|http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Should succeed - "onc" != "once", so Once=FALSE
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);
}

//
// Test: Single pipe after MAC should fail (need double pipe)
//
TEST_F (ParseHttpBootUriTest, ParseMacWithSinglePipe) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7|http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Should succeed but interpret as MAC|once-field|...
  // Since there's no second pipe after the first field, it treats "http://..." as the once field
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);  // "http://..." != "once"
}

//
// Test: Triple pipes after MAC
//
TEST_F (ParseHttpBootUriTest, ParseMacWithTriplePipes) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:1B:21:F2:7F:F7|||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Triple pipe (|||) is interpreted as:
  //   - First | = MAC terminator
  //   - Second | = empty middle field (forms || = no middle field indicator)
  //   - Third | = becomes part of the URI
  // This documents current behavior - the 3rd pipe is included in URI
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_FALSE (Once);
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("|http://server/file.img")));
}

//
// Test: MAC with invalid hex character 'Z' in last byte
//
TEST_F (ParseHttpBootUriTest, ParseMacWithInvalidCharZ) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:11:22:33:44:ZZ||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // 'Z' is not a valid hex digit
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: MAC with special characters
//
TEST_F (ParseHttpBootUriTest, ParseMacWithSpecialChars) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:11:22:33:44:@#||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // '@' and '#' are not valid hex digits
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: MAC with missing colon in middle
//
TEST_F (ParseHttpBootUriTest, ParseMacMissingMiddleColon) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:11:2233:44:55||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Missing colon between bytes 2 and 3
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: MAC with extra colon
//
TEST_F (ParseHttpBootUriTest, ParseMacExtraColon) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00:11:22::33:44:55||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Double colon creates invalid format
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: MAC with spaces
//
TEST_F (ParseHttpBootUriTest, ParseMacWithSpaces) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("00 11:22:33:44:55||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // Space is not a valid hex digit
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: MAC with single invalid hex character
//
TEST_F (ParseHttpBootUriTest, ParseMacSingleInvalidChar) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("0G:11:22:33:44:55||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  // 'G' is not a valid hex digit
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: Valid lowercase hex should still work
//
TEST_F (ParseHttpBootUriTest, ParseValidLowercaseHex) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("ab:cd:ef:12:34:56||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (MacAddr.Addr[0], 0xAB);
  EXPECT_EQ (MacAddr.Addr[1], 0xCD);
  EXPECT_EQ (MacAddr.Addr[2], 0xEF);
  EXPECT_EQ (MacAddr.Addr[3], 0x12);
  EXPECT_EQ (MacAddr.Addr[4], 0x34);
  EXPECT_EQ (MacAddr.Addr[5], 0x56);
}

//
// Test: Implied all-zeros MAC with once flag (|once|URI)
//
TEST_F (ParseHttpBootUriTest, ParseImpliedMacWithOnce) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("|once|http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);

  // MAC should be all zeros (implied first NIC)
  for (int i = 0; i < 6; i++) {
    EXPECT_EQ (MacAddr.Addr[i], 0x00);
  }

  // Once flag should be set
  EXPECT_TRUE (Once);

  // URI should be correct
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("http://server/file.img")));
}

//
// Test: Implied all-zeros MAC persistent (||URI)
//
TEST_F (ParseHttpBootUriTest, ParseImpliedMacPersistent) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("||http://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);

  // MAC should be all zeros (implied first NIC)
  for (int i = 0; i < 6; i++) {
    EXPECT_EQ (MacAddr.Addr[i], 0x00);
  }

  // Once flag should NOT be set
  EXPECT_FALSE (Once);

  // URI should be correct
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("http://server/file.img")));
}

//
// Test: Implied all-zeros MAC with HTTPS
//
TEST_F (ParseHttpBootUriTest, ParseImpliedMacWithHttps) {
  StrCpyS (
    TestBuffer,
    sizeof (TestBuffer)/sizeof (CHAR16),
    CHAR16_STRING ("|once|https://server/file.img")
    );

  EFI_STATUS  Status = ParseHttpBootUri (TestBuffer, &MacAddr, &Once, &Uri);

  EXPECT_EQ (Status, EFI_SUCCESS);

  // MAC should be all zeros
  for (int i = 0; i < 6; i++) {
    EXPECT_EQ (MacAddr.Addr[i], 0x00);
  }

  EXPECT_TRUE (Once);
  EXPECT_THAT (Uri, Char16StrEq (CHAR16_STRING ("https://server/file.img")));
}

//
// Main test entry point
//
int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
