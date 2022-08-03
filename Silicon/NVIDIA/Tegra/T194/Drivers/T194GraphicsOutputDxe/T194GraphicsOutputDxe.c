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
   @param [in]  Head           head index to check if active

  @retval TRUE  head is active
  @retval FALSE head is inactive
 ***************************************/
STATIC
BOOLEAN
IsHeadActive (
  IN CONST  GOP_INSTANCE  *Gop,
  IN CONST  INTN          Head
  )
{
  UINT32  DispActive;

  DispActive = MmioRead32 (Gop->DcAddr + DC_HEAD_OFFSET*Head + DC_DISP_DISP_ACTIVE_OFFSET);
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
      /* Active head: use FB and LUT settings from CBoot */
      FoundActiveHead = TRUE;

      Data[0] = SwapBytes64 (FbAddress);
      Data[1] = SwapBytes64 (FbSize);
      Data[2] = SwapBytes64 (LutAddress);
      Data[3] = SwapBytes64 (LutSize);
    } else {
      /* Inactive head: zero-fill everything */
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                  Status;
  EFI_PHYSICAL_ADDRESS        BaseAddress;
  UINTN                       RegionSize;
  GOP_INSTANCE                *Private;
  UINT32                      ScreenInputSize;
  UINTN                       ConfigureSize;
  UINT32                      ColorDepth;
  UINTN                       Pitch;
  EFI_PHYSICAL_ADDRESS        LowAddress;
  EFI_PHYSICAL_ADDRESS        HighAddress;
  EFI_PHYSICAL_ADDRESS        OldAddress;
  SCMI_CLOCK2_PROTOCOL        *ScmiClockProtocol;
  NVIDIA_CLOCK_NODE_PROTOCOL  *ClockNodeProtocol;
  BOOLEAN                     ClockEnabled;
  BOOLEAN                     WinEnabled;
  UINT32                      WinOptions;
  CHAR8                       ClockName[SCMI_MAX_STR_LEN];

  Private = NULL;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      BaseAddress = 0;
      Status      = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
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

      // If there are clocks listed make sure the primary one is enabled
      if ((ClockNodeProtocol != NULL) && (ClockNodeProtocol->Clocks != 0) && (ScmiClockProtocol != NULL)) {
        Status = ScmiClockProtocol->GetClockAttributes (ScmiClockProtocol, ClockNodeProtocol->ClockEntries[0].ClockId, &ClockEnabled, ClockName);
        if (EFI_ERROR (Status)) {
          break;
        }

        if (!ClockEnabled) {
          return EFI_UNSUPPORTED;
        }
      }

      ScreenInputSize = MmioRead32 (BaseAddress + PCALC_WINDOW_SET_CROPPED_SIZE_IN_OFFSET);
      if (((ScreenInputSize >> 16) == 0) || ((ScreenInputSize & MAX_UINT16) == 0)) {
        return EFI_UNSUPPORTED;
      }

      WinOptions = MmioRead32 (BaseAddress + DC_A_WIN_AD_WIN_OPTIONS_OFFSET);
      WinEnabled = (WinOptions & BIT30) == BIT30;
      if (WinEnabled == FALSE) {
        DEBUG ((DEBUG_ERROR, "%a: WindowA disabled on this head\n", __FUNCTION__));
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      BaseAddress = 0;
      Status      = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        break;
      }

      Private = AllocateZeroPool (sizeof (GOP_INSTANCE));
      if (Private == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }

      ColorDepth = MmioRead32 (BaseAddress + WIN_COLOR_DEPTH_OFFSET);
      if (ColorDepth == WIN_COLOR_DEPTH_R8G8B8A8) {
        Private->ModeInfo.PixelFormat = PixelRedGreenBlueReserved8BitPerColor;
      } else if (ColorDepth == WIN_COLOR_DEPTH_B8G8R8A8) {
        Private->ModeInfo.PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
      } else {
        Status = EFI_UNSUPPORTED;
        break;
      }

      ScreenInputSize                        = MmioRead32 (BaseAddress + PCALC_WINDOW_SET_CROPPED_SIZE_IN_OFFSET);
      Private->ModeInfo.Version              = 0;
      Private->ModeInfo.HorizontalResolution = ScreenInputSize & MAX_UINT16;
      Private->ModeInfo.VerticalResolution   = ScreenInputSize >> 16;
      Private->ModeInfo.PixelFormat          = PixelRedGreenBlueReserved8BitPerColor;
      Private->ModeInfo.PixelsPerScanLine    = Private->ModeInfo.HorizontalResolution;

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

      LowAddress  = MmioRead32 (BaseAddress + WINBUF_START_ADDR_LO_OFFSET);
      HighAddress = MmioRead32 (BaseAddress + WINBUF_START_ADDR_HI_OFFSET);
      OldAddress  = (HighAddress << 32) | LowAddress;

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

      MmioWrite32 (BaseAddress + WINBUF_START_ADDR_LO_OFFSET, Private->Mode.FrameBufferBase & MAX_UINT32);
      MmioWrite32 (BaseAddress + WINBUF_START_ADDR_HI_OFFSET, Private->Mode.FrameBufferBase >> 32);

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
      return EFI_SUCCESS;
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
