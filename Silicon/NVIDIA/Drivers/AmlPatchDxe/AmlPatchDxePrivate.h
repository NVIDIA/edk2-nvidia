/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#ifndef __AML_PATCH_DXE_PRIVATE_H__
#define __AML_PATCH_DXE_PRIVATE_H__

#include <Protocol/AmlPatchProtocol.h>
#include <IndustryStandard/Acpi10.h>

typedef struct {
  UINT32                      Signature;
  EFI_ACPI_DESCRIPTION_HEADER **RegisteredAmlTables;
  AML_OFFSET_TABLE_ENTRY      **RegisteredOffsetTables;
  UINTN                       NumAmlTables;
  NVIDIA_AML_PATCH_PROTOCOL   AmlPatchProtocol;
} NVIDIA_AML_PATCH_PRIVATE_DATA;

#define NVIDIA_AML_PATCH_SIGNATURE SIGNATURE_32('A','M','L','P')

#define NVIDIA_AML_PATCH_PRIVATE_DATA_FROM_PROTOCOL(a)   CR(a, NVIDIA_AML_PATCH_PRIVATE_DATA, AmlPatchProtocol, NVIDIA_AML_PATCH_SIGNATURE)

#define AML_NAME_LENGTH 4

#endif  /* __AML_PATCH_DXE_PRIVATE_H__ */
