/** @file

  Device Discovery Driver Library private structures

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DEVICE_DISCOVERY_LIBRARY_PRIVATE_H__
#define __DEVICE_DISCOVERY_LIBRARY_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/ClockParents.h>

extern SCMI_CLOCK2_PROTOCOL          *gScmiClockProtocol;
extern NVIDIA_CLOCK_PARENTS_PROTOCOL *gClockParentsProtocol;

typedef struct {
  EFI_EVENT OnExitBootServicesEvent;
} NVIDIA_DEVICE_DISCOVERY_CONTEXT;

#endif
