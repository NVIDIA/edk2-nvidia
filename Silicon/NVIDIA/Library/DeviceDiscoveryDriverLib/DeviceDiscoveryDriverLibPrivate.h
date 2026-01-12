/** @file

  Device Discovery Driver Library private structures

  SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DEVICE_DISCOVERY_LIBRARY_PRIVATE_H__
#define __DEVICE_DISCOVERY_LIBRARY_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/ClockParents.h>
#include <Library/SystemFiberLib.h>

extern SCMI_CLOCK2_PROTOCOL           *gScmiClockProtocol;
extern NVIDIA_CLOCK_PARENTS_PROTOCOL  *gClockParentsProtocol;

#define THREAD_STACK_SIZE  SIZE_64KB

typedef struct {
  EFI_EVENT    OnExitBootServicesEvent;
} NVIDIA_DEVICE_DISCOVERY_CONTEXT;

typedef struct {
  EFI_PHYSICAL_ADDRESS                   StackBase;
  EFI_EVENT                              Timer;
  SYSTEM_FIBER                           Fiber;
  EFI_HANDLE                             DriverHandle;
  EFI_HANDLE                             Controller;
  IN NVIDIA_DEVICE_TREE_NODE_PROTOCOL    *Node;
} NVIDIA_DEVICE_DISCOVERY_THREAD_CONTEXT;

// Make the DEBUG prints in this Library print the name of the Driver that called them
#ifdef _DEBUG_PRINT
  #undef _DEBUG_PRINT
#define _DEBUG_PRINT(PrintLevel, ...)              \
    do {                                             \
      if (DebugPrintLevelEnabled (PrintLevel)) {     \
        DebugPrint (PrintLevel, "%a:", gEfiCallerBaseName); \
        DebugPrint (PrintLevel, ##__VA_ARGS__);      \
      }                                              \
    } while (FALSE)
#endif

#endif
