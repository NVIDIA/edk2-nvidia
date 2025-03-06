/** @file
*
*  Ipmi function calls to support population of type42 smbios record by
*  RedfishPlatformHostInterfaceOemLib.
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent.
*
**/

#include <Uefi.h>
#include <Guid/GlobalVariable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <IndustryStandard/Ipmi.h>
#include <IndustryStandard/IpmiNetFnTransport.h>
#include "RedfishPlatformHostInterfaceIpmi.h"

/**
  Function to retrieve USB Description for Redfish Host Interface - RFHI.

  @param[out] *UsbId         Pointer to hold RFHI Data Descriptor USB
                             Product ID or Vendor ID for USB network
                             Interface V2.
  @param[IN] Type            Type of USB ID to get - Vendor/Product.

  @retval EFI_SUCCESS        USB Descripiton is successfully fetched.
  @retval EFI_DEVICE_ERROR   An IPMI failure occured.
**/
EFI_STATUS
GetRfhiUsbDescription (
  OUT UINT16  *UsbId,
  IN  UINT8   Type
  )
{
  EFI_STATUS                              Status;
  IPMI_GET_USB_DESCRIPTION_COMMAND_DATA   CommandData;
  IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA  ResponseData;
  UINT32                                  ResponseSize;

  // IPMI callout to NetFn 3C, command 30
  //    Request data:
  //      Byte 1: TYPE_VENDOR_ID/TYPE_PRODUCT_ID
  CommandData.Type = Type;
  ResponseSize     = sizeof (ResponseData);

  // Response data:
  //     Byte 1  : Completion code
  //     Byte 2,3: Vendor ID/Product ID based on Type
  Status = IpmiSubmitCommand (
             IPMI_NETFN_OEM,
             IPMI_OEM_GET_USB_DESCRIPTION_CMD,
             (UINT8 *)&CommandData,
             sizeof (CommandData),
             (UINT8 *)&ResponseData,
             &ResponseSize
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, ResponseData.CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  (*UsbId) = ((UINT16)ResponseData.VendorOrProductId[0] << 8) | ((UINT16)ResponseData.VendorOrProductId[1] &0xFF);
  return Status;
}

/**
  Function to retrieve USB Virtual Serial Number for Redfish Host Interface - RFHI.

  @param[out] *SerialNum   A pointer to the USB Serial Number of RFHI
                           Data Descriptor for USB network Interface V2.

  @retval EFI_SUCCESS      USB Serial number is successfully fetched.
  @retval EFI_DEVICE_ERROR An IPMI failure occured.
**/
EFI_STATUS
GetRfhiUsbVirtualSerialNumber (
  OUT CHAR8  *SerialNum
  )
{
  EFI_STATUS                                Status;
  IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA  ResponseData;
  UINT32                                    ResponseSize;
  UINT8                                     SerialNumLen;

  // IPMI callout to NetFn 3C, command 31
  ResponseSize = sizeof (ResponseData);

  // Response data:
  //     Byte 1   : Completion code
  //     Byte 2-65: SerialNumber
  Status = IpmiSubmitCommand (
             IPMI_NETFN_OEM,
             IPMI_OEM_GET_VIRTUAL_USB_SERIAL_NUMBER_CMD,
             NULL,
             0,
             (UINT8 *)&ResponseData,
             &ResponseSize
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, ResponseData.CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  SerialNumLen = ResponseSize - 1;
  ASSERT (SerialNumLen <= SERIAL_NUMBER_MAX_LENGTH);
  CopyMem (SerialNum, ResponseData.SerialNumber, SerialNumLen);
  SerialNum[SerialNumLen] = '\0';
  return Status;
}

/**
  Function to retrieve USB Hostname for Redfish Host Interface - RFHI.

  @param[out] *Hostname    A pointer to the USB Hostname of RFHI Data
                           Descriptor for USB network Interface V2.

  @retval EFI_SUCCESS      USB Hostname is successfully fetched.
  @retval EFI_DEVICE_ERROR An IPMI failure occured.
**/
EFI_STATUS
GetRfhiHostname (
  OUT CHAR8  *Hostname
  )
{
  EFI_STATUS                                       Status;
  IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA  ResponseData;
  UINT32                                           ResponseSize;
  UINT8                                            HostnameLen;

  // IPMI callout to NetFn 3C, command 32
  ResponseSize = sizeof (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA);

  // Response data:
  //     Byte 1   : Completion code
  //     Byte 2-65: Hostname
  Status = IpmiSubmitCommand (
             IPMI_NETFN_OEM,
             IPMI_OEM_GET_REDFISH_SERVICE_HOSTNAME_CMD,
             NULL,
             0,
             (UINT8 *)&ResponseData,
             &ResponseSize
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, ResponseData.CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  HostnameLen = (UINT8)(ResponseSize - 1);
  CopyMem (Hostname, ResponseData.Hostname, HostnameLen);
  Hostname[HostnameLen] = '\0';
  return Status;
}

/**
  Function to retrieve USB Channel Number for Redfish Host Interface - RFHI.

  @param[out] *ChannelNum   Pointer to hold USB Channel Number of RFHI Data
                            Descriptor for USB network Interface V2.

  @retval EFI_SUCCESS      USB Channel number is successfully fetched.
  @retval EFI_DEVICE_ERROR An IPMI failure occured.
**/
EFI_STATUS
GetRfhiIpmiChannelNumber (
  OUT UINT8  *ChannelNum
  )
{
  EFI_STATUS                                       Status;
  IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA  ResponseData;
  UINT32                                           ResponseSize;

  // IPMI callout to NetFn 3C, command 33
  ResponseSize = sizeof (ResponseData);

  // Response data:
  //     Byte 1   : Completion code
  //     Byte 2   : ChannelNum
  Status = IpmiSubmitCommand (
             IPMI_NETFN_OEM,
             IPMI_OEM_GET_IPMI_CHANNEL_NUMBER_OF_REDFISH_HOST_INTERFACE_CMD,
             NULL,
             0,
             (UINT8 *)&ResponseData,
             &ResponseSize
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, ResponseData.CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  (*ChannelNum) = ResponseData.ChannelNum;
  return Status;
}

/**
  Function to retrieve MAC Address for Redfish Host Interface - RFHI.
  MAC Address is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel       Channel number for USB network interface.
  @param[OUT] *MacAddress   Pointer to hold MAC Address of RFHI Data Descriptor
                            for USB network Interface V2.

  @retval EFI_SUCCESS       USB Channel number is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRfhiMacAddress (
  IN   UINT8  Channel,
  OUT  UINT8  *MacAddress
  )
{
  EFI_STATUS                                      Status;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST   GetLanConfigRequest;
  UINT8                                           ResponseData[32];
  UINT32                                          ResponseDataSize;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE  *GetLanConfigResponse;
  IPMI_LAN_MAC_ADDRESS                            *MacAddRsp;

  GetLanConfigResponse                    = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)ResponseData;
  GetLanConfigRequest.ChannelNumber.Uint8 = Channel;
  GetLanConfigRequest.ParameterSelector   = IpmiLanMacAddress;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE) + sizeof (IPMI_LAN_MAC_ADDRESS);
  Status           = IpmiSubmitCommand (
                       IPMI_NETFN_TRANSPORT,
                       IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                       (UINT8 *)&GetLanConfigRequest,
                       sizeof (GetLanConfigRequest),
                       ResponseData,
                       &ResponseDataSize
                       );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, GetLanConfigResponse->CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  MacAddRsp = (IPMI_LAN_MAC_ADDRESS *)GetLanConfigResponse->ParameterData;
  CopyMem (MacAddress, MacAddRsp->MacAddress, sizeof (IPMI_LAN_MAC_ADDRESS));
  return EFI_SUCCESS;
}

/**
  Function to retireve IP Discovery Type for Redfish Host Interface - RFHI.
  IP Discovery Type is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel            Channel number for USB network interface.
  @param[OUT] *IpDiscoveryType   Pointer to hold Ip Discovery Type of USB
                                 NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpDiscoveryType is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRfhiIpDiscoveryType (
  IN  UINT8  Channel,
  OUT UINT8  *IpDiscoveryType
  )
{
  EFI_STATUS                                      Status;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST   GetLanConfigRequest;
  UINT8                                           ResponseData[32];
  UINT32                                          ResponseDataSize;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE  *GetLanConfigResponse;
  UINT8                                           Type;

  GetLanConfigResponse = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)ResponseData;

  GetLanConfigRequest.ChannelNumber.Uint8 = Channel;
  GetLanConfigRequest.ParameterSelector   = IpmiLanIpAddressSource;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (ResponseData);
  Status           = IpmiSubmitCommand (
                       IPMI_NETFN_TRANSPORT,
                       IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                       (UINT8 *)&GetLanConfigRequest,
                       sizeof (GetLanConfigRequest),
                       ResponseData,
                       &ResponseDataSize
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error, Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, GetLanConfigResponse->CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  Type               = GetLanConfigResponse->ParameterData[0];
  Type               = Type & 0x0F;
  (*IpDiscoveryType) = Type;
  return EFI_SUCCESS;
}

/**
  Function to retireve IP Address for Redfish Host Interface - RFHI.
  IP Address is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel            Channel number for USB network interface.
  @param[OUT] *IpAdd             Pointer to hold IP Address of USB NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpAddress is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRfhiIpAddress (
  IN  UINT8  Channel,
  OUT UINT8  *IpAdd
  )
{
  EFI_STATUS                                      Status;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST   GetLanConfigRequest;
  UINT8                                           ResponseData[32];
  UINT32                                          ResponseDataSize;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE  *GetLanConfigResponse;
  IPMI_LAN_IP_ADDRESS                             *IpV4Address;

  GetLanConfigResponse = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)ResponseData;

  GetLanConfigRequest.ChannelNumber.Uint8 = Channel;
  GetLanConfigRequest.ParameterSelector   = IpmiLanIpAddress;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (ResponseData);
  Status           = IpmiSubmitCommand (
                       IPMI_NETFN_TRANSPORT,
                       IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                       (UINT8 *)&GetLanConfigRequest,
                       sizeof (GetLanConfigRequest),
                       ResponseData,
                       &ResponseDataSize
                       );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, GetLanConfigResponse->CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  IpV4Address = (IPMI_LAN_IP_ADDRESS *)GetLanConfigResponse->ParameterData;
  CopyMem (IpAdd, IpV4Address->IpAddress, sizeof (IPMI_LAN_IP_ADDRESS));
  return EFI_SUCCESS;
}

/**
  Function to retireve IP Mask for Redfish Host Interface - RFHI.
  IP Mask is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel            Channel number for USB network interface.
  @param[OUT] *IpAddMask         Pointer to hold Ip Mask of USB NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpAddMask is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRfhiIpMask (
  IN  UINT8  Channel,
  OUT UINT8  *IpAddMask
  )
{
  EFI_STATUS                                      Status;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST   GetLanConfigRequest;
  UINT8                                           ResponseData[32];
  UINT32                                          ResponseDataSize;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE  *GetLanConfigResponse;
  IPMI_LAN_SUBNET_MASK                            *Mask;

  GetLanConfigResponse = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)ResponseData;

  GetLanConfigRequest.ChannelNumber.Uint8 = Channel;
  GetLanConfigRequest.ParameterSelector   = IpmiLanSubnetMask;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (ResponseData);
  Status           = IpmiSubmitCommand (
                       IPMI_NETFN_TRANSPORT,
                       IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                       (UINT8 *)&GetLanConfigRequest,
                       sizeof (GetLanConfigRequest),
                       ResponseData,
                       &ResponseDataSize
                       );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, GetLanConfigResponse->CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  Mask = (IPMI_LAN_SUBNET_MASK *)GetLanConfigResponse->ParameterData;
  CopyMem (IpAddMask, Mask->IpAddress, sizeof (IPMI_LAN_SUBNET_MASK));
  return EFI_SUCCESS;
}

/**
  Function to retireve VLAN ID for Redfish Host Interface - RFHI.
  VLAN ID is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel         Channel number for USB network interface.
  @param[OUT] *VlanId         Pointer to hold VLAN ID of USB NIC for Redfish Over IP Protocol.

  @retval EFI_SUCCESS       VLanId is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRfhiVlanId (
  IN  UINT8   Channel,
  OUT UINT16  *VlanId
  )
{
  EFI_STATUS                                      Status;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST   GetLanConfigRequest;
  UINT8                                           ResponseData[32];
  UINT32                                          ResponseDataSize;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE  *GetLanConfigResponse;

  GetLanConfigResponse = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)ResponseData;

  GetLanConfigRequest.ChannelNumber.Uint8 = Channel;
  GetLanConfigRequest.ParameterSelector   = IpmiLanVlanId;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (ResponseData);
  Status           = IpmiSubmitCommand (
                       IPMI_NETFN_TRANSPORT,
                       IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                       (UINT8 *)&GetLanConfigRequest,
                       sizeof (GetLanConfigRequest),
                       ResponseData,
                       &ResponseDataSize
                       );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, GetLanConfigResponse->CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  (*VlanId) = ((UINT16)GetLanConfigResponse->ParameterData[0] << 8) | ((UINT16)GetLanConfigResponse->ParameterData[1] &0xFF);
  return EFI_SUCCESS;
}

/**
  Function to retireve IP Address format for Redfish Host Interface - RFHI.
  IP Address Format is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel         Channel number for USB network interface.
  @param[OUT] *IpAddFormat    Pointer to hold IP Address Format of USB NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpAddFormat is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRfhiIpAddFormat (
  IN  UINT8  Channel,
  OUT UINT8  *IpAddformat
  )
{
  EFI_STATUS                                      Status;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST   GetLanConfigRequest;
  UINT8                                           ResponseData[32];
  UINT32                                          ResponseDataSize;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE  *GetLanConfigResponse;

  GetLanConfigResponse = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)ResponseData;

  GetLanConfigRequest.ChannelNumber.Uint8 = Channel;
  GetLanConfigRequest.ParameterSelector   = IpmiIpv4OrIpv6AddressEnable;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (ResponseData);
  Status           = IpmiSubmitCommand (
                       IPMI_NETFN_TRANSPORT,
                       IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                       (UINT8 *)&GetLanConfigRequest,
                       sizeof (GetLanConfigRequest),
                       ResponseData,
                       &ResponseDataSize
                       );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, GetLanConfigResponse->CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  (*IpAddformat) = GetLanConfigResponse->ParameterData[0];
  return EFI_SUCCESS;
}

/**
  Function to retireve IP Port for Redfish Host Interface - RFHI.
  IP Port is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[OUT] *IpPort         Pointer to hold Ip port of USB NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpPort is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRfhiIpPort (
  OUT UINT16  *IpPort
  )
{
  EFI_STATUS                                      Status;
  IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA  ResponseData;
  UINT32                                          ResponseSize;

  // IPMI callout to NetFn 3C, command 33
  ResponseSize = sizeof (ResponseData);

  // Response data:
  //     Byte 1   : Completion code
  //     Byte 2   : IP Port
  Status = IpmiSubmitCommand (
             IPMI_NETFN_OEM,
             IPMI_OEM_GET_REDFISH_SERVICE_IP_PORT,
             NULL,
             0,
             (UINT8 *)&ResponseData,
             &ResponseSize
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, ResponseData.CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  (*IpPort) = (((UINT16)ResponseData.RedfishServiceIpPort[0] << 8) | ((UINT16)ResponseData.RedfishServiceIpPort[1] & 0xFF));
  return EFI_SUCCESS;
}

/**
  Function to retireve Service UUID for Redfish Host Interface - RFHI.
  Service UUID is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[OUT] *Uuid         Pointer to hold Redfish Service UUID.

  @retval EFI_SUCCESS       Uuid is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRfhiUuid (
  OUT EFI_GUID  *Uuid
  )
{
  EFI_STATUS                                   Status;
  IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA  ResponseData;
  UINT32                                       ResponseSize;

  // IPMI callout to NetFn 3C, command 33
  ResponseSize = sizeof (ResponseData);

  // Response data:
  //     Byte 1   : Completion code
  //     Byte 2   : ChannelNum
  Status = IpmiSubmitCommand (
             IPMI_NETFN_OEM,
             IPMI_OEM_GET_REDFISH_SERVICE_UUID,
             NULL,
             0,
             (UINT8 *)&ResponseData,
             &ResponseSize
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Status = %r, IPMI error. Returning\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, ResponseData.CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  CopyGuid (Uuid, &ResponseData.Uuid);
  return EFI_SUCCESS;
}
