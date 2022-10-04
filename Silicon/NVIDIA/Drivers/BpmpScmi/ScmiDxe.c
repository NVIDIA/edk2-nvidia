/** @file

  Copyright (c) 2017-2018, Arm Limited. All rights reserved.
  Copyright (c) 2018, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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

STATIC CONST SCMI_PROTOCOL_INIT_FXN  Protocols[] = {
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
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT32      ProtocolIndex;

  // Install supported protocol on ImageHandle.
  for (ProtocolIndex = 0; ProtocolIndex < ARRAY_SIZE (Protocols);
       ProtocolIndex++)
  {
    Status = Protocols[ProtocolIndex](&ImageHandle);
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      return Status;
    }
  }

  return EFI_SUCCESS;
}
