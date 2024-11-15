/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/SortLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/FwVariableLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PlatformBootOrderIpmiLib.h>
#include <Library/StatusRegLib.h>
#include <IndustryStandard/Ipmi.h>
#include <Guid/GlobalVariable.h>
#include "InternalPlatformBootOrderIpmiLib.h"

STATIC  IPMI_GET_BOOT_OPTIONS_RESPONSE  *mBootOptionsResponse = NULL;
STATIC  IPMI_SET_BOOT_OPTIONS_REQUEST   *mBootOptionsRequest  = NULL;

STATIC
EFI_STATUS
GetIPMIBootOrderParameter (
  IN UINT8                            ParameterSelector,
  OUT IPMI_GET_BOOT_OPTIONS_RESPONSE  *BootOptionsResponse
  )
{
  EFI_STATUS                     Status;
  IPMI_GET_BOOT_OPTIONS_REQUEST  BootOptionsRequest;
  UINT32                         ResponseSize;

  ZeroMem (&BootOptionsRequest, sizeof (IPMI_GET_BOOT_OPTIONS_REQUEST));
  BootOptionsRequest.ParameterSelector.Bits.ParameterSelector = ParameterSelector;
  BootOptionsRequest.SetSelector                              = 0;
  BootOptionsRequest.BlockSelector                            = 0;

  ResponseSize = sizeof (IPMI_GET_BOOT_OPTIONS_RESPONSE);
  if (ParameterSelector == IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK) {
    ResponseSize += sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4);
  } else if (ParameterSelector == IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS) {
    ResponseSize += sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5);
  }

  Status = IpmiSubmitCommand (
             IPMI_NETFN_CHASSIS,
             IPMI_CHASSIS_GET_SYSTEM_BOOT_OPTIONS,
             (VOID *)&BootOptionsRequest,
             sizeof (BootOptionsRequest),
             (VOID *)BootOptionsResponse,
             &ResponseSize
             );

  if (EFI_ERROR (Status) ||
      (BootOptionsResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) ||
      (BootOptionsResponse->ParameterValid.Bits.ParameterValid == IPMI_GET_BOOT_OPTIONS_PARAMETER_INVALID) ||
      (BootOptionsResponse->ParameterValid.Bits.ParameterSelector != ParameterSelector) ||
      (BootOptionsResponse->ParameterVersion.Bits.ParameterVersion != IPMI_PARAMETER_VERSION))
  {
    DEBUG ((DEBUG_ERROR, "Failed to get BMC Boot Options Parameter %u (IPMI CompCode = 0x%x)\r\n", ParameterSelector, BootOptionsResponse->CompletionCode));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetIPMIBootOrderParameter (
  IN UINT8                          ParameterSelector,
  IN IPMI_SET_BOOT_OPTIONS_REQUEST  *BootOptionsRequest
  )
{
  EFI_STATUS                      Status;
  IPMI_SET_BOOT_OPTIONS_RESPONSE  BootOptionsResponse;
  UINT32                          RequestSize;
  UINT32                          ResponseSize;

  BootOptionsRequest->ParameterValid.Bits.MarkParameterInvalid = 0;
  BootOptionsRequest->ParameterValid.Bits.ParameterSelector    = ParameterSelector;

  RequestSize = sizeof (IPMI_SET_BOOT_OPTIONS_REQUEST);
  if (ParameterSelector == IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK) {
    RequestSize += sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4);
  } else if (ParameterSelector == IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS) {
    RequestSize += sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5);
  }

  ResponseSize = sizeof (IPMI_SET_BOOT_OPTIONS_RESPONSE);

  Status = IpmiSubmitCommand (
             IPMI_NETFN_CHASSIS,
             IPMI_CHASSIS_SET_SYSTEM_BOOT_OPTIONS,
             (VOID *)BootOptionsRequest,
             RequestSize,
             (VOID *)&BootOptionsResponse,
             &ResponseSize
             );

  if (EFI_ERROR (Status) ||
      (BootOptionsResponse.CompletionCode != IPMI_COMP_CODE_NORMAL))
  {
    DEBUG ((DEBUG_ERROR, "Failed to set BMC Boot Options Parameter %u (IPMI CompCode = 0x%x)\r\n", ParameterSelector, BootOptionsResponse.CompletionCode));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
INTN
EFIAPI
Uint16SortCompare (
  IN CONST VOID  *Buffer1,
  IN CONST VOID  *Buffer2
  )
{
  return *(UINT16 *)Buffer1 - *(UINT16 *)Buffer2;
}

VOID
EFIAPI
CheckIPMIForBootOrderUpdates (
  VOID
  )
{
  EFI_STATUS                    Status;
  IPMI_BOOT_OPTIONS_PARAMETERS  *BootOptionsResponseParameters;
  IPMI_BOOT_OPTIONS_PARAMETERS  *BootOptionsRequestParameters;

  mBootOptionsResponse = AllocateZeroPool (sizeof (IPMI_GET_BOOT_OPTIONS_RESPONSE) + sizeof (IPMI_BOOT_OPTIONS_PARAMETERS));
  if (mBootOptionsResponse == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to allocate memory for handling IPMI BootOrder Responses\n"));
    goto CleanupAndReturn;
  }

  mBootOptionsRequest = AllocateZeroPool (sizeof (IPMI_GET_BOOT_OPTIONS_REQUEST) + sizeof (IPMI_BOOT_OPTIONS_PARAMETERS));
  if (mBootOptionsRequest == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to allocate memory for handling IPMI BootOrder Requests\n"));
    goto CleanupAndReturn;
  }

  Status = GetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK, mBootOptionsResponse);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_UNSUPPORTED) {
      DEBUG ((DEBUG_ERROR, "Error checking for IPMI BOOT_INFO_ACK: %r\n", Status));
    }

    goto CleanupAndReturn;
  }

  BootOptionsResponseParameters = (IPMI_BOOT_OPTIONS_PARAMETERS *)&mBootOptionsResponse->ParameterData[0];
  if (BootOptionsResponseParameters->Parm4.BootInitiatorAcknowledgeData & BOOT_OPTION_HANDLED_BY_BIOS) {
    Status = GetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS, mBootOptionsResponse);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error checking if IPMI boot options were already processed: %r\n", Status));
      goto CleanupAndReturn;
    }

    if (!BootOptionsResponseParameters->Parm5.Data1.Bits.BootFlagValid) {
      goto Done;
    }

    if (BootOptionsResponseParameters->Parm5.Data2.Bits.CmosClear) {
      DEBUG ((DEBUG_ERROR, "IPMI requested a CMOS clear\n"));

      Status = FwVariableDeleteAll ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Error clearing CMOS: %r\n", Status));
        goto CleanupAndReturn;
      }

      // Clear CmosClear bit but leave the rest to be processed after reset
      BootOptionsRequestParameters = (IPMI_BOOT_OPTIONS_PARAMETERS *)&mBootOptionsRequest->ParameterData[0];
      CopyMem (BootOptionsRequestParameters, BootOptionsResponseParameters, sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5));
      BootOptionsRequestParameters->Parm5.Data2.Bits.CmosClear = 0;

      Status = SetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS, mBootOptionsRequest);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "Error clearing IPMI CmosClear bit: %r\n", Status));
      }

      // Mark existing boot chain as good.
      ValidateActiveBootChain ();

      // Reset
      StatusRegReset ();
      gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
      ASSERT (FALSE);
    }

    // Don't free up allocations; they will be used and freed by ProcessIPMIBootOrderUpdates
    goto Done;
  }

CleanupAndReturn:
  FREE_NON_NULL (mBootOptionsRequest);
  FREE_NON_NULL (mBootOptionsResponse);

Done:
  return;
}

// When IPMI requests a temporary BootOrder change, we save the BootOrder
// and then move one element (or class) to the beginning. This function will
// restore the original BootOrder unless additional modifications have been made.
VOID
EFIAPI
RestoreBootOrder (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
  UINT16                      *SavedBootOrder;
  UINT16                      *SavedBootOrderCopy;
  UINTN                       SavedBootOrderSize;
  UINTN                       SavedBootOrderLength;
  UINT16                      *BootOrder;
  UINTN                       BootOrderSize;
  UINTN                       BootOrderLength;
  UINT16                      *BootCurrent;
  UINTN                       BootCurrentSize;
  UINTN                       ReorderedIndex;
  UINT16                      ReorderedBootNum;
  EFI_STATUS                  Status;
  NVIDIA_BOOT_ORDER_PRIORITY  *BootClass;
  NVIDIA_BOOT_ORDER_PRIORITY  *SavedBootClass;
  NVIDIA_BOOT_ORDER_PRIORITY  *VirtualBootClass;
  INTN                        SavedBootOrderIndex;
  UINTN                       VirtualCount;
  UINTN                       MovedItemCount;
  UINT8                       *SavedBootOrderFlags;
  UINTN                       SavedBootOrderFlagsSize;
  BOOLEAN                     VirtualFlag;
  BOOLEAN                     AllInstancesFlag;

  SavedBootOrder      = NULL;
  BootOrder           = NULL;
  SavedBootOrderCopy  = NULL;
  SavedBootOrderFlags = NULL;

  // Gather SavedBootOrder
  Status = GetVariable2 (SAVED_BOOT_ORDER_VARIABLE_NAME, &gNVIDIATokenSpaceGuid, (VOID **)&SavedBootOrder, &SavedBootOrderSize);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "%a: No SavedBootOrder found to be restored\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to determine SavedBootOrder: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  if ((SavedBootOrder == NULL) || (SavedBootOrderSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: SavedBootOrder is empty. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  SavedBootOrderLength = SavedBootOrderSize/sizeof (SavedBootOrder[0]);

  // Gather BootOrder
  Status = GetEfiGlobalVariable2 (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &BootOrderSize);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: No BootOrder found. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to determine BootOrder: %r\n", __FUNCTION__, Status));
    goto DeleteSaveAndCleanup;
  }

  if ((BootOrder == NULL) || (BootOrderSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: BootOrder is empty. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  BootOrderLength = BootOrderSize/sizeof (BootOrder[0]);

  // Make sure BootOrder only has one device moved to the front vs. SavedBootOrder,
  // or has all instances moved to the start
  if (SavedBootOrderLength != BootOrderLength) {
    DEBUG ((DEBUG_WARN, "%a: BootOrder (len=%u) and SavedBootOrder (len=%u) differ in size. Not restoring boot order\n", __FUNCTION__, BootOrderLength, SavedBootOrderLength));
    goto DeleteSaveAndCleanup;
  }

  ReorderedBootNum = BootOrder[0];
  for (ReorderedIndex = 0; ReorderedIndex < BootOrderLength; ReorderedIndex++) {
    if (SavedBootOrder[ReorderedIndex] == ReorderedBootNum) {
      break;
    }
  }

  if (ReorderedIndex >= BootOrderLength) {
    DEBUG ((DEBUG_WARN, "%a: First BootOrder device is not in SavedBootOrder. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  // Parse the flags, if present
  VirtualFlag      = FALSE;
  AllInstancesFlag = FALSE;
  Status           = GetVariable2 (SAVED_BOOT_ORDER_FLAGS_VARIABLE_NAME, &gNVIDIATokenSpaceGuid, (VOID **)&SavedBootOrderFlags, &SavedBootOrderFlagsSize);
  if (!EFI_ERROR (Status) && (SavedBootOrderFlags != NULL) && (SavedBootOrderFlagsSize == sizeof (UINT8))) {
    if (*SavedBootOrderFlags & SAVED_BOOT_ORDER_VIRTUAL_FLAG) {
      VirtualFlag = TRUE;
    }

    if (*SavedBootOrderFlags & SAVED_BOOT_ORDER_ALL_INSTANCES_FLAG) {
      AllInstancesFlag = TRUE;
    }
  }

  SavedBootOrderCopy = AllocateCopyPool (SavedBootOrderSize, SavedBootOrder);
  if (!AllInstancesFlag) {
    // See if we can recreate BootOrder by simply moving one item from SavedBootOrder to the front
    MOVE_INDEX_TO_START (SavedBootOrderCopy, ReorderedIndex);
  } else {
    // See if we can recreate BootOrder by moving all items of the class from SavedBootOrder to the front
    Status = GetBootClassOfOptionNum (BootOrder[0], &BootClass, mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
    if (Status == EFI_NOT_FOUND) {
      BootClass = NULL; // eg. the "ubuntu" option
    } else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Error (%r) checking if we can restore boot order. Not restoring boot order\n", __FUNCTION__, Status));
      goto DeleteSaveAndCleanup;
    }

    // If VirtualFlag is set, then we moved "virtual" to start and "usb" to after virtual
    VirtualCount = 0;
    if (VirtualFlag && BootClass && (AsciiStrCmp (BootClass->OrderName, "virtual") == 0)) {
      VirtualBootClass = BootClass;
      BootClass        = GetBootClassOfName ("usb", AsciiStrLen ("usb"), mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
    } else {
      VirtualBootClass = NULL;
    }

    MovedItemCount = 0;
    for (SavedBootOrderIndex = SavedBootOrderLength-1; SavedBootOrderIndex >= MovedItemCount; ) {
      Status = GetBootClassOfOptionNum (SavedBootOrderCopy[SavedBootOrderIndex], &SavedBootClass, mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
      if (Status == EFI_NOT_FOUND) {
        SavedBootClass = NULL; // eg. the "ubuntu" option
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "%a: Error (%r) checking if we can restore boot order. Not restoring boot order\n", __FUNCTION__, Status));
        goto DeleteSaveAndCleanup;
      }

      if (SavedBootClass == BootClass) {
        MOVE_INDEX_TO_START (&SavedBootOrderCopy[VirtualCount], SavedBootOrderIndex-VirtualCount);
        MovedItemCount++;
      } else if (SavedBootClass && (SavedBootClass == VirtualBootClass)) {
        MOVE_INDEX_TO_START (SavedBootOrderCopy, SavedBootOrderIndex);
        MovedItemCount++;
        VirtualCount++;
      } else {
        SavedBootOrderIndex--;
      }

      PrintBootOrder (DEBUG_VERBOSE, L"SavedBootOrderCopy during loop:", SavedBootOrderCopy, SavedBootOrderSize);
    }
  }

  if (CompareMem (BootOrder, SavedBootOrderCopy, BootOrderSize) != 0) {
    DEBUG ((DEBUG_WARN, "%a: BootOrder and SavedBootOrder have more changes than expected. Not restoring boot order\n", __FUNCTION__));
    PrintBootOrder (DEBUG_WARN, L"CurrentBootOrder:", BootOrder, BootOrderSize);
    PrintBootOrder (DEBUG_WARN, L"SavedBootOrder:", SavedBootOrder, SavedBootOrderSize);
    PrintBootOrder (DEBUG_INFO, L"SavedBootOrderCopy:", SavedBootOrderCopy, SavedBootOrderSize);
    goto DeleteSaveAndCleanup;
  }

  // At this point, we've confirmed that BootOrder equals SavedBootOrder except with one device or class moved to the beginning

  // If we're triggering this from an event, make sure BootCurrent is the reordered BootNum. If not, we don't need to restore.
  if (Event != NULL) {
    Status = GetEfiGlobalVariable2 (L"BootCurrent", (VOID **)&BootCurrent, &BootCurrentSize);
    if (EFI_ERROR (Status) || (BootCurrent == NULL) || (BootCurrentSize != sizeof (UINT16))) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to determine BootCurrent: %r\n", __FUNCTION__, Status));
      goto DeleteSaveAndCleanup;
    }

    if (BootCurrent[0] != ReorderedBootNum) {
      DEBUG ((DEBUG_WARN, "%a: Attempted to restore BootOrder when BootCurrent wasn't the temporary BootNum. Not restoring boot order\n", __FUNCTION__));
      goto DeleteSaveAndCleanup;
    }
  }

  // Restore BootOrder
  Status = gRT->SetVariable (
                  EFI_BOOT_ORDER_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  SavedBootOrderSize,
                  SavedBootOrder
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error restoring BootOrder: %r\n", __FUNCTION__, Status));
  } else {
    DEBUG ((DEBUG_INFO, "%a: BootOrder successfully restored\n", __FUNCTION__));
  }

DeleteSaveAndCleanup:
  Status = gRT->SetVariable (
                  SAVED_BOOT_ORDER_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  0,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error deleting SavedBootOrder: %r\n", __FUNCTION__, Status));
  }

  Status = gRT->SetVariable (
                  SAVED_BOOT_ORDER_FLAGS_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  0,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error deleting SavedBootOrderFlags: %r\n", __FUNCTION__, Status));
  }

CleanupAndReturn:
  FREE_NON_NULL (SavedBootOrder);
  FREE_NON_NULL (BootOrder);
  FREE_NON_NULL (SavedBootOrderCopy);
  FREE_NON_NULL (SavedBootOrderFlags);
  if (Event != NULL) {
    gBS->CloseEvent (Event);
  }
}

STATIC
BOOLEAN
CheckBootToUiAppVariable (
  )
{
  EFI_STATUS  Status;
  BOOLEAN     BootToUiApp;
  UINTN       BootToUiAppSize;

  BootToUiApp     = FALSE;
  BootToUiAppSize = sizeof (BootToUiApp);
  Status          = gRT->GetVariable (
                           BOOT_TO_UIAPP_VARIABLE_NAME,
                           &gNVIDIATokenSpaceGuid,
                           NULL,
                           &BootToUiAppSize,
                           &BootToUiApp
                           );

  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DEBUG_INFO, "%a: BootToUiApp not found\n", __FUNCTION__));
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Got error getting BootToUiApp variable: %r\n", __FUNCTION__, Status));
    }
  }

  return BootToUiApp;
}

STATIC
EFI_STATUS
SetBootToUiAppVariable (
  IN BOOLEAN  BootToUiApp
  )
{
  EFI_STATUS  Status;

  Status = gRT->SetVariable (
                  BOOT_TO_UIAPP_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  sizeof (BootToUiApp),
                  &BootToUiApp
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got error setting BootToUiApp variable to %a: %r\n", __FUNCTION__, BootToUiApp ? "TRUE" : "FALSE", Status));
  }

  return Status;
}

STATIC
EFI_STATUS
SetOsIndications (
  IN UINT64  OsIndicationsValue,
  IN UINT64  OsIndicationsMask
  )
{
  EFI_STATUS  Status;
  UINT64      OsIndications;
  UINTN       OsIndicationsSize;

  OsIndications     = 0;
  OsIndicationsSize = sizeof (OsIndications);
  Status            = gRT->GetVariable (
                             EFI_OS_INDICATIONS_VARIABLE_NAME,
                             &gEfiGlobalVariableGuid,
                             NULL,
                             &OsIndicationsSize,
                             &OsIndications
                             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting OsIndications: %r. Will create it\n", __FUNCTION__, Status));
  }

  OsIndications &= ~OsIndicationsMask;
  OsIndications |= (OsIndicationsValue & OsIndicationsMask);
  Status         = gRT->SetVariable (
                          EFI_OS_INDICATIONS_VARIABLE_NAME,
                          &gEfiGlobalVariableGuid,
                          EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                          sizeof (OsIndications),
                          &OsIndications
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error setting OsIndications: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

VOID
EFIAPI
ProcessIPMIBootOrderUpdates (
  VOID
  )
{
  EFI_STATUS                    Status;
  IPMI_BOOT_OPTIONS_PARAMETERS  *BootOptionsParameters;
  NVIDIA_BOOT_ORDER_PRIORITY    *RequestedBootClass;
  NVIDIA_BOOT_ORDER_PRIORITY    *OptionBootClass;
  CHAR8                         *RequestedClassName;
  UINT8                         RequestedInstance;
  UINTN                         BootOrderIndex;
  UINT16                        *BootOrder;
  UINTN                         BootOrderSize;
  UINTN                         BootOrderLength;
  UINT16                        *ClassInstanceList;
  UINTN                         ClassInstanceLength;
  UINTN                         ClassInstanceLengthRemaining;
  BOOLEAN                       IPv6;
  EFI_EVENT                     ReadyToBootEvent;
  NVIDIA_BOOT_ORDER_PRIORITY    *VirtualBootClass;
  UINT16                        *VirtualInstanceList;
  UINTN                         VirtualInstanceLength;
  UINTN                         VirtualInstanceLengthRemaining;
  UINT16                        DesiredOptionNumber;
  BOOLEAN                       WillModifyBootOrder;
  UINT8                         BootOrderFlags;
  BOOLEAN                       BootToUiApp;

  ClassInstanceList   = NULL;
  BootOrder           = NULL;
  VirtualBootClass    = NULL;
  VirtualInstanceList = NULL;
  BootToUiApp         = CheckBootToUiAppVariable ();

  if ((mBootOptionsResponse == NULL) || (mBootOptionsRequest == NULL)) {
    goto CleanupAndReturn;
  }

  if (PcdGet8 (PcdIpmiNetworkBootMode) == 1) {
    IPv6 = TRUE;
  } else {
    IPv6 = FALSE;
  }

  BootOptionsParameters = (IPMI_BOOT_OPTIONS_PARAMETERS *)&mBootOptionsResponse->ParameterData[0];

  if (!BootOptionsParameters->Parm5.Data1.Bits.BootFlagValid) {
    goto AcknowledgeAndCleanup;
  }

  // TODO update UEFI verbosity based on BootOptionsParameters->Parm5.Data3.Bits.BiosVerbosity?

  switch (BootOptionsParameters->Parm5.Data2.Bits.BootDeviceSelector) {
    case IPMI_BOOT_DEVICE_SELECTOR_NO_OVERRIDE:
      DEBUG ((DEBUG_ERROR, "IPMI requested no change to BootOrder\n"));
      goto AcknowledgeAndCleanup;
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_PXE:
      RequestedClassName = IPv6 ? "pxev6" : "pxev4";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_HARDDRIVE:
      RequestedClassName = "nvme";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_HARDDRIVE_SAFE_MODE:
      DEBUG ((DEBUG_WARN, "Ignoring unsupported boot device selector IPMI_BOOT_DEVICE_SELECTOR_HARDDRIVE_SAFE_MODE\n"));
      goto AcknowledgeAndCleanup;
    case IPMI_BOOT_DEVICE_SELECTOR_DIAGNOSTIC_PARTITION:
      DEBUG ((DEBUG_WARN, "Ignoring unsupported boot device selector IPMI_BOOT_DEVICE_SELECTOR_DIAGNOSTIC_PARTITION\n"));
      goto AcknowledgeAndCleanup;
    case IPMI_BOOT_DEVICE_SELECTOR_CD_DVD:
      RequestedClassName = "cdrom";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_BIOS_SETUP:
      BootToUiApp = TRUE;
      if (BootOptionsParameters->Parm5.Data1.Bits.PersistentOptions) {
        DEBUG ((DEBUG_ERROR, "IPMI requested to boot to UEFI Menu persistently\n"));
        Status = SetBootToUiAppVariable (TRUE);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Unable to make the change persistent: %r\n", Status));
        }
      } else {
        DEBUG ((DEBUG_ERROR, "IPMI requested to boot to UEFI Menu for this boot\n"));
      }

      goto AcknowledgeAndCleanup;
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_FLOPPY:
      RequestedClassName = "sata";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_CD_DVD:
      RequestedClassName = IPv6 ? "httpv6" : "httpv4";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_PRIMARY_REMOTE_MEDIA:
      DEBUG ((DEBUG_WARN, "Ignoring unsupported boot device selector IPMI_BOOT_DEVICE_SELECTOR_PRIMARY_REMOTE_MEDIA\n"));
      goto AcknowledgeAndCleanup;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_HARDDRIVE:
      RequestedClassName = "scsi";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_FLOPPY:
      RequestedClassName = "usb";
      // Redfish wants "usb" to treat "virtual" as higher priority USB devices than normal USB devices
      VirtualBootClass = GetBootClassOfName ("virtual", AsciiStrLen ("virtual"), mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
      break;
    default:
      DEBUG ((DEBUG_WARN, "Ignoring unknown boot device selector %d\n", BootOptionsParameters->Parm5.Data2.Bits.BootDeviceSelector));
      goto AcknowledgeAndCleanup;
  }

  RequestedBootClass = GetBootClassOfName (RequestedClassName, AsciiStrLen (RequestedClassName), mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
  if (RequestedBootClass == NULL) {
    DEBUG ((DEBUG_WARN, "Ignoring unsupported boot class \"%a\"\n", RequestedClassName));
    goto AcknowledgeAndCleanup;
  }

  RequestedInstance = BootOptionsParameters->Parm5.Data5.Bits.DeviceInstanceSelector;
  // Note: bit 4 selects between external(0) and internal(1) device instances, but we don't have that distinction, so ignore it
  RequestedInstance &= 0x0F;

  Status = GetEfiGlobalVariable2 (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &BootOrderSize);
  if (EFI_ERROR (Status) || (BootOrder == NULL)) {
    DEBUG ((DEBUG_ERROR, "Unable to determine BootOrder (Status %r) - ignoring request to prioritize %a instance %u\n", Status, RequestedClassName, RequestedInstance));
    goto AcknowledgeAndCleanup;
  }

  BootOrderLength = BootOrderSize/sizeof (BootOrder[0]);

  ClassInstanceLength = 0;
  ClassInstanceList   = AllocatePool (BootOrderSize);
  if (ClassInstanceList == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to allocate memory to process BootOrder - ignoring request to prioritize %a instance %u\n", RequestedClassName, RequestedInstance));
    goto AcknowledgeAndCleanup;
  }

  VirtualInstanceLength = 0;
  if (VirtualBootClass != NULL) {
    VirtualInstanceList = AllocatePool (BootOrderSize);
    if (VirtualInstanceList == NULL) {
      DEBUG ((DEBUG_ERROR, "Unable to allocate memory to process virtual devices - ignoring request to prioritize %a instance %u\n", RequestedClassName, RequestedInstance));
      goto AcknowledgeAndCleanup;
    }
  }

  // Find the list of instances of RequestedClass in BootOrder
  for (BootOrderIndex = 0; BootOrderIndex < BootOrderLength; BootOrderIndex++) {
    Status = GetBootClassOfOptionNum (BootOrder[BootOrderIndex], &OptionBootClass, mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
    if (Status == EFI_NOT_FOUND) {
      OptionBootClass = NULL; // eg. the "ubuntu" option
    } else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error (%r) parsing BootOrder - ignoring request to prioritize %a instance %u\n", Status, RequestedClassName, RequestedInstance));
      goto AcknowledgeAndCleanup;
    }

    if (VirtualBootClass && (OptionBootClass == VirtualBootClass)) {
      VirtualInstanceList[VirtualInstanceLength] = BootOrder[BootOrderIndex];
      VirtualInstanceLength++;
    } else if (OptionBootClass == RequestedBootClass) {
      ClassInstanceList[ClassInstanceLength] = BootOrder[BootOrderIndex];
      ClassInstanceLength++;
    }
  }

  if ((ClassInstanceLength == 0) && (VirtualInstanceLength == 0)) {
    DEBUG ((DEBUG_ERROR, "Unable to find any instance of %a in BootOrder - Ignoring boot order change request from IPMI\n", RequestedClassName));
    goto AcknowledgeAndCleanup;
  }

  ClassInstanceLengthRemaining   = ClassInstanceLength;
  VirtualInstanceLengthRemaining = VirtualInstanceLength;
  // Find the index of the N-th occurance of RequestedClass in BootOrder when sorted by number
  if (RequestedInstance == 0) {
    // We will move all instances to the start of boot order, going through the list backwards to preserve relative ordering
    // We want to end with Virtual classes at the front of the list, so start with real classes if available
    if (ClassInstanceLengthRemaining > 0) {
      DesiredOptionNumber = ClassInstanceList[--ClassInstanceLengthRemaining];
    } else {
      DesiredOptionNumber = VirtualInstanceList[--VirtualInstanceLengthRemaining];
    }
  } else if (RequestedInstance-1 < VirtualInstanceLengthRemaining) {
    PerformQuickSort (VirtualInstanceList, VirtualInstanceLengthRemaining, sizeof (VirtualInstanceList[0]), Uint16SortCompare);
    DesiredOptionNumber = VirtualInstanceList[RequestedInstance-1];
  } else if ((RequestedInstance - 1 - VirtualInstanceLengthRemaining) < ClassInstanceLengthRemaining) {
    PerformQuickSort (ClassInstanceList, ClassInstanceLengthRemaining, sizeof (ClassInstanceList[0]), Uint16SortCompare);
    DesiredOptionNumber = ClassInstanceList[RequestedInstance - 1 - VirtualInstanceLengthRemaining];
  } else {
    DEBUG ((DEBUG_WARN, "Unable to find requested instance %u of %a - Using all instances instead\n", RequestedInstance, RequestedClassName));
    RequestedInstance = 0;
    if (ClassInstanceLengthRemaining > 0) {
      DesiredOptionNumber = ClassInstanceList[--ClassInstanceLengthRemaining];
    } else {
      DesiredOptionNumber = VirtualInstanceList[--VirtualInstanceLengthRemaining];
    }
  }

  for (BootOrderIndex = 0; BootOrderIndex < BootOrderLength; BootOrderIndex++) {
    if (BootOrder[BootOrderIndex] == DesiredOptionNumber) {
      break;
    }
  }

  if (RequestedInstance == 0) {
    WillModifyBootOrder = FALSE;
    if (VirtualInstanceLength) {
      WillModifyBootOrder |= CompareMem (BootOrder, VirtualInstanceList, VirtualInstanceLength*sizeof (BootOrder[0])) != 0;
    }

    if (ClassInstanceLength) {
      WillModifyBootOrder |= CompareMem (&BootOrder[VirtualInstanceLength], ClassInstanceList, ClassInstanceLength*sizeof (BootOrder[0])) != 0;
    }
  } else {
    WillModifyBootOrder = (BootOrderIndex > 0);
  }

  // At this point BootOrderIndex is the entry to move to the start of the list first
  if (BootOptionsParameters->Parm5.Data1.Bits.PersistentOptions) {
    if (RequestedInstance == 0) {
      DEBUG ((DEBUG_ERROR, "IPMI requested to move all instances of %a to the start of BootOrder\n", RequestedClassName));
    } else {
      DEBUG ((DEBUG_ERROR, "IPMI requested to move %a instance %u to the start of BootOrder\n", RequestedClassName, RequestedInstance));
    }
  } else {
    if (RequestedInstance == 0) {
      DEBUG ((DEBUG_ERROR, "IPMI requested to use all instances of %a for this boot\n", RequestedClassName));
    } else {
      DEBUG ((DEBUG_ERROR, "IPMI requested to use %a instance %u for this boot\n", RequestedClassName, RequestedInstance));
    }

    // Prepare to restore BootOrder after this boot
    if (WillModifyBootOrder) {
      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      RestoreBootOrder,
                      NULL,
                      &gEfiEventReadyToBootGuid,
                      &ReadyToBootEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error registering ReadyToBoot event handler to restore BootOrder: %r\n", __FUNCTION__, Status));
        goto AcknowledgeAndCleanup;
      }

      Status = gRT->SetVariable (
                      // This is not getting saved correctly so that it can be looked up!
                      SAVED_BOOT_ORDER_VARIABLE_NAME,
                      &gNVIDIATokenSpaceGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                      BootOrderSize,
                      BootOrder
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error saving current BootOrder: %r\n", __FUNCTION__, Status));
        goto AcknowledgeAndCleanup;
      }

      BootOrderFlags = 0;
      if (RequestedInstance == 0) {
        BootOrderFlags |= SAVED_BOOT_ORDER_ALL_INSTANCES_FLAG;
      }

      if (VirtualBootClass != NULL) {
        BootOrderFlags |= SAVED_BOOT_ORDER_VIRTUAL_FLAG;
      }

      if (BootOrderFlags != 0) {
        // Save some flags for Restore
        Status = gRT->SetVariable (
                        SAVED_BOOT_ORDER_FLAGS_VARIABLE_NAME,
                        &gNVIDIATokenSpaceGuid,
                        EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                        sizeof (BootOrderFlags),
                        &BootOrderFlags
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Error saving BootOrder flags: %r\n", __FUNCTION__, Status));
          goto AcknowledgeAndCleanup;
        }
      }
    }
  }

  // Finally, update the BootOrder if necessary
  if (WillModifyBootOrder) {
    if (BootOrderIndex > 0) {
      MOVE_INDEX_TO_START (BootOrder, BootOrderIndex);
    }

    if (RequestedInstance == 0) {
      // Note: In this case the unmoved ClassInstanceList and VirtualInstanceList elements are ordered
      // in the same order as in BootOrder, so we can be smart when searching BootOrder for them
      while (ClassInstanceLengthRemaining > 0) {
        DesiredOptionNumber = ClassInstanceList[--ClassInstanceLengthRemaining];
        while ((BootOrderIndex > 0) && (BootOrder[BootOrderIndex] != DesiredOptionNumber)) {
          BootOrderIndex--;
        }

        NV_ASSERT_RETURN (BootOrderIndex > 0, goto AcknowledgeAndCleanup, "%a: Failed to parse BootOrder correctly to find ClassInstance\n", __FUNCTION__);
        MOVE_INDEX_TO_START (BootOrder, BootOrderIndex);
      }

      BootOrderIndex = BootOrderLength;
      while (VirtualInstanceLengthRemaining > 0) {
        DesiredOptionNumber = VirtualInstanceList[--VirtualInstanceLengthRemaining];
        while ((BootOrderIndex > 0) && (BootOrder[BootOrderIndex] != DesiredOptionNumber)) {
          BootOrderIndex--;
        }

        NV_ASSERT_RETURN (BootOrderIndex > 0, goto AcknowledgeAndCleanup, "%a: Failed to parse BootOrder correctly to find VirtualInstance\n", __FUNCTION__);
        MOVE_INDEX_TO_START (BootOrder, BootOrderIndex);
      }
    }

    Status = gRT->SetVariable (
                    EFI_BOOT_ORDER_VARIABLE_NAME,
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    BootOrderSize,
                    BootOrder
                    );
    if (EFI_ERROR (Status)) {
      if (RequestedInstance == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Error moving all instances of %a to the start of BootOrder: %r\n", __FUNCTION__, RequestedClassName, Status));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Error moving %a instance %u to the start of BootOrder: %r\n", __FUNCTION__, RequestedClassName, RequestedInstance, Status));
      }
    }

    PrintBootOrder (DEBUG_INFO, L"BootOrder after IPMI-requested change:", NULL, 0);
  } else {
    DEBUG ((DEBUG_INFO, "%a: IPMI request doesn't modify BootOrder\n", __FUNCTION__));
  }

  // We've successfully processed a BootOrder update that wasn't a request for UiApp or None, so don't run UiApp
  BootToUiApp = FALSE;
  if (BootOptionsParameters->Parm5.Data1.Bits.PersistentOptions) {
    // Something else is persistently booting now
    SetBootToUiAppVariable (FALSE);
  }

AcknowledgeAndCleanup:
  BootOptionsParameters = (IPMI_BOOT_OPTIONS_PARAMETERS *)&mBootOptionsRequest->ParameterData[0];

  SetMem (BootOptionsParameters, sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4), 0);
  BootOptionsParameters->Parm4.WriteMask                    = BOOT_OPTION_HANDLED_BY_BIOS;
  BootOptionsParameters->Parm4.BootInitiatorAcknowledgeData = 0;
  Status                                                    = SetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK, mBootOptionsRequest);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Error acknowledging IPMI boot order request: %r\n", Status));
  }

  SetMem (BootOptionsParameters, sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5), 0);
  Status = SetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS, mBootOptionsRequest);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Error clearing IPMI boot flags: %r\n", Status));
  }

CleanupAndReturn:
  if (BootToUiApp) {
    SetOsIndications (EFI_OS_INDICATIONS_BOOT_TO_FW_UI, EFI_OS_INDICATIONS_BOOT_TO_FW_UI);
  }

  FREE_NON_NULL (mBootOptionsRequest);
  FREE_NON_NULL (mBootOptionsResponse);
  FREE_NON_NULL (ClassInstanceList);
  FREE_NON_NULL (VirtualInstanceList);
  FREE_NON_NULL (BootOrder);

  return;
}
