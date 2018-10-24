/** @file
  Clock parents Protocol

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __CLOCK_PARENTS_PROTOCOL_H__
#define __CLOCK_PARENTS_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_PARENTS_NODE_PROTOCOL_GUID \
  { \
  0x26d3a358, 0xa8eb, 0x4f14, { 0x84, 0x0c, 0x09, 0xa2, 0x5b, 0xc4, 0xaa, 0x88 } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_CLOCK_PARENTS_PROTOCOL NVIDIA_CLOCK_PARENTS_PROTOCOL;

/**
  This function checks is the given parent is a parent of the specified clock.

  @param[in]     This                The instance of the NVIDIA_CLOCK_PARENTS_PROTOCOL.
  @param[in]     ClockId             ClockId to check parent against
  @param[in]     ParentId            ClockId of the parent

  @return EFI_SUCCESS                Parent is supported by clock
  @return EFI_NOT_FOUND              Parent is not supported by clock.
  @return others                     Failed to check if parent is supported
**/
typedef
EFI_STATUS
(EFIAPI *CLOCK_PARENTS_IS_PARENT) (
  IN  NVIDIA_CLOCK_PARENTS_PROTOCOL *This,
  IN  UINT32                        ClockId,
  IN  UINT32                        ParentId
  );

/**
  This function sets the parent for the specified clock.

  @param[in]     This                The instance of the NVIDIA_CLOCK_PARENTS_PROTOCOL.
  @param[in]     ClockId             ClockId to set parent for
  @param[in]     ParentId            ClockId of the parent

  @return EFI_SUCCESS                Parent is set for clock
  @return EFI_NOT_FOUND              Parent is not supported by clock.
  @return others                     Failed to set parent
**/
typedef
EFI_STATUS
(EFIAPI *CLOCK_PARENTS_SET_PARENT) (
  IN  NVIDIA_CLOCK_PARENTS_PROTOCOL *This,
  IN  UINT32                        ClockId,
  IN  UINT32                        ParentId
  );

/**
  This function gets the current parent of the specified clock.

  @param[in]     This                The instance of the NVIDIA_CLOCK_PARENTS_PROTOCOL.
  @param[in]     ClockId             ClockId to check parent of
  @param[out]    ParentId            ClockId of the parent

  @return EFI_SUCCESS                Parent is supported by clock
  @return others                     Failed to get parent
**/
typedef
EFI_STATUS
(EFIAPI *CLOCK_PARENTS_GET_PARENT) (
  IN  NVIDIA_CLOCK_PARENTS_PROTOCOL *This,
  IN  UINT32                        ClockId,
  OUT UINT32                        *ParentId
  );

/**
  This function gets the supported parents of the specified clock.

  @param[in]     This                The instance of the NVIDIA_CLOCK_PARENTS_PROTOCOL.
  @param[in]     ClockId             ClockId to check parents of.
  @param[out]    NumberOfParents     Number of parents supported
  @param[out]    ParentIds           Array of parent clock IDs supported. The caller should free this buffer.

  @return EFI_SUCCESS                Parent list is retrieved
  @return others                     Failed to get parent list
**/
typedef
EFI_STATUS
(EFIAPI *CLOCK_PARENTS_GET_PARENTS) (
  IN  NVIDIA_CLOCK_PARENTS_PROTOCOL *This,
  IN  UINT32                        ClockId,
  OUT UINT32                        *NumberOfParents,
  OUT UINT32                        **ParentIds
  );

/// NVIDIA_CLOCK_PARENT_PROTOCOL protocol structure.
struct _NVIDIA_CLOCK_PARENTS_PROTOCOL {

  CLOCK_PARENTS_IS_PARENT   IsParent;
  CLOCK_PARENTS_SET_PARENT  SetParent;
  CLOCK_PARENTS_GET_PARENT  GetParent;
  CLOCK_PARENTS_GET_PARENTS GetParents;
};

extern EFI_GUID gNVIDIAClockParentsProtocolGuid;

#endif
