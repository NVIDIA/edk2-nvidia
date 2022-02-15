/** @file
  EFI Graphics Output Protocol Test

  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UnitTestLib.h>

#include <Protocol/GraphicsOutput.h>

#define UNIT_TEST_NAME    "EFI Graphics Output Protocol test"
#define UNIT_TEST_VERSION "0.1.0"

/// No pixel channel intensity.
#define INTENSITY_NONE  0x00
/// Low pixel channel intensity.
#define INTENSITY_LOW   0x40
/// High pixel channel intensity.
#define INTENSITY_HIGH  0xBF
/// Full pixel channel intensity
#define INTENSITY_FULL  0xFF

/// A list of GUIDs to try and locate a valid EFI GOP instance with.
STATIC EFI_GUID* mEfiGopProtocolGuids[] = {
  &gEfiGraphicsOutputProtocolGuid,
  &gNVIDIATestGraphicsOutputProtocolGuid,
  NULL
};

/// Structure which wraps context for all tests in the EFI GOP test
/// suite.
typedef struct {
  /// Pointer to the EFI Graphics Output Protocol instance under test.
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GopProtocol;
  /// Pointer to a scratch software Blt buffer.
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer;
} EFI_GOP_TEST_SUITE_CONTEXT;

/// Module-wide test suite context, managed by the test suite setup
/// and teardown functions.
STATIC EFI_GOP_TEST_SUITE_CONTEXT mEfiGopTestSuiteContext;

/**
   Initialize the test suite context.

   If any of the functions fail, then ASSERT().
*/
STATIC
VOID
EFIAPI
TestSuiteSetup (
  VOID
  )
{
  EFI_GOP_TEST_SUITE_CONTEXT    *CONST Context = &mEfiGopTestSuiteContext;

  ZeroMem (Context, sizeof (*Context));
}

/**
   Release all resources acquired during test suite setup.

   If any of the functions fail, then ASSERT().
*/
STATIC
VOID
EFIAPI
TestSuiteTeardown (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_GOP_TEST_SUITE_CONTEXT    *CONST Context = &mEfiGopTestSuiteContext;

  if (Context->BltBuffer != NULL) {
    Status = gBS->FreePool (Context->BltBuffer);
    ASSERT_EFI_ERROR (Status);
  }
}

/**
   Checks if any valid mode is set, and if it isn't, attempts to set
   the default (first) mode.
*/
STATIC
UNIT_TEST_STATUS
EFIAPI
EfiGopCheckModeSet (
  IN EFI_GOP_TEST_SUITE_CONTEXT *CONST Context
  )
{
  EFI_STATUS                    Status;
  EFI_GUID                      **GopProtocolGuid;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GopProtocol;
  CONST UINT32                  DefaultModeNumber = 0;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer;

  if (Context->GopProtocol == NULL) {
    GopProtocolGuid = &mEfiGopProtocolGuids[0];
    for (; GopProtocolGuid != NULL; ++GopProtocolGuid) {
      Status = gBS->LocateProtocol (*GopProtocolGuid, NULL,
                                    (VOID**) &GopProtocol);
      if (!EFI_ERROR (Status)) {
        break;
      }
    }
    if (GopProtocolGuid == NULL) {
      DEBUG ((
        DEBUG_WARN,
        "%a: could not locate EFI GOP protocol instance: %r\r\n",
        __FUNCTION__, Status
      ));
      return UNIT_TEST_ERROR_PREREQUISITE_NOT_MET;
    }
    Context->GopProtocol = GopProtocol;
  }
  ASSERT (Context->GopProtocol != NULL);

  GopProtocol = Context->GopProtocol;
  if (!(GopProtocol->Mode->Mode < GopProtocol->Mode->MaxMode)) {
    Status = GopProtocol->SetMode (GopProtocol, DefaultModeNumber);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_WARN,
        "%a: SetMode failed: %r\r\n",
        __FUNCTION__, Status
      ));
      return UNIT_TEST_ERROR_PREREQUISITE_NOT_MET;
    }
  }
  ASSERT (GopProtocol->Mode->Mode < GopProtocol->Mode->MaxMode);

  BltBuffer = Context->BltBuffer;
  if (BltBuffer == NULL) {
    Status = gBS->AllocatePool (EfiBootServicesData,
                                GopProtocol->Mode->Info->HorizontalResolution
                                * GopProtocol->Mode->Info->VerticalResolution
                                * sizeof (*BltBuffer),
                                (VOID**) &BltBuffer);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_WARN,
        "%a: AllocatePool failed: %r\r\n",
        __FUNCTION__, Status
      ));
      return UNIT_TEST_ERROR_PREREQUISITE_NOT_MET;
    }
    Context->BltBuffer = BltBuffer;
  }
  ASSERT (Context->BltBuffer != NULL);

  return UNIT_TEST_PASSED;
}

/**
   Draws 8 vertical colored bars within the specified region, varying
   each color channel between the specified low and high intensities.
*/
STATIC
UNIT_TEST_STATUS
EfiGopDrawBarsVertical (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL *CONST GopProtocol,
  IN CONST UINTN                  DestinationX,
  IN CONST UINTN                  DestinationY,
  IN CONST UINTN                  Width,
  IN CONST UINTN                  Height,
  IN CONST UINT8                  LowIntensity,
  IN CONST UINT8                  HighIntensity
  )
{
  EFI_STATUS                        Status;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL     BltPixel;
  UINTN                             BarIndex;
  CONST UINTN                       BarCount = 8;

  for (BarIndex = 0; BarIndex < BarCount; ++BarIndex) {
    ZeroMem (&BltPixel, sizeof (BltPixel));
    BltPixel.Blue  = BarIndex & 0x1 ? HighIntensity : LowIntensity;
    BltPixel.Green = BarIndex & 0x2 ? HighIntensity : LowIntensity;
    BltPixel.Red   = BarIndex & 0x4 ? HighIntensity : LowIntensity;

    Status = GopProtocol->Blt (GopProtocol,
                               &BltPixel,
                               EfiBltVideoFill,
                               0, 0,
                               DestinationX + Width * BarIndex / BarCount,
                               DestinationY,
                               Width / BarCount,
                               Height,
                               0);
    UT_ASSERT_NOT_EFI_ERROR (Status);
  }

  return UNIT_TEST_PASSED;
}

/**
   Reads the framebuffer back into a software Blt buffer and
   calculates CRC-32 checksum of the specified rectangle.
*/
STATIC
UNIT_TEST_STATUS
EfiGopCalculateCrc32 (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL  *CONST GopProtocol,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CONST BltBuffer,
  IN  CONST UINTN                   SourceX,
  IN  CONST UINTN                   SourceY,
  IN  CONST UINTN                   Width,
  IN  CONST UINTN                   Height,
  OUT UINT32                        *CONST Crc32
  )
{
  EFI_STATUS    Status;

  Status = GopProtocol->Blt (GopProtocol,
                             BltBuffer,
                             EfiBltVideoToBltBuffer,
                             SourceX,
                             SourceY,
                             0, 0,
                             Width,
                             Height,
                             0);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Status = gBS->CalculateCrc32 ((VOID*) BltBuffer,
                                Width
                                * Height
                                * sizeof (*BltBuffer),
                                Crc32);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
   Performs a simple Blt test by drawing colored bars on the screen.
*/
STATIC
UNIT_TEST_STATUS
EFIAPI
EfiGopBltTest (
  IN EFI_GOP_TEST_SUITE_CONTEXT *CONST Context
  )
{
  UNIT_TEST_STATUS              TestStatus;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GopProtocol;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer;
  UINT32                        HorizontalResolution;
  UINT32                        VerticalResolution;
  UINT32                        Crc32;

  GopProtocol          = Context->GopProtocol;
  BltBuffer            = Context->BltBuffer;
  HorizontalResolution = GopProtocol->Mode->Info->HorizontalResolution;
  VerticalResolution   = GopProtocol->Mode->Info->VerticalResolution;

  /* Make sure the resolution is not garbage */
  UT_ASSERT_NOT_EQUAL (HorizontalResolution, 0);
  UT_ASSERT_NOT_EQUAL (VerticalResolution, 0);

  TestStatus = EfiGopDrawBarsVertical (GopProtocol,
                                       0, 0,
                                       HorizontalResolution,
                                       VerticalResolution / 2,
                                       INTENSITY_NONE, INTENSITY_FULL);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  TestStatus = EfiGopDrawBarsVertical (GopProtocol,
                                       0, VerticalResolution / 2,
                                       HorizontalResolution,
                                       VerticalResolution / 2,
                                       INTENSITY_LOW, INTENSITY_HIGH);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  TestStatus = EfiGopCalculateCrc32 (GopProtocol, BltBuffer,
                                     0, 0,
                                     HorizontalResolution,
                                     VerticalResolution,
                                     &Crc32);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  UT_LOG_INFO ("%a: CRC-32: %08x\r\n", __FUNCTION__, Crc32);
  return UNIT_TEST_PASSED;
}

/**
   Initialize the test suite.

   @param [in] Framework Unit test framework for the suite.

   @retval EFI_SUCCESS Suite initialized successfully.
*/
STATIC
EFI_STATUS
EFIAPI
InitTestSuite (
  IN UNIT_TEST_FRAMEWORK_HANDLE Framework
  )
{
  EFI_STATUS                Status;
  UNIT_TEST_SUITE_HANDLE    TestSuite;

  Status = CreateUnitTestSuite (&TestSuite,
                                Framework,
                                "EFI Graphics Output Protocol Tests",
                                "NVIDIA-Internal.EfiGop",
                                TestSuiteSetup,
                                TestSuiteTeardown);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to create the test suite."
      " Status = %r\n", __FUNCTION__, Status
    ));
    return Status;
  }

  AddTestCase (TestSuite,
               "EFI GOP Blt Test",
               "EfiGopBltTest",
               (UNIT_TEST_FUNCTION) EfiGopBltTest,
               (UNIT_TEST_PREREQUISITE) EfiGopCheckModeSet,
               NULL,
               (UNIT_TEST_CONTEXT) &mEfiGopTestSuiteContext);

  return Status;
}

/**
   Run the EFI GOP test in UEFI DXE stage / UEFI shell.

   @param [in] ImageHandle UEFI image handle.
   @param [in] SystemTable UEFI System Table.

   @retval EFI_SUCCESS All tests ran successfully.
*/
EFI_STATUS
EFIAPI
EfiGopTestDxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;

  DEBUG ((DEBUG_INFO, "%a v%a" "\r\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  Status = InitUnitTestFramework (&Framework,
                                  UNIT_TEST_NAME,
                                  gEfiCallerBaseName,
                                  UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: InitUnitTestFramework failed."
      " Status = %r\n", __FUNCTION__, Status
    ));
    return Status;
  }

  Status = InitTestSuite (Framework);
  if (!EFI_ERROR (Status)) {
    Status = RunAllTestSuites (Framework);
  }

  FreeUnitTestFramework (Framework);
  return Status;
}
