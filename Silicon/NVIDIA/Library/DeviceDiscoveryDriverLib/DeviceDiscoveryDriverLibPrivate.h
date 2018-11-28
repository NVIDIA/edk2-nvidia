/** @file

  Device Discovery Driver Library private structures

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __DEVICE_DISCOVERY_LIBRARY_PRIVATE_H__
#define __DEVICE_DISCOVERY_LIBRARY_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/ClockParents.h>

extern SCMI_CLOCK2_PROTOCOL          *gScmiClockProtocol;
extern NVIDIA_CLOCK_PARENTS_PROTOCOL *gClockParentsProtocol;

#endif
