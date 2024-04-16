/** @file

  Redfish chassis information collector. - internal header file

  (C) Copyright 2020-2022 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EFI_REDFISH_CHASSIS_INFO_H_
#define _EFI_REDFISH_CHASSIS_INFO_H_

#include <RedfishResourceCommon.h>

//
// Redfish initialization info.
//
#define REDFISH_MANAGED_URI          L"Chassis"
#define MAX_URI_LENGTH               256
#define MAX_CHASSIS_INFO_NODE_COUNT  32

typedef union {
  struct {
    UINT32    EfiVariableWriteOnceFlag : 1;
    UINT32    EfiVariableLockFlag      : 1;
    UINT32    Reserved_0               : 2;
    UINT32    EfiVariableAttributes    : 12;
    UINT32    EdkiiJsonType            : 8;
    UINT32    Reserved_1               : 8;
  } Bits;         ///< Bitfield definition of the register
  UINT32    Data; ///< The entire 32-bit value
} CHASSIS_INFO_PROP_ATTR;

EFI_STATUS
HandleResource (
  IN  REDFISH_RESOURCE_COMMON_PRIVATE  *Private,
  IN  EFI_STRING                       Uri
  );

#endif
