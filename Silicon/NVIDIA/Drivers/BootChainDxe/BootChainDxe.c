/** @file

  Boot Chain Protocol Driver

  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/StatusRegLib.h>
#include "BootChainDxePrivate.h"

STATIC EFI_EVENT  mNewImageEvent         = NULL;
STATIC VOID       *mNewImageRegistration = NULL;

UINT32                         mBootChain                          = MAX_UINT32;
BOOLEAN                        mUpdateBrBctFlag                    = FALSE;
NVIDIA_BR_BCT_UPDATE_PROTOCOL  *mBrBctUpdateProtocol               = NULL;
NVIDIA_BOOT_CHAIN_PROTOCOL     mProtocol                           = { 0 };
EFI_EVENT                      mReadyToBootEvent                   = NULL;
BC_VARIABLE                    mBCVariables[BC_VARIABLE_INDEX_MAX] = {
  [BC_CURRENT] =         { L"BootChainFwCurrent",
                           EFI_VARIABLE_BOOTSERVICE_ACCESS |
                           EFI_VARIABLE_RUNTIME_ACCESS,
                           sizeof (UINT32),
                           &gNVIDIAPublicVariableGuid },
  [BC_NEXT] =            { L"BootChainFwNext",
                           EFI_VARIABLE_BOOTSERVICE_ACCESS |
                           EFI_VARIABLE_RUNTIME_ACCESS |
                           EFI_VARIABLE_NON_VOLATILE,
                           sizeof (UINT32),
                           &gNVIDIAPublicVariableGuid },
  [BC_STATUS] =          { L"BootChainFwStatus",
                           EFI_VARIABLE_BOOTSERVICE_ACCESS |
                           EFI_VARIABLE_RUNTIME_ACCESS |
                           EFI_VARIABLE_NON_VOLATILE,
                           sizeof (UINT32),
                           &gNVIDIAPublicVariableGuid },
  [BC_PREVIOUS] =        { L"BootChainFwPrevious",
                           EFI_VARIABLE_BOOTSERVICE_ACCESS |
                           EFI_VARIABLE_NON_VOLATILE,
                           sizeof (UINT32),
                           &gNVIDIATokenSpaceGuid },
  [BC_RESET_COUNT] =     { L"BootChainFwResetCount",
                           EFI_VARIABLE_BOOTSERVICE_ACCESS |
                           EFI_VARIABLE_NON_VOLATILE,
                           sizeof (UINT32),
                           &gNVIDIATokenSpaceGuid },
  [AUTO_UPDATE_BR_BCT] = { L"AutoUpdateBrBct",
                           EFI_VARIABLE_BOOTSERVICE_ACCESS |
                           EFI_VARIABLE_NON_VOLATILE,
                           sizeof (UINT32),
                           &gNVIDIAPublicVariableGuid },
};

EFI_STATUS
EFIAPI
BCDeleteVariable (
  IN  BC_VARIABLE_INDEX  VariableIndex
  )
{
  BC_VARIABLE  *Variable;
  EFI_STATUS   Status;

  if (VariableIndex >= BC_VARIABLE_INDEX_MAX) {
    return EFI_INVALID_PARAMETER;
  }

  Variable = &mBCVariables[VariableIndex];
  DEBUG ((DEBUG_INFO, "%a: Deleting %s\n", __FUNCTION__, Variable->Name));

  Status = gRT->SetVariable (
                  Variable->Name,
                  Variable->Guid,
                  Variable->Attributes,
                  0,
                  NULL
                  );
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error deleting %s: %r\n",
      __FUNCTION__,
      Variable->Name,
      Status
      ));
  }

  return Status;
}

EFI_STATUS
EFIAPI
BCGetVariable (
  IN  BC_VARIABLE_INDEX  VariableIndex,
  OUT UINT32             *Value
  )
{
  BC_VARIABLE  *Variable;
  UINTN        Size;
  EFI_STATUS   Status;

  if (VariableIndex >= BC_VARIABLE_INDEX_MAX) {
    return EFI_INVALID_PARAMETER;
  }

  Variable = &mBCVariables[VariableIndex];

  *Value = 0;
  Size   = Variable->Bytes;
  Status = gRT->GetVariable (
                  Variable->Name,
                  Variable->Guid,
                  NULL,
                  &Size,
                  Value
                  );
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting %s: %r\n",
      __FUNCTION__,
      Variable->Name,
      Status
      ));
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Read %s=%u: %r\n",
    __FUNCTION__,
    Variable->Name,
    (!EFI_ERROR (Status)) ? *Value : 255,
    Status
    ));

  return Status;
}

EFI_STATUS
EFIAPI
BCSetVariable (
  IN  BC_VARIABLE_INDEX  VariableIndex,
  IN  UINT32             Value
  )
{
  BC_VARIABLE  *Variable;
  EFI_STATUS   Status;

  if (VariableIndex >= BC_VARIABLE_INDEX_MAX) {
    return EFI_INVALID_PARAMETER;
  }

  Variable = &mBCVariables[VariableIndex];
  DEBUG ((
    DEBUG_INFO,
    "%a: Setting %s=%u\n",
    __FUNCTION__,
    Variable->Name,
    Value
    ));

  Status = gRT->SetVariable (
                  Variable->Name,
                  Variable->Guid,
                  Variable->Attributes,
                  Variable->Bytes,
                  &Value
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error setting %s to %u: %r\n",
      __FUNCTION__,
      Variable->Name,
      Value,
      Status
      ));
  }

  return Status;
}

BOOLEAN
EFIAPI
BrBctUpdateNeeded (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT32      Value;

  Status = BCGetVariable (AUTO_UPDATE_BR_BCT, &Value);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return ((Value == 1) && mUpdateBrBctFlag);
}

VOID
EFIAPI
BootChainReset (
  UINT32  BootChain
  )
{
  StatusRegReset ();
  ResetCold ();
}

// NVIDIA_BOOT_CHAIN_PROTOCOL.CheckAndCancelUpdate()
EFI_STATUS
EFIAPI
BootChainCheckAndCancelUpdate (
  IN  NVIDIA_BOOT_CHAIN_PROTOCOL  *This,
  OUT BOOLEAN                     *Canceled
  )
{
  UINT32      BCNext;
  UINT32      BCStatus;
  EFI_STATUS  Status;

  if ((This != &mProtocol) || (Canceled == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Canceled = FALSE;

  Status = BCGetVariable (BC_NEXT, &BCNext);
  if (Status != EFI_NOT_FOUND) {
    *Canceled = TRUE;
    BCDeleteVariable (BC_NEXT);
  }

  Status = BCGetVariable (BC_STATUS, &BCStatus);
  if (Status != EFI_NOT_FOUND) {
    *Canceled = TRUE;
  }

  if (*Canceled) {
    BCSetVariable (BC_STATUS, STATUS_ERROR_CANCELED_FOR_FMP_CONFLICT);
  }

  return EFI_SUCCESS;
}

VOID
EFIAPI
BootChainReadyToBootNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  ValidateActiveBootChain ();
  gBS->CloseEvent (Event);
}

// NVIDIA_BOOT_CHAIN_PROTOCOL.ExecuteUpdate()
EFI_STATUS
EFIAPI
BootChainExecuteUpdate (
  IN  NVIDIA_BOOT_CHAIN_PROTOCOL  *This
  )
{
  UINT32      BCStatus;
  UINT32      BCNext;
  UINT32      BCResetCount;
  EFI_STATUS  Status;

  if (This != &mProtocol) {
    return EFI_INVALID_PARAMETER;
  }

  if (mBrBctUpdateProtocol == NULL) {
    DEBUG ((DEBUG_INFO, "%a: no BrBct protocol\n", __FUNCTION__));
    return EFI_NOT_READY;
  }

  BCStatus = MAX_UINT32;
  DEBUG ((DEBUG_INFO, "%a: Active boot chain=%u\n", __FUNCTION__, mBootChain));

  // if no update requested in Next, just boot OS
  Status = BCGetVariable (BC_NEXT, &BCNext);
  if (Status == EFI_NOT_FOUND) {
    goto BootOs;
  }

  if (EFI_ERROR (Status)) {
    BCStatus = STATUS_ERROR_READING_NEXT;
    goto SetStatusAndBootOs;
  }

  // update is requested in Next, check for existing Status variable
  Status = BCGetVariable (BC_STATUS, &BCStatus);
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    BCStatus = STATUS_ERROR_READING_STATUS;
    goto SetStatusAndBootOs;
  }

  // Status variable not found, start new update
  if (Status == EFI_NOT_FOUND) {
    UINTN       Size;
    EFI_STATUS  BootNextStatus;

    if (BCNext >= BOOT_CHAIN_COUNT) {
      BCStatus = STATUS_ERROR_BAD_BOOT_CHAIN_NEXT;
      goto SetStatusAndBootOs;
    }

    Size           = 0;
    BootNextStatus = gRT->GetVariable (
                            L"BootNext",
                            &gEfiGlobalVariableGuid,
                            NULL,
                            &Size,
                            NULL
                            );
    if (BootNextStatus != EFI_NOT_FOUND) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: BootNext exists, failing boot chain update\n",
        __FUNCTION__
        ));
      BCStatus = STATUS_ERROR_BOOT_NEXT_EXISTS;
      goto SetStatusAndBootOs;
    }
  }

  // Status variable found, check for IN_PROGRESS vs ERROR/SUCCESS
  if (Status == EFI_SUCCESS) {
    if (BCStatus == STATUS_IN_PROGRESS) {
      if (mBootChain == BCNext) {
        BCStatus = STATUS_SUCCESS;
        BCSetVariable (BC_STATUS, BCStatus);
        goto FinishUpdateAndBootOs;
      } else {
        // We tried rebooting to new chain, but it didn't work.
        BCStatus = STATUS_ERROR_BOOT_CHAIN_FAILED;
        goto SetStatusAndBootOs;
      }
    } else {
      // Status is already ERROR or SUCCESS, finish the update and boot OS
      goto FinishUpdateAndBootOs;
    }
  }

  //
  // we're here for new update requested
  //

  // check that requested chain is different than current
  if (BCNext == mBootChain) {
    BCStatus = STATUS_ERROR_NO_OPERATION_REQUIRED;
    goto SetStatusAndBootOs;
  }

  // check that UpdateBrBct flag is not set
  if (mUpdateBrBctFlag) {
    BCStatus = STATUS_ERROR_UPDATE_BR_BCT_FLAG_SET;
    goto SetStatusAndBootOs;
  }

  // check that requested chain is not failed
  if (BootChainIsFailed (BCNext)) {
    BCStatus = STATUS_ERROR_BOOT_CHAIN_IS_FAILED;
    goto SetStatusAndBootOs;
  }

  // save current boot chain before starting update
  Status = BCSetVariable (BC_PREVIOUS, mBootChain);
  if (EFI_ERROR (Status)) {
    BCStatus = STATUS_ERROR_SETTING_PREVIOUS;
    goto SetStatusAndBootOs;
  }

  // set Status to IN_PROGRESS
  BCStatus = STATUS_IN_PROGRESS;
  Status   = BCSetVariable (BC_STATUS, BCStatus);
  if (EFI_ERROR (Status)) {
    BCStatus = STATUS_ERROR_SETTING_IN_PROGRESS;
    goto SetStatusAndBootOs;
  }

  // update ResetCount and check for max
  Status = BCGetVariable (BC_RESET_COUNT, &BCResetCount);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      BCResetCount = 0;
    } else {
      BCStatus = STATUS_ERROR_READING_RESET_COUNT;
      goto SetStatusAndBootOs;
    }
  }

  if (BCResetCount >= BOOT_CHAIN_MAX_RESET_COUNT) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Max resets attempted, failing update\n",
      __FUNCTION__
      ));
    BCStatus = STATUS_ERROR_MAX_RESET_COUNT;
    goto SetStatusAndBootOs;
  }

  Status = BCSetVariable (BC_RESET_COUNT, ++BCResetCount);
  if (EFI_ERROR (Status)) {
    BCStatus = STATUS_ERROR_SETTING_RESET_COUNT;
    goto SetStatusAndBootOs;
  }

  // update BCT to new boot chain
  DEBUG ((
    DEBUG_INFO,
    "%a: updating BCT to new BootChain=%u\n",
    __FUNCTION__,
    BCNext
    ));

  // Mark existing boot chain as good.
  ValidateActiveBootChain ();

  Status = mBrBctUpdateProtocol->UpdateFwChain (mBrBctUpdateProtocol, BCNext);
  if (EFI_ERROR (Status)) {
    BCStatus = STATUS_ERROR_UPDATING_FW_CHAIN;
    goto SetStatusAndBootOs;
  }

  // reset into new FW boot chain
  DEBUG ((
    DEBUG_INFO,
    "%a: Resetting to boot chain=%u, status=%u, reset count=%u\n",
    __FUNCTION__,
    BCNext,
    BCStatus,
    BCResetCount
    ));

  Print (L"Rebooting to new boot chain\n\r");
  BootChainReset (BCNext);

SetStatusAndBootOs:
  DEBUG ((
    DEBUG_INFO,
    "%a: Setting status=%u before booting OS\n",
    __FUNCTION__,
    BCStatus
    ));

  BCSetVariable (BC_STATUS, BCStatus);

FinishUpdateAndBootOs:
  BootChainFinishUpdate (BCStatus);

BootOs:
  if ((BCStatus == MAX_UINT32) && BrBctUpdateNeeded ()) {
    DEBUG ((
      DEBUG_INFO,
      "%a: BrBctUpdateNeeded, new BootChain=%u\n",
      __FUNCTION__,
      mBootChain
      ));

    Status = mBrBctUpdateProtocol->UpdateFwChain (
                                     mBrBctUpdateProtocol,
                                     mBootChain
                                     );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: error updating BrBct to BootChain=%u\n",
        __FUNCTION__,
        mBootChain
        ));
    }

    ClearUpdateBrBctFlag ();
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Booting OS, FW BootChain=%u, Status=%u\n",
    __FUNCTION__,
    mBootChain,
    BCStatus
    ));

  return EFI_SUCCESS;
}

VOID
EFIAPI
BootChainFinishUpdate (
  IN     UINT32  BCStatus
  )
{
  UINT32      BCPrevious;
  BOOLEAN     RebootOriginalBootChain;
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a: BCStatus=%u\n", __FUNCTION__, BCStatus));

  // On error, see if we need to reboot the original boot chain
  RebootOriginalBootChain = FALSE;
  if (BCStatus != STATUS_SUCCESS) {
    Status = BCGetVariable (BC_PREVIOUS, &BCPrevious);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get Previous: %r\n",
        __FUNCTION__,
        Status
        ));
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unable to determine original boot chain\n",
        __FUNCTION__
        ));
      goto Cleanup;
    }

    if (mBootChain != BCPrevious) {
      RebootOriginalBootChain = TRUE;
    }
  }

Cleanup:
  BCDeleteVariable (BC_PREVIOUS);
  BCDeleteVariable (BC_NEXT);
  BCDeleteVariable (BC_RESET_COUNT);

  if (RebootOriginalBootChain) {
    Status = mBrBctUpdateProtocol->UpdateFwChain (mBrBctUpdateProtocol, BCPrevious);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to update BR-BCT boot chain: %r\n",
        __FUNCTION__,
        Status
        ));
      return;
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: Doing reset to restore original boot chain=%u\n",
      __FUNCTION__,
      BCPrevious
      ));

    Print (L"Rebooting to restore boot chain\n\r");
    BootChainReset (BCPrevious);
  }
}

/**
  Event notification for installation of BrBctUpdate protocol instance.

  @param  Event                 The Event that is being processed.
  @param  Context               Event Context.

**/
STATIC
VOID
EFIAPI
BrBctProtocolCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (
                  &gNVIDIABrBctUpdateProtocolGuid,
                  NULL,
                  (VOID **)&mBrBctUpdateProtocol
                  );
  DEBUG ((DEBUG_INFO, "%a: BrBctUpdate protocol: %r\n", __FUNCTION__, Status));

  if (!EFI_ERROR (Status)) {
    gBS->CloseEvent (Event);
  }
}

EFI_STATUS
EFIAPI
BootChainDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HANDLE  Handle;
  EFI_STATUS  Status;
  EFI_STATUS  ExitStatus;
  VOID        *Hob;

  ExitStatus = EFI_SUCCESS;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    TEGRA_PLATFORM_RESOURCE_INFO  *ResourceInfo;

    ResourceInfo     = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
    mBootChain       = ResourceInfo->ActiveBootChain;
    mUpdateBrBctFlag = ResourceInfo->BrBctUpdateFlag;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting resource info\n",
      __FUNCTION__
      ));
    return EFI_UNSUPPORTED;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  BootChainReadyToBootNotify,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &mReadyToBootEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error creating Ready to Boot event: %r\n",
      __FUNCTION__,
      Status
      ));
    ExitStatus = Status;
    goto Done;
  }

  mNewImageEvent = EfiCreateProtocolNotifyEvent (
                     &gNVIDIABrBctUpdateProtocolGuid,
                     TPL_CALLBACK,
                     BrBctProtocolCallback,
                     NULL,
                     &mNewImageRegistration
                     );
  if (mNewImageEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: protocol notify failed\n", __FUNCTION__));
  }

  mProtocol.ActiveBootChain      = mBootChain;
  mProtocol.GetPartitionName     = GetBootChainPartitionName;
  mProtocol.ExecuteUpdate        = BootChainExecuteUpdate;
  mProtocol.CheckAndCancelUpdate = BootChainCheckAndCancelUpdate;
  Handle                         = NULL;
  Status                         = gBS->InstallMultipleProtocolInterfaces (
                                          &Handle,
                                          &gNVIDIABootChainProtocolGuid,
                                          &mProtocol,
                                          NULL
                                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error installing protocol: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

Done:
  BCSetVariable (BC_CURRENT, mBootChain);

  // ReadyToBoot event handler always needs to run even if there are other errors
  return ExitStatus;
}
