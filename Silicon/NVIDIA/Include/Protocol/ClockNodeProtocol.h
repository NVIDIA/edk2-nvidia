/** @file
  Clock node protocol Protocol

  SPDX-FileCopyrightText: Copyright (c) 2018-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CLOCK_NODE_PROTOCOL_H__
#define __CLOCK_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_CLOCK_NODE_PROTOCOL_GUID \
  { \
  0xe33f1f78, 0xc32e, 0x4506, { 0x81, 0x6b, 0x14, 0xbf, 0xbe, 0x82, 0x81, 0x14 } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_CLOCK_NODE_PROTOCOL NVIDIA_CLOCK_NODE_PROTOCOL;

typedef struct {
  ///
  /// Id of the clock, matches what is used in clock SCMI protocol.
  ///
  UINT32         ClockId;

  ///
  /// Clock name from device database, does not necessarily match SCMI protocol.
  ///
  CONST CHAR8    *ClockName;

  ///
  /// This clock is marked as a parent clock.
  ///
  BOOLEAN        Parent;
} NVIDIA_CLOCK_NODE_ENTRY;

/**
  This function allows for simple enablement of clock nodes except those listed.

  @param[in]     This                The instance of the NVIDIA_CLOCK_NODE_PROTOCOL.
  @param[in]     ExceptionList       Optional NULL-terminated list of DT clock names
                                    to skip. NULL enables all clocks.

  @return EFI_SUCCESS                All non-excluded clocks enabled.
  @return EFI_NOT_READY              Clock control protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to enable all clocks
**/
typedef
EFI_STATUS
(EFIAPI *CLOCK_NODE_ENABLE_ALL)(
  IN  NVIDIA_CLOCK_NODE_PROTOCOL   *This,
  IN  CONST CHAR8 *CONST           *ExceptionList OPTIONAL
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
(EFIAPI *CLOCK_NODE_DISABLE_ALL)(
  IN  NVIDIA_CLOCK_NODE_PROTOCOL   *This
  );

/// NVIDIA_CLOCK_NODE_PROTOCOL protocol structure.
struct _NVIDIA_CLOCK_NODE_PROTOCOL {
  CLOCK_NODE_ENABLE_ALL      EnableAll;
  CLOCK_NODE_DISABLE_ALL     DisableAll;
  UINTN                      Clocks;
  NVIDIA_CLOCK_NODE_ENTRY    ClockEntries[0];
};

extern EFI_GUID  gNVIDIAClockNodeProtocolGuid;

#endif
