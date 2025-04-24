/** @file
  Host based unit tests for the RamDiskOS driver.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/GoogleTestLib.h>
#include <Library/PlatformResourceLib.h>                          // for TEGRA_PLATFORM_RESOURCE_INFO
#include <GoogleTest/Library/MockUefiLib.h>
#include <GoogleTest/Library/MockUefiBootServicesTableLib.h>      // for MockUefiBootServicesTableLib gBS_LocateProtocol
#include <GoogleTest/Library/MockHobLib.h>                        // for edk2/MdePkg GetFirstGuidHob;     uses GoogleTest infra
#include <GoogleTest/Library/MockRamDiskProto.h>                  // for RamDiskProtoMock  .Register

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/DebugLib.h>
  #include "Protocol/RamDisk.h"
  #include "RamDiskOSGoogleTest.h"
}

///////////////////////////////////////////////////////////////////////////////
// Definitions
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Symbol Definitions
///////////////////////////////////////////////////////////////////////////////

using namespace testing;

///////////////////////////////////////////////////////////////////////////////
// RamDiskOSEntryPoint Test
///////////////////////////////////////////////////////////////////////////////
class RamDiskOSEntryPointTest : public Test {
public:

protected:
  MockUefiBootServicesTableLib Mock_BSTLib;        // Boot Services Mock Lib; for gBS_LocateProtocol
  MockHobLib Mock_HobLib;                          // HobLib GoogleTest Mock Lib; for edk2/MdePkg GetFirstGuidHob
  MockRamDiskProto Mock_RamDiskProto;              // RamDisk Mock Protocol; for EFI_RAM_RISK_PROTOCOL.Register
  EFI_HOB_GUID_TYPE *PlatformResourceInfoHobData;
  TEGRA_PLATFORM_RESOURCE_INFO *PlatformResourceInfo;

  void
  SetUp (
    ) override
  {
    PlatformResourceInfoHobData                   = (EFI_HOB_GUID_TYPE *)malloc (sizeof (EFI_HOB_GUID_TYPE) + sizeof (TEGRA_PLATFORM_RESOURCE_INFO));
    PlatformResourceInfoHobData->Header.HobType   = EFI_HOB_TYPE_GUID_EXTENSION;
    PlatformResourceInfoHobData->Header.HobLength = sizeof (EFI_HOB_GUID_TYPE) + sizeof (TEGRA_PLATFORM_RESOURCE_INFO);
    PlatformResourceInfo                          = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (PlatformResourceInfoHobData);
    // RamDiskOSEntryPoint checks validity of the next 2 items. Note that any non-zero setting for
    // the next 2 items is acceptable, and these implemented values were selected for convenience
    PlatformResourceInfo->RamdiskOSInfo.Base = (UINTN)malloc (sizeof (TEGRA_PLATFORM_RESOURCE_INFO));
    PlatformResourceInfo->RamdiskOSInfo.Size = (UINTN)sizeof (TEGRA_PLATFORM_RESOURCE_INFO);
  }

  void
  TearDown (
    ) override
  {
    free (PlatformResourceInfoHobData);
    free ((void *)PlatformResourceInfo->RamdiskOSInfo.Base);
    PlatformResourceInfoHobData = NULL;
  }
};

///////////////////////////////////////////////////////////////////////////////
// test RamDiskOSEntryPoint with failing GetFirstGuidHob
///////////////////////////////////////////////////////////////////////////////
TEST_F (RamDiskOSEntryPointTest, EntryPointTest_HobFailure) {
  EFI_STATUS  Status;
  UINTN       TempHobLength;

  TempHobLength                                 = PlatformResourceInfoHobData->Header.HobLength;
  PlatformResourceInfoHobData->Header.HobLength = 0;

  // mock GetFirstGuidHob call from RamDiskOSEntryPoint
  EXPECT_CALL (Mock_HobLib, GetFirstGuidHob (BufferEq (&gNVIDIAPlatformResourceDataGuid, sizeof (EFI_GUID))))
    .WillRepeatedly (Return (PlatformResourceInfoHobData));

  // if GetFirstGuidHob fails then RamDiskOSEntryPoint should not call these
  EXPECT_CALL (Mock_BSTLib, gBS_LocateProtocol).Times (0);
  EXPECT_CALL (Mock_RamDiskProto, Register).Times (0);

  Status = RamDiskOSEntryPoint (NULL, NULL);
  EXPECT_EQ (Status, EFI_NOT_FOUND) << "unexpected return status";

  PlatformResourceInfoHobData->Header.HobLength = TempHobLength;  // restore for sebsequent tests
}

///////////////////////////////////////////////////////////////////////////////
// test RamDiskOSEntryPoint PlatformResourceInfo->RamdiskOSInfo.Base and .Size
///////////////////////////////////////////////////////////////////////////////
TEST_F (RamDiskOSEntryPointTest, EntryPointTest_PlatformResourceInfoFailure) {
  EFI_STATUS  Status;
  UINTN       TempBase, TempSize;

  TempBase = PlatformResourceInfo->RamdiskOSInfo.Base;
  TempSize = PlatformResourceInfo->RamdiskOSInfo.Size;

  // mock GetFirstGuidHob call from RamDiskOSEntryPoint
  EXPECT_CALL (Mock_HobLib, GetFirstGuidHob (BufferEq (&gNVIDIAPlatformResourceDataGuid, sizeof (EFI_GUID))))
    .WillRepeatedly (Return (PlatformResourceInfoHobData));

  // if GetFirstGuidHob fails then RamDiskOSEntryPoint should not call these
  EXPECT_CALL (Mock_BSTLib, gBS_LocateProtocol).Times (0);
  EXPECT_CALL (Mock_RamDiskProto, Register).Times (0);

  // call RamDiskOSEntryPoint with EFI_HANDLE ImageHandle = NULL, IN EFI_SYSTEM_TABLE *SystemTable = NULL
  // even though both of those parameters are not actually used in the ENTRY_POINT function
  PlatformResourceInfo->RamdiskOSInfo.Base = 0;
  Status                                   = RamDiskOSEntryPoint (NULL, NULL);
  EXPECT_EQ (Status, EFI_NOT_FOUND) << "unexpected return status";

  PlatformResourceInfo->RamdiskOSInfo.Base = TempBase;  // restore for sebsequent tests
  PlatformResourceInfo->RamdiskOSInfo.Size = 0;
  Status                                   = RamDiskOSEntryPoint (NULL, NULL);
  EXPECT_EQ (Status, EFI_NOT_FOUND) << "unexpected return status";

  PlatformResourceInfo->RamdiskOSInfo.Base = 0;
  PlatformResourceInfo->RamdiskOSInfo.Size = 0;
  Status                                   = RamDiskOSEntryPoint (NULL, NULL);
  EXPECT_EQ (Status, EFI_NOT_FOUND) << "unexpected return status";

  PlatformResourceInfo->RamdiskOSInfo.Base = TempBase;  // restore for sebsequent tests
  PlatformResourceInfo->RamdiskOSInfo.Size = TempSize;  // restore for sebsequent tests
}

///////////////////////////////////////////////////////////////////////////////
// test RamDiskOSEntryPoint with failing gBS->LocateProtocol
///////////////////////////////////////////////////////////////////////////////
TEST_F (RamDiskOSEntryPointTest, EntryPointTest_LocateProtocolFailure) {
  EFI_STATUS  Status;
  void        *Interface   = NULL;
  void        **pInterface = &Interface; // SetArgPointee requires reference var

  // mock GetFirstGuidHob call from RamDiskOSEntryPoint
  EXPECT_CALL (Mock_HobLib, GetFirstGuidHob (BufferEq (&gNVIDIAPlatformResourceDataGuid, sizeof (EFI_GUID))))
    .WillRepeatedly (Return (PlatformResourceInfoHobData));

  // mock gBS->LocateProtocol call from RamDiskOSEntryPoint to return &gMockRamDiskProtocol
  EXPECT_CALL (Mock_BSTLib, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2>(ByRef (*pInterface)),  // modify LocateProtocol arg[2] void* to NULL
         Return (EFI_INVALID_PARAMETER)
         )
       );

  // if gBS->LocateProtocol fails then RamDiskOSEntryPoint should not call RamDisk->Register
  EXPECT_CALL (Mock_RamDiskProto, Register).Times (0);

  Status = RamDiskOSEntryPoint (NULL, NULL);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER) << "unexpected return status";
}

///////////////////////////////////////////////////////////////////////////////
// test RamDiskOSEntryPoint with failing RamDisk->Register
///////////////////////////////////////////////////////////////////////////////
TEST_F (RamDiskOSEntryPointTest, EntryPointTest_RegisterFailure) {
  EFI_STATUS  Status;

  // mock GetFirstGuidHob call from RamDiskOSEntryPoint
  EXPECT_CALL (Mock_HobLib, GetFirstGuidHob (BufferEq (&gNVIDIAPlatformResourceDataGuid, sizeof (EFI_GUID))))
    .WillRepeatedly (Return (PlatformResourceInfoHobData));

  // mock gBS->LocateProtocol call from RamDiskOSEntryPoint to return &gMockRamDiskProtocol
  EXPECT_CALL (Mock_BSTLib, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2>(ByRef (gMockRamDiskProtocol)), // modify LocateProtocol arg[2]
         Return (EFI_SUCCESS)
         )
       );

  // mock the Ramdisk->Register call from RamDiskOSEntryPoint to return error
  EXPECT_CALL (Mock_RamDiskProto, Register)
    .WillOnce (Return (EFI_OUT_OF_RESOURCES));

  // call RamDiskOSEntryPoint with EFI_HANDLE ImageHandle = NULL, IN EFI_SYSTEM_TABLE *SystemTable = NULL
  Status = RamDiskOSEntryPoint (NULL, NULL);
  EXPECT_EQ (Status, EFI_OUT_OF_RESOURCES) << "unexpected return status";
}

///////////////////////////////////////////////////////////////////////////////
// test RamDiskOSEntryPoint with ImageHandle and *SystemTable both NULL
//
// Note: although RamDiskOSEntryPoint doesn't use ImageHandle and *SystemTable,
//       most driver ENTRY_POINT functions do, so accurate mocking of those
//       parameters would be useful for wider scale testing
///////////////////////////////////////////////////////////////////////////////
TEST_F (RamDiskOSEntryPointTest, EntryPointTest_ParametersCheck) {
  EFI_STATUS        Status;
  EFI_HANDLE        ImageHandle  = NULL;
  EFI_SYSTEM_TABLE  *SystemTable = NULL;

  // mock GetFirstGuidHob call from RamDiskOSEntryPoint
  EXPECT_CALL (Mock_HobLib, GetFirstGuidHob (BufferEq (&gNVIDIAPlatformResourceDataGuid, sizeof (EFI_GUID))))
    .WillRepeatedly (Return (PlatformResourceInfoHobData));

  // mock gBS->LocateProtocol call from RamDiskOSEntryPoint to return &gMockRamDiskProtocol
  EXPECT_CALL (Mock_BSTLib, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2>(ByRef (gMockRamDiskProtocol)), // modify LocateProtocol arg[2]
         Return (EFI_SUCCESS)
         )
       );

  // mock the Ramdisk->Register call from RamDiskOSEntryPoint to return EFI_SUCCESS
  EXPECT_CALL (Mock_RamDiskProto, Register)
    .WillOnce (Return (EFI_SUCCESS));

  // call RamDiskOSEntryPoint with EFI_HANDLE ImageHandle = NULL, IN EFI_SYSTEM_TABLE *SystemTable = NULL
  Status = RamDiskOSEntryPoint (ImageHandle, SystemTable);
  EXPECT_EQ (Status, EFI_SUCCESS) << "unexpected return status";
}

///////////////////////////////////////////////////////////////////////////////
int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
