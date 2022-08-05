/** @file

  This file defines the OEM IPMI commands for Redfish Interface.

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

/** IPMI definitions for Nvidia OEM commands

  This file contains 0x3c NetFn OEM commands
**/

#ifndef _IPMI_NET_FN_OEM_H_
#define _IPMI_NET_FN_OEM_H_

//
// Net function definition for OEM command
//
#define IPMI_NETFN_OEM  0x3c
//
//  Below is Definitions for IPMI Nvidia OEM Commands
//

#define IPMI_OEM_GET_USB_DESCRIPTION_CMD                                0x30
#define IPMI_OEM_GET_VIRTUAL_USB_SERIAL_NUMBER_CMD                      0x31
#define IPMI_OEM_GET_REDFISH_SERVICE_HOSTNAME_CMD                       0x32
#define IPMI_OEM_GET_IPMI_CHANNEL_NUMBER_OF_REDFISH_HOST_INTERFACE_CMD  0x33
#define IPMI_OEM_GET_REDFISH_SERVICE_UUID                               0x34
#define IPMI_OEM_GET_REDFISH_SERVICE_IP_PORT                            0x35
#define IPMI_RESPONSE_IPV4_SUPPORTED                                    0x00
#define IPMI_RESPONSE_IPV6_SUPPORTED                                    0x01
#define IPMI_RESPONSE_BOTH_IPV4_IPV6_SUPPORTED                          0x02
#define IPMI_RESPONSE_IP_ADDRESS_FORMAT_UNKNOWN                         0x03
#define TYPE_VENDOR_ID                                                  0x01
#define TYPE_PRODUCT_ID                                                 0x02

#define SERIAL_NUMBER_MAX_LENGTH  64
#define HOSTNAME_MAX_LENGTH       64

#pragma pack(1)

//
//  Constants and Structure definitions for "SMBIOS Type42 OEM IPMI" commands to follow here
//
typedef struct {
  UINT8    Type;
} IPMI_GET_USB_DESCRIPTION_COMMAND_DATA;

typedef struct {
  UINT8    CompletionCode;
  UINT8    VendorOrProductId[2];
} IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA;

typedef struct {
  UINT8    CompletionCode;
  CHAR8    SerialNumber[SERIAL_NUMBER_MAX_LENGTH];
} IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA;

typedef struct {
  UINT8    CompletionCode;
  CHAR8    Hostname[HOSTNAME_MAX_LENGTH];
} IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA;

typedef struct {
  UINT8    CompletionCode;
  UINT8    ChannelNum;
} IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA;

typedef struct {
  UINT8    CompletionCode;
  UINT8    RedfishServiceIpPort[2];
} IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA;

typedef struct {
  UINT8       CompletionCode;
  EFI_GUID    Uuid;
} IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA;

#pragma pack()

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
GetRFHIUSBDescription (
  OUT UINT16  *UsbId,
  IN  UINT8   Type
  );

/**
  Function to retrieve USB Virtual Serial Number for Redfish Host Interface - RFHI.

  @param[out] *SerialNum   Pointer to the USB Serial Number of RFHI Data
                           Descriptor for USB network Interface V2.

  @retval EFI_SUCCESS      USB Serial number is successfully fetched.
  @retval EFI_DEVICE_ERROR An IPMI failure occured.
**/
EFI_STATUS
GetRFHIUSBVirtualSerialNumber (
  OUT CHAR8  *SerialNum
  );

/**
  Function to retrieve USB Hostname for Redfish Host Interface - RFHI.

  @param[out] *Hostname    A pointer to the USB Hostname of RFHI Data
                           Descriptor for USB network Interface V2.

  @retval EFI_SUCCESS      USB Hostname is successfully fetched.
  @retval EFI_DEVICE_ERROR An IPMI failure occured.
**/
EFI_STATUS
GetRFHIHostname (
  OUT CHAR8  *Hostname
  );

/**
  Function to retrieve USB Channel Number for Redfish Host Interface - RFHI.

  @param[out] *ChannelNum   Pointer to hold USB Channel Number of RFHI
                            Data Descriptor for USB network Interface V2.

  @retval EFI_SUCCESS      USB Channel number is successfully fetched.
  @retval EFI_DEVICE_ERROR An IPMI failure occured.
**/
EFI_STATUS
GetRFHIIpmiChannelNumber (
  OUT UINT8  *ChannelNum
  );

/**
  Function to retrieve MAC Address for Redfish Host Interface - RFHI.
  MAC Address is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel       Channel number for USB network interface.
  @param[OUT] *MacAddress   Pointer to hold MAC Address of RFHI Data
                            Descriptor for USB network Interface V2.

  @retval EFI_SUCCESS       USB Channel number is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRFHIMACAddress (
  IN   UINT8  Channel,
  OUT  UINT8  *MacAddress
  );

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
GetRFHIIpDiscoveryType (
  IN  UINT8  Channel,
  OUT UINT8  *IpDiscoveryType
  );

/**
  Function to retireve IP Address for Redfish Host Interface - RFHI.
  IP Address is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters

  @param[IN]  Channel            Channel number for USB network interface.
  @param[OUT] *IpAdd             Pointer to hold IP Address of USB NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpAddress is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRFHIIpAddress (
  IN  UINT8  Channel,
  OUT UINT8  *IpAdd
  );

/**
  Function to retireve IP Mask for Redfish Host Interface - RFHI.
  IP Mask is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel            Channel number for USB network interface.
  @param[OUT] *IpAddMask         Pointer to hold Ip Mask of USB NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpAddMask is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRFHIIpMask (
  IN  UINT8  Channel,
  OUT UINT8  *IpAddMask
  );

/**
  Function to retireve VLAN ID for Redfish Host Interface - RFHI.
  VLAN ID is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel         Channel number for USB network interface.
  @param[OUT] *VlanId         Pointer to hold VLAN ID of USB NIC for Redfish Over IP Protocol.

  @retval EFI_SUCCESS       VLanId is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRFHIVlanId (
  IN  UINT8   Channel,
  OUT UINT16  *VlanId
  );

/**
  Function to retireve IP Port for Redfish Host Interface - RFHI.
  IP Port is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[OUT] *IpPort         Pointer to hold Ip port of USB NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpPort is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRFHIIpPort (
  OUT UINT16  *IpPort
  );

/**
  Function to retireve IP Address format for Redfish Host Interface - RFHI.
  IP Address Format is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[IN]  Channel         Channel number for USB network interface.
  @param[OUT] *IpAddFormat    Pointer to hold IP Address Format of USB NIC for Redfish over Ip Protocol.

  @retval EFI_SUCCESS       IpAddFormat is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
RFHIGetIpAddFormat (
  IN  UINT8  Channel,
  OUT UINT8  *IpAddformat
  );

/**
  Function to retireve Service UUID for Redfish Host Interface - RFHI.
  Service UUID is fetched through a standard OEM IPMI COMMAND - Get Lan Configuration Parameters.

  @param[OUT] *Uuid         Pointer to hold Redfish Service UUID.

  @retval EFI_SUCCESS       Uuid is successfully fetched.
  @retval EFI_DEVICE_ERROR  An IPMI failure occured.
**/
EFI_STATUS
GetRFHIUUID (
  OUT EFI_GUID  *Uuid
  );

#endif
