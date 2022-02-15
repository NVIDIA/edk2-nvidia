/** @file
  Clock node protocol Protocol

  Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CLOCK_NODE_PROTOCOL_H__
#define __CLOCK_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_CLOCK_NODE_PROTOCOL_GUID \
  { \
  0x6fa542ef, 0xec08, 0x4450, { 0xb1, 0x7b, 0xf6, 0x31, 0x5d, 0x32, 0xc5, 0x40 } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_CLOCK_NODE_PROTOCOL NVIDIA_CLOCK_NODE_PROTOCOL;

typedef struct {

  ///
  /// Id of the clock, matches what is used in clock SCMI protocol.
  ///
  UINT32                  ClockId;

  ///
  /// Clock name from device database, does not necessarily match SCMI protocol.
  ///
  CONST CHAR8             *ClockName;

  ///
  /// This clock is marked as a parent clock.
  ///
  BOOLEAN                 Parent;

} NVIDIA_CLOCK_NODE_ENTRY;

/**
  This function allows for simple enablement of all clock nodes.

  @param[in]     This                The instance of the NVIDIA_CLOCK_NODE_PROTOCOL.

  @return EFI_SUCCESS                All clocks enabled.
  @return EFI_NOT_READY              Clock control protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to enable all clocks
**/
typedef
EFI_STATUS
(EFIAPI *CLOCK_NODE_ENABLE_ALL) (
  IN  NVIDIA_CLOCK_NODE_PROTOCOL   *This
  );

/**
  This function allows for simple disablement of all clock nodes.

  @param[in]     This                The instance of the NVIDIA_CLOCK_NODE_PROTOCOL.

  @return EFI_SUCCESS                All clocks disabled.
  @return EFI_NOT_READY              Clock control protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to disable all clocks
**/
typedef
EFI_STATUS
(EFIAPI *CLOCK_NODE_DISABLE_ALL) (
  IN  NVIDIA_CLOCK_NODE_PROTOCOL   *This
  );

/// NVIDIA_CLOCK_NODE_PROTOCOL protocol structure.
struct _NVIDIA_CLOCK_NODE_PROTOCOL {

  CLOCK_NODE_ENABLE_ALL   EnableAll;
  CLOCK_NODE_DISABLE_ALL  DisableAll;
  UINTN                   Clocks;
  NVIDIA_CLOCK_NODE_ENTRY ClockEntries[0];
};

extern EFI_GUID gNVIDIAClockNodeProtocolGuid;

#endif
