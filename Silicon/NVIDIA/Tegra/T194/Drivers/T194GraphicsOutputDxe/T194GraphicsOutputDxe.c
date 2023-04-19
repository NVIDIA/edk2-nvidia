/** @file

  XUDC Driver

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2011 - 2020, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DisplayDeviceTreeHelperLib.h>
#include <Library/DmaLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/ClockNodeProtocol.h>

#include <libfdt.h>

#include "T194GraphicsOutputDxe.h"

EFI_EVENT  FdtInstallEvent;

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra194-dc", &gNVIDIANonDiscoverableT194DisplayDeviceGuid },
  { NULL,                 NULL                                         }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA T194 display driver",
  .UseDriverBinding                = TRUE,
  .AutoResetModule                 = FALSE,
  .SkipEdkiiNondiscoverableInstall = TRUE
};

/** GraphicsOutput Protocol function, mapping to
  EFI_GRAPHICS_OUTPUT_PROTOCOL.QueryMode
**/
STATIC
EFI_STATUS
EFIAPI
GraphicsQueryMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL           *This,
  IN UINT32                                 ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
{
  EFI_STATUS    Status = EFI_SUCCESS;
  GOP_INSTANCE  *Instance;

  Instance = GOP_INSTANCE_FROM_GOP_THIS (This);

  // Error checking
  if ((This == NULL) ||
      (Info == NULL) ||
      (SizeOfInfo == NULL) ||
      ((This != NULL) && (This->Mode == NULL)) ||
      (ModeNumber >= This->Mode->MaxMode))
  {
    DEBUG ((DEBUG_ERROR, "GraphicsQueryMode: ERROR - For mode number %d : Invalid Parameter.\n", ModeNumber));
    Status = EFI_INVALID_PARAMETER;
    goto EXIT;
  }

  *Info = AllocatePool (sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  if (*Info == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  CopyMem (*Info, &Instance->ModeInfo[Instance->ActiveHeadIndex], *SizeOfInfo);

EXIT:
  return Status;
}

/** GraphicsOutput Protocol function, mapping to
  EFI_GRAPHICS_OUTPUT_PROTOCOL.SetMode
**/
STATIC
EFI_STATUS
EFIAPI
GraphicsSetMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
  IN UINT32                        ModeNumber
  )
{
  EFI_STATUS                     Status;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  FillColour;

  // Check if this mode is supported
  if (ModeNumber >= This->Mode->MaxMode) {
    DEBUG ((DEBUG_ERROR, "GraphicsSetMode: ERROR - Unsupported mode number %d .\n", ModeNumber));
    return EFI_UNSUPPORTED;
  }

  // Update the UEFI mode information
  This->Mode->Mode = ModeNumber;

  // The UEFI spec requires that we now clear the visible portions of the
  // output display to black.

  // Set the fill colour to black
  SetMem (&FillColour, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL), 0x0);

  // Fill the entire visible area with the same colour.
  Status = This->Blt (
                   This,
                   &FillColour,
                   EfiBltVideoFill,
                   0,
                   0,
                   0,
                   0,
                   This->Mode->Info->HorizontalResolution,
                   This->Mode->Info->VerticalResolution,
                   0
                   );

  return Status;
}

/***************************************
 * GraphicsOutput Protocol function, mapping to
 * EFI_GRAPHICS_OUTPUT_PROTOCOL.Blt
 *
 *  ***************************************/
STATIC
EFI_STATUS
EFIAPI
GraphicsBlt (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *BltBuffer      OPTIONAL,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN UINTN                              SourceX,
  IN UINTN                              SourceY,
  IN UINTN                              DestinationX,
  IN UINTN                              DestinationY,
  IN UINTN                              Width,
  IN UINTN                              Height,
  IN UINTN                              Delta           OPTIONAL    // Number of BYTES in a row of the BltBuffer
  )
{
  GOP_INSTANCE  *Instance;

  Instance = GOP_INSTANCE_FROM_GOP_THIS (This);

  return FrameBufferBlt (
           Instance->Configure,
           BltBuffer,
           BltOperation,
           SourceX,
           SourceY,
           DestinationX,
           DestinationY,
           Width,
           Height,
           Delta
           );
}

/***************************************
  Read a Display Controller register

  @param [in]  HeadIndex         INTN of Head index
  @param [in]  RegOffset         UINT32 of Head register byte offset
  @param [out] Data              *UINT32 to store read data
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Invalid Head index
 ***************************************/
STATIC
EFI_STATUS
ReadDcReg32 (
  IN CONST  INTN    HeadIndex,
  IN CONST  UINT32  RegOffset,
  OUT       UINT32  *Data
  )
{
  if ((HeadIndex == DC_HEAD_INDEX_UNKNOWN) || (Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Data = MmioRead32 (DC_HEAD_0_BASE_ADDR + DC_PER_HEAD_OFFSET*HeadIndex + RegOffset);
  return EFI_SUCCESS;
}

/***************************************
  Write a Display Controller register

  @param [in]  HeadIndex         INTN of Head index
  @param [in]  RegOffset         UINT32 of Head register byte offset
  @param [in]  Data              UINT32 of data to write
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Invalid Head index
 ***************************************/
STATIC
EFI_STATUS
WriteDcReg32 (
  IN CONST  INTN    HeadIndex,
  IN CONST  UINT32  RegOffset,
  IN CONST  UINT32  Data
  )
{
  if (HeadIndex == DC_HEAD_INDEX_UNKNOWN) {
    return EFI_INVALID_PARAMETER;
  }

  MmioWrite32 (DC_HEAD_0_BASE_ADDR + DC_PER_HEAD_OFFSET*HeadIndex + RegOffset, Data);
  return EFI_SUCCESS;
}

/***************************************
  Read a Window register on the given Head

  @param [in]  HeadIndex         INTN of Head index
  @param [in]  WindowIndex       INTN of Window index
  @param [in]  WindowOffset      UINT32 of Window register byte offset
  @param [out] Reg               UINT32* to return window register data
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Invalid Head or Window index
 ***************************************/
STATIC
EFI_STATUS
ReadDcWinReg32 (
  IN CONST  INTN    HeadIndex,
  IN CONST  INTN    WindowIndex,
  IN CONST  UINT32  WindowOffset,
  OUT       UINT32  *Reg
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((WindowIndex < 0) || (WindowIndex > WINDOW_INDEX_MAX)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ReadDcReg32 (HeadIndex, (WindowIndex * DC_PER_WINDOW_OFFSET) + WindowOffset, Reg);

  return Status;
}

/***************************************
  Write a Window register on the given Head index

  @param [in]  HeadIndex         INTN of Head index
  @param [in]  WindowIndex       INTN of Window index
  @param [in]  WindowOffset      UINT32 of Window register byte offset
  @param [in]  Reg               UINT32 to window register data to write
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Failed to retrieve Head index
 ***************************************/
STATIC
EFI_STATUS
WriteDcWinReg32 (
  IN CONST  INTN    HeadIndex,
  IN CONST  INTN    WindowIndex,
  IN CONST  UINT32  WindowOffset,
  IN CONST  UINT32  Data
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((WindowIndex < 0) || (WindowIndex > WINDOW_INDEX_MAX)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = WriteDcReg32 (HeadIndex, (WindowIndex * DC_PER_WINDOW_OFFSET) + WindowOffset, Data);

  return Status;
}

/***************************************
  debug dump of registers for the indicated Head.index
  This function will also do some sanity checks as described
  in the ardisplay.html manual:

  @param [in]  HeadIndex         INTN of Head index
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Failed to retrieve Head data
 ***************************************/
STATIC
EFI_STATUS
DumpRegsForThisHead (
  IN CONST  INTN  HeadIndex
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      Data;
  UINT32      VSyncWidth;
  UINT32      HSyncWidth;
  UINT32      VFrontPorch;
  UINT32      HFrontPorch;
  UINT32      VBackPorch;
  UINT32      HBackPorch;
  UINT32      VDispActive;
  UINT32      HDispActive;

  if (EFI_ERROR (ReadDcReg32 (HeadIndex, DC_DISP_SYNC_WIDTH_OFFSET, &Data))) {
    return EFI_INVALID_PARAMETER;
  } else {
    DEBUG ((DEBUG_ERROR, "Head index=%d DC_DISP_SYNC_WIDTH  = 0x%08x\n", HeadIndex, Data));
    VSyncWidth = (Data >> 16) & MAX_UINT16;
    HSyncWidth = Data & MAX_UINT16;
  }

  if (EFI_ERROR (ReadDcReg32 (HeadIndex, DC_DISP_BACK_PORCH_OFFSET, &Data))) {
    return EFI_INVALID_PARAMETER;
  } else {
    DEBUG ((DEBUG_ERROR, "Head index=%d DC_DISP_BACK_PORCH  = 0x%08x\n", HeadIndex, Data));
    VBackPorch = (Data >> 16) & MAX_UINT16;
    HBackPorch = Data & MAX_UINT16;
  }

  if (EFI_ERROR (ReadDcReg32 (HeadIndex, DC_DISP_FRONT_PORCH_OFFSET, &Data))) {
    return EFI_INVALID_PARAMETER;
  } else {
    DEBUG ((DEBUG_ERROR, "Head index=%d DC_DISP_FRONT_PORCH = 0x%08x\n", HeadIndex, Data));
    VFrontPorch = (Data >> 16) & MAX_UINT16;
    HFrontPorch = Data & MAX_UINT16;
  }

  if (EFI_ERROR (ReadDcReg32 (HeadIndex, DC_DISP_DISP_ACTIVE_OFFSET, &Data))) {
    return EFI_INVALID_PARAMETER;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "Head index=%d DC_DISP_DISP_ACTIVE = 0x%08x = %u x %u\n",
      HeadIndex,
      Data,
      Data & 0x7fff,
      (Data >> 16) & 0x7fff
      ));
    VDispActive = (Data >> 16) & MAX_UINT16;
    HDispActive = Data & MAX_UINT16;
  }

  return Status;
}

/***************************************
  Return the first enabled+usable window on the given Head index.
  The Window index search always starts at 0 and runs to WINDOW_INDEX_MAX
  (inclusive).

  @param [in]  HeadIndex         INTN  of Head index
  @param [in]  WindowState       WindowState criteria
  @param [out] WinowIndexFound   INTN* of Window index, if usable
  @retval EFI_SUCCESS            A  usable window was found on the given head
  @retval EFI_NOT_FOUND          No usable window was found on the given head
  @retval EFI_INVALID_PARAMETER  Error during usable window search
 ***************************************/
STATIC
EFI_STATUS
GetFirstWinForThisHead (
  IN CONST  INTN         HeadIndex,
  IN CONST  WindowState  WinStateCriteria,
  OUT       INTN         *WindowIndexFound
  )
{
  EFI_STATUS  Status;
  INTN        WindowIndex;
  UINT32      WindowOffset, Reg;
  BOOLEAN     FoundActiveWindow = FALSE;
  UINT32      WindowHeight, WindowWidth;

  *WindowIndexFound = WINDOW_INDEX_UNKNOWN;

  for (WindowIndex = 0; WindowIndex <= WINDOW_INDEX_MAX; WindowIndex++ ) {
    /* first check: see if the window is enabled on the given head */
    WindowOffset = DC_A_WIN_AD_WIN_OPTIONS_OFFSET;
    Status       = ReadDcWinReg32 (HeadIndex, WindowIndex, WindowOffset, &Reg);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: error reading Window index=%d @ WindowOffset=0x%08x\n", __FUNCTION__, WindowIndex, WindowOffset));
      return EFI_INVALID_PARAMETER;
    }

    if ((Reg & DC_A_WIN_AD_WIN_OPTIONS_AD_WIN_ENABLE_ENABLE) == DC_A_WIN_AD_WIN_OPTIONS_AD_WIN_ENABLE_ENABLE) {
      DEBUG ((DEBUG_INFO, "%a: Head index %d Window index=%d  Enabled\n", __FUNCTION__, HeadIndex, WindowIndex));
    } else {
      DEBUG ((DEBUG_INFO, "%a: Head index %d Window index=%d  Disabled\n", __FUNCTION__, HeadIndex, WindowIndex));
      /* if a window is disabled don't bother checking the dimensions */
      continue;
    }

    if (WinStateCriteria == WindowStateEnabled) {
      DEBUG ((
        DEBUG_ERROR,
        "Head index %d: Window index=%d enabled\n",
        HeadIndex,
        WindowIndex
        ));
      *WindowIndexFound = WindowIndex;
      FoundActiveWindow = TRUE;
      break;
    }

    if (WinStateCriteria == WindowStateUsable) {
      /* second check: confirm the window dimensions are acceptable for UEFI */
      WindowOffset = DC_A_WIN_AD_PCALC_WINDOW_SET_CROPPED_SIZE_IN_0;
      Status       = ReadDcWinReg32 (HeadIndex, WindowIndex, WindowOffset, &Reg);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: error reading Window index=%d @ WindowOffset 0x%08x\n", __FUNCTION__, WindowIndex, WindowOffset));
        return EFI_INVALID_PARAMETER;
      }

      WindowWidth  = Reg & 0x7fff;
      WindowHeight = (Reg >> 16) & 0x7fff;
      if ((WindowWidth >= WIN_CROPPED_SIZE_IN_MIN_WIDTH) && (WindowHeight >= WIN_CROPPED_SIZE_IN_MIN_HEIGHT)) {
        DEBUG ((
          DEBUG_ERROR,
          "Head index %d: Window index=%d %ux%u >= %dx%d: enabled and usable by UEFI\n",
          HeadIndex,
          WindowIndex,
          WindowWidth,
          WindowHeight,
          WIN_CROPPED_SIZE_IN_MIN_WIDTH,
          WIN_CROPPED_SIZE_IN_MIN_HEIGHT
          ));
        *WindowIndexFound = WindowIndex;
        FoundActiveWindow = TRUE;
        break;
      } else {
        DEBUG ((
          DEBUG_ERROR,
          "Head index %d: Window index=%d %ux%u < %dx%d: enabled but NOT usable; keep searching\n",
          HeadIndex,
          WindowIndex,
          WindowWidth,
          WindowHeight,
          WIN_CROPPED_SIZE_IN_MIN_WIDTH,
          WIN_CROPPED_SIZE_IN_MIN_HEIGHT
          ));
      }
    }
  }

  if (FoundActiveWindow == FALSE) {
    Status = EFI_NOT_FOUND;
  }

  return Status;
}

/***************************************
  Return Head index based on BaseAddress

  @param [in]  BaseAddress       EFI_PHYSICAL_ADDRESS of Display head instance
  @param [out] HeadIndex         INTN* of Head index inferred from BaseAddress
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Failed to retrieve Head index
 ***************************************/
STATIC
EFI_STATUS
GetDispHeadFromAddr (
  IN CONST  EFI_PHYSICAL_ADDRESS  BaseAddress,
  OUT       INTN                  *HeadIndex
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  switch (BaseAddress) {
    case DC_HEAD_0_BASE_ADDR:
      *HeadIndex = 0;
      break;
    case DC_HEAD_1_BASE_ADDR:
      *HeadIndex = 1;
      break;
    case DC_HEAD_2_BASE_ADDR:
      *HeadIndex = 2;
      break;
    default:
      *HeadIndex = 0;
      Status     = EFI_INVALID_PARAMETER;
      break;
  }

  return Status;
}

/***************************************
  Retrieve LUT region details.

  @param [in]  GopInstance  ptr to GOP_INSTANCE
  @param [in]  HeadIndex    head index
  @param [out] LutBase      on return: LUT region start address
  @param [out] LutSize      on return: LUT region size

  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Failed to retrieve region details
 ***************************************/
STATIC
EFI_STATUS
GetLutRegion (
  IN  CONST GOP_INSTANCE    *CONST  GopInstance,
  IN  CONST INTN                    HeadIndex,
  OUT EFI_PHYSICAL_ADDRESS  *CONST  LutBase,
  OUT UINTN                 *CONST  LutSize
  )
{
  UINT32  Low32, High32;

  if (GopInstance == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  (void)ReadDcReg32 (HeadIndex, COREPVT_HEAD_SET_OUTPUT_LUT_BASE_LO_OFFSET, &Low32);
  (void)ReadDcReg32 (HeadIndex, COREPVT_HEAD_SET_OUTPUT_LUT_BASE_HI_OFFSET, &High32);
  *LutBase = (EFI_PHYSICAL_ADDRESS)(((UINT64)High32) << 32) | ((UINT64)Low32);

  (void)ReadDcReg32 (HeadIndex, CORE_HEAD_SET_CONTROL_OUTPUT_LUT_OFFSET, &Low32);
  switch (CORE_HEAD_SET_CONTROL_OUTPUT_LUT_SIZE (Low32)) {
    case CORE_HEAD_SET_CONTROL_OUTPUT_LUT_SIZE_257:
      *LutSize = 257 * sizeof (UINT64);
      break;
    case CORE_HEAD_SET_CONTROL_OUTPUT_LUT_SIZE_1025:
      *LutSize = 1025 * sizeof (UINT64);
      break;
    default:
      return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/***************************************
  Check if given head is active. The criteria for
  whether a display head is active is a non-zero,
  non-reset value in that head's DC_DISP_DISP_ACTIVE
  register.
  Note that if a head is not enabled/mapped into
  a supported iommu range, there will be a fault
  when accessing that head's register in the OS.

  @param [in]  *GopInstance   ptr to GOP_INSTANCE
  @param [in]  HeadIndex      Head index to check if active

  @retval TRUE  head is active
  @retval FALSE head is inactive
 ***************************************/
STATIC
BOOLEAN
IsHeadActive (
  IN CONST  GOP_INSTANCE  *GopInstance,
  IN CONST  INTN          HeadIndex
  )
{
  UINT32  DispActive;

  (void)ReadDcReg32 (HeadIndex, DC_DISP_DISP_ACTIVE_OFFSET, &DispActive);

  return (DispActive != 0) && (DispActive != DC_DISP_DISP_ACTIVE_RESET_VAL);
}

/***************************************
  support function for FdtInstalled callback
  to update the appropriate fb?_carveout node.

  @param [in]  DtBlob      ptr to Device Tree blob
  @param [in]  HeadIndex   Head index to update
  @param [in]  FbAddress   Address of FB
  @param [in]  FbSize      Size of FB
  @param [in]  LutAddress  Address of LUT
  @param [in]  LutSize     Size of LUT

  @retval TRUE   Operation successful.
  @retval FALSE  Error occurred
 ***************************************/
STATIC
BOOLEAN
UpdateFbCarveoutNode (
  IN VOID         *CONST         DtBlob,
  IN CONST INTN                  HeadIndex,
  IN CONST EFI_PHYSICAL_ADDRESS  FbAddress,
  IN CONST UINTN                 FbSize,
  IN CONST EFI_PHYSICAL_ADDRESS  LutAddress,
  IN CONST UINTN                 LutSize
  )
{
  INT32   Result;
  CHAR8   FbCarveoutPath[32];
  UINT64  RegProp[4];

  AsciiSPrint (
    FbCarveoutPath,
    sizeof (FbCarveoutPath),
    "/reserved-memory/fb%d_carveout",
    HeadIndex
    );

  Result = fdt_path_offset (DtBlob, FbCarveoutPath);
  if (Result < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting '%a' DT node offset: %a\n",
      __FUNCTION__,
      FbCarveoutPath,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  /* update the DT blob with the new FB and LUT details */
  RegProp[0] = SwapBytes64 (FbAddress);
  RegProp[1] = SwapBytes64 (FbSize);
  RegProp[2] = SwapBytes64 (LutAddress);
  RegProp[3] = SwapBytes64 (LutSize);

  Result = fdt_setprop_inplace (DtBlob, Result, "reg", RegProp, sizeof (RegProp));
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error updating '%a' DT node: %a\n",
      __FUNCTION__,
      FbCarveoutPath,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: Updated '%a': FbAddress  = 0x%016lx FbSize  = 0x%lx (%lu)\n"
    "%a: Updated '%a': LutAddress = 0x%016lx LutSize = 0x%lx (%lu)\n",
    __FUNCTION__,
    FbCarveoutPath,
    FbAddress,
    FbSize,
    FbSize,
    __FUNCTION__,
    FbCarveoutPath,
    LutAddress,
    LutSize,
    LutSize
    ));

  return TRUE;
}

/***************************************
  support function for FdtInstalled callback
  to update DT for given head

  @param [in]  DtBlob      ptr to Device Tree blob
  @param [in]  Private     ptr to GOP_INSTANCE
  @param [in]  HeadIndex   Head index to update

  @retval TRUE   Update successful
  @retval FALSE  Errors occurred
 ***************************************/
STATIC
BOOLEAN
UpdateDtForHead (
  IN VOID         *CONST  DtBlob,
  IN GOP_INSTANCE *CONST  Private,
  IN CONST INTN           HeadIndex
  )
{
  EFI_STATUS            Status;
  BOOLEAN               IsActive;
  EFI_PHYSICAL_ADDRESS  FbAddress, LutAddress;
  UINTN                 FbSize, LutSize;

  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *CONST  Mode =
    &Private->Mode[HeadIndex];

  DEBUG ((DEBUG_ERROR, "Mode[%d].FrameBufferBase = 0x%p\n", HeadIndex, Mode->FrameBufferBase));
  DEBUG ((DEBUG_ERROR, "Mode[%d].FrameBufferSize = %u\n", HeadIndex, Mode->FrameBufferSize));

  IsActive = IsHeadActive (Private, HeadIndex);
  if (IsActive) {
    /* Active head: use FB and LUT settings from CBoot */
    if (HeadIndex == Private->ActiveHeadIndex) {
      DEBUG ((DEBUG_ERROR, "Head index %d is active and used by UEFI\n", HeadIndex));
    } else {
      DEBUG ((DEBUG_ERROR, "Head index %d is active but not used by UEFI\n", HeadIndex));
    }

    FbAddress = Mode->FrameBufferBase;
    FbSize    = Mode->FrameBufferSize;

    Status = GetLutRegion (Private, HeadIndex, &LutAddress, &LutSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error getting LUT region, force LutAddress and LutSize to 0: %r\n", __FUNCTION__, Status));
      LutAddress = 0;
      LutSize    = 0;
    }
  } else {
    /* inactive head: zero-fill everything */
    DEBUG ((DEBUG_ERROR, "Head index %d is NOT active\n", HeadIndex));

    FbAddress = LutAddress = 0;
    FbSize    = LutSize    = 0;
  }

  if (!UpdateFbCarveoutNode (
         DtBlob,
         HeadIndex,
         FbAddress,
         FbSize,
         LutAddress,
         LutSize
         ))
  {
    return FALSE;
  }

  if (IsActive) {
    if (!UpdateDeviceTreeSimpleFramebufferInfo (
           DtBlob,
           Mode->Info,
           (UINT64)FbAddress,
           (UINT64)FbSize
           ))
    {
      return FALSE;
    }
  }

  return TRUE;
}

/***************************************
  Callback that will be invoked at FdtInstallEvent
  The fb?_carveout DT node's reg property associated
  with the active head will be updated with the
  framebuffer addreess and size information from
  CBoot.
  The other fb?_carveout DT node's reg propertys
  will be cleared to 0.

  @param [in]  Event       EFI_EVENT FdtInstallEvent
  @param [in]  Context     *GOP_INSTANCE callback context

  @retval none
 ***************************************/
STATIC
VOID
EFIAPI
FdtInstalled (
  IN EFI_EVENT            Event,
  IN GOP_INSTANCE *CONST  Private
  )
{
  EFI_STATUS  Status;
  INT32       Result;
  VOID        *DtBlob;
  INTN        HeadIndex, HeadCount;

  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid context\n", __FUNCTION__));
    return;
  }

  /* get ptr to DT blob base */
  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &DtBlob);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting DT base: %r\n", __FUNCTION__, Status));
    return;
  }

  Result = fdt_check_header (DtBlob);
  if (Result != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Error checking DT header: %a\n", __FUNCTION__, fdt_strerror (Result)));
    return;
  }

  if (Private->MaxHeadIndex == DC_HEAD_INDEX_UNKNOWN) {
    DEBUG ((DEBUG_ERROR, "%a: unsupported MaxHeadIndex=%d\n", __FUNCTION__, Private->MaxHeadIndex));
    return;
  }

  HeadCount = Private->MaxHeadIndex + 1;
  for (HeadIndex = 0; HeadIndex < HeadCount; HeadIndex++) {
    UpdateDtForHead (DtBlob, Private, HeadIndex);
  }
}

/***************************************
  initialise GOP_INSTANCE data during different phases of DeviceDiscovery.

  Some data is initialiased unconditionally as the FdtInstalled callback relies
  on those particular Private members:
    ->MaxHeadIndex                      init value of DC_HEAD_INDEX_UNKNOWN is disallowed
    ->ActiveHeadIndex                   init value of DC_HEAD_INDEX_UNKNOWN is ok
    ->Mode[HeadIndex].FrameBufferSize   requires ->ModeInfo[HeadIndex].VerticalResolution,
                                        (which requires ScreenInputSize) and Pitch
    ->Mode[HeadIndex].FrameBufferBase   requires ->Mode[HeadIndex].FrameBufferSize

  Since the FdtInstalled callback is installed when any ENABLED head+window
  is found, as opposed to ENABLED+USABLE by UEFI, those items must be initialised
  regardless of Phase.

  If Phase=DeviceDiscoveryDriverBindingStart, then additional data is
  initialised, as this implies an enabled head has a window which is usable ny
  UEFI, and the GOP protocol will be installed.

  @param[in]     Phase                 Current phase of the driver initialization
  @param[in]     ControllerHandle      Handle of the controller
  @param[in]     BaseAddress           Controller's base address, i.e. DC base address
  @param[in,out] Private               ptr to private GOP_INSTANCE
  @param[in]     HeadIndex             DC controller index; the current head to init
  @param[in]     WindowIndex           hardware window index; the enabled window

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred
 ***************************************/
EFI_STATUS
UpdateGopInfoForThisHead (
  IN  CONST NVIDIA_DEVICE_DISCOVERY_PHASES  Phase,
  IN  CONST EFI_HANDLE                      ControllerHandle,
  IN  EFI_PHYSICAL_ADDRESS                  BaseAddress,
  IN  OUT GOP_INSTANCE                      *Private,
  IN  INTN                                  HeadIndex,
  IN  INTN                                  WindowIndex
  )
{
  EFI_STATUS            Status;
  UINT32                ScreenInputSize;
  UINTN                 ConfigureSize;
  UINT32                ColorDepth;
  UINTN                 Pitch;
  UINT32                LowAddress;
  UINT32                HighAddress;
  EFI_PHYSICAL_ADDRESS  OldAddress;
  UINT32                WindowOffset;

  Private->Signature = GOP_INSTANCE_SIGNATURE;
  if (HeadIndex > Private->MaxHeadIndex) {
    Private->MaxHeadIndex = HeadIndex;
  }

  WindowOffset = WIN_COLOR_DEPTH_OFFSET;
  Status       = ReadDcWinReg32 (HeadIndex, WindowIndex, WindowOffset, &ColorDepth);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  if (ColorDepth == WIN_COLOR_DEPTH_R8G8B8A8) {
    Private->ModeInfo[HeadIndex].PixelFormat = PixelRedGreenBlueReserved8BitPerColor;
  } else if (ColorDepth == WIN_COLOR_DEPTH_B8G8R8A8) {
    Private->ModeInfo[HeadIndex].PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
  } else {
    Status = EFI_UNSUPPORTED;
    goto exit;
  }

  WindowOffset = DC_A_WIN_AD_PCALC_WINDOW_SET_CROPPED_SIZE_IN_0;
  Status       = ReadDcWinReg32 (HeadIndex, WindowIndex, WindowOffset, &ScreenInputSize);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  Private->ModeInfo[HeadIndex].Version              = 0;
  Private->ModeInfo[HeadIndex].HorizontalResolution = ScreenInputSize & MAX_UINT16;
  Private->ModeInfo[HeadIndex].VerticalResolution   = ScreenInputSize >> 16;
  Private->ModeInfo[HeadIndex].PixelFormat          = PixelRedGreenBlueReserved8BitPerColor;
  Private->ModeInfo[HeadIndex].PixelsPerScanLine    = Private->ModeInfo[HeadIndex].HorizontalResolution;
  DEBUG ((DEBUG_ERROR, "Modeinfo[%d].HorizontalResolution %u\n", HeadIndex, Private->ModeInfo[HeadIndex].HorizontalResolution));
  DEBUG ((DEBUG_ERROR, "Modeinfo[%d].VerticalResolution   %u\n", HeadIndex, Private->ModeInfo[HeadIndex].VerticalResolution));

  Pitch = Private->ModeInfo[HeadIndex].HorizontalResolution * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  if ((Pitch & -Pitch) != Pitch) {
    Pitch = (UINTN)GetPowerOfTwo32 ((UINT32)Pitch) << 1;
  }

  Private->Mode[HeadIndex].MaxMode         = 1;
  Private->Mode[HeadIndex].Mode            = 0;
  Private->Mode[HeadIndex].Info            = &Private->ModeInfo[HeadIndex];
  Private->Mode[HeadIndex].SizeOfInfo      = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  Private->Mode[HeadIndex].FrameBufferSize = Private->ModeInfo[HeadIndex].VerticalResolution * Pitch;

  if (Private->Mode[HeadIndex].FrameBufferBase == 0) {
    Status = DmaAllocateBuffer (
               EfiRuntimeServicesData,
               EFI_SIZE_TO_PAGES (Private->Mode[HeadIndex].FrameBufferSize),
               (VOID **)&Private->Mode[HeadIndex].FrameBufferBase
               );
    if (EFI_ERROR (Status)) {
      goto exit;
    }
  }

  WindowOffset = DC_A_WINBUF_AD_START_ADDR_OFFSET;
  Status       = ReadDcWinReg32 (HeadIndex, WindowIndex, WindowOffset, &LowAddress);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  WindowOffset = DC_A_WINBUF_AD_START_ADDR_HI_OFFSET;
  Status       = ReadDcWinReg32 (HeadIndex, WindowIndex, WindowOffset, &HighAddress);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  OldAddress = ((EFI_PHYSICAL_ADDRESS)HighAddress << 32) | (EFI_PHYSICAL_ADDRESS)LowAddress;
  if (OldAddress != 0) {
    CopyMem ((VOID *)Private->Mode[HeadIndex].FrameBufferBase, (CONST VOID *)OldAddress, Private->Mode[HeadIndex].FrameBufferSize);
  } else {
    ZeroMem ((VOID *)Private->Mode[HeadIndex].FrameBufferBase, Private->Mode[HeadIndex].FrameBufferSize);
  }

  // at this point all Private items required by the FdtInstalled callback has been set up
  // now need to setup required information for Private->Gop and Private->Configure
  // iff Phase == DeviceDiscoveryDriverBindingStart, since there is only one Gop and
  // one Configure supported/used by the Blt function

  if (Phase == DeviceDiscoveryDriverBindingStart) {
    if (Private->ActiveHeadIndex == DC_HEAD_INDEX_UNKNOWN) {
      Private->ActiveHeadIndex = HeadIndex;
    }

    if (Private->Configure == NULL) {
      ConfigureSize = 0;
      Status        = FrameBufferBltConfigure (
                        (VOID *)Private->Mode[HeadIndex].FrameBufferBase,
                        &Private->ModeInfo[HeadIndex],
                        NULL,
                        &ConfigureSize
                        );
      if (Status != EFI_BUFFER_TOO_SMALL) {
        if (!EFI_ERROR (Status)) {
          Status = EFI_DEVICE_ERROR;
        }

        goto exit;
      }

      Private->Configure = (FRAME_BUFFER_CONFIGURE *)AllocatePool (ConfigureSize);
      if (Private->Configure == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto exit;
      }

      Status = FrameBufferBltConfigure (
                 (VOID *)Private->Mode[HeadIndex].FrameBufferBase,
                 &Private->ModeInfo[HeadIndex],
                 Private->Configure,
                 &ConfigureSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Frame buffer not configured\n", __FUNCTION__));
        goto exit;
      }

      WindowOffset = DC_A_WINBUF_AD_START_ADDR_OFFSET;
      Status       = WriteDcWinReg32 (HeadIndex, WindowIndex, WindowOffset, Private->Mode[HeadIndex].FrameBufferBase & MAX_UINT32);
      if (EFI_ERROR (Status)) {
        goto exit;
      }

      WindowOffset = DC_A_WINBUF_AD_START_ADDR_HI_OFFSET;
      Status       = WriteDcWinReg32 (HeadIndex, WindowIndex, WindowOffset, Private->Mode[HeadIndex].FrameBufferBase >> 32);
      if (EFI_ERROR (Status)) {
        goto exit;
      }

      DEBUG ((
        DEBUG_ERROR,
        "Mode[%d].FrameBufferBase =0x%p; updated Window index %d start address\n",
        HeadIndex,
        Private->Mode[HeadIndex].FrameBufferBase,
        WindowIndex
        ));

      Private->Handle        = ControllerHandle;
      Private->Gop.QueryMode = GraphicsQueryMode;
      Private->Gop.SetMode   = GraphicsSetMode;
      Private->Gop.Blt       = GraphicsBlt;
      Private->Gop.Mode      = &Private->Mode[HeadIndex];
    }
  }

exit:

  return Status;
}

/***************************************
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred
 ***************************************/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  CONST NVIDIA_DEVICE_DISCOVERY_PHASES    Phase,
  IN  CONST EFI_HANDLE                        DriverHandle,
  IN  CONST EFI_HANDLE                        ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                    Status;
  EFI_PHYSICAL_ADDRESS          BaseAddress = 0;
  UINTN                         RegionSize;
  STATIC GOP_INSTANCE           *Private = NULL;
  SCMI_CLOCK2_PROTOCOL          *ScmiClockProtocol;
  NVIDIA_CLOCK_NODE_PROTOCOL    *ClockNodeProtocol;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *TempGopProtocol;
  BOOLEAN                       ClockEnabled;
  CHAR8                         ClockName[SCMI_MAX_STR_LEN];
  INTN                          HeadIndex;
  INTN                          WindowIndex;

  switch (Phase) {
    /******************************************************************************************/
    case DeviceDiscoveryDriverBindingSupported:

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        break;
      }

      Status = GetDispHeadFromAddr (BaseAddress, &HeadIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: error getting Head index\n", __FUNCTION__));
        break;
      }

      // some debug prints to confirm programming from Cbboot nvdisp-init
      Status = DumpRegsForThisHead (HeadIndex);

      Status = GetFirstWinForThisHead (HeadIndex, WindowStateEnabled, &WindowIndex);
      if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
      }

      // this head has an enabled window: do some sanity checks,

      Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid, NULL, (VOID **)&ScmiClockProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Head index=%d: failed to get ScmiClockProtocol\n", __FUNCTION__, HeadIndex));
        break;
      }

      Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockNodeProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Head index=%d: failed to get ClockNodeProtocol\n", __FUNCTION__, HeadIndex));
        break;
      }

      // If there are clocks listed make sure the primary one is enabled
      if ((ClockNodeProtocol != NULL) && (ClockNodeProtocol->Clocks != 0) && (ScmiClockProtocol != NULL)) {
        Status = ScmiClockProtocol->GetClockAttributes (ScmiClockProtocol, ClockNodeProtocol->ClockEntries[0].ClockId, &ClockEnabled, ClockName);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Head index=%d: failed detect clock enable state\n", __FUNCTION__, HeadIndex));
          break;
        }

        if (!ClockEnabled) {
          DEBUG ((DEBUG_ERROR, "%a: Clock not enabled for Head index %d\n", __FUNCTION__, HeadIndex));
          return EFI_UNSUPPORTED;
        }
      }

      // sanity checks passed: alloc GOP_INSTANCE Private on first Supported call

      if (Private == NULL) {
        Private = AllocateZeroPool (sizeof (GOP_INSTANCE));
        if (Private == NULL) {
          Status = EFI_OUT_OF_RESOURCES;
          break;
        }

        Private->MaxHeadIndex    = DC_HEAD_INDEX_UNKNOWN;
        Private->ActiveHeadIndex = DC_HEAD_INDEX_UNKNOWN;
        Private->DcAddr[0]       = DC_HEAD_0_BASE_ADDR;
        Private->DcAddr[1]       = DC_HEAD_1_BASE_ADDR;
        Private->DcAddr[2]       = DC_HEAD_2_BASE_ADDR;
        Private->DcAddr[3]       = DC_HEAD_3_BASE_ADDR;

        Status = gBS->CreateEventEx (
                        EVT_NOTIFY_SIGNAL,                  // Type
                        TPL_CALLBACK,                       // NotifyTpl
                        (EFI_EVENT_NOTIFY)FdtInstalled,     // NotifyFunction / callback
                        (VOID *)Private,                    // NotifyContext
                        &gFdtTableGuid,                     // EventGroup
                        &FdtInstallEvent
                        );                                  // Event
        DEBUG ((DEBUG_ERROR, "%a: FdtInstalled callback installed\n", __FUNCTION__));
      }

      // note that for eny enabled but unusable head+window config, this next call will get executed multiple times
      // update data for later usage by GetLutRegion<=FdtInstalled callback
      Status = UpdateGopInfoForThisHead (Phase, ControllerHandle, BaseAddress, Private, HeadIndex, WindowIndex);
      if (EFI_ERROR (Status)) {
        break;
      }

      /* check if there are any usable windows on the current head */
      Status = GetFirstWinForThisHead (HeadIndex, WindowStateUsable, &WindowIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Head index %d: no windows found usable by UEFI\n", __FUNCTION__, HeadIndex));
        return EFI_UNSUPPORTED;
      }

      /* found usable window on current head: return success */
      DEBUG ((DEBUG_ERROR, "%a: Head index %d: Window index %d usable by UEFI\n", __FUNCTION__, HeadIndex, WindowIndex));

      if (Private->ActiveHeadIndex != DC_HEAD_INDEX_UNKNOWN) {
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    /******************************************************************************************/
    case DeviceDiscoveryDriverBindingStart:

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        break;
      }

      Status = GetDispHeadFromAddr (BaseAddress, &HeadIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: error getting Head index\n", __FUNCTION__));
        break;
      }

      if (Private == NULL) {
        return EFI_UNSUPPORTED;
      }

      Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&TempGopProtocol);
      if (Status == EFI_SUCCESS) {
        DEBUG ((DEBUG_ERROR, "%a: GOP protocol already installed, but only one GOP instance is supported\n", __FUNCTION__));
      }

      Status = GetFirstWinForThisHead (HeadIndex, WindowStateUsable, &WindowIndex);

      if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
      }

      Status = UpdateGopInfoForThisHead (Phase, ControllerHandle, BaseAddress, Private, HeadIndex, WindowIndex);
      if (EFI_ERROR (Status)) {
        break;
      }

      // Install the Graphics Output Protocol and the Device Path
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Private->Handle,
                      &gEfiGraphicsOutputProtocolGuid,
                      &Private->Gop,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error installing GOP protocol\n", __FUNCTION__));
        return EFI_DEVICE_ERROR;
      } else {
        DEBUG ((DEBUG_ERROR, "%a: installed &Private->Gop=0x%p protocol\n", __FUNCTION__, &Private->Gop));
      }

      break;

    /******************************************************************************************/
    default:
      Status = EFI_SUCCESS;
      break;
  }

  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      if (Private->Mode[HeadIndex].FrameBufferBase != 0) {
        DmaFreeBuffer (EFI_SIZE_TO_PAGES (Private->Mode[HeadIndex].FrameBufferSize), (VOID *)Private->Mode[HeadIndex].FrameBufferBase);
        Private->Mode[HeadIndex].FrameBufferBase = 0;
        Private->Mode[HeadIndex].FrameBufferSize = 0;
      }

      FreePool (Private);
    }
  }

  return Status;
}
