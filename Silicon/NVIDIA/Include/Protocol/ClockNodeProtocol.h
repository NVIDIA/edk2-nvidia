/** @file
  Clock node protocol Protocol

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __CLOCK_NODE_PROTOCOL_H__
#define __CLOCK_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_CLOCK_NODE_PROTOCOL_GUID \
  { \
  0x50b572fb, 0x13bc, 0x4897, { 0xa6, 0x47, 0x9f, 0x14, 0xbb, 0xad, 0x22, 0x6b } \
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

/// NVIDIA_CLOCK_NODE_PROTOCOL protocol structure.
struct _NVIDIA_CLOCK_NODE_PROTOCOL {

  CLOCK_NODE_ENABLE_ALL   EnableAll;
  UINTN                   Clocks;
  NVIDIA_CLOCK_NODE_ENTRY ClockEntries[0];
};

extern EFI_GUID gNVIDIAClockNodeProtocolGuid;

#endif
