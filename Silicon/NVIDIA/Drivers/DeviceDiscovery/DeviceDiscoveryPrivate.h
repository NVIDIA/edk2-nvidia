/** @file

  Device discovery driver private data structures

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DEVICE_DISCOVERY_PRIVATE_H__
#define __DEVICE_DISCOVERY_PRIVATE_H__

#include <PiDxe.h>

typedef struct {
  VOID         *DeviceTreeBase; // Address of the device tree
  UINTN        DeviceTreeSize;  // Size of device tree binary

  EFI_EVENT    ProtocolNotificationEvent;
  VOID         *SearchKey;
} DEVICE_DISCOVERY_PRIVATE;

#pragma pack (1)
typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  MEMMAP_DEVICE_PATH          MemMap;
  EFI_DEVICE_PATH_PROTOCOL    End;
} DEVICE_DISCOVERY_MEMMAP_DEVICE_PATH;
typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  CONTROLLER_DEVICE_PATH      Controller;
  EFI_DEVICE_PATH_PROTOCOL    End;
} DEVICE_DISCOVERY_CONTROLLER_DEVICE_PATH;
typedef union {
  DEVICE_DISCOVERY_MEMMAP_DEVICE_PATH        MemMap;
  DEVICE_DISCOVERY_CONTROLLER_DEVICE_PATH    Controller;
} DEVICE_DISCOVERY_DEVICE_PATH;
#pragma pack ()

#define NUMBER_OF_OPTIONAL_PROTOCOLS  4

typedef enum {
  CmdResetAssert   = 1,
  CmdResetDeassert = 2,
  CmdResetModule   = 3,
  CmdResetGetMaxId = 4,
  CmdResetMax,
} MRQ_RESET_COMMANDS;

typedef enum {
  CmdPgQueryAbi = 0,
  CmdPgSetState = 1,
  CmdPgGetState = 2,
  CmdPgMax,
} MRQ_PG_COMMANDS;

typedef enum {
  CmdC2cQueryAbi            = 0,
  CmdC2cStartInitialization = 1,
  CmdC2cGetStatus           = 2,
  CmdC2cHotresetPrep        = 3,
  CmdC2cStartHotreset       = 4,
  CmdC2cMax,
} MRQ_C2C_COMMANDS;

#pragma pack (1)
typedef struct {
  UINT32    Command;
  UINT32    PgId;
  UINT32    Argument;
} MRQ_PG_COMMAND_PACKET;

typedef struct {
  UINT32    Command;
  UINT8     Partitions;
} MRQ_C2C_COMMAND_PACKET;
#pragma pack ()

#endif
