/** @file

  Numa Information Library

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef NUMA_INFO_LIB__
#define NUMA_INFO_LIB__

#include <Uefi/UefiBaseType.h>
#include <IndustryStandard/Acpi65.h>

typedef enum {
  NUMA_INFO_TYPE_CPU,
  NUMA_INFO_TYPE_HV,
  NUMA_INFO_TYPE_GPU,
  NUMA_INFO_TYPE_MAX
} NUMA_INFO_TYPE;

typedef struct {
  UINT32                        ProximityDomain;
  UINT32                        SocketId;
  NUMA_INFO_TYPE                DeviceType;
  UINT8                         DeviceHandleType;
  EFI_ACPI_6_5_DEVICE_HANDLE    DeviceHandle;
  BOOLEAN                       InitiatorDomain;
  BOOLEAN                       TargetDomain;
} NUMA_INFO_DOMAIN_INFO;

/**
  Returns limits of the proximity domains

  @param[out]   MaxProximityDomain        Pointer to Maximum Numa domain number
  @param[out]   NumberOfInitiatorDomains  Pointer to Number of Initiator domains
  @param[out]   NumberOfTargetDomains     Pointer to Number of Target domains

  @retval       EFI_SUCCESS     Limits returned successfully
  @retval       EFI_NOT_FOUND   Limits not found

**/
EFI_STATUS
EFIAPI
NumaInfoGetDomainLimits (
  OUT UINT32  *MaxProximityDomain,
  OUT UINT32  *NumberOfInitiatorDomains,
  OUT UINT32  *NumberOfTargetDomains
  );

/**
  Returns the Numa info for a given domain

  @param[in]    ProximityDomain     Proximity domain number
  @param[out]   DomainInfo          Pointer to NUMA_INFO_DOMAIN_INFO structure

  @retval       EFI_SUCCESS     Info returned successfully
  @retval       EFI_NOT_FOUND   Domain not found

**/
EFI_STATUS
EFIAPI
NumaInfoGetDomainDetails (
  IN  UINT32                 ProximityDomain,
  OUT NUMA_INFO_DOMAIN_INFO  *DomainInfo
  );

/**
  Returns the distance between two domains

  @param[in]    InitiatorDomain     Initiator domain number
  @param[in]    TargetDomain        Target domain number
  @param[out]   NormalizedDistance  Pointer to Normalized distance
  @param[out]   ReadLatency         Pointer to Read latency
  @param[out]   WriteLatency        Pointer to Write latency
  @param[out]   AccessBandwidth     Pointer to Access bandwidth

  @retval       EFI_SUCCESS     Distance returned successfully
  @retval       EFI_NOT_FOUND   Domain not found
 */

EFI_STATUS
EFIAPI
NumaInfoGetDistances (
  IN  UINT32  InitiatorDomain,
  IN  UINT32  TargetDomain,
  OUT UINT8   *NormalizedDistance OPTIONAL,
  OUT UINT16  *ReadLatency OPTIONAL,
  OUT UINT16  *WriteLatency OPTIONAL,
  OUT UINT16  *AccessBandwidth OPTIONAL
  );

#endif
