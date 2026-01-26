/** @file
  Redfish HTTP Boot Configuration DXE driver.

  This driver exposes HTTP boot URI configuration through Redfish using HII.
  It creates a single HII question for HTTP_BOOT_URI that supports flexible formats:
  - URI: Create boot option for first NIC (persistent)
  - MAC||URI: Create boot option for specific MAC (persistent)
  - MAC|once|URI: Create boot option for specific MAC (one-time)

  Uses HII varstore with manual ExtractConfig/RouteConfig implementation.
  RouteConfig creates Redfish boot option (Boot8C7D) and sets BootNext for transient boot on boot N.
  On boot N+1, firmware boots from the Redfish boot option. On boot N+2, cleanup occurs if "once" flag set.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishHttpBootConfigDxe.h"

//
// Global variables
//
EFI_HII_HANDLE                   mHiiHandle         = NULL;
EFI_HANDLE                       mDriverHandle      = NULL;
EFI_HII_CONFIG_ROUTING_PROTOCOL  *mHiiConfigRouting = NULL;
HTTP_BOOT_URI_STORAGE            mHttpBootUriConfig;   // In-memory HII config
CHAR16                           mHttpBootUriVarName[] = L"HttpBootUri";

//
// HII Config Access Protocol instance
//
GLOBAL_REMOVE_IF_UNREFERENCED
HII_VENDOR_DEVICE_PATH  mHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    REDFISH_HTTP_BOOT_CONFIG_FORMSET_GUID
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

REDFISH_HTTP_BOOT_CONFIG_PRIVATE_DATA  mPrivateData = {
  .Signature    = REDFISH_HTTP_BOOT_CONFIG_SIGNATURE,
  .ConfigAccess = {
    .ExtractConfig = HttpBootConfigExtractConfig,
    .RouteConfig   = HttpBootConfigRouteConfig,
    .Callback      = HttpBootConfigCallback
  }
};

/**
  RouteConfig callback - called when Redfish pushes configuration.

  Parses the config string, writes the variable, and creates boot options.

  @param[in]  This           Config Access Protocol
  @param[in]  Configuration  Configuration string
  @param[out] Progress       Progress pointer

  @retval EFI_SUCCESS        Configuration processed
  @retval Others             Error
**/
EFI_STATUS
EFIAPI
HttpBootConfigRouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Configuration,
  OUT EFI_STRING                            *Progress
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;

  if ((Configuration == NULL) || (Progress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Configuration;

  //
  // Check if this configuration is for our varstore
  //
  if (!HiiIsConfigHdrMatch (Configuration, &gNvidiaHttpBootConfigGuid, mHttpBootUriVarName)) {
    DEBUG ((DEBUG_ERROR, "%a: Configuration GUID/Name mismatch\n", __func__));
    return EFI_NOT_FOUND;
  }

  //
  // Parse config string into in-memory structure using HII Config Routing Protocol
  //
  BufferSize = sizeof (mHttpBootUriConfig);
  Status     = mHiiConfigRouting->ConfigToBlock (
                                    mHiiConfigRouting,
                                    Configuration,
                                    (UINT8 *)&mHttpBootUriConfig,
                                    &BufferSize,
                                    Progress
                                    );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ConfigToBlock failed: %r\n", __func__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Parsed config, HttpBootUri = %s\n", __func__, mHttpBootUriConfig.HttpBootUri));

  //
  // Handle empty URI - delete variable to trigger cleanup
  //
  if (StrLen (mHttpBootUriConfig.HttpBootUri) == 0) {
    DEBUG ((DEBUG_INFO, "%a: Empty URI, deleting variable to trigger cleanup\n", __func__));

    Status = gRT->SetVariable (
                    mHttpBootUriVarName,
                    &gNvidiaHttpBootConfigGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    0,
                    NULL
                    );

    if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to delete variable: %r\n", __func__, Status));
    } else {
      DEBUG ((DEBUG_INFO, "%a: Variable deleted successfully\n", __func__));
    }

    *Progress = Configuration + StrLen (Configuration);
    return EFI_SUCCESS;
  }

  //
  // Parse and validate the URI string BEFORE writing the variable
  // This prevents persisting invalid configuration
  //
  EFI_MAC_ADDRESS  MacAddr;
  BOOLEAN          Once;
  CHAR16           *Uri;
  UINT16           BootOptionNum;

  Status = ParseHttpBootUri (mHttpBootUriConfig.HttpBootUri, &MacAddr, &Once, &Uri);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to parse URI: %r\n", __func__, Status));
    *Progress = Configuration + StrLen (Configuration);
    return Status;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: Parsed - MAC=%02X:%02X:%02X:%02X:%02X:%02X, Once=%d, URI=%s\n",
    __func__,
    MacAddr.Addr[0],
    MacAddr.Addr[1],
    MacAddr.Addr[2],
    MacAddr.Addr[3],
    MacAddr.Addr[4],
    MacAddr.Addr[5],
    Once,
    Uri
    ));

  //
  // Write the variable (parsing succeeded)
  //
  Status = gRT->SetVariable (
                  mHttpBootUriVarName,
                  &gNvidiaHttpBootConfigGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  sizeof (mHttpBootUriConfig),
                  &mHttpBootUriConfig
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to write variable: %r\n", __func__, Status));
    *Progress = Configuration + StrLen (Configuration);
    return Status;
  }

  //
  // Create boot option and set BootNext
  // CRITICAL: We must set BootNext HERE (boot 2) so that BdsDxe sees it on boot 3.
  // BdsDxe caches BootNext early in the boot process, before our entry point runs,
  // so setting it in the entry point would be too late.
  //
  Status = CreateHttpBootOption (&MacAddr, Uri, &BootOptionNum);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create boot option: %r\n", __func__, Status));

    //
    // Delete the variable we just wrote (cleanup inconsistent state)
    //
    gRT->SetVariable (
           mHttpBootUriVarName,
           &gNvidiaHttpBootConfigGuid,
           EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
           0,
           NULL
           );

    *Progress = Configuration + StrLen (Configuration);
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Created Boot%04x and set BootNext\n", __func__, BootOptionNum));

  //
  // Flow:
  // 1. Boot N (here): RouteConfig creates Redfish boot option and sets BootNext → reboot
  // 2. Boot N+1: BdsDxe reads BootNext → boots from Redfish boot option
  // 3. Boot N+2: CompareAndSyncBootOptions cleans up if "once" flag was set
  //

  *Progress = Configuration + StrLen (Configuration);
  return EFI_SUCCESS;
}

/**
  ExtractConfig callback - called when Redfish reads configuration.

  Converts the in-memory HII config to a config string.

  @param[in]  This           Config Access Protocol
  @param[in]  Request        Request string
  @param[out] Progress       Progress pointer
  @param[out] Results        Results string

  @retval EFI_SUCCESS        Configuration extracted
  @retval Others             Error
**/
EFI_STATUS
EFIAPI
HttpBootConfigExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Request,
  OUT EFI_STRING                            *Progress,
  OUT EFI_STRING                            *Results
  )
{
  EFI_STATUS  Status;

  if ((Progress == NULL) || (Results == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Request;
  *Results  = NULL;

  //
  // Handle NULL Request - export all configuration
  //
  EFI_STRING  ConfigRequest    = NULL;
  EFI_STRING  ConfigRequestHdr = NULL;
  BOOLEAN     AllocatedRequest = FALSE;

  if (Request == NULL) {
    //
    // Request is NULL - construct full config header to export everything
    //
    ConfigRequestHdr = HiiConstructConfigHdr (
                         &gNvidiaHttpBootConfigGuid,
                         mHttpBootUriVarName,
                         mDriverHandle
                         );
    if (ConfigRequestHdr == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    ConfigRequest    = ConfigRequestHdr;
    AllocatedRequest = TRUE;
  } else {
    //
    // Check if this request is for our varstore
    //
    if (!HiiIsConfigHdrMatch (Request, &gNvidiaHttpBootConfigGuid, mHttpBootUriVarName)) {
      return EFI_NOT_FOUND;
    }

    ConfigRequest = (EFI_STRING)Request;
  }

  //
  // Read current variable value into in-memory config
  //
  UINTN  VarSize = sizeof (mHttpBootUriConfig);

  Status = gRT->GetVariable (
                  mHttpBootUriVarName,
                  &gNvidiaHttpBootConfigGuid,
                  NULL,
                  &VarSize,
                  &mHttpBootUriConfig
                  );
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read variable: %r\n", __func__, Status));
    if (AllocatedRequest) {
      FreePool (ConfigRequestHdr);
    }

    return Status;
  }

  if (Status == EFI_NOT_FOUND) {
    ZeroMem (&mHttpBootUriConfig, sizeof (mHttpBootUriConfig));
  }

  //
  // Convert block to config string using HII Config Routing Protocol
  //
  Status = mHiiConfigRouting->BlockToConfig (
                                mHiiConfigRouting,
                                ConfigRequest,
                                (UINT8 *)&mHttpBootUriConfig,
                                sizeof (mHttpBootUriConfig),
                                Results,
                                Progress
                                );

  //
  // Free the allocated config header if we created one
  //
  if (AllocatedRequest) {
    FreePool (ConfigRequestHdr);
  }

  return Status;
}

/**
  Callback handler for HII Config Access Protocol.

  Not used for Redfish (Redfish goes through RouteConfig).

  @param[in]     This           Config Access Protocol
  @param[in]     Action         Action type
  @param[in]     QuestionId     Question ID
  @param[in]     Type           Value type
  @param[in,out] Value          Value
  @param[out]    ActionRequest  Action request

  @retval EFI_SUCCESS           Callback handled
  @retval EFI_UNSUPPORTED       Action not supported
**/
EFI_STATUS
EFIAPI
HttpBootConfigCallback (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  EFI_BROWSER_ACTION                    Action,
  IN  EFI_QUESTION_ID                       QuestionId,
  IN  UINT8                                 Type,
  IN  OUT EFI_IFR_TYPE_VALUE                *Value,
  OUT EFI_BROWSER_ACTION_REQUEST            *ActionRequest
  )
{
  // Callbacks are not used for Redfish configuration
  return EFI_UNSUPPORTED;
}

/**
  Driver entry point.

  @param[in] ImageHandle  Image handle
  @param[in] SystemTable  System table

  @retval EFI_SUCCESS     Driver loaded successfully
  @retval Others          Error occurred
**/
EFI_STATUS
EFIAPI
RedfishHttpBootConfigEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  mDriverHandle = NULL;

  // Locate HII Config Routing Protocol
  Status = gBS->LocateProtocol (
                  &gEfiHiiConfigRoutingProtocolGuid,
                  NULL,
                  (VOID **)&mHiiConfigRouting
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate HII Config Routing Protocol: %r\n", __func__, Status));
    return Status;
  }

  // Install Config Access Protocol on new handle
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mDriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &mPrivateData.ConfigAccess,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install protocols: %r\n", __func__, Status));
    return Status;
  }

  // Install HII packages
  mHiiHandle = HiiAddPackages (
                 &gRedfishHttpBootConfigFormsetGuid,
                 mDriverHandle,
                 RedfishHttpBootConfigDxeStrings,
                 RedfishHttpBootConfigVfrBin,
                 NULL
                 );
  if (mHiiHandle == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to add HII packages\n", __func__));
    gBS->UninstallMultipleProtocolInterfaces (
           mDriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mHiiVendorDevicePath,
           &gEfiHiiConfigAccessProtocolGuid,
           &mPrivateData.ConfigAccess,
           NULL
           );
    return EFI_OUT_OF_RESOURCES;
  }

  // Compare and sync boot options with HttpBootUri variable
  Status = CompareAndSyncBootOptions ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Failed to sync boot options: %r\n", __func__, Status));
  }

  DEBUG ((DEBUG_INFO, "%a: Driver loaded successfully\n", __func__));

  return EFI_SUCCESS;
}
