/** @file

  Arm SBMR Status code Driver

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Base.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>

#include <IndustryStandard/Ipmi.h>

#include <Protocol/ReportStatusCodeHandler.h>

#define ARM_IPMI_GROUP_EXTENSION              0xAE
#define ARM_SBMR_SEND_PROGRESS_CODE_CMD       0x2
#define ARM_SBMR_SEND_PROGRESS_CODE_REQ_SIZE  10
#define ARM_SBMR_SEND_PROGRESS_CODE_RSP_SIZE  2

STATIC BOOLEAN  mDisableSmbrStatus = FALSE;

STATIC
EFI_STATUS
ArmSmbrStatusCodeCallback (
  IN EFI_STATUS_CODE_TYPE   CodeType,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN UINT32                 Instance,
  IN EFI_GUID               *CallerId,
  IN EFI_STATUS_CODE_DATA   *Data
  )
{
  EFI_STATUS  Status;
  UINT8       Request[ARM_SBMR_SEND_PROGRESS_CODE_REQ_SIZE];
  UINT8       Response[ARM_SBMR_SEND_PROGRESS_CODE_RSP_SIZE];
  UINT32      ResponseDataSize;

  if (mDisableSmbrStatus) {
    return EFI_UNSUPPORTED;
  }

  if (((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_PROGRESS_CODE) &&
      (Value == (EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_BS_PC_EXIT_BOOT_SERVICES)))
  {
    mDisableSmbrStatus = TRUE;
  }

  if (Instance > MAX_UINT8) {
    return EFI_INVALID_PARAMETER;
  }

  Request[0] = ARM_IPMI_GROUP_EXTENSION;
  CopyMem (&Request[1], &CodeType, sizeof (CodeType));
  CopyMem (&Request[5], &Value, sizeof (Value));
  Request[9] = (UINT8)Instance;

  ResponseDataSize = ARM_SBMR_SEND_PROGRESS_CODE_RSP_SIZE;
  Status           = IpmiSubmitCommand (
                       IPMI_NETFN_GROUP_EXT,
                       ARM_SBMR_SEND_PROGRESS_CODE_CMD,
                       Request,
                       sizeof (Request),
                       Response,
                       &ResponseDataSize
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send IPMI command - %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseDataSize != ARM_SBMR_SEND_PROGRESS_CODE_RSP_SIZE) {
    DEBUG ((DEBUG_ERROR, "%a: Failed unexpected response size, Got: %d, Expected: %d\r\n", __FUNCTION__, ResponseDataSize, ARM_SBMR_SEND_PROGRESS_CODE_RSP_SIZE));
    return EFI_DEVICE_ERROR;
  }

  if (Response[0] == IPMI_COMP_CODE_INVALID_COMMAND) {
    DEBUG ((DEBUG_ERROR, "%a: BMC does not support status codes, disabling\r\n", __FUNCTION__));
    mDisableSmbrStatus = TRUE;
  } else if (Response[0] != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed unexpected command completion code, Got: %x, Expected: %x\r\n", __FUNCTION__, Response[0], IPMI_COMP_CODE_NORMAL));
    return EFI_DEVICE_ERROR;
  } else if (Response[1] != ARM_IPMI_GROUP_EXTENSION) {
    DEBUG ((DEBUG_ERROR, "%a: Failed unexpected group id, Got: %x, Expected: %x\r\n", __FUNCTION__, Response[1], ARM_IPMI_GROUP_EXTENSION));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
ArmSmbrStatusCodeDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_RSC_HANDLER_PROTOCOL  *RscHandler;

  RscHandler = NULL;

  InitializeIpmiBase ();

  Status = gBS->LocateProtocol (&gEfiRscHandlerProtocolGuid, NULL, (VOID **)&RscHandler);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = RscHandler->Register (ArmSmbrStatusCodeCallback, TPL_CALLBACK);
  return Status;
}
