/** @file

  OEM Status code handling unit test

  SPDX-FileCopyrightText: Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <PiDxe.h>
#include <Uefi.h>
#include <Library/UnitTestLib.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>

#include <IndustryStandard/Ipmi.h>
#include <Protocol/IpmiTransportProtocol.h>
#include <Protocol/ReportStatusCodeHandler.h>

#include "../OemDescStatusCodeDxe.h"

#define UNIT_TEST_NAME     "OEM Send Description Test"
#define UNIT_TEST_VERSION  "1.0"

#define STUB_EFI_EVENT  ((EFI_EVENT) &mStubEvent)

#define MAX_STATUS_CODE_DATA_SIZE  1000

typedef struct {
  EFI_STATUS_CODE_TYPE     CodeType;
  EFI_STATUS_CODE_VALUE    Value;
  UINT16                   DataSize;
  UINT8                    *Data;
  UINT16                   IpmiReqSize;
  UINT8                    *IpmiReqData;
} OEM_DESC_TEST_DATA;

////////////////////////////////////////////////////////////////////////////////
// PRIVATE VARIABLES
////////////////////////////////////////////////////////////////////////////////

STATIC UINTN                     mStubEvent       = 0;
STATIC EFI_EVENT_NOTIFY          mIpmiNotify      = NULL;
STATIC UINTN                     mRegistration    = 0;
STATIC IPMI_TRANSPORT            mIpmiTransport   = { 0 };
STATIC EFI_RSC_HANDLER_PROTOCOL  mRsc             = { 0 };
STATIC EFI_BOOT_SERVICES         mBS              = { 0 };
STATIC EFI_RSC_HANDLER_CALLBACK  mOemDescCallback = NULL;
STATIC EFI_EVENT_NOTIFY          mNotifyFunction  = NULL;
STATIC EFI_STATUS_CODE_DATA      *mData           = NULL;

////////////////////////////////////////////////////////////////////////////////
// TEST DATA
////////////////////////////////////////////////////////////////////////////////

CHAR8  BinaryData[] = { 0xAA, 0x02, 0x16, 0x11, 0x40, 0x99 };

#define  SHORT_DESC_1  "0000:03:02.0"
UINT8  ShortDesc1Ipmi[] = {
  0x02, 0x00, 0x00, 0x40, 0x06, 0x00, 0x01, 0x02, 0x30, 0x30, 0x30, 0x30, 0x3A,
  0x30, 0x33, 0x3A, 0x30, 0x32, 0x2E, 0x30, 0x00
};

OEM_DESC_TEST_DATA  ShortDesc1 = {
  EFI_ERROR_CODE | EFI_ERROR_MINOR,
  EFI_IO_BUS_PCI | EFI_IOB_EC_CONTROLLER_ERROR,
  sizeof (SHORT_DESC_1),
  (UINT8 *)SHORT_DESC_1,
  sizeof (ShortDesc1Ipmi),
  ShortDesc1Ipmi
};

#define SHORT_DESC_2  "Line 1;\nLine 2"
UINT8  ShortDesc2Ipmi[] = {
  0x02, 0x00, 0x00, 0x40, 0x06, 0x00, 0x01, 0x02, 0x4C, 0x69, 0x6E, 0x65, 0x20,
  0x31, 0x3B, 0x00
};

OEM_DESC_TEST_DATA  ShortDesc2 = {
  EFI_ERROR_CODE | EFI_ERROR_MINOR,
  EFI_IO_BUS_PCI | EFI_IOB_EC_CONTROLLER_ERROR,
  sizeof (SHORT_DESC_2),
  (UINT8 *)SHORT_DESC_2,
  sizeof (ShortDesc2Ipmi),
  ShortDesc2Ipmi
};

#define SHORT_DESC_3  "123456789012345678"
UINT8  ShortDesc3Ipmi[] = {
  0x03, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x02, 0x31, 0x32, 0x33, 0x34, 0x35,
  0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x00
};

OEM_DESC_TEST_DATA  ShortDesc3 = {
  EFI_DEBUG_CODE,
  EFI_IO_BUS_PCI | EFI_IOB_EC_CONTROLLER_ERROR,
  sizeof (SHORT_DESC_3),
  (UINT8 *)SHORT_DESC_3,
  sizeof (ShortDesc3Ipmi),
  ShortDesc3Ipmi
};

#define LONG_DESC_1  "Secure Boot Failure - The device in SLOT X has failed authentication"
UINT8  LongDesc1Ipmi[] = {
  0x02, 0x00, 0x00, 0x90, 0x01, 0x10, 0x10, 0x03, 0x53, 0x65, 0x63, 0x75, 0x72,
  0x65, 0x20, 0x42, 0x6F, 0x6F, 0x74, 0x20, 0x46, 0x61, 0x69, 0x6C, 0x75, 0x72,
  0x65, 0x20, 0x2D, 0x20, 0x54, 0x68, 0x65, 0x20, 0x64, 0x65, 0x76, 0x69, 0x63,
  0x65, 0x20, 0x69, 0x6E, 0x20, 0x53, 0x4C, 0x4F, 0x54, 0x20, 0x58, 0x20, 0x68,
  0x61, 0x73, 0x20, 0x66, 0x61, 0x69, 0x6C, 0x65, 0x64, 0x20, 0x61, 0x75, 0x74,
  0x68, 0x65, 0x6E, 0x74, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x00
};

OEM_DESC_TEST_DATA  LongDesc1 = {
  EFI_ERROR_CODE | EFI_ERROR_UNRECOVERED,
  EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_DXE_BS_EC_INVALID_PASSWORD,
  sizeof (LONG_DESC_1),
  (UINT8 *)LONG_DESC_1,
  sizeof (LongDesc1Ipmi),
  LongDesc1Ipmi
};

#define LONG_DESC_2  "123456789012345678901234567890123456789012345678901234"
UINT8  LongDesc2Ipmi[] = {
  0x02, 0x00, 0x00, 0x80, 0x03, 0x10, 0x05, 0x00, 0x31, 0x32, 0x33, 0x34, 0x35,
  0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
  0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34,
  0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x00
};

OEM_DESC_TEST_DATA  LongDesc2 = {
  EFI_ERROR_CODE | EFI_ERROR_MAJOR,
  EFI_COMPUTING_UNIT_MEMORY | EFI_CU_HP_PC_MEMORY_CONTROLLER_INIT,
  sizeof (LONG_DESC_2),
  (UINT8 *)LONG_DESC_2,
  sizeof (LongDesc2Ipmi),
  LongDesc2Ipmi
};

UINT8  DevicePath1Bin[] = {
  0x01, 0x04, 0x14, 0x00, 0x2C, 0x43, 0x5A, 0x1E, 0x66, 0x04, 0x31, 0x4D, 0xB0,
  0x09, 0xD4, 0xD9, 0x23, 0x92, 0x71, 0xD3, 0x01, 0x03, 0x18, 0x00, 0x0B, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x46, 0x03, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x47,
  0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x1D, 0x05, 0x00, 0x00, 0x01, 0x05, 0x08,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0x04, 0x00, 0x4E, 0xAC, 0x08, 0x81,
  0x11, 0x9F, 0x59, 0x4D, 0x85, 0x0E, 0xE2, 0x1A, 0x52, 0x2C, 0x59, 0xB2, 0x00
};
UINT8  DevicePath1Ipmi[] = {
  0x02, 0x00, 0x00, 0x80, 0x03, 0x10, 0x05, 0x00, 0x56, 0x65, 0x6E, 0x48, 0x77,
  0x28, 0x31, 0x45, 0x35, 0x41, 0x34, 0x33, 0x32, 0x43, 0x2D, 0x30, 0x34, 0x36,
  0x36, 0x2D, 0x34, 0x44, 0x33, 0x31, 0x2D, 0x42, 0x30, 0x30, 0x39, 0x2D, 0x44,
  0x34, 0x44, 0x39, 0x32, 0x33, 0x39, 0x32, 0x37, 0x31, 0x44, 0x33, 0x29, 0x2F,
  0x4D, 0x65, 0x6D, 0x6F, 0x72, 0x79, 0x4D, 0x61, 0x70, 0x70, 0x65, 0x64, 0x28,
  0x30, 0x78, 0x42, 0x2C, 0x30, 0x78, 0x33, 0x34, 0x36, 0x30, 0x30, 0x30, 0x30,
  0x2C, 0x30, 0x78, 0x33, 0x34, 0x37, 0x46, 0x46, 0x46, 0x46, 0x29, 0x2F, 0x65,
  0x4D, 0x4D, 0x43, 0x28, 0x30, 0x78, 0x30, 0x29, 0x2F, 0x43, 0x74, 0x72, 0x6C,
  0x28, 0x30, 0x78, 0x30, 0x29, 0x00
};

OEM_DESC_TEST_DATA  DevicePath1 = {
  EFI_ERROR_CODE | EFI_ERROR_MAJOR,
  EFI_COMPUTING_UNIT_MEMORY | EFI_CU_HP_PC_MEMORY_CONTROLLER_INIT,
  sizeof (DevicePath1Bin),
  DevicePath1Bin,
  sizeof (DevicePath1Ipmi),
  DevicePath1Ipmi
};

////////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////////
EFI_STATUS
EFIAPI
OemDescStatusCodeDxeDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

////////////////////////////////////////////////////////////////////////////////
// MOCKED FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

/**
  Creates and returns a notification event and registers that event with all the protocol
  instances specified by ProtocolGuid.

  @param  ProtocolGuid    Supplies GUID of the protocol upon whose installation the event is fired.
  @param  NotifyTpl       Supplies the task priority level of the event notifications.
  @param  NotifyFunction  Supplies the function to notify when the event is signaled.
  @param  NotifyContext   The context parameter to pass to NotifyFunction.
  @param  Registration    A pointer to a memory location to receive the registration value.

  @return The notification event that was created.
**/
EFI_EVENT
EFIAPI
__wrap_EfiCreateProtocolNotifyEvent (
  IN  EFI_GUID          *ProtocolGuid,
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,
  IN  VOID              *NotifyContext   OPTIONAL,
  OUT VOID              **Registration
  )
{
  EFI_STATUS  Status = (EFI_STATUS)mock ();

  ASSERT (CompareGuid (&gIpmiTransportProtocolGuid, ProtocolGuid));
  ASSERT (NotifyFunction != NULL);
  ASSERT (Registration != NULL);

  if (EFI_ERROR (Status)) {
    return NULL;
  }

  mIpmiNotify   = NotifyFunction;
  *Registration = &mRegistration;
  return STUB_EFI_EVENT;
}

/**
  Closes an event.

  @param[in]  Event             The event to close.

  @retval EFI_SUCCESS           The event has been closed.
**/
EFI_STATUS
EFIAPI
MockedCloseEvent (
  IN EFI_EVENT  Event
  )
{
  mIpmiNotify = NULL;
  return EFI_SUCCESS;
}

/**
  Mocked version of EFI_RSC_HANDLER_REGISTER

  @param[in] Callback   A pointer to a function of type EFI_RSC_HANDLER_CALLBACK that is called when
                        a call to ReportStatusCode() occurs.
  @param[in] Tpl        TPL at which callback can be safely invoked.

  @retval  EFI_SUCCESS              Function was successfully registered.
  @retval  EFI_INVALID_PARAMETER    The callback function was NULL.
  @retval  EFI_OUT_OF_RESOURCES     The internal buffer ran out of space. No more functions can be
                                    registered.
  @retval  EFI_ALREADY_STARTED      The function was already registered. It can't be registered again.
**/
EFI_STATUS
EFIAPI
MockedRscHandlerRegister (
  IN EFI_RSC_HANDLER_CALLBACK  Callback,
  IN EFI_TPL                   Tpl
  )
{
  BOOLEAN  Success = (BOOLEAN)mock ();

  if (!Success) {
    return EFI_OUT_OF_RESOURCES;
  }

  mOemDescCallback = Callback;
  return EFI_SUCCESS;
}

/**
  Mocked version of EFI_RSC_HANDLER_UNREGISTER

  @param[in]  Callback  A pointer to a function of type EFI_RSC_HANDLER_CALLBACK that is to be
                        unregistered.

  @retval EFI_SUCCESS           The function was successfully unregistered.
  @retval EFI_INVALID_PARAMETER The callback function was NULL.
  @retval EFI_NOT_FOUND         The callback function was not found to be unregistered.
**/
EFI_STATUS
EFIAPI
MockedRscHandlerUnregister (
  IN EFI_RSC_HANDLER_CALLBACK  Callback
  )
{
  mOemDescCallback = NULL;
  return EFI_SUCCESS;
}

/**
  Mocked version of CreateEventEx

  @param[in]   Type             The type of event to create and its mode and attributes.
  @param[in]   NotifyTpl        The task priority level of event notifications,if needed.
  @param[in]   NotifyFunction   The pointer to the event's notification function, if any.
  @param[in]   NotifyContext    The pointer to the notification function's context; corresponds to parameter
                                Context in the notification function.
  @param[in]   EventGroup       The pointer to the unique identifier of the group to which this event belongs.
                                If this is NULL, then the function behaves as if the parameters were passed
                                to CreateEvent.
  @param[out]  Event            The pointer to the newly created event if the call succeeds; undefined
                                otherwise.

  @retval EFI_SUCCESS           The event structure was created.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The event could not be allocated.

**/
EFI_STATUS
EFIAPI
MockedCreateEventEx (
  IN       UINT32            Type,
  IN       EFI_TPL           NotifyTpl,
  IN       EFI_EVENT_NOTIFY  NotifyFunction OPTIONAL,
  IN CONST VOID              *NotifyContext OPTIONAL,
  IN CONST EFI_GUID          *EventGroup    OPTIONAL,
  OUT      EFI_EVENT         *Event
  )
{
  BOOLEAN  Success = (BOOLEAN)mock ();

  if (!Success) {
    return EFI_OUT_OF_RESOURCES;
  }

  mNotifyFunction = NotifyFunction;
  return EFI_SUCCESS;
}

/**
  Mocked version of submitting commands via Ipmi.

  @param[in]         This              This point for IPMI_TRANSPORT structure.
  @param[in]         NetFunction       Net function of the command.
  @param[in]         Lun               LUN
  @param[in]         Command           IPMI Command.
  @param[in]         RequestData       Command Request Data.
  @param[in]         RequestDataSize   Size of Command Request Data.
  @param[out]        ResponseData      Command Response Data.
  @param[in, out]    ResponseDataSize  Size of Command Response Data.

  @retval EFI_SUCCESS            The command byte stream was successfully submit to the device.
  @retval EFI_NOT_FOUND          The command was not successfully sent to the device.
  @retval EFI_NOT_READY          Ipmi Device is not ready for Ipmi command access.
  @retval EFI_DEVICE_ERROR       Ipmi Device hardware error.
  @retval EFI_TIMEOUT            The command time out.
  @retval EFI_UNSUPPORTED        The command was not successfully sent to the device.
  @retval EFI_OUT_OF_RESOURCES   The resource allcation is out of resource or data size error.
**/
EFI_STATUS
EFIAPI
MockedIpmiSubmitCommand (
  IN     IPMI_TRANSPORT  *This,
  IN     UINT8           NetFunction,
  IN     UINT8           Lun,
  IN     UINT8           Command,
  IN     UINT8           *RequestData,
  IN     UINT32          RequestDataSize,
  OUT    UINT8           *ResponseData,
  IN OUT UINT32          *ResponseDataSize
  )
{
  EFI_STATUS  Status;

  Status            = (EFI_STATUS)mock ();
  *ResponseDataSize = (UINT32)mock ();
  *ResponseData     = (UINT8)mock ();

  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (NetFunction == IPMI_NETFN_OEM);
  ASSERT (Command     == IPMI_CMD_OEM_SEND_DESCRIPTION);

  check_expected (RequestData);

  return EFI_SUCCESS;
}

/**
  Mocked version of EFI_LOCATE_PROTOCOL

  @param[in]  Protocol          Provides the protocol to search for.
  @param[in]  Registration      Optional registration key returned from
                                RegisterProtocolNotify().
  @param[out]  Interface        On return, a pointer to the first interface that matches Protocol and
                                Registration.

  @retval EFI_SUCCESS           A protocol instance matching Protocol was found and returned in
                                Interface.
  @retval EFI_NOT_FOUND         No protocol instances were found that match Protocol and
                                Registration.
  @retval EFI_INVALID_PARAMETER Interface is NULL.
                                Protocol is NULL.
**/
EFI_STATUS
EFIAPI
MockedLocateProtocol (
  IN  EFI_GUID  *Protocol,
  IN  VOID      *Registration  OPTIONAL,
  OUT VOID      **Interface
  )
{
  BOOLEAN  ProtocolFound = (BOOLEAN)mock ();

  if (!ProtocolFound) {
    return EFI_NOT_FOUND;
  }

  if (CompareGuid (&gEfiRscHandlerProtocolGuid, Protocol)) {
    mRsc.Register   = MockedRscHandlerRegister;
    mRsc.Unregister = MockedRscHandlerUnregister;
    *Interface      = &mRsc;
    return EFI_SUCCESS;
  }

  if (CompareGuid (&gIpmiTransportProtocolGuid, Protocol)) {
    mIpmiTransport.IpmiSubmitCommand = MockedIpmiSubmitCommand;
    *Interface                       = &mIpmiTransport;
    return EFI_SUCCESS;
  }

  UT_ASSERT_TRUE (FALSE);
  return EFI_NOT_FOUND;
}

/**
  Returns the debug print error level mask for the current module.

  @return  Debug print error level mask for the current module.
**/
UINT32
EFIAPI
__wrap_GetDebugPrintErrorLevel (
  VOID
  )
{
  return (UINT32)mock ();
}

////////////////////////////////////////////////////////////////////////////////
// TEST CASES
////////////////////////////////////////////////////////////////////////////////

/**
  Clean up after each test case

  @param[in]  Context                      Unit test context
  @retval  UNIT_TEST_PASSED                Test case cleanup succeeded.
  @retval  UNIT_TEST_ERROR_CLEANUP_FAILED  Test case cleanup failed.
**/
VOID
EFIAPI
OemDescTestCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
}

/**
  Test error handling paths of OemDescStatusCodeDxeDriverEntryPoint

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescInitErrors (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  gBS                 = &mBS;
  gBS->CreateEventEx  = MockedCreateEventEx;
  gBS->LocateProtocol = MockedLocateProtocol;
  gBS->CloseEvent     = MockedCloseEvent;

  //
  // Test case: fail to locate RscHandlerProtocol
  //
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedLocateProtocol, FALSE);
  Status = OemDescStatusCodeDxeDriverEntryPoint (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_NOT_FOUND);
  //
  // Test case: fail to register ReportStatusCode callback
  //
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedLocateProtocol, TRUE);
  will_return (MockedRscHandlerRegister, FALSE);
  Status = OemDescStatusCodeDxeDriverEntryPoint (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_OUT_OF_RESOURCES);
  //
  // Test case: fail to create notify ExitBootServices event
  //
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedLocateProtocol, TRUE);
  will_return (MockedRscHandlerRegister, TRUE);
  will_return (MockedCreateEventEx, FALSE);
  Status = OemDescStatusCodeDxeDriverEntryPoint (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_OUT_OF_RESOURCES);
  UT_ASSERT_TRUE (mIpmiNotify != NULL);

  return UNIT_TEST_PASSED;
}

/**
  Initialize OemDescStatusCodeDxe driver to get callback from REPORT_STATUS_CODE

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescInitSuccess (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  gBS                 = &mBS;
  gBS->CreateEventEx  = MockedCreateEventEx;
  gBS->LocateProtocol = MockedLocateProtocol;

  mData = AllocatePool (sizeof (EFI_STATUS_CODE_DATA) + MAX_STATUS_CODE_DATA_SIZE);
  ASSERT (mData != NULL);

  mData->HeaderSize = sizeof (EFI_STATUS_CODE_DATA);
  CopyGuid (&mData->Type, &gEfiStatusCodeSpecificDataGuid);

  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedLocateProtocol, TRUE);
  will_return (MockedRscHandlerRegister, TRUE);
  will_return (MockedCreateEventEx, TRUE);
  Status = OemDescStatusCodeDxeDriverEntryPoint (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_TRUE (mIpmiNotify != NULL);

  return UNIT_TEST_PASSED;
}

/**
  Send status code before IPMI driver is loaded

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescSendBeforeIpmi (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  OEM_DESC_TEST_DATA  *TestData[] = { &ShortDesc1, &ShortDesc2, &ShortDesc3, &LongDesc1, &LongDesc2 };
  EFI_STATUS          Status;
  UINTN               Index;

  for (Index = 0; Index < ARRAY_SIZE (TestData); Index++) {
    mData->Size = TestData[Index]->DataSize;
    CopyMem (mData + 1, TestData[Index]->Data, mData->Size);

    will_return (__wrap_GetDebugPrintErrorLevel, DEBUG_ERROR);
    ASSERT (mOemDescCallback != NULL);
    Status = mOemDescCallback (TestData[Index]->CodeType, TestData[Index]->Value, 0, NULL, mData);
    UT_ASSERT_NOT_EFI_ERROR (Status);
  }

  return UNIT_TEST_PASSED;
}

/**
  Send all staged messages once IPMI driver is loaded

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescSendWhenIpmiLoaded (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  OEM_DESC_TEST_DATA  *TestData[] = { &LongDesc1, &LongDesc2 };
  UINTN               Index;

  for (Index = 0; Index < ARRAY_SIZE (TestData); Index++) {
    will_return (MockedIpmiSubmitCommand, EFI_SUCCESS);
    will_return (MockedIpmiSubmitCommand, sizeof (IPMI_OEM_SEND_DESC_RSP_DATA));
    will_return (MockedIpmiSubmitCommand, IPMI_COMP_CODE_NORMAL);
    expect_memory (MockedIpmiSubmitCommand, RequestData, TestData[Index]->IpmiReqData, TestData[Index]->IpmiReqSize);
  }

  will_return (MockedLocateProtocol, TRUE);
  mIpmiNotify (NULL, NULL);

  return UNIT_TEST_PASSED;
}

/**
  Send status code with no description

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescSendNone (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_STATUS_CODE_TYPE   CodeType;
  EFI_STATUS_CODE_VALUE  Value;

  CodeType    = EFI_PROGRESS_CODE;
  Value       = EFI_IO_BUS_PCI | EFI_IOB_PCI_BUS_ENUM;
  mData->Size = 0;

  ASSERT (mOemDescCallback != NULL);
  Status = mOemDescCallback (CodeType, Value, 0, NULL, mData);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  For status code that has binary data, do not send OEM description

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescSendBinary (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_STATUS_CODE_TYPE   CodeType;
  EFI_STATUS_CODE_VALUE  Value;

  CodeType    = EFI_PROGRESS_CODE;
  Value       = EFI_IO_BUS_PCI | EFI_IOB_PCI_BUS_ENUM;
  mData->Size = sizeof (BinaryData);
  CopyMem (mData + 1, BinaryData, mData->Size);

  will_return (__wrap_GetDebugPrintErrorLevel, DEBUG_ERROR | DEBUG_WARN | DEBUG_INFO | DEBUG_VERBOSE);

  ASSERT (mOemDescCallback != NULL);
  Status = mOemDescCallback (CodeType, Value, 0, NULL, mData);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Send status code with short description

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescSendContext (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  OEM_DESC_TEST_DATA  *TestData = (OEM_DESC_TEST_DATA *)Context;
  EFI_STATUS          Status;

  mData->Size = TestData->DataSize;
  CopyMem (mData + 1, TestData->Data, mData->Size);

  will_return (MockedIpmiSubmitCommand, EFI_SUCCESS);
  will_return (MockedIpmiSubmitCommand, sizeof (IPMI_OEM_SEND_DESC_RSP_DATA));
  will_return (MockedIpmiSubmitCommand, IPMI_COMP_CODE_NORMAL);
  expect_memory (MockedIpmiSubmitCommand, RequestData, TestData->IpmiReqData, TestData->IpmiReqSize);

  will_return (__wrap_GetDebugPrintErrorLevel, DEBUG_ERROR | DEBUG_WARN | DEBUG_INFO | DEBUG_VERBOSE);

  ASSERT (mOemDescCallback != NULL);
  Status = mOemDescCallback (TestData->CodeType, TestData->Value, 0, NULL, mData);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Bypass sending description based on debug level

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescFilterSend (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  mData->Size = ShortDesc1.DataSize;
  CopyMem (mData + 1, ShortDesc1.Data, mData->Size);

  ASSERT (mOemDescCallback != NULL);

  //
  // Disable DEBUG_ERROR, and verify that no description sent for major errors
  //
  will_return (__wrap_GetDebugPrintErrorLevel, ~DEBUG_ERROR);
  Status = mOemDescCallback (EFI_ERROR_CODE | EFI_ERROR_MAJOR, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  //
  // Disable DEBUG_INFO, and verify that no description sent for minor errors
  //
  will_return (__wrap_GetDebugPrintErrorLevel, ~DEBUG_INFO);
  Status = mOemDescCallback (EFI_ERROR_CODE | EFI_ERROR_MINOR, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  //
  // Disable DEBUG_INFO, and verify that no description sent for progress code
  //
  will_return (__wrap_GetDebugPrintErrorLevel, ~DEBUG_INFO);
  Status = mOemDescCallback (EFI_PROGRESS_CODE, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  //
  // Disable DEBUG_VERBOSE, and verify that no description sent for debug code
  //
  will_return (__wrap_GetDebugPrintErrorLevel, ~DEBUG_VERBOSE);
  Status = mOemDescCallback (EFI_DEBUG_CODE, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Attempt to send description, but BMC does not support

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescBmcNotSupport (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  mData->Size = ShortDesc1.DataSize;
  CopyMem (mData + 1, ShortDesc1.Data, mData->Size);

  will_return (MockedIpmiSubmitCommand, EFI_SUCCESS);
  will_return (MockedIpmiSubmitCommand, sizeof (IPMI_OEM_SEND_DESC_RSP_DATA));
  will_return (MockedIpmiSubmitCommand, IPMI_COMP_CODE_INVALID_COMMAND);
  expect_memory (MockedIpmiSubmitCommand, RequestData, ShortDesc1.IpmiReqData, ShortDesc1.IpmiReqSize);
  will_return (__wrap_GetDebugPrintErrorLevel, DEBUG_ERROR | DEBUG_WARN | DEBUG_INFO | DEBUG_VERBOSE);

  ASSERT (mOemDescCallback != NULL);
  Status = mOemDescCallback (ShortDesc1.CodeType, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_TRUE (Status == EFI_UNSUPPORTED);
  //
  // In the case BMC does not support OEM send description, the callback is
  // expected to be disabled. Successive calls will do nothing.
  //
  Status = mOemDescCallback (ShortDesc1.CodeType, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_TRUE (Status == EFI_UNSUPPORTED);
  //
  // Reinitialize to re-register the callback
  //
  OemDescInitSuccess (Context);

  return UNIT_TEST_PASSED;
}

/**
  Send description, but receive IPMI error

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescReceiveIpmiError (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  mData->Size = ShortDesc1.DataSize;
  CopyMem (mData + 1, ShortDesc1.Data, mData->Size);

  will_return (MockedIpmiSubmitCommand, EFI_TIMEOUT);
  will_return (MockedIpmiSubmitCommand, sizeof (IPMI_OEM_SEND_DESC_RSP_DATA));
  will_return (MockedIpmiSubmitCommand, IPMI_COMP_CODE_INVALID_COMMAND);
  will_return (__wrap_GetDebugPrintErrorLevel, DEBUG_ERROR | DEBUG_WARN | DEBUG_INFO | DEBUG_VERBOSE);

  ASSERT (mOemDescCallback != NULL);
  Status = mOemDescCallback (ShortDesc1.CodeType, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_TRUE (Status == EFI_TIMEOUT);

  return UNIT_TEST_PASSED;
}

/**
  Send description, but receive incorrect response data

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescReceiveWrongResponse (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  mData->Size = ShortDesc1.DataSize;
  CopyMem (mData + 1, ShortDesc1.Data, mData->Size);

  will_return (MockedIpmiSubmitCommand, EFI_SUCCESS);
  will_return (MockedIpmiSubmitCommand, 5);
  will_return (MockedIpmiSubmitCommand, IPMI_COMP_CODE_INVALID_COMMAND);
  expect_memory (MockedIpmiSubmitCommand, RequestData, ShortDesc1.IpmiReqData, ShortDesc1.IpmiReqSize);
  will_return (__wrap_GetDebugPrintErrorLevel, DEBUG_ERROR | DEBUG_WARN | DEBUG_INFO | DEBUG_VERBOSE);

  ASSERT (mOemDescCallback != NULL);
  Status = mOemDescCallback (ShortDesc1.CodeType, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  Send description, but receive error completion code

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescReceiveErrorCode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  mData->Size = ShortDesc1.DataSize;
  CopyMem (mData + 1, ShortDesc1.Data, mData->Size);

  will_return (MockedIpmiSubmitCommand, EFI_SUCCESS);
  will_return (MockedIpmiSubmitCommand, sizeof (IPMI_OEM_SEND_DESC_RSP_DATA));
  will_return (MockedIpmiSubmitCommand, IPMI_COMP_CODE_BMC_INIT_IN_PROGRESS);
  expect_memory (MockedIpmiSubmitCommand, RequestData, ShortDesc1.IpmiReqData, ShortDesc1.IpmiReqSize);
  will_return (__wrap_GetDebugPrintErrorLevel, DEBUG_ERROR | DEBUG_WARN | DEBUG_INFO | DEBUG_VERBOSE);

  ASSERT (mOemDescCallback != NULL);
  Status = mOemDescCallback (ShortDesc1.CodeType, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  Trigger ExitBootServices, OemDescStatusCode will be disabled

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
OemDescTriggerExitBootServices (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  ASSERT (mNotifyFunction != NULL);
  mNotifyFunction (NULL, NULL);
  //
  // After ExitBootServices. Successive calls to send OEM description will do nothing.
  //
  Status = mOemDescCallback (ShortDesc1.CodeType, ShortDesc1.Value, 0, NULL, mData);
  UT_ASSERT_TRUE (Status == EFI_UNSUPPORTED);

  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for OEM Send
  Description unit tests and run the unit tests.

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
  UNIT_TEST_SUITE_HANDLE      OemDescTests;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a: v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to setup Test Framework. Exiting with status = %r\n", Status));
    ASSERT (FALSE);
    return Status;
  }

  //
  // Populate the OEM Send Description Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&OemDescTests, Framework, "OEM Send Description Tests", "UnitTest.OemSendDesc", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for OEM Send Description Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  Status = AddTestCase (OemDescTests, "Error handling cases during driver entry point", "DriverInitErrors", OemDescInitErrors, NULL, NULL, NULL);
  Status = AddTestCase (OemDescTests, "Driver initializes successfully", "DriverInitSuccess", OemDescInitSuccess, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send error codes before IPMI driver is loaded", "SendBeforeIpmi", OemDescSendBeforeIpmi, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send all staged messages as soon as IPMI driver is loaded", "SendWhenIpmiLoaded", OemDescSendWhenIpmiLoaded, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send progress code with no description", "SendNoDescription", OemDescSendNone, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send progress code with binary data", "SendCodeWithBinary", OemDescSendBinary, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send short description", "SendShortDescription", OemDescSendContext, NULL, OemDescTestCleanup, &ShortDesc1);
  Status = AddTestCase (OemDescTests, "Send description that has new-line charaters", "SendDescWithNewLine", OemDescSendContext, NULL, OemDescTestCleanup, &ShortDesc2);
  Status = AddTestCase (OemDescTests, "Send description that has 18 charaters", "Send18Chars", OemDescSendContext, NULL, OemDescTestCleanup, &ShortDesc3);
  Status = AddTestCase (OemDescTests, "Send long description", "SendLongDescription", OemDescSendContext, NULL, OemDescTestCleanup, &LongDesc1);
  Status = AddTestCase (OemDescTests, "Send description that has 54 charaters", "Send54Chars", OemDescSendContext, NULL, OemDescTestCleanup, &LongDesc2);
  Status = AddTestCase (OemDescTests, "Send device path", "SendDevicePath", OemDescSendContext, NULL, OemDescTestCleanup, &DevicePath1);
  Status = AddTestCase (OemDescTests, "Filter sending description based on debug level", "FilterSend", OemDescFilterSend, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send description, but BMC does not support", "BmcNotSupport", OemDescBmcNotSupport, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send description, but receive IPMI error", "ReceiveIpmiError", OemDescReceiveIpmiError, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send description, but receive incorrect response data", "ReceiveWrongResponse", OemDescReceiveWrongResponse, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Send description, but receive error completion code", "ReceiveErrorCode", OemDescReceiveErrorCode, NULL, OemDescTestCleanup, NULL);
  Status = AddTestCase (OemDescTests, "Trigger ExitBootServices, OemDescStatusCode will be disabled", "TriggerExitBootServices", OemDescTriggerExitBootServices, NULL, NULL, NULL);

  // Execute the tests.
  Status = RunAllTestSuites (Framework);

  FreePool (mData);

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
