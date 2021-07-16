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
*  Portions provided under the following terms:
*  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
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
