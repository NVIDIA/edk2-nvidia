/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __AML_GENERATION_DXE_PRIVATE_H__
#define __AML_GENERATION_DXE_PRIVATE_H__

#include <Protocol/AmlGenerationProtocol.h>
#include <IndustryStandard/Acpi10.h>

typedef struct {
  UINT32                          Signature;
  EFI_ACPI_DESCRIPTION_HEADER     *CurrentTable;
  VOID                            *ScopeStart;
  NVIDIA_AML_GENERATION_PROTOCOL  AmlGenerationProtocol;
} NVIDIA_AML_GENERATION_PRIVATE_DATA;

#define NVIDIA_AML_GENERATION_SIGNATURE SIGNATURE_32('A','M','L','G')

#define NVIDIA_AML_GENERATION_PRIVATE_DATA_FROM_PROTOCOL(a)   CR(a, NVIDIA_AML_GENERATION_PRIVATE_DATA, AmlGenerationProtocol, NVIDIA_AML_GENERATION_SIGNATURE)

#define AML_NAME_LENGTH 4

#pragma pack(1)
typedef PACKED struct {
  UINT8   OpCode;
  UINT32  PkgLength;
  CHAR8   Name[AML_NAME_LENGTH];
} AML_SCOPE_HEADER;
#pragma pack()

#endif  /* __AML_GENERATION_DXE_PRIVATE_H__ */
