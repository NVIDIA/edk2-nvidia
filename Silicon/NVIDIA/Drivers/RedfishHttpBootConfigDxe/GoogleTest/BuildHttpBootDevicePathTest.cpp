/** @file
  Unit tests for BuildHttpBootDevicePath function.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <gtest/gtest.h>
#include <Library/GoogleTestLib.h>
#include "DevicePathLibStub.h"

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/MemoryAllocationLib.h>
  #include <Protocol/DevicePath.h>
  #include "../RedfishHttpBootConfigUtils.h"
}

using namespace testing;

#define CHAR16_STRING(x)  (reinterpret_cast<CHAR16*>(const_cast<char16_t*>(u##x)))

//
// Test fixture for BuildHttpBootDevicePath tests
//
class BuildHttpBootDevicePathTest : public Test {
protected:
  EFI_HANDLE NicHandle;
  EFI_DEVICE_PATH_PROTOCOL *ResultDevicePath;
  EFI_DEVICE_PATH_PROTOCOL BaseDevicePath;

  void
  SetUp (
    ) override
  {
    NicHandle        = (EFI_HANDLE)0x1234;
    ResultDevicePath = NULL;

    // Set up a basic device path for the NIC
    BaseDevicePath.Type    = HARDWARE_DEVICE_PATH;
    BaseDevicePath.SubType = HW_PCI_DP;
    SetDevicePathNodeLength (&BaseDevicePath, sizeof (EFI_DEVICE_PATH_PROTOCOL));

    // Default: DevicePathFromHandle returns our base device path
    SetStubDevicePath (&BaseDevicePath);

    // Default: AppendDevicePathNode uses default implementation
    ClearStubAppendDevicePathNodeResult ();
  }

  void
  TearDown (
    ) override
  {
    if (ResultDevicePath != NULL) {
      FreePool (ResultDevicePath);
      ResultDevicePath = NULL;
    }
  }
};

//
// Test: DevicePathFromHandle returns NULL
//
TEST_F (BuildHttpBootDevicePathTest, DevicePathFromHandleReturnsNull) {
  SetStubDevicePath (NULL);

  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("http://server/file"),
                         &ResultDevicePath
                         );

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

//
// Test: IPv4 URI builds device path successfully
//
TEST_F (BuildHttpBootDevicePathTest, IPv4Uri) {
  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("http://192.168.1.1/file.img"),
                         &ResultDevicePath
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_NE (ResultDevicePath, nullptr);
}

//
// Test: IPv6 URI builds device path successfully
//
TEST_F (BuildHttpBootDevicePathTest, IPv6Uri) {
  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("http://[2001:db8::1]/file.img"),
                         &ResultDevicePath
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_NE (ResultDevicePath, nullptr);
}

//
// Test: HTTPS URI
//
TEST_F (BuildHttpBootDevicePathTest, HttpsUri) {
  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("https://server/secure.img"),
                         &ResultDevicePath
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_NE (ResultDevicePath, nullptr);
}

//
// Test: Domain name URI
//
TEST_F (BuildHttpBootDevicePathTest, DomainNameUri) {
  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("http://example.com/file.img"),
                         &ResultDevicePath
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_NE (ResultDevicePath, nullptr);
}

//
// Test: AppendDevicePathNode fails (OOM for first append)
//
TEST_F (BuildHttpBootDevicePathTest, AppendDevicePathNodeFailsFirst) {
  // Make AppendDevicePathNode return NULL (simulate OOM)
  SetStubAppendDevicePathNodeResult (NULL);

  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("http://server/file"),
                         &ResultDevicePath
                         );

  // Should return EFI_OUT_OF_RESOURCES when first AppendDevicePathNode fails
  EXPECT_EQ (Status, EFI_OUT_OF_RESOURCES);
}

//
// Test: AppendDevicePathNode fails on second call (final device path)
//
TEST_F (BuildHttpBootDevicePathTest, AppendDevicePathNodeFailsSecond) {
  // Make the second call to AppendDevicePathNode fail
  SetStubAppendDevicePathNodeFailOnCall (2);

  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("http://server/file"),
                         &ResultDevicePath
                         );

  // Should return EFI_OUT_OF_RESOURCES when second AppendDevicePathNode fails
  EXPECT_EQ (Status, EFI_OUT_OF_RESOURCES);
}

//
// Note: Lines 171-172 (UnicodeStrToAsciiStrS error path) are not covered because
// UnicodeStrToAsciiStrS asserts on invalid input rather than returning an error.
// This defensive error handling is difficult to test without disabling assertions.
//

//
// Test: Very long URI
//
TEST_F (BuildHttpBootDevicePathTest, VeryLongUri) {
  CHAR16  LongUri[1024];

  StrCpyS (LongUri, 1024, CHAR16_STRING ("http://example.com/"));

  // Add a long path
  for (int i = 0; i < 50; i++) {
    StrCatS (LongUri, 1024, CHAR16_STRING ("very/long/path/"));
  }

  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         LongUri,
                         &ResultDevicePath
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_NE (ResultDevicePath, nullptr);
}

//
// Test: URI with port number
//
TEST_F (BuildHttpBootDevicePathTest, UriWithPort) {
  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("http://192.168.1.1:8080/file.img"),
                         &ResultDevicePath
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_NE (ResultDevicePath, nullptr);
}

//
// Test: IPv6 URI with port
//
TEST_F (BuildHttpBootDevicePathTest, IPv6UriWithPort) {
  EFI_STATUS  Status = BuildHttpBootDevicePath (
                         NicHandle,
                         CHAR16_STRING ("http://[2001:db8::1]:8080/file.img"),
                         &ResultDevicePath
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_NE (ResultDevicePath, nullptr);
}
