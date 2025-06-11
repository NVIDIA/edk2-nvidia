/** @file
Reset Library for Standalone MM in Hafnium Deployments.

SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <PiMm.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/MmServicesTableLib.h>
#include <Library/DebugLib.h>

#include <Library/HobLib.h>
#include <Library/ArmSvcLib.h>
#include <Library/ResetSystemLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/MmCommunication2.h>

STATIC
EFI_STATUS
StMmHafniumL2Reset (
  VOID
  )
{
  ARM_SVC_ARGS               ArmSvcArgs;
  EFI_MM_COMMUNICATE_HEADER  *Header;
  UINT64                     MboxAddr;
  UINT32                     MboxSize;
  EFI_STATUS                 Status;

  if (IsOpteePresent ()) {
    Status = EFI_UNSUPPORTED;
    goto ExitMmCommSendResetReq;
  }

  Status = GetMboxAddrSize (RASFW_VMID, &MboxAddr, &MboxSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get RAS's Mailbox info %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitMmCommSendResetReq;
  }

  Header = (EFI_MM_COMMUNICATE_HEADER *)MboxAddr;

  /* Prepare FFA request */
  ZeroMem (&ArmSvcArgs, sizeof (ARM_SVC_ARGS));

  ArmSvcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ;
  ArmSvcArgs.Arg1 = ((UINTN)(STMM_VMID) << 16) | RASFW_VMID;
  ArmSvcArgs.Arg2 = 0;
  ArmSvcArgs.Arg3 = RAS_FW_MM_RESET_REQ;
  ArmSvcArgs.Arg4 = 0;
  ArmSvcArgs.Arg5 = (UINT64)Header;  /* For verification purposes */
  ArmSvcArgs.Arg6 = 0;
  ArmSvcArgs.Arg7 = 0;

  /* Prepare MM_COMMUNICATE */
  CopyGuid (&(Header->HeaderGuid), &gNVIDIAMmRasResetReqGuid);

  ArmCallSvc (&ArmSvcArgs);
  if (ArmSvcArgs.Arg3 != ARM_SVC_ID_FFA_SUCCESS_AARCH64) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Send FF-A Direct MSg failed: 0x%x\n for L2 reset to RASFW\n",
      __FUNCTION__,
      ArmSvcArgs.Arg3
      ));
    Status = EFI_NOT_READY;
    goto ExitMmCommSendResetReq;
  }

ExitMmCommSendResetReq:
  return Status;
}

/**
  This function causes a system-wide reset (cold reset), in which
  all circuitry within the system returns to its initial state. This type of reset
  is asynchronous to system operation and operates without regard to
  cycle boundaries.

  If this function returns, it means that the system does not support cold reset.
**/
VOID
EFIAPI
ResetCold (
  VOID
  )
{
  StMmHafniumL2Reset ();
}

/**
  This function causes a system-wide initialization (warm reset), in which all processors
  are set to their initial state. Pending cycles are not corrupted.

  If this function returns, it means that the system does not support warm reset.
**/
VOID
EFIAPI
ResetWarm (
  VOID
  )
{
  DEBUG ((
    DEBUG_INFO,
    "Warm reboot not supported by platform, issuing cold reboot\n"
    ));
  ResetCold ();
}

/**
  This function causes the system to enter a power state equivalent
  to the ACPI G2/S5 or G3 states.

  If this function returns, it means that the system does not support shutdown reset.
**/
VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  DEBUG ((DEBUG_ERROR, "ResetShutdown isn't supported\n"));
}

/**
  This function causes a systemwide reset. The exact type of the reset is
  defined by the EFI_GUID that follows the Null-terminated Unicode string passed
  into ResetData. If the platform does not recognize the EFI_GUID in ResetData
  the platform must pick a supported reset type to perform.The platform may
  optionally log the parameters from any non-normal reset that occurs.

  @param[in]  DataSize   The size, in bytes, of ResetData.
  @param[in]  ResetData  The data buffer starts with a Null-terminated string,
                         followed by the EFI_GUID.
**/
VOID
EFIAPI
ResetPlatformSpecific (
  IN UINTN  DataSize,
  IN VOID   *ResetData
  )
{
  // Map the platform specific reset as reboot
  ResetCold ();
}

/**
  The ResetSystem function resets the entire platform.

  @param[in] ResetType      The type of reset to perform.
  @param[in] ResetStatus    The status code for the reset.
  @param[in] DataSize       The size, in bytes, of ResetData.
  @param[in] ResetData      For a ResetType of EfiResetCold, EfiResetWarm, or EfiResetShutdown
                            the data buffer starts with a Null-terminated string, optionally
                            followed by additional binary data. The string is a description
                            that the caller may use to further indicate the reason for the
                            system reset.
**/
VOID
EFIAPI
ResetSystem (
  IN EFI_RESET_TYPE  ResetType,
  IN EFI_STATUS      ResetStatus,
  IN UINTN           DataSize,
  IN VOID            *ResetData OPTIONAL
  )
{
  switch (ResetType) {
    case EfiResetWarm:
      ResetWarm ();
      break;

    case EfiResetCold:
      ResetCold ();
      break;

    case EfiResetShutdown:
      ResetShutdown ();
      return;

    case EfiResetPlatformSpecific:
      ResetPlatformSpecific (DataSize, ResetData);
      return;

    default:
      return;
  }
}
