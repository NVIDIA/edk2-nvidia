/** @file
*  Provides functions that give information about the cores that are enabled
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef MP_CORE_INFO_LIB_H__
#define MP_CORE_INFO_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Gets the ProcessorId of the specified CPU. This should return the ProcessorId
  for each cpu for indexes between 0 and the number of cores enabled on system - 1.

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
  );

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
  );

/**
  Gets the location of the specified CPU

  @param[in]    ProcessorId     Id of the processor, for Arm processors this is mpidr
  @param[out]   Socket          Socket number of the processor
  @param[out]   Cluster         Cluster number of the processor
  @param[out]   Core            Core number of the processor
  @param[out]   Thread          Thread number of the processor

  @retval       EFI_SUCCESS             Processor location is returned
  @retval       EFI_INVALID_PARAMETER   Processor Id is invalid
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetProcessorLocation (
  IN  UINT64  ProcessorId,
  OUT UINT32  *Socket   OPTIONAL,
  OUT UINT32  *Cluster  OPTIONAL,
  OUT UINT32  *Core     OPTIONAL,
  OUT UINT32  *Thread   OPTIONAL
  );

/**
  Gets the Id from the location of the specified CPU

  @param[in]    Socket          Socket number of the processor
  @param[in]    Cluster         Cluster number of the processor
  @param[in]    Core            Core number of the processor
  @param[in]    Thread          Thread number of the processor
  @param[out]   ProcessorId     Id of the processor, for Arm processors this is mpidr

  @retval       EFI_SUCCESS             Processor location is returned
  @retval       EFI_INVALID_PARAMETER   Processor Id is NULL
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetProcessorIdFromLocation (
  IN  UINT32  Socket,
  IN  UINT32  Cluster,
  IN  UINT32  Core,
  IN  UINT32  Thread,
  OUT UINT64  *ProcessorId
  );

/**
  Gets information about processors on the platform.
  This returns the number of cores actually enabled as well as the max values
  for the location identifiers present.

  @param[out]   NumEnabledCores   Number of cores enabled
  @param[out]   MaxSocket         Max socket value (0-based)
  @param[out]   MaxCluster        Max cluster value (0-based)
  @param[out]   MaxCore           Max core value (0-based)
  @param[out]   MaxThread         Max thread value (0-based)

  @retval       EFI_SUCCESS         Platfrom info is returned
  @retval       EFI_DEVICE_ERROR    Other error
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetPlatformInfo (
  OUT UINT32  *NumEnabledCores    OPTIONAL,
  OUT UINT32  *MaxSocket          OPTIONAL,
  OUT UINT32  *MaxCluster         OPTIONAL,
  OUT UINT32  *MaxCore            OPTIONAL,
  OUT UINT32  *MaxThread          OPTIONAL
  );

/**
  Gets information about processors of a particular socket
  This returns the number of cores actually enabled as well as the max values
  for the location identifiers present for the specified socket.

  @param[in]    Socket            Socket id (0-based)
  @param[out]   NumEnabledCores   Number of cores enabled
  @param[out]   MaxCluster        Max cluster value (0-based)
  @param[out]   MaxCore           Max core value (0-based)
  @param[out]   MaxThread         Max thread value (0-based)
  @param[out]   FirstCoreId       First core id on this socket

  @retval       EFI_SUCCESS         Processor location is returned
  @retval       EFI_NOT_FOUND       No processors supported on this socket
  @retval       EFI_DEVICE_ERROR    Other error
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetSocketInfo (
  IN  UINT32  Socket,
  OUT UINT32  *NumEnabledCores    OPTIONAL,
  OUT UINT32  *MaxCluster         OPTIONAL,
  OUT UINT32  *MaxCore            OPTIONAL,
  OUT UINT32  *MaxThread          OPTIONAL,
  OUT UINT64  *FirstCoreId        OPTIONAL
  );

/**
  Gets information about processors of a particular cluster in a socket.
  This returns the number of cores actually enabled as well as the max values
  for the location identifiers present for the specified cluster.

  @param[in]    Socket            Socket id (0-based)
  @param[in]    Cluster           Cluster id (0-based)
  @param[out]   NumEnabledCores   Number of cores enabled
  @param[out]   MaxCore           Max core value (0-based)
  @param[out]   MaxThread         Max thread value (0-based)
  @param[out]   FirstCoreId       First core id on this socket

  @retval       EFI_SUCCESS         Processor location is returned
  @retval       EFI_NOT_FOUND       No processors supported on this socket/cluster
  @retval       EFI_DEVICE_ERROR    Other error
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetSocketClusterInfo (
  IN  UINT32  Socket,
  IN  UINT32  Cluster,
  OUT UINT32  *NumEnabledCores    OPTIONAL,
  OUT UINT32  *MaxCore            OPTIONAL,
  OUT UINT32  *MaxThread          OPTIONAL,
  OUT UINT64  *FirstCoreId        OPTIONAL
  );

/**
  Get the first enabled socket

  @retval  First enabled socket
**/
UINT32
EFIAPI
MpCoreInfoGetFirstEnabledSocket (
  VOID
  );

/**
  Get the next enabled socket

  @param[in, out]  SocketId  Socket index. On input the last socket id, on output the next enabled socket id
                             if error is returned, SocketId is set to MAX_UINT32

  @retval  EFI_SUCCESS - Socket found
  @retval  EFI_NOT_FOUND - No more sockets
  @retval  EFI_INVALID_PARAMETER - SocketId is NULL
  @retval  EFI_DEVICE_ERROR - Failed to get platform info
**/
EFI_STATUS
EFIAPI
MpCoreInfoGetNextEnabledSocket (
  IN OUT UINT32  *SocketId
  );

// Used to iterate over all enabled sockets, this enumerates all sockets that have at least one enabled core
#define MPCORE_FOR_EACH_ENABLED_SOCKET(SocketId) \
  for (SocketId = MpCoreInfoGetFirstEnabledSocket(); \
       SocketId != MAX_UINT32; \
       MpCoreInfoGetNextEnabledSocket(&SocketId))

#endif // MP_CORE_INFO_LIB_H__
