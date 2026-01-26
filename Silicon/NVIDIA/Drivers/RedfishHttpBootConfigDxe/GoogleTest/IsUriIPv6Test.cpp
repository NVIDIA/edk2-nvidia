/** @file
  Unit tests for IsUriIPv6 function.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <gtest/gtest.h>
#include <Library/GoogleTestLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include "../RedfishHttpBootConfigUtils.h"
}

using namespace testing;

//
// Test fixture for IsUriIPv6 tests
//
class IsUriIPv6Test : public Test {
};

//
// Test: NULL input should return FALSE
//
TEST_F (IsUriIPv6Test, NullInput) {
  EXPECT_FALSE (IsUriIPv6 (NULL));
}

//
// Test: Empty string should return FALSE
//
TEST_F (IsUriIPv6Test, EmptyString) {
  EXPECT_FALSE (IsUriIPv6 (""));
}

//
// Test: IPv6 URIs with brackets should return TRUE
//
TEST_F (IsUriIPv6Test, IPv6HttpSimple) {
  EXPECT_TRUE (IsUriIPv6 ("http://[2001:db8::1]/file"));
}

TEST_F (IsUriIPv6Test, IPv6HttpsSimple) {
  EXPECT_TRUE (IsUriIPv6 ("https://[2001:db8::1]/path"));
}

TEST_F (IsUriIPv6Test, IPv6WithPort) {
  EXPECT_TRUE (IsUriIPv6 ("http://[2001:db8::1]:8080/path"));
}

TEST_F (IsUriIPv6Test, IPv6Localhost) {
  EXPECT_TRUE (IsUriIPv6 ("http://[::1]/file"));
}

TEST_F (IsUriIPv6Test, IPv6FullAddress) {
  EXPECT_TRUE (IsUriIPv6 ("http://[2001:0db8:0000:0000:0000:ff00:0042:8329]/path"));
}

TEST_F (IsUriIPv6Test, IPv6WithPortAndQuery) {
  EXPECT_TRUE (IsUriIPv6 ("https://[fe80::1]:443/path?query=value"));
}

//
// Test: IPv4 URIs without brackets should return FALSE
//
TEST_F (IsUriIPv6Test, IPv4HttpSimple) {
  EXPECT_FALSE (IsUriIPv6 ("http://192.168.1.1/file"));
}

TEST_F (IsUriIPv6Test, IPv4HttpsSimple) {
  EXPECT_FALSE (IsUriIPv6 ("https://10.0.0.1/path"));
}

TEST_F (IsUriIPv6Test, IPv4WithPort) {
  EXPECT_FALSE (IsUriIPv6 ("http://172.16.0.1:8080/path"));
}

TEST_F (IsUriIPv6Test, IPv4Localhost) {
  EXPECT_FALSE (IsUriIPv6 ("http://127.0.0.1/file"));
}

//
// Test: Domain names without brackets should return FALSE
//
TEST_F (IsUriIPv6Test, DomainName) {
  EXPECT_FALSE (IsUriIPv6 ("http://example.com/file"));
}

TEST_F (IsUriIPv6Test, DomainNameHttps) {
  EXPECT_FALSE (IsUriIPv6 ("https://www.example.com/path"));
}

TEST_F (IsUriIPv6Test, DomainNameWithPort) {
  EXPECT_FALSE (IsUriIPv6 ("http://example.com:8080/path"));
}

TEST_F (IsUriIPv6Test, DomainNameWithQuery) {
  EXPECT_FALSE (IsUriIPv6 ("https://example.com/path?query=value"));
}

//
// Test: Edge cases
//
TEST_F (IsUriIPv6Test, OnlyBracket) {
  // Contains bracket, so returns TRUE (even though malformed)
  EXPECT_TRUE (IsUriIPv6 ("["));
}

TEST_F (IsUriIPv6Test, BracketInPath) {
  // Bracket in path (not in host), but still detected
  EXPECT_TRUE (IsUriIPv6 ("http://example.com/path[test]"));
}

TEST_F (IsUriIPv6Test, ClosingBracketOnly) {
  // No opening bracket
  EXPECT_FALSE (IsUriIPv6 ("http://example.com/path]"));
}

TEST_F (IsUriIPv6Test, NoScheme) {
  // Just an IPv6 address in brackets
  EXPECT_TRUE (IsUriIPv6 ("[2001:db8::1]"));
}
