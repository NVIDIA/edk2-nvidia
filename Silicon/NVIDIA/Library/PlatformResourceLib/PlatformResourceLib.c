/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiPei.h>
#include <Library/ArmLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Guid/ArmMpCoreInfo.h>
#include <libfdt.h>
#include "SocResourceConfig.h"

#define GET_AFFINITY_BASED_MPID(Aff3, Aff2, Aff1, Aff0)         \
  (((UINT64)(Aff3) << 32) | ((Aff2) << 16) | ((Aff1) << 8) | (Aff0))

STATIC
EFI_PHYSICAL_ADDRESS  TegraUARTBaseAddress = 0x0;

/**
  Set Tegra UART Base Address
**/
VOID
EFIAPI
SetTegraUARTBaseAddress (
  IN EFI_PHYSICAL_ADDRESS  UartBaseAddress
  )
{
  TegraUARTBaseAddress = UartBaseAddress;
}

/**
  Retrieve CPU BL Address

**/
UINTN
EFIAPI
GetCPUBLBaseAddress (
  VOID
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;
  UINTN  CpuBootloaderAddressLo;
  UINTN  CpuBootloaderAddressHi;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddressLo = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress (ChipID));
  CpuBootloaderAddressHi = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress (ChipID) + sizeof (UINT32));
  CpuBootloaderAddress   = (CpuBootloaderAddressHi << 32) | CpuBootloaderAddressLo;

  return CpuBootloaderAddress;
}

/**
  Get Max Core info from DTB

*/
STATIC
EFI_STATUS
EFIAPI
UpdateCoreInfoFromDtb (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  OUT SOC_CORE_BITMAP_INFO         *SocCoreBitmapInfo
  )
{
  CONST VOID  *Dtb;
  UINTN       MaxClusters;
  UINTN       MaxCoresPerCluster;
  UINTN       MaxSockets;
  INT32       CpuMapOffset;
  INT32       Cluster0Offset;
  INT32       NodeOffset;
  CHAR8       ClusterNodeStr[] = "clusterxxx";
  CHAR8       CoreNodeStr[]    = "corexx";
  CHAR8       SocketNodeStr[]  = "/socket@xx";
  INT32       SocketOffset;
  CHAR8       CpuMapPathStr[] = "/socket@xx/cpus/cpu-map";
  CHAR8       *CpuMapPathFormat;
  UINTN       Socket;
  INT32       CpusOffset;
  CONST VOID  *Property;
  INT32       Length;
  UINT32      NumaNodeId;
  UINT32      CpuMapSocketScanCount;

  Dtb              = (VOID *)PlatformResourceInfo->ResourceInfo->DtbLoadAddress;
  MaxSockets       = 0;
  CpuMapPathFormat = NULL;

  // find highest numa-node-id, if present
  CpusOffset = fdt_path_offset (Dtb, "/cpus");
  fdt_for_each_subnode (NodeOffset, Dtb, CpusOffset) {
    Property = fdt_getprop (Dtb, NodeOffset, "device_type", &Length);
    if ((Property == NULL) || (AsciiStrCmp (Property, "cpu") != 0)) {
      NodeOffset = fdt_next_subnode (Dtb, NodeOffset);
      continue;
    }

    Property = fdt_getprop (Dtb, NodeOffset, "numa-node-id", &Length);
    if ((Property == NULL) || (Length != sizeof (NumaNodeId))) {
      if (MaxSockets != 0) {
        DEBUG ((DEBUG_ERROR, "Missing numa-node-id in /cpus/%a\n", fdt_get_name (Dtb, NodeOffset, NULL)));
        return EFI_DEVICE_ERROR;
      } else {
        break;
      }
    }

    NumaNodeId = fdt32_to_cpu (*(UINT32 *)Property);
    if ((NumaNodeId + 1) > MaxSockets) {
      MaxSockets = NumaNodeId + 1;
    }
  }

  // No sockets found in cpu-map, checking with /socket@xx nodes
  if (MaxSockets == 0) {
    // check for socket nodes, 100 limit due to socket@xx string
    for (MaxSockets = 0; MaxSockets < 100; MaxSockets++) {
      AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", MaxSockets);
      SocketOffset = fdt_path_offset (Dtb, SocketNodeStr);
      if (SocketOffset < 0) {
        break;
      }
    }

    if (MaxSockets != 0) {
      CpuMapPathFormat = "/socket@%u/cpus/cpu-map";
    }
  }

  if (CpuMapPathFormat == NULL) {
    CpuMapPathFormat      = "/cpus/cpu-map";
    CpuMapSocketScanCount = 1;
  } else {
    CpuMapSocketScanCount = MaxSockets;
  }

  // If no socket info detected, set to 1
  if (MaxSockets == 0) {
    MaxSockets = 1;
  }

  SocCoreBitmapInfo->MaxPossibleSockets = MaxSockets;

  // count clusters across all sockets
  MaxClusters = 0;
  for (Socket = 0; Socket < CpuMapSocketScanCount; Socket++) {
    UINTN  Cluster;

    AsciiSPrint (CpuMapPathStr, sizeof (CpuMapPathStr), CpuMapPathFormat, Socket);
    CpuMapOffset = fdt_path_offset (Dtb, CpuMapPathStr);
    if (CpuMapOffset < 0) {
      SocCoreBitmapInfo->MaxPossibleClustersPerSystem = 1;
      SocCoreBitmapInfo->MaxPossibleCoresPerCluster   = 1;
      DEBUG ((
        DEBUG_ERROR,
        "%a: %a missing in DTB, using Clusters=%u, CoresPerCluster=%u\n",
        __FUNCTION__,
        CpuMapPathStr,
        SocCoreBitmapInfo->MaxPossibleClustersPerSystem,
        SocCoreBitmapInfo->MaxPossibleCoresPerCluster
        ));
      return EFI_SUCCESS;
    }

    Cluster = 0;
    while (TRUE) {
      AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr), "cluster%u", Cluster);
      NodeOffset = fdt_subnode_offset (Dtb, CpuMapOffset, ClusterNodeStr);
      if (NodeOffset < 0) {
        break;
      }

      MaxClusters++;
      Cluster++;
      NV_ASSERT_RETURN (Cluster < 1000, return EFI_DEVICE_ERROR, "Too many clusters seen\r\n");
    }

    DEBUG ((DEBUG_INFO, "Socket=%u MaxClusters=%u\n", Socket, MaxClusters));
  }

  SocCoreBitmapInfo->MaxPossibleClustersPerSystem = MaxClusters;

  // Use cluster0 node to find max core subnode
  Cluster0Offset = fdt_subnode_offset (Dtb, CpuMapOffset, "cluster0");
  if (Cluster0Offset < 0) {
    SocCoreBitmapInfo->MaxPossibleCoresPerCluster = 1;
    DEBUG ((
      DEBUG_ERROR,
      "No cluster0 in %a, using CoresPerCluster=%u\n",
      CpuMapPathStr,
      SocCoreBitmapInfo->MaxPossibleCoresPerCluster
      ));
    return EFI_SUCCESS;
  }

  MaxCoresPerCluster = 1;
  while (TRUE) {
    AsciiSPrint (CoreNodeStr, sizeof (CoreNodeStr), "core%u", MaxCoresPerCluster);
    NodeOffset = fdt_subnode_offset (Dtb, Cluster0Offset, CoreNodeStr);
    if (NodeOffset < 0) {
      break;
    }

    MaxCoresPerCluster++;

    NV_ASSERT_RETURN (MaxCoresPerCluster < 100, return EFI_DEVICE_ERROR, "Too many cores seen\r\n");
  }

  SocCoreBitmapInfo->MaxPossibleCoresPerCluster = MaxCoresPerCluster;
  SocCoreBitmapInfo->MaxPossibleCoresPerSystem  = SocCoreBitmapInfo->MaxPossibleCoresPerCluster * SocCoreBitmapInfo->MaxPossibleClustersPerSystem;
  PlatformResourceInfo->MaxPossibleSockets      = SocCoreBitmapInfo->MaxPossibleSockets;

  return EFI_SUCCESS;
}

/**
  Check if given core is enabled

**/
STATIC
BOOLEAN
EFIAPI
PlatformResourceIsCoreEnabled (
  IN SOC_CORE_BITMAP_INFO  *SocCoreBitmapInfo,
  IN  UINT32               CpuIndex
  )
{
  UINTN  Index;
  UINTN  Bit;

  Index = CpuIndex /  (8*sizeof (SocCoreBitmapInfo->EnabledCoresBitMap[0]));
  Bit   = CpuIndex %  (8*sizeof (SocCoreBitmapInfo->EnabledCoresBitMap[0]));

  return ((SocCoreBitmapInfo->EnabledCoresBitMap[Index] & (1ULL << Bit)) != 0);
}

STATIC
UINT64
EFIAPI
GetMpidrFromCoreIndex (
  IN SOC_CORE_BITMAP_INFO  *SocCoreBitmapInfo,
  IN UINT32                CpuIndex,
  IN UINT32                ThreadId
  )
{
  UINTN   Socket;
  UINTN   Cluster;
  UINTN   Core;
  UINT64  Mpidr;
  UINT32  SocketCoreId;
  UINTN   MaxCoresPerSocket;

  MaxCoresPerSocket = SocCoreBitmapInfo->MaxPossibleCoresPerSystem / SocCoreBitmapInfo->MaxPossibleSockets;

  Socket = CpuIndex / MaxCoresPerSocket;
  NV_ASSERT_RETURN (
    Socket < SocCoreBitmapInfo->MaxPossibleSockets,
    return 0,
    "Invalid Socket %u >= %u \r\n",
    Socket,
    SocCoreBitmapInfo->MaxPossibleSockets
    );

  SocketCoreId = CpuIndex - (Socket * MaxCoresPerSocket);

  Cluster = SocketCoreId / SocCoreBitmapInfo->MaxPossibleCoresPerCluster;
  NV_ASSERT_RETURN (
    Cluster < SocCoreBitmapInfo->MaxPossibleClustersPerSystem,
    return 0,
    "Invalid Cluster %u >= %u \r\n",
    Cluster,
    SocCoreBitmapInfo->MaxPossibleClustersPerSystem
    );

  Core = SocketCoreId % SocCoreBitmapInfo->MaxPossibleCoresPerCluster;
  NV_ASSERT_RETURN (
    Core < SocCoreBitmapInfo->MaxPossibleCoresPerCluster,
    return 0,
    "Invalid Core %u >= %u \r\n",
    Core,
    SocCoreBitmapInfo->MaxPossibleCoresPerCluster
    );

  Mpidr = (UINT64)GET_AFFINITY_BASED_MPID (Socket, Cluster, Core, ThreadId);

  return Mpidr;
}

/**
  Get Platform Resource Information
  Does not update the CPU info structures.

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformation (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  UINTN  CpuBootloaderAddress;

  PlatformResourceInfo->ResourceInfo = AllocateZeroPool (sizeof (TEGRA_RESOURCE_INFO));
  if (PlatformResourceInfo->ResourceInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  PlatformResourceInfo->BoardInfo = AllocateZeroPool (sizeof (TEGRA_BOARD_INFO));
  if (PlatformResourceInfo->BoardInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  return SocGetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo, FALSE);
}

/**
  Update info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
UpdatePlatformResourceInformation (
  VOID
  )
{
  EFI_STATUS                    Status;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  UINTN                         CoreIndex;
  UINTN                         CoreInfoIndex;
  ARM_CORE_INFO                 *ArmCoreInfo;
  UINTN                         Index;
  UINTN                         EnabledCoresWordCount;
  UINTN                         EnabledCoresWordIndex;
  UINTN                         PrintedWords      = 0;
  UINTN                         WordsPerLine      = 3;
  SOC_CORE_BITMAP_INFO          SocCoreBitmapInfo = { 0 };
  UINT32                        NumberOfEnabledCores;
  UINT32                        ThreadId;
  UINTN                         CpuBootloaderAddress;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  NV_ASSERT_RETURN (
    (Hob != NULL) &&
    (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)),
    return EFI_DEVICE_ERROR,
    "Failed to get PlatformResourceInfo\r\n"
    );

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);

  Status = UpdateCoreInfoFromDtb (PlatformResourceInfo, &SocCoreBitmapInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_ERROR, "DTB maximums: sockets=%u clusters=%u cores=%u\n", SocCoreBitmapInfo.MaxPossibleSockets, SocCoreBitmapInfo.MaxPossibleClustersPerSystem, SocCoreBitmapInfo.MaxPossibleCoresPerSystem));

  CpuBootloaderAddress = GetCPUBLBaseAddress ();
  Status               = SocGetEnabledCoresBitMap (CpuBootloaderAddress, &SocCoreBitmapInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumberOfEnabledCores = 0;
  for (CoreIndex = 0; CoreIndex < SocCoreBitmapInfo.MaxPossibleCoresPerSystem; CoreIndex++) {
    if (PlatformResourceIsCoreEnabled (&SocCoreBitmapInfo, CoreIndex)) {
      NumberOfEnabledCores++;
    }
  }

  // No CPUs detected assume single core pre-silicon targets
  if (NumberOfEnabledCores == 0) {
    SocCoreBitmapInfo.EnabledCoresBitMap[0] = 1;
    NumberOfEnabledCores++;
  }

  DEBUG ((DEBUG_ERROR, "SocketMask=0x%x NumberOfEnabledCores=%u\n", PlatformResourceInfo->SocketMask, NumberOfEnabledCores));

  EnabledCoresWordCount = ALIGN_VALUE (SocCoreBitmapInfo.MaxPossibleCoresPerSystem, 64) / 64;
  for (Index = 0; Index < EnabledCoresWordCount; Index++) {
    EnabledCoresWordIndex = EnabledCoresWordCount - Index - 1;
    if ((PrintedWords == 0) &&
        (SocCoreBitmapInfo.EnabledCoresBitMap[EnabledCoresWordIndex] == 0))
    {
      continue;
    }

    if ((PrintedWords++ % WordsPerLine) == 0) {
      DEBUG ((DEBUG_ERROR, "EnabledCores"));
    }

    DEBUG ((DEBUG_ERROR, "[%u]=0x%016llx ", EnabledCoresWordIndex, SocCoreBitmapInfo.EnabledCoresBitMap[EnabledCoresWordIndex]));

    if (((PrintedWords % WordsPerLine) == 0) || (Index == (EnabledCoresWordCount - 1))) {
      DEBUG ((DEBUG_ERROR, "\n"));
    }
  }

  Status = SocUpdatePlatformResourceInformation (PlatformResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ArmCoreInfo = (ARM_CORE_INFO *)BuildGuidHob (&gArmMpCoreInfoGuid, sizeof (ARM_CORE_INFO) * NumberOfEnabledCores * SocCoreBitmapInfo.ThreadsPerCore);
  if (ArmCoreInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CoreInfoIndex = 0;
  for (CoreIndex = 0; CoreIndex < SocCoreBitmapInfo.MaxPossibleCoresPerSystem; CoreIndex++) {
    if (PlatformResourceIsCoreEnabled (&SocCoreBitmapInfo, CoreIndex)) {
      for (ThreadId = 0; ThreadId < SocCoreBitmapInfo.ThreadsPerCore; ThreadId++) {
        ArmCoreInfo[CoreInfoIndex].Mpidr = GetMpidrFromCoreIndex (&SocCoreBitmapInfo, CoreIndex, ThreadId);
        CoreInfoIndex++;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformationStandaloneMm (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN PHYSICAL_ADDRESS              CpuBootloaderAddress
  )
{
  return SocGetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo, TRUE);
}

/**
 * Get the sockets Enabled Bit Mask.
 *
 * @param[in] CpuBlAddress          Address of the CPU BL params.
 *
 * @retval  Bitmask of enabled sockets (0x1 if CPUBL is 0).
**/
UINT32
EFIAPI
GetSocketMaskStMm (
  IN UINTN  CpuBlAddress
  )
{
  UINT32  SocketMask;

  if (CpuBlAddress == 0) {
    SocketMask = 0x1;
  } else {
    SocketMask = SocGetSocketMask (CpuBlAddress);
  }

  return SocketMask;
}

/**
 * Check if socket is enabled in the CPU BL Params's socket mask.
 * This API is usually only called from StMM.
 *
 * @param[in] CpuBlAddress          Address of the CPU BL params.
 * @param[in] SocketNum             Socket to check.
 *
 * @retval  TRUE                    Socket is enabled.
 * @retval  FALSE                   Socket is not enabled.
**/
BOOLEAN
EFIAPI
IsSocketEnabledStMm (
  IN UINTN   CpuBlAddress,
  IN UINT32  SocketNum
  )
{
  UINT32   SocketMask;
  BOOLEAN  SocketEnabled;

  SocketMask = GetSocketMaskStMm (CpuBlAddress);

  SocketEnabled = ((SocketMask & (1U << SocketNum)) ? TRUE : FALSE);
  return SocketEnabled;
}

/**
 * Retrieves if the systems supports software core disable
 *
 * @param[in]  Socket           Socket Id
 * @param[out] TotalCoreCount   Total Core Count
 *
 * @retval  EFI_SUCCESS             System supports software core disable and returns the total core count
 * @retval  EFI_INVALID_PARAMETER   Invalid socket id.
 * @retval  EFI_INVALID_PARAMETER   TotalCoreCount is NULL
 * @retval  EFI_UNSUPPORTED         Unsupported feature
**/
EFI_STATUS
EFIAPI
SupportsSoftwareCoreDisable (
  IN UINT32   Socket,
  OUT UINT32  *TotalCoreCount
  )
{
  if (TotalCoreCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return SocSupportsSoftwareCoreDisable (Socket, TotalCoreCount);
}
