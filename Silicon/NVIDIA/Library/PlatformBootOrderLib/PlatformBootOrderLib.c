/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/SortLib.h>
#include <Library/DebugLib.h>
#include <Library/FwVariableLib.h>
#include <IndustryStandard/Ipmi.h>
#include <Guid/GlobalVariable.h>

#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

#define NVIDIA_BOOT_TYPE_HTTP                    0
#define NVIDIA_BOOT_TYPE_BOOTIMG                 1
#define NVIDIA_BOOT_TYPE_VIRTUAL                 2
#define IPMI_GET_BOOT_OPTIONS_PARAMETER_INVALID  1
#define IPMI_PARAMETER_VERSION                   1

#define SAVED_BOOT_ORDER_VARIABLE_NAME  L"SavedBootOrder"

typedef struct {
  CHAR8    *OrderName;
  INT32    PriorityOrder;
  UINT8    Type;
  UINT8    SubType;
  UINT8    ExtraSpecifier;
} NVIDIA_BOOT_ORDER_PRIORITY;

STATIC NVIDIA_BOOT_ORDER_PRIORITY  mBootPriority[] = {
  { "scsi",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SCSI_DP,           MAX_UINT8                },
  { "usb",      MAX_INT32, MESSAGING_DEVICE_PATH, MSG_USB_DP,            MAX_UINT8                },
  { "sata",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SATA_DP,           MAX_UINT8                },
  { "pxev4",    MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv4_DP,           MAX_UINT8                },
  { "httpv4",   MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv4_DP,           NVIDIA_BOOT_TYPE_HTTP    },
  { "pxev6",    MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv6_DP,           MAX_UINT8                },
  { "httpv6",   MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv6_DP,           NVIDIA_BOOT_TYPE_HTTP    },
  { "nvme",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_NVME_NAMESPACE_DP, MAX_UINT8                },
  { "ufs",      MAX_INT32, MESSAGING_DEVICE_PATH, MSG_UFS_DP,            MAX_UINT8                },
  { "sd",       MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SD_DP,             MAX_UINT8                },
  { "emmc",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_EMMC_DP,           MAX_UINT8                },
  { "cdrom",    MAX_INT32, MEDIA_DEVICE_PATH,     MEDIA_CDROM_DP,        MAX_UINT8                },
  { "boot.img", MAX_INT32, MAX_UINT8,             MAX_UINT8,             NVIDIA_BOOT_TYPE_BOOTIMG },
  { "virtual",  MAX_INT32, MESSAGING_DEVICE_PATH, MSG_USB_DP,            NVIDIA_BOOT_TYPE_VIRTUAL },
};

STATIC  IPMI_GET_BOOT_OPTIONS_RESPONSE  *mBootOptionsResponse = NULL;
STATIC  IPMI_SET_BOOT_OPTIONS_REQUEST   *mBootOptionsRequest  = NULL;

#define DEFAULT_BOOT_ORDER_STRING  "boot.img,usb,sd,emmc,ufs"

STATIC
VOID
PrintBootOrder (
  IN CONST UINTN   DebugPrintLevel,
  IN CONST CHAR16  *HeaderMessage
  )
{
  EFI_STATUS                    Status;
  UINT16                        *BootOrder;
  UINTN                         BootOrderSize;
  UINTN                         BootOrderLength;
  UINTN                         BootOrderIndex;
  CHAR16                        OptionName[sizeof ("Boot####")];
  EFI_BOOT_MANAGER_LOAD_OPTION  Option;

  if (!DebugPrintLevelEnabled (DebugPrintLevel)) {
    return;
  }

  BootOrder = NULL;

  // Gather BootOrder
  Status = GetEfiGlobalVariable2 (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &BootOrderSize);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DebugPrintLevel, "%a: No BootOrder found\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DebugPrintLevel, "%a: Unable to determine BootOrder: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  if ((BootOrder == NULL) || (BootOrderSize == 0)) {
    DEBUG ((DebugPrintLevel, "%a: BootOrder is empty\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  BootOrderLength = BootOrderSize/sizeof (BootOrder[0]);

  DEBUG ((DebugPrintLevel, "%a: %s\n", __FUNCTION__, HeaderMessage));
  for (BootOrderIndex = 0; BootOrderIndex < BootOrderLength; BootOrderIndex++) {
    UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOrder[BootOrderIndex]);
    Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
    if (EFI_ERROR (Status)) {
      DEBUG ((DebugPrintLevel, "%a: Error getting boot option for BootOrder[%llu] = 0x%04x: %r\n", __FUNCTION__, BootOrderIndex, BootOrder[BootOrderIndex], Status));
      goto CleanupAndReturn;
    }

    DEBUG ((DebugPrintLevel, "%a: BootOrder[%llu] = 0x%04x = %s\n", __FUNCTION__, BootOrderIndex, BootOrder[BootOrderIndex], Option.Description));
    EfiBootManagerFreeLoadOption (&Option);
  }

CleanupAndReturn:
  FREE_NON_NULL (BootOrder);
}

NVIDIA_BOOT_ORDER_PRIORITY *
EFIAPI
GetBootClassOfOption (
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *Option
  )
{
  UINTN                     OptionalDataSize;
  UINTN                     BootPriorityIndex;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePathNode;
  UINT8                     ExtraSpecifier;

  OptionalDataSize = 0;
  if (Option->OptionalData != NULL) {
    OptionalDataSize = StrSize ((CONST CHAR16 *)Option->OptionalData);
  }

  if ((Option->OptionalData != NULL) &&
      (Option->OptionalDataSize == OptionalDataSize + sizeof (EFI_GUID)) &&
      CompareGuid (
        (EFI_GUID *)((UINT8 *)Option->OptionalData + OptionalDataSize),
        &gNVIDIABmBootOptionGuid
        ))
  {
    for (BootPriorityIndex = 0; BootPriorityIndex < ARRAY_SIZE (mBootPriority); BootPriorityIndex++) {
      if (mBootPriority[BootPriorityIndex].ExtraSpecifier == NVIDIA_BOOT_TYPE_BOOTIMG) {
        DEBUG ((DEBUG_VERBOSE, "Option %d has class %a\n", Option->OptionNumber, mBootPriority[BootPriorityIndex].OrderName));
        return &mBootPriority[BootPriorityIndex];
      }
    }
  }

  ExtraSpecifier = MAX_UINT8;
  DevicePathNode = Option->FilePath;
  while (!IsDevicePathEndType (DevicePathNode)) {
    if ((DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) &&
        (DevicePathSubType (DevicePathNode) == MSG_URI_DP))
    {
      ExtraSpecifier = NVIDIA_BOOT_TYPE_HTTP;
      break;
    } else if ((DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) &&
               (DevicePathSubType (DevicePathNode) == MSG_USB_DP) &&
               (StrStr (Option->Description, L"Virtual")))
    {
      ExtraSpecifier = NVIDIA_BOOT_TYPE_VIRTUAL;
      break;
    }

    DevicePathNode = NextDevicePathNode (DevicePathNode);
  }

  DevicePathNode = Option->FilePath;
  while (!IsDevicePathEndType (DevicePathNode)) {
    for (BootPriorityIndex = 0; BootPriorityIndex < ARRAY_SIZE (mBootPriority); BootPriorityIndex++) {
      if ((DevicePathType (DevicePathNode) == mBootPriority[BootPriorityIndex].Type) &&
          (DevicePathSubType (DevicePathNode) == mBootPriority[BootPriorityIndex].SubType) &&
          (ExtraSpecifier == mBootPriority[BootPriorityIndex].ExtraSpecifier))
      {
        DEBUG ((DEBUG_VERBOSE, "Option %d has class %a\n", Option->OptionNumber, mBootPriority[BootPriorityIndex].OrderName));
        return &mBootPriority[BootPriorityIndex];
      }
    }

    DevicePathNode = NextDevicePathNode (DevicePathNode);
  }

  return NULL;
}

NVIDIA_BOOT_ORDER_PRIORITY *
EFIAPI
GetBootClassOfName (
  CHAR8  *ClassName,
  UINTN  ClassNameLen
  )
{
  UINTN  BootPriorityIndex;
  UINTN  BootPriorityMatchLen;

  for (BootPriorityIndex = 0; BootPriorityIndex < ARRAY_SIZE (mBootPriority); BootPriorityIndex++) {
    BootPriorityMatchLen = AsciiStrLen (mBootPriority[BootPriorityIndex].OrderName);
    if ((BootPriorityMatchLen == ClassNameLen) &&
        (CompareMem (ClassName, mBootPriority[BootPriorityIndex].OrderName, ClassNameLen) == 0))
    {
      return &mBootPriority[BootPriorityIndex];
    }
  }

  DEBUG ((DEBUG_ERROR, "Unable to determine class of boot device type \"%a\"\r\n", ClassName));
  return NULL;
}

STATIC
INT32
EFIAPI
GetDevicePriority (
  IN UINT16  BootOption
  )
{
  EFI_STATUS                    Status;
  CHAR16                        OptionName[sizeof ("Boot####")];
  EFI_BOOT_MANAGER_LOAD_OPTION  Option;
  NVIDIA_BOOT_ORDER_PRIORITY    *BootPriorityClass;
  INT32                         DevicePriority;

  UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOption);
  Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
  if (EFI_ERROR (Status)) {
    return MAX_INT32;
  }

  BootPriorityClass = GetBootClassOfOption (&Option);

  if (BootPriorityClass == NULL) {
    DevicePriority = MAX_INT32;
  } else {
    DevicePriority = BootPriorityClass->PriorityOrder;
  }

  DEBUG ((DEBUG_VERBOSE, "Found %s priority to be %d\r\n", ConvertDevicePathToText (Option.FilePath, TRUE, FALSE), DevicePriority));
  EfiBootManagerFreeLoadOption (&Option);
  return DevicePriority;
}

STATIC
INTN
EFIAPI
BootOrderSortCompare (
  IN CONST VOID  *Buffer1,
  IN CONST VOID  *Buffer2
  )
{
  INT32  Priority1;
  INT32  Priority2;

  Priority1 = GetDevicePriority (*(UINT16 *)Buffer1);
  Priority2 = GetDevicePriority (*(UINT16 *)Buffer2);

  return Priority1 - Priority2;
}

VOID
EFIAPI
ParseDefaultBootPriority (
  VOID
  )
{
  EFI_STATUS                  Status;
  INT32                       Priority;
  CHAR8                       *DefaultBootOrder;
  UINTN                       DefaultBootOrderSize;
  CHAR8                       *CurrentBootPriorityStr;
  CHAR8                       *CurrentBootPriorityEnd;
  UINTN                       CurrentBootPriorityLen;
  NVIDIA_BOOT_ORDER_PRIORITY  *ClassBootPriority;

  Priority = 0;
  // Process the priority order
  Status = GetVariable2 (
             L"DefaultBootPriority",
             &gNVIDIATokenSpaceGuid,
             (VOID **)&DefaultBootOrder,
             &DefaultBootOrderSize
             );
  if (EFI_ERROR (Status)) {
    DefaultBootOrder     = DEFAULT_BOOT_ORDER_STRING;
    DefaultBootOrderSize = sizeof (DEFAULT_BOOT_ORDER_STRING);
  }

  CurrentBootPriorityStr = DefaultBootOrder;
  while (CurrentBootPriorityStr < (DefaultBootOrder + DefaultBootOrderSize)) {
    CurrentBootPriorityEnd = CurrentBootPriorityStr;
    while (CurrentBootPriorityEnd < (DefaultBootOrder + DefaultBootOrderSize)) {
      if ((*CurrentBootPriorityEnd == ',') ||
          (*CurrentBootPriorityEnd == '\0'))
      {
        break;
      }

      CurrentBootPriorityEnd++;
    }

    CurrentBootPriorityLen = CurrentBootPriorityEnd - CurrentBootPriorityStr;

    ClassBootPriority = GetBootClassOfName (CurrentBootPriorityStr, CurrentBootPriorityLen);
    if (ClassBootPriority != NULL) {
      DEBUG ((DEBUG_INFO, "Setting %a priority to %d\r\n", ClassBootPriority->OrderName, Priority));
      ClassBootPriority->PriorityOrder = Priority;
      Priority++;
    } else {
      *CurrentBootPriorityEnd = '\0';
      DEBUG ((DEBUG_ERROR, "Ignoring unknown boot class %a\r\n", CurrentBootPriorityStr));
    }

    CurrentBootPriorityStr += CurrentBootPriorityLen + 1;
  }

  return;
}

VOID
EFIAPI
SetBootOrder (
  VOID
  )
{
  EFI_STATUS  Status;
  BOOLEAN     VariableData;
  UINTN       VariableSize;
  UINT32      VariableAttributes;

  VariableData = FALSE;
  VariableSize = sizeof (BOOLEAN);
  Status       = gRT->GetVariable (
                        L"PlatformBootOrderSet",
                        &gNVIDIATokenSpaceGuid,
                        &VariableAttributes,
                        &VariableSize,
                        (VOID *)&VariableData
                        );
  if (!EFI_ERROR (Status) && (VariableSize == sizeof (BOOLEAN))) {
    if (VariableData == TRUE) {
      return;
    }
  }

  ParseDefaultBootPriority ();

  EfiBootManagerSortLoadOptionVariable (LoadOptionTypeBoot, BootOrderSortCompare);

  VariableData = TRUE;
  gRT->SetVariable (
         L"PlatformBootOrderSet",
         &gNVIDIATokenSpaceGuid,
         EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
         sizeof (BOOLEAN),
         &VariableData
         );

  return;
}

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
    DEBUG ((DEBUG_ERROR, "Failed to get BMC Boot Options Parameter %d (IPMI CompCode = 0x%x)\r\n", ParameterSelector, BootOptionsResponse->CompletionCode));
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
    DEBUG ((DEBUG_ERROR, "Failed to set BMC Boot Options Parameter %d (IPMI CompCode = 0x%x)\r\n", ParameterSelector, BootOptionsResponse.CompletionCode));
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
        // Don't reset the system, but instead process the rest of the command to avoid a reboot cycle
        goto Done;
      }

      // Reset
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
// and then move one element to the beginning. This function will restore
// the original BootOrder unless additional modifications have been made.
VOID
EFIAPI
RestoreBootOrder (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
  UINT16      *SavedBootOrder;
  UINTN       SavedBootOrderSize;
  UINTN       SavedBootOrderLength;
  UINT16      *BootOrder;
  UINTN       BootOrderSize;
  UINTN       BootOrderLength;
  UINT16      *BootCurrent;
  UINTN       BootCurrentSize;
  UINTN       ReorderedIndex;
  UINT16      ReorderedBootNum;
  EFI_STATUS  Status;

  SavedBootOrder = NULL;
  BootOrder      = NULL;

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

  // Make sure BootOrder only has one device moved to the front vs. SavedBootOrder
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

  if ((CompareMem (&SavedBootOrder[0], &BootOrder[1], ReorderedIndex * sizeof (BootOrder[0])) != 0) ||
      (CompareMem (
         &SavedBootOrder[ReorderedIndex+1],
         &BootOrder[ReorderedIndex+1],
         (BootOrderLength - 1 - ReorderedIndex) * sizeof (BootOrder[0])
         ) != 0))
  {
    DEBUG ((DEBUG_WARN, "%a: BootOrder and SavedBootOrder have more changes than expected. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  // At this point, we've confirmed that BootOrder equals SavedBootOrder except with one device moved to the beginning

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

CleanupAndReturn:
  FREE_NON_NULL (SavedBootOrder);
  FREE_NON_NULL (BootOrder);
  if (Event != NULL) {
    gBS->CloseEvent (Event);
  }
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
  INTN                          FirstBootOrderIndex;
  UINT16                        *BootOrder;
  UINTN                         BootOrderSize;
  UINTN                         BootOrderLength;
  UINT16                        *ClassInstanceList;
  UINTN                         ClassInstanceLength;
  UINTN                         ClassInstanceIndex;
  UINT16                        BootOption;
  UINT64                        OsIndications;
  UINTN                         OsIndicationsSize;
  CHAR16                        OptionName[sizeof ("Boot####")];
  EFI_BOOT_MANAGER_LOAD_OPTION  Option;
  BOOLEAN                       IPv6;
  EFI_EVENT                     ReadyToBootEvent;

  ClassInstanceList = NULL;
  BootOrder         = NULL;

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
        DEBUG ((DEBUG_ERROR, "Error getting OsIndications to request to boot to UEFI menu: %r\n", Status));
      }

      OsIndications |= EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
      Status         = gRT->SetVariable (
                              EFI_OS_INDICATIONS_VARIABLE_NAME,
                              &gEfiGlobalVariableGuid,
                              EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                              sizeof (OsIndications),
                              &OsIndications
                              );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Error setting OsIndications to request to boot to UEFI menu: %r\n", Status));
      }

      DEBUG ((DEBUG_ERROR, "IPMI requested to boot to UEFI Menu\n"));

      goto AcknowledgeAndCleanup;
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_FLOPPY:
      RequestedClassName = "sata";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_CD_DVD:
      RequestedClassName = IPv6 ? "httpv6" : "httpv4";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_PRIMARY_REMOTE_MEDIA:
      RequestedClassName = "virtual";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_HARDDRIVE:
      RequestedClassName = "scsi";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_FLOPPY:
      RequestedClassName = "usb";
      break;
    default:
      DEBUG ((DEBUG_WARN, "Ignoring unknown boot device selector %d\n", BootOptionsParameters->Parm5.Data2.Bits.BootDeviceSelector));
      goto AcknowledgeAndCleanup;
  }

  RequestedBootClass = GetBootClassOfName (RequestedClassName, AsciiStrLen (RequestedClassName));
  if (RequestedBootClass == NULL) {
    DEBUG ((DEBUG_WARN, "Ignoring unsupported boot class \"%a\"\n", RequestedClassName));
    goto AcknowledgeAndCleanup;
  }

  RequestedInstance = BootOptionsParameters->Parm5.Data5.Bits.DeviceInstanceSelector;
  // Note: bit 4 selects between external(0) and internal(1) device instances, but we don't have that distinction, so ignore it
  RequestedInstance &= 0x0F;

  Status = GetEfiGlobalVariable2 (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &BootOrderSize);
  if (EFI_ERROR (Status) || (BootOrder == NULL)) {
    DEBUG ((DEBUG_ERROR, "Unable to determine BootOrder (Status %r) - ignoring request to prioritize %a instance %d\n", Status, RequestedClassName, RequestedInstance));
    goto AcknowledgeAndCleanup;
  }

  BootOrderLength = BootOrderSize/sizeof (BootOrder[0]);

  if (RequestedInstance > 0) {
    ClassInstanceList = AllocatePool (BootOrderSize);
    if (ClassInstanceList == NULL) {
      DEBUG ((DEBUG_ERROR, "Unable to allocate memory to process BootOrder - ignoring request to prioritize %a instance %d\n", RequestedClassName, RequestedInstance));
      goto AcknowledgeAndCleanup;
    }

    ClassInstanceLength = 0;
  }

  // Find the index of the first occurance of RequestedClass in BootOrder
  // and the list of instances of RequestedClass in BootOrder [if RequestedInstance > 0]
  FirstBootOrderIndex = -1;
  for (BootOrderIndex = 0; BootOrderIndex < BootOrderLength; BootOrderIndex++) {
    UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOrder[BootOrderIndex]);
    Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error (%r) parsing BootOrder - ignoring request to prioritize %a instance %d\n", Status, RequestedClassName, RequestedInstance));
      goto AcknowledgeAndCleanup;
    }

    OptionBootClass = GetBootClassOfOption (&Option);
    EfiBootManagerFreeLoadOption (&Option);

    if (OptionBootClass == RequestedBootClass) {
      if (FirstBootOrderIndex == -1) {
        FirstBootOrderIndex = BootOrderIndex;
        if (RequestedInstance == 0) {
          break;
        }
      }

      ClassInstanceList[ClassInstanceLength] = BootOrder[BootOrderIndex];
      ClassInstanceLength++;
    }
  }

  if (FirstBootOrderIndex == -1) {
    DEBUG ((DEBUG_ERROR, "Unable to find any instance of %a in BootOrder - Ignoring boot order change request from IPMI\n", RequestedClassName));
    goto AcknowledgeAndCleanup;
  }

  // Find the index of the N-th occurance of RequestedClass in BootOrder when sorted by number
  if (RequestedInstance == 0) {
    BootOrderIndex = FirstBootOrderIndex;
  } else {
    if (RequestedInstance-1 < ClassInstanceLength) {
      PerformQuickSort (ClassInstanceList, ClassInstanceLength, sizeof (ClassInstanceList[0]), Uint16SortCompare);
      ClassInstanceIndex = RequestedInstance-1;
      for (BootOrderIndex = 0; BootOrderIndex < BootOrderLength; BootOrderIndex++) {
        if (BootOrder[BootOrderIndex] == ClassInstanceList[ClassInstanceIndex]) {
          break;
        }
      }
    } else {
      DEBUG ((DEBUG_WARN, "Unable to find requested instance %d of %a - Using first found instance instead\n", RequestedInstance, RequestedClassName));
      BootOrderIndex = FirstBootOrderIndex;
    }
  }

  // At this point BootOrderIndex is the entry to move to the start of the list

  if (BootOptionsParameters->Parm5.Data1.Bits.PersistentOptions) {
    DEBUG ((DEBUG_ERROR, "IPMI requested to move %a instance %d to the start of BootOrder\n", RequestedClassName, RequestedInstance));
  } else {
    DEBUG ((DEBUG_ERROR, "IPMI requested to use %a instance %d for this boot\n", RequestedClassName, RequestedInstance));

    // Prepare to restore BootOrder after this boot
    if (BootOrderIndex > 0) {
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
    }
  }

  // Finally, update the BootOrder
  if (BootOrderIndex > 0) {
    BootOption = BootOrder[BootOrderIndex];
    CopyMem (&BootOrder[1], &BootOrder[0], BootOrderIndex*sizeof (BootOrder[0]));
    BootOrder[0] = BootOption;

    Status = gRT->SetVariable (
                    EFI_BOOT_ORDER_VARIABLE_NAME,
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    BootOrderSize,
                    BootOrder
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error moving %a instance %d to the start of BootOrder: %r\n", __FUNCTION__, RequestedClassName, RequestedInstance, Status));
    }

    PrintBootOrder (DEBUG_INFO, L"BootOrder after IPMI-requested change:");
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
  FREE_NON_NULL (mBootOptionsRequest);
  FREE_NON_NULL (mBootOptionsResponse);
  FREE_NON_NULL (ClassInstanceList);
  FREE_NON_NULL (BootOrder);

  return;
}
