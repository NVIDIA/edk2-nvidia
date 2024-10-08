/** @file
  Base Debug library instance based on Hafnium VM API.
  It is based on the original, serial enabled, DebugLib.

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/DebugLogScratchRegLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi/UefiBaseType.h>
#include <Library/DebugPrintErrorLevelLib.h>
#include <Library/ArmSvcLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/TimerLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>

/* Define the maximum debug and assert message length that this library supports */
#define MAX_DEBUG_MESSAGE_LENGTH  0x100

#define DEBUG_PROPERTY_ASSERT_RESET_ENABLED  0x40

/*
 * VA_LIST can not initialize to NULL for all compiler, so we use this to
 * indicate a null VA_LIST
 */
VA_LIST  mVaListNull;

/*
 * FFA ABI to send debug logs.
 */
#define FFA_CONSOLE_LOG_64  0xC400008A

/**
 * The constructor function doesn't need anything.
 *
 * @retval EFI_SUCCESS   The constructor always returns RETURN_SUCCESS.
 *
 */
RETURN_STATUS
EFIAPI
BaseDebugLibHafniumConstructor (
  VOID
  )
{
  return EFI_SUCCESS;
}

/**
 * Worker function to send characters to Hafnium, one by one.
 *
 * @param Buffer    NULL-terminated ASCII string to send
 *
 */
STATIC
VOID
BaseDebugHafniumPrint (
  IN  CHAR8  *Buffer
  )
{
  ARM_SVC_ARGS  ArmSvcArgs;
  CHAR8         FirmwareName[MAX_DEBUG_MESSAGE_LENGTH];
  UINT32        i;

  ZeroMem (&ArmSvcArgs, sizeof (ARM_SVC_ARGS));

  AsciiSPrint (FirmwareName, MAX_DEBUG_MESSAGE_LENGTH, "%s ", (CHAR16 *)PcdGetPtr (PcdFirmwareNickNameString));
  for (i = 0; i < AsciiStrLen (FirmwareName); i++) {
    ArmSvcArgs.Arg0 = FFA_CONSOLE_LOG_64;
    ArmSvcArgs.Arg1 = 1;
    ArmSvcArgs.Arg2 = FirmwareName[i];
    ArmCallSvc (&ArmSvcArgs);
  }

  for (i = 0; i < AsciiStrLen (Buffer); i++) {
    ArmSvcArgs.Arg0 = FFA_CONSOLE_LOG_64;
    ArmSvcArgs.Arg1 = 1;
    ArmSvcArgs.Arg2 = Buffer[i];
    ArmCallSvc (&ArmSvcArgs);
  }
}

/**
  Prints a debug message to the debug output device if the specified error level is enabled.

  If any bit in ErrorLevel is also set in DebugPrintErrorLevelLib function
  GetDebugPrintErrorLevel (), then print the message specified by Format and the
  associated variable argument list to the debug output device.

  If Format is NULL, then ASSERT().

  @param  ErrorLevel  The error level of the debug message.
  @param  Format      Format string for the debug message to print.
  @param  ...         Variable argument list whose contents are accessed
                      based on the format string specified by Format.

**/
VOID
EFIAPI
DebugPrint (
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  ...
  )
{
  VA_LIST  Marker;

  VA_START (Marker, Format);
  DebugVPrint (ErrorLevel, Format, Marker);
  VA_END (Marker);
}

/**
  Prints a debug message to the debug output device if the specified
  error level is enabled base on Null-terminated format string and a
  VA_LIST argument list or a BASE_LIST argument list.

  If any bit in ErrorLevel is also set in DebugPrintErrorLevelLib function
  GetDebugPrintErrorLevel (), then print the message specified by Format and
  the associated variable argument list to the debug output device.

  If Format is NULL, then ASSERT().

  @param  ErrorLevel      The error level of the debug message.
  @param  Format          Format string for the debug message to print.
  @param  VaListMarker    VA_LIST marker for the variable argument list.
  @param  BaseListMarker  BASE_LIST marker for the variable argument list.

**/
VOID
DebugPrintMarker (
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  IN  VA_LIST      VaListMarker,
  IN  BASE_LIST    BaseListMarker
  )
{
  CHAR8  Buffer[MAX_DEBUG_MESSAGE_LENGTH];

  //
  // If Format is NULL, then ASSERT().
  //
  ASSERT (Format != NULL);

  //
  // Check driver debug mask value and global mask
  //
  if ((ErrorLevel & GetDebugPrintErrorLevel ()) == 0) {
    return;
  }

  //
  // Convert the DEBUG() message to an ASCII String
  //
  if (BaseListMarker == NULL) {
    AsciiVSPrint (Buffer, MAX_DEBUG_MESSAGE_LENGTH, Format, VaListMarker);
  } else {
    AsciiBSPrint (Buffer, MAX_DEBUG_MESSAGE_LENGTH, Format, BaseListMarker);
  }

  //
  // Send the print string to a FFA
  //
  BaseDebugHafniumPrint (Buffer);
}

/**
  Prints a debug message to the debug output device if the specified
  error level is enabled.

  If any bit in ErrorLevel is also set in DebugPrintErrorLevelLib function
  GetDebugPrintErrorLevel (), then print the message specified by Format and
  the associated variable argument list to the debug output device.

  If Format is NULL, then ASSERT().

  @param  ErrorLevel    The error level of the debug message.
  @param  Format        Format string for the debug message to print.
  @param  VaListMarker  VA_LIST marker for the variable argument list.

**/
VOID
EFIAPI
DebugVPrint (
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  IN  VA_LIST      VaListMarker
  )
{
  DebugPrintMarker (ErrorLevel, Format, VaListMarker, NULL);
}

/**
  Prints a debug message to the debug output device if the specified
  error level is enabled.
  This function use BASE_LIST which would provide a more compatible
  service than VA_LIST.

  If any bit in ErrorLevel is also set in DebugPrintErrorLevelLib function
  GetDebugPrintErrorLevel (), then print the message specified by Format and
  the associated variable argument list to the debug output device.

  If Format is NULL, then ASSERT().

  @param  ErrorLevel      The error level of the debug message.
  @param  Format          Format string for the debug message to print.
  @param  BaseListMarker  BASE_LIST marker for the variable argument list.

**/
VOID
EFIAPI
DebugBPrint (
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  IN  BASE_LIST    BaseListMarker
  )
{
  DebugPrintMarker (ErrorLevel, Format, mVaListNull, BaseListMarker);
}

/**
  Prints an assert message containing a filename, line number, and description.
  This may be followed by a breakpoint or a dead loop.

  Print a message of the form "ASSERT <FileName>(<LineNumber>): <Description>\n"
  to the debug output device.  If DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED bit of
  PcdDebugProperyMask is set then CpuBreakpoint() is called. Otherwise, if
  DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED bit of PcdDebugProperyMask is set then
  CpuDeadLoop() is called.  If neither of these bits are set, then this function
  returns immediately after the message is printed to the debug output device.
  DebugAssert() must actively prevent recursion.  If DebugAssert() is called while
  processing another DebugAssert(), then DebugAssert() must return immediately.

  If FileName is NULL, then a <FileName> string of "(NULL) Filename" is printed.
  If Description is NULL, then a <Description> string of "(NULL) Description" is printed.

  @param  FileName     The pointer to the name of the source file that generated the assert condition.
  @param  LineNumber   The line number in the source file that generated the assert condition
  @param  Description  The pointer to the description of the assert condition.

**/
VOID
EFIAPI
DebugAssert (
  IN CONST CHAR8  *FileName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *Description
  )
{
  CHAR8                Buffer[MAX_DEBUG_MESSAGE_LENGTH];
  UINT32               ResetDelay;
  EFI_VIRTUAL_ADDRESS  ScratchRegBase;
  UINTN                ScratchRegSize;
  UINT64               FileNameScratchBase;
  UINT64               LineNumScratchBase;
  UINT64               FwNameScratchBase;
  EFI_STATUS           Status;

  //
  // Generate the ASSERT() message in Ascii format
  //
  AsciiSPrint (Buffer, sizeof (Buffer), "ASSERT [%a] %a(%d): %a\n", gEfiCallerBaseName, FileName, LineNumber, Description);

  //
  // Send the print string to FFA
  //
  BaseDebugHafniumPrint (Buffer);

  //
  // Should we log the information to scratch registers.
  //
  if (PcdGetBool (PcdNvLogToScratchRegs) == TRUE) {
    Status = GetDeviceRegion ("tegra-scratch", &ScratchRegBase, &ScratchRegSize);
    if (EFI_ERROR (Status)) {
      AsciiSPrint (Buffer, sizeof (Buffer), "Failed to get Scratch Reg Base %u\n", Status);
      goto bkpt;
    }

    FwNameScratchBase   = ScratchRegBase + (PcdGet32 (PcdNvFwNameStartReg) * sizeof (UINT32));
    FileNameScratchBase = ScratchRegBase + (PcdGet32 (PcdNvFileNameStartReg) * sizeof (UINT32));
    LineNumScratchBase  = ScratchRegBase + (PcdGet32 (PcdNvLineNumStartReg) * sizeof (UINT32));

    LogStringToScratchRegisters ((const CHAR8 *)PcdGetPtr (PcdNvFirmwareStr), FwNameScratchBase, 1);
    LogFileNameToScratchRegisters (
      FileName,
      FileNameScratchBase,
      PcdGet32 (PcdNvFileNameRegLimit)
      );
    LogLineNumToScratchRegisters (
      LineNumber,
      LineNumScratchBase,
      PcdGet32 (PcdNvLineNumRegLimit)
      );
  }

bkpt:
  //
  // Generate a Breakpoint, DeadLoop, or NOP based on PCD settings
  //
  if ((PcdGet8 (PcdDebugPropertyMask) & DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED) != 0) {
    CpuBreakpoint ();
  } else if ((PcdGet8 (PcdDebugPropertyMask) & DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED) != 0) {
    CpuDeadLoop ();
  } else if ((PcdGet8 (PcdDebugPropertyMask) & DEBUG_PROPERTY_ASSERT_RESET_ENABLED) != 0) {
    ResetDelay = PcdGet32 (PcdAssertResetTimeoutValue);
    if (ResetDelay > 0) {
      AsciiSPrint (Buffer, sizeof (Buffer), "\nResetting the system in %u seconds.\n", ResetDelay);
      BaseDebugHafniumPrint (Buffer);
      MicroSecondDelay (ResetDelay * 1000000);
    }
  }

  ResetCold ();
}

/**
  Fills a target buffer with PcdDebugClearMemoryValue, and returns the target buffer.

  This function fills Length bytes of Buffer with the value specified by
  PcdDebugClearMemoryValue, and returns Buffer.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param   Buffer  The pointer to the target buffer to be filled with PcdDebugClearMemoryValue.
  @param   Length  The number of bytes in Buffer to fill with zeros PcdDebugClearMemoryValue.

  @return  Buffer  The pointer to the target buffer filled with PcdDebugClearMemoryValue.

**/
VOID *
EFIAPI
DebugClearMemory (
  OUT VOID  *Buffer,
  IN UINTN  Length
  )
{
  //
  // If Buffer is NULL, then ASSERT().
  //
  ASSERT (Buffer != NULL);

  //
  // SetMem() checks for the the ASSERT() condition on Length and returns Buffer
  //
  return SetMem (Buffer, Length, PcdGet8 (PcdDebugClearMemoryValue));
}

/**
  Returns TRUE if ASSERT() macros are enabled.

  This function returns TRUE if the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of
  PcdDebugProperyMask is set.  Otherwise FALSE is returned.

  @retval  TRUE    The DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of PcdDebugProperyMask is set.
  @retval  FALSE   The DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugAssertEnabled (
  VOID
  )
{
  return (BOOLEAN)((PcdGet8 (PcdDebugPropertyMask) & DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED) != 0);
}

/**
  Returns TRUE if DEBUG() macros are enabled.

  This function returns TRUE if the DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of
  PcdDebugProperyMask is set.  Otherwise FALSE is returned.

  @retval  TRUE    The DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of PcdDebugProperyMask is set.
  @retval  FALSE   The DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugPrintEnabled (
  VOID
  )
{
  return (BOOLEAN)((PcdGet8 (PcdDebugPropertyMask) & DEBUG_PROPERTY_DEBUG_PRINT_ENABLED) != 0);
}

/**
  Returns TRUE if DEBUG_CODE() macros are enabled.

  This function returns TRUE if the DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of
  PcdDebugProperyMask is set.  Otherwise FALSE is returned.

  @retval  TRUE    The DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of PcdDebugProperyMask is set.
  @retval  FALSE   The DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugCodeEnabled (
  VOID
  )
{
  return (BOOLEAN)((PcdGet8 (PcdDebugPropertyMask) & DEBUG_PROPERTY_DEBUG_CODE_ENABLED) != 0);
}

/**
  Returns TRUE if DEBUG_CLEAR_MEMORY() macro is enabled.

  This function returns TRUE if the DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED bit of
  PcdDebugProperyMask is set.  Otherwise FALSE is returned.

  @retval  TRUE    The DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED bit of PcdDebugProperyMask is set.
  @retval  FALSE   The DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugClearMemoryEnabled (
  VOID
  )
{
  return (BOOLEAN)((PcdGet8 (PcdDebugPropertyMask) & DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED) != 0);
}

/**
  Returns TRUE if any one of the bit is set both in ErrorLevel and PcdFixedDebugPrintErrorLevel.

  This function compares the bit mask of ErrorLevel and PcdFixedDebugPrintErrorLevel.

  @retval  TRUE    Current ErrorLevel is supported.
  @retval  FALSE   Current ErrorLevel is not supported.

**/
BOOLEAN
EFIAPI
DebugPrintLevelEnabled (
  IN  CONST UINTN  ErrorLevel
  )
{
  return (BOOLEAN)((ErrorLevel & PcdGet32 (PcdFixedDebugPrintErrorLevel)) != 0);
}
