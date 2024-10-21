/** @file

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#ifndef CONFIGURATION_MANAGER_DATA_PROTOCOL_H__
#define CONFIGURATION_MANAGER_DATA_PROTOCOL_H__

#define NVIDIA_CONFIGURATION_MANAGER_DATA_PROTOCOL_GUID \
  { \
  0x1a8fd893, 0x4752, 0x40b9, { 0x9b, 0xc7, 0x75, 0x94, 0x04, 0xff, 0xcd, 0xff } \
  }

/** A structure describing the platform configuration
    manager repository information
*/
typedef struct ProtocolParserEntry {
  // Configuration Manager Object ID
  CM_OBJECT_ID       CmObjectId;

  // Configuration Manager Object Token
  CM_OBJECT_TOKEN    CmObjectToken;

  // Configuration Manager Object Size
  UINT32             CmObjectSize;

  // Configuration Manager Object Count
  UINT32             CmObjectCount;

  // Configuration Manager Object Pointer
  VOID               *CmObjectPtr;
} LEGACY_CM_PROTOCOL_OBJECT;

extern EFI_GUID  gNVIDIAConfigurationManagerDataProtocolGuid;

#endif // CONFIGURATION_MANAGER_DATA_PROTOCOL_H__
