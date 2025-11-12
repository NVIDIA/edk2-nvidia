/** @file
  Api's to communicate with OP-TEE OS (Trusted OS based on ARM TrustZone) via
  secure monitor calls to send data to fTPM TA

  SPDX-FileCopyrightText: Copyright (c) 2023 - 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/Tcg2PhysicalPresenceLib.h>
#include <Library/OpteeNvLib.h>
#include "OpteeTpmDeviceLib.h"

STATIC EFI_EVENT  ExitBootServicesEvent = NULL;

/**
  OpteeTpmDeviceLiDestructor

  Destructor for the OPTEE TPM library.

  @param[in] ImageHandle Handle of the driver
  @param[in] Systemtable Pointer to the EFI system table.

  @retval EFI_SUCCESS on Success.
 **/
EFI_STATUS
EFIAPI
OpteeTpmDeviceLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (ExitBootServicesEvent != 0) {
    gBS->CloseEvent (ExitBootServicesEvent);
  }

  return EFI_SUCCESS;
}

/**
  OpteeTpmDeviceLibConstructor

  Constructor for the OPTEE TPM library.

  @param[in] ImageHandle Handle of the driver
  @param[in] Systemtable Pointer to the EFI system table.

  @retval EFI_SUCCESS on Success.
          Other       on Failure.
 **/
EFI_STATUS
EFIAPI
OpteeTpmDeviceLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_NOTIFY,
                  ExitBootServicesCallBack,
                  NULL,
                  &ExitBootServicesEvent
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Create ExitBootServices Callback %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitOpteeTpmDeviceLibConstructor;
  }

ExitOpteeTpmDeviceLibConstructor:
  return Status;
}
