/** @file

  Nuvoton RTC Unit Test

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>

#include <Protocol/I2cIo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/RealTimeClockLib.h>
#include <Library/TimeBaseLib.h>
#include <Guid/RtPropertiesTable.h>

#include <HostBasedTestStubLib/PcdStubLib.h>

#include "../NuvotonRealTimeClockLib.h"

#define UNIT_TEST_NAME     "Nuvoton RTC Lib Test"
#define UNIT_TEST_VERSION  "1.0"

#define STUB_EFI_EVENT  ((EFI_EVENT) &mStubEvent)

#define FIRST_GET_TIME_PERF_COUNT   0x210041055ull
#define FIRST_SET_TIME_PERF_COUNT   0x210081222ull
#define SECOND_SET_TIME_PERF_COUNT  0x310081000ull
#define SECOND_GET_TIME_PERF_COUNT  0x210001000ull
#define MAX_I2C_LEN                 32

typedef struct {
  UINT8    OperationCount;
  struct {
    UINT8    Flags;
    UINT8    LengthInBytes;
    UINT8    Buffer[MAX_I2C_LEN];
  } Operation[2];
} EXPECTED_I2C_REQUEST;

////////////////////////////////////////////////////////////////////////////////
// PRIVATE VARIABLES
////////////////////////////////////////////////////////////////////////////////

STATIC UINTN                    mStubEvent    = 0;
STATIC UINTN                    mRegistration = 0;
STATIC EFI_BOOT_SERVICES        mBS           = { 0 };
STATIC EFI_EVENT_NOTIFY         mI2cIoNotify  = NULL;
STATIC EFI_EVENT_NOTIFY         mExitBsNotify = NULL;
STATIC EFI_RT_PROPERTIES_TABLE  mRtProperties;
STATIC EFI_I2C_IO_PROTOCOL      mI2cIo     = { 0 };
STATIC INT64                    mRtcOffset = 0;

////////////////////////////////////////////////////////////////////////////////
// MOCKED FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

/**
  Mocked for Queue an I2C transaction for execution on the I2C controller.

  @param[in] This               Pointer to an EFI_I2C_IO_PROTOCOL structure.
  @param[in] SlaveAddressIndex  Index value into an array of slave addresses
  @param[in] Event              Event to signal for asynchronous transactions
  @param[in] RequestPacket      Pointer to an EFI_I2C_REQUEST_PACKET structure
                                describing the I2C transaction
  @param[out] I2cStatus         Optional buffer to receive the I2C completion status

  @retval EFI_SUCCESS           The transaction completed successfully
  @retval EFI_DEVICE_ERROR      There was an I2C error (NACK)
**/
EFI_STATUS
EFIAPI
MockedI2cIoProtocolQueueRequest (
  IN CONST EFI_I2C_IO_PROTOCOL  *This,
  IN UINTN                      SlaveAddressIndex,
  IN EFI_EVENT                  Event      OPTIONAL,
  IN EFI_I2C_REQUEST_PACKET     *RequestPacket,
  OUT EFI_STATUS                *I2cStatus OPTIONAL
  )
{
  EFI_STATUS  Status = (EFI_STATUS)mock ();

  ASSERT (This == &mI2cIo);
  ASSERT (Event == NULL);
  ASSERT (RequestPacket != NULL);
  ASSERT (I2cStatus == NULL);

  check_expected (RequestPacket);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Cmocka check_function to verify that RequestPacket is expected

  @param[in] Value           RequestPacket
  @param[in] CheckValueData  Expected value of RequestPacket

  @retval 1      Value is expected
  @retval 0      Value is not expected
**/
INT32
CheckMockedQueueRequest (
  IN CONST LargestIntegralType  Value,
  IN CONST LargestIntegralType  CheckValueData
  )
{
  EFI_I2C_REQUEST_PACKET  *RequestPacket  = (EFI_I2C_REQUEST_PACKET *)Value;
  EXPECTED_I2C_REQUEST    *ExpectedPacket = (EXPECTED_I2C_REQUEST *)CheckValueData;
  UINT32                  Index;

  if (RequestPacket->OperationCount != ExpectedPacket->OperationCount) {
    return 0;
  }

  for (Index = 0; Index < ExpectedPacket->OperationCount; Index++) {
    if (RequestPacket->Operation[Index].Flags != ExpectedPacket->Operation[Index].Flags) {
      return 0;
    }

    if (RequestPacket->Operation[Index].LengthInBytes != ExpectedPacket->Operation[Index].LengthInBytes) {
      return 0;
    }

    // Verify that the write bytes (including the I2C register offset) are expected
    if ((Index == 0) && (RequestPacket->Operation[Index].Flags == 0)) {
      if (CompareMem (
            RequestPacket->Operation[Index].Buffer,
            ExpectedPacket->Operation[Index].Buffer,
            RequestPacket->Operation[Index].LengthInBytes
            ) != 0)
      {
        return 0;
      }
    }

    // Copy read data over
    if ((Index > 0) && (RequestPacket->Operation[Index].Flags == I2C_FLAG_READ)) {
      CopyMem (
        RequestPacket->Operation[Index].Buffer,
        ExpectedPacket->Operation[Index].Buffer,
        RequestPacket->Operation[Index].LengthInBytes
        );
    }
  }

  return 1;
}

/**
  Mocked for the UEFI Runtime Service GetVariable().

  @param  VariableName the name of the vendor's variable
  @param  VendorGuid   Unify identifier for vendor.
  @param  Attributes   Point to memory location to return the attributes of variable.
  @param  DataSize     As input, point to the maximum size of return Data-Buffer.
  @param  Data         Point to return Data-Buffer.

  @retval  EFI_SUCCESS            The function completed successfully.
  @retval  EFI_NOT_FOUND          The variable was not found.
**/
EFI_STATUS
EFIAPI
__wrap_EfiGetVariable (
  IN      CHAR16    *VariableName,
  IN      EFI_GUID  *VendorGuid,
  OUT     UINT32    *Attributes OPTIONAL,
  IN OUT  UINTN     *DataSize,
  OUT     VOID      *Data
  )
{
  EFI_STATUS  Status = (EFI_STATUS)mock ();

  ASSERT (StrCmp (VariableName, L"RTC_OFFSET") == 0);
  ASSERT (CompareGuid (VendorGuid, &gNVIDIATokenSpaceGuid));
  ASSERT (Attributes == NULL);
  ASSERT (*DataSize == sizeof (mRtcOffset));

  if (EFI_ERROR (Status)) {
    return Status;
  }

  *(INT64 *)Data = mRtcOffset;
  return EFI_SUCCESS;
}

/**
  Mocked for the UEFI Runtime Service GetNextVariableName()

  @param  VariableName the name of the vendor's variable
  @param  VendorGuid   Unify identifier for vendor.
  @param  Attributes   the attributes of variable.
  @param  DataSize     The size in bytes of Data-Buffer.
  @param  Data         Point to the content of the variable.

  @retval  EFI_SUCCESS            The firmware has successfully stored the variable
  @retval  EFI_INVALID_PARAMETER  An invalid combination of attribute bits was supplied
  @retval  EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the variable and its data.
**/
EFI_STATUS
EFIAPI
__wrap_EfiSetVariable (
  IN CHAR16    *VariableName,
  IN EFI_GUID  *VendorGuid,
  IN UINT32    Attributes,
  IN UINTN     DataSize,
  IN VOID      *Data
  )
{
  EFI_STATUS  Status = (EFI_STATUS)mock ();

  ASSERT (StrCmp (VariableName, L"RTC_OFFSET") == 0);
  ASSERT (CompareGuid (VendorGuid, &gNVIDIATokenSpaceGuid));
  ASSERT (Attributes == (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS));
  ASSERT (DataSize == sizeof (mRtcOffset));

  if (EFI_ERROR (Status)) {
    return Status;
  }

  mRtcOffset = *(INT64 *)Data;
  return EFI_SUCCESS;
}

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

  ASSERT (CompareGuid (&gEfiI2cIoProtocolGuid, ProtocolGuid));
  ASSERT (NotifyFunction != NULL);
  ASSERT (Registration != NULL);

  if (EFI_ERROR (Status)) {
    return NULL;
  }

  mI2cIoNotify  = NotifyFunction;
  *Registration = &mRegistration;
  return STUB_EFI_EVENT;
}

/**
  Retrieves the current value of a 64-bit free running performance counter.

  @return The current value of the free running performance counter.
**/
UINT64
EFIAPI
__wrap_GetPerformanceCounter (
  VOID
  )
{
  return (UINT64)mock ();
}

/**
  Converts elapsed ticks of performance counter to time in nanoseconds.

  @param  Ticks     The number of elapsed ticks of running performance counter.
  @return The elapsed time in nanoseconds.
**/
UINT64
EFIAPI
__wrap_GetTimeInNanoSecond (
  IN      UINT64  Ticks
  )
{
  return Ticks;
}

/**
  Mocked to return whether ExitBootServices() has been called

  @retval  TRUE  The system has finished executing the EVT_SIGNAL_EXIT_BOOT_SERVICES event.
  @retval  FALSE The system has not finished executing the EVT_SIGNAL_EXIT_BOOT_SERVICES event.

**/
BOOLEAN
EFIAPI
__wrap_EfiAtRuntime (
  VOID
  )
{
  return (BOOLEAN)mock ();
}

/**
  Creates an event in a group.

  @param[in]   Type             The type of event to create and its mode and attributes.
  @param[in]   NotifyTpl        The task priority level of event notifications,if needed.
  @param[in]   NotifyFunction   The pointer to the event's notification function, if any.
  @param[in]   NotifyContext    The pointer to the notification function's context.
  @param[in]   EventGroup       The pointer to the unique identifier of the group to which this event belongs.
  @param[out]  Event            The pointer to the newly created event if the call succeeds.

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
  EFI_STATUS  Status = (EFI_STATUS)mock ();

  if (EFI_ERROR (Status)) {
    return Status;
  }

  mExitBsNotify = NotifyFunction;
  return EFI_SUCCESS;
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
  mI2cIoNotify = NULL;
  return EFI_SUCCESS;
}

/**
  Returns an array of handles that support the requested protocol in a buffer allocated from pool.

  @param[in]       SearchType   Specifies which handle(s) are to be returned.
  @param[in]       Protocol     Provides the protocol to search by.
  @param[in]       SearchKey    Supplies the search key depending on the SearchType.
  @param[out]      NoHandles    The number of handles returned in Buffer.
  @param[out]      Buffer       A pointer to the buffer to return the requested array of handles that

  @retval EFI_SUCCESS           The array of handles was returned in Buffer, and the number of
                                handles in Buffer was returned in NoHandles.
  @retval EFI_NOT_FOUND         No handles match the search.
  @retval EFI_OUT_OF_RESOURCES  There is not enough pool memory to store the matching results.
  @retval EFI_INVALID_PARAMETER NoHandles is NULL.
  @retval EFI_INVALID_PARAMETER Buffer is NULL.
**/
EFI_STATUS
EFIAPI
MockedLocateHandleBuffer (
  IN     EFI_LOCATE_SEARCH_TYPE  SearchType,
  IN     EFI_GUID                *Protocol       OPTIONAL,
  IN     VOID                    *SearchKey      OPTIONAL,
  OUT    UINTN                   *NoHandles,
  OUT    EFI_HANDLE              **Buffer
  )
{
  EFI_STATUS  Status = (EFI_STATUS)mock ();

  ASSERT (SearchType == ByRegisterNotify);
  ASSERT (CompareGuid (&gEfiI2cIoProtocolGuid, Protocol));
  ASSERT (SearchKey == &mRegistration);
  ASSERT (NoHandles != NULL);
  ASSERT (Buffer != NULL);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  *NoHandles = 3;
  *Buffer    = AllocatePool (*NoHandles * sizeof (EFI_HANDLE));
  return EFI_SUCCESS;
}

/**
  Queries a handle to determine if it supports a specified protocol.

  @param[in]   Handle           The handle being queried.
  @param[in]   Protocol         The published unique identifier of the protocol.
  @param[out]  Interface        Supplies the address where a pointer to the corresponding Protocol
                                Interface is returned.

  @retval EFI_SUCCESS           The interface information for the specified protocol was returned.
  @retval EFI_UNSUPPORTED       The device does not support the specified protocol.
  @retval EFI_INVALID_PARAMETER Handle is NULL.
  @retval EFI_INVALID_PARAMETER Protocol is NULL.
  @retval EFI_INVALID_PARAMETER Interface is NULL.
**/
EFI_STATUS
EFIAPI
MockedHandleProtocol (
  IN  EFI_HANDLE  Handle,
  IN  EFI_GUID    *Protocol,
  OUT VOID        **Interface
  )
{
  EFI_STATUS  Status   = (EFI_STATUS)mock ();
  EFI_GUID    *DevGuid = (EFI_GUID *)mock ();

  ASSERT (CompareGuid (&gEfiI2cIoProtocolGuid, Protocol));
  ASSERT (Interface != NULL);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  mI2cIo.DeviceGuid   = DevGuid;
  mI2cIo.QueueRequest = MockedI2cIoProtocolQueueRequest;
  *Interface          = &mI2cIo;
  return EFI_SUCCESS;
}

/**
  Retrieves a pointer to the system configuration table from the EFI System Table
  based on a specified GUID.

  @param  TableGuid       The pointer to table's GUID type..
  @param  Table           The pointer to the table associated with TableGuid in the EFI System Table.

  @retval EFI_SUCCESS     A configuration table matching TableGuid was found.
  @retval EFI_NOT_FOUND   A configuration table matching TableGuid could not be found.
**/
EFI_STATUS
EFIAPI
__wrap_EfiGetSystemConfigurationTable (
  IN  EFI_GUID  *TableGuid,
  OUT VOID      **Table
  )
{
  mRtProperties.Version                  = 1;
  mRtProperties.Length                   = 8;
  mRtProperties.RuntimeServicesSupported = (UINT32)mock ();

  if (mRtProperties.RuntimeServicesSupported == 0) {
    return EFI_UNSUPPORTED;
  }

  *Table = &mRtProperties;
  return EFI_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// TEST DATA
////////////////////////////////////////////////////////////////////////////////

EXPECTED_I2C_REQUEST  I2cRequestCtl12Hour = {
  2, // OperationCount
  {
    {
      0, // I2C_WRITE
      1, // LengthInBytes
      { NUVOTON_RTC_CONTROL_ADDRESS }
    },
    {
      I2C_FLAG_READ,
      1, // LengthInBytes
      { 0x00                        }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestCtl24Hour = {
  2, // OperationCount
  {
    {
      0, // I2C_WRITE
      1, // LengthInBytes
      { NUVOTON_RTC_CONTROL_ADDRESS }
    },
    {
      I2C_FLAG_READ,
      1, // LengthInBytes
      { 0x20                        }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestCtlStop = {
  2, // OperationCount
  {
    {
      0, // I2C_WRITE
      1, // LengthInBytes
      { NUVOTON_RTC_CONTROL_ADDRESS }
    },
    {
      I2C_FLAG_READ,
      1, // LengthInBytes
      { 0x80                        }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestCtlTwo1 = {
  2, // OperationCount
  {
    {
      0, // I2C_WRITE
      1, // LengthInBytes
      { NUVOTON_RTC_CONTROL_ADDRESS }
    },
    {
      I2C_FLAG_READ,
      1, // LengthInBytes
      { 0x01                        }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestDateTimeCtlStop = {
  2, // OperationCount
  {
    {
      0, // I2C_WRITE
      1, // LengthInBytes
      { NUVOTON_RTC_TIME_ADDRESS }
    },
    {
      I2C_FLAG_READ,
      11, // LengthInBytes
      //   0     1     2     3     4     5     6     7     8     9    10
      { 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x02, 0x13, 0x09, 0x22, 0xA0}
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestDateTimeCtlNoon = {
  2, // OperationCount
  {
    {
      0, // I2C_WRITE
      1, // LengthInBytes
      { NUVOTON_RTC_TIME_ADDRESS }
    },
    {
      I2C_FLAG_READ,
      11, // LengthInBytes
      //   0     1     2     3     4     5     6     7     8     9    10
      { 0x00, 0x00, 0x00, 0x00, 0x92, 0x00, 0x02, 0x13, 0x09, 0x22, 0x00}
    }
  }
};

//
// Byte 6 (day or week) is used to detect BMC changes the time register
// Below read series has expected value of byte 6.
//
EXPECTED_I2C_REQUEST  I2cRequestDateTimeCtlIntact = {
  2, // OperationCount
  {
    {
      0, // I2C_WRITE
      1, // LengthInBytes
      { NUVOTON_RTC_TIME_ADDRESS }
    },
    {
      I2C_FLAG_READ,
      11, // LengthInBytes
      //   0     1     2     3     4     5     6     7     8     9    10
      { 0x39, 0x00, 0x20, 0x00, 0x23, 0x00, 0x00, 0x15, 0x09, 0x22, 0x20}
    }
  }
};

//
// Byte 6 (day or week) is used to detect BMC changes the time register
// Below read series has unexpected value of byte 6, hence BMC has updated it.
//
EXPECTED_I2C_REQUEST  I2cRequestDateTimeCtlCorrupt = {
  2, // OperationCount
  {
    {
      0, // I2C_WRITE
      1, // LengthInBytes
      { NUVOTON_RTC_TIME_ADDRESS }
    },
    {
      I2C_FLAG_READ,
      11, // LengthInBytes
      //   0     1     2     3     4     5     6     7     8     9    10
      { 0x39, 0x00, 0x20, 0x00, 0x23, 0x00, 0x04, 0x15, 0x09, 0x22, 0x20}
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestSetCtlSts = {
  1, // OperationCount
  {
    {
      0, // I2C_WRITE
      3, // LengthInBytes
      { NUVOTON_RTC_CONTROL_ADDRESS, 0x20, 0x00 }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestSetPrimary = {
  1, // OperationCount
  {
    {
      0, // I2C_WRITE
      2, // LengthInBytes
      { NUVOTON_RTC_PRIMARY_ACCESS_ADDRESS, 0x01 }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestSetWDayNoon = {
  1, // OperationCount
  {
    {
      0, // I2C_WRITE
      2, // LengthInBytes
      { NUVOTON_RTC_DAY_OF_WEEK_ADDRESS, 0x05 }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestSetWDayFix = {
  1, // OperationCount
  {
    {
      0, // I2C_WRITE
      2, // LengthInBytes
      { NUVOTON_RTC_DAY_OF_WEEK_ADDRESS, 0x00 }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestSetTimePdt = {
  1, // OperationCount
  {
    {
      0,  // I2C_WRITE
      11, // LengthInBytes
      //                             0     1     2     3     4     5     6     7     8     9
      { NUVOTON_RTC_TIME_ADDRESS, 0x23, 0x00, 0x50, 0x00, 0x82, 0x00, 0x00, 0x15, 0x09, 0x22 }
    }
  }
};

EXPECTED_I2C_REQUEST  I2cRequestSetTimePst = {
  1, // OperationCount
  {
    {
      0,  // I2C_WRITE
      11, // LengthInBytes
      //                             0     1     2     3     4     5     6     7     8     9
      { NUVOTON_RTC_TIME_ADDRESS, 0x34, 0x00, 0x20, 0x00, 0x22, 0x00, 0x00, 0x10, 0x02, 0x22 }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
// TEST CASES
////////////////////////////////////////////////////////////////////////////////

/**
  Test error handling paths of LibRtcInitialize

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcInitErrors (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  gBS                     = &mBS;
  gBS->CreateEventEx      = MockedCreateEventEx;
  gBS->CloseEvent         = MockedCloseEvent;
  gBS->LocateHandleBuffer = MockedLocateHandleBuffer;
  gBS->HandleProtocol     = MockedHandleProtocol;

  //
  // Test case 1: fail to create protocol notify event
  //
  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, FALSE);
  will_return (__wrap_EfiGetVariable, EFI_NOT_FOUND);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_NOT_READY);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_OUT_OF_RESOURCES);
  //
  // Test case 2: fail to create notify ExitBootServices event
  //
  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, FALSE);
  will_return (__wrap_EfiGetVariable, EFI_NOT_FOUND);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedCreateEventEx, EFI_OUT_OF_RESOURCES);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_OUT_OF_RESOURCES);

  return UNIT_TEST_PASSED;
}

/**
  LibRtcInitialize successful, but no Nuvoton RTC found

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcNotFound (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_TIME    Time = { 0 };

  gBS                     = &mBS;
  gBS->CreateEventEx      = MockedCreateEventEx;
  gBS->CloseEvent         = MockedCloseEvent;
  gBS->LocateHandleBuffer = MockedLocateHandleBuffer;
  gBS->HandleProtocol     = MockedHandleProtocol;

  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, FALSE);
  will_return (__wrap_EfiGetVariable, EFI_NOT_FOUND);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedCreateEventEx, EFI_SUCCESS);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);

  // Simulate polling until I2cIoProtocol becomes available
  will_return (MockedLocateHandleBuffer, EFI_NOT_FOUND);
  mI2cIoNotify (NULL, NULL);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);
  will_return (MockedLocateHandleBuffer, EFI_NOT_FOUND);
  mI2cIoNotify (NULL, NULL);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);

  // Simulate multiple I2C handles found but none has Nuvoton RTC on
  will_return (MockedLocateHandleBuffer, EFI_SUCCESS);
  will_return (MockedHandleProtocol, EFI_UNSUPPORTED);
  will_return (MockedHandleProtocol, &gNVIDIAI2cNcp81599);
  will_return (MockedHandleProtocol, EFI_SUCCESS);
  will_return (MockedHandleProtocol, &gNVIDIAI2cMaxim20024);
  will_return (MockedHandleProtocol, EFI_SUCCESS);
  will_return (MockedHandleProtocol, &gNVIDIAI2cUnknown);
  will_return (MockedLocateHandleBuffer, EFI_NOT_FOUND);  // exit the polling loop
  mI2cIoNotify (NULL, NULL);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);

  // Since there is no Nuvoton RTC, calling GetTime would fail
  will_return (__wrap_GetPerformanceCounter, 0x210001000ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  Status = LibGetTime (&Time, NULL);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  // Since there is no Nuvoton RTC, calling SetTime would fail
  Time.Month  = 9;
  Time.Day    = 15;
  Time.Year   = 2022;
  Time.Hour   = 14;
  Time.Minute = 50;
  Time.Second = 23;
  will_return (__wrap_GetPerformanceCounter, 0x210001055ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  Test LibRtcInitialize with CPU on primary I2C of RTC

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcInitPrimary (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  gBS                     = &mBS;
  gBS->CreateEventEx      = MockedCreateEventEx;
  gBS->CloseEvent         = MockedCloseEvent;
  gBS->LocateHandleBuffer = MockedLocateHandleBuffer;
  gBS->HandleProtocol     = MockedHandleProtocol;

  // When CPU is on primary I2C, it needs to program RTC control register to
  // gain write access to the time registers.

  // Inject error to I2C write to control register
  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, FALSE);
  will_return (__wrap_EfiGetVariable, EFI_NOT_FOUND);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedCreateEventEx, EFI_SUCCESS);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);

  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, TRUE);
  will_return (MockedLocateHandleBuffer, EFI_SUCCESS);
  will_return (MockedHandleProtocol, EFI_SUCCESS);
  will_return (MockedHandleProtocol, &gNVIDIAI2cNct3018y);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetCtlSts);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_DEVICE_ERROR);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetPrimary);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_DEVICE_ERROR);
  mI2cIoNotify (NULL, NULL);

  // The notify event should be closed after Nuvoton RTC found
  UT_ASSERT_TRUE (mI2cIoNotify == NULL);

  // Initialization successful
  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, FALSE);
  will_return (__wrap_EfiGetVariable, EFI_NOT_FOUND);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedCreateEventEx, EFI_SUCCESS);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);

  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, TRUE);
  will_return (MockedLocateHandleBuffer, EFI_SUCCESS);
  will_return (MockedHandleProtocol, EFI_SUCCESS);
  will_return (MockedHandleProtocol, &gNVIDIAI2cNct3018y);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetCtlSts);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetPrimary);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  mI2cIoNotify (NULL, NULL);

  // The notify event should be closed after Nuvoton RTC found
  UT_ASSERT_TRUE (mI2cIoNotify == NULL);

  return UNIT_TEST_PASSED;
}

/**
  Test LibRtcInitialize successful case

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcInitSuccess (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  gBS                     = &mBS;
  gBS->CreateEventEx      = MockedCreateEventEx;
  gBS->CloseEvent         = MockedCloseEvent;
  gBS->LocateHandleBuffer = MockedLocateHandleBuffer;
  gBS->HandleProtocol     = MockedHandleProtocol;

  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, FALSE);
  will_return (__wrap_EfiGetVariable, EFI_NOT_FOUND);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedCreateEventEx, EFI_SUCCESS);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);

  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, FALSE);
  will_return (MockedLocateHandleBuffer, EFI_SUCCESS);
  will_return (MockedHandleProtocol, EFI_SUCCESS);
  will_return (MockedHandleProtocol, &gNVIDIAI2cNct3018y);
  mI2cIoNotify (NULL, NULL);

  // The notify event should be closed after Nuvoton RTC found
  UT_ASSERT_TRUE (mI2cIoNotify == NULL);

  // Call exit boot services
  will_return (__wrap_EfiGetSystemConfigurationTable, 0);
  mExitBsNotify (NULL, NULL);

  return UNIT_TEST_PASSED;
}

/**
  Calls to get/set wake-up time are not supported

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcGetSetWakeup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_TIME    Time;
  BOOLEAN     Enabled;
  BOOLEAN     Pending;

  // GetWakeupTime is defined but not supported
  Status = LibGetWakeupTime (&Enabled, &Pending, &Time);
  UT_ASSERT_TRUE (Status == EFI_UNSUPPORTED);

  // SetWakeupTime is defined but not supported
  Status = LibSetWakeupTime (TRUE, &Time);
  UT_ASSERT_TRUE (Status == EFI_UNSUPPORTED);

  return UNIT_TEST_PASSED;
}

/**
  Check error handling paths of LibGetTime

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcGetTimeErrors (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_TIME               Time;
  EFI_TIME_CAPABILITIES  Capabilities;

  // No Time pointer given
  Status = LibGetTime (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_INVALID_PARAMETER);

  // I2C read fails
  will_return (__wrap_GetPerformanceCounter, 0x210021888ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestDateTimeCtlNoon);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_DEVICE_ERROR);
  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  // I2C read succeeds, but RTC is stopped
  will_return (__wrap_GetPerformanceCounter, 0x210031777ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestDateTimeCtlStop);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  Get date/time the first time during boot

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcGetTimeBootFirst (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_TIME               Time;
  EFI_TIME_CAPABILITIES  Capabilities;

  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestDateTimeCtlNoon);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetWDayNoon);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  will_return (__wrap_EfiSetVariable, EFI_SUCCESS);

  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  UT_ASSERT_TRUE (Time.Month  == 9);
  UT_ASSERT_TRUE (Time.Day    == 13);
  UT_ASSERT_TRUE (Time.Year   == 2022);
  UT_ASSERT_TRUE (Time.Hour   == 12);
  UT_ASSERT_TRUE (Time.Minute == 0);
  UT_ASSERT_TRUE (Time.Second == 0);
  UT_ASSERT_TRUE (Time.Nanosecond == 858636373);

  return UNIT_TEST_PASSED;
}

/**
  Get date/time the second time during boot

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcGetTimeBootSecond (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_TIME               Time;
  EFI_TIME_CAPABILITIES  Capabilities;

  //
  // After the first get time, which is read from RTC.
  // Successive get time will come from ARM performance counter.
  // Verify that there are no I2C transactions, and the time is in sync with
  // performance counter values.
  //
  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT + 321);
  will_return (__wrap_EfiAtRuntime, FALSE);
  will_return (__wrap_EfiAtRuntime, FALSE);

  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  UT_ASSERT_TRUE (Time.Month  == 9);
  UT_ASSERT_TRUE (Time.Day    == 13);
  UT_ASSERT_TRUE (Time.Year   == 2022);
  UT_ASSERT_TRUE (Time.Hour   == 12);
  UT_ASSERT_TRUE (Time.Minute == 0);
  UT_ASSERT_TRUE (Time.Second == 0);
  UT_ASSERT_TRUE (Time.Nanosecond == 858636373 + 321);

  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT + 2000000000ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  will_return (__wrap_EfiAtRuntime, FALSE);

  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  UT_ASSERT_TRUE (Time.Month  == 9);
  UT_ASSERT_TRUE (Time.Day    == 13);
  UT_ASSERT_TRUE (Time.Year   == 2022);
  UT_ASSERT_TRUE (Time.Hour   == 12);
  UT_ASSERT_TRUE (Time.Minute == 0);
  UT_ASSERT_TRUE (Time.Second == 2);
  UT_ASSERT_TRUE (Time.Nanosecond == 858636373);

  return UNIT_TEST_PASSED;
}

/**
  Check error handling paths of LibSetTime

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcSetTimeErrors (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_TIME    Time = { 0 };

  // No Time pointer given
  Status = LibSetTime (NULL);
  UT_ASSERT_TRUE (Status == EFI_INVALID_PARAMETER);

  // Invalid Time
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_INVALID_PARAMETER);

  Time.Month    = 9;
  Time.Day      = 15;
  Time.Year     = 2022;
  Time.Hour     = 21;
  Time.Minute   = 50;
  Time.Second   = 23;
  Time.TimeZone = 420;    // PDT UTC-7:00

  // I2C transaction does not go through
  will_return (__wrap_GetPerformanceCounter, 0x210051606ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestCtl12Hour);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_NOT_READY);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  // I2C read succeeds, but RTC is stopped
  will_return (__wrap_GetPerformanceCounter, 0x210051776ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestCtlStop);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  // CPU does not have time write ownership
  will_return (__wrap_GetPerformanceCounter, 0x210051890ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestCtlTwo1);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  // I2C write fails
  will_return (__wrap_GetPerformanceCounter, 0x210051988ull);
  will_return (__wrap_EfiAtRuntime, FALSE);
  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestCtl12Hour);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetTimePdt);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_OUT_OF_RESOURCES);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  Set date/time during boot

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcSetTimeBoot (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_TIME    Time = { 0 };

  Time.Month      = 9;
  Time.Day        = 15;
  Time.Year       = 2022;
  Time.Hour       = 21;
  Time.Minute     = 50;
  Time.Second     = 23;
  Time.Nanosecond = 111111;
  Time.TimeZone   = 420;  // PDT UTC-7:00
  Time.Daylight   = EFI_TIME_IN_DAYLIGHT | EFI_TIME_ADJUST_DAYLIGHT;

  will_return (__wrap_GetPerformanceCounter, FIRST_SET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, FALSE);
  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestCtl12Hour);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetTimePdt);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  will_return (__wrap_EfiSetVariable, EFI_SUCCESS);

  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  Get date/time after setting the time

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcGetTimeAfterSet (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_TIME               Time;
  EFI_TIME_CAPABILITIES  Capabilities;

  //
  // TimeZone and DayLight are maintained and passed in to LibGetTime by
  // RealTimeClockRuntimeDxe
  //
  Time.TimeZone = 420;    // PDT UTC-7:00
  Time.Daylight = EFI_TIME_IN_DAYLIGHT | EFI_TIME_ADJUST_DAYLIGHT;

  will_return (__wrap_GetPerformanceCounter, FIRST_SET_TIME_PERF_COUNT + 1234);
  will_return (__wrap_EfiAtRuntime, FALSE);
  will_return (__wrap_EfiAtRuntime, FALSE);

  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  UT_ASSERT_TRUE (Time.Month  == 9);
  UT_ASSERT_TRUE (Time.Day    == 15);
  UT_ASSERT_TRUE (Time.Year   == 2022);
  UT_ASSERT_TRUE (Time.Hour   == 21);
  UT_ASSERT_TRUE (Time.Minute == 50);
  UT_ASSERT_TRUE (Time.Second == 23);
  UT_ASSERT_TRUE (Time.Nanosecond == 858900212);

  return UNIT_TEST_PASSED;
}

/**
  Set date/time during OS runtime

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcSetTimeOs (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_TIME    Time = { 0 };

  Time.Month      = 2;
  Time.Day        = 10;
  Time.Year       = 2022;
  Time.Hour       = 21;
  Time.Minute     = 50;
  Time.Second     = 23;
  Time.Nanosecond = 222222;
  Time.TimeZone   = 480;  // PST UTC-8:00
  Time.Daylight   = EFI_TIME_ADJUST_DAYLIGHT;

  // Verify that it gracefully exit if RT->SetTime is not supported
  will_return (__wrap_EfiGetSystemConfigurationTable, EFI_RT_SUPPORTED_GET_TIME);
  mExitBsNotify (NULL, NULL);
  will_return (__wrap_GetPerformanceCounter, SECOND_SET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, TRUE);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_UNSUPPORTED);

  // Verify that it gracefully exit if RT->SetVariable is not supported
  will_return (__wrap_EfiGetSystemConfigurationTable, EFI_RT_SUPPORTED_SET_TIME);
  mExitBsNotify (NULL, NULL);
  will_return (__wrap_GetPerformanceCounter, SECOND_SET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, TRUE);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_UNSUPPORTED);

  // Enable RT->SetTime and RT->SetVariable back
  will_return (
    __wrap_EfiGetSystemConfigurationTable,
    EFI_RT_SUPPORTED_GET_TIME | EFI_RT_SUPPORTED_SET_TIME | EFI_RT_SUPPORTED_SET_VARIABLE
    );
  mExitBsNotify (NULL, NULL);

  // Set new time
  will_return (__wrap_GetPerformanceCounter, SECOND_SET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, TRUE);
  will_return (__wrap_EfiSetVariable, EFI_SUCCESS);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  // Verify that RTC_OFFSET variable has below value
  UT_ASSERT_TRUE (mRtcOffset == -18752405);

  return UNIT_TEST_PASSED;
}

/**
  Get date/time during OS runtime

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcGetTimeOs (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_TIME               Time;
  EFI_TIME_CAPABILITIES  Capabilities;

  //
  // TimeZone and DayLight are maintained and passed in to LibGetTime by
  // RealTimeClockRuntimeDxe
  //
  Time.TimeZone = 480;    // PST UTC-8:00
  Time.Daylight = EFI_TIME_ADJUST_DAYLIGHT;

  // Verify that it gracefully exit if RT->GetTime is not supported
  will_return (__wrap_EfiGetSystemConfigurationTable, EFI_RT_SUPPORTED_SET_TIME);
  mExitBsNotify (NULL, NULL);
  will_return (__wrap_GetPerformanceCounter, SECOND_SET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, TRUE);
  will_return (__wrap_EfiAtRuntime, TRUE);
  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_UNSUPPORTED);

  // Enable RT->GetTime back
  will_return (
    __wrap_EfiGetSystemConfigurationTable,
    EFI_RT_SUPPORTED_GET_TIME | EFI_RT_SUPPORTED_SET_TIME | EFI_RT_SUPPORTED_SET_VARIABLE
    );
  mExitBsNotify (NULL, NULL);

  // Get time
  will_return (__wrap_GetPerformanceCounter, SECOND_SET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, TRUE);
  will_return (__wrap_EfiAtRuntime, TRUE);

  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  UT_ASSERT_TRUE (Time.Month  == 2);
  UT_ASSERT_TRUE (Time.Day    == 10);
  UT_ASSERT_TRUE (Time.Year   == 2022);
  UT_ASSERT_TRUE (Time.Hour   == 21);
  UT_ASSERT_TRUE (Time.Minute == 50);
  UT_ASSERT_TRUE (Time.Second == 23);
  UT_ASSERT_TRUE (Time.Nanosecond == 153865728);

  return UNIT_TEST_PASSED;
}

/**
  Get time on next boot after setting time during OS

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcGetTimeNextBootWithUpdate (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_TIME               Time;
  EFI_TIME_CAPABILITIES  Capabilities;

  //
  // Reinitialize to simulate a reboot
  //
  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, FALSE);
  will_return (__wrap_EfiGetVariable, EFI_SUCCESS);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedCreateEventEx, EFI_SUCCESS);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);

  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, FALSE);
  will_return (MockedLocateHandleBuffer, EFI_SUCCESS);
  will_return (MockedHandleProtocol, EFI_SUCCESS);
  will_return (MockedHandleProtocol, &gNVIDIAI2cNct3018y);
  mI2cIoNotify (NULL, NULL);

  // TimeZone and DayLight are maintained and passed in to LibGetTime by
  // RealTimeClockRuntimeDxe
  Time.TimeZone = 480;    // PST UTC-8:00
  Time.Daylight = EFI_TIME_ADJUST_DAYLIGHT;

  // Mocked values for GetTime
  will_return (__wrap_GetPerformanceCounter, SECOND_GET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestDateTimeCtlIntact);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);

  // Mocked values for SetTime, called by GetTime to update RTC time and clear RTC_OFFSET variable
  will_return (__wrap_GetPerformanceCounter, SECOND_GET_TIME_PERF_COUNT + 1000);
  will_return (__wrap_EfiAtRuntime, FALSE);
  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestCtl24Hour);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetTimePst);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  will_return (__wrap_EfiSetVariable, EFI_SUCCESS);

  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mRtcOffset == 0);

  UT_ASSERT_TRUE (Time.Month  == 2);
  UT_ASSERT_TRUE (Time.Day    == 10);
  UT_ASSERT_TRUE (Time.Year   == 2022);
  UT_ASSERT_TRUE (Time.Hour   == 22);
  UT_ASSERT_TRUE (Time.Minute == 20);
  UT_ASSERT_TRUE (Time.Second == 34);
  UT_ASSERT_TRUE (Time.Nanosecond == 858374144);

  return UNIT_TEST_PASSED;
}

/**
  Get time on next boot after setting time during OS. However, BMC also changes
  RTC time, hence discard the time set by SetTime.

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcGetTimeNextBootNoUpdate (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  EFI_TIME               Time;
  EFI_TIME_CAPABILITIES  Capabilities;

  //
  // Reinitialize to simulate a reboot
  //
  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, FALSE);
  will_return (__wrap_EfiGetVariable, EFI_SUCCESS);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedCreateEventEx, EFI_SUCCESS);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mI2cIoNotify != NULL);

  MockLibPcdGetBool (_PCD_TOKEN_PcdCpuHasRtcControl, FALSE);
  will_return (MockedLocateHandleBuffer, EFI_SUCCESS);
  will_return (MockedHandleProtocol, EFI_SUCCESS);
  will_return (MockedHandleProtocol, &gNVIDIAI2cNct3018y);
  mI2cIoNotify (NULL, NULL);

  // TimeZone and DayLight are maintained and passed in to LibGetTime by
  // RealTimeClockRuntimeDxe
  Time.TimeZone = 480;    // PST UTC-8:00
  Time.Daylight = EFI_TIME_ADJUST_DAYLIGHT;

  // Mocked values for GetTime
  will_return (__wrap_GetPerformanceCounter, SECOND_GET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, FALSE);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestDateTimeCtlCorrupt);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);
  expect_check (MockedI2cIoProtocolQueueRequest, RequestPacket, CheckMockedQueueRequest, &I2cRequestSetWDayFix);
  will_return (MockedI2cIoProtocolQueueRequest, EFI_SUCCESS);

  // SetTime isn't called by GetTime this time since BMC had updated the RTC.
  will_return (__wrap_EfiSetVariable, EFI_SUCCESS);

  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mRtcOffset == 0);

  UT_ASSERT_TRUE (Time.Month  == 9);
  UT_ASSERT_TRUE (Time.Day    == 16);
  UT_ASSERT_TRUE (Time.Year   == 2022);
  UT_ASSERT_TRUE (Time.Hour   == 7);
  UT_ASSERT_TRUE (Time.Minute == 20);
  UT_ASSERT_TRUE (Time.Second == 39);
  UT_ASSERT_TRUE (Time.Nanosecond == 858374144);

  return UNIT_TEST_PASSED;
}

/**
  Get/set time with Virtual RTC

  @param[in]  Context                   Unit test context
  @retval  UNIT_TEST_PASSED             The Unit test has passed.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
NRtcVirtualRtc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  INT64                  BuildEpoch;
  EFI_TIME               Time = { 0 };
  EFI_TIME_CAPABILITIES  Capabilities;

  // Re-initialize with Virtual RTC enabled
  MockLibPcdGetBool (_PCD_TOKEN_PcdVirtualRTC, TRUE);
  will_return (__wrap_EfiGetVariable, EFI_NOT_FOUND);
  will_return (__wrap_EfiCreateProtocolNotifyEvent, EFI_SUCCESS);
  will_return (MockedCreateEventEx, EFI_SUCCESS);
  Status = LibRtcInitialize (NULL, NULL);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  // Since Virtual RTC uses build time as a reference and
  // NuvotonRealTimeClockLib are built separately from this unit test,
  // so BUILD_EPOCH cannot be obtained from compliling this unit test.
  // Hence this first GetTime is for obtaining BUILD_EPOCH only.
  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT);
  will_return (__wrap_EfiAtRuntime, FALSE);
  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  BuildEpoch = EfiTimeToEpoch (&Time);

  // Verify that the time changes after a 100 seconds
  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT + (100 * 1000000000ull));
  will_return (__wrap_EfiAtRuntime, FALSE);
  will_return (__wrap_EfiAtRuntime, FALSE);
  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE ((BuildEpoch + 100) == EfiTimeToEpoch (&Time));

  // Verify that SetTime -10 minutes writes expected value to RTC_OFFSET variable
  EpochToEfiTime (EfiTimeToEpoch (&Time) - (10 * SEC_PER_MIN), &Time);
  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT + (200 * 1000000000ull));
  will_return (__wrap_EfiAtRuntime, FALSE);
  will_return (__wrap_EfiSetVariable, EFI_SUCCESS);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (mRtcOffset == (BuildEpoch + 100 - (10 * SEC_PER_MIN)));

  // Verify that the time read back correctly
  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT + (500 * 1000000000ull));
  will_return (__wrap_EfiAtRuntime, TRUE);
  will_return (__wrap_EfiAtRuntime, TRUE);
  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (EfiTimeToEpoch (&Time) == (BuildEpoch + 500 - 100 - (10 * SEC_PER_MIN)));

  // SetTime during OS runtime
  EpochToEfiTime (EfiTimeToEpoch (&Time) + (10 * SEC_PER_MIN), &Time);
  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT + (600 * 1000000000ull));
  will_return (__wrap_EfiAtRuntime, TRUE);
  will_return (__wrap_EfiSetVariable, EFI_SUCCESS);
  Status = LibSetTime (&Time);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);

  // Verify that the time read back correctly
  will_return (__wrap_GetPerformanceCounter, FIRST_GET_TIME_PERF_COUNT + (700 * 1000000000ull));
  will_return (__wrap_EfiAtRuntime, TRUE);
  will_return (__wrap_EfiAtRuntime, TRUE);
  Status = LibGetTime (&Time, &Capabilities);
  UT_ASSERT_TRUE (Status == EFI_SUCCESS);
  UT_ASSERT_TRUE (EfiTimeToEpoch (&Time) == (BuildEpoch + 500));

  return UNIT_TEST_PASSED;
}

/**
  Prepare for a unit test run

  @param Context                      Ignored
  @retval UNIT_TEST_PASSED            Setup succeeded.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
NRtcSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UefiPcdClear ();
  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for Nuvoton RTC
  unit tests and run the unit tests.

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
  UNIT_TEST_SUITE_HANDLE      NRtcTests;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a: v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to setup Test Framework. Exiting with status = %r\n", Status));
    ASSERT (FALSE);
    return Status;
  }

  UefiPcdInit ();

  //
  // Populate the Nuvoton RTC Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&NRtcTests, Framework, "Nuvoton RTC Tests", "UnitTest.NuvotonRTC", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Nuvoton RTC Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  Status = AddTestCase (NRtcTests, "Error handling cases during library entry point", "NRtcInitErrors", NRtcInitErrors, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "Error handling when no Nuvoton RTC found", "NRtcNotFound", NRtcNotFound, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "RTC library initializes successfully", "RtcLibInitSuccess", NRtcInitSuccess, NRtcSetup, NULL, NULL);

  Status = AddTestCase (NRtcTests, "Calls to Get/SetWakeupTime are unsupported", "GetSetWakeup", NRtcGetSetWakeup, NRtcSetup, NULL, NULL);

  Status = AddTestCase (NRtcTests, "Error handling cases of GetTime", "GetTimeErrors", NRtcGetTimeErrors, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "GetTime the first time during boot (read from RTC, sync with ARM counter)", "GetTimeBootFirst", NRtcGetTimeBootFirst, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "GetTime the second time (read from ARM counter, not RTC)", "GetTimeBootSecond", NRtcGetTimeBootSecond, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "Error handling cases of SetTime", "SetTimeErrors", NRtcSetTimeErrors, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "SetTime during boot with TimeZone set (update RTC time)", "SetTimeBoot", NRtcSetTimeBoot, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "GetTime after SetTime during boot", "GetTimeAfterSet", NRtcGetTimeAfterSet, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "SetTime during OS runtime (new time is staged in 'RTC_OFFSET' variable, but RTC is not updated)", "SetTimeOs", NRtcSetTimeOs, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "GetTime after SetTime during OS runtime", "GetTimeOs", NRtcGetTimeOs, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "GetTime on next boot (RTC is updated with new time from 'RTC_OFFSET')", "GetTimeNextBootWithUpdate", NRtcGetTimeNextBootWithUpdate, NRtcSetup, NULL, NULL);
  Status = AddTestCase (NRtcTests, "GetTime on next boot, but RTC had been changed by BMC (discard 'RTC_OFFSET')", "GetTimeNextBootNoUpdate", NRtcGetTimeNextBootNoUpdate, NRtcSetup, NULL, NULL);

  Status = AddTestCase (NRtcTests, "RTC library initializes with CPU on primary I2C", "RtcLibInitPrimary", NRtcInitPrimary, NRtcSetup, NULL, NULL);

  Status = AddTestCase (NRtcTests, "GetTime/SetTime with Virtual RTC", "VirtualRtc", NRtcVirtualRtc, NRtcSetup, NULL, NULL);

  // Execute the tests.
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
