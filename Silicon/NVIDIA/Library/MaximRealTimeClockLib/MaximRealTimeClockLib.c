/** @file

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/RealTimeClockLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/I2cIo.h>
#include <Library/TimeBaseLib.h>
#include <Guid/GlobalVariable.h>
#include <Guid/RtPropertiesTable.h>


#include "MaximRealTimeClockLib.h"

STATIC VOID                       *mI2cIoSearchToken = NULL;
STATIC EFI_I2C_IO_PROTOCOL        *mI2cIo = NULL;
STATIC EFI_EVENT                  mRtcExitBootServicesEvent = NULL;
STATIC INT64                      mRtcOffset = 0;
STATIC INT64                      mPerfomanceTimerOffset = MAX_INT64;
STATIC UINT32                     mRuntimeServicesSupported = 0;

/**
  Returns the current time and date information, and the time-keeping
  capabilities of the hardware platform.

  @param  Time                  A pointer to storage to receive a snapshot of
                                the current time.
  @param  Capabilities          An optional pointer to a buffer to receive the
                                real time clock device's capabilities.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_INVALID_PARAMETER Time is NULL.
  @retval EFI_DEVICE_ERROR      The time could not be retrieved due to hardware
                                error.

**/
EFI_STATUS
EFIAPI
LibGetTime (
  OUT EFI_TIME                *Time,
  OUT EFI_TIME_CAPABILITIES   *Capabilities
  )
{
  EFI_STATUS                     Status;
  MAXIM_RTC_UPDATE_DATA          TimeUpdate;
  MAXIM_I2C_REQUEST_PACKET_2_OPS RequestData;
  EFI_I2C_REQUEST_PACKET         *RequestPacket = (EFI_I2C_REQUEST_PACKET *)&RequestData;
  BOOLEAN                        BCDMode = FALSE;
  BOOLEAN                        TwentyFourHourMode = FALSE;
  UINT64                         PerformanceTimerNanoseconds = 0;
  UINT32                         RtcEpochSeconds;
  UINT32                         PerformanceEpochSeconds;

  if (Time == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PerformanceTimerNanoseconds = GetTimeInNanoSecond (GetPerformanceCounter ());
  if (EfiAtRuntime () || (mPerfomanceTimerOffset != MAX_INT64)) {
    if (EfiAtRuntime () && (mRuntimeServicesSupported & EFI_RT_SUPPORTED_GET_TIME) == 0) {
      return EFI_UNSUPPORTED;
    }
    PerformanceTimerNanoseconds += mPerfomanceTimerOffset;
    PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000ull;
    EpochToEfiTime (PerformanceEpochSeconds, Time);
  } else {
    if (PcdGetBool (PcdVirtualRTC)) {
      RtcEpochSeconds = mRtcOffset;
      EpochToEfiTime (RtcEpochSeconds, Time);
      PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000ull;
      mPerfomanceTimerOffset = ((UINT64)RtcEpochSeconds - (UINT64)PerformanceEpochSeconds) * 1000000000ull;
    } else if (mI2cIo == NULL) {
      return EFI_DEVICE_ERROR;
    } else {

      RequestData.OperationCount = 2;
      RequestData.Operation[0].Buffer = (VOID *)&TimeUpdate.Address;
      RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address);
      RequestData.Operation[0].Flags = 0;
      TimeUpdate.Address = MAXIC_RTC_CONTROL_ADDRESS;
      RequestData.Operation[1].Buffer = (VOID *)&TimeUpdate.Control;
      RequestData.Operation[1].LengthInBytes = sizeof (TimeUpdate.Control);
      RequestData.Operation[1].Flags = I2C_FLAG_READ;
      Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to get control register: %r.\r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }

      BCDMode = (TimeUpdate.Control.BCD == 1);
      TwentyFourHourMode = (TimeUpdate.Control.TwentyFourHourMode == 1);

      RequestData.OperationCount = 1;
      RequestData.Operation[0].Buffer = (VOID *)&TimeUpdate;
      RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.Update);
      RequestData.Operation[0].Flags = 0;
      TimeUpdate.Address = MAXIM_RTC_UPDATE0_ADDRESS;
      TimeUpdate.Update.ClearFlagsOnRead = 1;
      TimeUpdate.Update.UpdateFromWrite = 0;
      TimeUpdate.Update.FreezeSeconds = 0;
      TimeUpdate.Update.Reserved1 = 0;
      TimeUpdate.Update.Reserved2 = 0;
      TimeUpdate.Update.ReadBufferUpdate = 1;
      Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to request read update: %r.\r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }
      MicroSecondDelay (MAXIM_I2C_DELAY_US);

      RequestData.OperationCount = 2;
      RequestData.Operation[0].Buffer = (VOID *)&TimeUpdate.Address;
      RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address);
      RequestData.Operation[0].Flags = 0;
      TimeUpdate.Address = MAXIM_RTC_TIME_ADDRESS;
      RequestData.Operation[1].Buffer = (VOID *)&TimeUpdate.DateTime;
      RequestData.Operation[1].LengthInBytes = sizeof (TimeUpdate.DateTime);
      RequestData.Operation[1].Flags = I2C_FLAG_READ;
      Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to get time: %r.\r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }

      if (BCDMode) {
        Time->Second = BcdToDecimal8 (TimeUpdate.DateTime.Seconds);
        Time->Minute = BcdToDecimal8 (TimeUpdate.DateTime.Minutes);
        if (TwentyFourHourMode) {
          Time->Hour   = BcdToDecimal8 (TimeUpdate.DateTime.Hours & 0x3F);
        } else {
          Time->Hour   = BcdToDecimal8 (TimeUpdate.DateTime.Hours & 0x3F);
          if ((Time->Hour & BIT6) == BIT6) {
            Time->Hour += 12;
          }
        }
        Time->Day = BcdToDecimal8 (TimeUpdate.DateTime.Day);
        Time->Month = BcdToDecimal8 (TimeUpdate.DateTime.Month);
        Time->Year = BcdToDecimal8 (TimeUpdate.DateTime.Years) + MAXIM_BASE_YEAR;
      } else {
        Time->Second = TimeUpdate.DateTime.Seconds;
        Time->Minute = TimeUpdate.DateTime.Minutes;
        if (TwentyFourHourMode) {
          Time->Hour   = (TimeUpdate.DateTime.Hours & 0x3F);
        } else {
          Time->Hour   = (TimeUpdate.DateTime.Hours & 0x3F);
          if ((Time->Hour & BIT6) == BIT6) {
            Time->Hour += 12;
          }
        }
        Time->Day = TimeUpdate.DateTime.Day;
        Time->Month = TimeUpdate.DateTime.Month;
        Time->Year = TimeUpdate.DateTime.Years + MAXIM_BASE_YEAR;
      }
      RtcEpochSeconds = EfiTimeToEpoch (Time);
      if (mRtcOffset != 0) {
        RtcEpochSeconds += mRtcOffset;
        EpochToEfiTime (RtcEpochSeconds, Time);
        LibSetTime (Time);
      }

      PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000ull;
      mPerfomanceTimerOffset = ((UINT64)RtcEpochSeconds - (UINT64)PerformanceEpochSeconds) * 1000000000ull;
    }
  }

  Time->Nanosecond = PerformanceTimerNanoseconds % 1000000000;

  if (Capabilities != NULL) {
    Capabilities->Resolution = 1;
    Capabilities->Accuracy = 0;
    Capabilities->SetsToZero = FALSE;
  }
  return EFI_SUCCESS;
}


/**
  Sets the current local time and date information.

  @param  Time                  A pointer to the current time.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_INVALID_PARAMETER A time field is out of range.
  @retval EFI_DEVICE_ERROR      The time could not be set due due to hardware
                                error.

**/
EFI_STATUS
EFIAPI
LibSetTime (
  IN EFI_TIME                *Time
  )
{
  EFI_STATUS                  Status;
  MAXIM_RTC_UPDATE_DATA       TimeUpdate;
  EFI_I2C_REQUEST_PACKET      RequestPacket;
  UINT64                      PerformanceTimerNanoseconds = 0;
  UINT32                      RtcEpochSeconds;
  UINT32                      PerformanceEpochSeconds;
  INT64                       NewPerformanceOffset;

  if (Time == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Check the input parameters are within the range specified by UEFI
  if (!IsTimeValid (Time)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Time->Year < MAXIM_BASE_YEAR) ||
      (Time->Year >= (MAXIM_BASE_YEAR + 100))) {
    return EFI_INVALID_PARAMETER;
  }

  PerformanceTimerNanoseconds = GetTimeInNanoSecond (GetPerformanceCounter ());
  if (EfiAtRuntime ()) {
    if ((mRuntimeServicesSupported & EFI_RT_SUPPORTED_SET_TIME) == 0) {
      return EFI_UNSUPPORTED;
    } else {
      //Set Variable is required.
      //in this case the set time should also be 0 but add check to be safe.
      if ((mRuntimeServicesSupported & EFI_RT_SUPPORTED_SET_VARIABLE) == 0) {
        return EFI_UNSUPPORTED;
      }
      RtcEpochSeconds = EfiTimeToEpoch (Time);
      PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000;
      NewPerformanceOffset = (RtcEpochSeconds - PerformanceEpochSeconds);
      if (PcdGetBool (PcdVirtualRTC)) {
        mRtcOffset += PerformanceEpochSeconds;
      } else {
        mRtcOffset += NewPerformanceOffset - (mPerfomanceTimerOffset / 1000000000);
      }
      EfiSetVariable (L"RTC_OFFSET", &gNVIDIATokenSpaceGuid,
                      EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                      sizeof (mRtcOffset),
                      &mRtcOffset);
      mPerfomanceTimerOffset = NewPerformanceOffset * 1000000000;
    }
  } else if (PcdGetBool (PcdVirtualRTC)) {
    RtcEpochSeconds = EfiTimeToEpoch (Time);
    PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000;
    NewPerformanceOffset = (RtcEpochSeconds - PerformanceEpochSeconds);
    mRtcOffset = RtcEpochSeconds;
    EfiSetVariable (L"RTC_OFFSET", &gNVIDIATokenSpaceGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                    sizeof (mRtcOffset),
                    &mRtcOffset);
    mPerfomanceTimerOffset = NewPerformanceOffset * 1000000000;
  } else {
    if (mI2cIo == NULL) {
      return EFI_DEVICE_ERROR;
    }

    RequestPacket.OperationCount = 1;
    RequestPacket.Operation[0].Flags = 0;
    RequestPacket.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.Control);
    RequestPacket.Operation[0].Buffer = (UINT8 *)&TimeUpdate;
    TimeUpdate.Address = MAXIC_RTC_CONTROL_ADDRESS;
    TimeUpdate.Control.BCD = 0;
    TimeUpdate.Control.TwentyFourHourMode = 1;
    TimeUpdate.Control.Reserved = 0;
    Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, &RequestPacket, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to set control setting: %r.\r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }

    RequestPacket.OperationCount = 1;
    RequestPacket.Operation[0].Flags = 0;
    RequestPacket.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.Update);
    RequestPacket.Operation[0].Buffer = (UINT8 *)&TimeUpdate;
    TimeUpdate.Address = MAXIM_RTC_UPDATE0_ADDRESS;
    TimeUpdate.Update.ClearFlagsOnRead = 1;
    TimeUpdate.Update.UpdateFromWrite = 1;
    TimeUpdate.Update.FreezeSeconds = 0;
    TimeUpdate.Update.Reserved1 = 0;
    TimeUpdate.Update.Reserved2 = 0;
    TimeUpdate.Update.ReadBufferUpdate = 0;
    Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, &RequestPacket, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to commit control settings: %r.\r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }

    RequestPacket.OperationCount = 1;
    RequestPacket.Operation[0].Flags = 0;
    RequestPacket.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.DateTime);
    RequestPacket.Operation[0].Buffer = (UINT8 *)&TimeUpdate;
    TimeUpdate.Address = MAXIM_RTC_TIME_ADDRESS;
    TimeUpdate.DateTime.Day = Time->Day;
    TimeUpdate.DateTime.DayOfWeek = 1 << EfiTimeToWday (Time);
    TimeUpdate.DateTime.Hours = Time->Hour;
    TimeUpdate.DateTime.Minutes = Time->Minute;
    TimeUpdate.DateTime.Month = Time->Month;
    TimeUpdate.DateTime.Seconds = Time->Second;
    TimeUpdate.DateTime.Years = Time->Year - MAXIM_BASE_YEAR;
    Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, &RequestPacket, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to store time: %r.\r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }

    RequestPacket.OperationCount = 1;
    RequestPacket.Operation[0].Flags = 0;
    RequestPacket.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.Update);
    RequestPacket.Operation[0].Buffer = (UINT8 *)&TimeUpdate;
    TimeUpdate.Address = MAXIM_RTC_UPDATE0_ADDRESS;
    TimeUpdate.Update.ClearFlagsOnRead = 1;
    TimeUpdate.Update.UpdateFromWrite = 1;
    TimeUpdate.Update.FreezeSeconds = 0;
    TimeUpdate.Update.Reserved1 = 0;
    TimeUpdate.Update.Reserved2 = 0;
    TimeUpdate.Update.ReadBufferUpdate = 0;
    Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, &RequestPacket, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to commit time: %r.\r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }
    MicroSecondDelay (MAXIM_I2C_DELAY_US);
    mRtcOffset = 0;
    EfiSetVariable (L"RTC_OFFSET", &gNVIDIATokenSpaceGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                    sizeof (mRtcOffset),
                    &mRtcOffset);

    RtcEpochSeconds = EfiTimeToEpoch (Time);
    PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000ull;
    mPerfomanceTimerOffset = ((UINT64)RtcEpochSeconds - (UINT64)PerformanceEpochSeconds) * 1000000000ull;
  }
  return EFI_SUCCESS;
}


/**
  Returns the current wakeup alarm clock setting.

  @param  Enabled               Indicates if the alarm is currently enabled or
                                disabled.
  @param  Pending               Indicates if the alarm signal is pending and
                                requires acknowledgement.
  @param  Time                  The current alarm setting.

  @retval EFI_SUCCESS           The alarm settings were returned.
  @retval EFI_INVALID_PARAMETER Any parameter is NULL.
  @retval EFI_DEVICE_ERROR      The wakeup time could not be retrieved due to a
                                hardware error.
  @retval EFI_UNSUPPORTED       A wakeup timer is not supported on this
                                platform.

**/
EFI_STATUS
EFIAPI
LibGetWakeupTime (
  OUT BOOLEAN     *Enabled,
  OUT BOOLEAN     *Pending,
  OUT EFI_TIME    *Time
  )
{
  //
  // Currently unimplemented. The PCF8563 does not support setting the alarm
  // for an arbitrary date/time, but only for a minute/hour/day/weekday
  // combination. It should also depend on a platform specific setting that
  // indicates whether the PCF8563's interrupt line is connected in a way that
  // allows it to power up the system in the first place.
  //
  return EFI_UNSUPPORTED;
}


/**
  Sets the system wakeup alarm clock time.

  @param  Enabled               Enable or disable the wakeup alarm.
  @param  Time                  If Enable is TRUE, the time to set the wakeup
                                alarm for.Master

  @retval EFI_SUCCESS           If Enable is TRUE, then the wakeup alarm was
                                enabled. If Enable is FALSE, then the wakeup
                                alarm was disabled.
  @retval EFI_INVALID_PARAMETER A time field is out of range.
  @retval EFI_DEVICE_ERROR      The wakeup time could not be set due to a
                                hardware error.
  @retval EFI_UNSUPPORTED       A wakeup timer is not supported on this
                                platform.

**/
EFI_STATUS
EFIAPI
LibSetWakeupTime (
  IN BOOLEAN      Enabled,
  OUT EFI_TIME    *Time
  )
{
  // see comment above
  return EFI_UNSUPPORTED;
}

STATIC
VOID
I2cIoRegistrationEvent (
  IN  EFI_EVENT       Event,
  IN  VOID            *Context
  )
{
  EFI_HANDLE                *Handles = NULL;
  UINTN                     NoHandles;
  EFI_STATUS                Status;
  EFI_I2C_IO_PROTOCOL       *I2cIo;
  UINTN                     Index;

  //
  // Try to connect the newly registered driver to our handle.
  //
  while (mI2cIo == NULL) {
    Status = gBS->LocateHandleBuffer (ByRegisterNotify,
                                      &gEfiI2cIoProtocolGuid,
                                      mI2cIoSearchToken,
                                      &NoHandles,
                                      &Handles);
    if (EFI_ERROR (Status)) {
      break;
    }

    for (Index = 0; Index < NoHandles; Index++) {
      Status = gBS->HandleProtocol (
                      Handles[Index],
                      &gEfiI2cIoProtocolGuid,
                      (VOID **)&I2cIo);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to get i2c interface: %r", __FUNCTION__, Status));
        continue;
      }

      if ((CompareGuid (&gNVIDIAI2cMaxim77620, I2cIo->DeviceGuid)) ||
          (CompareGuid (&gNVIDIAI2cMaxim20024, I2cIo->DeviceGuid))) {
        gBS->CloseEvent (Event);
        mI2cIo = I2cIo;
        break;
      }
    }
    FreePool (Handles);
  }
}

/**
  Get the RT supported information

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
LibRtcExitBootServicesEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  EFI_STATUS Status;
  EFI_RT_PROPERTIES_TABLE *RtProperties;

  Status = EfiGetSystemConfigurationTable (&gEfiRtPropertiesTableGuid, (VOID **)&RtProperties);
  if (EFI_ERROR (Status)) {
    mRuntimeServicesSupported = MAX_UINT32;
  } else {
    mRuntimeServicesSupported = RtProperties->RuntimeServicesSupported;
  }
}

/**
  Library entry point

  @param  ImageHandle           Handle that identifies the loaded image.
  @param  SystemTable           System Table for this image.

  @retval EFI_SUCCESS           The operation completed successfully.

**/
EFI_STATUS
EFIAPI
LibRtcInitialize (
  IN EFI_HANDLE                            ImageHandle,
  IN EFI_SYSTEM_TABLE                      *SystemTable
  )
{
  EFI_EVENT           Event;
  EFI_STATUS          Status;
  UINTN               VariableSize = sizeof (mRtcOffset);

  Status = EfiGetVariable (L"RTC_OFFSET", &gNVIDIATokenSpaceGuid, NULL, &VariableSize, &mRtcOffset);
  if (EFI_ERROR (Status)) {
    if (PcdGetBool (PcdVirtualRTC)) {
      mRtcOffset = BUILD_EPOCH;
    } else {
      mRtcOffset = 0;
    }
  }

  //
  // Register a protocol registration notification callback on the I2C Io
  // protocol. This will notify us even if the protocol instance we are looking
  // for has already been installed.
  //
  Event = EfiCreateProtocolNotifyEvent (
            &gEfiI2cIoProtocolGuid,
            TPL_CALLBACK,
            I2cIoRegistrationEvent,
            NULL,
            &mI2cIoSearchToken
            );
  if (Event == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to create protocol event\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Register for the virtual address change event
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  LibRtcExitBootServicesEvent,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &mRtcExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to create exit boot services event\r\n", __FUNCTION__));
    gBS->CloseEvent (Event);
  }

  return EFI_SUCCESS;
}
