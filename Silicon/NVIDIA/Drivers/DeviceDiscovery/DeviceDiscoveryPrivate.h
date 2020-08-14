/** @file

  Device discovery driver private data structures

  Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __DEVICE_DISCOVERY_PRIVATE_H__
#define __DEVICE_DISCOVERY_PRIVATE_H__

#include <PiDxe.h>

typedef struct {
  VOID       *DeviceTreeBase; //Address of the device tree
  UINTN      DeviceTreeSize;  //Size of device tree binary

  EFI_EVENT  ProtocolNotificationEvent;
  VOID       *SearchKey;
} DEVICE_DISCOVERY_PRIVATE;

#pragma pack (1)
typedef struct {
  MEMMAP_DEVICE_PATH                  MemMap;
  EFI_DEVICE_PATH_PROTOCOL            End;
} DEVICE_DISCOVERY_MEMMAP_DEVICE_PATH;
typedef struct {
  VENDOR_DEVICE_PATH                  Vendor;
  CONTROLLER_DEVICE_PATH              Controller;
  EFI_DEVICE_PATH_PROTOCOL            End;
} DEVICE_DISCOVERY_VENDOR_DEVICE_PATH;
#pragma pack ()

#define NUMBER_OF_OPTIONAL_PROTOCOLS 3

typedef enum {
  CmdResetAssert = 1,
  CmdResetDeassert = 2,
  CmdResetModule = 3,
  CmdResetGetMaxId = 4,
  CmdResetMax,
} MRQ_RESET_COMMANDS;

typedef enum {
  CmdPgAssert = 0,
  CmdPgDeassert = 1,
  CmdPgMax,
} MRQ_PG_COMMANDS;

#endif
