/** @file
  Unit tests for the implementation of DeviceTreeHelperLib.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/GoogleTestLib.h>
#include <string.h>
#include <endian.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/WildcardStringLib.h>
}

using namespace testing;

//////////////////////////////////////////////////////////////////////////////
class WildcardString : public testing::TestWithParam<tuple<const char *, const char *, bool> > {
};

TEST_P (WildcardString, WildcardStringMatchAsciiTest) {
  const char  *wildcard    = std::get<0>(GetParam ());
  const char  *checkString = std::get<1>(GetParam ());
  bool        expected     = std::get<2>(GetParam ());

  bool  actualResult = WildcardStringMatchAscii (wildcard, checkString);

  EXPECT_EQ (expected, actualResult);
}

INSTANTIATE_TEST_SUITE_P (
  StringValues,
  WildcardString,
  testing::Values (
             std::make_tuple ("hello*", "hello world", true),
             std::make_tuple ("test?", "testing", false),
             std::make_tuple ("*world", "hello world", true),
             std::make_tuple ("abc*", "abcdefg", true),
             std::make_tuple ("123*", "1234", true),
             std::make_tuple ("t*st", "test", true),
             std::make_tuple ("h*llo", "hello", true),
             std::make_tuple ("abc*", "abc", true),
             std::make_tuple ("123*", "123", true),
             std::make_tuple ("no*", "yes", false),
             std::make_tuple ("te*t", "text", true),
             std::make_tuple ("h*llo", "hullo", true),
             std::make_tuple ("abc*", "ab", false),
             std::make_tuple ("123*", "12", false),
             std::make_tuple ("wild*", "wildcard", true),
             std::make_tuple ("t*st", "toast", true),
             std::make_tuple ("abc*", "abcd", true),
             std::make_tuple ("123*", "1235", true),
             std::make_tuple ("no*", "no", true),
             std::make_tuple ("te*t", "tent", true),
             std::make_tuple ("h*llo", "hallo", true),
             std::make_tuple ("abc*", "abcde", true),
             std::make_tuple ("123*", "1236", true),
             std::make_tuple ("wild*", "wilderness", true),
             std::make_tuple ("device1", "device10", false),
             std::make_tuple ("", "", true),                         // Empty strings should match
             std::make_tuple ("*", "", true),                        // Wildcard should match empty string
             std::make_tuple ("*", "hello", true),                   // Wildcard should match any non-empty string
             std::make_tuple ("hello", "hello", true),               // Exact match should succeed
             std::make_tuple ("hello", "hell", false),               // Exact match should fail
             std::make_tuple ("hello*", "hello", true),              // Wildcard should match partial string
             std::make_tuple ("hello*", "hello world", true),        // Wildcard should match full string
             std::make_tuple ("*hello*", "world hello world", true), // Wildcard should match multiple occurrences
             std::make_tuple ("*hello*", "world world", false),      // Wildcard should fail if not found
             std::make_tuple ("*test*", "testing", true),            // Wildcard should match partial string with multiple occurrences
             std::make_tuple ("*test*", "test", true),               // Wildcard should match partial string with single occurrence
             std::make_tuple ("*te*", "text", true),                 // Wildcard should match partial string with single occurrence
             std::make_tuple ("*test*", "tent", false),              // Wildcard should fail if not found
             std::make_tuple ("*test*", "hello test world", true),   // Wildcard should match partial string with multiple occurrences
             std::make_tuple ("*test*", "hello world", false),       // Wildcard should fail if not found
             std::make_tuple ("*t*s*", "testing", true),             // Wildcard should match partial string with multiple occurrences
             std::make_tuple ("*t*s*", "test", true),                // Wildcard should match partial string with single occurrence
             std::make_tuple ("*t*s*", "text", false),               // Wildcard should not match partial string with single occurrence
             std::make_tuple ("*t*s*", "tent", false),               // Wildcard should fail if not found
             std::make_tuple ("*t*s*", "hello test world", true),    // Wildcard should match partial string with multiple occurrences
             std::make_tuple ("*t*s*", "hello world", false),        // Wildcard should fail if not found
             std::make_tuple ("cache", "arm,mpam-cache", false)      // Wildcard should fail if start does not match
             )
  );

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
