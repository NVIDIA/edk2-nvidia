/** @file

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/TimeBaseLib.h>
#include <Guid/GlobalVariable.h>
#include <Guid/RtPropertiesTable.h>

#include <Protocol/I2cMaster.h>
#include <Protocol/I2cEnumerate.h>

#include "NuvotonRealTimeClockLib.h"

STATIC VOID                     *mI2cMasterSearchToken     = NULL;
STATIC EFI_I2C_MASTER_PROTOCOL  *mI2cMaster                = NULL;
STATIC UINT16                   mSlaveAddr                 = 0;
STATIC EFI_EVENT                mRtcExitBootServicesEvent  = NULL;
STATIC EFI_EVENT                mRtcVirtualAddrChangeEvent = NULL;
STATIC INT64                    mRtcOffset                 = 0;
STATIC INT64                    mPerfomanceTimerOffset     = MAX_INT64;
STATIC UINT32                   mRuntimeServicesSupported  = 0;
STATIC BOOLEAN                  mVirtualRtc                = FALSE;
STATIC BOOLEAN                  mCpuHasRtcControl          = FALSE;

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
  OUT EFI_TIME               *Time,
  OUT EFI_TIME_CAPABILITIES  *Capabilities
  )
{
  EFI_STATUS                Status;
  NUVOTON_RTC_TIME_PACKET   TimePacket;
  I2C_REQUEST_PACKET_2_OPS  RequestData;
  EFI_I2C_REQUEST_PACKET    *RequestPacket              = (EFI_I2C_REQUEST_PACKET *)&RequestData;
  BOOLEAN                   BcdMode                     = FALSE;
  BOOLEAN                   TwentyFourHourMode          = FALSE;
  UINT64                    PerformanceTimerNanoseconds = 0;
  UINT32                    RtcEpochSeconds;
  UINT32                    PerformanceEpochSeconds;

  if (Time == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (EfiAtRuntime () && ((mRuntimeServicesSupported & EFI_RT_SUPPORTED_GET_TIME) == 0)) {
    return EFI_UNSUPPORTED;
  }

  PerformanceTimerNanoseconds = GetTimeInNanoSecond (GetPerformanceCounter ());

  if (mVirtualRtc) {
    if (mPerfomanceTimerOffset != MAX_INT64) {
      PerformanceTimerNanoseconds += mPerfomanceTimerOffset;
      PerformanceEpochSeconds      = PerformanceTimerNanoseconds / 1000000000ull;
      RtcEpochSeconds              = PerformanceEpochSeconds;
    } else {
      RtcEpochSeconds         = mRtcOffset;
      PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000ull;
      mPerfomanceTimerOffset  = ((UINT64)RtcEpochSeconds - (UINT64)PerformanceEpochSeconds) * 1000000000ull;
    }
  } else if (mI2cMaster == NULL) {
    return EFI_DEVICE_ERROR;
  } else {
    //
    // Read RTC date/time and control together in a burst read
    //
    TimePacket.Address                     = NUVOTON_RTC_TIME_ADDRESS;
    RequestData.OperationCount             = 2;
    RequestData.Operation[0].Flags         = 0; // I2C WRITE
    RequestData.Operation[0].Buffer        = (VOID *)&TimePacket.Address;
    RequestData.Operation[0].LengthInBytes = sizeof (TimePacket.Address);
    RequestData.Operation[1].Flags         = I2C_FLAG_READ;
    RequestData.Operation[1].Buffer        = (VOID *)&TimePacket.DateTime;
    RequestData.Operation[1].LengthInBytes = sizeof (TimePacket.DateTime) +
                                             sizeof (TimePacket.Control);
    Status = mI2cMaster->StartRequest (mI2cMaster, mSlaveAddr, RequestPacket, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to read time registers: %r.\r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }

    //
    // If RTC is stopped, it is unusable.
    //
    if (TimePacket.Control.ST == NUVOTON_RTC_CONTROL_ST_STOP) {
      DEBUG ((DEBUG_ERROR, "%a: RTC is stopped.\r\n", __FUNCTION__));
      return EFI_DEVICE_ERROR;
    }

    //
    // Convert to UEFI time format
    //
    BcdMode            = (TimePacket.Control.DM == NUVOTON_RTC_CONTROL_DM_BCD);
    TwentyFourHourMode = (TimePacket.Control.HF == NUVOTON_RTC_CONTROL_HF_24H);

    Time->Second = TimePacket.DateTime.Second & NUVOTON_RTC_SECOND_MASK;
    Time->Minute = TimePacket.DateTime.Minute & NUVOTON_RTC_MINUTE_MASK;
    Time->Hour   = TimePacket.DateTime.Hour   & NUVOTON_RTC_HOUR_MASK;
    Time->Day    = TimePacket.DateTime.Day    & NUVOTON_RTC_DAY_MASK;
    Time->Month  = TimePacket.DateTime.Month  & NUVOTON_RTC_MONTH_MASK;
    Time->Year   = TimePacket.DateTime.Year   & NUVOTON_RTC_YEAR_MASK;

    if (BcdMode) {
      Time->Second = BcdToDecimal8 (Time->Second);
      Time->Minute = BcdToDecimal8 (Time->Minute);
      Time->Hour   = BcdToDecimal8 (Time->Hour);
      Time->Day    = BcdToDecimal8 (Time->Day);
      Time->Month  = BcdToDecimal8 (Time->Month);
      Time->Year   = BcdToDecimal8 (Time->Year);
    }

    if (!TwentyFourHourMode) {
      Time->Hour %= 12;
      if ((TimePacket.DateTime.Hour & NUVOTON_RTC_PM_MASK) != 0) {
        Time->Hour += 12;
      }
    }

    Time->Year += NUVOTON_RTC_BASE_YEAR;

    RtcEpochSeconds = EfiTimeToEpoch (Time);

    //
    // If performance counter time is not in sync with RTC time, sync it to RTC time.
    // Otherwise, use counter time to have better precision.
    //
    PerformanceEpochSeconds = (PerformanceTimerNanoseconds + mPerfomanceTimerOffset) / 1000000000ull;
    if ((PerformanceEpochSeconds != RtcEpochSeconds) && (PerformanceEpochSeconds != RtcEpochSeconds + 1)) {
      PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000ull;
      mPerfomanceTimerOffset  = ((UINT64)RtcEpochSeconds - (UINT64)PerformanceEpochSeconds) * 1000000000ull;
    } else {
      RtcEpochSeconds = PerformanceEpochSeconds;
    }
  }

  //
  // Convert UTC to local time based on TimeZone and Daylight
  //
  if (Time->TimeZone != EFI_UNSPECIFIED_TIMEZONE) {
    RtcEpochSeconds += Time->TimeZone * SEC_PER_MIN;
  } else if ((Time->Daylight & EFI_TIME_IN_DAYLIGHT) == EFI_TIME_IN_DAYLIGHT) {
    RtcEpochSeconds += SEC_PER_HOUR;
  }

  EpochToEfiTime (RtcEpochSeconds, Time);

  Time->Nanosecond = PerformanceTimerNanoseconds % 1000000000;

  if (Capabilities != NULL) {
    Capabilities->Resolution = 1;
    Capabilities->Accuracy   = 0;
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
  IN EFI_TIME  *Time
  )
{
  EFI_STATUS                  Status;
  NUVOTON_RTC_CONTROL_PACKET  ControlPacket = { 0 };
  NUVOTON_RTC_TIME_PACKET     TimePacket    = { 0 };
  I2C_REQUEST_PACKET_2_OPS    RequestData;
  EFI_I2C_REQUEST_PACKET      *RequestPacket              = (EFI_I2C_REQUEST_PACKET *)&RequestData;
  BOOLEAN                     BcdMode                     = FALSE;
  BOOLEAN                     TwentyFourHourMode          = FALSE;
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

  if (EfiAtRuntime () && ((mRuntimeServicesSupported & EFI_RT_SUPPORTED_SET_TIME) == 0)) {
    return EFI_UNSUPPORTED;
  }

  //
  // Convert local time to UTC based on TimeZone and Daylight
  //
  RtcEpochSeconds = EfiTimeToEpoch (Time);
  if (Time->TimeZone != EFI_UNSPECIFIED_TIMEZONE) {
    RtcEpochSeconds -= Time->TimeZone * SEC_PER_MIN;
  } else if ((Time->Daylight & EFI_TIME_IN_DAYLIGHT) == EFI_TIME_IN_DAYLIGHT) {
    RtcEpochSeconds -= SEC_PER_HOUR;
  }

  PerformanceTimerNanoseconds = GetTimeInNanoSecond (GetPerformanceCounter ());

  if (mVirtualRtc) {
    PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000;
    NewPerformanceOffset    = (RtcEpochSeconds - PerformanceEpochSeconds);
    mRtcOffset              = RtcEpochSeconds;
    EfiSetVariable (
      L"RTC_OFFSET",
      &gNVIDIATokenSpaceGuid,
      EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
      sizeof (mRtcOffset),
      &mRtcOffset
      );
    mPerfomanceTimerOffset = NewPerformanceOffset * 1000000000;
  } else if (mI2cMaster == NULL) {
    return EFI_DEVICE_ERROR;
  } else {
    //
    // Read RTC control register, which is readonly for CPU.
    //
    ControlPacket.Address                  = NUVOTON_RTC_CONTROL_ADDRESS;
    RequestData.OperationCount             = 2;
    RequestData.Operation[0].Flags         = 0; // I2C WRITE
    RequestData.Operation[0].Buffer        = (VOID *)&ControlPacket.Address;
    RequestData.Operation[0].LengthInBytes = sizeof (ControlPacket.Address);
    RequestData.Operation[1].Flags         = I2C_FLAG_READ;
    RequestData.Operation[1].Buffer        = (VOID *)&ControlPacket.Control;
    RequestData.Operation[1].LengthInBytes = sizeof (ControlPacket.Control);

    Status = mI2cMaster->StartRequest (mI2cMaster, mSlaveAddr, RequestPacket, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to read control registers: %r.\r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }

    //
    // If RTC is stopped, it is unusable.
    //
    if (ControlPacket.Control.ST == NUVOTON_RTC_CONTROL_ST_STOP) {
      DEBUG ((DEBUG_ERROR, "%a: RTC is stopped.\r\n", __FUNCTION__));
      return EFI_DEVICE_ERROR;
    }

    //
    // If CPU does not have write ownership, RTC time cannot be changed.
    //
    if (!mCpuHasRtcControl &&
        (ControlPacket.Control.TWO == NUVOTON_RTC_CONTROL_TWO_PRIMARY))
    {
      DEBUG ((DEBUG_ERROR, "%a: CPU is not holding the write ownership.\r\n", __FUNCTION__));
      return EFI_DEVICE_ERROR;
    }

    BcdMode            = (ControlPacket.Control.DM == NUVOTON_RTC_CONTROL_DM_BCD);
    TwentyFourHourMode = (ControlPacket.Control.HF == NUVOTON_RTC_CONTROL_HF_24H);

    EpochToEfiTime (RtcEpochSeconds, Time);
    //
    // Convert date/time format to match RTC settings
    //
    TimePacket.Address         = NUVOTON_RTC_TIME_ADDRESS;
    TimePacket.DateTime.Second = Time->Second;
    TimePacket.DateTime.Minute = Time->Minute;
    TimePacket.DateTime.Hour   = Time->Hour;
    TimePacket.DateTime.Day    = Time->Day;
    TimePacket.DateTime.Month  = Time->Month;
    TimePacket.DateTime.Year   = Time->Year - NUVOTON_RTC_BASE_YEAR;

    if (!TwentyFourHourMode) {
      TimePacket.DateTime.Hour = ((TimePacket.DateTime.Hour + 11) % 12) + 1;
    }

    if (BcdMode) {
      TimePacket.DateTime.Second = DecimalToBcd8 (TimePacket.DateTime.Second);
      TimePacket.DateTime.Minute = DecimalToBcd8 (TimePacket.DateTime.Minute);
      TimePacket.DateTime.Hour   = DecimalToBcd8 (TimePacket.DateTime.Hour);
      TimePacket.DateTime.Day    = DecimalToBcd8 (TimePacket.DateTime.Day);
      TimePacket.DateTime.Month  = DecimalToBcd8 (TimePacket.DateTime.Month);
      TimePacket.DateTime.Year   = DecimalToBcd8 (TimePacket.DateTime.Year);
    }

    if (!TwentyFourHourMode && (Time->Hour >= 12)) {
      TimePacket.DateTime.Hour |= NUVOTON_RTC_PM_MASK;
    }

    //
    // DayOfWeek is not used by UEFI. It is repurposed to track if BMC updates RTC time.
    // If BMC does, DayOfWeek would have different offset than what UEFI sets here.
    //
    TimePacket.DateTime.DayOfWeek = (EfiTimeToWday (Time) + NUVOTON_RTC_WDAY_OFFSET) % 7;

    //
    // Update RTC date/time registers
    // Note: Second/minute/hour for alarm are also written. However, they are
    //       read-only to CPU and will be ignored by RTC.
    //       Daylight saving mode bit is read-only to CPU because it resides in
    //       control register.
    //
    RequestData.OperationCount             = 1;
    RequestData.Operation[0].Flags         = 0; // I2C WRITE
    RequestData.Operation[0].Buffer        = (VOID *)&TimePacket.Address;
    RequestData.Operation[0].LengthInBytes = sizeof (TimePacket.Address) +
                                             sizeof (TimePacket.DateTime);
    Status = mI2cMaster->StartRequest (mI2cMaster, mSlaveAddr, RequestPacket, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to store time: %r.\r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }

    PerformanceEpochSeconds = PerformanceTimerNanoseconds / 1000000000ull;
    mPerfomanceTimerOffset  = ((UINT64)RtcEpochSeconds - (UINT64)PerformanceEpochSeconds) * 1000000000ull;
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
  OUT BOOLEAN   *Enabled,
  OUT BOOLEAN   *Pending,
  OUT EFI_TIME  *Time
  )
{
  //
  // NCT3018Y only allows alarm to be set by Primary I2C (BMC)
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
  IN BOOLEAN    Enabled,
  OUT EFI_TIME  *Time
  )
{
  //
  // NCT3018Y only allows alarm to be set by Primary I2C (BMC)
  //
  return EFI_UNSUPPORTED;
}

/**
  Configure RTC

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
STATIC
VOID
LibRtcConfigure (
  VOID
  )
{
  EFI_STATUS                  Status;
  NUVOTON_RTC_CONTROL_PACKET  ControlPacket       = { 0 };
  NUVOTON_RTC_PRIMARY_PACKET  PrimaryAccessPacket = { 0 };
  I2C_REQUEST_PACKET_2_OPS    RequestData;
  EFI_I2C_REQUEST_PACKET      *RequestPacket = (EFI_I2C_REQUEST_PACKET *)&RequestData;

  //
  // NCT3018Y has two I2C interfaces: primary and secondary.
  // - Only primary I2C has write access to control and status registers.
  // - On power-up, secondary I2C has the write ownership to date/time registers by default.
  // - Primary I2C can set either set TWO bit to take write ownership from secondary I2C.
  //   Or primary I2C can set I2CPA bit to allow both interfaces to change the time.
  //
  // For platforms that have CPU routed to primary I2C, CPU sets the I2CPA bit to
  // gain date/time register write access and also clear status bits.
  //
  // For platforms that CPU on secondary I2C, the writes to control/status registers
  // would be ignored.
  //
  if (PcdGetBool (PcdCpuHasRtcControl) && (mI2cMaster != NULL)) {
    ControlPacket.Address = NUVOTON_RTC_CONTROL_ADDRESS;
    //
    // Default settings for RTC
    // Set 24-hour by default because BMC has such assumption
    //
    ControlPacket.Control.TWO  = NUVOTON_RTC_CONTROL_TWO_SECONDARY;
    ControlPacket.Control.CIE  = NUVOTON_RTC_CONTROL_CIE_DISABLE;
    ControlPacket.Control.OFIE = NUVOTON_RTC_CONTROL_OFIE_DISABLE;
    ControlPacket.Control.AIE  = NUVOTON_RTC_CONTROL_AIE_DISABLE;
    ControlPacket.Control.DSM  = NUVOTON_RTC_CONTROL_DSM_DST_OFF;
    ControlPacket.Control.HF   = NUVOTON_RTC_CONTROL_HF_24H;
    ControlPacket.Control.DM   = NUVOTON_RTC_CONTROL_DM_BCD;
    ControlPacket.Control.ST   = NUVOTON_RTC_CONTROL_ST_RUN;
    //
    // Zero out status bits to refresh
    //
    ControlPacket.Status.AF   = 0;
    ControlPacket.Status.OF   = 0;
    ControlPacket.Status.RTCF = 0;
    ControlPacket.Status.CIF  = 0;

    RequestData.OperationCount             = 1;
    RequestData.Operation[0].Flags         = 0; // I2C WRITE
    RequestData.Operation[0].Buffer        = (VOID *)&ControlPacket.Address;
    RequestData.Operation[0].LengthInBytes = sizeof (ControlPacket.Address) +
                                             sizeof (ControlPacket.Control) +
                                             sizeof (ControlPacket.Status);
    Status = mI2cMaster->StartRequest (mI2cMaster, mSlaveAddr, RequestPacket, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to program control register: %r.\r\n", __FUNCTION__, Status));
    }

    //
    // Set I2CPA bit for CPU to has write access to time registers
    //
    PrimaryAccessPacket.Address             = NUVOTON_RTC_PRIMARY_ACCESS_ADDRESS;
    PrimaryAccessPacket.PrimaryAccess.I2CPA = 1;

    RequestData.OperationCount             = 1;
    RequestData.Operation[0].Flags         = 0; // I2C WRITE
    RequestData.Operation[0].Buffer        = (VOID *)&PrimaryAccessPacket.Address;
    RequestData.Operation[0].LengthInBytes = sizeof (PrimaryAccessPacket.Address) +
                                             sizeof (PrimaryAccessPacket.PrimaryAccess);
    Status = mI2cMaster->StartRequest (mI2cMaster, mSlaveAddr, RequestPacket, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to program primary access register: %r.\r\n", __FUNCTION__, Status));
    }
  }
}

/**
  Get I2C protocol for this bus

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
STATIC
VOID
EFIAPI
I2cMasterRegistrationEvent (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS                  Status;
  EFI_HANDLE                  Handle;
  UINTN                       HandleSize;
  EFI_I2C_MASTER_PROTOCOL     *I2cMasterProtocol;
  EFI_I2C_ENUMERATE_PROTOCOL  *I2cEnumerateProtocol;
  CONST EFI_I2C_DEVICE        *I2cDevice;

  //
  // Try to connect the newly registered driver to our handle.
  //
  do {
    HandleSize = sizeof (Handle);
    Status     = gBS->LocateHandle (ByRegisterNotify, NULL, mI2cMasterSearchToken, &HandleSize, &Handle);
    if (EFI_ERROR (Status)) {
      return;
    }

    Status = gBS->HandleProtocol (Handle, &gEfiI2cEnumerateProtocolGuid, (VOID **)&I2cEnumerateProtocol);
    if (EFI_ERROR (Status)) {
      return;
    }

    I2cDevice = NULL;
    do {
      Status = I2cEnumerateProtocol->Enumerate (I2cEnumerateProtocol, &I2cDevice);
      if (!EFI_ERROR (Status)) {
        if (CompareGuid (I2cDevice->DeviceGuid, &gNVIDIAI2cNct3018y)) {
          break;
        }
      }
    } while (!EFI_ERROR (Status));

    if (EFI_ERROR (Status)) {
      return;
    }

    mSlaveAddr = I2cDevice->SlaveAddressArray[0];

    Status = gBS->HandleProtocol (Handle, &gEfiI2cMasterProtocolGuid, (VOID **)&I2cMasterProtocol);
    if (EFI_ERROR (Status)) {
      I2cMasterProtocol = NULL;
    }
  } while (I2cMasterProtocol == NULL);

  gBS->CloseEvent (Event);

  mI2cMaster = I2cMasterProtocol;

  LibRtcConfigure ();
}

/**
  Get the RT supported information

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
LibRtcExitBootServicesEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS               Status;
  EFI_RT_PROPERTIES_TABLE  *RtProperties;

  Status = EfiGetSystemConfigurationTable (&gEfiRtPropertiesTableGuid, (VOID **)&RtProperties);
  if (EFI_ERROR (Status)) {
    mRuntimeServicesSupported = MAX_UINT32;
  } else {
    mRuntimeServicesSupported = RtProperties->RuntimeServicesSupported;
  }
}

/**
  Fixup pointers so Get/SetTime can be called in runtime.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
LibRtcVirtualNotifyEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EfiConvertPointer (0x0, (VOID **)&mI2cMaster);
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
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_EVENT   Event;
  EFI_STATUS  Status;
  UINTN       VariableSize = sizeof (mRtcOffset);

  mI2cMaster             = NULL;
  mPerfomanceTimerOffset = MAX_INT64;
  mCpuHasRtcControl      = PcdGetBool (PcdCpuHasRtcControl);

  mVirtualRtc = PcdGetBool (PcdVirtualRTC);
  if (mVirtualRtc) {
    Status = EfiGetVariable (L"RTC_OFFSET", &gNVIDIATokenSpaceGuid, NULL, &VariableSize, &mRtcOffset);
    if (EFI_ERROR (Status)) {
      mRtcOffset = PcdGet64 (PcdBuildEpoch);
    }
  }

  //
  // Register a protocol registration notification callback on the I2C Io
  // protocol. This will notify us even if the protocol instance we are looking
  // for has already been installed.
  //
  Event = EfiCreateProtocolNotifyEvent (
            &gEfiI2cMasterProtocolGuid,
            TPL_CALLBACK,
            I2cMasterRegistrationEvent,
            NULL,
            &mI2cMasterSearchToken
            );
  if (Event == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create protocol event\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Register for ExitBootServices event
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
    DEBUG ((DEBUG_ERROR, "%a: Failed to create exit boot services event\r\n", __FUNCTION__));
    gBS->CloseEvent (Event);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Register for the virtual address change event
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  LibRtcVirtualNotifyEvent,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mRtcVirtualAddrChangeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create virtual address change event\r\n", __FUNCTION__));
    gBS->CloseEvent (Event);
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}
