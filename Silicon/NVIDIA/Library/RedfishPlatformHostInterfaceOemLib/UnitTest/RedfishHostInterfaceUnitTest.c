/** @file

  This file defines unit tests to verify various return response scenearios
  of OEM IPMI commands for Redfish Interface.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "RedfishHostInterfaceUnitTest.h"

#define UNIT_TEST_NAME     "Redfish Host Interface Ipmi Commands Test"
#define UNIT_TEST_VERSION  "1.0"

IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA           *ResponseResultsUsbDesc      = NULL;
IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA         *ResponseResultsSerNum       = NULL;
IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA  *ResponseResultsHostname     = NULL;
IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA  *ResponseResultsChnlNum      = NULL;
IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA   *ResponseResultsIpPort       = NULL;
IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA      *ResponseResultsUuid         = NULL;
IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE   *ResponseResultsMacAddr      = NULL;
IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE   *ResponseResultsIpDiscType   = NULL;
IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE   *ResponseResultsIpAddr       = NULL;
IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE   *ResponseResultsIpMask       = NULL;
IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE   *ResponseResultsIpAddrFormat = NULL;
IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE   *ResponseResultsVlanId       = NULL;

// Initialize it and use it only for unit testing purposes
static UINT8  TestChannel = 3;

IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA  ValidResponseUsbDesc = {
  0x00,           // CompletionCode - Normal
  { 0x20, 0x30 }, // VendorOrProductID
};

IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA  InvalidCompletionUsbDesc = {
  0xC3,  // Invalid completion code
  { 0x20, 0x30 },
};

IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA  DeviceFailureUsbDesc = {
  0xFF, // Invalid CompletionCode
  { 0xFF, 0xFF },
};

IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA  ValidResponseSerialNum = {
  0x00,              // CompletionCode - Normal
  "321AECDFD7685\0", // Serial Number
};

IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA  InvalidCompletionSerialNum = {
  0xC3,  // Invalid completion code
  "321ACSDFD7685\0",
};

IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA  DeviceFailureSerialNum = {
  0xFF,         // Invalid CompletionCode
  "FFFFFFFF\0", // Invalid Serial Number
};

IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA  ValidResponseHostname = {
  0x00,         // CompletionCode - Normal
  "ubuntu01\0", // Hostname
};

IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA  InvalidCompletionHostname = {
  0xC3,   // Invalid completion code
  "ubuntu\0",
};

IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA  DeviceFailureHostname = {
  0xFF,   // Invalid CompletionCode
  "FF\0", // Invalid hostname
};

IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA  ValidResponseChnlNum = {
  0x00, // CompletionCode - Normal
  0x03, // Channel Number
};

IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA  InvalidCompletionChnlNum = {
  0xC3,   // Invalid completion code
  0x03,
};

IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA  DeviceFailureChnlNum = {
  0xFF, // Invalid CompletionCode
  0xFF, // Invalid ChannelNumber
};

IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA  ValidResponseIpPort = {
  0x00,           // CompletionCode - Normal
  { 0x01, 0xBB }, // IP Port
};

IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA  InvalidCompletionIpPort = {
  0xC3,        // Invalid completion code
  { 0x01, 0xBB },
};

IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA  DeviceFailureIpPort = {
  0xFF,           // Invalid CompletionCode
  { 0xFF, 0xFF }, // Invalid IP Port
};

IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA  ValidResponseUuid = {
  0x00,                                               // CompletionCode - Normal
  {                                                     // SERVICE UUID
    0x5c99a21,                                          // Data1
    0xc70f,                                             // Data2
    0x4ad2,                                             // Data3
    { 0x8a, 0x5f, 0x35, 0xdf, 0x33, 0x43, 0xf5, 0x1e }, // Data4[8]
  },
};

IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA  InvalidCompletionUuid = {
  0xC3,                                               // Invalid completion code
  {                                                     // SERVICE UUID
    0x5c99a21,                                          // Data1
    0xc70f,                                             // Data2
    0x4ad2,                                             // Data3
    { 0x8a, 0x5f, 0x35, 0xdf, 0x33, 0x43, 0xf5, 0x1e }, // Data4[8]
  },
};

IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA  DeviceFailureUuid = {
  0xFF, // Invalid CompletionCode
  {                                                     // SERVICE UUID
    0xFFFFFFFF,                                         // Data1
    0xFFFF,                                             // Data2
    0xFFFF,                                             // Data3
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, // Data4[8]
  },
};

// Get MAC Address return responses
UINT8  ValidResponseMacAddr[] = {
  0x00,                              // CompletionCode - Normal
  0x00,                              // Parameter Revision
  0xD4, 0xBE, 0xD9, 0x8D, 0x46, 0x9A // ParameterData
};

UINT8  InvalidCompletionMacAddr[] = {
  0xC3,                        // Invalid CompletionCode
  0x00,                        // ParameterRevision
  0xCD, 0xBA, 0x87, 0xE9, 0x8A // ParameterData
};

UINT8  DeviceFailureMacAddr[] = {
  0xFF,                              // Invalid CompletionCode
  0xFF,                              // Invalid ParameterRevision
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF // Invalid ParameterData
};

// Get IPDiscoveryType return responses
UINT8  ValidResponseIpDiscType[] = {
  0x00,            // CompletionCode - Normal
  0x00,            // Parameter Revision
  0x01             // ParameterData
};

UINT8  InvalidCompletionIpDiscType[] = {
  0xC3,         // Invalid CompletionCode
  0x00,         // ParameterRevision
  0x01          // ParameterData
};

UINT8  DeviceFailureIpDiscType[] = {
  0xFF,            // Invalid CompletionCode
  0xFF,            // Invalid ParameterRevision
  0xFF             // Invalid ParameterData
};

// Get IpAddress return responses
UINT8  ValidResponseIPAddr[] = {
  0x00,                                                                                          // CompletionCode - Normal
  0x00,                                                                                          // Parameter Revision
  0x0A, 0x98, 0x70, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // ParameterData
};

UINT8  InvalidCompletionIPAddr[] = {
  0xC3,                                                                                          // Invalid CompletionCode
  0x00,                                                                                          // ParameterRevision
  0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // ParameterData
};

UINT8  DeviceFailureIPAddr[] = {
  0xFF,                                                                                          // Invalid CompletionCode
  0xFF,                                                                                          // Invalid ParameterRevision
  0xFF, 0xFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF // Invalid ParameterData
};

// Get IPMask return responses
UINT8  ValidResponseIPMask[] = {
  0x00,                                                                                          // CompletionCode - Normal
  0x00,                                                                                          // Parameter Revision
  0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // ParameterData
};

UINT8  InvalidCompletionIPMask[] = {
  0xC3,                                                                                          // Invalid CompletionCode
  0x00,                                                                                          // ParameterRevision
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // ParameterData
};

UINT8  DeviceFailureIPMask[] = {
  0x00,                                                                                          // CompletionCode - Normal
  0x00,                                                                                          // Parameter Revision
  0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // ParameterData
};

// Get Vlan Id return responses
UINT8  ValidResponseVLanId[] = {
  0x00,            // CompletionCode - Normal
  0x00,            // Parameter Revision
  0x00, 0x00,      // ParameterData
};

UINT8  InvalidCompletionVLanId[] = {
  0xC3,            // CompletionCode - Normal
  0x00,            // Parameter Revision
  0xFF, 0xEF,      // ParameterData
};

UINT8  DeviceFailureVLanId[] = {
  0xFF,            // CompletionCode - Normal
  0xFF,            // Parameter Revision
  0xFF, 0xFF,      // ParameterData
};

// Get IP Address Format return responses
UINT8  ValidResponseIpAddrFormat[] = {
  0x00,            // CompletionCode - Normal
  0x00,            // Parameter Revision
  0x01             // ParameterData
};

UINT8  InvalidCompletionIpAddrFormat[] = {
  0xC3,            // CompletionCode - Normal
  0x00,            // Parameter Revision
  0xFF             // ParameterData
};

UINT8  DeviceFailureIpAddrFormat[] = {
  0xFF,            // CompletionCode - Normal
  0xFF,            // Parameter Revision
  0xFF             // ParameterData
};

UINT8  IpSize     = sizeof (ValidResponseIPAddr);
UINT8  MacSize    = sizeof (ValidResponseMacAddr);
UINT8  VlanIdSize = sizeof (ValidResponseVLanId);

/**
  A simple unit test to test the RFHIGetUSBDescription function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
UsbDescVendor_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      UsbVendorId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUsbDesc, &DeviceFailureUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA), EFI_DEVICE_ERROR);

  Status = GetRFHIUSBDescription (&UsbVendorId, TYPE_VENDOR_ID);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetUSBDescription_Vendor function code
  path when an IPMI command returns a bad completion code.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
UsbDescVendor_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      UsbVendorId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUsbDesc, &InvalidCompletionUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIUSBDescription (&UsbVendorId, TYPE_VENDOR_ID);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetUSBDescription_Vendor function code
  path when an IPMI returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
UsbDescVendor_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      UsbVendorId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUsbDesc, &ValidResponseUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIUSBDescription (&UsbVendorId, TYPE_VENDOR_ID);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetUSBDescription_Product function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
UsbDescProduct_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      UsbProductId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUsbDesc, &DeviceFailureUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA), EFI_DEVICE_ERROR);

  Status = GetRFHIUSBDescription (&UsbProductId, TYPE_PRODUCT_ID);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetUSBDescription_Product function code
  path when an IPMI command returns a bad completion code.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
UsbDescProduct_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      UsbProductId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUsbDesc, &InvalidCompletionUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIUSBDescription (&UsbProductId, TYPE_PRODUCT_ID);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetUSBDescription_Product function code
  path when an IPMI returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
UsbDescProduct_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      UsbProductId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUsbDesc, &ValidResponseUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUsbDesc, sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIUSBDescription (&UsbProductId, TYPE_PRODUCT_ID);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetUSBVirtualSerialNumber function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
SerialNum_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR8       SerialNum[SERIAL_NUMBER_MAX_LENGTH];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsSerNum, &DeviceFailureSerialNum, sizeof (IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsSerNum, sizeof (IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA), EFI_DEVICE_ERROR);

  Status = GetRFHIUSBVirtualSerialNumber (&SerialNum[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetUSBVirtualSerialNumber function code
  path when an IPMI command returns a bad completion code.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
SerialNum_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR8       SerialNum[SERIAL_NUMBER_MAX_LENGTH];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsSerNum, &InvalidCompletionSerialNum, sizeof (IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsSerNum, sizeof (IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIUSBVirtualSerialNumber (&SerialNum[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetUSBVirtualSerialNumber function
  code path when an IPMI returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
SerialNum_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR8       SerialNum[SERIAL_NUMBER_MAX_LENGTH];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsSerNum, &ValidResponseSerialNum, sizeof (IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsSerNum, sizeof (IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIUSBVirtualSerialNumber (&SerialNum[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetHostname function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test

     case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
Hostname_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR8       Hostname[HOSTNAME_MAX_LENGTH];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsHostname, &DeviceFailureHostname, sizeof (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsHostname, sizeof (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA), EFI_DEVICE_ERROR);

  Status = GetRFHIHostname (&Hostname[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetHostname function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
Hostname_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR8       Hostname[HOSTNAME_MAX_LENGTH];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsHostname, &InvalidCompletionHostname, sizeof (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsHostname, sizeof (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIHostname (&Hostname[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetHostname function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
Hostname_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR8       Hostname[HOSTNAME_MAX_LENGTH];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsHostname, &ValidResponseHostname, sizeof (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsHostname, sizeof (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIHostname (&Hostname[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetIpmiChannelNumber function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ChnlNum_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       ChnlNum;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsChnlNum, &DeviceFailureChnlNum, sizeof (IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsChnlNum, sizeof (IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA), EFI_DEVICE_ERROR);

  Status = GetRFHIIpmiChannelNumber (&ChnlNum);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetIpmiChannelNumber function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ChnlNum_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       ChnlNum;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsChnlNum, &InvalidCompletionChnlNum, sizeof (IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsChnlNum, sizeof (IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIIpmiChannelNumber (&ChnlNum);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetIpmiChannelNumber function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ChnlNum_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       ChnlNum;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsChnlNum, &ValidResponseChnlNum, sizeof (IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsChnlNum, sizeof (IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIIpmiChannelNumber (&ChnlNum);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetMACAddress function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
MacAddr_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       MacAddr[6];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsMacAddr, &DeviceFailureMacAddr, MacSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsMacAddr, MacSize, EFI_DEVICE_ERROR);

  Status = GetRFHIMACAddress (TestChannel, &MacAddr[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetMACAddress function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
MacAddr_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       MacAddr[6];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsMacAddr, &InvalidCompletionMacAddr, MacSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsMacAddr, MacSize, EFI_SUCCESS);

  Status = GetRFHIMACAddress (TestChannel, &MacAddr[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetMACAddress function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
MacAddr_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       MacAddr[6];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsMacAddr, &ValidResponseMacAddr, MacSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsMacAddr, MacSize, EFI_SUCCESS);

  Status = GetRFHIMACAddress (TestChannel, &MacAddr[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpDiscoveryType function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IpDiscoveryType_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpDiscType;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpDiscType, &DeviceFailureIpDiscType, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpDiscType, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE), EFI_DEVICE_ERROR);

  Status = GetRFHIIpDiscoveryType (TestChannel, &IpDiscType);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpDiscoveryType function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.

**/
UNIT_TEST_STATUS
EFIAPI
IpDiscoveryType_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpDiscType;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpDiscType, &InvalidCompletionIpDiscType, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpDiscType, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE), EFI_SUCCESS);

  Status = GetRFHIIpDiscoveryType (TestChannel, &IpDiscType);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpDiscoveryType function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IpDiscoveryType_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpDiscType;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpDiscType, &ValidResponseIpDiscType, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpDiscType, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE), EFI_SUCCESS);

  Status = GetRFHIIpDiscoveryType (TestChannel, &IpDiscType);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpAddress function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IPAddress_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpAddr[16];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpAddr, &DeviceFailureIPAddr, IpSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpAddr, IpSize, EFI_DEVICE_ERROR);

  Status = GetRFHIIpAddress (TestChannel, &IpAddr[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpAddress function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.

**/
UNIT_TEST_STATUS
EFIAPI
IPAddress_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpAddr[16];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpAddr, &InvalidCompletionIPAddr, IpSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpAddr, IpSize, EFI_SUCCESS);

  Status = GetRFHIIpAddress (TestChannel, &IpAddr[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpAddress function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IpAddress_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpAddr[16];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpAddr, &ValidResponseIPAddr, IpSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpAddr, IpSize, EFI_SUCCESS);

  Status = GetRFHIIpAddress (TestChannel, &IpAddr[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpMask function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IPMask_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpMask[16];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpMask, &DeviceFailureIPMask, IpSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpMask, IpSize, EFI_DEVICE_ERROR);

  Status = GetRFHIIpMask (TestChannel, &IpMask[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the  GetRFHIIpMask function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.

**/
UNIT_TEST_STATUS
EFIAPI
IPMask_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpMask[16];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpMask, &InvalidCompletionIPMask, IpSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpMask, IpSize, EFI_SUCCESS);

  Status = GetRFHIIpMask (TestChannel, &IpMask[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpMask function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IPMask_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpMask[16];

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpMask, &ValidResponseIPMask, IpSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpMask, IpSize, EFI_SUCCESS);

  Status = GetRFHIIpMask (TestChannel, &IpMask[0]);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIVlanId function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
VLanId_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      VlanId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsVlanId, &DeviceFailureVLanId, VlanIdSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsVlanId, VlanIdSize, EFI_DEVICE_ERROR);

  Status = GetRFHIVlanId (TestChannel, &VlanId);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the  GetRFHIVlanId function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.

**/
UNIT_TEST_STATUS
EFIAPI
VLanId_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      VLanId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsVlanId, &InvalidCompletionVLanId, VlanIdSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsVlanId, VlanIdSize, EFI_SUCCESS);

  Status = GetRFHIVlanId (TestChannel, &VLanId);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIVlanId function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
VLanId_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      VLanId;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsVlanId, &ValidResponseVLanId, VlanIdSize);
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsVlanId, VlanIdSize, EFI_SUCCESS);

  Status = GetRFHIVlanId (TestChannel, &VLanId);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetIpAddFormat function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IpAddrFormat_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpAddrFormat;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpAddrFormat, &DeviceFailureIpAddrFormat, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpAddrFormat, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE), EFI_DEVICE_ERROR);

  Status = RFHIGetIpAddFormat (TestChannel, &IpAddrFormat);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the  RFHIGetIpAddFormat function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.

**/
UNIT_TEST_STATUS
EFIAPI
IpAddrFormat_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpAddrFormat;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpAddrFormat, &InvalidCompletionIpAddrFormat, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpAddrFormat, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE), EFI_SUCCESS);

  Status = RFHIGetIpAddFormat (TestChannel, &IpAddrFormat);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetIpAddFormat function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IpAddrFormat_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       IpAddrFormat;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpAddrFormat, &ValidResponseVLanId, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpAddrFormat, sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE), EFI_SUCCESS);

  Status = RFHIGetIpAddFormat (TestChannel, &IpAddrFormat);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIIpPort function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IpPort_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      IpPort;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpPort, &DeviceFailureIpPort, sizeof (IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpPort, sizeof (IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA), EFI_DEVICE_ERROR);

  Status = GetRFHIIpPort (&IpPort);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the  RFHIGetIpPort function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.

**/
UNIT_TEST_STATUS
EFIAPI
IpPort_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      IpPort;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpPort, &InvalidCompletionIpPort, sizeof (IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpPort, sizeof (IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIIpPort (&IpPort);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the RFHIGetIpPort function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IpPort_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT16      IpPort;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsIpPort, &ValidResponseIpPort, sizeof (IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsIpPort, sizeof (IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIIpPort (&IpPort);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIUUID function code
  path when an IPMI failure occurs.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
Uuid_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    Uuid;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUuid, &DeviceFailureUuid, sizeof (IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUuid, sizeof (IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA), EFI_DEVICE_ERROR);

  Status = GetRFHIUUID (&Uuid);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIUUID function code
  path when an IPMI command returns bad completion data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.

**/
UNIT_TEST_STATUS
EFIAPI
Uuid_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    Uuid;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUuid, &InvalidCompletionUuid, sizeof (IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUuid, sizeof (IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIUUID (&Uuid);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the GetRFHIUUID function code
  path when an IPMI command returns valid data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
Uuid_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    Uuid;

  Status = EFI_SUCCESS;

  CopyMem (ResponseResultsUuid, &ValidResponseUuid, sizeof (IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResultsUuid, sizeof (IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA), EFI_SUCCESS);

  Status = GetRFHIUUID (&Uuid);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for the
  sample unit tests and run the unit tests.

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
EFI_STATUS
EFIAPI
SetupAndRunUnitTests (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      RedfishHI;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a: v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  ResponseResultsUsbDesc      = (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA *)AllocateZeroPool (sizeof (IPMI_GET_USB_DESCRIPTION_RESPONSE_DATA));
  ResponseResultsSerNum       = (IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA *)AllocateZeroPool (sizeof (IPMI_GET_USB_SERIAL_NUMBER_RESPONSE_DATA));
  ResponseResultsHostname     = (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA *)AllocateZeroPool (sizeof (IPMI_GET_REDFISH_SERVICE_HOSTNAME_RESPONSE_DATA));
  ResponseResultsChnlNum      = (IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA *)AllocateZeroPool (sizeof (IPMI_GET_IPMI_CHANNEL_NUMBER_RFHI_RESPONSE_DATA));
  ResponseResultsIpPort       = (IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA *)AllocateZeroPool (sizeof (IPMI_GET_REDFISH_SERVICE_IP_PORT_RESPONSE_DATA));
  ResponseResultsUuid         = (IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA *)AllocateZeroPool (sizeof (IPMI_GET_REDFISH_SERVICE_UUID_RESPONSE_DATA));
  ResponseResultsMacAddr      = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)AllocateZeroPool (MacSize);
  ResponseResultsIpDiscType   = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)AllocateZeroPool (sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE));
  ResponseResultsIpAddr       = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)AllocateZeroPool (IpSize);
  ResponseResultsIpMask       = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)AllocateZeroPool (IpSize);
  ResponseResultsIpAddrFormat = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)AllocateZeroPool (sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE));
  ResponseResultsVlanId       = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)AllocateZeroPool (VlanIdSize);

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to setup Test Framework. Exiting with status = %r\n", Status));
    ASSERT (FALSE);
    return Status;
  }

  //
  // Populate the Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&RedfishHI, Framework, "Redfish Host Interface Tests", "UnitTest.RedfishHI", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Redfish Host Interface Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  Status = AddTestCase (RedfishHI, "IPMI response fails, USB VendorID", "IpmiFailure", UsbDescVendor_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, USB VendorID", "BadCompletion", UsbDescVendor_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, USB VendorID", "ValidData", UsbDescVendor_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, USB ProductID", "IpmiFailure", UsbDescProduct_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, USB ProductID", "BadCompletion", UsbDescProduct_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, USB ProductID", "ValidData", UsbDescProduct_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Serial Number", "IpmiFailure", SerialNum_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Serial Number", "BadCompletion", SerialNum_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Serial Number", "ValidData", SerialNum_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Hostname", "IpmiFailure", Hostname_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Hostname", "BadCompletion", Hostname_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Hostname", "ValidData", Hostname_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Channel Number", "IpmiFailure", ChnlNum_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Channel Number", "BadCompletion", ChnlNum_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Channel Number", "ValidData", ChnlNum_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, MAC Address", "IpmiFailure", MacAddr_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, MAC Address", "BadCompletion", MacAddr_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, MAC Address", "ValidData", MacAddr_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Redfish IP Discovery Type", "IpmiFailure", IpDiscoveryType_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Redfish IP Discovery Type", "BadCompletion", IpDiscoveryType_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Redfish IP Discovery Type", "ValidData", IpDiscoveryType_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Redfish IP Address", "IpmiFailure", IPAddress_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Redfish IP Address", "BadCompletion", IPAddress_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Redfish IP Address", "ValidData", IpAddress_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Redfish IP Mask", "IpmiFailure", IPMask_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Redfish IP Mask", "BadCompletion", IPMask_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Redfish IP Mask", "ValidData", IPMask_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Redfish VLAN ID", "IpmiFailure", VLanId_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Redfish VLAN ID", "BadCompletion", VLanId_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Redfish VLAN ID", "ValidData", VLanId_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Redfish IP Address Format", "IpmiFailure", IpAddrFormat_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Redfish IP Address Format", "BadCompletion", IpAddrFormat_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Redfish IP Address Format", "ValidData", IpAddrFormat_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Redfish IP Port", "IpmiFailure", IpPort_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Redfish IP Port", "BadCompletion", IpPort_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Redfish IP Port", "ValidData", IpPort_ValidData, NULL, NULL, NULL);

  Status = AddTestCase (RedfishHI, "IPMI response fails, Redfish Service UUID", "IpmiFailure", Uuid_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "If Ipmi completion is bad, should gracefully fail, Redfish Service UUID", "BadCompletion", Uuid_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishHI, "Valid data, this should return EFI_SUCCESS, Redfish Service UUID", "ValidData", Uuid_ValidData, NULL, NULL, NULL);

  // Execute the tests.
  Status = RunAllTestSuites (Framework);
  // Free memory before exit
  FreePool (ResponseResultsUsbDesc);
  FreePool (ResponseResultsSerNum);
  FreePool (ResponseResultsHostname);
  FreePool (ResponseResultsChnlNum);
  FreePool (ResponseResultsIpPort);
  FreePool (ResponseResultsUuid);
  FreePool (ResponseResultsMacAddr);
  FreePool (ResponseResultsIpDiscType);
  FreePool (ResponseResultsIpAddr);
  FreePool (ResponseResultsIpMask);
  FreePool (ResponseResultsIpAddrFormat);
  FreePool (ResponseResultsVlanId);

  return Status;
}

/**
  Standard UEFI entry point for target based
  unit test execution from UEFI Shell.
**/
EFI_STATUS
EFIAPI
BaseLibUnitTestAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return SetupAndRunUnitTests ();
}

/**
  Standard POSIX C entry point for host based unit test execution.
**/
int
main (
  int   argc,
  char  *argv[]
  )
{
  return SetupAndRunUnitTests ();
}
