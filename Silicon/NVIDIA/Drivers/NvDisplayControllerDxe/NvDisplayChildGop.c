/** @file

  NV Display Controller Driver - Child GOP

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/DisplayDeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include "NvDisplay.h"

/**
  Checks if ChildHandle is a child handle of ControllerHandle (in
  terms of EFI Device Path).

  ControllerHandle is assumed to be managed by this driver (NOT an
  arbitrary driver).

  @param[in] DriverHandle      Handle of the driver.
  @param[in] ControllerHandle  Handle of the controller.
  @param[in] ChildHandle       Handle of the child.

  @retval TRUE   ChildHandle is a child handle of ControllerHandle.
  @retval FALSE  ChildHandle is not a child handle of ControllerHandle.
*/
STATIC
BOOLEAN
IsChildHandle (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST EFI_HANDLE  ChildHandle
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                ParentHandle;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  Status = gBS->OpenProtocol (
                  ChildHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath,
                  DriverHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_UNSUPPORTED) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to retrieve device path from handle %p: %r\r\n",
        __FUNCTION__,
        ChildHandle,
        Status
        ));
    }

    return FALSE;
  }

  /* Locate a handle with a gEdkiiNonDiscoverableDeviceProtocolGuid
     protocol instance, which this driver installs on its controller
     handles. */
  Status = gBS->LocateDevicePath (
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  &DevicePath,
                  &ParentHandle
                  );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to locate parent handle: %r\r\n",
        __FUNCTION__,
        Status
        ));
    }

    return FALSE;
  }

  return ParentHandle == ControllerHandle;
}

/**
  Checks if the given GOP instance has an active mode.

  @param[in] Gop  The GOP protocol instance to check.

  @return TRUE   A mode is active.
  @return FALSE  No mode is active.
*/
STATIC
BOOLEAN
IsGopModeActive (
  IN CONST EFI_GRAPHICS_OUTPUT_PROTOCOL *CONST  Gop
  )
{
  CONST EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *CONST  Mode = Gop->Mode;

  return (  (Mode != NULL)
         && (Mode->Mode < Mode->MaxMode)
         && (Mode->Info != NULL)
         && (Mode->SizeOfInfo >= sizeof (*Mode->Info)));
}

/**
  Locates a child handle with an active GOP instance installed.

  @param[in]  DriverHandle      Handle of the driver.
  @param[in]  ControllerHandle  Handle of the controller.
  @param[out] Protocol          The located active GOP instance.

  @retval EFI_SUCCESS    Child handle found successfully.
  @retval !=EFI_SUCCESS  Error occurred.
*/
EFI_STATUS
NvDisplayLocateActiveChildGop (
  IN  CONST EFI_HANDLE                      DriverHandle,
  IN  CONST EFI_HANDLE                      ControllerHandle,
  OUT EFI_GRAPHICS_OUTPUT_PROTOCOL **CONST  Protocol  OPTIONAL
  )
{
  EFI_STATUS                    Status;
  UINTN                         Index, Count;
  EFI_HANDLE                    GopHandle, *Handles = NULL;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  &Count,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to enumerate graphics output device handles: %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  for (Index = 0; Index < Count; ++Index) {
    GopHandle = Handles[Index];

    if (IsChildHandle (DriverHandle, ControllerHandle, GopHandle)) {
      Status = gBS->OpenProtocol (
                      GopHandle,
                      &gEfiGraphicsOutputProtocolGuid,
                      (VOID **)&Gop,
                      DriverHandle,
                      ControllerHandle,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to retrieve graphics output protocol from handle %p: %r\r\n",
          __FUNCTION__,
          GopHandle,
          Status
          ));
        goto Exit;
      }

      if (IsGopModeActive (Gop)) {
        Status = EFI_SUCCESS;

        if (Protocol != NULL) {
          *Protocol = Gop;
        }

        goto Exit;
      }
    }
  }

  Status = EFI_NOT_FOUND;

Exit:
  if (Handles != NULL) {
    FreePool (Handles);
  }

  return Status;
}

/**
  Update the Device Tree with mode and framebuffer info using an
  active GOP instance installed on a child handle.

  @param[in] DriverHandle      Handle of the driver.
  @param[in] ControllerHandle  Handle of the controller.

  @return TRUE   Device Tree updated successfully.
  @return FALSE  No Device Tree was found.
  @return FALSE  No GOP child handle was found.
  @return FALSE  The GOP child handle was inactive.
  @return FALSE  Could not retrieve the framebuffer region.
  @return FALSE  Failed to update the Device Tree.
*/
BOOLEAN
NvDisplayUpdateFdtTableActiveChildGop (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle
  )
{
  EFI_STATUS                    Status;
  VOID                          *Fdt;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;
  EFI_PHYSICAL_ADDRESS          FrameBufferBase;
  UINT64                        FrameBufferSize;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Fdt);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Status = NvDisplayLocateActiveChildGop (DriverHandle, ControllerHandle, &Gop);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  FrameBufferBase = Gop->Mode->FrameBufferBase;
  FrameBufferSize = Gop->Mode->FrameBufferSize;

  if (  (Gop->Mode->Info->PixelFormat == PixelBltOnly)
     || (FrameBufferBase == 0) || (FrameBufferSize == 0))
  {
    Status = NvDisplayGetFramebufferRegion (&FrameBufferBase, &FrameBufferSize);
    if (EFI_ERROR (Status)) {
      return FALSE;
    }
  }

  return UpdateDeviceTreeSimpleFramebufferInfo (
           Fdt,
           Gop->Mode->Info,
           (UINT64)FrameBufferBase,
           (UINT64)FrameBufferSize
           );
}
