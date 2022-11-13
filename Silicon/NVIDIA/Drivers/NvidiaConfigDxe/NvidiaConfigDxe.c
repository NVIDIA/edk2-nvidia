/** @file
*  NVIDIA Configuration Dxe
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Guid/MdeModuleHii.h>
#include <Guid/HiiPlatformSetupFormset.h>

#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigRouting.h>

#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/HiiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiLib.h>

#include "NvidiaConfigHii.h"

#define MAX_VARIABLE_NAME  (256 * sizeof(CHAR16))

extern EFI_GUID  gNVIDIAResourceConfigFormsetGuid;

//
// These are the VFR compiler generated data representing our VFR data.
//
extern UINT8  NvidiaConfigHiiBin[];
extern UINT8  NvidiaConfigDxeStrings[];

//
// HII specific Vendor Device Path definition.
//
typedef struct {
  VENDOR_DEVICE_PATH          VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} HII_VENDOR_DEVICE_PATH;

HII_VENDOR_DEVICE_PATH  mNvidiaConfigHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    NVIDIA_CONFIG_FORMSET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

EFI_HII_CONFIG_ACCESS_PROTOCOL  mConfigAccess;
CHAR16                          mHiiControlStorageName[] = L"NVIDIA_CONFIG_HII_CONTROL";
NVIDIA_CONFIG_HII_CONTROL       mHiiControlSettings      = { 0 };
EFI_HANDLE                      mDriverHandle;

/**
  Initializes any variables to current or default settings

**/
VOID
EFIAPI
InitializeSettings (
  )
{
  EFI_STATUS                  Status;
  VOID                        *AcpiBase;
  NVIDIA_KERNEL_COMMAND_LINE  CmdLine;
  UINTN                       KernelCmdLineLen;
  NVIDIA_OS_REDUNDANCY        RedundancyLevel;
  UINTN                       BufferSize;

  // Initialize PCIe Form Settings
  PcdSet8S (PcdPcieResourceConfigNeeded, PcdGet8 (PcdPcieResourceConfigNeeded));
  PcdSet8S (PcdPcieEntryInAcpiConfigNeeded, PcdGet8 (PcdPcieEntryInAcpiConfigNeeded));
  PcdSet8S (PcdPcieEntryInAcpi, PcdGet8 (PcdPcieEntryInAcpi));
  if (PcdGet8 (PcdPcieResourceConfigNeeded) == 1) {
    Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
    if (EFI_ERROR (Status)) {
      PcdSet8S (PcdPcieResourceConfigNeeded, 0);
      PcdSet8S (PcdPcieEntryInAcpiConfigNeeded, 0);
    }
  }

  // Initialize Quick Boot Form Settings
  PcdSet8S (PcdQuickBootEnabled, PcdGet8 (PcdQuickBootEnabled));

  // Initialize New Device Hierarchy Form Settings
  PcdSet8S (PcdNewDeviceHierarchy, PcdGet8 (PcdNewDeviceHierarchy));

  // Initialize OS Chain A status Form Settings
  PcdSet32S (PcdOsChainStatusA, PcdGet32 (PcdOsChainStatusA));

  // Initialize OS Chain B status Form Settings
  PcdSet32S (PcdOsChainStatusB, PcdGet32 (PcdOsChainStatusB));

  // Initialize L4T boot mode form settings
  PcdSet32S (PcdL4TDefaultBootMode, PcdGet32 (PcdL4TDefaultBootMode));

  // Initialize Kernel Command Line Form Setting
  KernelCmdLineLen = 0;
  Status           = gRT->GetVariable (L"KernelCommandLine", &gNVIDIAPublicVariableGuid, NULL, &KernelCmdLineLen, NULL);
  if (Status == EFI_NOT_FOUND) {
    KernelCmdLineLen = 0;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "%a: Error Requesting command line variable %r\r\n", __FUNCTION__, Status));
    KernelCmdLineLen = 0;
  }

  if (KernelCmdLineLen < sizeof (CmdLine)) {
    KernelCmdLineLen = sizeof (CmdLine);
    ZeroMem (&CmdLine, KernelCmdLineLen);
    Status = gRT->SetVariable (L"KernelCommandLine", &gNVIDIAPublicVariableGuid, EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS, KernelCmdLineLen, (VOID *)&CmdLine);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error setting command line variable %r\r\n", __FUNCTION__, Status));
    }
  }

  BufferSize = sizeof (RedundancyLevel);
  Status     = gRT->GetVariable (L"RootfsRedundancyLevel", &gNVIDIAPublicVariableGuid, NULL, &BufferSize, &RedundancyLevel);
  if (EFI_ERROR (Status)) {
    RedundancyLevel.Level = 0;
    BufferSize            = sizeof (RedundancyLevel);
    Status                = gRT->SetVariable (
                                   L"RootfsRedundancyLevel",
                                   &gNVIDIAPublicVariableGuid,
                                   EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS,
                                   BufferSize,
                                   &RedundancyLevel
                                   );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error setting RootfsRedundancyLevel variable %r\r\n", __FUNCTION__, Status));
    }
  }

  mHiiControlSettings.L4TSupported = PcdGetBool (PcdL4TConfigurationSupport);
}

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.

  @param[in]  This           Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Request        A null-terminated Unicode string in
                             <ConfigRequest> format.
  @param[out] Progress       On return, points to a character in the Request
                             string. Points to the string's null terminator if
                             request was successful. Points to the most recent
                             '&' before the first failing name/value pair (or
                             the beginning of the string if the failure is in
                             the first name/value pair) if the request was not
                             successful.
  @param[out] Results        A null-terminated Unicode string in
                             <ConfigAltResp> format which has all values filled
                             in for the names in the Request string. String to
                             be allocated by the called function.

  @retval EFI_SUCCESS             The Results is filled with the requested
                                  values.
  @retval EFI_OUT_OF_RESOURCES    Not enough memory to store the results.
  @retval EFI_INVALID_PARAMETER   Request is illegal syntax, or unknown name.
  @retval EFI_NOT_FOUND           Routing data doesn't match any storage in
                                  this driver.

**/
EFI_STATUS
EFIAPI
ConfigExtractConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN CONST EFI_STRING                      Request,
  OUT EFI_STRING                           *Progress,
  OUT EFI_STRING                           *Results
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;
  EFI_STRING  ConfigRequestHdr;
  EFI_STRING  ConfigRequest;
  BOOLEAN     AllocatedRequest;
  UINTN       Size;

  if ((This == NULL) || (Progress == NULL) || (Results == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Request;
  if ((Request != NULL) && !HiiIsConfigHdrMatch (Request, &gNVIDIAResourceConfigFormsetGuid, mHiiControlStorageName)) {
    return EFI_NOT_FOUND;
  }

  ConfigRequestHdr = NULL;
  ConfigRequest    = NULL;
  AllocatedRequest = FALSE;
  Size             = 0;

  //
  // Convert buffer data to <ConfigResp> by helper function BlockToConfig().
  //
  BufferSize    = sizeof (NVIDIA_CONFIG_HII_CONTROL);
  ConfigRequest = Request;
  if ((Request == NULL) || (StrStr (Request, L"OFFSET") == NULL)) {
    //
    // Request has no request element, construct full request string.
    // Allocate and fill a buffer large enough to hold the <ConfigHdr> template
    // followed by "&OFFSET=0&WIDTH=WWWWWWWWWWWWWWWW" followed by a Null-terminator
    //
    ConfigRequestHdr = HiiConstructConfigHdr (&gNVIDIAResourceConfigFormsetGuid, mHiiControlStorageName, mDriverHandle);
    if (ConfigRequestHdr == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Size          = (StrLen (ConfigRequestHdr) + 32 + 1) * sizeof (CHAR16);
    ConfigRequest = AllocateZeroPool (Size);
    if (ConfigRequest == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    AllocatedRequest = TRUE;
    UnicodeSPrint (ConfigRequest, Size, L"%s&OFFSET=0&WIDTH=%016LX", ConfigRequestHdr, (UINT64)BufferSize);
    FreePool (ConfigRequestHdr);
  }

  Status = gHiiConfigRouting->BlockToConfig (
                                gHiiConfigRouting,
                                ConfigRequest,
                                (UINT8 *)&mHiiControlSettings,
                                BufferSize,
                                Results,
                                Progress
                                );
  //
  // Free the allocated config request string.
  //
  if (AllocatedRequest) {
    FreePool (ConfigRequest);
    ConfigRequest = NULL;
  }

  //
  // Set Progress string to the original request string.
  //
  if (Request == NULL) {
    *Progress = NULL;
  } else if (StrStr (Request, L"OFFSET") == NULL) {
    *Progress = Request + StrLen (Request);
  }

  return Status;
}

/**
  This function processes the results of changes in configuration.

  @param[in]  This           Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Configuration  A null-terminated Unicode string in <ConfigResp>
                             format.
  @param[out] Progress       A pointer to a string filled in with the offset of
                             the most recent '&' before the first failing
                             name/value pair (or the beginning of the string if
                             the failure is in the first name/value pair) or
                             the terminating NULL if all was successful.

  @retval EFI_SUCCESS             The Results is processed successfully.
  @retval EFI_INVALID_PARAMETER   Configuration is NULL.
  @retval EFI_NOT_FOUND           Routing data doesn't match any storage in
                                  this driver.

**/
EFI_STATUS
EFIAPI
ConfigRouteConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN CONST EFI_STRING                      Configuration,
  OUT EFI_STRING                           *Progress
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;

  Status = EFI_SUCCESS;

  if ((This == NULL) || (Configuration == NULL) || (Progress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check routing data in <ConfigHdr>.
  // Note: if only one Storage is used, then this checking could be skipped.
  //
  if (!HiiIsConfigHdrMatch (Configuration, &gNVIDIAResourceConfigFormsetGuid, mHiiControlStorageName)) {
    *Progress = Configuration;
    return EFI_NOT_FOUND;
  }

  //
  // Convert <ConfigResp> to buffer data by helper function ConfigToBlock().
  //
  BufferSize = sizeof (NVIDIA_CONFIG_HII_CONTROL);
  Status     = gHiiConfigRouting->ConfigToBlock (
                                    gHiiConfigRouting,
                                    Configuration,
                                    (UINT8 *)&mHiiControlSettings,
                                    &BufferSize,
                                    Progress
                                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

/**
  This function processes the results of changes in configuration.

  @param[in]  This           Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Action         Specifies the type of action taken by the browser.
  @param[in]  QuestionId     A unique value which is sent to the original
                             exporting driver so that it can identify the type
                             of data to expect.
  @param[in]  Type           The type of value for the question.
  @param[in]  Value          A pointer to the data being sent to the original
                             exporting driver.
  @param[out] ActionRequest  On return, points to the action requested by the
                             callback function.

  @retval EFI_SUCCESS             The callback successfully handled the action.
  @retval EFI_OUT_OF_RESOURCES    Not enough storage is available to hold the
                                  variable and its data.
  @retval EFI_DEVICE_ERROR        The variable could not be saved.
  @retval EFI_UNSUPPORTED         The specified Action is not supported by the
                                  callback.

**/
EFI_STATUS
EFIAPI
ConfigCallback (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN     EFI_BROWSER_ACTION                Action,
  IN     EFI_QUESTION_ID                   QuestionId,
  IN     UINT8                             Type,
  IN     EFI_IFR_TYPE_VALUE                *Value,
  OUT EFI_BROWSER_ACTION_REQUEST           *ActionRequest
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  VarDeleteStatus;
  CHAR16      *CurrentName;
  CHAR16      *NextName;
  EFI_GUID    CurrentGuid;
  EFI_GUID    NextGuid;
  UINTN       NameSize;

  Status = EFI_UNSUPPORTED;

  if (Action == EFI_BROWSER_ACTION_CHANGED) {
    switch (QuestionId) {
      case KEY_RESET_VARIABLES:
        CurrentName = AllocateZeroPool (MAX_VARIABLE_NAME);
        if (CurrentName == NULL) {
          Status = EFI_OUT_OF_RESOURCES;
          break;
        }

        NextName = AllocateZeroPool (MAX_VARIABLE_NAME);
        if (NextName == NULL) {
          Status = EFI_OUT_OF_RESOURCES;
          FreePool (CurrentName);
          break;
        }

        NameSize = MAX_VARIABLE_NAME;
        Status   = gRT->GetNextVariableName (&NameSize, NextName, &NextGuid);

        while (!EFI_ERROR (Status)) {
          CopyMem (CurrentName, NextName, NameSize);
          CopyGuid (&CurrentGuid, &NextGuid);

          NameSize = MAX_VARIABLE_NAME;
          Status   = gRT->GetNextVariableName (&NameSize, NextName, &NextGuid);

          // Delete Current Name variable
          VarDeleteStatus = gRT->SetVariable (
                                   CurrentName,
                                   &CurrentGuid,
                                   0,
                                   0,
                                   NULL
                                   );
          DEBUG ((DEBUG_ERROR, "Delete Variable %g:%s %r\r\n", &CurrentGuid, CurrentName, VarDeleteStatus));
        }

        FreePool (NextName);
        FreePool (CurrentName);
        Status = EFI_SUCCESS;

        break;

      default:
        break;
    }
  }

  return Status;
}

VOID
EFIAPI
OnEndOfDxe (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS      Status;
  EFI_HII_HANDLE  HiiHandle;

  gBS->CloseEvent (Event);

  InitializeSettings ();

  mConfigAccess.Callback      = ConfigCallback;
  mConfigAccess.ExtractConfig = ConfigExtractConfig;
  mConfigAccess.RouteConfig   = ConfigRouteConfig;

  mDriverHandle = NULL;
  Status        = gBS->InstallMultipleProtocolInterfaces (
                         &mDriverHandle,
                         &gEfiDevicePathProtocolGuid,
                         &mNvidiaConfigHiiVendorDevicePath,
                         &gEfiHiiConfigAccessProtocolGuid,
                         &mConfigAccess,
                         NULL
                         );
  if (!EFI_ERROR (Status)) {
    HiiHandle = HiiAddPackages (
                  &gNVIDIAResourceConfigFormsetGuid,
                  mDriverHandle,
                  NvidiaConfigDxeStrings,
                  NvidiaConfigHiiBin,
                  NULL
                  );

    if (HiiHandle == NULL) {
      gBS->UninstallMultipleProtocolInterfaces (
             mDriverHandle,
             &gEfiDevicePathProtocolGuid,
             &mNvidiaConfigHiiVendorDevicePath,
             &gEfiHiiConfigAccessProtocolGuid,
             &mConfigAccess,
             NULL
             );
    }
  }
}

/**
  Update Serial Port PCDs.
**/
STATIC
EFI_STATUS
UpdateSerialPcds (
  VOID
  )
{
  UINT32      NumSbsaUartControllers;
  EFI_STATUS  Status;
  UINT8       DefaultPortConfig;
  UINTN       SerialPortVarLen;

  NumSbsaUartControllers = 0;

  // Obtain SBSA Handle Info
  Status = GetMatchingEnabledDeviceTreeNodes ("arm,sbsa-uart", NULL, &NumSbsaUartControllers);
  if (Status == EFI_NOT_FOUND) {
    PcdSet8S (PcdSerialTypeConfig, NVIDIA_SERIAL_PORT_TYPE_16550);
    DefaultPortConfig = NVIDIA_SERIAL_PORT_SPCR_FULL_16550;
  } else {
    PcdSet8S (PcdSerialTypeConfig, NVIDIA_SERIAL_PORT_TYPE_SBSA);
    DefaultPortConfig = NVIDIA_SERIAL_PORT_SPCR_SBSA;
  }

  SerialPortVarLen = 0;
  Status           = gRT->GetVariable (L"SerialPortConfig", &gNVIDIATokenSpaceGuid, NULL, &SerialPortVarLen, NULL);
  if (Status == EFI_NOT_FOUND) {
    PcdSet8S (PcdSerialPortConfig, DefaultPortConfig);
  }

  return EFI_SUCCESS;
}

/**
  Install NVIDIA Config driver.

  @param  ImageHandle     The image handle.
  @param  SystemTable     The system table.

  @retval EFI_SUCEESS     Install Boot manager menu success.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
NvidiaConfigDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   EndOfDxeEvent;

  UpdateSerialPcds ();

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnEndOfDxe,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &EndOfDxeEvent
                  );

  return Status;
}
