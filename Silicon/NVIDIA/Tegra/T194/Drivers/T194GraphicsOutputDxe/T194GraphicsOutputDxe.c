/** @file

  XUDC Driver

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2011 - 2020, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DmaLib.h>
#include <Library/BaseMemoryLib.h>

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

STATIC CONST CHAR8  *mFbCarveoutPaths[] = {
  "/reserved-memory/fb0_carveout",
  "/reserved-memory/fb1_carveout",
  "/reserved-memory/fb2_carveout",
  "/reserved-memory/fb3_carveout"
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

  CopyMem (*Info, &Instance->ModeInfo, *SizeOfInfo);

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

/**
  Read a Window register based off of the given BaseAddress.
  Note that the BaseAddress should already adjusted for the correct Head.

  @param [in]  BaseAddress       EFI_PHYSICAL_ADDRESS of Display head instance
  @param [in]  WindowIndex       INTN of Window index
  @param [in]  WindowOffset      UINT32 of Window register byte offset
  @param [out] Reg               *UINT32 to return window register data
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Failed to retrieve Head index
**/
STATIC
EFI_STATUS
ReadDcWinReg32 (
  IN CONST  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN CONST  INTN                  WindowIndex,
  IN CONST  UINT32                WindowOffset,
  OUT       UINT32                *Reg
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((WindowIndex < 0) || (WindowIndex > WINDOW_INDEX_MAX)) {
    return EFI_INVALID_PARAMETER;
  }

  *Reg = MmioRead32 (BaseAddress + (WindowIndex * DC_PER_WINDOW_OFFSET) + WindowOffset);

  return Status;
}

/**
  Write a Window register based off of the given BaseAddress.
  Note that the BaseAddress should already adjusted for the correct Head.

  @param [in]  BaseAddress       EFI_PHYSICAL_ADDRESS of Display head instance
  @param [in]  WindowIndex       INTN of Window index
  @param [in]  WindowOffset      UINT32 of Window register byte offset
  @param [in]  Reg               UINT32 to window register data to write
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Failed to retrieve Head index
**/
STATIC
EFI_STATUS
WriteDcWinReg32 (
  IN CONST  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN CONST  INTN                  WindowIndex,
  IN CONST  UINT32                WindowOffset,
  IN CONST  UINT32                Reg
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((WindowIndex < 0) || (WindowIndex > WINDOW_INDEX_MAX)) {
    return EFI_INVALID_PARAMETER;
  }

  MmioWrite32 (BaseAddress + (WindowIndex * DC_PER_WINDOW_OFFSET) + WindowOffset, Reg);

  return Status;
}

/**
  Return the first enabled+usable window on the given Head index.
  The Window index search always starts at 0 and runs to WINDOW_INDEX_MAX
  (inclusive).

  @param [in]  BaseAddress       EFI_PHYSICAL_ADDRESS of Display head instance
  @param [in]  HeadIndex         INTN  of Head   index
  @param [out] WinowIndexFound   INTN* of Window index, if usable
  @retval EFI_SUCCESS            A  usable window was found on the given head
  @retval EFI_NOT_FOUND          No usable window was found on the given head
  @retval EFI_INVALID_PARAMETER  Error during usable window search
**/
STATIC
EFI_STATUS
GetFirstUsableWinForThisHead (
  IN CONST  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN CONST  INTN                  HeadIndex,
  OUT       INTN                  *WindowIndexFound
  )
{
  EFI_STATUS  Status;
  INTN        WindowIndex;
  UINT32      WindowOffset, Reg;
  BOOLEAN     FoundActiveWindow = FALSE;
  UINT32      WindowHeight, WindowWidth;

  for (WindowIndex = 0; WindowIndex <= WINDOW_INDEX_MAX; WindowIndex++ ) {
    /* first check: see if the window is enabled on the given head */
    WindowOffset = DC_A_WIN_AD_WIN_OPTIONS_OFFSET;
    Status       = ReadDcWinReg32 (BaseAddress, WindowIndex, WindowOffset, &Reg);
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

    /* second check: confirm the window dimensions are acceptable for UEFI */
    WindowOffset = DC_A_WIN_AD_PCALC_WINDOW_SET_CROPPED_SIZE_IN_0;
    Status       = ReadDcWinReg32 (BaseAddress, WindowIndex, WindowOffset, &Reg);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: error reading Window index=%d @ WindowOffset 0x%08x\n", __FUNCTION__, WindowIndex, WindowOffset));
      return EFI_INVALID_PARAMETER;
    }

    WindowWidth  = Reg & 0x7fff;
    WindowHeight = (Reg >> 16) & 0x7fff;
    if ((WindowWidth >= WIN_CROPPED_SIZE_IN_MIN_WIDTH) && (WindowHeight >= WIN_CROPPED_SIZE_IN_MIN_HEIGHT)) {
      DEBUG ((
        DEBUG_ERROR,
        "Head index %d: Window index=%d %ux%u >= %ux%u: acceptable to use\n",
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
        "Head index %d: Window index=%d %ux%u < %ux%u: NOT acceptable to use\n",
        HeadIndex,
        WindowIndex,
        WindowWidth,
        WindowHeight,
        WIN_CROPPED_SIZE_IN_MIN_WIDTH,
        WIN_CROPPED_SIZE_IN_MIN_HEIGHT
        ));
    }
  }

  if (FoundActiveWindow == FALSE) {
    Status = EFI_NOT_FOUND;
  }

  return Status;
}

/**
  Return Head index based on BaseAddress

  @param [in]  BaseAddress       EFI_PHYSICAL_ADDRESS of Display head instance
  @param [out] HeadIndex         INTN* of Head index inferred from BaseAddress
  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Failed to retrieve Head index
**/
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

/**
  Retrieve LUT region details.

  @param [in]  Gop      ptr to GOP_INSTANCE
  @param [out] LutBase  Holds LUT region start address on return
  @param [out] LutSize  Holds LUT region size on return

  @retval EFI_SUCCESS            Operation successful
  @retval EFI_INVALID_PARAMETER  Failed to retrieve region details
**/
STATIC
EFI_STATUS
GetLutRegion (
  IN  CONST GOP_INSTANCE    *CONST  Gop,
  OUT EFI_PHYSICAL_ADDRESS  *CONST  LutBase,
  OUT UINTN                 *CONST  LutSize
  )
{
  UINT32  Low32, High32;

  if (Gop == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Low32    = MmioRead32 (Gop->DcAddr + COREPVT_HEAD_SET_OUTPUT_LUT_BASE_LO_OFFSET);
  High32   = MmioRead32 (Gop->DcAddr + COREPVT_HEAD_SET_OUTPUT_LUT_BASE_HI_OFFSET);
  *LutBase = (EFI_PHYSICAL_ADDRESS)(((UINT64)High32) << 32) | ((UINT64)Low32);

  Low32 = MmioRead32 (Gop->DcAddr + CORE_HEAD_SET_CONTROL_OUTPUT_LUT_OFFSET);
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

   @param [in]  *Gop           ptr to GOP_INSTANCE
   @param [in]  HeadIndex      Head index to check if active

  @retval TRUE  head is active
  @retval FALSE head is inactive
 ***************************************/
STATIC
BOOLEAN
IsHeadActive (
  IN CONST  GOP_INSTANCE  *Gop,
  IN CONST  INTN          HeadIndex
  )
{
  UINT32  DispActive;

  DispActive = MmioRead32 (DC_HEAD_0_BASE_ADDR + DC_PER_HEAD_OFFSET*HeadIndex + DC_DISP_DISP_ACTIVE_OFFSET);
  return (DispActive != 0) && (DispActive != DC_DISP_DISP_ACTIVE_RESET_VAL);
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
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                    Status;
  INT32                         Result;
  VOID                          *DtBlob;
  CONST CHAR8                   *FbCarveoutPath;
  INTN                          FbCarveoutNodeOffset;
  UINT64                        Data[4];
  EFI_PHYSICAL_ADDRESS          FbAddress, LutAddress;
  UINTN                         FbSize, LutSize;
  GOP_INSTANCE          *CONST  Gop = (GOP_INSTANCE *)Context;
  BOOLEAN                       FoundActiveHead;
  INTN                          Head;

  if ((Gop == NULL) || (Gop->Mode.Info == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid context\n", __FUNCTION__));
    return;
  }

  FbAddress = Gop->Mode.FrameBufferBase;
  FbSize    = Gop->Mode.FrameBufferSize;

  Status = GetLutRegion (Gop, &LutAddress, &LutSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting LUT region: %r\n", __FUNCTION__, Status));
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

  FoundActiveHead = FALSE;
  for (Head = 0; Head < ARRAY_SIZE (mFbCarveoutPaths); Head++) {
    FbCarveoutPath       = mFbCarveoutPaths[Head];
    FbCarveoutNodeOffset = fdt_path_offset (DtBlob, FbCarveoutPath);
    if (FbCarveoutNodeOffset < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Error getting %a DT node offset: %a\n",
        __FUNCTION__,
        FbCarveoutPath,
        fdt_strerror (FbCarveoutNodeOffset)
        ));
      continue;
    }

    if (!FoundActiveHead && IsHeadActive (Gop, Head)) {
      DEBUG ((DEBUG_ERROR, "%a: head %d is active\n", __FUNCTION__, Head));
      /* Active head: use FB and LUT settings from CBoot */
      FoundActiveHead = TRUE;

      Data[0] = SwapBytes64 (FbAddress);
      Data[1] = SwapBytes64 (FbSize);
      Data[2] = SwapBytes64 (LutAddress);
      Data[3] = SwapBytes64 (LutSize);
    } else {
      /* Inactive head: zero-fill everything */
      DEBUG ((DEBUG_ERROR, "%a: head %d is NOT active\n", __FUNCTION__, Head));
      ZeroMem (Data, sizeof (Data));
    }

    /* update the DT blob with the new FB and LUT details */
    Result = fdt_setprop_inplace (DtBlob, FbCarveoutNodeOffset, "reg", Data, sizeof (Data));
    if (Result != 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Error updating %a DT node\n",
        __FUNCTION__,
        FbCarveoutPath
        ));
      continue;
    }

    DEBUG ((
      DEBUG_ERROR,
      "%a: Updated %a reg: FbAddress  = 0x%016lx FbSize  = 0x%lx (%lu)\n"
      "%a: Updated %a reg: LutAddress = 0x%016lx LutSize = 0x%lx (%lu)\n",
      __FUNCTION__,
      FbCarveoutPath,
      Data[0],
      Data[1],
      Data[1],
      __FUNCTION__,
      FbCarveoutPath,
      Data[2],
      Data[3],
      Data[3]
      ));
  }
}

/**
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

**/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  CONST NVIDIA_DEVICE_DISCOVERY_PHASES    Phase,
  IN  CONST EFI_HANDLE                        DriverHandle,
  IN  CONST EFI_HANDLE                        ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                    Status;
  EFI_PHYSICAL_ADDRESS          BaseAddress;
  UINTN                         RegionSize;
  GOP_INSTANCE                  *Private;
  UINT32                        ScreenInputSize;
  UINTN                         ConfigureSize;
  UINT32                        ColorDepth;
  UINTN                         Pitch;
  UINT32                        LowAddress;
  UINT32                        HighAddress;
  EFI_PHYSICAL_ADDRESS          OldAddress;
  SCMI_CLOCK2_PROTOCOL          *ScmiClockProtocol;
  NVIDIA_CLOCK_NODE_PROTOCOL    *ClockNodeProtocol;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *TempGopProtocol;
  BOOLEAN                       ClockEnabled;
  CHAR8                         ClockName[SCMI_MAX_STR_LEN];
  INTN                          HeadIndex;
  INTN                          WindowIndex;
  UINT32                        WindowOffset;

  Private = NULL;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:

      Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&TempGopProtocol);
      if (Status == EFI_SUCCESS) {
        DEBUG ((DEBUG_INFO, "%a: GOP already installed, only a single GOP instance supported\n", __FUNCTION__));
        return EFI_UNSUPPORTED;
      }

      BaseAddress = 0;
      Status      = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        break;
      }

      Status = GetDispHeadFromAddr (BaseAddress, &HeadIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: error getting Head index\n", __FUNCTION__));
        break;
      }

      Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid, NULL, (VOID **)&ScmiClockProtocol);
      if (EFI_ERROR (Status)) {
        break;
      }

      Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockNodeProtocol);
      if (EFI_ERROR (Status)) {
        break;
      }

      /* If there are clocks listed make sure the primary one is enabled */
      if ((ClockNodeProtocol != NULL) && (ClockNodeProtocol->Clocks != 0) && (ScmiClockProtocol != NULL)) {
        Status = ScmiClockProtocol->GetClockAttributes (ScmiClockProtocol, ClockNodeProtocol->ClockEntries[0].ClockId, &ClockEnabled, ClockName);
        if (EFI_ERROR (Status)) {
          break;
        }

        if (!ClockEnabled) {
          DEBUG ((DEBUG_ERROR, "%a: Clock not enabled for Head index %d\n", __FUNCTION__, HeadIndex));
          return EFI_UNSUPPORTED;
        }
      }

      Status = GetFirstUsableWinForThisHead (BaseAddress, HeadIndex, &WindowIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Head index %d: no usable windows found\n", __FUNCTION__, HeadIndex));
        return EFI_UNSUPPORTED;
      }

      DEBUG ((DEBUG_INFO, "%a: Head index %d: Window index %d usable\n", __FUNCTION__, HeadIndex, WindowIndex));
      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      BaseAddress = 0;
      Status      = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        break;
      }

      Status = GetDispHeadFromAddr (BaseAddress, &HeadIndex);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: error getting Head index\n", __FUNCTION__));
        break;
      }

      Private = AllocateZeroPool (sizeof (GOP_INSTANCE));
      if (Private == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }

      Status = GetFirstUsableWinForThisHead (BaseAddress, HeadIndex, &WindowIndex);
      if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
      }

      WindowOffset = WIN_COLOR_DEPTH_OFFSET;
      Status       = ReadDcWinReg32 (BaseAddress, WindowIndex, WindowOffset, &ColorDepth);
      if (EFI_ERROR (Status)) {
        break;
      }

      if (ColorDepth == WIN_COLOR_DEPTH_R8G8B8A8) {
        Private->ModeInfo.PixelFormat = PixelRedGreenBlueReserved8BitPerColor;
      } else if (ColorDepth == WIN_COLOR_DEPTH_B8G8R8A8) {
        Private->ModeInfo.PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
      } else {
        Status = EFI_UNSUPPORTED;
        break;
      }

      WindowOffset = DC_A_WIN_AD_PCALC_WINDOW_SET_CROPPED_SIZE_IN_0;
      Status       = ReadDcWinReg32 (BaseAddress, WindowIndex, WindowOffset, &ScreenInputSize);
      if (EFI_ERROR (Status)) {
        break;
      }

      Private->ModeInfo.Version              = 0;
      Private->ModeInfo.HorizontalResolution = ScreenInputSize & MAX_UINT16;
      Private->ModeInfo.VerticalResolution   = ScreenInputSize >> 16;
      Private->ModeInfo.PixelFormat          = PixelRedGreenBlueReserved8BitPerColor;
      Private->ModeInfo.PixelsPerScanLine    = Private->ModeInfo.HorizontalResolution;
      DEBUG ((DEBUG_INFO, "%a: Modeinfo.HorizontalResolution %u\n", __FUNCTION__, Private->ModeInfo.HorizontalResolution));
      DEBUG ((DEBUG_INFO, "%a: Modeinfo.VerticalResolution   %u\n", __FUNCTION__, Private->ModeInfo.VerticalResolution));

      Pitch = Private->ModeInfo.HorizontalResolution * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
      if ((Pitch & -Pitch) != Pitch) {
        Pitch = (UINTN)GetPowerOfTwo32 ((UINT32)Pitch) << 1;
      }

      Private->Mode.FrameBufferSize = Private->ModeInfo.VerticalResolution * Pitch;

      Private->Mode.MaxMode    = 1;
      Private->Mode.Mode       = 0;
      Private->Mode.Info       = &Private->ModeInfo;
      Private->Mode.SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

      Status = DmaAllocateBuffer (
                 EfiRuntimeServicesData,
                 EFI_SIZE_TO_PAGES (Private->Mode.FrameBufferSize),
                 (VOID **)&Private->Mode.FrameBufferBase
                 );
      if (EFI_ERROR (Status)) {
        break;
      }

      WindowOffset = DC_A_WINBUF_AD_START_ADDR_OFFSET;
      Status       = ReadDcWinReg32 (BaseAddress, WindowIndex, WindowOffset, &LowAddress);
      if (EFI_ERROR (Status)) {
        break;
      }

      WindowOffset = DC_A_WINBUF_AD_START_ADDR_HI_OFFSET;
      Status       = ReadDcWinReg32 (BaseAddress, WindowIndex, WindowOffset, &HighAddress);
      if (EFI_ERROR (Status)) {
        break;
      }

      OldAddress = ((EFI_PHYSICAL_ADDRESS)HighAddress << 32) | (EFI_PHYSICAL_ADDRESS)LowAddress;
      if (OldAddress != 0) {
        CopyMem ((VOID *)Private->Mode.FrameBufferBase, (CONST VOID *)OldAddress, Private->Mode.FrameBufferSize);
      } else {
        ZeroMem ((VOID *)Private->Mode.FrameBufferBase, Private->Mode.FrameBufferSize);
      }

      ConfigureSize = 0;
      Status        = FrameBufferBltConfigure (
                        (VOID *)Private->Mode.FrameBufferBase,
                        &Private->ModeInfo,
                        NULL,
                        &ConfigureSize
                        );
      if (Status != EFI_BUFFER_TOO_SMALL) {
        if (!EFI_ERROR (Status)) {
          Status = EFI_DEVICE_ERROR;
        }

        break;
      }

      Private->Configure = (FRAME_BUFFER_CONFIGURE *)AllocatePool (ConfigureSize);
      if (Private->Configure == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }

      Status = FrameBufferBltConfigure (
                 (VOID *)Private->Mode.FrameBufferBase,
                 &Private->ModeInfo,
                 Private->Configure,
                 &ConfigureSize
                 );
      if (EFI_ERROR (Status)) {
        break;
      }

      DEBUG ((
        DEBUG_ERROR,
        "%a: Window %d .FrameBufferBase=0x%p\n",
        __FUNCTION__,
        WindowIndex,
        Private->Mode.FrameBufferBase
        ));
      WindowOffset = DC_A_WINBUF_AD_START_ADDR_OFFSET;
      Status       = WriteDcWinReg32 (BaseAddress, WindowIndex, WindowOffset, Private->Mode.FrameBufferBase & MAX_UINT32);
      if (EFI_ERROR (Status)) {
        break;
      }

      WindowOffset = DC_A_WINBUF_AD_START_ADDR_HI_OFFSET;
      Status       = WriteDcWinReg32 (BaseAddress, WindowIndex, WindowOffset, Private->Mode.FrameBufferBase >> 32);
      if (EFI_ERROR (Status)) {
        break;
      }

      Private->Signature     = GOP_INSTANCE_SIGNATURE;
      Private->Handle        = ControllerHandle;
      Private->Gop.QueryMode = GraphicsQueryMode;
      Private->Gop.SetMode   = GraphicsSetMode;
      Private->Gop.Blt       = GraphicsBlt;
      Private->Gop.Mode      = &Private->Mode;
      Private->DcAddr        = BaseAddress;

      // Install the Graphics Output Protocol and the Device Path
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Private->Handle,
                      &gEfiGraphicsOutputProtocolGuid,
                      &Private->Gop,
                      NULL
                      );

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error installing GOP protocol; skipping DT callback event\n", __FUNCTION__));
      } else {
        Status = gBS->CreateEventEx (
                        EVT_NOTIFY_SIGNAL,              // Type
                        TPL_CALLBACK,                   // NotifyTpl
                        FdtInstalled,                   // NotifyFunction
                        (void *)Private,                // NotifyContext
                        &gFdtTableGuid,                 // EventGroup
                        &FdtInstallEvent
                        );                              // Event
      }

      break;

    default:
      Status = EFI_SUCCESS;
      break;
  }

  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      if (Private->Mode.FrameBufferBase != 0) {
        DmaFreeBuffer (EFI_SIZE_TO_PAGES (Private->Mode.FrameBufferSize), (VOID *)Private->Mode.FrameBufferBase);
        Private->Mode.FrameBufferBase = 0;
        Private->Mode.FrameBufferSize = 0;
      }

      FreePool (Private);
    }
  }

  return Status;
}
