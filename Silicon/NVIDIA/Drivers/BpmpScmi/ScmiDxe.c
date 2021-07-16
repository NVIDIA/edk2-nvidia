/** @file

  Copyright (c) 2017-2018, Arm Limited. All rights reserved.
  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

  System Control and Management Interface V1.0
    http://infocenter.arm.com/help/topic/com.arm.doc.den0056a/
    DEN0056A_System_Control_and_Management_Interface.pdf
**/

#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/ArmScmiClockProtocol.h>

#include "BpmpScmiClockProtocolPrivate.h"
#include "ScmiDxe.h"

STATIC CONST SCMI_PROTOCOL_INIT_FXN Protocols[] = {
  ScmiClockProtocolInit
};

/** BPMP driver entry point function.

  This function installs the SCMI protocols implemented using BpmpIpc.

  @param[in] ImageHandle     Handle to this EFI Image which will be used to
                             install Base, Clock and Performance protocols.
  @param[in] SystemTable     A pointer to boot time system table.

  @retval EFI_SUCCESS       Driver initalized successfully.
  @retval !(EFI_SUCCESS)    Other errors.
**/
EFI_STATUS
EFIAPI
BpmpScmiDxeEntryPoint (
  IN EFI_HANDLE             ImageHandle,
  IN EFI_SYSTEM_TABLE       *SystemTable
  )
{
  EFI_STATUS          Status;
  UINT32              ProtocolIndex;

  // Install supported protocol on ImageHandle.
  for (ProtocolIndex = 0; ProtocolIndex < ARRAY_SIZE (Protocols);
       ProtocolIndex++) {
      Status = Protocols[ProtocolIndex] (&ImageHandle);
      if (EFI_ERROR (Status)) {
        ASSERT_EFI_ERROR (Status);
        return Status;
      }
  }

  return EFI_SUCCESS;
}
