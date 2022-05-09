/** @file

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
STATIC BOOLEAN                    mVrsRtc = FALSE;
STATIC BOOLEAN                    mMaximSplitUpdateRtc = FALSE;
STATIC EFI_EVENT                  mRtcExitBootServicesEvent = NULL;
STATIC INT64                      mRtcOffset = 0;
STATIC INT64                      mPerfomanceTimerOffset = MAX_INT64;
STATIC UINT32                     mRuntimeServicesSupported = 0;
STATIC BOOLEAN                    mVirtualRTC = FALSE;

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
  I2C_REQUEST_PACKET_2_OPS       RequestData;
  EFI_I2C_REQUEST_PACKET         *RequestPacket = (EFI_I2C_REQUEST_PACKET *)&RequestData;
  BOOLEAN                        BCDMode = FALSE;
  BOOLEAN                        TwentyFourHourMode = FALSE;
  UINT64                         PerformanceTimerNanoseconds = 0;
  UINT32                         RtcEpochSeconds;
  UINT32                         PerformanceEpochSeconds;
  UINT8                          RTCValue[4];
  UINT8                          Register;
  UINT8                          Index;

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
    if (mVirtualRTC) {
      RtcEpochSeconds = mRtcOffset;
      EpochToEfiTime (RtcEpochSeconds, Time);
      PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000ull;
      mPerfomanceTimerOffset = ((UINT64)RtcEpochSeconds - (UINT64)PerformanceEpochSeconds) * 1000000000ull;
    } else if (mI2cIo == NULL) {
      return EFI_DEVICE_ERROR;
    } else {
      if (mVrsRtc) {
        for (Index = 0; Index < 4; Index++) {
          RequestData.OperationCount = 2;
          Register = VRS_RTC_T_BASE + Index;
          RequestData.Operation[0].Buffer = &Register;
          RequestData.Operation[0].LengthInBytes = 1;
          RequestData.Operation[0].Flags = 0;
          RequestData.Operation[1].Buffer = (VOID *)&RTCValue[Index];
          RequestData.Operation[1].LengthInBytes = 1;
          RequestData.Operation[1].Flags = I2C_FLAG_READ;
          Status = mI2cIo->QueueRequest (mI2cIo, 0, NULL, RequestPacket, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((EFI_D_ERROR, "%a: Failed to get rtc register %02x: %r.\r\n", __FUNCTION__, Register, Status));
            return EFI_DEVICE_ERROR;
          }
        }
        RtcEpochSeconds = RTCValue[0] << 24 | RTCValue[1] << 16 | RTCValue[2] << 8 | RTCValue[3];
        //Time isn't initialized kick off by writing build time
        if (RtcEpochSeconds == 0) {
          DEBUG ((DEBUG_INFO, "%a: Reset time to build epoch\r\n", __FUNCTION__));
          RtcEpochSeconds = BUILD_EPOCH;
          EpochToEfiTime (BUILD_EPOCH, Time);
          LibSetTime (Time);
          RtcEpochSeconds -= mRtcOffset;
        }
        EpochToEfiTime (RtcEpochSeconds, Time);
      } else {
        RequestData.OperationCount = 2;
        RequestData.Operation[0].Buffer = (VOID *)&TimeUpdate.Address;
        RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address);
        RequestData.Operation[0].Flags = 0;
        TimeUpdate.Address = MAXIM_RTC_CONTROL_ADDRESS;
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
        RequestData.Operation[0].Flags = 0;
        RequestData.Operation[0].Buffer = (VOID *)&TimeUpdate;
        TimeUpdate.Address = MAXIM_RTC_UPDATE0_ADDRESS;
        if (mMaximSplitUpdateRtc) {
          RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.SplitUpdate);
          TimeUpdate.SplitUpdate.ClearFlagsOnRead = 1;
          TimeUpdate.SplitUpdate.UpdateFromWrite = 0;
          TimeUpdate.SplitUpdate.FreezeSeconds = 0;
          TimeUpdate.SplitUpdate.Reserved1 = 0;
          TimeUpdate.SplitUpdate.Reserved2 = 0;
          TimeUpdate.SplitUpdate.ReadBufferUpdate = 1;
        } else {
          RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.Update);
          TimeUpdate.Update.ClearFlagsOnRead = 1;
          TimeUpdate.Update.UpdateFromWrite = 0;
          TimeUpdate.Update.FreezeSeconds = 0;
          TimeUpdate.Update.Reserved1 = 0;
          TimeUpdate.Update.Reserved2 = 0;
          TimeUpdate.Update.ReadBufferUpdate = 1;
        }
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
      }
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
  I2C_REQUEST_PACKET_2_OPS    RequestData;
  EFI_I2C_REQUEST_PACKET      *RequestPacket = (EFI_I2C_REQUEST_PACKET *)&RequestData;
  UINT64                      PerformanceTimerNanoseconds = 0;
  UINT32                      RtcEpochSeconds;
  UINT32                      PerformanceEpochSeconds;
  INT64                       NewPerformanceOffset;
  UINT8                       VrsBuffer[2];
  UINT8                       VrsRtcValue[4];
  UINT8                       Index;
  UINT32                      WriteFlags;
  UINT8                       VrsAttempts;


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
      if (mVirtualRTC) {
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
  } else if (mVirtualRTC) {
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
    if (mVrsRtc) {
      //Check for PEC
      VrsBuffer[0] = VRS_CTL_2;
      RequestData.OperationCount = 2;
      RequestData.Operation[0].Buffer = (VOID *)VrsBuffer;
      RequestData.Operation[0].LengthInBytes = 1;
      RequestData.Operation[0].Flags = 0;
      RequestData.Operation[1].Buffer = (VOID *)VrsBuffer+1;
      RequestData.Operation[1].LengthInBytes = 1;
      RequestData.Operation[1].Flags = I2C_FLAG_READ;
      Status = mI2cIo->QueueRequest (mI2cIo, 0, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to get rtc control register: %r.\r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }

      if ((VrsBuffer[1] & VRS_CTL_2_EN_PEC) == VRS_CTL_2_EN_PEC) {
        WriteFlags = I2C_FLAG_SMBUS_PEC;
      } else {
        WriteFlags = 0;
      }

      VrsAttempts = 0;
      while (VrsAttempts < VRS_RTC_ATTEMPTS) {
        for (Index = 0; Index < 4; Index++) {
          RequestData.OperationCount = 2;
          VrsBuffer[0] = VRS_RTC_T_BASE + Index;
          RequestData.Operation[0].Buffer = VrsBuffer;
          RequestData.Operation[0].LengthInBytes = 1;
          RequestData.Operation[0].Flags = 0;
          RequestData.Operation[1].Buffer = (VOID *)&VrsRtcValue[Index];
          RequestData.Operation[1].LengthInBytes = 1;
          RequestData.Operation[1].Flags = I2C_FLAG_READ;
          Status = mI2cIo->QueueRequest (mI2cIo, 0, NULL, RequestPacket, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((EFI_D_ERROR, "%a: Failed to get rtc register %02x: %r.\r\n", __FUNCTION__, VrsBuffer[0], Status));
            return EFI_DEVICE_ERROR;
          }
        }
        RtcEpochSeconds = VrsRtcValue[0] << 24 | VrsRtcValue[1] << 16 | VrsRtcValue[2] << 8 | VrsRtcValue[3];
        if (RtcEpochSeconds != 0) {
          break;
        }

        RtcEpochSeconds = 0x01;
        for (Index = 0; Index < 4; Index++) {
          VrsBuffer[0] = VRS_RTC_T_BASE + Index;
          VrsBuffer[1] = (RtcEpochSeconds >> (8 * (3 - Index))) & 0xFF;
          RequestData.OperationCount = 1;
          RequestData.Operation[0].Buffer = VrsBuffer;
          RequestData.Operation[0].LengthInBytes = 2;
          RequestData.Operation[0].Flags = WriteFlags;
          Status = mI2cIo->QueueRequest (mI2cIo, 0, NULL, RequestPacket, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((EFI_D_ERROR, "%a: Failed to set rtc register %x: %r.\r\n", __FUNCTION__, VrsBuffer[0] ,Status));
            return EFI_DEVICE_ERROR;
          }
        }

        RtcEpochSeconds = 0xFFFFFFFE;
        for (Index = 0; Index < 4; Index++) {
          VrsBuffer[0] = VRS_RTC_A_BASE + Index;
          VrsBuffer[1] = (RtcEpochSeconds >> (8 * (3 - Index))) & 0xFF;
          RequestData.OperationCount = 1;
          RequestData.Operation[0].Buffer = VrsBuffer;
          RequestData.Operation[0].LengthInBytes = 2;
          RequestData.Operation[0].Flags = WriteFlags;
          Status = mI2cIo->QueueRequest (mI2cIo, 0, NULL, RequestPacket, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((EFI_D_ERROR, "%a: Failed to set rtc register %x: %r.\r\n", __FUNCTION__, VrsBuffer[0] ,Status));
            return EFI_DEVICE_ERROR;
          }
        }

        MicroSecondDelay (VRS_I2C_DELAY_US);
        RtcEpochSeconds = 0;
        VrsAttempts++;
      }

      if (RtcEpochSeconds == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to start VRS-10 RTC falling back to performance counter\r\n", __FUNCTION__));
        RtcEpochSeconds = PerformanceTimerNanoseconds / 1000000000;
      }

      mRtcOffset = EfiTimeToEpoch (Time) - RtcEpochSeconds;

    } else {
      RequestData.OperationCount = 1;
      RequestData.Operation[0].Flags = 0;
      RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.Control);
      RequestData.Operation[0].Buffer = (UINT8 *)&TimeUpdate;
      TimeUpdate.Address = MAXIM_RTC_CONTROL_ADDRESS;
      TimeUpdate.Control.BCD = 0;
      TimeUpdate.Control.TwentyFourHourMode = 1;
      TimeUpdate.Control.Reserved = 0;
      Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to set control setting: %r.\r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }
      RequestData.OperationCount = 1;
      RequestData.Operation[0].Flags = 0;
      RequestData.Operation[0].Buffer = (UINT8 *)&TimeUpdate;
      TimeUpdate.Address = MAXIM_RTC_UPDATE0_ADDRESS;
      if (mMaximSplitUpdateRtc) {
        RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.SplitUpdate);
        TimeUpdate.SplitUpdate.ClearFlagsOnRead = 1;
        TimeUpdate.SplitUpdate.UpdateFromWrite = 1;
        TimeUpdate.SplitUpdate.FreezeSeconds = 0;
        TimeUpdate.SplitUpdate.Reserved1 = 0;
        TimeUpdate.SplitUpdate.Reserved2 = 0;
        TimeUpdate.SplitUpdate.ReadBufferUpdate = 0;
      } else {
        RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.Update);
        TimeUpdate.Update.ClearFlagsOnRead = 1;
        TimeUpdate.Update.UpdateFromWrite = 1;
        TimeUpdate.Update.FreezeSeconds = 0;
        TimeUpdate.Update.Reserved1 = 0;
        TimeUpdate.Update.Reserved2 = 0;
        TimeUpdate.Update.ReadBufferUpdate = 0;
      }
      Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to commit control settings: %r.\r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }
      RequestData.OperationCount = 1;
      RequestData.Operation[0].Flags = 0;
      RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.DateTime);
      RequestData.Operation[0].Buffer = (UINT8 *)&TimeUpdate;
      TimeUpdate.Address = MAXIM_RTC_TIME_ADDRESS;
      TimeUpdate.DateTime.Day = Time->Day;
      TimeUpdate.DateTime.DayOfWeek = 1 << EfiTimeToWday (Time);
      TimeUpdate.DateTime.Hours = Time->Hour;
      TimeUpdate.DateTime.Minutes = Time->Minute;
      TimeUpdate.DateTime.Month = Time->Month;
      TimeUpdate.DateTime.Seconds = Time->Second;
      TimeUpdate.DateTime.Years = Time->Year - MAXIM_BASE_YEAR;
      Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to store time: %r.\r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }
      RequestData.OperationCount = 1;
      RequestData.Operation[0].Flags = 0;
      RequestData.Operation[0].Buffer = (UINT8 *)&TimeUpdate;
      TimeUpdate.Address = MAXIM_RTC_UPDATE0_ADDRESS;
      if (mMaximSplitUpdateRtc) {
        RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.SplitUpdate);
        TimeUpdate.SplitUpdate.ClearFlagsOnRead = 1;
        TimeUpdate.SplitUpdate.UpdateFromWrite = 1;
        TimeUpdate.SplitUpdate.FreezeSeconds = 0;
        TimeUpdate.SplitUpdate.Reserved1 = 0;
        TimeUpdate.SplitUpdate.Reserved2 = 0;
        TimeUpdate.SplitUpdate.ReadBufferUpdate = 0;
      } else {
        RequestData.Operation[0].LengthInBytes = sizeof (TimeUpdate.Address) + sizeof (TimeUpdate.Update);
        TimeUpdate.Update.ClearFlagsOnRead = 1;
        TimeUpdate.Update.UpdateFromWrite = 1;
        TimeUpdate.Update.FreezeSeconds = 0;
        TimeUpdate.Update.Reserved1 = 0;
        TimeUpdate.Update.Reserved2 = 0;
        TimeUpdate.Update.ReadBufferUpdate = 0;
      }
      Status = mI2cIo->QueueRequest (mI2cIo, MAXIM_I2C_ADDRESS_INDEX, NULL, RequestPacket, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to commit time: %r.\r\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }
      MicroSecondDelay (MAXIM_I2C_DELAY_US);
      mRtcOffset = 0;
    }
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
      } else if (CompareGuid (&gNVIDIAI2cMaxim77851, I2cIo->DeviceGuid)) {
        gBS->CloseEvent (Event);
        mI2cIo = I2cIo;
        mMaximSplitUpdateRtc = TRUE;
        break;
      } else if (CompareGuid (&gNVIDIAI2cVrsPseq, I2cIo->DeviceGuid)) {
        gBS->CloseEvent (Event);
        mI2cIo = I2cIo;
        mVrsRtc = TRUE;
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

  mVirtualRTC = PcdGetBool (PcdVirtualRTC);

  Status = EfiGetVariable (L"RTC_OFFSET", &gNVIDIATokenSpaceGuid, NULL, &VariableSize, &mRtcOffset);
  if (EFI_ERROR (Status)) {
    if (mVirtualRTC) {
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
