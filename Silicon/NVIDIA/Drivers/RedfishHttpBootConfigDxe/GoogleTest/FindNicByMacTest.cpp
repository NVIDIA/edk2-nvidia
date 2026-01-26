/** @file
  Unit tests for FindNicByMac function.
  Uses pure GoogleTest with MockUefiBootServicesTableLib.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <gtest/gtest.h>
#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockUefiBootServicesTableLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/MemoryAllocationLib.h>
  #include <Protocol/SimpleNetwork.h>
  #include "../RedfishHttpBootConfigUtils.h"
}

using namespace testing;

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
// Test fixture for FindNicByMac tests
//
class FindNicByMacTest : public Test {
protected:
  StrictMock<MockUefiBootServicesTableLib> MockBs;
  EFI_MAC_ADDRESS TargetMac;
  EFI_HANDLE ResultHandle;

  // Mock NIC handles and SNP structures
  EFI_HANDLE MockHandle1;
  EFI_HANDLE MockHandle2;
  EFI_HANDLE MockHandle3;
  MOCK_SNP MockSnp1;
  MOCK_SNP MockSnp2;
  MOCK_SNP MockSnp3;

  void
  SetUp (
    ) override
  {
    ZeroMem (&TargetMac, sizeof (TargetMac));
    ResultHandle = NULL;

    // Initialize mock handles (use unique addresses)
    MockHandle1 = (EFI_HANDLE)0x1000;
    MockHandle2 = (EFI_HANDLE)0x2000;
    MockHandle3 = (EFI_HANDLE)0x3000;

    // Initialize Mock SNP structures
    ZeroMem (&MockSnp1, sizeof (MockSnp1));
    ZeroMem (&MockSnp2, sizeof (MockSnp2));
    ZeroMem (&MockSnp3, sizeof (MockSnp3));

    // Link Mode to SNP
    MockSnp1.Snp.Mode = &MockSnp1.SnpMode.Mode;
    MockSnp2.Snp.Mode = &MockSnp2.SnpMode.Mode;
    MockSnp3.Snp.Mode = &MockSnp3.SnpMode.Mode;

    // Link CurrentAddress in Mode
    MockSnp1.SnpMode.Mode.CurrentAddress = MockSnp1.SnpMode.MacAddress;
    MockSnp2.SnpMode.Mode.CurrentAddress = MockSnp2.SnpMode.MacAddress;
    MockSnp3.SnpMode.Mode.CurrentAddress = MockSnp3.SnpMode.MacAddress;

    // Set default MAC addresses
    SetMockSnpMac (&MockSnp1, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    SetMockSnpMac (&MockSnp2, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    SetMockSnpMac (&MockSnp3, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66);
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
    // Copy to CurrentAddress (since it's a struct, not a pointer)
    CopyMem (&MockSnp->SnpMode.Mode.CurrentAddress, &MockSnp->SnpMode.MacAddress, sizeof (EFI_MAC_ADDRESS));
  }

  void
  SetTargetMac (
    UINT8  b0,
    UINT8  b1,
    UINT8  b2,
    UINT8  b3,
    UINT8  b4,
    UINT8  b5
    )
  {
    TargetMac.Addr[0] = b0;
    TargetMac.Addr[1] = b1;
    TargetMac.Addr[2] = b2;
    TargetMac.Addr[3] = b3;
    TargetMac.Addr[4] = b4;
    TargetMac.Addr[5] = b5;
  }
};

//
// Test: NULL parameters should return EFI_INVALID_PARAMETER
//
TEST_F (FindNicByMacTest, NullMacAddress) {
  EFI_STATUS  Status = FindNicByMac (NULL, &ResultHandle);

  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F (FindNicByMacTest, NullNicHandle) {
  SetTargetMac (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
  EFI_STATUS  Status = FindNicByMac (&TargetMac, NULL);

  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Test: LocateHandleBuffer fails - should propagate error
//
TEST_F (FindNicByMacTest, LocateHandleBufferFails) {
  SetTargetMac (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

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

  EFI_STATUS  Status = FindNicByMac (&TargetMac, &ResultHandle);

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

//
// Test: No NICs found (HandleCount = 0)
//
TEST_F (FindNicByMacTest, NoNicsFound) {
  SetTargetMac (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

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

  EFI_STATUS  Status = FindNicByMac (&TargetMac, &ResultHandle);

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

//
// Test: Single NIC with matching MAC
//
TEST_F (FindNicByMacTest, SingleNicMatching) {
  SetTargetMac (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 1);

  HandleArray[0] = MockHandle1;

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
         SetArgPointee<3>(1),           // HandleCount = 1
         SetArgPointee<4>(HandleArray), // Handles array
         Return (EFI_SUCCESS)
         )
       );

  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle1,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp1.Snp),
         Return (EFI_SUCCESS)
         )
       );

  EFI_STATUS  Status = FindNicByMac (&TargetMac, &ResultHandle);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (ResultHandle, MockHandle1);

  // HandleArray is freed by FindNicByMac via FreePool()
}

//
// Test: Single NIC with non-matching MAC
//
TEST_F (FindNicByMacTest, SingleNicNonMatching) {
  SetTargetMac (0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);  // Different from MockSnp1

  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 1);

  HandleArray[0] = MockHandle1;

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

  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle1,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp1.Snp),
         Return (EFI_SUCCESS)
         )
       );

  EFI_STATUS  Status = FindNicByMac (&TargetMac, &ResultHandle);

  EXPECT_EQ (Status, EFI_NOT_FOUND);

  // HandleArray is freed by FindNicByMac via FreePool()
}

//
// Test: Multiple NICs, match first
//
TEST_F (FindNicByMacTest, MultipleNicsMatchFirst) {
  SetTargetMac (0x00, 0x11, 0x22, 0x33, 0x44, 0x55);  // Matches MockSnp1

  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 3);

  HandleArray[0] = MockHandle1;
  HandleArray[1] = MockHandle2;
  HandleArray[2] = MockHandle3;

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
         SetArgPointee<3>(3),
         SetArgPointee<4>(HandleArray),
         Return (EFI_SUCCESS)
         )
       );

  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle1,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp1.Snp),
         Return (EFI_SUCCESS)
         )
       );

  // Should not call HandleProtocol for other handles (early return)

  EFI_STATUS  Status = FindNicByMac (&TargetMac, &ResultHandle);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (ResultHandle, MockHandle1);

  // HandleArray is freed by FindNicByMac via FreePool()
}

//
// Test: Multiple NICs, match second
//
TEST_F (FindNicByMacTest, MultipleNicsMatchSecond) {
  SetTargetMac (0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);  // Matches MockSnp2

  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 3);

  HandleArray[0] = MockHandle1;
  HandleArray[1] = MockHandle2;
  HandleArray[2] = MockHandle3;

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
         SetArgPointee<3>(3),
         SetArgPointee<4>(HandleArray),
         Return (EFI_SUCCESS)
         )
       );

  // First handle - no match
  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle1,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp1.Snp),
         Return (EFI_SUCCESS)
         )
       );

  // Second handle - match!
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

  EFI_STATUS  Status = FindNicByMac (&TargetMac, &ResultHandle);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (ResultHandle, MockHandle2);

  // HandleArray is freed by FindNicByMac via FreePool()
}

//
// Test: Multiple NICs, no match
//
TEST_F (FindNicByMacTest, MultipleNicsNoMatch) {
  SetTargetMac (0x99, 0x88, 0x77, 0x66, 0x55, 0x44);  // Doesn't match any

  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 3);

  HandleArray[0] = MockHandle1;
  HandleArray[1] = MockHandle2;
  HandleArray[2] = MockHandle3;

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
         SetArgPointee<3>(3),
         SetArgPointee<4>(HandleArray),
         Return (EFI_SUCCESS)
         )
       );

  // All three HandleProtocol calls
  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle1,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp1.Snp),
         Return (EFI_SUCCESS)
         )
       );

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

  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle3,
      _,
      _
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<2>(&MockSnp3.Snp),
         Return (EFI_SUCCESS)
         )
       );

  EFI_STATUS  Status = FindNicByMac (&TargetMac, &ResultHandle);

  EXPECT_EQ (Status, EFI_NOT_FOUND);

  // HandleArray is freed by FindNicByMac via FreePool()
}

//
// Test: HandleProtocol fails for one NIC, continues to next
//
TEST_F (FindNicByMacTest, HandleProtocolFailsContinues) {
  SetTargetMac (0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);  // Matches MockSnp2

  EFI_HANDLE  *HandleArray = (EFI_HANDLE *)malloc (sizeof (EFI_HANDLE) * 2);

  HandleArray[0] = MockHandle1;
  HandleArray[1] = MockHandle2;

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

  // First handle - HandleProtocol fails
  EXPECT_CALL (
    MockBs,
    gBS_HandleProtocol (
      MockHandle1,
      _,
      _
      )
    )
    .WillOnce (Return (EFI_UNSUPPORTED));

  // Second handle - succeeds and matches
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

  EFI_STATUS  Status = FindNicByMac (&TargetMac, &ResultHandle);

  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (ResultHandle, MockHandle2);

  // HandleArray is freed by FindNicByMac via FreePool()
}
