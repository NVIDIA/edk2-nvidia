/** @file
*  Provides functions that give information about the cores that are enabled
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>
#include <Library/HobLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Guid/ArmMpCoreInfo.h>

#include "MpCoreInfoLib_private.h"

STATIC CONST ARM_CORE_INFO                 *mArmCoreInfo  = NULL;
STATIC UINT32                              mCoresPresent  = 0;
STATIC CONST TEGRA_PLATFORM_RESOURCE_INFO  *mResourceInfo = NULL;

#define GET_AFFINITY_BASED_MPID(Aff3, Aff2, Aff1, Aff0)         \
  (((UINT64)(Aff3) << 32) | ((Aff2) << 16) | ((Aff1) << 8) | (Aff0))
//
// ARM MP Core IDs
//
#define ARM_CORE_AFF0  0xFF
#define ARM_CORE_AFF1  (0xFF << 8)
#define ARM_CORE_AFF2  (0xFF << 16)
#define ARM_CORE_AFF3  (0xFFULL << 32)
#define GET_MPIDR_AFF0(MpId)  ((MpId) & ARM_CORE_AFF0)
#define GET_MPIDR_AFF1(MpId)  (((MpId) & ARM_CORE_AFF1) >> 8)
#define GET_MPIDR_AFF2(MpId)  (((MpId) & ARM_CORE_AFF2) >> 16)
#define GET_MPIDR_AFF3(MpId)  (((MpId) & ARM_CORE_AFF3) >> 32)

// Utility function to reset library globals
EFIAPI
VOID
MpCoreInfoLibResetModule (
  VOID
  )
{
  mArmCoreInfo  = NULL;
  mCoresPresent = 0;
  mResourceInfo = NULL;
}

STATIC
EFI_STATUS
EFIAPI
GetCoreInfoFromHob (
  VOID
  )
{
  EFI_HOB_GENERIC_HEADER  *Hob;
  VOID                    *HobData;
  UINTN                   HobDataSize;

  if (mArmCoreInfo != NULL) {
    return EFI_SUCCESS;
  }

  Hob = GetFirstGuidHob (&gArmMpCoreInfoGuid);
  if (Hob != NULL) {
    HobData       = GET_GUID_HOB_DATA (Hob);
    HobDataSize   = GET_GUID_HOB_DATA_SIZE (Hob);
    mArmCoreInfo  = (ARM_CORE_INFO *)HobData;
    mCoresPresent = HobDataSize / sizeof (ARM_CORE_INFO);
    return EFI_SUCCESS;
  } else {
    return EFI_DEVICE_ERROR;
  }
}

STATIC
BOOLEAN
EFIAPI
IsAffinityMpIdrSupported (
  VOID
  )
{
  VOID  *Hob;

  if (mResourceInfo != NULL) {
    return mResourceInfo->AffinityMpIdrSupported;
  } else {
    Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
    if ((Hob != NULL) &&
        (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
    {
      mResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
      return mResourceInfo->AffinityMpIdrSupported;
    }

    return FALSE;
  }
}

/**
  Gets the ProcessorId of the specified CPU. This should return the ProcessorId
  for each cpu for indexes between 0 and the number of cores enabled on system.

  @param[in]    Index           0 based index of the core to get information on
  @param[out]   ProcessorId     Id of the processor, for Arm processors this is mpidr

  @retval       EFI_SUCCESS             Processor Id returned
  @retval       EFI_NOT_FOUND           Index is not a supported cpu
  @retval       EFI_DEVICE_ERROR        Cpu Info not found
  @retval       EFI_INVALID_PARAMETER   ProcessorId is NULL
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetProcessorIdFromIndex (
  IN  UINT32  Index,
  OUT UINT64  *ProcessorId
  )
{
  EFI_STATUS  Status;

  if (ProcessorId == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetCoreInfoFromHob ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Index < mCoresPresent) {
    *ProcessorId = mArmCoreInfo[Index].Mpidr;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

/**
  Checks if a processor id is enabled

  @param[in]   ProcessorId     Id of the processor, for Arm processors this is mpidr

  @retval       EFI_SUCCESS       Processor Id is supported
  @retval       EFI_DEVICE_ERROR  Cpu Info not found
  @retval       EFI_NOT_FOUND     Processor Id is not enabled
**/
EFI_STATUS
EFIAPI
MpCoreInfoIsProcessorEnabled (
  IN  UINT64  ProcessorId
  )
{
  EFI_STATUS  Status;
  UINT32      Index;

  Status = GetCoreInfoFromHob ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < mCoresPresent; Index++) {
    if (ProcessorId == mArmCoreInfo[Index].Mpidr) {
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Gets the location of the specified CPU

  @param[in]    ProcessorId     Id of the processor, for Arm processors this is mpidr
  @param[out]   Socket          Socket number of the processor
  @param[out]   Cluster         Cluster number of the processor
  @param[out]   Core            Core number of the processor

  @retval       EFI_SUCCESS             Processor location is returned
  @retval       EFI_INVALID_PARAMETER   Processor Id is invalid
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetProcessorLocation (
  IN  UINT64  ProcessorId,
  OUT UINT32  *Socket   OPTIONAL,
  OUT UINT32  *Cluster  OPTIONAL,
  OUT UINT32  *Core     OPTIONAL
  )
{
  UINT32  LocalCore;
  UINT32  LocalCluster;
  UINT32  LocalSocket;

  if (!IsAffinityMpIdrSupported ()) {
    LocalCore    = GET_MPIDR_AFF0 (ProcessorId);
    LocalCluster = GET_MPIDR_AFF1 (ProcessorId);
    LocalSocket  = GET_MPIDR_AFF2 (ProcessorId);
  } else {
    LocalCore    = GET_MPIDR_AFF1 (ProcessorId);
    LocalCluster = GET_MPIDR_AFF2 (ProcessorId);
    LocalSocket  = GET_MPIDR_AFF3 (ProcessorId);
  }

  if (Socket != NULL) {
    *Socket = LocalSocket;
  }

  if (Cluster != NULL) {
    *Cluster = LocalCluster;
  }

  if (Core != NULL) {
    *Core = LocalCore;
  }

  return EFI_SUCCESS;
}

/**
  Gets the Id from the location of the specified CPU

  @param[in]    Socket          Socket number of the processor
  @param[in]    Cluster         Cluster number of the processor
  @param[in]    Core            Core number of the processor
  @param[out]   ProcessorId     Id of the processor, for Arm processors this is mpidr

  @retval       EFI_SUCCESS             Processor location is returned
  @retval       EFI_INVALID_PARAMETER   Socket,Cluster, or Core values are not correct
  @retval       EFI_INVALID_PARAMETER   Processor Id is NULL
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetProcessorIdFromLocation (
  IN  UINT32  Socket,
  IN  UINT32  Cluster,
  IN  UINT32  Core,
  OUT UINT64  *ProcessorId
  )
{
  if (Socket > MAX_UINT8) {
    return EFI_INVALID_PARAMETER;
  }

  if (Cluster > MAX_UINT8) {
    return EFI_INVALID_PARAMETER;
  }

  if (Core > MAX_UINT8) {
    return EFI_INVALID_PARAMETER;
  }

  if (ProcessorId == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!IsAffinityMpIdrSupported ()) {
    *ProcessorId = GET_AFFINITY_BASED_MPID (0, Socket, Cluster, Core);
  } else {
    *ProcessorId = GET_AFFINITY_BASED_MPID (Socket, Cluster, Core, 0);
  }

  return EFI_SUCCESS;
}

/**
  Gets information about processors of a particular socket, a particular
  cluster in a socket, or the whole patform.
  This returns the number of cores actually enabled as well as the max values
  for the location identifiers present for the specified search.

  @param[in]    IsSocketSpecified Is socket used to match
  @param[in]    SocketToMatch     Socket id (0-based)
  @param[in]    IsClusterSpecified Is cluster used to match
  @param[in]    ClusterToMatch     Cluster id (0-based)
  @param[out]   NumEnabledCores   Number of cores enabled
  @param[out]   MaxSocket         Max socket value (0-based)
  @param[out]   MaxCluster        Max cluster value (0-based)
  @param[out]   MaxCore           Max core value (0-based)
  @param[out]   FirstCoreId       First core id on this socket/cluster

  @retval       EFI_SUCCESS         Processor location is returned
  @retval       EFI_NOT_FOUND       No processors supported on this socket
  @retval       EFI_INVALID_PARAMETER IsClusterSpecified was set without IsSocketSpecified
  @retval       EFI_DEVICE_ERROR    Other error
**/
STATIC
EFI_STATUS
EFIAPI
MpCoreInfoGetInfoCommon (
  IN BOOLEAN  IsSocketSpecified,
  IN  UINT32  SocketToMatch,
  IN BOOLEAN  IsClusterSpecified,
  IN  UINT32  ClusterToMatch,
  OUT UINT32  *NumEnabledCores  OPTIONAL,
  OUT UINT32  *MaxSocket        OPTIONAL,
  OUT UINT32  *MaxCluster       OPTIONAL,
  OUT UINT32  *MaxCore          OPTIONAL,
  OUT UINT64  *FirstCoreId      OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINT32      CoreCount;
  UINT32      SocketIdMax;
  UINT32      ClusterIdMax;
  UINT32      CoreIdMax;
  UINT64      FirstMatchedCore;
  UINT32      Index;
  UINT64      ProcessorId;
  UINT32      Socket;
  UINT32      Cluster;
  UINT32      Core;

  CoreCount        = 0;
  SocketIdMax      = 0;
  ClusterIdMax     = 0;
  CoreIdMax        = 0;
  FirstMatchedCore = 0;
  Index            = 0;

  if (IsClusterSpecified && !IsSocketSpecified) {
    return EFI_INVALID_PARAMETER;
  }

  while (!EFI_ERROR (MpCoreInfoGetProcessorIdFromIndex (Index, &ProcessorId))) {
    Index++;
    Status = MpCoreInfoGetProcessorLocation (ProcessorId, &Socket, &Cluster, &Core);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (IsSocketSpecified && (Socket != SocketToMatch)) {
      continue;
    }

    if (IsClusterSpecified && (Cluster != ClusterToMatch)) {
      continue;
    }

    if (CoreCount == 0) {
      FirstMatchedCore = ProcessorId;
    }

    if (Socket > SocketIdMax) {
      SocketIdMax = Socket;
    }

    if (Cluster > ClusterIdMax) {
      ClusterIdMax = Cluster;
    }

    if (Core > CoreIdMax) {
      CoreIdMax = Core;
    }

    CoreCount++;
  }

  if (CoreCount == 0) {
    return EFI_NOT_FOUND;
  }

  if (NumEnabledCores != NULL) {
    *NumEnabledCores = CoreCount;
  }

  if (MaxSocket != NULL) {
    *MaxSocket = SocketIdMax;
  }

  if (MaxCluster != NULL) {
    *MaxCluster = ClusterIdMax;
  }

  if (MaxCore != NULL) {
    *MaxCore = CoreIdMax;
  }

  if (FirstCoreId != NULL) {
    *FirstCoreId = FirstMatchedCore;
  }

  return EFI_SUCCESS;
}

/**
  Gets information about processors on the platform.
  This returns the number of cores actually enabled as well as the max values
  for the location identifiers present.

  @param[out]   NumEnabledCores Number of cores enabled
  @param[out]   MaxSocket       Max socket value (0-based)
  @param[out]   MaxCluster      Max cluster value (0-based)
  @param[out]   MaxCore         Max core value (0-based)

  @retval       EFI_SUCCESS         Platfrom info is returned
  @retval       EFI_DEVICE_ERROR    Other error
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetPlatformInfo (
  OUT UINT32  *NumEnabledCores  OPTIONAL,
  OUT UINT32  *MaxSocket        OPTIONAL,
  OUT UINT32  *MaxCluster       OPTIONAL,
  OUT UINT32  *MaxCore          OPTIONAL
  )
{
  return MpCoreInfoGetInfoCommon (
           FALSE,
           0,
           FALSE,
           0,
           NumEnabledCores,
           MaxSocket,
           MaxCluster,
           MaxCore,
           NULL
           );
}

/**
  Gets information about processors of a particular socket
  This returns the number of cores actually enabled as well as the max values
  for the location identifiers present for the specified socket.

  @param[in]    Socket          Socket id (0-based)
  @param[out]   NumEnabledCores Number of cores enabled
  @param[out]   MaxCluster      Max cluster value (0-based)
  @param[out]   MaxCore         Max core value (0-based)
  @param[out]   FirstCoreId     First core id on this socket

  @retval       EFI_SUCCESS         Processor location is returned
  @retval       EFI_NOT_FOUND       No processors supported on this socket
  @retval       EFI_DEVICE_ERROR    Other error
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetSocketInfo (
  IN  UINT32  Socket,
  OUT UINT32  *NumEnabledCores  OPTIONAL,
  OUT UINT32  *MaxCluster       OPTIONAL,
  OUT UINT32  *MaxCore          OPTIONAL,
  OUT UINT64  *FirstCoreId      OPTIONAL
  )
{
  return MpCoreInfoGetInfoCommon (
           TRUE,
           Socket,
           FALSE,
           0,
           NumEnabledCores,
           NULL,
           MaxCluster,
           MaxCore,
           FirstCoreId
           );
}

/**
  Gets information about processors of a particular cluster in a socket.
  This returns the number of cores actually enabled as well as the max values
  for the location identifiers present for the specified cluster.

  @param[in]    Socket          Socket id (0-based)
  @param[in]    Cluster         Cluster id (0-based)
  @param[out]   NumEnabledCores Number of cores enabled
  @param[out]   MaxCore         Max core value (0-based)
  @param[out]   FirstCoreId     First core id on this socket

  @retval       EFI_SUCCESS         Processor location is returned
  @retval       EFI_NOT_FOUND       No processors supported on this socket/cluster
  @retval       EFI_DEVICE_ERROR    Other error
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetSocketClusterInfo (
  IN  UINT32  Socket,
  IN  UINT32  Cluster,
  OUT UINT32  *NumEnabledCores  OPTIONAL,
  OUT UINT32  *MaxCore          OPTIONAL,
  OUT UINT64  *FirstCoreId      OPTIONAL
  )
{
  return MpCoreInfoGetInfoCommon (
           TRUE,
           Socket,
           TRUE,
           Cluster,
           NumEnabledCores,
           NULL,
           NULL,
           MaxCore,
           FirstCoreId
           );
}
