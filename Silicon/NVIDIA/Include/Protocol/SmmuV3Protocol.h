/** @file
  NVIDIA SMMMU V3 Controller Protocol

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_SMMU_V3_PROTOCOL_H__
#define __NVIDIA_SMMU_V3_PROTOCOL_H__

#define NVIDIA_SMMUV3_CONTROLLER_PROTOCOL_GUID \
  { \
  0xF6C64F84, 0x702C, 0x4BE7, { 0xA4, 0x1B, 0x64, 0xD5, 0xB5, 0x5F, 0x10, 0x1C } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_SMMUV3_CONTROLLER_PROTOCOL NVIDIA_SMMUV3_CONTROLLER_PROTOCOL;

/**
  Set SMMU attribute for a system memory.

  @param[in]  This              The protocol instance pointer.
  @param[in]  Mapping           The mapping value returned from Map().
  @param[in]  IoMmuAccess       The IOMMU access.
  @param[in]  StreamId          The StreamId.

  @retval EFI_SUCCESS            The IoMmuAccess is set for the memory range specified by DeviceAddress and Length.
  @retval EFI_INVALID_PARAMETER  Invalid Input Parameters.
  @retval EFI_UNSUPPORTED        The IoMmuAccess or the Mapping is not supported by the SMMU.
  @retval EFI_OUT_OF_RESOURCES   There are not enough resources available to modify the IOMMU access.
  @retval EFI_DEVICE_ERROR       The SMMU device reported an error while attempting the operation.

**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_SMMU_SET_ATTRIBUTE)(
  IN NVIDIA_SMMUV3_CONTROLLER_PROTOCOL  *This,
  IN VOID                               *Mapping,
  IN UINT64                             IoMmuAccess,
  IN UINT32                             StreamId
  );

/// NVIDIA_SMMUV3_CONTROLLER_PROTOCOL protocol structure.
struct _NVIDIA_SMMUV3_CONTROLLER_PROTOCOL {
  UINT32                       PHandle;
  NVIDIA_SMMU_SET_ATTRIBUTE    SetAttribute;
};

extern EFI_GUID  gNVIDIASmmuV3ProtocolGuid;

#endif
