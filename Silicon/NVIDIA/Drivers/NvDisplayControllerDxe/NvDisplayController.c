/** @file

  NV Display Controller Driver - Common

  SPDX-FileCopyrightText: Copyright (c) 2024-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/NonDiscoverableDevice.h>

#include <NVIDIAConfiguration.h>

#include "NvDisplay.h"
#include "NvDisplayController.h"

#define NV_DISPLAY_CONTROLLER_SIGNATURE  SIGNATURE_32('N','V','D','C')

typedef struct {
  UINT32                             Signature;
  EFI_HANDLE                         DriverHandle;
  EFI_HANDLE                         ControllerHandle;
  NV_DISPLAY_CONTROLLER_HW_ENABLE    HwEnable;
  UINT8                              HandoffMode;
  UINT8                              HandoffMethod;
  NON_DISCOVERABLE_DEVICE            Device;
  BOOLEAN                            HwEnabled;
  BOOLEAN                            PerformHandoff;
  EFI_EVENT                          OnFdtInstalledEvent;
  EFI_EVENT                          OnReadyToBootEvent;
} NV_DISPLAY_CONTROLLER_PRIVATE;

#define NV_DISPLAY_CONTROLLER_PRIVATE_FROM_DEVICE(a)  CR(\
    a,                                                   \
    NV_DISPLAY_CONTROLLER_PRIVATE,                       \
    Device,                                              \
    NV_DISPLAY_CONTROLLER_SIGNATURE                      \
    )

/**
  Retrieve controller private data from the given controller handle.

  @param[out] Private           Pointer to the private data.
  @param[in]  DriverHandle      Handle of the driver.
  @param[in]  ControllerHandle  Handle of the controller.

  @retval EFI_SUCCESS    Pointer to private data successfully retrieved.
  @retval !=EFI_SUCCESS  Error(s) occurred.
*/
STATIC
EFI_STATUS
GetControllerPrivate (
  OUT NV_DISPLAY_CONTROLLER_PRIVATE **CONST  Private  OPTIONAL,
  IN  CONST EFI_HANDLE                       DriverHandle,
  IN  CONST EFI_HANDLE                       ControllerHandle
  )
{
  EFI_STATUS               Status;
  NON_DISCOVERABLE_DEVICE  *Device;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Device,
                  DriverHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_UNSUPPORTED) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to open EDK2 non-discoverable device protocol: %r\r\n",
        __FUNCTION__,
        Status
        ));
    }

    return Status;
  }

  if (Private != NULL) {
    *Private = NV_DISPLAY_CONTROLLER_PRIVATE_FROM_DEVICE (Device);
  }

  return EFI_SUCCESS;
}

/**
  Check if ACPI mode is enabled.

  @return TRUE   Force BLT-only mode.
  @return FALSE  Do not force BLT-only mode.
*/
STATIC
BOOLEAN
CheckAcpiMode (
  )
{
  EFI_STATUS  Status;
  UINTN       Data, DataSize;

  DataSize = sizeof (Data);
  Status   = gRT->GetVariable (L"DtAcpiPref", &gDtPlatformFormSetGuid, NULL, &DataSize, &Data);
  if (!EFI_ERROR (Status)) {
    return Data != 0;
  }

  DEBUG ((DEBUG_WARN, "%a: failed to retrieve DT/ACPI preference variable: %r\r\n", __FUNCTION__, Status));
  return !PcdGetBool (PcdDefaultDtPref);
}

/**
  Check if we should force BLT-only mode or not.

  @param[in] Private  Display controller private data.

  @return TRUE   Force BLT-only mode.
  @return FALSE  Do not force BLT-only mode.
*/
STATIC
BOOLEAN
CheckForceBltOnly (
  IN CONST NV_DISPLAY_CONTROLLER_PRIVATE *CONST  Private
  )
{
  switch (Private->HandoffMode) {
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_NEVER:
    default:
      return TRUE;
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_ALWAYS:
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_AUTO:
      return Private->HandoffMethod != NVIDIA_SOC_DISPLAY_HANDOFF_METHOD_EFIFB;
  }
}

/**
  Check if we should perform display hand-off or not.

  @param[in] Private  Display controller private data.

  @return TRUE   Leave the display running on UEFI exit.
  @return FALSE  Reset the display on UEFI exit.
*/
STATIC
BOOLEAN
CheckPerformHandoff (
  IN CONST NV_DISPLAY_CONTROLLER_PRIVATE *CONST  Private
  )
{
  switch (Private->HandoffMode) {
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_NEVER:
    default:
      return FALSE;
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_ALWAYS:
      return TRUE;
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_AUTO:
      return Private->PerformHandoff;
  }
}

/**
  Event notification function for whenever the FDT table is updated.

  @param[in] Event    Event used for the notification.
  @param[in] Context  Context for the notification.
*/
STATIC
VOID
EFIAPI
FdtTableNotifyFunction (
  IN CONST EFI_EVENT  Event,
  IN VOID *CONST      Context
  )
{
  NV_DISPLAY_CONTROLLER_PRIVATE *CONST  Private =
    (NV_DISPLAY_CONTROLLER_PRIVATE *)Context;

  ASSERT (Private->HandoffMode != NVIDIA_SOC_DISPLAY_HANDOFF_MODE_NEVER);
  ASSERT (Private->HandoffMethod == NVIDIA_SOC_DISPLAY_HANDOFF_METHOD_SIMPLEFB);

  /* Since the FDT was just reinstalled, we must always run the update
     routine. */
  Private->PerformHandoff = NvDisplayUpdateFdtTableActiveChildGop (
                              Private->DriverHandle,
                              Private->ControllerHandle
                              );
}

/**
  Event notification function for ReadyToBoot event.

  @param[in] Event    Event used for the notification.
  @param[in] Context  Context for the notification.
*/
STATIC
VOID
EFIAPI
ReadyToBootNotifyFunction (
  IN CONST EFI_EVENT  Event,
  IN VOID *CONST      Context
  )
{
  NV_DISPLAY_CONTROLLER_PRIVATE *CONST  Private =
    (NV_DISPLAY_CONTROLLER_PRIVATE *)Context;

  ASSERT (Private->HandoffMode != NVIDIA_SOC_DISPLAY_HANDOFF_MODE_NEVER);

  switch (Private->HandoffMethod) {
    case NVIDIA_SOC_DISPLAY_HANDOFF_METHOD_SIMPLEFB:
      /* Only run the FDT update routine if the FDT has not been
         updated yet. */
      if (!Private->PerformHandoff) {
        Private->PerformHandoff = NvDisplayUpdateFdtTableActiveChildGop (
                                    Private->DriverHandle,
                                    Private->ControllerHandle
                                    );
      }

      break;

    case NVIDIA_SOC_DISPLAY_HANDOFF_METHOD_EFIFB:
      Private->PerformHandoff = NvDisplayEnableEfifbActiveChildGop (
                                  Private->DriverHandle,
                                  Private->ControllerHandle
                                  );
      break;
  }
}

/**
  Destroys controller private data during ExitBootServices.

  @param[in] Private  Display controller private data.

  @retval EFI_SUCCESS            Private data successfully destroyed.
  @retval EFI_INVALID_PARAMETER  Private is NULL.
  @retval !=EFI_SUCCESS          Error(s) occurred.
*/
STATIC
EFI_STATUS
DestroyControllerPrivateOnExitBootServices (
  IN NV_DISPLAY_CONTROLLER_PRIVATE *CONST  Private
  )
{
  EFI_STATUS  Status, Status1;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;

  if (Private->OnFdtInstalledEvent != NULL) {
    Status1 = gBS->CloseEvent (Private->OnFdtInstalledEvent);
    if (EFI_ERROR (Status1)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to close OnFdtInstalled event: %r\r\n",
        __FUNCTION__,
        Status1
        ));
    }

    if (!EFI_ERROR (Status)) {
      Status = Status1;
    }

    Private->OnFdtInstalledEvent = NULL;
  }

  if (Private->OnReadyToBootEvent != NULL) {
    Status1 = gBS->CloseEvent (Private->OnReadyToBootEvent);
    if (EFI_ERROR (Status1)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to close OnReadyToBoot event: %r\r\n",
        __FUNCTION__,
        Status1
        ));
    }

    if (!EFI_ERROR (Status)) {
      Status = Status1;
    }

    Private->OnReadyToBootEvent = NULL;
  }

  if (Private->HwEnabled) {
    Status1 = Private->HwEnable (Private->DriverHandle, Private->ControllerHandle, FALSE);
    if (!EFI_ERROR (Status)) {
      Status = Status1;
    }

    Private->HwEnabled = FALSE;
  }

  return Status;
}

/**
  Destroys controller private data.

  Cannot be called during ExitBootServices since it also frees the
  display private data.

  @param[in] Private  Display controller private data.

  @retval EFI_SUCCESS            Private data successfully destroyed.
  @retval EFI_INVALID_PARAMETER  Private is NULL.
  @retval !=EFI_SUCCESS          Error(s) occurred.
*/
STATIC
EFI_STATUS
DestroyControllerPrivate (
  IN NV_DISPLAY_CONTROLLER_PRIVATE *CONST  Private
  )
{
  EFI_STATUS  Status;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DestroyControllerPrivateOnExitBootServices (Private);
  FreePool (Private);

  return Status;
}

/**
  Creates controller private data.

  @param[out] Private           Display controller private data.
  @param[in]  DriverHandle      Handle of the driver.
  @param[in]  ControllerHandle  Handle of the controller.
  @param[in]  HwEnable          Chip-specific display HW control function.

  @retval EFI_SUCCESS            Operation successful.
  @retval EFI_INVALID_PARAMETER  Private is NULL.
  @retval others                 Error(s) occurred.
*/
STATIC
EFI_STATUS
CreateControllerPrivate (
  OUT NV_DISPLAY_CONTROLLER_PRIVATE **CONST  Private,
  IN  CONST EFI_HANDLE                       DriverHandle,
  IN  CONST EFI_HANDLE                       ControllerHandle,
  IN  CONST NV_DISPLAY_CONTROLLER_HW_ENABLE  HwEnable
  )
{
  EFI_STATUS                     Status;
  UINTN                          ResourcesSize;
  BOOLEAN                        IsAcpiMode;
  NON_DISCOVERABLE_DEVICE        *Device;
  NV_DISPLAY_CONTROLLER_PRIVATE  *Result = NULL;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Device,
                  DriverHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to open NVIDIA non-discoverable device protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  Status = NvDisplayGetMmioRegions (DriverHandle, ControllerHandle, NULL, &ResourcesSize);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result = (NV_DISPLAY_CONTROLLER_PRIVATE *)AllocateZeroPool (sizeof (*Result) + ResourcesSize);
  if (Result == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not allocate %u bytes for display controller private data\r\n",
      __FUNCTION__,
      sizeof (*Result) + ResourcesSize
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  IsAcpiMode = CheckAcpiMode ();

  Result->Signature        = NV_DISPLAY_CONTROLLER_SIGNATURE;
  Result->DriverHandle     = DriverHandle;
  Result->ControllerHandle = ControllerHandle;
  Result->HwEnable         = HwEnable;
  Result->HandoffMode      = IsAcpiMode ? NVIDIA_SOC_DISPLAY_HANDOFF_MODE_ALWAYS : PcdGet8 (PcdSocDisplayHandoffMode);
  Result->HandoffMethod    = IsAcpiMode ? NVIDIA_SOC_DISPLAY_HANDOFF_METHOD_EFIFB : PcdGet8 (PcdSocDisplayHandoffMethod);

  CopyMem (&Result->Device, Device, sizeof (*Device));

  Result->Device.Resources =
    (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(Result + 1);

  Status = NvDisplayGetMmioRegions (
             DriverHandle,
             ControllerHandle,
             Result->Device.Resources,
             &ResourcesSize
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = Result->HwEnable (DriverHandle, ControllerHandle, TRUE);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result->HwEnabled = TRUE;

  switch (Result->HandoffMode) {
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_NEVER:
    default:
      break;

    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_ALWAYS:
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_AUTO:
      if (Result->HandoffMethod == NVIDIA_SOC_DISPLAY_HANDOFF_METHOD_SIMPLEFB) {
        Status = gBS->CreateEventEx (
                        EVT_NOTIFY_SIGNAL,
                        TPL_CALLBACK,
                        FdtTableNotifyFunction,
                        Result,
                        &gFdtTableGuid,
                        &Result->OnFdtInstalledEvent
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: failed to create OnFdtInstalled event: %r\r\n",
            __FUNCTION__,
            Status
            ));
          Result->OnFdtInstalledEvent = NULL;
          goto Exit;
        }
      }

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      ReadyToBootNotifyFunction,
                      Result,
                      &gEfiEventReadyToBootGuid,
                      &Result->OnReadyToBootEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to create OnReadyToBoot event: %r\r\n",
          __FUNCTION__,
          Status
          ));
        Result->OnReadyToBootEvent = NULL;
        goto Exit;
      }

      break;
  }

  *Private = Result;
  Result   = NULL;

Exit:
  if (Result != NULL) {
    DestroyControllerPrivate (Result);
  }

  return Status;
}

/**
  Starts the NV display controller driver on the given controller
  handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.
  @param[in] HwEnable          Chip-specific display HW control function.

  @retval EFI_SUCCESS          Operation successful.
  @retval EFI_ALREADY_STARTED  Driver has already been started on the given handle.
  @retval !=EFI_SUCCESS        Operation failed.
*/
EFI_STATUS
NvDisplayControllerStart (
  IN CONST EFI_HANDLE                       DriverHandle,
  IN CONST EFI_HANDLE                       ControllerHandle,
  IN CONST NV_DISPLAY_CONTROLLER_HW_ENABLE  HwEnable
  )
{
  EFI_STATUS                     Status;
  NV_DISPLAY_CONTROLLER_PRIVATE  *Private;
  CONST EFI_GUID                 *Protocols[2]  = { NULL, NULL };
  VOID                           *Interfaces[2] = { NULL, NULL };

  Status = GetControllerPrivate (NULL, DriverHandle, ControllerHandle);
  if (!EFI_ERROR (Status)) {
    return EFI_ALREADY_STARTED;
  } else if (Status != EFI_UNSUPPORTED) {
    return Status;
  }

  Status = CreateControllerPrivate (&Private, DriverHandle, ControllerHandle, HwEnable);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Protocols[0]  = &gEdkiiNonDiscoverableDeviceProtocolGuid;
  Interfaces[0] = &Private->Device;

  if (CheckForceBltOnly (Private)) {
    Protocols[1]  = &gNVIDIAGraphicsOutputForceBltOnlyProtocolGuid;
    Interfaces[1] = NULL;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  (EFI_HANDLE *)&ControllerHandle,
                  Protocols[0],
                  Interfaces[0],
                  Protocols[1],
                  Interfaces[1],
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to install EDK2 non-discoverable device protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    DestroyControllerPrivate (Private);
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Stops the NV display controller driver on the given controller
  handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
EFI_STATUS
NvDisplayControllerStop (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle
  )
{
  EFI_STATUS                     Status;
  NV_DISPLAY_CONTROLLER_PRIVATE  *Private;
  CONST EFI_GUID                 *Protocols[2]  = { NULL, NULL };
  VOID                           *Interfaces[2] = { NULL, NULL };

  Status = GetControllerPrivate (&Private, DriverHandle, ControllerHandle);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_UNSUPPORTED) {
      Status = EFI_SUCCESS;
    }

    return Status;
  }

  Protocols[0]  = &gEdkiiNonDiscoverableDeviceProtocolGuid;
  Interfaces[0] = &Private->Device;

  if (CheckForceBltOnly (Private)) {
    Protocols[1]  = &gNVIDIAGraphicsOutputForceBltOnlyProtocolGuid;
    Interfaces[1] = NULL;
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  ControllerHandle,
                  Protocols[0],
                  Interfaces[0],
                  Protocols[1],
                  Interfaces[1],
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to uninstall EDK2 non-discoverable device protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  return DestroyControllerPrivate (Private);
}

/**
  Handles the ExitBootServices event within the NV display controller
  driver started on the given controller handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.

  @retval EFI_SUCCESS  Display shut down.
  @retval EFI_ABORTED  Performing hand-off, display left running.
  @retval others       Operation failed.
*/
EFI_STATUS
NvDisplayControllerOnExitBootServices (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle
  )
{
  EFI_STATUS                     Status;
  NV_DISPLAY_CONTROLLER_PRIVATE  *Private;

  Status = GetControllerPrivate (&Private, DriverHandle, ControllerHandle);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_UNSUPPORTED) {
      Status = EFI_SUCCESS;
    }

    return Status;
  }

  if (CheckPerformHandoff (Private)) {
    /* We should perform hand-off, leave the display running. */
    return EFI_ABORTED;
  }

  /* No hand-off, reset the display to a known good state. */
  return DestroyControllerPrivateOnExitBootServices (Private);
}
