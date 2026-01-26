/** @file
  Implementation of CompareAndSyncBootOptions function.
  Separated from main driver to enable unit testing.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishHttpBootConfigUtils.h"

/**
  Clear BootNext if it points to the Redfish HTTP boot option.

  @param[in]  RedfishBootNum  The Redfish boot option number to check against

  @retval EFI_SUCCESS  BootNext cleared or doesn't point to our boot option
  @retval Others       Error from GetVariable or SetVariable
**/
STATIC
EFI_STATUS
ClearBootNextIfNeeded (
  IN UINT16  RedfishBootNum
  )
{
  EFI_STATUS  Status;
  UINT16      CurrentBootNext;
  UINTN       BootNextSize;

  BootNextSize = sizeof (CurrentBootNext);
  Status       = gRT->GetVariable (
                        L"BootNext",
                        &gEfiGlobalVariableGuid,
                        NULL,
                        &BootNextSize,
                        &CurrentBootNext
                        );

  if (!EFI_ERROR (Status) && (CurrentBootNext == RedfishBootNum)) {
    DEBUG ((DEBUG_INFO, "%a: Clearing BootNext (was pointing to deleted boot option 0x%04x)\n", __func__, RedfishBootNum));

    Status = gRT->SetVariable (
                    L"BootNext",
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    0,
                    NULL
                    );

    if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to clear BootNext: %r\n", __func__, Status));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Compare HttpBootUri variable with Redfish HTTP Boot Option and sync as needed.

  Multi-boot cleanup logic:
  - Boot N (creation): HII RouteConfig creates boot option and sets BootNext
  - Boot N+1 (wait): BdsDxe reads BootNext and boots from Redfish boot option
  - Boot N+2 (cleanup): This function deletes one-time boot option and variable

  Handles three cases:
  1. Variable doesn't exist → delete boot option if present
  2. Variable exists but boot option doesn't → create boot option
  3. Variable exists with "once" flag → delete after HTTP boot completes

  @retval EFI_SUCCESS      Sync completed successfully
  @retval EFI_INVALID_PARAMETER Invalid URI format
  @retval Others           Error from variable services
**/
EFI_STATUS
CompareAndSyncBootOptions (
  VOID
  )
{
  EFI_STATUS       Status;
  CHAR16           UriString[HTTP_BOOT_URI_MAX_SIZE];
  UINTN            VarSize;
  EFI_MAC_ADDRESS  MacAddr;
  BOOLEAN          Once;
  CHAR16           *Uri;
  CHAR16           RedfishBootName[16];
  UINT16           RedfishBootNum = REDFISH_HTTP_BOOT_OPTION_NUM;

  UnicodeSPrint (RedfishBootName, sizeof (RedfishBootName), L"Boot%04x", RedfishBootNum);

  //
  // Read HttpBootUri variable
  //
  VarSize = sizeof (UriString);
  Status  = gRT->GetVariable (
                   L"HttpBootUri",
                   &gNvidiaHttpBootConfigGuid,
                   NULL,
                   &VarSize,
                   UriString
                   );

  if (Status == EFI_NOT_FOUND) {
    //
    // Variable doesn't exist - clean up Redfish boot option if it exists
    //
    DEBUG ((DEBUG_INFO, "%a: HttpBootUri not found, cleaning up %s if it exists\n", __func__, RedfishBootName));

    Status = gRT->SetVariable (
                    RedfishBootName,
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    0,
                    NULL
                    );

    if (!EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: Deleted %s\n", __func__, RedfishBootName));
    } else if (Status != EFI_NOT_FOUND) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to delete %s: %r\n", __func__, RedfishBootName, Status));
    }

    // Clear BootNext if it points to our boot option
    ClearBootNextIfNeeded (RedfishBootNum);

    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Failed to read HttpBootUri: %r\n", __func__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: HttpBootUri = %s\n", __func__, UriString));

  //
  // Parse URI string to check for "once" flag
  //
  Status = ParseHttpBootUri (UriString, &MacAddr, &Once, &Uri);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to parse HttpBootUri: %r\n", __func__, Status));
    return Status;
  }

  //
  // Check if boot option actually exists
  // If not, create it (handles manual deletion, firmware reset, etc.)
  //
  UINTN  BootOptionSize;

  BootOptionSize = 0;
  Status         = gRT->GetVariable (
                          RedfishBootName,
                          &gEfiGlobalVariableGuid,
                          NULL,
                          &BootOptionSize,
                          NULL
                          );

  BOOLEAN  BootOptionExists = (Status == EFI_BUFFER_TOO_SMALL);

  if (!BootOptionExists) {
    //
    // Boot option doesn't exist, create it
    // This handles cases where:
    // - Boot option was manually deleted
    // - Firmware was reset
    // - Previous creation failed
    //
    UINT16  BootOptionNum;

    DEBUG ((DEBUG_INFO, "%a: Boot option %s doesn't exist, creating it\n", __func__, RedfishBootName));

    Status = CreateHttpBootOption (&MacAddr, Uri, &BootOptionNum);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to create boot option: %r\n", __func__, Status));
      return Status;
    }

    DEBUG ((DEBUG_INFO, "%a: Created boot option and set BootNext\n", __func__));

    //
    // If this is a one-time boot, we'll clean up on next boot after HTTP boot executes
    // If persistent, the boot option will remain
    //
    if (Once) {
      DEBUG ((DEBUG_INFO, "%a: One-time boot, will cleanup after HTTP boot\n", __func__));
    } else {
      DEBUG ((DEBUG_INFO, "%a: Persistent boot, boot option will remain\n", __func__));
    }

    return EFI_SUCCESS;
  }

  //
  // Boot option exists
  // If "once" flag is set, check if we should clean up
  // We only clean up AFTER the HTTP boot has happened (boot N+2)
  // To detect this: check if BootNext is still set to RedfishBootNum
  // - If BootNext = RedfishBootNum: we're on boot N+1, before HTTP boot → don't delete yet
  // - If BootNext != RedfishBootNum: we're on boot N+2+, after HTTP boot → delete now
  //
  if (Once) {
    UINT16  CurrentBootNext;
    UINTN   BootNextSize;

    DEBUG ((DEBUG_INFO, "%a: One-time boot flag detected, checking if we should clean up\n", __func__));

    // Read BootNext to see if BdsDxe has used it yet
    BootNextSize = sizeof (CurrentBootNext);
    Status       = gRT->GetVariable (
                          L"BootNext",
                          &gEfiGlobalVariableGuid,
                          NULL,
                          &BootNextSize,
                          &CurrentBootNext
                          );

    if (!EFI_ERROR (Status) && (CurrentBootNext == RedfishBootNum)) {
      //
      // BootNext is still set to RedfishBootNum, which means BdsDxe hasn't used it yet
      // We're on boot N+1, BEFORE the HTTP boot happens
      // Don't delete anything yet - BdsDxe needs the boot option to exist
      //
      DEBUG ((DEBUG_INFO, "%a: BootNext still set to 0x%04x, not cleaning up yet (boot N+1)\n", __func__, CurrentBootNext));
    } else {
      //
      // BootNext is not set to RedfishBootNum (either cleared or set to something else)
      // This means BdsDxe already used it and we're on boot N+2 or later
      // Clean up the variable and boot option now
      //
      DEBUG ((DEBUG_INFO, "%a: BootNext not set to 0x%04x, cleaning up now (boot N+2)\n", __func__, RedfishBootNum));

      // Delete the variable
      Status = gRT->SetVariable (
                      L"HttpBootUri",
                      &gNvidiaHttpBootConfigGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                      0,
                      NULL
                      );

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to delete HttpBootUri: %r\n", __func__, Status));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Deleted one-time HttpBootUri variable\n", __func__));
      }

      // Delete Redfish boot option
      Status = gRT->SetVariable (
                      RedfishBootName,
                      &gEfiGlobalVariableGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                      0,
                      NULL
                      );

      if (!EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "%a: Deleted %s\n", __func__, RedfishBootName));
      } else if (Status != EFI_NOT_FOUND) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to delete %s: %r\n", __func__, RedfishBootName, Status));
      }

      // Clear BootNext if it points to our boot option (shouldn't normally happen
      // since we only clean up after BootNext was already used, but be safe)
      ClearBootNextIfNeeded (RedfishBootNum);
    }
  } else {
    //
    // Persistent case: variable exists without "once" flag
    // Recreate boot option and set BootNext to ensure HTTP boot happens on every boot
    // CreateHttpBootOption() is idempotent - it overwrites existing boot option
    //
    UINT16  BootOptionNum;

    DEBUG ((DEBUG_INFO, "%a: Persistent boot, recreating boot option and setting BootNext\n", __func__));

    Status = CreateHttpBootOption (&MacAddr, Uri, &BootOptionNum);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to create/update boot option: %r\n", __func__, Status));
      return Status;
    }

    DEBUG ((DEBUG_INFO, "%a: Persistent boot option updated and BootNext set to Boot%04x\n", __func__, BootOptionNum));
  }

  return EFI_SUCCESS;
}
