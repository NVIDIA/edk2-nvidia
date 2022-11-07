/** @file
  Unit tests of the Redfish bootstrap credential library.

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "CredentialBootstrapUnitTest.h"

#define UNIT_TEST_NAME     "Credential Bootstrapping Test"
#define UNIT_TEST_VERSION  "1.0"
#define USERNAME_STRING    "AAAAAAAAAAAAAAAA"
#define PASSWORD_STRING    "BBBBBBBBBBBBBBBB"
#define EMPTY_STRING       ""
#define USERNAME_SHORT     "A"
#define PASSWORD_SHORT     "B"

IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE  *ResponseResults = NULL;

IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE  ValidResponse = {
  0x00, // CompletionCode
  0x52, // GroupExtensionId
  { 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41 }, // Username
  { 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42 }, // Password
};

IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE  InvalidCompletion = {
  0xC0, // CompletionCode = Node Busy
  0x52, // GroupExtensionId
  { 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41 }, // Username
  { 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42 }, // Password
};

IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE  InvalidGroup = {
  0x00, // CompletionCode
  0x53, // Invalid GroupExtensionId
  { 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41 }, // Username
  { 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42 }, // Password
};

IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE  DeviceFailure = {
  0xFF, // CompletionCode
  0xFF, // Invalid GroupExtensionId
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, // Username
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, // Password
};

IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE  EmptyUsernamePassword = {
  0x00, // CompletionCode
  0x52, // GroupExtensionId
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // Empty Username
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // Empty Password
};

IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE  ShortUsernamePassword = {
  0x00, // CompletionCode
  0x52, // GroupExtensionId
  { 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // Short Username
  { 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // Short Password
};

/**
  A simple unit test to test the code path when an IPMI failure occurs

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
RCBS_IpmiFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &DeviceFailure, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_DEVICE_ERROR);

  UT_EXPECT_ASSERT_FAILURE (LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password), NULL);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the code path when an IPMI command returns a bad completion code

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
RCBS_BadCompletion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                 Status;
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Status   = EFI_SUCCESS;
  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &InvalidCompletion, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_SUCCESS);

  Status = LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_PROTOCOL_ERROR);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the code path when an IPMI command returns the wrong group ext

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
RCBS_WrongGroupExtension (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                 Status;
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Status   = EFI_SUCCESS;
  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &InvalidGroup, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_SUCCESS);

  Status = LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the code path when an IPMI returns valid data

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
RCBS_ValidData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                 Status;
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Status   = EFI_SUCCESS;
  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &ValidResponse, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_SUCCESS);

  Status = LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (AsciiStrCmp (Username, USERNAME_STRING), 0x00);
  UT_ASSERT_EQUAL (AsciiStrCmp (Password, PASSWORD_STRING), 0x00);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the code path when an IPMI returns empty username
  and password.

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
RCBS_EmptyUsernamePassword (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                 Status;
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Status   = EFI_SUCCESS;
  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &EmptyUsernamePassword, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_SUCCESS);

  Status = LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (AsciiStrCmp (Username, EMPTY_STRING), 0x00);
  UT_ASSERT_EQUAL (AsciiStrCmp (Password, EMPTY_STRING), 0x00);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the code path when an IPMI returns short username
  and password.

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
RCBS_ShortUsernamePassword (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                 Status;
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Status   = EFI_SUCCESS;
  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &ShortUsernamePassword, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_SUCCESS);

  Status = LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (AsciiStrCmp (Username, USERNAME_SHORT), 0x00);
  UT_ASSERT_EQUAL (AsciiStrCmp (Password, PASSWORD_SHORT), 0x00);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the code path when redfish credentials service was stopped.

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
RCBS_CredentialsServiceStopped (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                 Status;
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Status   = EFI_SUCCESS;
  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &ValidResponse, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_SUCCESS);

  LibCredentialExitBootServicesNotify (NULL);
  Status = LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_ACCESS_DENIED);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the code path when redfish credentials are successfully retrieved

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
RCBS_CredentialsSuccessfullyRetrieved (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                 Status;
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Status   = EFI_SUCCESS;
  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &ValidResponse, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_SUCCESS);

  Status = LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

  return UNIT_TEST_PASSED;
}

/**
  A simple unit test to test the Entrypoint function when Ipmi is not working

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
RCBS_EntryIpmiFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR8                      *Username;
  CHAR8                      *Password;
  EDKII_REDFISH_AUTH_METHOD  AuthMethod;

  Username = NULL;
  Password = NULL;

  CopyMem (ResponseResults, &InvalidGroup, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));
  MockIpmiSubmitCommand ((UINT8 *)ResponseResults, sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE), EFI_NOT_FOUND);

  UT_EXPECT_ASSERT_FAILURE (LibCredentialGetAuthInfo (NULL, &AuthMethod, &Username, &Password), NULL);

  FREE_NON_NULL (Username);
  FREE_NON_NULL (Password);

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
  UNIT_TEST_SUITE_HANDLE      RedfishCB;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a: v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  ResponseResults = (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE *)AllocateZeroPool (sizeof (IPMI_BOOTSTRAP_CREDENTIALS_RESULT_RESPONSE));

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to setup Test Framework. Exiting with status = %r\n", Status));
    ASSERT (FALSE);
    return Status;
  }

  //
  // Populate the Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&RedfishCB, Framework, "Redfish Credential Bootstrapping Tests", "UnitTest.RedfishCB", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Redfish Credential Bootstrap Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  Status = AddTestCase (RedfishCB, "IPMI response fails", "IpmiFailure", RCBS_IpmiFailure, NULL, NULL, NULL);
  Status = AddTestCase (RedfishCB, "If Ipmi completion is bad, should gracefully fail", "BadCompletion", RCBS_BadCompletion, NULL, NULL, NULL);
  Status = AddTestCase (RedfishCB, "If Ipmi response is the wrong group extension id, should gracefully fail", "WrongGroup", RCBS_WrongGroupExtension, NULL, NULL, NULL);
  Status = AddTestCase (RedfishCB, "Valid data, this should return EFI_SUCCESS", "ValidData", RCBS_ValidData, NULL, NULL, NULL);
  Status = AddTestCase (RedfishCB, "Valid data with empty user name and password, this should return EFI_SUCCESS", "ValidData", RCBS_EmptyUsernamePassword, NULL, NULL, NULL);
  Status = AddTestCase (RedfishCB, "Valid data with short user name and password, this should return EFI_SUCCESS", "ValidData", RCBS_ShortUsernamePassword, NULL, NULL, NULL);
  Status = AddTestCase (RedfishCB, "LibCredentialGetAuthInfo test: Credentials successfully retrieved", "CredsRetrieved", RCBS_CredentialsSuccessfullyRetrieved, NULL, NULL, NULL);
  Status = AddTestCase (RedfishCB, "LibCredentialGetAuthInfo test: IPMI fails", "EntryIpmiFails", RCBS_EntryIpmiFails, NULL, NULL, NULL);
  Status = AddTestCase (RedfishCB, "LibCredentialGetAuthInfo test: Credential service was stopped", "CredsStop", RCBS_CredentialsServiceStopped, NULL, NULL, NULL);

  //
  // Execute the tests.
  //
  Status = RunAllTestSuites (Framework);

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
