/** @file
  Unit tests for CreateHttpBootOption function.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <gtest/gtest.h>
#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockUefiBootServicesTableLib.h>
#include <GoogleTest/Library/MockUefiRuntimeServicesTableLib.h>
#include "CreateHttpBootOptionStub.h"

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/MemoryAllocationLib.h>
  #include <Library/PrintLib.h>
  #include <Protocol/SimpleNetwork.h>
  #include <Guid/GlobalVariable.h>
}

using namespace testing;

#define CHAR16_STRING(x)  (reinterpret_cast<CHAR16*>(const_cast<char16_t*>(u##x)))

// Forward declare the function under test
extern "C" {
  EFI_STATUS
  CreateHttpBootOption (
    IN  EFI_MAC_ADDRESS  *MacAddr,
    IN  CONST CHAR16     *Uri,
    OUT UINT16           *OptionNum
    );
}

//
// Mock SNP Mode structure helper
//
typedef struct {
  EFI_SIMPLE_NETWORK_MODE    Mode;
  EFI_MAC_ADDRESS            MacAddress;
} MOCK_SNP_MODE;

//
// Mock SNP structure helper
//
typedef struct {
  EFI_SIMPLE_NETWORK_PROTOCOL    Snp;
  MOCK_SNP_MODE                  SnpMode;
} MOCK_SNP;

//
// Test fixture for CreateHttpBootOption tests
//
class CreateHttpBootOptionTest : public Test {
protected:
  StrictMock<MockUefiBootServicesTableLib> MockBs;
  StrictMock<MockUefiRuntimeServicesTableLib> MockRt;
  EFI_MAC_ADDRESS MacAddr;
  UINT16 OptionNum;
  EFI_HANDLE MockHandle;
  MOCK_SNP MockSnp;
  EFI_DEVICE_PATH_PROTOCOL MockDevicePath;

  void
  SetUp (
    ) override
  {
    ZeroMem (&MacAddr, sizeof (MacAddr));
    OptionNum = 0;

    // Initialize mock handle
    MockHandle = (EFI_HANDLE)0x1234;

    // Initialize Mock SNP structure
    ZeroMem (&MockSnp, sizeof (MockSnp));
    MockSnp.Snp.Mode                    = &MockSnp.SnpMode.Mode;
    MockSnp.SnpMode.Mode.CurrentAddress = MockSnp.SnpMode.MacAddress;
    SetMockSnpMac (&MockSnp, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

    // Initialize mock device path
    ZeroMem (&MockDevicePath, sizeof (MockDevicePath));
    MockDevicePath.Type      = END_DEVICE_PATH_TYPE;
    MockDevicePath.SubType   = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    MockDevicePath.Length[0] = sizeof (EFI_DEVICE_PATH_PROTOCOL);
    MockDevicePath.Length[1] = 0;

    // Clear stub state
    ClearStubFindNicByMac ();
    ClearStubBuildHttpBootDevicePath ();
  }

  void
  SetMacAddr (
    UINT8  b0,
    UINT8  b1,
    UINT8  b2,
    UINT8  b3,
    UINT8  b4,
    UINT8  b5
    )
  {
    MacAddr.Addr[0] = b0;
    MacAddr.Addr[1] = b1;
    MacAddr.Addr[2] = b2;
    MacAddr.Addr[3] = b3;
    MacAddr.Addr[4] = b4;
    MacAddr.Addr[5] = b5;
  }

  void
  SetMockSnpMac (
    MOCK_SNP  *MockSnp,
    UINT8     b0,
    UINT8     b1,
    UINT8     b2,
    UINT8     b3,
    UINT8     b4,
    UINT8     b5
    )
  {
    MockSnp->SnpMode.MacAddress.Addr[0] = b0;
    MockSnp->SnpMode.MacAddress.Addr[1] = b1;
    MockSnp->SnpMode.MacAddress.Addr[2] = b2;
    MockSnp->SnpMode.MacAddress.Addr[3] = b3;
    MockSnp->SnpMode.MacAddress.Addr[4] = b4;
    MockSnp->SnpMode.MacAddress.Addr[5] = b5;
    CopyMem (&MockSnp->SnpMode.Mode.CurrentAddress, &MockSnp->SnpMode.MacAddress, sizeof (EFI_MAC_ADDRESS));
  }
};

//
// Test: Success with specific MAC address
//
TEST_F (CreateHttpBootOptionTest, SuccessWithSpecificMac) {
  SetMacAddr (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

  // Stub FindNicByMac to succeed
  SetStubFindNicByMac (EFI_SUCCESS, MockHandle);

  // Stub BuildHttpBootDevicePath to succeed
  SetStubBuildHttpBootDevicePath (EFI_SUCCESS, &MockDevicePath);

  // Expect SetVariable to be called twice (Boot#### and BootNext)
  EXPECT_CALL (MockRt, gRT_SetVariable (_, _, _, _, _))
    .Times (2)
    .WillRepeatedly (Return (EFI_SUCCESS));

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (OptionNum, 0x8C7D);  // REDFISH_HTTP_BOOT_OPTION_NUM
}

//
// Test: Success with all-zeros MAC (first NIC with link up)
//
TEST_F (CreateHttpBootOptionTest, SuccessWithAllZerosMac) {
  // MAC is already all zeros from SetUp
  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 1);

  HandleArray[0] = MockHandle;

  // Set MediaPresent = TRUE (link up)
  MockSnp.SnpMode.Mode.MediaPresent = TRUE;

  // Expect LocateHandleBuffer to find NICs
  EXPECT_CALL (
    MockBs,
    gBS_LocateHandleBuffer (
      ByProtocol,
      _,
      NULL,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(1),
         SetArgPointee<4>(HandleArray),
         Return (EFI_SUCCESS)
         )
       );

  // Expect HandleProtocol to get SNP (called once, NIC has link)
  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp.Snp),
         Return (EFI_SUCCESS)
         )
       );

  // Stub BuildHttpBootDevicePath to succeed
  SetStubBuildHttpBootDevicePath (EFI_SUCCESS, &MockDevicePath);

  // Expect SetVariable to be called twice
  EXPECT_CALL (MockRt, gRT_SetVariable (_, _, _, _, _))
    .Times (2)
    .WillRepeatedly (Return (EFI_SUCCESS));

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
}

//
// Test: LocateHandleBuffer fails (all NICs mode)
//
TEST_F (CreateHttpBootOptionTest, LocateHandleBufferFails) {
  // MAC is all zeros (all NICs mode)

  EXPECT_CALL (
    MockBs,
    gBS_LocateHandleBuffer (
      ByProtocol,
      _,
      NULL,
      _,
      _
      )
    )
    .WillOnce (Return (EFI_NOT_FOUND));

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

//
// Test: LocateHandleBuffer returns no NICs (all NICs mode)
//
TEST_F (CreateHttpBootOptionTest, LocateHandleBufferNoNics) {
  // MAC is all zeros (all NICs mode)

  EXPECT_CALL (
    MockBs,
    gBS_LocateHandleBuffer (
      ByProtocol,
      _,
      NULL,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(0),       // HandleCount = 0
         SetArgPointee<4>(nullptr), // Handles = NULL
         Return (EFI_SUCCESS)
         )
       );

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

//
// Test: HandleProtocol fails (all NICs mode)
//
TEST_F (CreateHttpBootOptionTest, HandleProtocolFails) {
  // MAC is all zeros (all NICs mode)
  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 1);

  HandleArray[0] = MockHandle;

  EXPECT_CALL (
    MockBs,
    gBS_LocateHandleBuffer (
      ByProtocol,
      _,
      NULL,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(1),
         SetArgPointee<4>(HandleArray),
         Return (EFI_SUCCESS)
         )
       );

  // Expect HandleProtocol twice: once for link check, once for fallback
  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle,
      _,
      _
      )
    )
    .Times (2)
    .WillRepeatedly (Return (EFI_UNSUPPORTED));

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_UNSUPPORTED);
}

//
// Test: FindNicByMac fails (specific MAC mode)
//
TEST_F (CreateHttpBootOptionTest, FindNicByMacFails) {
  SetMacAddr (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

  // Stub FindNicByMac to fail
  SetStubFindNicByMac (EFI_NOT_FOUND, NULL);

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

//
// Test: BuildHttpBootDevicePath fails
//
TEST_F (CreateHttpBootOptionTest, BuildHttpBootDevicePathFails) {
  SetMacAddr (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

  // Stub FindNicByMac to succeed
  SetStubFindNicByMac (EFI_SUCCESS, MockHandle);

  // Stub BuildHttpBootDevicePath to fail
  SetStubBuildHttpBootDevicePath (EFI_INVALID_PARAMETER, NULL);

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: First SetVariable fails (Boot#### creation)
//
TEST_F (CreateHttpBootOptionTest, FirstSetVariableFails) {
  SetMacAddr (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

  // Stub FindNicByMac to succeed
  SetStubFindNicByMac (EFI_SUCCESS, MockHandle);

  // Stub BuildHttpBootDevicePath to succeed
  SetStubBuildHttpBootDevicePath (EFI_SUCCESS, &MockDevicePath);

  // First SetVariable (Boot####) fails
  EXPECT_CALL (MockRt, gRT_SetVariable (_, _, _, _, _))
    .WillOnce (Return (EFI_OUT_OF_RESOURCES));

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_OUT_OF_RESOURCES);
}

//
// Test: Second SetVariable fails (BootNext)
//
TEST_F (CreateHttpBootOptionTest, SecondSetVariableFails) {
  SetMacAddr (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

  // Stub FindNicByMac to succeed
  SetStubFindNicByMac (EFI_SUCCESS, MockHandle);

  // Stub BuildHttpBootDevicePath to succeed
  SetStubBuildHttpBootDevicePath (EFI_SUCCESS, &MockDevicePath);

  // First SetVariable (Boot####) succeeds, second (BootNext) fails
  EXPECT_CALL (MockRt, gRT_SetVariable (_, _, _, _, _))
    .WillOnce (Return (EFI_SUCCESS))
    .WillOnce (Return (EFI_ACCESS_DENIED));

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_ACCESS_DENIED);
}

//
// Test: All-zeros MAC selects second NIC (first has no link)
//
TEST_F (CreateHttpBootOptionTest, AllZerosMacSelectsSecondNicWithLink) {
  // MAC is all zeros
  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 2);
  EFI_HANDLE  MockHandle2  = (EFI_HANDLE)0x5678;
  MOCK_SNP    MockSnp2;

  HandleArray[0] = MockHandle;
  HandleArray[1] = MockHandle2;

  // First NIC has no link
  MockSnp.SnpMode.Mode.MediaPresent = FALSE;

  // Second NIC has link
  ZeroMem (&MockSnp2, sizeof (MockSnp2));
  MockSnp2.Snp.Mode                    = &MockSnp2.SnpMode.Mode;
  MockSnp2.SnpMode.Mode.CurrentAddress = MockSnp2.SnpMode.MacAddress;
  MockSnp2.SnpMode.Mode.MediaPresent   = TRUE;
  SetMockSnpMac (&MockSnp2, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

  // Expect LocateHandleBuffer to find 2 NICs
  EXPECT_CALL (
    MockBs,
    gBS_LocateHandleBuffer (
      ByProtocol,
      _,
      NULL,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(2),
         SetArgPointee<4>(HandleArray),
         Return (EFI_SUCCESS)
         )
       );

  // Expect HandleProtocol for first NIC (no link)
  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp.Snp),
         Return (EFI_SUCCESS)
         )
       );

  // Expect HandleProtocol for second NIC (has link)
  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle2,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp2.Snp),
         Return (EFI_SUCCESS)
         )
       );

  // Stub BuildHttpBootDevicePath to succeed
  SetStubBuildHttpBootDevicePath (EFI_SUCCESS, &MockDevicePath);

  // Expect SetVariable to be called twice
  EXPECT_CALL (MockRt, gRT_SetVariable (_, _, _, _, _))
    .Times (2)
    .WillRepeatedly (Return (EFI_SUCCESS));

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
}

//
// Test: All-zeros MAC fallback when no NICs have link
//
TEST_F (CreateHttpBootOptionTest, AllZerosMacFallbackNoLink) {
  // MAC is all zeros
  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 1);

  HandleArray[0] = MockHandle;

  // NIC has no link
  MockSnp.SnpMode.Mode.MediaPresent = FALSE;

  // Expect LocateHandleBuffer to find NICs
  EXPECT_CALL (
    MockBs,
    gBS_LocateHandleBuffer (
      ByProtocol,
      _,
      NULL,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(1),
         SetArgPointee<4>(HandleArray),
         Return (EFI_SUCCESS)
         )
       );

  // Expect HandleProtocol twice: once for link check, once for fallback
  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle,
      _,
      _
      )
    )
    .Times (2)
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(&MockSnp.Snp),
         Return (EFI_SUCCESS)
         )
       );

  // Stub BuildHttpBootDevicePath to succeed
  SetStubBuildHttpBootDevicePath (EFI_SUCCESS, &MockDevicePath);

  // Expect SetVariable to be called twice
  EXPECT_CALL (MockRt, gRT_SetVariable (_, _, _, _, _))
    .Times (2)
    .WillRepeatedly (Return (EFI_SUCCESS));

  EFI_STATUS  Status = CreateHttpBootOption (
                         &MacAddr,
                         CHAR16_STRING ("http://192.168.1.1/boot.img"),
                         &OptionNum
                         );

  // Should succeed (fallback to first NIC)
  EXPECT_EQ (Status, EFI_SUCCESS);
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
