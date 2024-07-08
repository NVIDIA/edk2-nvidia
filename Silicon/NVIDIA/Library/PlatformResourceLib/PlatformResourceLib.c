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
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
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

  Dtb = (VOID *)PlatformResourceInfo->ResourceInfo->DtbLoadAddress;

  // count number of socket nodes, 100 limit due to socket@xx string
  for (MaxSockets = 0; MaxSockets < 100; MaxSockets++) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", MaxSockets);
    SocketOffset = fdt_path_offset (Dtb, SocketNodeStr);
    if (SocketOffset < 0) {
      break;
    }
  }

  // handle global vs per-socket cpu map
  if (MaxSockets == 0) {
    PlatformResourceInfo->MaxPossibleSockets = 1;
    CpuMapPathFormat                         = "/cpus/cpu-map";
  } else {
    PlatformResourceInfo->MaxPossibleSockets = MaxSockets;
    CpuMapPathFormat                         = "/socket@%u/cpus/cpu-map";
  }

  // count clusters across all sockets
  MaxClusters = 0;
  for (Socket = 0; Socket < PlatformResourceInfo->MaxPossibleSockets; Socket++) {
    UINTN  Cluster;

    AsciiSPrint (CpuMapPathStr, sizeof (CpuMapPathStr), CpuMapPathFormat, Socket);
    CpuMapOffset = fdt_path_offset (Dtb, CpuMapPathStr);
    if (CpuMapOffset < 0) {
      PlatformResourceInfo->MaxPossibleClusters        = 1;
      PlatformResourceInfo->MaxPossibleCoresPerCluster = 1;
      DEBUG ((
        DEBUG_ERROR,
        "%a: %a missing in DTB, using Clusters=%u, CoresPerCluster=%u\n",
        __FUNCTION__,
        CpuMapPathStr,
        PlatformResourceInfo->MaxPossibleClusters,
        PlatformResourceInfo->MaxPossibleCoresPerCluster
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

  PlatformResourceInfo->MaxPossibleClusters = MaxClusters;

  // Use cluster0 node to find max core subnode
  Cluster0Offset = fdt_subnode_offset (Dtb, CpuMapOffset, "cluster0");
  if (Cluster0Offset < 0) {
    PlatformResourceInfo->MaxPossibleCoresPerCluster = 1;
    DEBUG ((
      DEBUG_ERROR,
      "No cluster0 in %a, using CoresPerCluster=%u\n",
      CpuMapPathStr,
      PlatformResourceInfo->MaxPossibleCoresPerCluster
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

  PlatformResourceInfo->MaxPossibleCoresPerCluster = MaxCoresPerCluster;

  return EFI_SUCCESS;
}

/**
  Check if given core is enabled

**/
STATIC
BOOLEAN
EFIAPI
PlatformResourceIsCoreEnabled (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN  UINT32                       CpuIndex
  )
{
  UINTN  Index;
  UINTN  Bit;

  Index = CpuIndex /  (8*sizeof (PlatformResourceInfo->EnabledCoresBitMap[0]));
  Bit   = CpuIndex %  (8*sizeof (PlatformResourceInfo->EnabledCoresBitMap[0]));

  return ((PlatformResourceInfo->EnabledCoresBitMap[Index] & (1ULL << Bit)) != 0);
}

STATIC
UINT64
EFIAPI
GetMpidrFromCoreIndex (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN UINT32                        CpuIndex
  )
{
  UINTN   Socket;
  UINTN   Cluster;
  UINTN   Core;
  UINT64  Mpidr;
  UINT32  SocketCoreId;
  UINTN   MaxCoresPerSocket;

  MaxCoresPerSocket = PlatformResourceInfo->MaxPossibleCores / PlatformResourceInfo->MaxPossibleSockets;

  Socket = CpuIndex / MaxCoresPerSocket;
  NV_ASSERT_RETURN (
    Socket < PlatformResourceInfo->MaxPossibleSockets,
    return 0,
    "Invalid Socket %u >= %u \r\n",
    Socket,
    PlatformResourceInfo->MaxPossibleSockets
    );

  SocketCoreId = CpuIndex - (Socket * MaxCoresPerSocket);

  Cluster = SocketCoreId / PlatformResourceInfo->MaxPossibleCoresPerCluster;
  NV_ASSERT_RETURN (
    Cluster < PlatformResourceInfo->MaxPossibleClusters,
    return 0,
    "Invalid Cluster %u >= %u \r\n",
    Cluster,
    PlatformResourceInfo->MaxPossibleClusters
    );

  Core = SocketCoreId % PlatformResourceInfo->MaxPossibleCoresPerCluster;
  NV_ASSERT_RETURN (
    Core < PlatformResourceInfo->MaxPossibleCoresPerCluster,
    return 0,
    "Invalid Core %u >= %u \r\n",
    Core,
    PlatformResourceInfo->MaxPossibleCoresPerCluster
    );

  // Check the Pcd and modify MPIDR generation if required
  if (!PlatformResourceInfo->AffinityMpIdrSupported) {
    ASSERT (Socket == 0);
    Mpidr = (UINT64)GET_MPID (Cluster, Core);
  } else {
    Mpidr = (UINT64)GET_AFFINITY_BASED_MPID (Socket, Cluster, Core, 0);
  }

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
  UINTN                         PrintedWords = 0;
  UINTN                         WordsPerLine = 3;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  NV_ASSERT_RETURN (
    (Hob != NULL) &&
    (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)),
    return EFI_DEVICE_ERROR,
    "Failed to get PlatformResourceInfo\r\n"
    );

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);

  Status = UpdateCoreInfoFromDtb (PlatformResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PlatformResourceInfo->MaxPossibleCores = PlatformResourceInfo->MaxPossibleCoresPerCluster * PlatformResourceInfo->MaxPossibleClusters;

  DEBUG ((DEBUG_ERROR, "DTB maximums: sockets=%u clusters=%u cores=%u\n", PlatformResourceInfo->MaxPossibleSockets, PlatformResourceInfo->MaxPossibleClusters, PlatformResourceInfo->MaxPossibleCores));

  Status = SocGetEnabledCoresBitMap (PlatformResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (CoreIndex = 0; CoreIndex < PlatformResourceInfo->MaxPossibleCores; CoreIndex++) {
    if (PlatformResourceIsCoreEnabled (PlatformResourceInfo, CoreIndex)) {
      PlatformResourceInfo->NumberOfEnabledCores++;
    }
  }

  // No CPUs detected assume single core pre-silicon targets
  if (PlatformResourceInfo->NumberOfEnabledCores == 0) {
    PlatformResourceInfo->EnabledCoresBitMap[0] = 1;
    PlatformResourceInfo->NumberOfEnabledCores++;
  }

  DEBUG ((DEBUG_ERROR, "SocketMask=0x%x NumberOfEnabledCores=%u\n", PlatformResourceInfo->SocketMask, PlatformResourceInfo->NumberOfEnabledCores));

  EnabledCoresWordCount = ALIGN_VALUE (PlatformResourceInfo->MaxPossibleCores, 64) / 64;
  for (Index = 0; Index < EnabledCoresWordCount; Index++) {
    EnabledCoresWordIndex = EnabledCoresWordCount - Index - 1;
    if ((PrintedWords == 0) &&
        (PlatformResourceInfo->EnabledCoresBitMap[EnabledCoresWordIndex] == 0))
    {
      continue;
    }

    if ((PrintedWords++ % WordsPerLine) == 0) {
      DEBUG ((DEBUG_ERROR, "EnabledCores"));
    }

    DEBUG ((DEBUG_ERROR, "[%u]=0x%016llx ", EnabledCoresWordIndex, PlatformResourceInfo->EnabledCoresBitMap[EnabledCoresWordIndex]));

    if (((PrintedWords % WordsPerLine) == 0) || (Index == (EnabledCoresWordCount - 1))) {
      DEBUG ((DEBUG_ERROR, "\n"));
    }
  }

  Status = SocUpdatePlatformResourceInformation (PlatformResourceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ArmCoreInfo = (ARM_CORE_INFO *)BuildGuidHob (&gArmMpCoreInfoGuid, sizeof (ARM_CORE_INFO) * PlatformResourceInfo->NumberOfEnabledCores);
  if (ArmCoreInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CoreInfoIndex = 0;
  for (CoreIndex = 0; CoreIndex < PlatformResourceInfo->MaxPossibleCores; CoreIndex++) {
    if (PlatformResourceIsCoreEnabled (PlatformResourceInfo, CoreIndex)) {
      ArmCoreInfo[CoreInfoIndex].Mpidr = GetMpidrFromCoreIndex (PlatformResourceInfo, CoreIndex);
      CoreInfoIndex++;
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
