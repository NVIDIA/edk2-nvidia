/** @file
  The main process for TimerUtil application.

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/HiiLib.h>
#include <Library/TimerLib.h>

#include <Protocol/Timer.h>

#define HARDWARE_THRESHOLD     10000       // 1ms threshold for hardware detection
#define BASE_TEST_DURATION_HW  2000000     // 200ms test duration for hardware
#define BASE_TEST_DURATION_VM  500000      // 50ms test duration for virtual
#define BASE_TIMER_INTERVAL    10000       // 1ms timer interval
#define BASE_TEST_PERIOD       2000        // 200us test period
#define TOLERANCE_PERCENT      25          // 25% tolerance
#define TOLERANCE_PERCENT_VM   50          // 50% tolerance for virtual due to unpredictable timing

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM  mTimerUtilParamList[] = {
  { L"--set_period", TypeValue },
  { L"--get_period", TypeFlag  },
  { L"--notify",     TypeFlag  },
  { L"--test",       TypeFlag  },
  { L"-?",           TypeFlag  },
  { NULL,            TypeMax   },
};

EFI_TIMER_ARCH_PROTOCOL  *mTimerProtocol;
EFI_HII_HANDLE           mHiiHandle;
CHAR16                   mAppName[]    = L"TimerUtil";
static volatile UINT64   mStartTime    = 0;
static volatile UINT64   mCurrentTime  = 0;
static volatile BOOLEAN  mTimerStarted = FALSE;

/**
  Timer callback function for the created event timer

  @param  Event     The event that fired
  @param  Context   Event context (unused)

  @return None
**/
VOID
EFIAPI
TimerEventCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if (!mTimerStarted) {
    mStartTime    = GetPerformanceCounter ();
    mTimerStarted = TRUE;
  }

  mCurrentTime = GetPerformanceCounter ();
}

/**
  Print the current timer period and other related information.
**/
VOID
PrintTimerPeriod (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT64      TimerPeriod;

  Status = mTimerProtocol->GetTimerPeriod (mTimerProtocol, &TimerPeriod);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_GET_PERIOD_FAILED), mHiiHandle, mAppName);
    return;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_DISPLAY_PERIOD), mHiiHandle, mAppName, TimerPeriod);
}

/**
  Set the timer period to a new value.

  @param[in] TimerPeriod   The new timer period in 100ns units
**/
VOID
SetTimerPeriod (
  IN UINT64  TimerPeriod
  )
{
  EFI_STATUS  Status;

  // Set timer period
  Status = mTimerProtocol->SetTimerPeriod (mTimerProtocol, TimerPeriod);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_SET_PERIOD_FAILED), mHiiHandle, mAppName);
    return;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_SET_PERIOD_SUCCESS), mHiiHandle, mAppName, TimerPeriod);
}

/**
  Unregister the current timer notification handler.

  @retval EFI_SUCCESS    The operation completed successfully.
  @return Others         The operation failed.
**/
EFI_STATUS
UnregisterTimerHandler (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = mTimerProtocol->RegisterHandler (mTimerProtocol, NULL);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_UNREGISTER_FAILED), mHiiHandle, mAppName, Status);
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_UNREGISTER_SUCCESS), mHiiHandle, mAppName);
  }

  return Status;
}

/**
  Register a timer notification handler.

  @param[in] NotifyFunction   The notification function to register

  @retval EFI_SUCCESS    The operation completed successfully.
  @return Others         The operation failed.
**/
EFI_STATUS
RegisterTimerHandler (
  IN EFI_TIMER_NOTIFY  NotifyFunction
  )
{
  EFI_STATUS  Status;

  Status = mTimerProtocol->RegisterHandler (mTimerProtocol, NotifyFunction);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_FAILED_STATUS), mHiiHandle, mAppName, Status);
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_FAILED), mHiiHandle, mAppName);
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_SUCCESS), mHiiHandle, mAppName);
  }

  return Status;
}

/**
  Checks if the given time is within the expected range.

  @param[in] ActualTime      The measured time in nanoseconds
  @param[in] ExpectedTime    The expected time in nanoseconds
  @param[in] Tolerance       The allowed tolerance in nanoseconds

  @retval TRUE   Time is within tolerance
  @retval FALSE  Time is outside tolerance
**/
STATIC
BOOLEAN
IsTimeWithinTolerance (
  IN UINT64  ActualTime,
  IN UINT64  ExpectedTime,
  IN UINT64  Tolerance
  )
{
  return (ActualTime >= (ExpectedTime - Tolerance)) &&
         (ActualTime <= (ExpectedTime + Tolerance));
}

/**
  Measures elapsed time using the timer callback.

  @param[in]  TimerEvent     The timer event to use
  @param[in]  TimerInterval  The timer interval in 100ns units
  @param[in]  StallTime      The time to stall in microseconds
  @param[out] ElapsedTimeNs  The elapsed time in nanoseconds

  @retval EFI_SUCCESS    Time measured successfully
  @retval Others         Error occurred during measurement
**/
STATIC
EFI_STATUS
MeasureElapsedTime (
  IN  EFI_EVENT  TimerEvent,
  IN  UINT64     TimerInterval,
  IN  UINT64     StallTime,
  OUT UINT64     *ElapsedTimeNs
  )
{
  EFI_STATUS  Status;

  // Reset timer variables
  mStartTime    = 0;
  mCurrentTime  = 0;
  mTimerStarted = FALSE;

  // Set timer to fire periodically
  Status = gBS->SetTimer (TimerEvent, TimerPeriodic, TimerInterval);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Wait for specified time
  gBS->Stall (StallTime);

  // Cancel timer
  gBS->SetTimer (TimerEvent, TimerCancel, 0);

  // Calculate elapsed time
  *ElapsedTimeNs = GetTimeInNanoSecond (mCurrentTime - mStartTime);

  return EFI_SUCCESS;
}

/**
  Runs a series of timer tests to verify proper functionality.

  @param[in] mTimerProtocol  Timer protocol instance
  @param[in] mHiiHandle      HII handle for string output
  @param[in] mAppName        Application name for output

  @retval EFI_SUCCESS    Tests completed successfully
  @retval Others         Error occurred during tests
**/
STATIC
EFI_STATUS
RunTimerTests (
  EFI_TIMER_ARCH_PROTOCOL  *mTimerProtocol,
  EFI_HII_HANDLE           mHiiHandle,
  CHAR16                   *mAppName
  )
{
  EFI_STATUS  Status;
  UINT64      OriginalPeriod   = 0;
  UINT64      ActualTestPeriod = 0;
  UINT64      ElapsedTimeNs;
  EFI_EVENT   TimerEvent  = NULL;
  BOOLEAN     TestsPassed = TRUE;

  // Start the timer tests
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_TEST_STARTED), mHiiHandle, mAppName);

  // Get original timer period
  Status = mTimerProtocol->GetTimerPeriod (mTimerProtocol, &OriginalPeriod);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_GET_PERIOD_FAILED), mHiiHandle, mAppName);
    return Status;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_ORIGINAL_PERIOD), mHiiHandle, mAppName, OriginalPeriod);

  // Determine hardware or virtual environment and set parameters
  BOOLEAN  IsHardwareEnv = (OriginalPeriod < HARDWARE_THRESHOLD);
  UINT64   ScaleFactor   = IsHardwareEnv ? 1 : (OriginalPeriod / HARDWARE_THRESHOLD);

  // Calculate test parameters (all in 100ns units)
  UINT64  TestDuration     = IsHardwareEnv ? BASE_TEST_DURATION_HW : BASE_TEST_DURATION_VM;
  UINT64  TimerInterval    = BASE_TIMER_INTERVAL * ScaleFactor;
  UINT64  TestPeriod       = IsHardwareEnv ? BASE_TEST_PERIOD : (OriginalPeriod / 5);
  UINT64  TolerancePercent = IsHardwareEnv ? TOLERANCE_PERCENT : TOLERANCE_PERCENT_VM;
  UINT64  ToleranceTime    = (TestDuration * TolerancePercent) / 100;

  // Convert test duration from 100ns units to microseconds for Stall function
  UINT64  StallTime = TestDuration / 10;

  // Expected time in nanoseconds for verification
  UINT64  ExpectedTimeNs = TestDuration * 100;
  UINT64  ToleranceNs    = ToleranceTime * 100;

  // Log environment and parameters
  ShellPrintHiiEx (
    -1,
    -1,
    NULL,
    STRING_TOKEN (STR_TIMER_UTIL_ENV_TYPE),
    mHiiHandle,
    mAppName,
    IsHardwareEnv ? L"Hardware" : L"Virtual"
    );

  if (!IsHardwareEnv) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_TIMER_UTIL_SCALING),
      mHiiHandle,
      mAppName,
      ScaleFactor
      );
  }

  ShellPrintHiiEx (
    -1,
    -1,
    NULL,
    STRING_TOKEN (STR_TIMER_UTIL_TEST_PARAMETERS),
    mHiiHandle,
    mAppName,
    ExpectedTimeNs,
    ToleranceNs,
    StallTime,
    TimerInterval,
    TestPeriod
    );

  // Create timer event
  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  TimerEventCallback,
                  NULL,
                  &TimerEvent
                  );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_FAILED_STATUS), mHiiHandle, mAppName, Status);
    return Status;
  }

  //
  // Test 1: Original timer period
  //
  Status = MeasureElapsedTime (
             TimerEvent,
             TimerInterval,
             StallTime,
             &ElapsedTimeNs
             );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_FAILED_STATUS), mHiiHandle, mAppName, Status);
    goto CleanupAndExit;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_ELAPSED_TIME), mHiiHandle, mAppName, ElapsedTimeNs);

  // Verify elapsed time is within expected range
  if (!IsTimeWithinTolerance (ElapsedTimeNs, ExpectedTimeNs, ToleranceNs)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_TEST_FAILED), mHiiHandle, mAppName);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_TIMER_UTIL_TIME_INVALID),
      mHiiHandle,
      mAppName,
      ElapsedTimeNs,
      ExpectedTimeNs
      );
    TestsPassed = FALSE;
    goto CleanupAndExit;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_TEST_PASSED_NOTIFY), mHiiHandle, mAppName, OriginalPeriod);

  //
  // Test 2: Modified timer period
  //
  Status = mTimerProtocol->SetTimerPeriod (mTimerProtocol, TestPeriod);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_SET_PERIOD_FAILED), mHiiHandle, mAppName);
    goto CleanupAndExit;
  }

  // Verify new period was set correctly
  Status = mTimerProtocol->GetTimerPeriod (mTimerProtocol, &ActualTestPeriod);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_GET_PERIOD_FAILED), mHiiHandle, mAppName);
    goto CleanupAndExit;
  }

  if (ActualTestPeriod != TestPeriod) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_TEST_FAILED), mHiiHandle, mAppName);
    TestsPassed = FALSE;
    goto CleanupAndExit;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_TEST_PASSED_SET), mHiiHandle, mAppName);

  // Measure elapsed time with new period
  Status = MeasureElapsedTime (
             TimerEvent,
             TimerInterval,
             StallTime,
             &ElapsedTimeNs
             );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_FAILED_STATUS), mHiiHandle, mAppName, Status);
    goto CleanupAndExit;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_ELAPSED_TIME), mHiiHandle, mAppName, ElapsedTimeNs);

  // Verify elapsed time with new period
  if (!IsTimeWithinTolerance (ElapsedTimeNs, ExpectedTimeNs, ToleranceNs)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_TEST_FAILED), mHiiHandle, mAppName);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_TIMER_UTIL_TIME_INVALID),
      mHiiHandle,
      mAppName,
      ElapsedTimeNs,
      ExpectedTimeNs
      );
    TestsPassed = FALSE;
    goto CleanupAndExit;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_TEST_PASSED_NOTIFY), mHiiHandle, mAppName, ActualTestPeriod);

  if (TestsPassed) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_TEST_PASSED_ALL), mHiiHandle, mAppName);
  }

CleanupAndExit:
  // Restore original timer period
  Status = mTimerProtocol->SetTimerPeriod (mTimerProtocol, OriginalPeriod);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_SET_PERIOD_FAILED), mHiiHandle, mAppName);
  }

  if (TimerEvent != NULL) {
    gBS->CloseEvent (TimerEvent);
  }

  return TestsPassed ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers, including
  both device drivers and bus drivers.

  The entry point for TimerUtil application that parse the command line input and calls Timer commands.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS    The operation completed successfully.

**/
EFI_STATUS
EFIAPI
InitializeTimerUtil (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                   Status;
  LIST_ENTRY                   *ParamPackage;
  CONST CHAR16                 *ValueStr;
  CHAR16                       *ProblemParam;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;

  //
  // Retrieve HII package list from ImageHandle
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = gHiiDatabase->NewPackageList (
                           gHiiDatabase,
                           PackageList,
                           NULL,
                           &mHiiHandle
                           );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (mHiiHandle != NULL);

  Status = ShellCommandLineParseEx (mTimerUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_LOCATE_FAILED), mHiiHandle, mAppName);
    goto Done;
  }

  // Locate the Timer Architecture Protocol
  Status = gBS->LocateProtocol (&gEfiTimerArchProtocolGuid, NULL, (VOID **)&mTimerProtocol);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_LOCATE_FAILED), mHiiHandle, mAppName);
    goto Done;
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_PROTOCOL_FOUND), mHiiHandle, mAppName);
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--test")) {
    Status = RunTimerTests (mTimerProtocol, mHiiHandle, mAppName);
    if (EFI_ERROR (Status)) {
      goto Done;
    }
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--notify")) {
    EFI_STATUS  Status;
    EFI_EVENT   TimerEvent     = NULL;
    UINT64      OriginalPeriod = 0;
    UINT64      NewPeriod      = 2000; // 200us
    UINT64      ElapsedTimeNs  = 0;

    mStartTime    = 0;
    mCurrentTime  = 0;
    mTimerStarted = FALSE;

    // Get current timer period for reference
    Status = mTimerProtocol->GetTimerPeriod (mTimerProtocol, &OriginalPeriod);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_GET_PERIOD_FAILED), mHiiHandle, mAppName);
      goto NotifyDone;
    }

    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_DISPLAY_PERIOD), mHiiHandle, mAppName, OriginalPeriod);

    // Create a timer event
    Status = gBS->CreateEvent (
                    EVT_TIMER | EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    TimerEventCallback,
                    NULL,
                    &TimerEvent
                    );
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_FAILED_STATUS), mHiiHandle, mAppName, Status);
      goto NotifyDone;
    }

    // Set timer to fire periodically
    Status = gBS->SetTimer (TimerEvent, TimerPeriodic, 10000); // 1ms
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_FAILED_STATUS), mHiiHandle, mAppName, Status);
      goto NotifyDone;
    }

    // Wait for 500ms
    gBS->Stall (500000);

    // Cancel timer
    gBS->SetTimer (TimerEvent, TimerCancel, 0);

    // Convert to nanoseconds
    ElapsedTimeNs = GetTimeInNanoSecond (mCurrentTime - mStartTime);
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_ELAPSED_TIME), mHiiHandle, mAppName, ElapsedTimeNs);
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_DISPLAY_PERIOD), mHiiHandle, mAppName, OriginalPeriod);

    // Now change timer period and measure again
    Status = mTimerProtocol->SetTimerPeriod (mTimerProtocol, NewPeriod);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_SET_PERIOD_FAILED), mHiiHandle, mAppName);
      goto NotifyDone;
    }

    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_SET_PERIOD_SUCCESS), mHiiHandle, mAppName, NewPeriod);

    // Reset counters
    mStartTime    = 0;
    mCurrentTime  = 0;
    mTimerStarted = FALSE;

    // Set timer to fire periodically again
    Status = gBS->SetTimer (TimerEvent, TimerPeriodic, 10000); // 1ms
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_NOTIFY_FAILED_STATUS), mHiiHandle, mAppName, Status);
      goto NotifyDone;
    }

    // Wait for 500ms again
    gBS->Stall (500000);

    // Cancel timer
    gBS->SetTimer (TimerEvent, TimerCancel, 0);

    // Convert to nanoseconds
    ElapsedTimeNs = GetTimeInNanoSecond (mCurrentTime - mStartTime);
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_ELAPSED_TIME), mHiiHandle, mAppName, ElapsedTimeNs);

    // Restore original timer period
    Status = mTimerProtocol->SetTimerPeriod (mTimerProtocol, OriginalPeriod);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_SET_PERIOD_FAILED), mHiiHandle, mAppName);
    } else {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_SET_PERIOD_SUCCESS), mHiiHandle, mAppName, OriginalPeriod);
    }

NotifyDone:
    // Clean up timer event if created
    if (TimerEvent != NULL) {
      gBS->CloseEvent (TimerEvent);
    }

    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--get_period")) {
    PrintTimerPeriod ();
    goto Done;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--set_period");
  if (NULL != ValueStr) {
    UINTN  Value = ShellStrToUintn (ValueStr);

    if (Value > 0) {
      SetTimerPeriod (Value);
    } else {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TIMER_UTIL_BAD_PERIOD_VALUE), mHiiHandle, mAppName);
    }
  }

Done:
  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
