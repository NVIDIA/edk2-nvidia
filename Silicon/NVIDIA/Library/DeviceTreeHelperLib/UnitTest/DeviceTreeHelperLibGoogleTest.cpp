/** @file
  Unit tests for the implementation of DeviceTreeHelperLib.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockFdtLib.h>
#include <GoogleTest/Library/MockDtPlatformDtbLoaderLib.h>
#include <string.h>
#include <endian.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/DeviceTreeHelperLib.h>
  #include <Library/PlatformResourceLib.h>
}

using namespace testing;
using ::testing::NiceMock;

typedef struct {
  UINT32    Tag;
  UINT32    Length;
  UINT32    NameOffset;
  CHAR8     Data[100];
} FDT_PROPERTY_LEN_100;

typedef struct {
  UINT32    Tag;
  UINT32    Length;
  UINT32    NameOffset;
  UINT32    ValueBigEndian;
} FDT_PROPERTY_32;

#define MAX_REGISTER_NUMS  4
typedef struct {
  UINT32    Tag;
  UINT32    Length;
  UINT32    NameOffset;
  UINT32    ValueBigEndian[MAX_REGISTER_NUMS * 3 * 2];
} FDT_PROPERTY_MAX_MEMORY_RANGE;

#define TEST_NODE_OFFSET                   5
#define TEST_DEVICE_TREE_ADDRESS           ((VOID *)0xDEADBEEF)
#define TEST_DEVICE_TREE_SIZE              ((UINTN)0x1000)
#define TEST_PLATFORM_DEVICE_TREE_ADDRESS  ((VOID *)0xAFAFAFAF)
#define TEST_PLATFORM_DEVICE_TREE_SIZE     ((UINTN)0x4000)
enum {
  SINGLE_COMPAT = 0,
  CPU_TYPE      = SINGLE_COMPAT,
  SINGLE_COMPAT2,
  MISSING_TYPE2 = SINGLE_COMPAT2,
  DUAL_COMPAT,
  MEMORY_TYPE = DUAL_COMPAT,
  WRONG_COMPAT,
  WRONG_TYPE = WRONG_COMPAT,
  MISSING_COMPAT,
  MISSING_TYPE = MISSING_COMPAT,
  NUMBER_OF_COMPATIBLE_TYPES
};

#define SINGLE_COMPAT_STRING   "device1"
#define SINGLE_COMPAT_STRING2  "device2"
#define DUAL_COMPAT_STRING     "device1\0device2"
#define WRONG_COMPAT_STRING    "device3"
#define WRONG_COMPAT_STRING2   "device4"

#define CPU_TYPE_STRING     "cpu"
#define MEMORY_TYPE_STRING  "memory"
#define WRONG_TYPE_STRING   "bad_type"

CONST CHAR8  *Device1CompatInfo[]       = { SINGLE_COMPAT_STRING, NULL };
CONST CHAR8  *Device2CompatInfo[]       = { SINGLE_COMPAT_STRING2, NULL };
CONST CHAR8  *DeviceBothCompatInfo[]    = { SINGLE_COMPAT_STRING, SINGLE_COMPAT_STRING2, NULL };
CONST CHAR8  *DeviceMissingCompatInfo[] = { WRONG_COMPAT_STRING2, NULL };

enum {
  STATUS_OKAY = 0,
  STATUS_DISABLED,
  STATUS_MISSING,
  STATUS_ERROR,
  NUMBER_OF_STATUS_TYPES
};

#define STATUS_OKAY_STRING      "okay"
#define STATUS_DISABLED_STRING  "disabled"

#define TEST_MAX_OFFSET  ((NUMBER_OF_COMPATIBLE_TYPES * NUMBER_OF_STATUS_TYPES)-1)

ACTION (BigEndianToHost16) {
  return be16toh (arg0);
};
ACTION (HostToBigEndian16) {
  return htobe16 (arg0);
};
ACTION (BigEndianToHost32) {
  return be32toh (arg0);
};
ACTION (HostToBigEndian32) {
  return htobe32 (arg0);
};
ACTION (BigEndianToHost64) {
  return be64toh (arg0);
};
ACTION (HostToBigEndian64) {
  return htobe64 (arg0);
};

//////////////////////////////////////////////////////////////////////////////
class DeviceTreeHelperBase : public  Test {
protected:
  NiceMock<MockFdtLib> FdtMock;
  NiceMock<MockDtPlatformDtbLoaderLib> DtPlatformDtbLoaderMock;
  EFI_STATUS Status;
  EFI_STATUS ExpectedStatus;
  void
  SetUp (
    ) override
  {
    SetDeviceTreePointer (0, 0);
    ON_CALL (
      FdtMock,
      Fdt16ToCpu (
        _
        )
      )
      .WillByDefault (
         BigEndianToHost16 ()
         );
    ON_CALL (
      FdtMock,
      CpuToFdt16 (
        _
        )
      )
      .WillByDefault (
         HostToBigEndian16 ()
         );
    ON_CALL (
      FdtMock,
      Fdt32ToCpu (
        _
        )
      )
      .WillByDefault (
         BigEndianToHost32 ()
         );
    ON_CALL (
      FdtMock,
      CpuToFdt32 (
        _
        )
      )
      .WillByDefault (
         HostToBigEndian32 ()
         );
    ON_CALL (
      FdtMock,
      Fdt64ToCpu (
        _
        )
      )
      .WillByDefault (
         BigEndianToHost64 ()
         );
    ON_CALL (
      FdtMock,
      CpuToFdt64 (
        _
        )
      )
      .WillByDefault (
         HostToBigEndian64 ()
         );
  }
};

class DeviceTreeHelperPlatform : public  DeviceTreeHelperBase {
protected:
  void
  SetUp (
    ) override
  {
    DeviceTreeHelperBase::SetUp ();
    ON_CALL (
      DtPlatformDtbLoaderMock,
      DtPlatformLoadDtb (
        NotNull (),
        NotNull ()
        )
      )
      .WillByDefault (
         DoAll (
           SetArgPointee<0>(TEST_PLATFORM_DEVICE_TREE_ADDRESS),
           SetArgPointee<1>(TEST_PLATFORM_DEVICE_TREE_SIZE),
           Return (EFI_SUCCESS)
           )
         );
  }
};

//////////////////////////////////////////////////////////////////////////////
class PointerTestNoPlatform : public  DeviceTreeHelperBase {
protected:
  void
  SetUp (
    ) override
  {
    DeviceTreeHelperBase::SetUp ();
    EXPECT_CALL (
      DtPlatformDtbLoaderMock,
      DtPlatformLoadDtb (
        NotNull (),
        NotNull ()
        )
      )
      .WillRepeatedly (
         Return (EFI_NOT_FOUND)
         );
  }
};

// Test SetDeviceTreePointer() and GetDeviceTreePointer API with platform dtb
TEST_F (PointerTestNoPlatform, DeviceTreePointer) {
  VOID   *DeviceTree;
  UINTN  DeviceTreeSize;

  EXPECT_CALL (
    DtPlatformDtbLoaderMock,
    DtPlatformLoadDtb (
      NotNull (),
      NotNull ()
      )
    )
    .WillRepeatedly (Return (EFI_NOT_FOUND));

  SetDeviceTreePointer (TEST_DEVICE_TREE_ADDRESS, TEST_DEVICE_TREE_SIZE);

  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreePointer (NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreePointer (NULL, &DeviceTreeSize));
  DeviceTree = 0;
  EXPECT_EQ (EFI_SUCCESS, GetDeviceTreePointer (&DeviceTree, NULL));
  EXPECT_EQ (DeviceTree, TEST_DEVICE_TREE_ADDRESS);
  DeviceTree = 0;
  EXPECT_EQ (EFI_SUCCESS, GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize));
  EXPECT_EQ (DeviceTree, TEST_DEVICE_TREE_ADDRESS);
  EXPECT_EQ (DeviceTreeSize, TEST_DEVICE_TREE_SIZE);

  SetDeviceTreePointer (0, 0);
  EXPECT_EQ (EFI_NOT_FOUND, GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize));
}

// Test APIS for correct handing with no DTB found
TEST_F (PointerTestNoPlatform, CompilanceNoDtb) {
  INT32        NodeOffset = -1;
  UINT32       NodeCount;
  CONST CHAR8  *CompatibleInfo[] = { "Compat", NULL };
  UINT64       PropertyValue64;
  UINT32       PropertyValue32;
  UINT32       Index;
  CHAR8        *NodePath;
  UINT32       NodePathSize;
  UINT32       NumberOfRegisters;

 #ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS
  UINT64  KernelStart;
  UINT64  KernelDtbStart;
  VOID    *DeviceTreeBase;
  UINT32  NodeHandle;
 #endif

  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNextCompatibleNode (CompatibleInfo, &NodeOffset));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetCompatibleNodeCount (CompatibleInfo, &NodeCount));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNextCpuNode (&NodeOffset));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetCpuNodeCount (&NodeCount));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNextMemoryNode (&NodeOffset));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetMemoryNodeCount (&NodeCount));
 #ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS
  NodeCount = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, GetMatchingEnabledDeviceTreeNodes ("Compat", NULL, &NodeCount));
 #endif
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNodeByPHandle (0, &NodeOffset));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNodePHandle (0, &NodeCount));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNodeByPath ("path", &NodeOffset));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNodePath (0, &NodePath, &NodePathSize));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNodeProperty (0, "prop", NULL, NULL));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNodePropertyValue64 (0, "prop", &PropertyValue64));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNodePropertyValue32 (0, "prop", &PropertyValue32));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeLocateStringIndex (0, "prop", "string", &Index));
 #ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS
  EXPECT_EQ (EFI_DEVICE_ERROR, GetKernelAddress (&KernelStart, &KernelDtbStart));
  EXPECT_EQ (EFI_DEVICE_ERROR, GetDeviceTreeNode (0, &DeviceTreeBase, &NodeOffset));
  EXPECT_EQ (EFI_DEVICE_ERROR, GetDeviceTreeHandle (TEST_PLATFORM_DEVICE_TREE_ADDRESS, 0, &NodeHandle));
 #endif
  NumberOfRegisters = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetRegisters (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));
  NumberOfRegisters = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetRanges (TEST_NODE_OFFSET, "ranges", NULL, &NumberOfRegisters));
 #ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS
  NumberOfRegisters = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, GetDeviceTreeRegisters (0, NULL, &NumberOfRegisters));
 #endif
}

class PointerTestPlatform : public  DeviceTreeHelperPlatform {
};
// Test SetDeviceTreePointer() and GetDeviceTreePointer API with a platform dtb
TEST_F (PointerTestPlatform, DeviceTreePointer) {
  VOID   *DeviceTree;
  UINTN  DeviceTreeSize;

  SetDeviceTreePointer (0, 0);
  DeviceTree     = NULL;
  DeviceTreeSize = 0;
  EXPECT_EQ (EFI_SUCCESS, GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize));
  EXPECT_EQ (DeviceTree, TEST_PLATFORM_DEVICE_TREE_ADDRESS);
  EXPECT_EQ (DeviceTreeSize, TEST_PLATFORM_DEVICE_TREE_SIZE);

  SetDeviceTreePointer (TEST_DEVICE_TREE_ADDRESS, TEST_DEVICE_TREE_SIZE);
  DeviceTree     = NULL;
  DeviceTreeSize = 0;
  EXPECT_EQ (EFI_SUCCESS, GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize));
  EXPECT_EQ (DeviceTree, TEST_DEVICE_TREE_ADDRESS);
  EXPECT_EQ (DeviceTreeSize, TEST_DEVICE_TREE_SIZE);

  SetDeviceTreePointer (TEST_DEVICE_TREE_ADDRESS, 0);
  DeviceTree     = NULL;
  DeviceTreeSize = 0;
  EXPECT_EQ (EFI_SUCCESS, GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize));
  EXPECT_EQ (DeviceTree, TEST_PLATFORM_DEVICE_TREE_ADDRESS);
  EXPECT_EQ (DeviceTreeSize, TEST_PLATFORM_DEVICE_TREE_SIZE);

  SetDeviceTreePointer (0, 0);
  DeviceTree     = NULL;
  DeviceTreeSize = 0;
  EXPECT_EQ (EFI_SUCCESS, GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize));
  EXPECT_EQ (DeviceTree, TEST_PLATFORM_DEVICE_TREE_ADDRESS);
  EXPECT_EQ (DeviceTreeSize, TEST_PLATFORM_DEVICE_TREE_SIZE);

  SetDeviceTreePointer (0, 0);
  DeviceTree = NULL;
  EXPECT_EQ (EFI_SUCCESS, GetDeviceTreePointer (&DeviceTree, NULL));
  EXPECT_EQ (DeviceTree, TEST_PLATFORM_DEVICE_TREE_ADDRESS);
}

class HandleTestPlatform : public  DeviceTreeHelperPlatform {
};

TEST_F (HandleTestPlatform, GetDeviceTreeNodeCompliance) {
  VOID   *DeviceTreeBase;
  INT32  NodeOffset;

  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeNode (0, NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeNode (0, &DeviceTreeBase, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeNode (0, NULL, &NodeOffset));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeNode (TEST_PLATFORM_DEVICE_TREE_SIZE+1, &DeviceTreeBase, &NodeOffset));
}

TEST_F (HandleTestPlatform, GetDeviceTreeHandleCompliance) {
  UINT32  Handle;

  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeHandle (NULL, 0, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeHandle (TEST_PLATFORM_DEVICE_TREE_ADDRESS, 0, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeHandle (NULL, 0, &Handle));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeHandle (TEST_DEVICE_TREE_ADDRESS, 0, &Handle));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetDeviceTreeHandle (TEST_PLATFORM_DEVICE_TREE_ADDRESS, TEST_PLATFORM_DEVICE_TREE_SIZE+1, &Handle));
}

TEST_F (HandleTestPlatform, GetDeviceTreeNodeHandle) {
  UINT32  Handle;
  INT32   TestOffset;
  VOID    *DeviceTreeBase;

  for (INT32 Offset = 0; Offset < (INT32)TEST_PLATFORM_DEVICE_TREE_SIZE; Offset++) {
    EXPECT_EQ (EFI_SUCCESS, GetDeviceTreeHandle (TEST_PLATFORM_DEVICE_TREE_ADDRESS, Offset, &Handle));
    EXPECT_EQ (EFI_SUCCESS, GetDeviceTreeNode (Handle, &DeviceTreeBase, &TestOffset));
    EXPECT_EQ (DeviceTreeBase, TEST_PLATFORM_DEVICE_TREE_ADDRESS);
    EXPECT_EQ (Offset, TestOffset);
  }
}

ACTION (NextOffset) {
  return arg1 + 1;
}

MATCHER_P (IsStatusType, n, "") {
  return ((arg / NUMBER_OF_COMPATIBLE_TYPES) == n);
}

MATCHER_P (IsCompatibleType, n, "") {
  return ((arg % NUMBER_OF_COMPATIBLE_TYPES) == n);
}

class DeviceEnumeration : public  DeviceTreeHelperPlatform {
protected:

  constexpr static FDT_PROPERTY_LEN_100 StatusOkayProperty     = { 0, sizeof (STATUS_OKAY_STRING), 0, STATUS_OKAY_STRING };
  constexpr static FDT_PROPERTY_LEN_100 StatusDisabledProperty = { 0, sizeof (STATUS_DISABLED_STRING), 0, STATUS_DISABLED_STRING };

  void
  SetUp (
    ) override
  {
    DeviceTreeHelperPlatform::SetUp ();
    EXPECT_CALL (
      FdtMock,
      FdtNextNode (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        Lt (TEST_MAX_OFFSET),
        _
        )
      )
      .WillRepeatedly (NextOffset ());
    EXPECT_CALL (
      FdtMock,
      FdtNextNode (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        Eq (TEST_MAX_OFFSET),
        _
        )
      )
      .WillRepeatedly (Return (-1));
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsStatusType (STATUS_DISABLED),
        StrEq ("status"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(StatusDisabledProperty.Length),
           Return ((FDT_PROPERTY *)&StatusDisabledProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        AllOf (IsStatusType (STATUS_MISSING), Not (IsCompatibleType (WRONG_COMPAT))),
        StrEq ("status"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(0),
           ReturnNull ()
           )
         );
    // Have wrong version return disabled for both to be able to test 1 node path
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        AllOf (IsStatusType (STATUS_MISSING), IsCompatibleType (WRONG_COMPAT)),
        StrEq ("status"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(StatusDisabledProperty.Length),
           Return ((FDT_PROPERTY *)&StatusDisabledProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsStatusType (STATUS_OKAY),
        StrEq ("status"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(StatusOkayProperty.Length),
           Return ((FDT_PROPERTY *)&StatusOkayProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsStatusType (STATUS_ERROR),
        StrEq ("status"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(-1),
           Return ((FDT_PROPERTY *)&StatusOkayProperty)
           )
         );
  }
};

constexpr FDT_PROPERTY_LEN_100  DeviceEnumeration::StatusOkayProperty;
constexpr FDT_PROPERTY_LEN_100  DeviceEnumeration::StatusDisabledProperty;

class DeviceEnumerationCompatible : public  DeviceEnumeration,
  public  testing::WithParamInterface<int> {
protected:

  constexpr static FDT_PROPERTY_LEN_100 SingleCompatibilityProperty  = { 0, sizeof (SINGLE_COMPAT_STRING), 0, SINGLE_COMPAT_STRING };
  constexpr static FDT_PROPERTY_LEN_100 SingleCompatibility2Property = { 0, sizeof (SINGLE_COMPAT_STRING2), 0, SINGLE_COMPAT_STRING2 };
  constexpr static FDT_PROPERTY_LEN_100 DualCompatibilityProperty    = { 0, sizeof (DUAL_COMPAT_STRING), 0, DUAL_COMPAT_STRING };
  constexpr static FDT_PROPERTY_LEN_100 WrongCompatibilityProperty   = { 0, sizeof (WRONG_COMPAT_STRING), 0, WRONG_COMPAT_STRING };

  void
  SetUp (
    ) override
  {
    DeviceEnumeration::SetUp ();
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (SINGLE_COMPAT),
        StrEq ("compatible"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(SingleCompatibilityProperty.Length),
           Return ((FDT_PROPERTY *)&SingleCompatibilityProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (SINGLE_COMPAT2),
        StrEq ("compatible"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(SingleCompatibility2Property.Length),
           Return ((FDT_PROPERTY *)&SingleCompatibility2Property)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (DUAL_COMPAT),
        StrEq ("compatible"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(DualCompatibilityProperty.Length),
           Return ((FDT_PROPERTY *)&DualCompatibilityProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (WRONG_COMPAT),
        StrEq ("compatible"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(WrongCompatibilityProperty.Length),
           Return ((FDT_PROPERTY *)&WrongCompatibilityProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (MISSING_COMPAT),
        StrEq ("compatible"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(0),
           ReturnNull ()
           )
         );
  }
};

constexpr FDT_PROPERTY_LEN_100  DeviceEnumerationCompatible::SingleCompatibilityProperty;
constexpr FDT_PROPERTY_LEN_100  DeviceEnumerationCompatible::SingleCompatibility2Property;
constexpr FDT_PROPERTY_LEN_100  DeviceEnumerationCompatible::DualCompatibilityProperty;
constexpr FDT_PROPERTY_LEN_100  DeviceEnumerationCompatible::WrongCompatibilityProperty;

TEST_P (DeviceEnumerationCompatible, GetNextCompatibleNode) {
  INT32        NodeOffset;
  UINT32       NodeCount;
  UINT32       ExpectedNodeCount;
  CONST CHAR8  **CompatibleInfo;
  INT32        Match1;
  INT32        Match2;
  INT32        Match3;

  switch (GetParam ()) {
    case 0:
      CompatibleInfo = Device1CompatInfo;
      // 2 matching types and 2 matching statuses
      ExpectedNodeCount = 4;
      Match1            = SINGLE_COMPAT;
      Match2            = DUAL_COMPAT;
      Match3            = -1;
      break;
    case 1:
      CompatibleInfo = Device2CompatInfo;
      // 2 matching types and 2 matching statuses
      ExpectedNodeCount = 4;
      Match1            = SINGLE_COMPAT2;
      Match2            = DUAL_COMPAT;
      Match3            = -1;
      break;
    case 2:
      CompatibleInfo = DeviceBothCompatInfo;
      // 3 matching types and 2 matching statuses
      ExpectedNodeCount = 6;
      Match1            = SINGLE_COMPAT;
      Match2            = SINGLE_COMPAT2;
      Match3            = DUAL_COMPAT;
      break;
    case 3:
      CompatibleInfo = DeviceMissingCompatInfo;
      // 0 matching types and 2 matching statuses
      ExpectedNodeCount = 0;
      Match1            = -1;
      Match2            = -1;
      Match3            = -1;
      break;
    default:
      ASSERT_LE (GetParam (), 3);
      return;
  }

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNextCompatibleNode (NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNextCompatibleNode (CompatibleInfo, NULL));
  NodeOffset = -1;
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNextCompatibleNode (NULL, &NodeOffset));

  NodeOffset = -1;
  NodeCount  = 0;
  while (TRUE) {
    Status = DeviceTreeGetNextCompatibleNode (CompatibleInfo, &NodeOffset);
    if (EFI_ERROR (Status)) {
      break;
    }

    EXPECT_THAT (NodeOffset, AnyOf (IsCompatibleType (Match1), IsCompatibleType (Match2), IsCompatibleType (Match3)));
    EXPECT_THAT (NodeOffset, AnyOf (IsStatusType (STATUS_OKAY), IsStatusType (STATUS_MISSING)));

    NodeCount++;
    ASSERT_LE (NodeOffset, TEST_MAX_OFFSET+1);
  }

  EXPECT_EQ (NodeCount, ExpectedNodeCount);
  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

TEST_P (DeviceEnumerationCompatible, GetCompatibleNodeCount) {
  UINT32       NodeCount;
  UINT32       ExpectedNodeCount;
  CONST CHAR8  **CompatibleInfo;

  switch (GetParam ()) {
    case 0:
      CompatibleInfo = Device1CompatInfo;
      // 2 matching types and 2 matching statuses
      ExpectedNodeCount = 4;
      ExpectedStatus    = EFI_SUCCESS;
      break;
    case 1:
      CompatibleInfo = Device2CompatInfo;
      // 2 matching types and 2 matching statuses
      ExpectedNodeCount = 4;
      ExpectedStatus    = EFI_SUCCESS;
      break;
    case 2:
      CompatibleInfo = DeviceBothCompatInfo;
      // 3 matching types and 2 matching statuses
      ExpectedNodeCount = 6;
      ExpectedStatus    = EFI_SUCCESS;
      break;
    case 3:
      CompatibleInfo = DeviceMissingCompatInfo;
      // 0 matching types and 2 matching statuses
      ExpectedNodeCount = 0;
      ExpectedStatus    = EFI_NOT_FOUND;
      break;
    default:
      ASSERT_LE (GetParam (), 3);
      return;
  }

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetCompatibleNodeCount (NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetCompatibleNodeCount (CompatibleInfo, NULL));
  NodeCount = 0;
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetCompatibleNodeCount (NULL, &NodeCount));

  NodeCount = 0;
  EXPECT_EQ (ExpectedStatus, DeviceTreeGetCompatibleNodeCount (CompatibleInfo, &NodeCount));
  EXPECT_EQ (NodeCount, ExpectedNodeCount);
}

#ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS
TEST_P (DeviceEnumerationCompatible, GetMatchingEnabledDeviceTreeNodes) {
  CONST CHAR8  *CompatibleString;
  INT32        NodeOffset;
  UINT32       NodeCount;
  UINT32       ExpectedNodeCount;
  UINT32       TestedNodeCount;
  UINT32       *HandleArray;
  INT32        Match1;
  INT32        Match2;
  VOID         *DeviceTreeBase;
  UINT32       TestHandle;

  switch (GetParam ()) {
    case 0:
      CompatibleString = SINGLE_COMPAT_STRING;
      // 2 matching types and 2 matching statuses
      ExpectedNodeCount = 4;
      Match1            = SINGLE_COMPAT;
      Match2            = DUAL_COMPAT;
      break;
    case 1:
      CompatibleString = SINGLE_COMPAT_STRING2;
      // 2 matching types and 2 matching statuses
      ExpectedNodeCount = 4;
      Match1            = SINGLE_COMPAT2;
      Match2            = DUAL_COMPAT;
      break;
    case 2:
      CompatibleString = WRONG_COMPAT_STRING;
      // 1 matching types and 1 matching statuses
      ExpectedNodeCount = 1;
      Match1            = WRONG_COMPAT;
      Match2            = -1;
      break;
    case 3:
      CompatibleString = WRONG_COMPAT_STRING2;
      // 0 matching types and 2 matching statuses
      ExpectedNodeCount = 0;
      Match1            = -1;
      Match2            = -1;
      break;
    default:
      ASSERT_LE (GetParam (), 3);
      return;
  }

  HandleArray = NULL;

  EXPECT_EQ (EFI_INVALID_PARAMETER, GetMatchingEnabledDeviceTreeNodes (NULL, NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetMatchingEnabledDeviceTreeNodes (CompatibleString, NULL, NULL));
  NodeCount = 0;
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetMatchingEnabledDeviceTreeNodes (NULL, NULL, &NodeCount));
  NodeCount = 1;
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetMatchingEnabledDeviceTreeNodes (CompatibleString, NULL, &NodeCount));

  NodeCount = 0;
  Status    = GetMatchingEnabledDeviceTreeNodes (CompatibleString, NULL, &NodeCount);
  if (ExpectedNodeCount == 0) {
    EXPECT_EQ (Status, EFI_NOT_FOUND);
    // Force a buffer of 1 for NodeCount to test that it returns not_found as well
    NodeCount   = 1;
    HandleArray = (UINT32 *)malloc (sizeof (UINT32));
    ASSERT_NE (HandleArray, nullptr);

    EXPECT_EQ (EFI_NOT_FOUND, GetMatchingEnabledDeviceTreeNodes (CompatibleString, HandleArray, &NodeCount));
  } else {
    EXPECT_EQ (Status, EFI_BUFFER_TOO_SMALL);
    EXPECT_EQ (NodeCount, ExpectedNodeCount);
    HandleArray = (UINT32 *)malloc (NodeCount * sizeof (UINT32));
    ASSERT_NE (HandleArray, nullptr);
    EXPECT_EQ (EFI_SUCCESS, GetMatchingEnabledDeviceTreeNodes (CompatibleString, HandleArray, &NodeCount));
    EXPECT_EQ (NodeCount, ExpectedNodeCount);

    for (unsigned int Index = 0; Index < NodeCount; Index++) {
      EXPECT_EQ (EFI_SUCCESS, GetDeviceTreeNode (HandleArray[Index], &DeviceTreeBase, &NodeOffset));
      EXPECT_EQ (DeviceTreeBase, TEST_PLATFORM_DEVICE_TREE_ADDRESS);
      EXPECT_THAT (NodeOffset, AnyOf (IsCompatibleType (Match1), IsCompatibleType (Match2)));
      EXPECT_THAT (NodeOffset, AnyOf (IsStatusType (STATUS_OKAY), IsStatusType (STATUS_MISSING)));

      EXPECT_EQ (EFI_SUCCESS, GetDeviceTreeHandle (DeviceTreeBase, NodeOffset, &TestHandle));
      EXPECT_EQ (TestHandle, HandleArray[Index]);
    }

    // Check that return on too small fills out data
    memset (HandleArray, 0xFF, NodeCount * sizeof (UINT32));
    TestedNodeCount = NodeCount - 1;
    NodeCount       = TestedNodeCount;
    EXPECT_EQ (EFI_BUFFER_TOO_SMALL, GetMatchingEnabledDeviceTreeNodes (CompatibleString, HandleArray, &NodeCount));
    EXPECT_EQ (NodeCount, ExpectedNodeCount);

    for (unsigned int Index = 0; Index < TestedNodeCount; Index++) {
      EXPECT_EQ (EFI_SUCCESS, GetDeviceTreeNode (HandleArray[Index], &DeviceTreeBase, &NodeOffset));
      EXPECT_EQ (DeviceTreeBase, TEST_PLATFORM_DEVICE_TREE_ADDRESS);
      EXPECT_THAT (NodeOffset, AnyOf (IsCompatibleType (Match1), IsCompatibleType (Match2)));
      EXPECT_THAT (NodeOffset, AnyOf (IsStatusType (STATUS_OKAY), IsStatusType (STATUS_MISSING)));

      EXPECT_EQ (EFI_SUCCESS, GetDeviceTreeHandle (DeviceTreeBase, NodeOffset, &TestHandle));
      EXPECT_EQ (TestHandle, HandleArray[Index]);
    }
  }

  if (HandleArray != NULL) {
    free (HandleArray);
    HandleArray = NULL;
  }
}
#endif

INSTANTIATE_TEST_SUITE_P (
  DeviceEnumerationCompatibleTests,
  DeviceEnumerationCompatible,
  testing::Range (0, 4)
  );

class DeviceEnumerationType : public  DeviceEnumeration {
protected:
  constexpr static FDT_PROPERTY_LEN_100 CpuTypeProperty    = { 0, sizeof (CPU_TYPE_STRING), 0, CPU_TYPE_STRING };
  constexpr static FDT_PROPERTY_LEN_100 MemoryTypeProperty = { 0, sizeof (MEMORY_TYPE_STRING), 0, MEMORY_TYPE_STRING };
  constexpr static FDT_PROPERTY_LEN_100 WrongTypeProperty  = { 0, sizeof (WRONG_TYPE_STRING), 0, WRONG_TYPE_STRING };

  void
  SetUp (
    ) override
  {
    DeviceEnumeration::SetUp ();
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (CPU_TYPE),
        StrEq ("device_type"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(CpuTypeProperty.Length),
           Return ((FDT_PROPERTY *)&CpuTypeProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (MEMORY_TYPE),
        StrEq ("device_type"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(MemoryTypeProperty.Length),
           Return ((FDT_PROPERTY *)&MemoryTypeProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (WRONG_TYPE),
        StrEq ("device_type"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(WrongTypeProperty.Length),
           Return ((FDT_PROPERTY *)&WrongTypeProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        AnyOf (IsCompatibleType (MISSING_TYPE), IsCompatibleType (MISSING_TYPE2)),
        StrEq ("device_type"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(0),
           ReturnNull ()
           )
         );
  }
};

constexpr FDT_PROPERTY_LEN_100  DeviceEnumerationType::CpuTypeProperty;
constexpr FDT_PROPERTY_LEN_100  DeviceEnumerationType::MemoryTypeProperty;
constexpr FDT_PROPERTY_LEN_100  DeviceEnumerationType::WrongTypeProperty;

TEST_F (DeviceEnumerationType, GetNextCpuNode) {
  INT32   NodeOffset;
  UINT32  NodeCount;
  UINT32  ExpectedNodeCount;

  // 1 matching types and 2 matching statuses
  ExpectedNodeCount = 2;

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNextCpuNode (NULL));

  NodeOffset = -1;
  NodeCount  = 0;
  while (TRUE) {
    Status = DeviceTreeGetNextCpuNode (&NodeOffset);
    if (EFI_ERROR (Status)) {
      break;
    }

    EXPECT_THAT (NodeOffset, IsCompatibleType (CPU_TYPE));
    EXPECT_THAT (NodeOffset, AnyOf (IsStatusType (STATUS_OKAY), IsStatusType (STATUS_MISSING)));

    NodeCount++;
    ASSERT_LE (NodeOffset, TEST_MAX_OFFSET+1);
  }

  EXPECT_EQ (NodeCount, ExpectedNodeCount);
  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

TEST_F (DeviceEnumerationType, GetCpuNodeCount) {
  UINT32  NodeCount;
  UINT32  ExpectedNodeCount;

  // 1 matching types and 2 matching statuses
  ExpectedNodeCount = 2;

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetCpuNodeCount (NULL));

  NodeCount = 0;
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetCpuNodeCount (&NodeCount));
  EXPECT_EQ (NodeCount, ExpectedNodeCount);
}

TEST_F (DeviceEnumerationType, GetNextMemoryNode) {
  INT32   NodeOffset;
  UINT32  NodeCount;
  UINT32  ExpectedNodeCount;

  // 1 matching types and 2 matching statuses
  ExpectedNodeCount = 2;

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNextCpuNode (NULL));

  NodeOffset = -1;
  NodeCount  = 0;
  while (TRUE) {
    Status = DeviceTreeGetNextMemoryNode (&NodeOffset);
    if (EFI_ERROR (Status)) {
      break;
    }

    EXPECT_THAT (NodeOffset, IsCompatibleType (MEMORY_TYPE));
    EXPECT_THAT (NodeOffset, AnyOf (IsStatusType (STATUS_OKAY), IsStatusType (STATUS_MISSING)));

    NodeCount++;
    ASSERT_LE (NodeOffset, TEST_MAX_OFFSET+1);
  }

  EXPECT_EQ (NodeCount, ExpectedNodeCount);
  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

TEST_F (DeviceEnumerationType, GetMemoryNodeCount) {
  UINT32  NodeCount;
  UINT32  ExpectedNodeCount;

  // 1 matching types and 2 matching statuses
  ExpectedNodeCount = 2;

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetMemoryNodeCount (NULL));

  NodeCount = 0;
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetMemoryNodeCount (&NodeCount));
  EXPECT_EQ (NodeCount, ExpectedNodeCount);
}

class DeviceEnumerationNoType : public  DeviceEnumeration {
protected:
  constexpr static FDT_PROPERTY_LEN_100 WrongTypeProperty = { 0, sizeof (WRONG_TYPE_STRING), 0, WRONG_TYPE_STRING };

  void
  SetUp (
    ) override
  {
    DeviceEnumeration::SetUp ();
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        _,
        StrEq ("device_type"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(0),
           ReturnNull ()
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        IsCompatibleType (WRONG_TYPE),
        StrEq ("device_type"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(WrongTypeProperty.Length),
           Return ((FDT_PROPERTY *)&WrongTypeProperty)
           )
         );
  }
};

constexpr FDT_PROPERTY_LEN_100  DeviceEnumerationNoType::WrongTypeProperty;

TEST_F (DeviceEnumerationNoType, GetTypeCountExpect0) {
  UINT32  NodeCount;

  NodeCount = MAX_UINT32;
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetMemoryNodeCount (&NodeCount));
  EXPECT_EQ (NodeCount, (UINT32)0);
  NodeCount = MAX_UINT32;
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetCpuNodeCount (&NodeCount));
  EXPECT_EQ (NodeCount, (UINT32)0);
}

class DevicePHandle : public  DeviceEnumeration {
protected:

  constexpr static FDT_PROPERTY_LEN_100 PHandle1Property = { 0, sizeof (UINT32), 0, { 0x00, 0x00, 0x00, 0x01 }
  };
  constexpr static FDT_PROPERTY_LEN_100 PHandle2Property = { 0, sizeof (UINT32), 0, { 0x00, 0x00, 0x00, 0x02 }
  };
  constexpr static FDT_PROPERTY_LEN_100 PHandle3Property = { 0, sizeof (UINT32), 0, { 0x00, 0x00, 0x00, 0x03 }
  };

  void
  SetUp (
    ) override
  {
    DeviceEnumeration::SetUp ();
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        _,
        NotNull (),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(0),
           ReturnNull ()
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        4,
        StrEq ("phandle"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(PHandle1Property.Length),
           Return ((FDT_PROPERTY *)&PHandle1Property)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        6,
        StrEq ("phandle"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(PHandle2Property.Length),
           Return ((FDT_PROPERTY *)&PHandle2Property)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        8,
        StrEq ("linux,phandle"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(PHandle3Property.Length),
           Return ((FDT_PROPERTY *)&PHandle3Property)
           )
         );
  }
};

constexpr FDT_PROPERTY_LEN_100  DevicePHandle::PHandle1Property;
constexpr FDT_PROPERTY_LEN_100  DevicePHandle::PHandle2Property;
constexpr FDT_PROPERTY_LEN_100  DevicePHandle::PHandle3Property;

TEST_F (DevicePHandle, GetNodeByPHandle) {
  INT32  NodeOffset;

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodeByPHandle (0, NULL));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPHandle (0, &NodeOffset));
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPHandle (1, &NodeOffset));
  EXPECT_EQ (NodeOffset, 4);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPHandle (2, &NodeOffset));
  EXPECT_EQ (NodeOffset, 6);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPHandle (3, &NodeOffset));
  EXPECT_EQ (NodeOffset, 8);
}

TEST_F (DevicePHandle, GetNodePHandle) {
  UINT32  NodePHandle;

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodePHandle (0, NULL));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodePHandle (0, &NodePHandle));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodePHandle (1, &NodePHandle));
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodePHandle (4, &NodePHandle));
  EXPECT_EQ (NodePHandle, (UINT32)1);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodePHandle (6, &NodePHandle));
  EXPECT_EQ (NodePHandle, (UINT32)2);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodePHandle (8, &NodePHandle));
  EXPECT_EQ (NodePHandle, (UINT32)3);
}

#define ALIAS1_NAME  "al1"
#define ALIAS1_PATH  "/level0_0/level1_1"
#define ALIAS2_NAME  "al2"
#define ALIAS2_PATH  "/level0_1"
#define ALIAS3_NAME  "al3"
#define ALIAS3_PATH  "/level0_2"
class DevicePath : public  DeviceTreeHelperPlatform {
protected:
  constexpr static FDT_PROPERTY_LEN_100 alias1Property = { 0, sizeof (ALIAS1_PATH), 0, ALIAS1_PATH };
  constexpr static FDT_PROPERTY_LEN_100 alias2Property = { 0, sizeof (ALIAS2_PATH), 0, ALIAS2_PATH };
  constexpr static FDT_PROPERTY_LEN_100 alias3Property = { 0, sizeof (ALIAS3_PATH), 0, ALIAS3_PATH };
};

constexpr FDT_PROPERTY_LEN_100  DevicePath::alias1Property;
constexpr FDT_PROPERTY_LEN_100  DevicePath::alias2Property;
constexpr FDT_PROPERTY_LEN_100  DevicePath::alias3Property;

typedef struct {
  INT32          ParentOffset;
  CONST CHAR8    *Name;
  INT32          SubNode;
} SUBNODE_NAME_OFFSET;

#define ALIAS_OFFSET  15
SUBNODE_NAME_OFFSET  SubNodeOffsets[] = {
  { 0,  "level0_0", 1            },
  { 0,  "level0_1", 2            },
  { 1,  "level1_0", 3            },
  { 1,  "level1_1", 4            },
  { 2,  "level1_0", 5            },
  { 2,  "level1_1", 6            },
  { 3,  "level2_0", 7            },
  { 3,  "level2_1", 8            },
  { 4,  "level2_0", 9            },
  { 4,  "level2_1", 10           },
  { 5,  "level2_0", 11           },
  { 5,  "level2_1", 12           },
  { 6,  "level2_0", 13           },
  { 6,  "level2_1", 14           },
  { 0,  "aliases",  ALIAS_OFFSET },
  { -1, NULL,       -1           }
};

INT32
EFIAPI
TestSubnodeOffsetNameLen (
  IN CONST VOID   *Fdt,
  IN INT32        ParentOffset,
  IN CONST CHAR8  *Name,
  IN INT32        NameLength
  )
{
  UINT32  Index;

  if (Fdt == NULL) {
    return -1;
  }

  Index = 0;
  while (SubNodeOffsets[Index].ParentOffset != -1) {
    if (SubNodeOffsets[Index].ParentOffset == ParentOffset) {
      if ((strlen (SubNodeOffsets[Index].Name) == (UINT32)NameLength) &&
          (strncmp (SubNodeOffsets[Index].Name, Name, NameLength) == 0))
      {
        return SubNodeOffsets[Index].SubNode;
      }
    }

    Index++;
  }

  return -1;
}

TEST_F (DevicePath, GetNodeByPath) {
  INT32  NodeOffset;

  EXPECT_CALL (
    FdtMock,
    FdtSubnodeOffsetNameLen (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      _,
      NotNull (),
      Gt (0)
      )
    )
    .WillRepeatedly (Invoke (TestSubnodeOffsetNameLen));
  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      ALIAS_OFFSET,
      _,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(0),
         ReturnNull ()
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      ALIAS_OFFSET,
      StrEq (ALIAS1_NAME),
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(alias1Property.Length),
         Return ((FDT_PROPERTY *)&alias1Property)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      ALIAS_OFFSET,
      StrEq (ALIAS2_NAME),
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(alias2Property.Length),
         Return ((FDT_PROPERTY *)&alias2Property)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      ALIAS_OFFSET,
      StrEq (ALIAS3_NAME),
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(alias3Property.Length),
         Return ((FDT_PROPERTY *)&alias3Property)
         )
       );
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodeByPath (NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodeByPath (NULL, &NodeOffset));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodeByPath ("test_path", NULL));

  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("/level0_2", &NodeOffset));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("/level0", &NodeOffset));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("/level0_11", &NodeOffset));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("/level0_1/level1", &NodeOffset));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("/level0_1/level1_2", &NodeOffset));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("/level0_1/level1_01", &NodeOffset));

  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath ("/level0_0", &NodeOffset));
  EXPECT_EQ (NodeOffset, 1);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath ("/level0_0/level1_1", &NodeOffset));
  EXPECT_EQ (NodeOffset, 4);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath ("/level0_0///level1_1", &NodeOffset));
  EXPECT_EQ (NodeOffset, 4);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath ("/level0_1/level1_0/level2_1", &NodeOffset));
  EXPECT_EQ (NodeOffset, 12);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath ("/level0_0///level1_1//", &NodeOffset));
  EXPECT_EQ (NodeOffset, 4);

  // alias based nodes
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("al0", &NodeOffset));
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("al0/foo", &NodeOffset));
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath (ALIAS1_NAME, &NodeOffset));
  EXPECT_EQ (NodeOffset, 4);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath (ALIAS2_NAME, &NodeOffset));
  EXPECT_EQ (NodeOffset, 2);
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath (ALIAS3_NAME, &NodeOffset));
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath ("al1/level2_0", &NodeOffset));
  EXPECT_EQ (NodeOffset, 9);
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath ("al1/level0_0", &NodeOffset));
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeByPath ("al2/level1_1/level2_0", &NodeOffset));
  EXPECT_EQ (NodeOffset, 13);
}

TEST_F (DevicePath, GetNodeByPathNoAlias) {
  INT32  NodeOffset;

  EXPECT_CALL (
    FdtMock,
    FdtSubnodeOffsetNameLen (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      _,
      NotNull (),
      Gt (0)
      )
    )
    .WillRepeatedly (Return (-1));

  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeByPath (ALIAS1_NAME, &NodeOffset));
}

#define TEST_PATH        "/this/is/the/path"
#define TEST_PATH_0      "this"
#define TEST_PATH_1      "is"
#define TEST_PATH_2      "the"
#define TEST_PATH_3      "path"
#define TEST_PATH_DEPTH  4

TEST_F (DevicePath, GetNodePath) {
  CHAR8   *NodePath;
  UINT32  NodePathSize;

  EXPECT_CALL (
    FdtMock,
    FdtGetName (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(sizeof (TEST_PATH_3)-1),
         Return (TEST_PATH_3)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtGetName (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET-1,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(sizeof (TEST_PATH_2)-1),
         Return (TEST_PATH_2)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtGetName (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET-2,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(sizeof (TEST_PATH_1)-1),
         Return (TEST_PATH_1)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtGetName (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET-3,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(sizeof (TEST_PATH_0)-1),
         Return (TEST_PATH_0)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtNextNode (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      0,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(1),
         Return (TEST_NODE_OFFSET-3)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtNextNode (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET-3,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(2),
         Return (TEST_NODE_OFFSET-2)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtNextNode (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET-2,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(3),
         Return (TEST_NODE_OFFSET-1)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtNextNode (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET-1,
      NotNull ()
      )
    )
    .WillRepeatedly (
       DoAll (
         SetArgPointee<2>(4),
         Return (TEST_NODE_OFFSET)
         )
       );

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodePath (TEST_NODE_OFFSET, NULL, NULL));
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodePath (TEST_NODE_OFFSET, &NodePath, NULL));
  EXPECT_STREQ (TEST_PATH, NodePath);
  NodePathSize = MAX_UINT32;
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodePath (TEST_NODE_OFFSET, &NodePath, &NodePathSize));
  EXPECT_STREQ (TEST_PATH, NodePath);
  EXPECT_EQ (NodePathSize, sizeof (TEST_PATH));
}

#define STRING_LIST  "device0\0device01\0device10\0device1"
class DeviceProperty : public  DeviceTreeHelperPlatform, public  testing::WithParamInterface<int> {
protected:
  constexpr static FDT_PROPERTY_LEN_100 GoodProperty = { 0, 4, 0, { 0x11, 0x22, 0x33, 0x44 }
  };
  constexpr static FDT_PROPERTY_LEN_100 BadProperty = { 0, 4, 0, { 0x55, 0x66, 0x77, 0x00 }
  };
  // Stored in big endian
  constexpr static FDT_PROPERTY_LEN_100 Property64 = { 0, 8, 0, { 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00 }
  };
  constexpr static FDT_PROPERTY_LEN_100 Property64in32 = { 0, 8, 0, { 0x00, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44 }
  };
  constexpr static FDT_PROPERTY_LEN_100 Property32 = { 0, 4, 0, { 0x11, 0x22, 0x33, 0x44 }
  };
  constexpr static FDT_PROPERTY_LEN_100 StringListProperty = { 0, sizeof (STRING_LIST), 0, STRING_LIST };

  void
  SetUp (
    ) override
  {
    DeviceTreeHelperPlatform::SetUp ();
  }
};

constexpr FDT_PROPERTY_LEN_100  DeviceProperty::GoodProperty;
constexpr FDT_PROPERTY_LEN_100  DeviceProperty::BadProperty;
constexpr FDT_PROPERTY_LEN_100  DeviceProperty::Property64;
constexpr FDT_PROPERTY_LEN_100  DeviceProperty::Property64in32;
constexpr FDT_PROPERTY_LEN_100  DeviceProperty::Property32;
constexpr FDT_PROPERTY_LEN_100  DeviceProperty::StringListProperty;

TEST_F (DeviceProperty, GetProperty) {
  UINT32      PropertySize;
  CONST VOID  *PropertyData;

  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET,
      StrEq ("good_property"),
      NotNull ()
      )
    )
    .Times (3)
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(GoodProperty.Length),
         Return ((FDT_PROPERTY *)&GoodProperty)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET,
      StrEq ("missing_property"),
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(0),
         ReturnNull ()
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET,
      StrEq ("bad_property"),
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(-1),
         Return ((FDT_PROPERTY *)&GoodProperty)
         )
       );

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodeProperty (TEST_NODE_OFFSET, NULL, &PropertyData, &PropertySize));

  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeProperty (TEST_NODE_OFFSET, "good_property", NULL, &PropertySize));
  EXPECT_EQ (PropertySize, GoodProperty.Length);

  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeProperty (TEST_NODE_OFFSET, "good_property", &PropertyData, NULL));
  EXPECT_EQ (PropertyData, &GoodProperty.Data);

  EXPECT_EQ (EFI_SUCCESS, DeviceTreeGetNodeProperty (TEST_NODE_OFFSET, "good_property", &PropertyData, &PropertySize));
  EXPECT_EQ (PropertyData, &GoodProperty.Data);
  EXPECT_EQ (PropertySize, GoodProperty.Length);

  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodeProperty (TEST_NODE_OFFSET, "missing_property", &PropertyData, &PropertySize));
  EXPECT_EQ (EFI_DEVICE_ERROR, DeviceTreeGetNodeProperty (TEST_NODE_OFFSET, "bad_property", &PropertyData, &PropertySize));
}

TEST_P (DeviceProperty, GetPropertyValue) {
  UINT64  PropertyValue64;
  UINT32  PropertyValue32;

  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET,
      StrEq ("property64"),
      NotNull ()
      )
    )
    .Times (2)
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(GetParam ()),
         Return ((FDT_PROPERTY *)&Property64)
         )
       );
  if (GetParam () != 8) {
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        TEST_NODE_OFFSET,
        StrEq ("property32"),
        NotNull ()
        )
      )
      .Times (1)
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(GetParam ()),
           Return ((FDT_PROPERTY *)&Property32)
           )
         );
  } else {
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        TEST_NODE_OFFSET,
        StrEq ("property32"),
        NotNull ()
        )
      )
      .Times (1)
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(GetParam ()),
           Return ((FDT_PROPERTY *)&Property64in32)
           )
         );
  }

  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET,
      StrEq ("missing_property"),
      NotNull ()
      )
    )
    .Times (1)
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(0),
         ReturnNull ()
         )
       );

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodePropertyValue64 (TEST_NODE_OFFSET, NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodePropertyValue64 (TEST_NODE_OFFSET, NULL, &PropertyValue64));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodePropertyValue64 (TEST_NODE_OFFSET, "property64", NULL));

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodePropertyValue32 (TEST_NODE_OFFSET, NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodePropertyValue32 (TEST_NODE_OFFSET, NULL, &PropertyValue32));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetNodePropertyValue32 (TEST_NODE_OFFSET, "property64", NULL));

  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeGetNodePropertyValue64 (TEST_NODE_OFFSET, "missing_property", &PropertyValue64));

  if ((GetParam () == 4) || (GetParam () == 8)) {
    ExpectedStatus = EFI_SUCCESS;
  } else {
    ExpectedStatus = EFI_BAD_BUFFER_SIZE;
  }

  EXPECT_EQ (ExpectedStatus, DeviceTreeGetNodePropertyValue64 (TEST_NODE_OFFSET, "property64", &PropertyValue64));
  if (GetParam () == 4) {
    EXPECT_EQ (PropertyValue64, htobe32 (*(UINT32 *)Property64.Data));
  } else if (GetParam () == 8) {
    EXPECT_EQ (PropertyValue64, htobe64 (*(UINT64 *)Property64.Data));
  }

  PropertyValue32 = 0;
  EXPECT_EQ (ExpectedStatus, DeviceTreeGetNodePropertyValue32 (TEST_NODE_OFFSET, "property32", &PropertyValue32));
  if ((GetParam () == 4) || (GetParam () == 8)) {
    EXPECT_EQ (PropertyValue32, htobe32 (*(UINT32 *)Property32.Data));
  }

  if (GetParam () == 4) {
    ExpectedStatus = EFI_SUCCESS;
  } else if (GetParam () == 8) {
    ExpectedStatus = EFI_NO_MAPPING;
  } else {
    ExpectedStatus = EFI_BAD_BUFFER_SIZE;
  }

  PropertyValue32 = 0;
  EXPECT_EQ (ExpectedStatus, DeviceTreeGetNodePropertyValue32 (TEST_NODE_OFFSET, "property64", &PropertyValue32));
  if (GetParam () == 4) {
    EXPECT_EQ (PropertyValue32, htobe32 (*(UINT32 *)Property64.Data));
  }
}

TEST_F (DeviceProperty, LocateStringIndex) {
  UINT32  StringIndex;

  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET,
      StrEq ("string_list"),
      NotNull ()
      )
    )
    .Times (6)
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(StringListProperty.Length),
         Return ((FDT_PROPERTY *)&StringListProperty)
         )
       );

  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_NODE_OFFSET,
      StrEq ("string_list_missing"),
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(0),
         ReturnNull ()
         )
       );

  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, NULL, NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, NULL, "device", NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", "device", NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, NULL, NULL, &StringIndex));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", NULL, &StringIndex));
  EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, NULL, "device", &StringIndex));

  StringIndex = MAX_UINT32;
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", "device", &StringIndex));
  StringIndex = MAX_UINT32;
  EXPECT_EQ (EFI_NOT_FOUND, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", "device02", &StringIndex));
  StringIndex = MAX_UINT32;
  EXPECT_EQ (EFI_NO_MAPPING, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list_missing", "device0", &StringIndex));
  StringIndex = MAX_UINT32;
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", "device0", &StringIndex));
  EXPECT_EQ (StringIndex, (UINT32)0);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", "device01", &StringIndex));
  EXPECT_EQ (StringIndex, (UINT32)1);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", "device10", &StringIndex));
  EXPECT_EQ (StringIndex, (UINT32)2);
  EXPECT_EQ (EFI_SUCCESS, DeviceTreeLocateStringIndex (TEST_NODE_OFFSET, "string_list", "device1", &StringIndex));
  EXPECT_EQ (StringIndex, (UINT32)3);
}

INSTANTIATE_TEST_SUITE_P (
  DevicePropretyValue,
  DeviceProperty,
  testing::Range (0, 13)
  );

#ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS
class KernelAddress : public  DeviceTreeHelperPlatform, public  testing::WithParamInterface<int> {
protected:
  constexpr static FDT_PROPERTY_LEN_100 KernelStartProperty = { 0, sizeof (UINT64), 0, { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 }
  };
  constexpr static FDT_PROPERTY_LEN_100 KernelStartDtbProperty = { 0, sizeof (UINT64), 0, { 0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78 }
  };
};
constexpr FDT_PROPERTY_LEN_100  KernelAddress::KernelStartProperty;
constexpr FDT_PROPERTY_LEN_100  KernelAddress::KernelStartDtbProperty;

TEST_P (KernelAddress, GetKernelAddress) {
  UINT64  KernelStart;
  UINT64  KernelDtbStart;

  if (GetParam () == TEST_NODE_OFFSET) {
    ExpectedStatus = EFI_SUCCESS;
  } else {
    ExpectedStatus = EFI_NOT_FOUND;
  }

  ON_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      AnyOf (TEST_NODE_OFFSET, TEST_NODE_OFFSET+1),
      StrEq ("kernel-start"),
      NotNull ()
      )
    )
    .WillByDefault (
       DoAll (
         SetArgPointee<3>(KernelStartProperty.Length),
         Return ((FDT_PROPERTY *)&KernelStartProperty)
         )
       );
  ON_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      AnyOf (TEST_NODE_OFFSET, TEST_NODE_OFFSET+2),
      StrEq ("kernel-dtb-start"),
      NotNull ()
      )
    )
    .WillByDefault (
       DoAll (
         SetArgPointee<3>(KernelStartDtbProperty.Length),
         Return ((FDT_PROPERTY *)&KernelStartDtbProperty)
         )
       );
  EXPECT_CALL (
    FdtMock,
    FdtSubnodeOffsetNameLen (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      0,
      StartsWith ("chosen"),
      Eq ((INT32)sizeof ("chosen")-1)
      )
    )
    .WillRepeatedly (Return (GetParam ()));

  EXPECT_EQ (EFI_INVALID_PARAMETER, GetKernelAddress (NULL, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetKernelAddress (&KernelStart, NULL));
  EXPECT_EQ (EFI_INVALID_PARAMETER, GetKernelAddress (NULL, &KernelDtbStart));

  EXPECT_EQ (ExpectedStatus, GetKernelAddress (&KernelStart, &KernelDtbStart));
  if (!EFI_ERROR (ExpectedStatus)) {
    EXPECT_EQ (KernelStart, (UINT64)0x0011223344556677);
    EXPECT_EQ (KernelDtbStart, (UINT64)0x0112233445566778);
  }
}
INSTANTIATE_TEST_SUITE_P (
  KernelAddressValues,
  KernelAddress,
  testing::Values (-1, TEST_NODE_OFFSET, TEST_NODE_OFFSET+1, TEST_NODE_OFFSET+2, TEST_NODE_OFFSET+3)
  );

#endif

#define TEST_PARENT_NODE_OFFSET  4
#define REGISTER_NAMES_4         "reg0\0reg10\0reg100\0reg1000"
#define REGISTER_NAMES_1         "reg0"
#define REGISTER_NAMES_2         "reg0\0reg10"
#define REGISTER_NAMES_3         "reg0\0reg10\0reg100"
class DeviceRegisters : public  DeviceTreeHelperPlatform, public  testing::WithParamInterface<::std::tuple<const CHAR8 *, bool, bool, int, int> > {
protected:
  const CHAR8 *test_type;
  bool Address64BBit;
  bool Size64Bit;
  int AddressCells;
  int SizeCells;
  int NumberOfEntries;
  int NumberOfNames;
  int SizeOfEntry;

  FDT_PROPERTY_32 AddressCellsProperty;
  FDT_PROPERTY_32 SizeCellsProperty;
  FDT_PROPERTY_MAX_MEMORY_RANGE RegisterProperty;
  constexpr static FDT_PROPERTY_LEN_100 RegisterNamesProperty = { 0, 0, 0, REGISTER_NAMES_4 };

  EFI_STATUS
  GenericMemoryTest (
    INT32   NodeOffset,
    VOID    *TestData,
    UINT32  *NumberOfEntries
    );

  void
  SetUp (
    ) override
  {
    INT32        NamesSize;
    const CHAR8  *TestProperty;
    CHAR8        NameProperty[32];
    int          NumberOfAddresses;

    DeviceTreeHelperPlatform::SetUp ();
    for (unsigned int Index = 0; Index < (sizeof (RegisterProperty.ValueBigEndian)/sizeof (UINT32)); Index++) {
      RegisterProperty.ValueBigEndian[Index] = htobe32 ((Index+1) | ((Index+2) << 16));
    }

    std::tie (test_type, Address64BBit, Size64Bit, NumberOfEntries, NumberOfNames) = GetParam ();

    TestProperty = test_type;
    if (strcmp (test_type, "reg") == 0) {
      SizeOfEntry       = sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA);
      NumberOfAddresses = 1;
    } else if (strcmp (test_type, "reg_dep") == 0) {
      SizeOfEntry       = sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA);
      TestProperty      = "reg";
      NumberOfAddresses = 1;
    } else {
      SizeOfEntry       = sizeof (NVIDIA_DEVICE_TREE_RANGES_DATA);
      NumberOfAddresses = 2;
    }

    strcpy (NameProperty, TestProperty);
    strcat (NameProperty, "-names");

    switch (NumberOfNames) {
      case 0:
        NamesSize = 0;
        break;
      case 1:
        NamesSize = sizeof (REGISTER_NAMES_1);
        break;
      case 2:
        NamesSize = sizeof (REGISTER_NAMES_2);
        break;
      case 3:
        NamesSize = sizeof (REGISTER_NAMES_3);
        break;
      case 4:
        NamesSize = sizeof (REGISTER_NAMES_4);
        break;
      default:
        NamesSize = 0;
    }

    EXPECT_CALL (
      FdtMock,
      FdtNextNode (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        0,
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<2>(1),
           Return (TEST_PARENT_NODE_OFFSET)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtNextNode (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        TEST_PARENT_NODE_OFFSET,
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<2>(2),
           Return (TEST_NODE_OFFSET)
           )
         );

    AddressCells                        = Address64BBit ? 2 : 1;
    AddressCellsProperty.ValueBigEndian = htobe32 (AddressCells);
    SizeCells                           = Size64Bit ? 2 : 1;
    SizeCellsProperty.ValueBigEndian    = htobe32 (SizeCells);
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        TEST_PARENT_NODE_OFFSET,
        StrEq ("#address-cells"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(sizeof (UINT32)),
           Return ((FDT_PROPERTY *)&AddressCellsProperty)
           )
         );
    EXPECT_CALL (
      FdtMock,
      FdtGetProperty (
        Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
        TEST_PARENT_NODE_OFFSET,
        StrEq ("#size-cells"),
        NotNull ()
        )
      )
      .WillRepeatedly (
         DoAll (
           SetArgPointee<3>(sizeof (UINT32)),
           Return ((FDT_PROPERTY *)&SizeCellsProperty)
           )
         );
    if (NumberOfEntries != 0) {
      EXPECT_CALL (
        FdtMock,
        FdtGetProperty (
          Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
          TEST_NODE_OFFSET,
          StrEq (TestProperty),
          NotNull ()
          )
        )
        .WillRepeatedly (
           DoAll (
             SetArgPointee<3>(sizeof (UINT32) * ((AddressCells * NumberOfAddresses) + SizeCells) * NumberOfEntries),
             Return ((FDT_PROPERTY *)&RegisterProperty)
             )
           );
      if (NamesSize != 0) {
        EXPECT_CALL (
          FdtMock,
          FdtGetProperty (
            Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
            TEST_NODE_OFFSET,
            StrEq (NameProperty),
            NotNull ()
            )
          )
          .WillRepeatedly (
             DoAll (
               SetArgPointee<3>(NamesSize),
               Return ((FDT_PROPERTY *)&RegisterNamesProperty)
               )
             );
      } else {
        EXPECT_CALL (
          FdtMock,
          FdtGetProperty (
            Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
            TEST_NODE_OFFSET,
            StrEq (NameProperty),
            NotNull ()
            )
          )
          .WillRepeatedly (
             ReturnNull ()
             );
      }
    } else {
      EXPECT_CALL (
        FdtMock,
        FdtGetProperty (
          Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
          TEST_NODE_OFFSET,
          StrEq (TestProperty),
          NotNull ()
          )
        )
        .WillRepeatedly (
           ReturnNull ()
           );
    }
  }
};

constexpr FDT_PROPERTY_LEN_100  DeviceRegisters::RegisterNamesProperty;

EFI_STATUS
DeviceRegisters::GenericMemoryTest (
  INT32   NodeOffset,
  VOID    *TestData,
  UINT32  *NumberOfEntries
  )
{
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray;
  NVIDIA_DEVICE_TREE_RANGES_DATA    *RangesArray;
  UINT64                            ExpectedAddress;
  UINT64                            ExpectedParentAddress;
  UINT64                            ExpectedSize;

  if (SizeOfEntry == sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA)) {
    RegisterArray = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)TestData;

    if (strcmp (test_type, "reg") == 0) {
      Status = DeviceTreeGetRegisters (NodeOffset, RegisterArray, NumberOfEntries);
    } else {
      UINT32  Handle;
      Status = GetDeviceTreeHandle (TEST_PLATFORM_DEVICE_TREE_ADDRESS, NodeOffset, &Handle);
      EXPECT_EQ (Status, EFI_SUCCESS);
      Status = GetDeviceTreeRegisters (Handle, RegisterArray, NumberOfEntries);
    }

    if (!EFI_ERROR (Status)) {
      for (unsigned int Index = 0; Index < *NumberOfEntries; Index++) {
        unsigned int  AddressOffset = Index * (AddressCells + SizeCells);
        unsigned int  SizeOffset    = Index * (AddressCells + SizeCells) + AddressCells;
        if (Address64BBit) {
          ExpectedAddress = be64toh (*(UINT64 *)&RegisterProperty.ValueBigEndian[AddressOffset]);
        } else {
          ExpectedAddress = be32toh (RegisterProperty.ValueBigEndian[AddressOffset]);
        }

        EXPECT_EQ (ExpectedAddress, RegisterArray[Index].BaseAddress);
        if (Size64Bit) {
          ExpectedSize = be64toh (*(UINT64 *)&RegisterProperty.ValueBigEndian[SizeOffset]);
        } else {
          ExpectedSize = be32toh (RegisterProperty.ValueBigEndian[SizeOffset]);
        }

        EXPECT_EQ (ExpectedSize, RegisterArray[Index].Size);
        if (Index < (unsigned int)NumberOfNames) {
          switch (Index) {
            case 0:
              EXPECT_STREQ (RegisterArray[Index].Name, "reg0");
              break;
            case 1:
              EXPECT_STREQ (RegisterArray[Index].Name, "reg10");
              break;
            case 2:
              EXPECT_STREQ (RegisterArray[Index].Name, "reg100");
              break;
            case 3:
              EXPECT_STREQ (RegisterArray[Index].Name, "reg1000");
              break;
            default:
              break;
          }
        } else {
          EXPECT_EQ (RegisterArray[Index].Name, nullptr);
        }
      }
    }
  } else if (SizeOfEntry == sizeof (NVIDIA_DEVICE_TREE_RANGES_DATA)) {
    RangesArray = (NVIDIA_DEVICE_TREE_RANGES_DATA *)TestData;

    Status = DeviceTreeGetRanges (NodeOffset, test_type, RangesArray, NumberOfEntries);
    if (!EFI_ERROR (Status)) {
      EXPECT_EQ (EFI_INVALID_PARAMETER, DeviceTreeGetRanges (NodeOffset, NULL, RangesArray, NumberOfEntries));
      for (unsigned int Index = 0; Index < *NumberOfEntries; Index++) {
        unsigned int  AddressOffset       = Index * ((AddressCells*2) + SizeCells);
        unsigned int  AddressParentOffset = Index * ((AddressCells*2) + SizeCells) + AddressCells;
        unsigned int  SizeOffset          = Index * ((AddressCells*2) + SizeCells) + AddressCells*2;
        if (Address64BBit) {
          ExpectedAddress       = be64toh (*(UINT64 *)&RegisterProperty.ValueBigEndian[AddressOffset]);
          ExpectedParentAddress = be64toh (*(UINT64 *)&RegisterProperty.ValueBigEndian[AddressParentOffset]);
        } else {
          ExpectedAddress       = be32toh (RegisterProperty.ValueBigEndian[AddressOffset]);
          ExpectedParentAddress = be32toh (RegisterProperty.ValueBigEndian[AddressParentOffset]);
        }

        EXPECT_EQ (ExpectedAddress, RangesArray[Index].ChildAddress);
        EXPECT_EQ (ExpectedParentAddress, RangesArray[Index].ParentAddress);
        if (Size64Bit) {
          ExpectedSize = be64toh (*(UINT64 *)&RegisterProperty.ValueBigEndian[SizeOffset]);
        } else {
          ExpectedSize = be32toh (RegisterProperty.ValueBigEndian[SizeOffset]);
        }

        EXPECT_EQ (ExpectedSize, RangesArray[Index].Size);
        if (Index < (unsigned int)NumberOfNames) {
          switch (Index) {
            case 0:
              EXPECT_STREQ (RangesArray[Index].Name, "reg0");
              break;
            case 1:
              EXPECT_STREQ (RangesArray[Index].Name, "reg10");
              break;
            case 2:
              EXPECT_STREQ (RangesArray[Index].Name, "reg100");
              break;
            case 3:
              EXPECT_STREQ (RangesArray[Index].Name, "reg1000");
              break;
            default:
              break;
          }
        } else {
          EXPECT_EQ (RangesArray[Index].Name, nullptr);
        }
      }
    }
  }

  return Status;
}

TEST_P (DeviceRegisters, GetRegisters) {
  UINT32  NumberOfRegisters;
  VOID    *TestData = NULL;

  EXPECT_EQ (EFI_INVALID_PARAMETER, GenericMemoryTest (TEST_NODE_OFFSET, NULL, NULL));
  NumberOfRegisters = 1;
  EXPECT_EQ (EFI_INVALID_PARAMETER, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));

  NumberOfRegisters = 0;
  if (NumberOfEntries != 0) {
    EXPECT_EQ (EFI_BUFFER_TOO_SMALL, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));
    EXPECT_EQ (NumberOfRegisters, (UINT32)NumberOfEntries);

    TestData = malloc (SizeOfEntry * NumberOfRegisters);
    ASSERT_NE (TestData, nullptr);

    NumberOfRegisters--;
    EXPECT_EQ (EFI_BUFFER_TOO_SMALL, GenericMemoryTest (TEST_NODE_OFFSET, TestData, &NumberOfRegisters));
    EXPECT_EQ (NumberOfRegisters, (UINT32)NumberOfEntries);
    EXPECT_EQ (EFI_SUCCESS, GenericMemoryTest (TEST_NODE_OFFSET, TestData, &NumberOfRegisters));
  } else {
    EXPECT_EQ (EFI_NOT_FOUND, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));
  }

  SizeCellsProperty.ValueBigEndian = htobe32 (3);
  NumberOfRegisters                = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));
  SizeCellsProperty.ValueBigEndian = htobe32 (0);
  NumberOfRegisters                = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));
  SizeCellsProperty.ValueBigEndian    = htobe32 (SizeCells);
  AddressCellsProperty.ValueBigEndian = htobe32 (3);
  NumberOfRegisters                   = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));
  AddressCellsProperty.ValueBigEndian = htobe32 (0);
  NumberOfRegisters                   = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));
  AddressCellsProperty.ValueBigEndian = htobe32 (AddressCells);

  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_PARENT_NODE_OFFSET,
      StrEq ("#address-cells"),
      NotNull ()
      )
    )
    .WillOnce (
       ReturnNull ()
       )
    .RetiresOnSaturation ();
  NumberOfRegisters = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));
  EXPECT_CALL (
    FdtMock,
    FdtGetProperty (
      Eq (TEST_PLATFORM_DEVICE_TREE_ADDRESS),
      TEST_PARENT_NODE_OFFSET,
      StrEq ("#size-cells"),
      NotNull ()
      )
    )
    .WillOnce (
       ReturnNull ()
       )
    .RetiresOnSaturation ();
  NumberOfRegisters = 0;
  EXPECT_EQ (EFI_DEVICE_ERROR, GenericMemoryTest (TEST_NODE_OFFSET, NULL, &NumberOfRegisters));

  if (TestData != NULL) {
    free (TestData);
  }
}

INSTANTIATE_TEST_SUITE_P (
  DeviceRegistersValues,
  DeviceRegisters,
  Combine (Values ("reg", "reg_dep", "ranges"), Bool (), Bool (), Range (0, MAX_REGISTER_NUMS+1), Range (0, MAX_REGISTER_NUMS+1))
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
