/** @file
  Redfish BMC Reset Driver

  This driver provides the ability to reset BMC to factory defaults via Redfish.
  It uses an immediate reset approach with fresh REST EX connections.

  SECURITY: Credentials are passed directly at reset time and are never stored
  persistently. They exist only in memory during the reset operation.

  Flow:
  1. During Init(), driver caches:
     - Controller handle from original REST EX handle
     - BMC location (IP address)
     - Host IP configuration from SMBIOS Type 42
  2. User triggers reset from menu -> caller prompts for credentials
  3. Credentials are passed directly to FactoryReset()
  4. Driver creates a FRESH REST EX child with the credentials
  5. Sends POST request to /redfish/v1/Managers/BMC_0/Actions/Manager.ResetToDefaults
  6. BMC resets - console connection will be lost

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/RedfishLib.h>
#include <Library/NetLib.h>

#include <Protocol/EdkIIRedfishConfigHandler.h>
#include <Protocol/RestEx.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/Http.h>
#include <Protocol/Smbios.h>
#include <IndustryStandard/SmBios.h>
#include <IndustryStandard/RedfishHostInterface.h>

#include <Protocol/BmcReset.h>

//
// Module globals
//
STATIC NVIDIA_BMC_RESET_PROTOCOL              mBmcResetProtocol;
STATIC EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL  mConfigHandler;
STATIC EFI_HANDLE                             mDriverHandle      = NULL;
STATIC EFI_HANDLE                             mImageHandle       = NULL;
STATIC BOOLEAN                                mProtocolInstalled = FALSE;

//
// Cached data from Init() - used to create fresh REST EX connections
//
STATIC EFI_HANDLE  mCachedController   = NULL;   // NIC controller handle
STATIC CHAR16      *mCachedBmcLocation = NULL;   // Deep-copied BMC IP/hostname
STATIC BOOLEAN     mCachedUseHttps     = FALSE;

//
// Cached HTTP configuration from original REST EX (for network interface binding)
//
STATIC EFI_REST_EX_HTTP_CONFIG_DATA  *mCachedHttpConfig = NULL;

//
// BMC Manager Reset URI (ASCII for RedfishLib)
//
#define BMC_RESET_TO_DEFAULTS_URI  "/redfish/v1/Managers/BMC_0/Actions/Manager.ResetToDefaults"

//
// Reset payload
//
#define BMC_RESET_PAYLOAD  "{\"ResetToDefaultsType\": \"ResetAll\"}"

//
// Content-Type for JSON payload
//
#define BMC_RESET_CONTENT_TYPE  "application/json"

//
// HTTP timeout in milliseconds
//
#define BMC_RESET_HTTP_TIMEOUT_MS  5000

/**
  Get host IP address from SMBIOS Type 42 (Redfish Host Interface).

  This function parses SMBIOS Type 42 to find the host IP address used for
  Redfish communication. The IP configuration is stored in mCachedHttpConfig.

  @retval EFI_SUCCESS       Host IP successfully retrieved and cached.
  @retval EFI_NOT_FOUND     SMBIOS Type 42 not found or no valid IP.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
  @retval Others            Error occurred.
**/
STATIC
EFI_STATUS
GetHostIpFromSmbiosType42 (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_SMBIOS_PROTOCOL            *Smbios;
  EFI_SMBIOS_HANDLE              SmbiosHandle;
  EFI_SMBIOS_TABLE_HEADER        *Record;
  SMBIOS_TABLE_TYPE42            *Type42Record;
  UINT8                          *DataPtr;
  UINT8                          DeviceDescLen;
  UINT8                          ProtocolCount;
  UINT8                          ProtocolType;
  UINT8                          ProtocolLen;
  REDFISH_OVER_IP_PROTOCOL_DATA  *ProtocolData;

  DEBUG ((DEBUG_INFO, "%a: Trying SMBIOS Type 42 for Host IP\n", __func__));

  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Could not locate SMBIOS protocol: %r\n", __func__, Status));
    return Status;
  }

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status       = Smbios->GetNext (Smbios, &SmbiosHandle, NULL, &Record, NULL);

  while (!EFI_ERROR (Status) && (SmbiosHandle != SMBIOS_HANDLE_PI_RESERVED)) {
    if (Record->Type == SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE) {
      Type42Record = (SMBIOS_TABLE_TYPE42 *)Record;

      //
      // Check for Network Host Interface type (0x40)
      //
      if (Type42Record->InterfaceType == MCHostInterfaceTypeNetworkHostInterface) {
        //
        // SMBIOS Type 42 structure layout:
        // - Hdr (4 bytes)
        // - InterfaceType (1 byte)
        // - InterfaceTypeSpecificDataLength (1 byte) = DeviceDescLen
        // - InterfaceTypeSpecificData[DeviceDescLen] = Device Descriptor
        // - ProtocolCount (1 byte)
        // - Protocol Records...
        //
        DeviceDescLen = Type42Record->InterfaceTypeSpecificDataLength;

        //
        // Calculate offset to Protocol Count field:
        // OFFSET_OF gets us to InterfaceTypeSpecificData[], then add DeviceDescLen
        //
        DataPtr = (UINT8 *)Type42Record +
                  OFFSET_OF (SMBIOS_TABLE_TYPE42, InterfaceTypeSpecificData) +
                  DeviceDescLen;
        ProtocolCount = *DataPtr;
        DataPtr++;

        DEBUG ((
          DEBUG_INFO,
          "%a: SMBIOS Type 42 - DeviceDescLen=%u, ProtocolCount=%u\n",
          __func__,
          DeviceDescLen,
          ProtocolCount
          ));

        //
        // Look for Redfish over IP protocol (type 0x04)
        //
        while (ProtocolCount > 0) {
          ProtocolType = DataPtr[0];
          ProtocolLen  = DataPtr[1];

          DEBUG ((DEBUG_INFO, "%a: Protocol Type=0x%02x, Len=%u\n", __func__, ProtocolType, ProtocolLen));

          if (ProtocolType == MCHostInterfaceProtocolTypeRedfishOverIP) {
            ProtocolData = (REDFISH_OVER_IP_PROTOCOL_DATA *)(DataPtr + 2);

            DEBUG ((
              DEBUG_INFO,
              "%a: SMBIOS Type 42 - HostIP: %d.%d.%d.%d, Mask: %d.%d.%d.%d\n",
              __func__,
              ProtocolData->HostIpAddress[0],
              ProtocolData->HostIpAddress[1],
              ProtocolData->HostIpAddress[2],
              ProtocolData->HostIpAddress[3],
              ProtocolData->HostIpMask[0],
              ProtocolData->HostIpMask[1],
              ProtocolData->HostIpMask[2],
              ProtocolData->HostIpMask[3]
              ));

            //
            // Check if Host IP is valid (non-zero)
            //
            if ((ProtocolData->HostIpAddress[0] != 0) ||
                (ProtocolData->HostIpAddress[1] != 0) ||
                (ProtocolData->HostIpAddress[2] != 0) ||
                (ProtocolData->HostIpAddress[3] != 0))
            {
              //
              // Create HTTP config with this IP
              //
              mCachedHttpConfig = AllocateZeroPool (sizeof (EFI_REST_EX_HTTP_CONFIG_DATA));
              if (mCachedHttpConfig == NULL) {
                DEBUG ((DEBUG_ERROR, "%a: Failed to allocate HTTP config\n", __func__));
                return EFI_OUT_OF_RESOURCES;
              }

              mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node = AllocateZeroPool (sizeof (EFI_HTTPv4_ACCESS_POINT));
              if (mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node == NULL) {
                DEBUG ((DEBUG_ERROR, "%a: Failed to allocate IPv4Node\n", __func__));
                //
                // Clean up the already allocated HTTP config to avoid partial state
                //
                FreePool (mCachedHttpConfig);
                mCachedHttpConfig = NULL;
                return EFI_OUT_OF_RESOURCES;
              }

              mCachedHttpConfig->SendReceiveTimeout                                     = BMC_RESET_HTTP_TIMEOUT_MS;
              mCachedHttpConfig->HttpConfigData.HttpVersion                             = HttpVersion11;
              mCachedHttpConfig->HttpConfigData.LocalAddressIsIPv6                      = FALSE;
              mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node->UseDefaultAddress = FALSE;
              CopyMem (
                &mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node->LocalAddress,
                ProtocolData->HostIpAddress,
                sizeof (EFI_IPv4_ADDRESS)
                );
              CopyMem (
                &mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node->LocalSubnet,
                ProtocolData->HostIpMask,
                sizeof (EFI_IPv4_ADDRESS)
                );
              DEBUG ((
                DEBUG_INFO,
                "%a: Using SMBIOS Type 42 - LocalIP: %d.%d.%d.%d\n",
                __func__,
                ProtocolData->HostIpAddress[0],
                ProtocolData->HostIpAddress[1],
                ProtocolData->HostIpAddress[2],
                ProtocolData->HostIpAddress[3]
                ));

              return EFI_SUCCESS;
            }

            break;  // Found Redfish protocol, done
          }

          DataPtr += 2 + ProtocolLen;
          ProtocolCount--;
        }
      }

      break;  // Found Type 42, done
    }

    Status = Smbios->GetNext (Smbios, &SmbiosHandle, NULL, &Record, NULL);
  }

  DEBUG ((DEBUG_WARN, "%a: No valid Host IP found in SMBIOS Type 42\n", __func__));
  return EFI_NOT_FOUND;
}

/**
  Execute the actual BMC factory reset via Redfish.
  Creates a fresh REST EX connection using the provided credentials.

  @param[in]  Username  BMC username (ASCII, null-terminated).
  @param[in]  Password  BMC password (ASCII, null-terminated).

  @retval EFI_SUCCESS             Reset command sent successfully (or BMC is resetting).
  @retval EFI_NOT_READY           Service not available.
  @retval EFI_INVALID_PARAMETER   Username or Password is NULL.
  @retval Others                  Error occurred.
**/
STATIC
EFI_STATUS
ExecuteBmcFactoryReset (
  IN CONST CHAR8  *Username,
  IN CONST CHAR8  *Password
  )
{
  EFI_STATUS                          Status;
  REDFISH_SERVICE                     RedfishService;
  REDFISH_RESPONSE                    Response;
  EFI_HANDLE                          FreshRestExHandle;
  REDFISH_CONFIG_SERVICE_INFORMATION  FreshServiceInfo;
  EFI_REST_EX_PROTOCOL                *RestEx;
  EFI_REST_EX_HTTP_CONFIG_DATA        *RestExHttpConfigData;

  DEBUG ((DEBUG_INFO, "%a: Executing BMC factory reset\n", __func__));

  //
  // Validate parameters
  //
  if ((Username == NULL) || (Password == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid credentials (NULL)\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Validate cached data
  //
  if ((mCachedController == NULL) || (mCachedBmcLocation == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: No cached service info\n", __func__));
    return EFI_NOT_READY;
  }

  RedfishService       = NULL;
  FreshRestExHandle    = NULL;
  RestEx               = NULL;
  RestExHttpConfigData = NULL;

  ZeroMem (&Response, sizeof (Response));
  ZeroMem (&FreshServiceInfo, sizeof (FreshServiceInfo));

  //
  // Create fresh REST EX child on the cached controller
  //
  DEBUG ((DEBUG_INFO, "%a: Creating fresh REST EX child on controller %p\n", __func__, mCachedController));

  Status = NetLibCreateServiceChild (
             mCachedController,
             mImageHandle,
             &gEfiRestExServiceBindingProtocolGuid,
             &FreshRestExHandle
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create REST EX child: %r\n", __func__, Status));
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "%a: Created fresh REST EX handle: %p\n", __func__, FreshRestExHandle));

  //
  // Get the REST EX protocol from the fresh handle
  //
  Status = gBS->OpenProtocol (
                  FreshRestExHandle,
                  &gEfiRestExProtocolGuid,
                  (VOID **)&RestEx,
                  mImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get REST EX protocol: %r\n", __func__, Status));
    goto Cleanup;
  }

  //
  // Configure the REST EX handle with HTTP settings
  // This is required before the handle can be used for HTTP requests
  //
  // Use cached config if available (from original REST EX), otherwise create new
  //
  if (mCachedHttpConfig != NULL) {
    //
    // Deep copy the cached config
    //
    RestExHttpConfigData = AllocateZeroPool (sizeof (EFI_REST_EX_HTTP_CONFIG_DATA));
    if (RestExHttpConfigData == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate REST EX config data\n", __func__));
      Status = EFI_OUT_OF_RESOURCES;
      goto Cleanup;
    }

    CopyMem (RestExHttpConfigData, mCachedHttpConfig, sizeof (EFI_REST_EX_HTTP_CONFIG_DATA));

    //
    // Deep copy the access point from cached config
    //
    if (!mCachedHttpConfig->HttpConfigData.LocalAddressIsIPv6) {
      if (mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node != NULL) {
        RestExHttpConfigData->HttpConfigData.AccessPoint.IPv4Node = AllocateCopyPool (
                                                                      sizeof (EFI_HTTPv4_ACCESS_POINT),
                                                                      mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node
                                                                      );
        if (RestExHttpConfigData->HttpConfigData.AccessPoint.IPv4Node == NULL) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to copy IPv4 access point\n", __func__));
          Status = EFI_OUT_OF_RESOURCES;
          goto Cleanup;
        }

        DEBUG ((
          DEBUG_ERROR,
          "%a: Using cached HTTP config - LocalIP: %d.%d.%d.%d\n",
          __func__,
          RestExHttpConfigData->HttpConfigData.AccessPoint.IPv4Node->LocalAddress.Addr[0],
          RestExHttpConfigData->HttpConfigData.AccessPoint.IPv4Node->LocalAddress.Addr[1],
          RestExHttpConfigData->HttpConfigData.AccessPoint.IPv4Node->LocalAddress.Addr[2],
          RestExHttpConfigData->HttpConfigData.AccessPoint.IPv4Node->LocalAddress.Addr[3]
          ));
      }
    } else {
      if (mCachedHttpConfig->HttpConfigData.AccessPoint.IPv6Node != NULL) {
        RestExHttpConfigData->HttpConfigData.AccessPoint.IPv6Node = AllocateCopyPool (
                                                                      sizeof (EFI_HTTPv6_ACCESS_POINT),
                                                                      mCachedHttpConfig->HttpConfigData.AccessPoint.IPv6Node
                                                                      );
        if (RestExHttpConfigData->HttpConfigData.AccessPoint.IPv6Node == NULL) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to copy IPv6 access point\n", __func__));
          Status = EFI_OUT_OF_RESOURCES;
          goto Cleanup;
        }
      }
    }
  } else {
    //
    // No cached HTTP config means SMBIOS Type 42 didn't have valid Host IP.
    // Without the local IP address, we cannot configure the REST EX handle.
    //
    DEBUG ((DEBUG_ERROR, "%a: No cached HTTP config - SMBIOS Type 42 Host IP is required\n", __func__));
    DEBUG ((DEBUG_ERROR, "%a: Ensure SMBIOS Type 42 (Management Controller Host Interface) has valid HostIpAddress\n", __func__));
    Status = EFI_NOT_READY;
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "%a: Configuring REST EX handle\n", __func__));

  Status = RestEx->Configure (RestEx, (EFI_REST_EX_CONFIG_DATA)RestExHttpConfigData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to configure REST EX: %r\n", __func__, Status));
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "%a: REST EX configured successfully\n", __func__));

  //
  // Build fresh service info with the new REST EX handle
  //
  FreshServiceInfo.RedfishServiceRestExHandle = FreshRestExHandle;
  FreshServiceInfo.RedfishServiceLocation     = mCachedBmcLocation;
  FreshServiceInfo.RedfishServiceUseHttps     = mCachedUseHttps;

  DEBUG ((DEBUG_INFO, "%a: Creating Redfish service with user credentials\n", __func__));

  //
  // Create Redfish service with provided credentials and fresh REST EX
  // Note: Caller retains ownership of credentials and is responsible for cleanup
  //
  RedfishService = RedfishCreateServiceWithCredential (
                     &FreshServiceInfo,
                     AuthMethodHttpBasic,
                     (CHAR8 *)Username,
                     (CHAR8 *)Password
                     );

  if (RedfishService == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create Redfish service\n", __func__));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "%a: Redfish service created, sending POST to %a\n", __func__, BMC_RESET_TO_DEFAULTS_URI));
  DEBUG ((DEBUG_INFO, "%a: Payload: %a\n", __func__, BMC_RESET_PAYLOAD));

  //
  // Send the POST request using RedfishLib
  //
  Status = RedfishPostToUri (
             RedfishService,
             BMC_RESET_TO_DEFAULTS_URI,
             BMC_RESET_PAYLOAD,
             AsciiStrLen (BMC_RESET_PAYLOAD),
             BMC_RESET_CONTENT_TYPE,
             &Response
             );

  //
  // Log the HTTP status code regardless of success/failure
  // Check Status first before accessing Response data
  //
  if (!EFI_ERROR (Status) && (Response.StatusCode != NULL)) {
    DEBUG ((DEBUG_INFO, "%a: HTTP Status Code: %d\n", __func__, *Response.StatusCode));
  } else if (Response.StatusCode != NULL) {
    //
    // Status failed but we have a response code - log it for debugging
    //
    DEBUG ((DEBUG_WARN, "%a: Request failed with HTTP Status Code: %d\n", __func__, *Response.StatusCode));
  } else {
    //
    // No HTTP status code means the connection was dropped.
    // For BMC reset, this is EXPECTED - the BMC resets and drops the connection
    // before sending a response. Treat this as success.
    //
    DEBUG ((DEBUG_INFO, "%a: No HTTP Status Code - BMC resetting (connection dropped)\n", __func__));
    if (Status == EFI_DEVICE_ERROR) {
      DEBUG ((DEBUG_INFO, "%a: BMC reset in progress\n", __func__));
      Status = EFI_SUCCESS;
    }
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: POST request failed: %r\n", __func__, Status));
  } else {
    DEBUG ((DEBUG_INFO, "%a: BMC factory reset initiated successfully!\n", __func__));
  }

Cleanup:
  //
  // Clean up response
  //
  if (Response.StatusCode != NULL) {
    RedfishFreeResponse (
      Response.StatusCode,
      Response.HeaderCount,
      Response.Headers,
      Response.Payload
      );
  }

  //
  // Clean up Redfish service
  //
  if (RedfishService != NULL) {
    RedfishCleanupService (RedfishService);
  }

  //
  // Note: Credentials are owned by caller - do not free them here
  //

  //
  // Clean up REST EX config data
  //
  if (RestExHttpConfigData != NULL) {
    if (!RestExHttpConfigData->HttpConfigData.LocalAddressIsIPv6) {
      if (RestExHttpConfigData->HttpConfigData.AccessPoint.IPv4Node != NULL) {
        FreePool (RestExHttpConfigData->HttpConfigData.AccessPoint.IPv4Node);
      }
    } else {
      if (RestExHttpConfigData->HttpConfigData.AccessPoint.IPv6Node != NULL) {
        FreePool (RestExHttpConfigData->HttpConfigData.AccessPoint.IPv6Node);
      }
    }

    FreePool (RestExHttpConfigData);
  }

  //
  // Destroy the fresh REST EX child
  //
  if (FreshRestExHandle != NULL) {
    NetLibDestroyServiceChild (
      mCachedController,
      mImageHandle,
      &gEfiRestExServiceBindingProtocolGuid,
      FreshRestExHandle
      );
  }

  return Status;
}

/**
  Check if BMC reset service is available.

  @param[in]  This    Pointer to NVIDIA_BMC_RESET_PROTOCOL instance.

  @retval TRUE        Service is available.
  @retval FALSE       Service is not available.
**/
STATIC
BOOLEAN
EFIAPI
BmcResetIsAvailable (
  IN NVIDIA_BMC_RESET_PROTOCOL  *This
  )
{
  //
  // Service is available if we have cached controller and BMC location
  //
  return ((mCachedController != NULL) && (mCachedBmcLocation != NULL));
}

/**
  Request BMC factory reset with provided credentials.

  This immediately creates a fresh REST EX connection and executes
  the factory reset using the provided credentials. Credentials are
  used immediately and never stored persistently.

  @param[in]  This      Pointer to NVIDIA_BMC_RESET_PROTOCOL instance.
  @param[in]  Username  BMC username (ASCII, null-terminated). Caller retains ownership.
  @param[in]  Password  BMC password (ASCII, null-terminated). Caller retains ownership.

  @retval EFI_SUCCESS             Reset command sent successfully.
  @retval EFI_NOT_READY           Service not available.
  @retval EFI_INVALID_PARAMETER   Username or Password is NULL.
  @retval Others                  Error occurred.
**/
STATIC
EFI_STATUS
EFIAPI
BmcResetFactoryReset (
  IN NVIDIA_BMC_RESET_PROTOCOL  *This,
  IN CONST CHAR8                *Username,
  IN CONST CHAR8                *Password
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a: Initiating BMC factory reset\n", __func__));

  if ((Username == NULL) || (Password == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid credentials (NULL)\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  if ((mCachedController == NULL) || (mCachedBmcLocation == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Service not available (no cached controller or BMC location)\n", __func__));
    return EFI_NOT_READY;
  }

  Status = ExecuteBmcFactoryReset (Username, Password);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: BMC factory reset failed: %r\n", __func__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: BMC factory reset initiated successfully\n", __func__));

  return EFI_SUCCESS;
}

/**
  Initialize the Redfish Config Handler.

  This function is called by the Redfish Config Handler Driver when
  a Redfish service becomes available. We extract the controller handle
  from the original REST EX handle and cache it for creating fresh
  connections later.

  @param[in]  This                     Pointer to EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL instance.
  @param[in]  RedfishConfigServiceInfo Pointer to the Redfish service configuration.

  @retval EFI_SUCCESS                  Handler initialized successfully.
**/
STATIC
EFI_STATUS
EFIAPI
RedfishBmcResetInit (
  IN EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL  *This,
  IN REDFISH_CONFIG_SERVICE_INFORMATION     *RedfishConfigServiceInfo
  )
{
  EFI_STATUS                    Status;
  EFI_SERVICE_BINDING_PROTOCOL  *ServiceBinding;
  EFI_SERVICE_BINDING_PROTOCOL  *TestBinding;
  UINTN                         HandleCount;
  EFI_HANDLE                    *HandleBuffer;
  UINTN                         Index;

  DEBUG ((DEBUG_INFO, "%a: Redfish service available\n", __func__));

  if (RedfishConfigServiceInfo == NULL) {
    DEBUG ((DEBUG_WARN, "%a: No service info provided\n", __func__));
    return EFI_SUCCESS;
  }

  if (RedfishConfigServiceInfo->RedfishServiceRestExHandle == NULL) {
    DEBUG ((DEBUG_WARN, "%a: No REST EX handle provided\n", __func__));
    return EFI_SUCCESS;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: RestExHandle=%p, Location=%s, UseHttps=%d\n",
    __func__,
    RedfishConfigServiceInfo->RedfishServiceRestExHandle,
    RedfishConfigServiceInfo->RedfishServiceLocation,
    RedfishConfigServiceInfo->RedfishServiceUseHttps
    ));

  //
  // Get the Service Binding protocol from the REST EX handle
  // The REST EX child handle should have access to its parent's service binding
  //
  Status = gBS->OpenProtocol (
                  RedfishConfigServiceInfo->RedfishServiceRestExHandle,
                  &gEfiRestExServiceBindingProtocolGuid,
                  (VOID **)&ServiceBinding,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    //
    // The REST EX handle itself might not have Service Binding,
    // so we need to find the controller that created it
    //
    DEBUG ((DEBUG_INFO, "%a: REST EX handle doesn't have Service Binding, searching controllers...\n", __func__));
    ServiceBinding = NULL;
  }

  //
  // Find all handles with REST EX Service Binding Protocol
  //
  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiRestExServiceBindingProtocolGuid,
                        NULL,
                        &HandleCount,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate REST EX controllers: %r\n", __func__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Found %u REST EX Service Binding controller(s)\n", __func__, (UINT32)HandleCount));

  //
  // Find the controller handle that has matching service binding,
  // or just use the first one if we couldn't get ServiceBinding earlier
  //
  mCachedController = NULL;

  if (ServiceBinding != NULL) {
    //
    // Try to match the service binding instance
    //
    for (Index = 0; Index < HandleCount; Index++) {
      Status = gBS->OpenProtocol (
                      HandleBuffer[Index],
                      &gEfiRestExServiceBindingProtocolGuid,
                      (VOID **)&TestBinding,
                      NULL,
                      NULL,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (!EFI_ERROR (Status) && (TestBinding == ServiceBinding)) {
        mCachedController = HandleBuffer[Index];
        DEBUG ((DEBUG_INFO, "%a: Found matching controller handle: %p\n", __func__, mCachedController));
        break;
      }
    }
  }

  //
  // If no match found, use the first controller
  //
  if (mCachedController == NULL) {
    mCachedController = HandleBuffer[0];
    DEBUG ((DEBUG_INFO, "%a: Using first controller handle: %p\n", __func__, mCachedController));
  }

  FreePool (HandleBuffer);

  if (mCachedController == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Could not find controller handle\n", __func__));
    return EFI_NOT_FOUND;
  }

  //
  // Cache BMC Location (make a deep copy)
  //
  if (RedfishConfigServiceInfo->RedfishServiceLocation != NULL) {
    if (mCachedBmcLocation != NULL) {
      FreePool (mCachedBmcLocation);
    }

    mCachedBmcLocation = AllocateCopyPool (
                           StrSize (RedfishConfigServiceInfo->RedfishServiceLocation),
                           RedfishConfigServiceInfo->RedfishServiceLocation
                           );
    if (mCachedBmcLocation == NULL) {
      mCachedController = NULL;
      DEBUG ((DEBUG_ERROR, "%a: Failed to cache BMC location\n", __func__));
      return EFI_OUT_OF_RESOURCES;
    }

    DEBUG ((DEBUG_INFO, "%a: Cached BMC Location: %s\n", __func__, mCachedBmcLocation));
  }

  //
  // Cache HTTPS flag
  //
  mCachedUseHttps = RedfishConfigServiceInfo->RedfishServiceUseHttps;

  //
  // Get the local IP address from SMBIOS Type 42 (Redfish Host Interface)
  // This is needed to configure the fresh REST EX handle for HTTP requests
  // Note: Failure here is non-fatal - we log it but continue initialization
  //
  Status = GetHostIpFromSmbiosType42 ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Could not get Host IP from SMBIOS Type 42: %r\n", __func__, Status));
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Successfully cached: Controller=%p, Location=%s, HTTPS=%d\n",
    __func__,
    mCachedController,
    mCachedBmcLocation,
    mCachedUseHttps
    ));

  return EFI_SUCCESS;
}

/**
  Stop the Redfish Config Handler.

  @param[in]  This    Pointer to EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL instance.

  @retval EFI_SUCCESS Handler stopped successfully.
**/
STATIC
EFI_STATUS
EFIAPI
RedfishBmcResetStop (
  IN EDKII_REDFISH_CONFIG_HANDLER_PROTOCOL  *This
  )
{
  DEBUG ((DEBUG_INFO, "%a: Stopping\n", __func__));

  //
  // Free cached data
  //
  if (mCachedBmcLocation != NULL) {
    FreePool (mCachedBmcLocation);
    mCachedBmcLocation = NULL;
  }

  if (mCachedHttpConfig != NULL) {
    if (!mCachedHttpConfig->HttpConfigData.LocalAddressIsIPv6) {
      if (mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node != NULL) {
        FreePool (mCachedHttpConfig->HttpConfigData.AccessPoint.IPv4Node);
      }
    } else {
      if (mCachedHttpConfig->HttpConfigData.AccessPoint.IPv6Node != NULL) {
        FreePool (mCachedHttpConfig->HttpConfigData.AccessPoint.IPv6Node);
      }
    }

    FreePool (mCachedHttpConfig);
    mCachedHttpConfig = NULL;
  }

  mCachedController = NULL;

  return EFI_SUCCESS;
}

/**
  Entry point for the Redfish BMC Reset driver.

  @param[in]  ImageHandle   The firmware allocated handle for the EFI image.
  @param[in]  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS       Driver initialized successfully.
  @retval Others            Error occurred.
**/
EFI_STATUS
EFIAPI
RedfishBmcResetEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a: Entry\n", __func__));

  mDriverHandle = NULL;
  mImageHandle  = ImageHandle;

  //
  // Initialize the Config Handler Protocol
  //
  mConfigHandler.Init = RedfishBmcResetInit;
  mConfigHandler.Stop = RedfishBmcResetStop;

  //
  // Initialize BMC Reset Protocol
  //
  mBmcResetProtocol.FactoryReset = BmcResetFactoryReset;
  mBmcResetProtocol.IsAvailable  = BmcResetIsAvailable;

  //
  // Install both protocols on a new handle
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mDriverHandle,
                  &gEdkIIRedfishConfigHandlerProtocolGuid,
                  &mConfigHandler,
                  &gNvidiaBmcResetProtocolGuid,
                  &mBmcResetProtocol,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install protocols: %r\n", __func__, Status));
    return Status;
  }

  mProtocolInstalled = TRUE;
  DEBUG ((DEBUG_INFO, "%a: Protocols installed successfully\n", __func__));

  return EFI_SUCCESS;
}

/**
  Unload handler for the driver.

  @param[in]  ImageHandle   Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS       The image has been unloaded.
**/
EFI_STATUS
EFIAPI
RedfishBmcResetUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a: Unloading\n", __func__));

  //
  // Stop the handler first
  //
  RedfishBmcResetStop (&mConfigHandler);

  //
  // Uninstall protocols
  //
  if (mProtocolInstalled && (mDriverHandle != NULL)) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    mDriverHandle,
                    &gEdkIIRedfishConfigHandlerProtocolGuid,
                    &mConfigHandler,
                    &gNvidiaBmcResetProtocolGuid,
                    &mBmcResetProtocol,
                    NULL
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall protocols: %r\n", __func__, Status));
      return Status;
    }

    mProtocolInstalled = FALSE;
    mDriverHandle      = NULL;
  }

  return EFI_SUCCESS;
}
