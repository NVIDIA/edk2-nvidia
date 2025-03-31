/** @file
  Unit tests for the implementation of MpCoreInfoLib.

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockHobLib.h>
#include <string.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/MpCoreInfoLib.h>
  #include <Pi/PiHob.h>
  #include <Guid/ArmMpCoreInfo.h>
  #include "../MpCoreInfoLib_private.h"
}

using namespace testing;

// Info for creating core info
#define NUM_SOCKETS  4UL                                            // How many sockets should be created
#define NUM_CLUSTERS(Socket)                (Socket+1)              // How many clusters should be created per socket
#define NUM_CORES(Socket, Cluster)          (Socket+Cluster+1)      // How many cores should be created per cluster
#define NUM_THREADS(Socket, Cluster, Core)  (Socket+Cluster+Core+1) // How many threads should be created per core
#define GET_AFFINITY_BASED_MPID(Aff3, Aff2, Aff1, Aff0)         \
  (((UINT64)(Aff3) << 32) | ((Aff2) << 16) | ((Aff1) << 8) | (Aff0))

//////////////////////////////////////////////////////////////////////////////
class MpCoreInfoLibTest : public  Test {
public:
  MpCoreInfoLibTest(
                    );  // This is the constructor
  ~MpCoreInfoLibTest(
                     );  // This is the deconstructor
protected:
  MockHobLib HobMock;
  EFI_STATUS Status;
  UINT32 NumCores;
  EFI_HOB_GUID_TYPE *MpCoreHobData;
  ARM_CORE_INFO *MpCoreInfo;
};

MpCoreInfoLibTest::MpCoreInfoLibTest(
                                     void
                                     )
{
  UINT32  SocketId;
  UINT32  ClusterId;
  UINT32  CoreId;
  UINT32  ThreadId;

  NumCores = 0;
  for (SocketId = 0; SocketId < NUM_SOCKETS; SocketId++) {
    for (ClusterId = 0; ClusterId < NUM_CLUSTERS (SocketId); ClusterId++) {
      for (CoreId = 0; CoreId < NUM_CORES (SocketId, ClusterId); CoreId++) {
        for (ThreadId = 0; ThreadId < NUM_THREADS (SocketId, ClusterId, CoreId); ThreadId++) {
          NumCores++;
        }
      }
    }
  }

  MpCoreHobData                   = (EFI_HOB_GUID_TYPE *)malloc (sizeof (EFI_HOB_GUID_TYPE) + sizeof (ARM_CORE_INFO) * NumCores);
  MpCoreHobData->Header.HobType   = EFI_HOB_TYPE_GUID_EXTENSION;
  MpCoreHobData->Header.HobLength = sizeof (EFI_HOB_GUID_TYPE) + sizeof (ARM_CORE_INFO) * NumCores;
  memcpy (&MpCoreHobData->Name, &gArmMpCoreInfoGuid, sizeof (EFI_GUID));
  MpCoreInfo = (ARM_CORE_INFO *)GET_GUID_HOB_DATA (MpCoreHobData);
  NumCores   = 0;
  for (SocketId = 0; SocketId < NUM_SOCKETS; SocketId++) {
    for (ClusterId = 0; ClusterId < NUM_CLUSTERS (SocketId); ClusterId++) {
      for (CoreId = 0; CoreId < NUM_CORES (SocketId, ClusterId); CoreId++) {
        for (ThreadId = 0; ThreadId < NUM_THREADS (SocketId, ClusterId, CoreId); ThreadId++) {
          MpCoreInfo[NumCores].Mpidr = GET_AFFINITY_BASED_MPID (SocketId, ClusterId, CoreId, ThreadId);
          NumCores++;
        }
      }
    }
  }

  Status = EFI_SUCCESS;
  MpCoreInfoLibResetModule ();
}

MpCoreInfoLibTest::~MpCoreInfoLibTest(
                                      void
                                      )
{
  free (MpCoreHobData);
  MpCoreHobData = NULL;
}

// Test MpCoreInfoGetProcessorIdFromIndex() API to verify error when pointer is
// NULL.
TEST_F (MpCoreInfoLibTest, GetProcessorIdInvalid) {
  Status = MpCoreInfoGetProcessorIdFromIndex (0, NULL);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

// Test MpCoreInfoGetProcessorIdFromIndex() API to verify error when hob is
// not found.
TEST_F (MpCoreInfoLibTest, GetProcessorIdNoHob) {
  UINT64  ProcessorId;

  EXPECT_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    );

  Status = MpCoreInfoGetProcessorIdFromIndex (0, &ProcessorId);
  EXPECT_EQ (Status, EFI_DEVICE_ERROR);
}

// Test MpCoreInfoGetProcessorIdFromIndex() API to verify correct behavior.
TEST_F (MpCoreInfoLibTest, GetProcessorIdNormal) {
  UINT64  ProcessorId;
  UINT32  Index;

  EXPECT_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    )
    .WillRepeatedly (Return (MpCoreHobData));

  Index = 0;
  while (TRUE) {
    Status = MpCoreInfoGetProcessorIdFromIndex (Index, &ProcessorId);
    if (EFI_ERROR (Status)) {
      break;
    }

    EXPECT_EQ (Status, EFI_SUCCESS);
    EXPECT_EQ (ProcessorId, MpCoreInfo[Index].Mpidr);
    Index++;
  }

  EXPECT_EQ (Status, EFI_NOT_FOUND);
  EXPECT_EQ (Index, NumCores);
}

// Test MpCoreInfoIsProcessorEnabled() API to verify error when hob is
// not found.
TEST_F (MpCoreInfoLibTest, IsProcessorEnabledNoHob) {
  EXPECT_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    );

  Status = MpCoreInfoIsProcessorEnabled (MpCoreInfo[0].Mpidr);
  EXPECT_EQ (Status, EFI_DEVICE_ERROR);
}

// Test IsProcessorEnabled() API for success
TEST_F (MpCoreInfoLibTest, IsProcessorEnabled) {
  EXPECT_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    )
    .WillRepeatedly (Return (MpCoreHobData));

  Status = MpCoreInfoIsProcessorEnabled (MpCoreInfo[0].Mpidr);
  EXPECT_EQ (Status, EFI_SUCCESS);
}

// Test IsProcessorEnabled() API for not found
TEST_F (MpCoreInfoLibTest, IsProcessorEnabledFailure) {
  EXPECT_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    )
    .WillRepeatedly (Return (MpCoreHobData));

  Status = MpCoreInfoIsProcessorEnabled (GET_AFFINITY_BASED_MPID (NUM_SOCKETS, 0, 0, 0));
  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

// Test GetProcessorLocation() API
TEST_F (MpCoreInfoLibTest, GetProcessorLocation) {
  UINT32  TestSocket;
  UINT32  TestCluster;
  UINT32  TestCore;
  UINT32  TestThread;
  UINT64  TestProcessorId;

  UINT32  Socket;
  UINT32  Cluster;
  UINT32  Core;
  UINT32  Thread;

  ON_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    )
    .WillByDefault (Return (MpCoreHobData));

  TestSocket  = NUM_SOCKETS-1;
  TestCluster = NUM_CLUSTERS (TestSocket)-1;
  TestCore    = NUM_CORES (TestSocket, TestCluster) - 1;
  TestThread  = 0;

  TestProcessorId = GET_AFFINITY_BASED_MPID (TestSocket, TestCluster, TestCore, TestThread);

  Status = MpCoreInfoGetProcessorLocation (
             TestProcessorId,
             NULL,
             NULL,
             NULL,
             NULL
             );
  EXPECT_EQ (Status, EFI_SUCCESS);

  Socket = MAX_UINT32;
  Status = MpCoreInfoGetProcessorLocation (
             TestProcessorId,
             &Socket,
             NULL,
             NULL,
             NULL
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Socket, TestSocket);

  Cluster = MAX_UINT32;
  Status  = MpCoreInfoGetProcessorLocation (
              TestProcessorId,
              NULL,
              &Cluster,
              NULL,
              NULL
              );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Cluster, TestCluster);

  Core   = MAX_UINT32;
  Status = MpCoreInfoGetProcessorLocation (
             TestProcessorId,
             NULL,
             NULL,
             &Core,
             NULL
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Core, TestCore);

  Thread = MAX_UINT32;
  Status = MpCoreInfoGetProcessorLocation (
             TestProcessorId,
             NULL,
             NULL,
             NULL,
             &Thread
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Thread, TestThread);

  Socket  = MAX_UINT32;
  Cluster = MAX_UINT32;
  Core    = MAX_UINT32;
  Thread  = MAX_UINT32;
  Status  = MpCoreInfoGetProcessorLocation (
              TestProcessorId,
              &Socket,
              &Cluster,
              &Core,
              &Thread
              );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Socket, TestSocket);
  EXPECT_EQ (Cluster, TestCluster);
  EXPECT_EQ (Core, TestCore);
  EXPECT_EQ (Thread, TestThread);
}

// Test GetProcessorLocation() API with thread parameter
TEST_F (MpCoreInfoLibTest, GetProcessorLocationWithThread) {
  UINT32  TestSocket;
  UINT32  TestCluster;
  UINT32  TestCore;
  UINT32  TestThread;
  UINT64  TestProcessorId;

  UINT32  Socket;
  UINT32  Cluster;
  UINT32  Core;
  UINT32  Thread;

  ON_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    )
    .WillByDefault (Return (MpCoreHobData));

  TestSocket  = NUM_SOCKETS-1;
  TestCluster = NUM_CLUSTERS (TestSocket)-1;
  TestCore    = NUM_CORES (TestSocket, TestCluster) - 1;
  TestThread  = NUM_THREADS (TestSocket, TestCluster, TestCore) - 1;

  TestProcessorId = GET_AFFINITY_BASED_MPID (TestSocket, TestCluster, TestCore, TestThread);

  Status = MpCoreInfoGetProcessorLocation (
             TestProcessorId,
             NULL,
             NULL,
             NULL,
             NULL
             );
  EXPECT_EQ (Status, EFI_SUCCESS);

  Socket = MAX_UINT32;
  Status = MpCoreInfoGetProcessorLocation (
             TestProcessorId,
             &Socket,
             NULL,
             NULL,
             NULL
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Socket, TestSocket);

  Cluster = MAX_UINT32;
  Status  = MpCoreInfoGetProcessorLocation (
              TestProcessorId,
              NULL,
              &Cluster,
              NULL,
              NULL
              );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Cluster, TestCluster);

  Core   = MAX_UINT32;
  Status = MpCoreInfoGetProcessorLocation (
             TestProcessorId,
             NULL,
             NULL,
             &Core,
             NULL
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Core, TestCore);

  Thread = MAX_UINT32;
  Status = MpCoreInfoGetProcessorLocation (
             TestProcessorId,
             NULL,
             NULL,
             NULL,
             &Thread
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Thread, TestThread);

  Socket  = MAX_UINT32;
  Cluster = MAX_UINT32;
  Core    = MAX_UINT32;
  Thread  = MAX_UINT32;
  Status  = MpCoreInfoGetProcessorLocation (
              TestProcessorId,
              &Socket,
              &Cluster,
              &Core,
              &Thread
              );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (Socket, TestSocket);
  EXPECT_EQ (Cluster, TestCluster);
  EXPECT_EQ (Core, TestCore);
  EXPECT_EQ (Thread, TestThread);
}

// Test MpCoreInfoGetProcessorIdFromLocation() API
TEST_F (MpCoreInfoLibTest, GetProcessorIdFromLocation) {
  UINT32  TestSocket;
  UINT32  TestCluster;
  UINT32  TestCore;
  UINT32  TestThread;
  UINT64  TestProcessorId;
  UINT64  ProcessorId;

  ON_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    )
    .WillByDefault (Return (MpCoreHobData));

  Status = MpCoreInfoGetProcessorIdFromLocation (
             MAX_UINT8+1,
             0,
             0,
             0,
             &ProcessorId
             );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);

  Status = MpCoreInfoGetProcessorIdFromLocation (
             0,
             MAX_UINT8+1,
             0,
             0,
             &ProcessorId
             );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);

  Status = MpCoreInfoGetProcessorIdFromLocation (
             0,
             0,
             MAX_UINT8+1,
             0,
             &ProcessorId
             );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);

  Status = MpCoreInfoGetProcessorIdFromLocation (
             0,
             0,
             0,
             MAX_UINT8+1,
             &ProcessorId
             );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);

  Status = MpCoreInfoGetProcessorIdFromLocation (
             0,
             0,
             0,
             0,
             NULL
             );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);

  TestSocket      = NUM_SOCKETS-1;
  TestCluster     = NUM_CLUSTERS (TestSocket)-1;
  TestCore        = NUM_CORES (TestSocket, TestCluster) - 1;
  TestThread      = 0;
  TestProcessorId = GET_AFFINITY_BASED_MPID (TestSocket, TestCluster, TestCore, TestThread);

  Status = MpCoreInfoGetProcessorIdFromLocation (
             TestSocket,
             TestCluster,
             TestCore,
             TestThread,
             &ProcessorId
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (ProcessorId, TestProcessorId);
}

// Test MpCoreInfoGetPlatformInfo() API
TEST_F (MpCoreInfoLibTest, GetPlatformInfo) {
  UINT32  NumEnabledCores;
  UINT32  MaxSocket;
  UINT32  MaxCluster;
  UINT32  MaxCore;
  UINT32  MaxThread;

  ON_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    )
    .WillByDefault (Return (MpCoreHobData));

  NumEnabledCores = MAX_UINT32;
  MaxSocket       = MAX_UINT32;
  MaxCluster      = MAX_UINT32;
  MaxCore         = MAX_UINT32;
  MaxThread       = MAX_UINT32;
  Status          = MpCoreInfoGetPlatformInfo (&NumEnabledCores, &MaxSocket, &MaxCluster, &MaxCore, &MaxThread);
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (NumEnabledCores, NumCores);
  EXPECT_EQ (MaxSocket, NUM_SOCKETS-1);
  EXPECT_EQ (MaxCluster, NUM_CLUSTERS (NUM_SOCKETS-1) - 1);
  EXPECT_EQ (MaxCore, NUM_CORES (NUM_SOCKETS-1, NUM_CLUSTERS (NUM_SOCKETS-1) - 1) - 1);
  EXPECT_EQ (MaxThread, NUM_THREADS (NUM_SOCKETS-1, NUM_CLUSTERS (NUM_SOCKETS-1) - 1, NUM_CORES (NUM_SOCKETS-1, NUM_CLUSTERS (NUM_SOCKETS-1) - 1) - 1) - 1);

  NumEnabledCores = MAX_UINT32;
  Status          = MpCoreInfoGetPlatformInfo (&NumEnabledCores, NULL, NULL, NULL, NULL);
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (NumEnabledCores, NumCores);

  MaxSocket = MAX_UINT32;
  Status    = MpCoreInfoGetPlatformInfo (NULL, &MaxSocket, NULL, NULL, NULL);
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (MaxSocket, NUM_SOCKETS-1);

  MaxCluster = MAX_UINT32;
  Status     = MpCoreInfoGetPlatformInfo (NULL, NULL, &MaxCluster, NULL, NULL);
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (MaxCluster, NUM_CLUSTERS (NUM_SOCKETS-1) - 1);

  MaxCore = MAX_UINT32;
  Status  = MpCoreInfoGetPlatformInfo (NULL, NULL, NULL, &MaxCore, NULL);
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (MaxCore, NUM_CORES (NUM_SOCKETS-1, NUM_CLUSTERS (NUM_SOCKETS-1) - 1) - 1);

  MaxThread = MAX_UINT32;
  Status    = MpCoreInfoGetPlatformInfo (NULL, NULL, NULL, NULL, &MaxThread);
  EXPECT_EQ (Status, EFI_SUCCESS);
  EXPECT_EQ (MaxThread, NUM_THREADS (NUM_SOCKETS-1, NUM_CLUSTERS (NUM_SOCKETS-1) - 1, NUM_CORES (NUM_SOCKETS-1, NUM_CLUSTERS (NUM_SOCKETS-1) - 1) - 1) - 1);

  Status = MpCoreInfoGetPlatformInfo (&NumEnabledCores, &MaxSocket, &MaxCluster, &MaxCore, &MaxThread);
  EXPECT_EQ (Status, EFI_SUCCESS);
}

// Test MpCoreInfGetSocketInfo() API
TEST_F (MpCoreInfoLibTest, GetSocketInfo) {
  UINT32  SocketIndex;
  UINT32  NumEnabledCores;
  UINT32  ExpectedEnabledCores;
  UINT32  MaxCluster;
  UINT32  MaxCore;
  UINT32  MaxThread;
  UINT64  FirstCoreId;
  UINT32  ClusterIndex;
  UINT32  CoreIndex;

  ON_CALL (
    HobMock,
    GetFirstGuidHob (
      BufferEq (&gArmMpCoreInfoGuid, sizeof (EFI_GUID))
      )
    )
    .WillByDefault (Return (MpCoreHobData));

  for (SocketIndex = 0; SocketIndex <= NUM_SOCKETS; SocketIndex++) {
    ExpectedEnabledCores = 0;
    for (ClusterIndex = 0; ClusterIndex < NUM_CLUSTERS (SocketIndex); ClusterIndex++) {
      for (CoreIndex = 0; CoreIndex < NUM_CORES (SocketIndex, ClusterIndex); CoreIndex++) {
        ExpectedEnabledCores += NUM_THREADS (SocketIndex, ClusterIndex, CoreIndex);
      }
    }

    NumEnabledCores = MAX_UINT32;
    MaxCluster      = MAX_UINT32;
    MaxCore         = MAX_UINT32;
    MaxThread       = MAX_UINT32;
    FirstCoreId     = MAX_UINT64;
    Status          = MpCoreInfoGetSocketInfo (SocketIndex, &NumEnabledCores, &MaxCluster, &MaxCore, &MaxThread, &FirstCoreId);
    if (SocketIndex == NUM_SOCKETS) {
      EXPECT_EQ (Status, EFI_NOT_FOUND);
    } else {
      EXPECT_EQ (Status, EFI_SUCCESS);
      EXPECT_EQ (NumEnabledCores, ExpectedEnabledCores);
      EXPECT_EQ (MaxCluster, NUM_CLUSTERS (SocketIndex) - 1);
      EXPECT_EQ (MaxCore, NUM_CORES (SocketIndex, NUM_CLUSTERS (SocketIndex) - 1) - 1);
      EXPECT_EQ (MaxThread, NUM_THREADS (SocketIndex, NUM_CLUSTERS (SocketIndex) - 1, NUM_CORES (SocketIndex, NUM_CLUSTERS (SocketIndex) - 1) - 1) - 1);
      EXPECT_EQ (FirstCoreId, GET_AFFINITY_BASED_MPID (SocketIndex, 0, 0, 0));
    }

    NumEnabledCores = MAX_UINT32;
    Status          = MpCoreInfoGetSocketInfo (SocketIndex, &NumEnabledCores, NULL, NULL, NULL, NULL);
    if (SocketIndex == NUM_SOCKETS) {
      EXPECT_EQ (Status, EFI_NOT_FOUND);
    } else {
      EXPECT_EQ (Status, EFI_SUCCESS);
      EXPECT_EQ (NumEnabledCores, ExpectedEnabledCores);
    }

    MaxCluster = MAX_UINT32;
    Status     = MpCoreInfoGetSocketInfo (SocketIndex, NULL, &MaxCluster, NULL, NULL, NULL);
    if (SocketIndex == NUM_SOCKETS) {
      EXPECT_EQ (Status, EFI_NOT_FOUND);
    } else {
      EXPECT_EQ (Status, EFI_SUCCESS);
      EXPECT_EQ (MaxCluster, NUM_CLUSTERS (SocketIndex) - 1);
    }

    MaxCore = MAX_UINT32;
    Status  = MpCoreInfoGetSocketInfo (SocketIndex, NULL, NULL, &MaxCore, NULL, NULL);
    if (SocketIndex == NUM_SOCKETS) {
      EXPECT_EQ (Status, EFI_NOT_FOUND);
    } else {
      EXPECT_EQ (Status, EFI_SUCCESS);
      EXPECT_EQ (MaxCore, NUM_CORES (SocketIndex, NUM_CLUSTERS (SocketIndex) - 1) - 1);
    }

    MaxThread = MAX_UINT32;
    Status    = MpCoreInfoGetSocketInfo (SocketIndex, NULL, NULL, NULL, &MaxThread, NULL);
    if (SocketIndex == NUM_SOCKETS) {
      EXPECT_EQ (Status, EFI_NOT_FOUND);
    } else {
      EXPECT_EQ (Status, EFI_SUCCESS);
      EXPECT_EQ (MaxThread, NUM_THREADS (SocketIndex, NUM_CLUSTERS (SocketIndex) - 1, NUM_CORES (SocketIndex, NUM_CLUSTERS (SocketIndex) - 1) - 1) - 1);
    }

    FirstCoreId = MAX_UINT64;
    Status      = MpCoreInfoGetSocketInfo (SocketIndex, NULL, NULL, NULL, NULL, &FirstCoreId);
    if (SocketIndex == NUM_SOCKETS) {
      EXPECT_EQ (Status, EFI_NOT_FOUND);
    } else {
      EXPECT_EQ (Status, EFI_SUCCESS);
      EXPECT_EQ (FirstCoreId, GET_AFFINITY_BASED_MPID (SocketIndex, 0, 0, 0));
    }

    Status = MpCoreInfoGetSocketInfo (SocketIndex, NULL, NULL, NULL, NULL, NULL);
    if (SocketIndex == NUM_SOCKETS) {
      EXPECT_EQ (Status, EFI_NOT_FOUND);
    } else {
      EXPECT_EQ (Status, EFI_SUCCESS);
    }
  }
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
