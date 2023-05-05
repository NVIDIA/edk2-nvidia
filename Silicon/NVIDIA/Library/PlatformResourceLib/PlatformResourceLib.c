/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <Library/PlatformResourceInternalLib.h>
#include "T194ResourceConfig.h"
#include "T234ResourceConfig.h"

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
  Retrieve Tegra UART Base Address

**/
EFI_PHYSICAL_ADDRESS
EFIAPI
GetTegraUARTBaseAddress (
  VOID
  )
{
  UINTN                 ChipID;
  EFI_PHYSICAL_ADDRESS  TegraUARTBase;
  BOOLEAN               ValidPrivatePlatform;

  if (TegraUARTBaseAddress != 0x0) {
    return TegraUARTBaseAddress;
  }

  ValidPrivatePlatform = GetTegraUARTBaseAddressInternal (&TegraUARTBase);
  if (ValidPrivatePlatform) {
    return TegraUARTBase;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return FixedPcdGet64 (PcdTegra16550UartBaseT194);
    case T234_CHIP_ID:
      return FixedPcdGet64 (PcdTegra16550UartBaseT234);
    default:
      return 0x0;
  }
}

/**
  It's to get the UART instance number that the trust-firmware hands over.
  Currently that chain is broken so temporarily override the UART instance number
  to the fixed known id based on the chip id.

**/
STATIC
BOOLEAN
GetSharedUARTInstanceId (
  IN  UINTN   ChipID,
  OUT UINT32  *UARTInstanceNumber
  )
{
  switch (ChipID) {
    case T234_CHIP_ID:
      *UARTInstanceNumber = 1; // UART_A
      return TRUE;
    default:
      return FALSE;
  }
}

/**
  Retrieve the type and address of UART based on the instance Number

**/
EFI_STATUS
EFIAPI
GetUARTInstanceInfo (
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
  )
{
  UINTN    ChipID;
  UINT32   SharedUARTInstanceId;
  BOOLEAN  ValidPrivatePlatform;

  *UARTInstanceType    = TEGRA_UART_TYPE_NONE;
  *UARTInstanceAddress = 0x0;

  ValidPrivatePlatform = GetUARTInstanceInfoInternal (UARTInstanceType, UARTInstanceAddress);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      *UARTInstanceType    = TEGRA_UART_TYPE_TCU;
      *UARTInstanceAddress = (EFI_PHYSICAL_ADDRESS)FixedPcdGet64 (PcdTegra16550UartBaseT194);
      return EFI_SUCCESS;
    case T234_CHIP_ID:
      if (!GetSharedUARTInstanceId (T234_CHIP_ID, &SharedUARTInstanceId)) {
        return EFI_UNSUPPORTED;
      }

      return T234UARTInstanceInfo (SharedUARTInstanceId, UARTInstanceType, UARTInstanceAddress);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Retrieve chip specific info for GIC

**/
BOOLEAN
EFIAPI
GetGicInfo (
  OUT TEGRA_GIC_INFO  *GicInfo
  )
{
  UINTN    ChipID;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = GetGicInfoInternal (GicInfo);
  if (ValidPrivatePlatform) {
    return TRUE;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      // *CompatString = "arm,gic-v2";
      GicInfo->GicCompatString = "arm,gic-400";
      GicInfo->ItsCompatString = "";
      GicInfo->Version         = 2;
      break;
    case T234_CHIP_ID:
      GicInfo->GicCompatString = "arm,gic-v3";
      GicInfo->ItsCompatString = "arm,gic-v3-its";
      GicInfo->Version         = 3;
      break;
    default:
      return FALSE;
  }

  return TRUE;
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
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  UINTN    CpuBootloaderAddressLo;
  UINTN    CpuBootloaderAddressHi;
  UINTN    SystemMemoryBaseAddress;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = GetCPUBLBaseAddressInternal (&CpuBootloaderAddress);
  if (ValidPrivatePlatform) {
    return CpuBootloaderAddress;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddressLo = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress (ChipID));
  CpuBootloaderAddressHi = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress (ChipID) + sizeof (UINT32));
  CpuBootloaderAddress   = (CpuBootloaderAddressHi << 32) | CpuBootloaderAddressLo;

  if (ChipID == T194_CHIP_ID) {
    SystemMemoryBaseAddress = TegraGetSystemMemoryBaseAddress (ChipID);

    if (CpuBootloaderAddress < SystemMemoryBaseAddress) {
      CpuBootloaderAddress <<= 16;
    }
  }

  return CpuBootloaderAddress;
}

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
EFIAPI
GetDramPageBlacklistInfoAddress (
  VOID
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetDramPageBlacklistInfoAddress (CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetDramPageBlacklistInfoAddress (CpuBootloaderAddress);
    default:
      return 0x0;
  }
}

/**
  Retrieve DTB Address

**/
UINT64
EFIAPI
GetDTBBaseAddress (
  VOID
  )
{
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  UINT64   DTBBaseAddress;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = GetDTBBaseAddressInternal (&DTBBaseAddress);
  if (ValidPrivatePlatform) {
    return DTBBaseAddress;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetDTBBaseAddress (CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetDTBBaseAddress (CpuBootloaderAddress);
    default:
      return 0x0;
  }
}

/**
  Retrieve GR Blob Address

**/
UINT64
EFIAPI
GetGRBlobBaseAddress (
  VOID
  )
{
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  UINT64   GRBlobBaseAddress;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = GetGRBlobBaseAddressInternal (&GRBlobBaseAddress);
  if (ValidPrivatePlatform) {
    return GRBlobBaseAddress;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetGRBlobBaseAddress (CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetGRBlobBaseAddress (CpuBootloaderAddress);
    default:
      return 0x0;
  }
}

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
ValidateActiveBootChain (
  VOID
  )
{
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = ValidateActiveBootChainInternal ();
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234ValidateActiveBootChain (CpuBootloaderAddress);
    case T194_CHIP_ID:
      return T194ValidateActiveBootChain (CpuBootloaderAddress);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Set next boot chain

**/
EFI_STATUS
EFIAPI
SetNextBootChain (
  IN  UINT32  BootChain
  )
{
  UINTN    ChipID;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = SetNextBootChainInternal (BootChain);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234SetNextBootChain (BootChain);
      break;
    case T194_CHIP_ID:
      return T194SetNextBootChain (BootChain);
      break;
    default:
      return EFI_UNSUPPORTED;
      break;
  }
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

  DEBUG ((DEBUG_ERROR, "MaxSockets=%u\n", PlatformResourceInfo->MaxPossibleSockets));

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

    DEBUG ((DEBUG_ERROR, "Socket=%u MaxClusters=%u\n", Socket, MaxClusters));
  }

  PlatformResourceInfo->MaxPossibleClusters = MaxClusters;
  DEBUG ((DEBUG_ERROR, "MaxClusters=%u\n", PlatformResourceInfo->MaxPossibleClusters));

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
  DEBUG ((DEBUG_ERROR, "MaxCoresPerCluster=%u\n", PlatformResourceInfo->MaxPossibleCoresPerCluster));
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
    "Invalid Socket %d >= %d \r\n",
    Socket,
    PlatformResourceInfo->MaxPossibleSockets
    );

  SocketCoreId = CpuIndex - (Socket * MaxCoresPerSocket);

  Cluster = SocketCoreId / PlatformResourceInfo->MaxPossibleCoresPerCluster;
  NV_ASSERT_RETURN (
    Cluster < PlatformResourceInfo->MaxPossibleClusters,
    return 0,
    "Invalid Cluster %d >= %d \r\n",
    Cluster,
    PlatformResourceInfo->MaxPossibleClusters
    );

  Core = SocketCoreId % PlatformResourceInfo->MaxPossibleCoresPerCluster;
  NV_ASSERT_RETURN (
    Core < PlatformResourceInfo->MaxPossibleCoresPerCluster,
    return 0,
    "Invalid Core %d >= %d \r\n",
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
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  BOOLEAN  ValidPrivatePlatform;

  PlatformResourceInfo->ResourceInfo = AllocateZeroPool (sizeof (TEGRA_RESOURCE_INFO));
  if (PlatformResourceInfo->ResourceInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  PlatformResourceInfo->BoardInfo = AllocateZeroPool (sizeof (TEGRA_BOARD_INFO));
  if (PlatformResourceInfo->BoardInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ValidPrivatePlatform = GetPlatformResourceInformationInternal (PlatformResourceInfo);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo);
    case T194_CHIP_ID:
      return T194GetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Update CPU info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
UpdatePlatformResourceCpuInformation (
  VOID
  )
{
  EFI_STATUS                    Status;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  UINTN                         ChipID;
  UINTN                         CoreIndex;
  UINTN                         CoreInfoIndex;
  ARM_CORE_INFO                 *ArmCoreInfo;

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

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      Status = T194GetEnabledCoresBitMap (PlatformResourceInfo);
      break;
    case T234_CHIP_ID:
      Status = T234GetEnabledCoresBitMap (PlatformResourceInfo);
      break;
    default:
      return EFI_UNSUPPORTED;
  }

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

EFI_STATUS
EFIAPI
GetRootfsStatusReg (
  IN UINT32  *RegisterValue
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    case T194_CHIP_ID:
      return T194GetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    default:
      return EFI_UNSUPPORTED;
  }
}

EFI_STATUS
EFIAPI
SetRootfsStatusReg (
  IN UINT32  RegisterValue
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234SetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    case T194_CHIP_ID:
      return T194SetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    default:
      return EFI_UNSUPPORTED;
  }
}
