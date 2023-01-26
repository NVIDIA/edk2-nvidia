/** @file

  USB v2 network interface instance of RedfishPlatformHostInterfaceOemLib.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>
  Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Guid/SmBios.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/RedfishHostInterfaceLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/UsbNicInfoProtocol.h>
#include <RedfishPlatformHostInterfaceIpmi.h>

#define VERBOSE_COLUMN_SIZE      (16)
#define MAX_IP_ADDRESS_STR_SIZE  30
#define DEFAULT_REDFISH_IP_PORT  443
#define REDFISH_HI_DEBUG         DEBUG_INFO

// Channel number for Redfish Host Interface
UINT8  Channel;

EFI_MAC_ADDRESS  mBmcMacAddress;
BOOLEAN          mBmcMacReady = FALSE;
EFI_HANDLE       mImageHandle = NULL;
EFI_EVENT        mEvent       = NULL;

/**
  Dump IPv4 address.

  @param[in] Ip IPv4 address
**/
VOID
InternalDumpIp4Addr (
  IN EFI_IPv4_ADDRESS  *Ip
  )
{
  UINTN  Index;

  for (Index = 0; Index < 4; Index++) {
    DEBUG ((REDFISH_HI_DEBUG, "%d", Ip->Addr[Index]));
    if (Index < 3) {
      DEBUG ((REDFISH_HI_DEBUG, "."));
    }
  }

  DEBUG ((REDFISH_HI_DEBUG, "\n"));
}

/**
  Dump IPv6 address.

  @param[in] Ip IPv6 address
**/
VOID
InternalDumpIp6Addr (
  IN EFI_IPv6_ADDRESS  *Ip
  )
{
  UINTN  Index;

  for (Index = 0; Index < 16; Index++) {
    if (Ip->Addr[Index] != 0) {
      DEBUG ((REDFISH_HI_DEBUG, "%x", Ip->Addr[Index]));
    }

    Index++;

    if (Index > 15) {
      return;
    }

    if (((Ip->Addr[Index] & 0xf0) == 0) && (Ip->Addr[Index - 1] != 0)) {
      DEBUG ((REDFISH_HI_DEBUG, "0"));
    }

    DEBUG ((REDFISH_HI_DEBUG, "%x", Ip->Addr[Index]));

    if (Index < 15) {
      DEBUG ((REDFISH_HI_DEBUG, ":"));
    }
  }

  DEBUG ((REDFISH_HI_DEBUG, "\n"));
}

/**
  Dump data

  @param[in] Data Pointer to data.
  @param[in] Size size of data to dump.
**/
VOID
InternalDumpData (
  IN UINT8  *Data,
  IN UINTN  Size
  )
{
  UINTN  Index;

  for (Index = 0; Index < Size; Index++) {
    DEBUG ((REDFISH_HI_DEBUG, "%02x ", (UINTN)Data[Index]));
  }
}

/**
  Dump hex data

  @param[in] Data Pointer to hex data.
  @param[in] Size size of hex data to dump.
**/
VOID
InternalDumpHex (
  IN UINT8  *Data,
  IN UINTN  Size
  )
{
  UINTN  Index;
  UINTN  Count;
  UINTN  Left;

  Count = Size / VERBOSE_COLUMN_SIZE;
  Left  = Size % VERBOSE_COLUMN_SIZE;
  for (Index = 0; Index < Count; Index++) {
    InternalDumpData (Data + Index * VERBOSE_COLUMN_SIZE, VERBOSE_COLUMN_SIZE);
    DEBUG ((REDFISH_HI_DEBUG, "\n"));
  }

  if (Left != 0) {
    InternalDumpData (Data + Index * VERBOSE_COLUMN_SIZE, Left);
    DEBUG ((REDFISH_HI_DEBUG, "\n"));
  }

  DEBUG ((REDFISH_HI_DEBUG, "\n"));
}

/**
  Dump Redfish over IP protocol data

  @param[in] RedfishProtocolData     Pointer to REDFISH_OVER_IP_PROTOCOL_DATA
  @param[in] RedfishProtocolDataSize size of data to dump.
**/
VOID
DumpRedfishIpProtocolData (
  IN REDFISH_OVER_IP_PROTOCOL_DATA  *RedfishProtocolData,
  IN UINT8                          RedfishProtocolDataSize
  )
{
  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData: \n"));
  InternalDumpHex ((UINT8 *)RedfishProtocolData, RedfishProtocolDataSize);

  DEBUG ((REDFISH_HI_DEBUG, "Parsing as below: \n"));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->ServiceUuid - %g\n", &(RedfishProtocolData->ServiceUuid)));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->HostIpAssignmentType - %d\n", RedfishProtocolData->HostIpAssignmentType));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->HostIpAddressFormat - %d\n", RedfishProtocolData->HostIpAddressFormat));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->HostIpAddress: \n"));
  if (RedfishProtocolData->HostIpAddressFormat == REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP4) {
    InternalDumpIp4Addr ((EFI_IPv4_ADDRESS *)(RedfishProtocolData->HostIpAddress));
  } else {
    InternalDumpIp6Addr ((EFI_IPv6_ADDRESS *)(RedfishProtocolData->HostIpAddress));
  }

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->HostIpMask: \n"));
  if (RedfishProtocolData->HostIpAddressFormat == REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP4) {
    InternalDumpIp4Addr ((EFI_IPv4_ADDRESS *)(RedfishProtocolData->HostIpMask));
  } else {
    InternalDumpIp6Addr ((EFI_IPv6_ADDRESS *)(RedfishProtocolData->HostIpMask));
  }

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->RedfishServiceIpDiscoveryType - %d\n", RedfishProtocolData->RedfishServiceIpDiscoveryType));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->RedfishServiceIpAddressFormat - %d\n", RedfishProtocolData->RedfishServiceIpAddressFormat));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->RedfishServiceIpAddress: \n"));
  if (RedfishProtocolData->RedfishServiceIpAddressFormat == REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP4) {
    InternalDumpIp4Addr ((EFI_IPv4_ADDRESS *)(RedfishProtocolData->RedfishServiceIpAddress));
  } else {
    InternalDumpIp6Addr ((EFI_IPv6_ADDRESS *)(RedfishProtocolData->RedfishServiceIpAddress));
  }

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->RedfishServiceIpMask: \n"));
  if (RedfishProtocolData->RedfishServiceIpAddressFormat == REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP4) {
    InternalDumpIp4Addr ((EFI_IPv4_ADDRESS *)(RedfishProtocolData->RedfishServiceIpMask));
  } else {
    InternalDumpIp6Addr ((EFI_IPv6_ADDRESS *)(RedfishProtocolData->RedfishServiceIpMask));
  }

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->RedfishServiceIpPort - %d\n", RedfishProtocolData->RedfishServiceIpPort));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->RedfishServiceVlanId - %d\n", RedfishProtocolData->RedfishServiceVlanId));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->RedfishServiceHostnameLength - %d\n", RedfishProtocolData->RedfishServiceHostnameLength));

  DEBUG ((REDFISH_HI_DEBUG, "RedfishProtocolData->RedfishserviceHostname - %a\n", RedfishProtocolData->RedfishServiceHostname));
}

/**
  Dump Redfish over IP Device Descriptor data

  @param[in] RedfishDescriptorData     Pointer to REDFISH_OVER_IP_PROTOCOL_DATA
  @param[in] RedfishDescriptorDataSize size of data to dump.
**/
VOID
DumpRedfishDeviceDescriptorData (
  IN USB_INTERFACE_DEVICE_DESCRIPTOR_V2  *RedfishDescriptorData,
  IN UINT8                               RedfishDescriptorDataSize
  )
{
  DEBUG ((REDFISH_HI_DEBUG, "RedfishDeviceDescriptorData: \n"));
  InternalDumpHex ((UINT8 *)RedfishDescriptorData, RedfishDescriptorDataSize);

  DEBUG ((REDFISH_HI_DEBUG, "RedfishDeviceDescriptor->Length - %d\n", RedfishDescriptorData->Length));
  DEBUG ((REDFISH_HI_DEBUG, "RedfishDeviceDescriptor->IdVendor - %d\n", RedfishDescriptorData->IdVendor));
  DEBUG ((REDFISH_HI_DEBUG, "RedfishDeviceDescriptor->IdProduct - %d\n", RedfishDescriptorData->IdProduct));
  DEBUG ((REDFISH_HI_DEBUG, "RedfishDeviceDescriptor->MacAddress -  %x %x %x %x %x %x\n", RedfishDescriptorData->MacAddress[0], RedfishDescriptorData->MacAddress[1], RedfishDescriptorData->MacAddress[2], RedfishDescriptorData->MacAddress[3], RedfishDescriptorData->MacAddress[4], RedfishDescriptorData->MacAddress[5]));
  DEBUG ((REDFISH_HI_DEBUG, "RedfishDeviceDescriptor->Characteristics - %d\n", RedfishDescriptorData->Characteristics));
  DEBUG ((REDFISH_HI_DEBUG, "RedfishDeviceDescriptor->CBHandle - %d\n", RedfishDescriptorData->CredentialBootstrappingHandle));
}

/**
  fetches the Credential Bootstrapping Handle

  @param[out] CBHandle    Pointer to retrieve Credential Bootstrapping Handle

  @retval EFI_SUCCESS     CBHandle is returned successfully.
  @retval EFI_NOT_FOUND   CBHandle not found.
**/
EFI_STATUS
GetType38Handle (
  OUT SMBIOS_HANDLE  *CBHandle
  )
{
  EFI_STATUS                Status;
  SMBIOS_TABLE_ENTRY_POINT  *SmbiosTable;
  SMBIOS_STRUCTURE_POINTER  Smbios;
  SMBIOS_STRUCTURE_POINTER  SmbiosEnd;
  CHAR8                     *String;

  Status = EfiGetSystemConfigurationTable (&gEfiSmbiosTableGuid, (VOID **)&SmbiosTable);
  if (EFI_ERROR (Status) || (SmbiosTable == NULL)) {
    return EFI_NOT_FOUND;
  }

  Smbios.Hdr    = (SMBIOS_STRUCTURE *)(UINTN)SmbiosTable->TableAddress;
  SmbiosEnd.Raw = (UINT8 *)((UINTN)SmbiosTable->TableAddress + SmbiosTable->TableLength);

  do {
    if (Smbios.Hdr->Type == 38) {
      //
      // SMBIOS tables are byte packed so we need to do a byte copy to
      // prevent alignment faults on Itanium-based platform.
      //
      CopyMem (CBHandle, &Smbios.Hdr->Handle, sizeof (SMBIOS_HANDLE));
      return EFI_SUCCESS;
    }

    //
    // Go to the next SMBIOS structure. Each SMBIOS structure may include 2 parts:
    // 1. Formatted section; 2. Unformatted string section. So, 2 steps are needed
    // to skip one SMBIOS structure.
    //

    //
    // Step 1: Skip over formatted section.
    //
    String = (CHAR8 *)(Smbios.Raw + Smbios.Hdr->Length);

    //
    // Step 2: Skip over unformatted string section.
    //
    do {
      //
      // Each string is terminated with a NULL(00h) BYTE and the sets of strings
      // is terminated with an additional NULL(00h) BYTE.
      //
      for ( ; *String != 0; String++) {
      }

      if (*(UINT8 *)++String == 0) {
        //
        // Pointer to the next SMBIOS structure.
        //
        Smbios.Raw = (UINT8 *)++String;
        break;
      }
    } while (TRUE);
  } while (Smbios.Raw < SmbiosEnd.Raw);

  return EFI_NOT_FOUND;
}

/**
  Get platform Redfish host interface device descriptor.

  @param[out] DeviceType        Pointer to retrieve device type.
  @param[out] DeviceDescriptor  Pointer to retrieve REDFISH_INTERFACE_DATA, caller has to free
                                this memory using FreePool().
  @retval EFI_SUCCESS     Device descriptor is returned successfully in DeviceDescriptor.
  @retval EFI_NOT_FOUND   No Redfish host interface descriptor provided on this platform.
  @retval Others          Fail to get device descriptor.
**/
EFI_STATUS
RedfishPlatformHostInterfaceDeviceDescriptor (
  OUT UINT8                   *DeviceType,
  OUT REDFISH_INTERFACE_DATA  **DeviceDescriptor
  )
{
  EFI_STATUS                          Status;
  REDFISH_INTERFACE_DATA              *RedfishInterfaceData;
  USB_INTERFACE_DEVICE_DESCRIPTOR_V2  *DeviceDesc;
  UINT16                              VendorID;
  UINT16                              ProductID;
  UINT16                              CBHandle;

  RedfishInterfaceData = AllocateZeroPool (sizeof (USB_INTERFACE_DEVICE_DESCRIPTOR_V2) + 1);
  if (RedfishInterfaceData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RedfishInterfaceData->DeviceType = REDFISH_HOST_INTERFACE_DEVICE_TYPE_USB_V2;

  DeviceDesc         = (USB_INTERFACE_DEVICE_DESCRIPTOR_V2 *)((UINT8 *)RedfishInterfaceData + 1);
  DeviceDesc->Length = sizeof (USB_INTERFACE_DEVICE_DESCRIPTOR_V2) + 1;

  // Fetch Vendor ID and Product ID
  GetRFHIUSBDescription (&VendorID, TYPE_VENDOR_ID);
  GetRFHIUSBDescription (&ProductID, TYPE_PRODUCT_ID);
  CopyMem ((VOID *)&DeviceDesc->IdVendor, (VOID *)&VendorID, sizeof (DeviceDesc->IdVendor));
  CopyMem ((VOID *)&DeviceDesc->IdProduct, (VOID *)&ProductID, sizeof (DeviceDesc->IdProduct));

  // Credential Bootstrapping is enabled on all NVIDIA server platforms
  // Bit  0    - Credential bootstrapping via IPMI commands is supported.
  // Bits 1-15 - Reserved
  DeviceDesc->Characteristics = 0x1;
  Status                      = GetRFHIIpmiChannelNumber (&Channel);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Channel Number Retrieval failed\n", __FUNCTION__));
    return Status;
  }

  //
  // Return the USB NIC MAC address on host side.
  // MAC Address is came from UsbRndisDxe driver.
  //
  CopyMem ((VOID *)&DeviceDesc->MacAddress, (VOID *)&mBmcMacAddress.Addr, sizeof (DeviceDesc->MacAddress));

  // Credential Bootstrapping is an IPMI command, the handle of the interface is SSIF
  // Get the BIOS generated handle for the SMBIOS table type 38 - SSIF
  if (EFI_SUCCESS == GetType38Handle (&CBHandle)) {
    DeviceDesc->CredentialBootstrappingHandle = CBHandle;
  }

  *DeviceType       = REDFISH_HOST_INTERFACE_DEVICE_TYPE_PCI_PCIE_V2;
  *DeviceDescriptor = RedfishInterfaceData;
  DumpRedfishDeviceDescriptorData (DeviceDesc, sizeof (USB_INTERFACE_DEVICE_DESCRIPTOR_V2) - 1);
  return EFI_SUCCESS;
}

/**
  Get platform Redfish host interface protocol data.
  Caller should pass NULL in ProtocolRecord to retrive the first protocol record.
  Then continuously pass previous ProtocolRecord for retrieving the next ProtocolRecord.

  @param[out] ProtocolRecord     Pointer to retrieve the protocol record.
                                 caller has to free the new protocol record returned from
                                 this function using FreePool().
  @param[in] IndexOfProtocolData The index of protocol data.

  @retval EFI_SUCCESS     Protocol records are all returned.
  @retval EFI_NOT_FOUND   No more protocol records.
  @retval Others          Fail to get protocol records.
                        **/
EFI_STATUS
RedfishPlatformHostInterfaceProtocolData (
  OUT MC_HOST_INTERFACE_PROTOCOL_RECORD  **ProtocolRecord,
  IN UINT8                               IndexOfProtocolData
  )
{
  UINT8     ProtocolRecordSize;
  EFI_GUID  ServiceUuid;
  UINT8     IpAddFormat;
  // Redfish Service Protocol data
  UINT8                              RfSIpDiscoveryType;
  UINT8                              RfSIpAddressFormat;
  UINT8                              RfSIpAddress[16];
  UINT8                              HostIpAddress[16];
  UINT8                              RfSIpMask[16];
  UINT16                             RfSIpPort;            // Used for Static and AutoConfigure.
  UINT16                             RfSVlanId;            // Used for Static and AutoConfigure.
  UINT8                              RfSHostnameLength;    // length of the hostname string
  CHAR8                              *RfSHostname;
  MC_HOST_INTERFACE_PROTOCOL_RECORD  *CurrentProtocolRecord;
  REDFISH_OVER_IP_PROTOCOL_DATA      *ProtocolData;
  EFI_STATUS                         Status;
  UINT8                              Index;
  UINT8                              Size;

  if (IndexOfProtocolData != 0) {
    return EFI_NOT_FOUND;
  } else {
    //
    // Return the first Redfish protocol data to caller. Currently We only support
    // one protocol record.
    //
    Status = GetRFHIUUID (&ServiceUuid);
    if (Status != EFI_SUCCESS) {
      // IPMI command failed, initialize Redfish service UUID to zeroes.
      ServiceUuid.Data1 = 0;
      ServiceUuid.Data2 = 0;
      ServiceUuid.Data3 = 0;
      for (int i = 0; i < sizeof (ServiceUuid.Data4); i++) {
        ServiceUuid.Data4[i] = 0;
      }
    }

    Status = GetRFHIIpDiscoveryType (Channel, &RfSIpDiscoveryType);
    if (Status != EFI_SUCCESS) {
      // Set it to 00h -unknown
      RfSIpDiscoveryType = 0;
    }

    // Response of the command: IpmiIpv4OrIpv6AddressEnable
    // data 1 –
    // The following values can be set according the capabilities.
    // 00h = IPv6 addressing disabled - IPMI_RESPONSE_IPV4_SUPPORTED
    // 01h = Enable IPv6 addressing only. IPv4 addressing is disabled - IPMI_RESPONSE_IPV6_SUPPORTED
    // 02h = Enable IPv6 and IPv4 addressing simultaneously - IPMI_RESPONSE_BOTH_IPV4_IPV6_SUPPORTED

    Status = RFHIGetIpAddFormat (Channel, &IpAddFormat);
    if (Status != EFI_SUCCESS) {
      // Set it to 03h -unknown
      IpAddFormat = IPMI_RESPONSE_IP_ADDRESS_FORMAT_UNKNOWN;
    }

    if (IpAddFormat == IPMI_RESPONSE_IPV4_SUPPORTED) {
      RfSIpAddressFormat = REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP4;
    } else if (IpAddFormat == IPMI_RESPONSE_IPV6_SUPPORTED) {
      RfSIpAddressFormat = REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP6;
    } else if (IpAddFormat == IPMI_RESPONSE_BOTH_IPV4_IPV6_SUPPORTED) {
      // TODO: if the response is 02, that means both IPV4 and IPV6 are supported.
      // But for the smbiosview or dmidecode to display the IP address in right format,
      // this field should be set to either IPV4 or IPV6. Currently BMC is setting it to
      // supported both as Open BMC does the same.
      RfSIpAddressFormat = REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP4;
    } else {
      // if the response is 0x03 or anything else, assign format unknown.
      RfSIpAddressFormat = REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_UNKNOWN;
    }

    Status = GetRFHIIpAddress (Channel, RfSIpAddress);
    if (Status != EFI_SUCCESS) {
      // Initialize both Host Ip and Redfish Ip to all zeroes
      for (UINT8 i = 0; i < sizeof (RfSIpAddress); i++) {
        RfSIpAddress[i]  = 0;
        HostIpAddress[i] = 0;
      }
    } else {
      // Host IP and Redfish IP are of same subnet, with same network id, differ the host id by 1.
      for (Index = 0; Index < sizeof (RfSIpAddress); Index++) {
        HostIpAddress[Index] =   RfSIpAddress[Index];
      }

      // First 4 bytes contains IPV4 address, byte 4 is host id
      HostIpAddress[3] += 1;
    }

    Status = GetRFHIIpMask (Channel, RfSIpMask);
    if (Status != EFI_SUCCESS) {
      // Intialize IP mask to all zeroes
      for (int Index = 0; Index < sizeof (RfSIpMask); Index++) {
        RfSIpMask[Index] = 0;
      }
    }

    Status =   GetRFHIIpPort (&RfSIpPort);
    if (Status != EFI_SUCCESS) {
      // IPMI command failed,  RfsIpPort is not set, initialize it to the default port.
      RfSIpPort = DEFAULT_REDFISH_IP_PORT;     // https
    }

    Status = GetRFHIVlanId (Channel, &RfSVlanId);
    if (Status != EFI_SUCCESS) {
      RfSVlanId = 0;
    }

    RfSHostnameLength  = 0;
    ProtocolRecordSize = sizeof (REDFISH_OVER_IP_PROTOCOL_DATA);

    RfSHostname = (CHAR8 *)AllocateZeroPool (HOSTNAME_MAX_LENGTH + 1);
    if (RfSHostname != NULL) {
      Status = GetRFHIHostname (RfSHostname);
      if (Status == EFI_SUCCESS) {
        RfSHostnameLength  = AsciiStrLen (RfSHostname) + 1;
        ProtocolRecordSize = sizeof (REDFISH_OVER_IP_PROTOCOL_DATA) + RfSHostnameLength - 1;
      }
    }

    CurrentProtocolRecord = (MC_HOST_INTERFACE_PROTOCOL_RECORD *)AllocateZeroPool (sizeof (MC_HOST_INTERFACE_PROTOCOL_RECORD) -1 + ProtocolRecordSize);

    if (CurrentProtocolRecord == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Fail to allocate memory to store the Protocol Record Data \n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    CurrentProtocolRecord->ProtocolType        = MCHostInterfaceProtocolTypeRedfishOverIP;
    CurrentProtocolRecord->ProtocolTypeDataLen = ProtocolRecordSize;
    ProtocolData                               = (REDFISH_OVER_IP_PROTOCOL_DATA *)(&CurrentProtocolRecord->ProtocolTypeData);
    // update the protocol record 0 fields
    CopyMem ((VOID *)&ProtocolData->ServiceUuid, (VOID *)&ServiceUuid, sizeof (EFI_GUID));
    // BMC Configures the IP address of the USB NIC that acts as the redfish interface between HOST CPU
    // and the BMC. So populate the IP related fields with same data retireived from IPMI command
    // 'GetLanConfigurationParameters'
    ProtocolData->HostIpAssignmentType          = RfSIpDiscoveryType;
    ProtocolData->RedfishServiceIpDiscoveryType = RfSIpDiscoveryType;
    ProtocolData->HostIpAddressFormat           = RfSIpAddressFormat;
    ProtocolData->RedfishServiceIpAddressFormat = RfSIpAddressFormat;
    CopyMem ((VOID *)&ProtocolData->HostIpAddress, &HostIpAddress, sizeof (HostIpAddress));
    CopyMem ((VOID *)&ProtocolData->RedfishServiceIpAddress, &RfSIpAddress, sizeof (RfSIpAddress));
    CopyMem ((VOID *)&ProtocolData->HostIpMask, (VOID *)&RfSIpMask, sizeof (RfSIpMask));
    CopyMem ((VOID *)&ProtocolData->RedfishServiceIpMask, (VOID *)&RfSIpMask, sizeof (RfSIpMask));
    ProtocolData->RedfishServiceIpPort         = RfSIpPort;
    ProtocolData->RedfishServiceVlanId         = (UINT32)RfSVlanId;
    ProtocolData->RedfishServiceHostnameLength = RfSHostnameLength;
    if (RfSHostnameLength) {
      AsciiStrCpyS ((CHAR8 *)ProtocolData->RedfishServiceHostname, RfSHostnameLength, RfSHostname);
      FreePool (RfSHostname);
    }

    *ProtocolRecord = CurrentProtocolRecord;

    Size = sizeof (MC_HOST_INTERFACE_PROTOCOL_RECORD) -1 + ProtocolRecordSize - 2;
    DumpRedfishIpProtocolData (ProtocolData, Size);
  }

  return EFI_SUCCESS;
}

/**
  Get USB Virtual Serial Number.

  @param[out] SerialNumber    Pointer to retrieve complete serial number.
                              It is the responsibility of the caller to free the allocated memory for serial number.
  @retval EFI_SUCCESS         Serial number is returned.
  @retval Others              Failed to get the serial number
**/
EFI_STATUS
RedfishPlatformHostInterfaceUSBSerialNumber (
  OUT CHAR8  **SerialNumber
  )
{
  CHAR8       *SerialNum;
  EFI_STATUS  Status;

  SerialNum = AllocateZeroPool (SERIAL_NUMBER_MAX_LENGTH);
  if (SerialNum == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to allocate memory for serial number.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetRFHIUSBVirtualSerialNumber (SerialNum);

  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to retrieve serial number.\n", __FUNCTION__));
    return Status;
  }

  *SerialNumber = SerialNum;
  return EFI_SUCCESS;
}

/**
  Get the MAC address of NIC.

  @param[out] MacAddress      Pointer to retrieve MAC address

  @retval   EFI_SUCCESS      MAC address is returned in MacAddress

**/
EFI_STATUS
GetMacAddressInformation (
  OUT EFI_MAC_ADDRESS  *MacAddress
  )
{
  EFI_STATUS                    Status;
  NVIDIA_USB_NIC_INFO_PROTOCOL  *UsbNicInfo;

  if (MacAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol (&gNVIDIAUsbNicInfoProtocolGuid, NULL, (VOID **)&UsbNicInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, failed to locate gNVIDIAUsbNicInfoProtocolGuid: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = UsbNicInfo->GetMacAddress (UsbNicInfo, MacAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, failed to get MAC address: %r\n", __FUNCTION__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Callback function when gNVIDIAUsbNicInfoProtocolGuid is installed.

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.
**/
VOID
EFIAPI
EfiUsbNicProtocolIsReady (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS  Status;

  Status = GetMacAddressInformation (&mBmcMacAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, GetMacAddressInformation: %r\n", __FUNCTION__, Status));
    return;
  }

  DEBUG ((REDFISH_HI_DEBUG, "%a, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __FUNCTION__, mBmcMacAddress.Addr[0], mBmcMacAddress.Addr[1], mBmcMacAddress.Addr[2], mBmcMacAddress.Addr[3], mBmcMacAddress.Addr[4], mBmcMacAddress.Addr[5]));
  mBmcMacReady = TRUE;

  //
  // Notify RedfishHostInterfaceDxe to generate SMBIOS type 42.
  //
  Status = gBS->InstallProtocolInterface (
                  &mImageHandle,
                  &gNVIDIAHostInterfaceReadyProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, failed to notify RedfishHostInterfaceDxe: %r\n", __FUNCTION__, Status));
  }

  gBS->CloseEvent (Event);
}

/**
  Get the EFI protocol GUID installed by platform library which
  indicates the necessary information is ready for building
  SMBIOS 42h record.

  @param[out] InformationReadinessGuid  Pointer to retrive the protocol
                                        GUID.

  @retval EFI_SUCCESS          Notification is required for building up
                               SMBIOS type 42h record.
  @retval EFI_UNSUPPORTED      Notification is not required for building up
                               SMBIOS type 42h record.
  @retval EFI_ALREADY_STARTED  Platform host information is already ready.
  @retval Others               Other errors.
**/
EFI_STATUS
RedfishPlatformHostInterfaceNotification (
  OUT EFI_GUID  **InformationReadinessGuid
  )
{
  if (InformationReadinessGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (mBmcMacReady) {
    return EFI_ALREADY_STARTED;
  }

  *InformationReadinessGuid = AllocatePool (sizeof (EFI_GUID));
  if (*InformationReadinessGuid == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (*InformationReadinessGuid, &gNVIDIAHostInterfaceReadyProtocolGuid);

  return EFI_SUCCESS;
}

/**
  Construct Redfish host interface protocol data.

  @param ImageHandle     The image handle.
  @param SystemTable     The system table.

  @retval  EFI_SUCEESS  Install Boot manager menu success.
  @retval  Other        Return error status.

**/
EFI_STATUS
EFIAPI
RedfishPlatformHostInterfaceConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  VOID  *Registration;

  mImageHandle = ImageHandle;
  mBmcMacReady = FALSE;
  ZeroMem (&mBmcMacAddress, sizeof (EFI_MAC_ADDRESS));

  mEvent =   EfiCreateProtocolNotifyEvent (
               &gNVIDIAUsbNicInfoProtocolGuid,
               TPL_CALLBACK,
               EfiUsbNicProtocolIsReady,
               NULL,
               &Registration
               );

  return EFI_SUCCESS;
}
