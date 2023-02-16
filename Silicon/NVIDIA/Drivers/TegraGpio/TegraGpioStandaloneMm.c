/** @file

  Standalone MM Tegra GPIO Driver

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Protocol/EmbeddedGpio.h>

#include "TegraGpioPrivate.h"

typedef struct {
  CONST CHAR8              *Name;
  CONST GPIO_CONTROLLER    *Controllers;
  UINTN                    NumControllers;
} TEGRA_GPIO_MAP;

STATIC CONST GPIO_CONTROLLER  Th500GpioControllers[] = {
  TEGRA_GPIO_ENTRY (0,  0, 0, 8),
  TEGRA_GPIO_ENTRY (1,  0, 1, 8),
  TEGRA_GPIO_ENTRY (2,  0, 2, 2),
  TEGRA_GPIO_ENTRY (3,  0, 3, 6),
  TEGRA_GPIO_ENTRY (4,  0, 4, 8),
  TEGRA_GPIO_ENTRY (5,  1, 0, 8),
  TEGRA_GPIO_ENTRY (6,  1, 1, 8),
  TEGRA_GPIO_ENTRY (7,  1, 2, 8),
  TEGRA_GPIO_ENTRY (8,  1, 3, 8),
  TEGRA_GPIO_ENTRY (9,  1, 4, 4),
  TEGRA_GPIO_ENTRY (10, 1, 5, 6)
};

STATIC NVIDIA_GPIO_CONTROLLER_ENTRY  *mControllerArray = NULL;
STATIC UINT32                        mControllerCount  = 0;
STATIC PLATFORM_GPIO_CONTROLLER      *mGpioController  = NULL;

STATIC CONST TEGRA_GPIO_MAP  mTegraGpioMap[] = {
  { "th500-gpio", Th500GpioControllers, ARRAY_SIZE (Th500GpioControllers) },
};

STATIC
EFI_STATUS
GetGpioAddress (
  IN  EMBEDDED_GPIO_PIN  Gpio,
  OUT UINTN              *GpioAddress
  )
{
  UINTN  Index;

  if (GpioAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < mGpioController->GpioControllerCount; Index++) {
    UINTN  RegisterOffset;
    if ((Gpio < mGpioController->GpioController[Index].GpioIndex) ||
        (Gpio >= (mGpioController->GpioController[Index].GpioIndex + mGpioController->GpioController[Index].InternalGpioCount)))
    {
      continue;
    }

    if (mGpioController->GpioController[Index].RegisterBase == 0) {
      *GpioAddress = 0;
    } else {
      RegisterOffset = (Gpio - mGpioController->GpioController[Index].GpioIndex) * GPIO_REGISTER_SPACING;
      *GpioAddress   = mGpioController->GpioController[Index].RegisterBase + RegisterOffset;
    }

    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

/**
 * Gets the state of a GPIO pin
 *
 * @param[in]  This       pointer to protocol
 * @param[in]  Gpio       which pin to read
 * @param[out] Value      state of the pin
 *
 * @return EFI_SUCCESS - GPIO state returned in Value
 */
EFI_STATUS
GetGpioState (
  IN  EMBEDDED_GPIO      *This,
  IN  EMBEDDED_GPIO_PIN  Gpio,
  OUT UINTN              *Value
  )
{
  UINT32      Mode;
  UINT32      State;
  UINTN       Address;
  EFI_STATUS  Status;

  if ((NULL == This) || (NULL == Value)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetGpioAddress (Gpio, &Address);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Address == 0) {
    return EFI_NOT_FOUND;
  }

  Mode = MmioRead32 (Address + GPIO_ENABLE_CONFIG_OFFSET);
  if ((Mode & GPIO_OUTPUT_BIT_VALUE) == 0) {
    State = MmioRead32 (Address + GPIO_INPUT_VALUE_OFFSET);
  } else {
    State = MmioRead32 (Address + GPIO_OUTPUT_VALUE_OFFSET);
  }

  *Value = State;
  return EFI_SUCCESS;
}

/**
 * Sets the state of a GPIO pin
 *
 * @param[in]  This       pointer to protocol
 * @param[in]  Gpio       which pin to modify
 * @param[in]  Mode       mode to set
 *
 * @return EFI_SUCCESS - GPIO set as requested
 */
EFI_STATUS
SetGpioState (
  IN EMBEDDED_GPIO       *This,
  IN EMBEDDED_GPIO_PIN   Gpio,
  IN EMBEDDED_GPIO_MODE  Mode
  )
{
  UINTN       Address;
  EFI_STATUS  Status;
  UINT32      State = 0;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetGpioAddress (Gpio, &Address);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Address == 0) {
    return EFI_NOT_FOUND;
  }

  switch (Mode) {
    case GPIO_MODE_INPUT:
      MmioBitFieldWrite32 (
        Address + GPIO_ENABLE_CONFIG_OFFSET,
        GPIO_ENABLE_BIT,
        GPIO_OUTPUT_BIT,
        GPIO_ENABLE_BIT_VALUE
        );
      return EFI_SUCCESS;

    case GPIO_MODE_OUTPUT_1:
      State = 1;

    case GPIO_MODE_OUTPUT_0:
      MmioWrite32 (Address + GPIO_OUTPUT_VALUE_OFFSET, State);
      MmioWrite32 (Address + GPIO_OUTPUT_CONTROL_OFFET, 0);
      MmioBitFieldWrite32 (
        Address + GPIO_ENABLE_CONFIG_OFFSET,
        GPIO_ENABLE_BIT,
        GPIO_OUTPUT_BIT,
        GPIO_ENABLE_BIT_VALUE|GPIO_OUTPUT_BIT_VALUE
        );

      return EFI_SUCCESS;

    default:
      return EFI_UNSUPPORTED;
  }
}

/**
 * Gets the mode (function) of a GPIO pin
 *
 * @param[in]  This       pointer to protocol
 * @param[in]  Gpio       which pin
 * @param[out] Mode       pointer to output mode value
 *
 * @return EFI_SUCCESS - mode value retrieved
 */
EFI_STATUS
GetGpioMode (
  IN  EMBEDDED_GPIO       *This,
  IN  EMBEDDED_GPIO_PIN   Gpio,
  OUT EMBEDDED_GPIO_MODE  *Mode
  )
{
  UINT32      EnableConfig;
  UINT32      State;
  UINTN       Address;
  EFI_STATUS  Status;

  if ((NULL == This) || (NULL == Mode)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetGpioAddress (Gpio, &Address);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Address == 0) {
    return EFI_NOT_FOUND;
  }

  EnableConfig = MmioRead32 (Address + GPIO_ENABLE_CONFIG_OFFSET);
  if ((EnableConfig & GPIO_OUTPUT_BIT_VALUE) == 0) {
    *Mode = GPIO_MODE_INPUT;
  } else {
    State = MmioRead32 (Address + GPIO_OUTPUT_VALUE_OFFSET);
    if (State == 0) {
      *Mode = GPIO_MODE_OUTPUT_0;
    } else {
      *Mode = GPIO_MODE_OUTPUT_1;
    }
  }

  return EFI_SUCCESS;
}

/**
 * Sets the pull-up / pull-down resistor of a GPIO pin
 *
 * @param[in]  This       pointer to protocol
 * @param[in]  Gpio       which pin
 * @param[in]  Direction  pull-up, pull-down, or none
 *
 * @return EFI_SUCCESS - pin was set
 */
EFI_STATUS
SetGpioPull (
  IN  EMBEDDED_GPIO       *This,
  IN  EMBEDDED_GPIO_PIN   Gpio,
  IN  EMBEDDED_GPIO_PULL  Direction
  )
{
  return EFI_UNSUPPORTED;
}

STATIC CONST EMBEDDED_GPIO  mGpioEmbeddedProtocol = {
  .Get     = GetGpioState,
  .Set     = SetGpioState,
  .GetMode = GetGpioMode,
  .SetPull = SetGpioPull
};

/**
 * Installs the Gpio protocols
 *
 * @return EFI_SUCCESS         - protocols installed
 * @return others              - Failed to install protocols
 */
STATIC
EFI_STATUS
InstallGpioProtocols (
  VOID
  )
{
  EFI_STATUS                Status;
  PLATFORM_GPIO_CONTROLLER  *GpioController      = NULL;
  UINTN                     CurrentController    = 0;
  UINTN                     ControllerIndex      = 0;
  UINTN                     GpioControllerIndex  = 0;
  UINTN                     TotalControllerCount = 0;
  EFI_HANDLE                Handle;

  for (GpioControllerIndex = 0; GpioControllerIndex < mControllerCount; GpioControllerIndex++) {
    TotalControllerCount += mControllerArray[GpioControllerIndex].ControllerCount;
  }

  GpioController = (PLATFORM_GPIO_CONTROLLER *)AllocatePool (sizeof (PLATFORM_GPIO_CONTROLLER) + (TotalControllerCount * sizeof (GPIO_CONTROLLER)));
  if (NULL == GpioController) {
    return EFI_OUT_OF_RESOURCES;
  }

  GpioController->GpioControllerCount = TotalControllerCount;
  GpioController->GpioCount           = TotalControllerCount * GPIO_PINS_PER_CONTROLLER;
  GpioController->GpioController      = (GPIO_CONTROLLER *)((UINTN)GpioController + sizeof (PLATFORM_GPIO_CONTROLLER));

  CurrentController = 0;
  for (GpioControllerIndex = 0; GpioControllerIndex < mControllerCount; GpioControllerIndex++) {
    CopyMem (
      GpioController->GpioController + CurrentController,
      mControllerArray[GpioControllerIndex].ControllerDefault,
      mControllerArray[GpioControllerIndex].ControllerCount * sizeof (GPIO_CONTROLLER)
      );

    for (ControllerIndex = 0; ControllerIndex < mControllerArray[GpioControllerIndex].ControllerCount; ControllerIndex++) {
      GpioController->GpioController[CurrentController].GpioIndex     = GPIO (mControllerArray[GpioControllerIndex].Handle, GpioController->GpioController[CurrentController].GpioIndex);
      GpioController->GpioController[CurrentController].RegisterBase += mControllerArray[GpioControllerIndex].BaseAddress;
      CurrentController++;
    }
  }

  Handle = NULL;
  Status = gMmst->MmInstallProtocolInterface (
                    &Handle,
                    &gEmbeddedGpioProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    (VOID *)&mGpioEmbeddedProtocol
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install gpio protocol: %r\n", __FUNCTION__, Status));
    FreePool (GpioController);
  } else {
    mGpioController = GpioController;
  }

  return Status;
}

/**
  get map table entry for given name

  @param[in] Name                  Name to find.

  @retval TEGRA_GPIO_MAP *         Map table entry pointer or NULL if not found.

**/
STATIC
CONST TEGRA_GPIO_MAP *
EFIAPI
TegraGpioStmmGetMap (
  IN CONST CHAR8  *Name
  )
{
  UINTN                 Index;
  CONST TEGRA_GPIO_MAP  *Map;

  Map = mTegraGpioMap;
  for (Index = 0; Index < ARRAY_SIZE (mTegraGpioMap); Index++, Map++) {
    if (AsciiStrnCmp (Map->Name, Name, AsciiStrLen (Map->Name)) == 0) {
      return Map;
    }
  }

  return NULL;
}

/**
  Initialize the GPIO standalone MM driver

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
TegraGpioStmmInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS            Status;
  EFI_MM_DEVICE_REGION  *Regions;
  UINT32                NumRegions;
  UINTN                 Index;
  CONST TEGRA_GPIO_MAP  *Map;
  CHAR8                 ControllerType[DEVICE_REGION_NAME_MAX_LEN];
  CONST CHAR8           *DeviceNameEnd;
  CONST CHAR8           *DeviceName;

  Status = GetDeviceTypeRegions ("gpio", &Regions, &NumRegions);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to get gpio regions: %r\n", __FUNCTION__, Status));
    return Status;
  }

  for (Index = 0; Index < NumRegions; Index++) {
    DeviceName    = Regions[Index].DeviceRegionName;
    DeviceNameEnd = AsciiStrStr (DeviceName, "-socket");
    if (DeviceNameEnd == NULL) {
      DeviceNameEnd = DeviceName + AsciiStrLen (DeviceName);
    }

    ZeroMem (ControllerType, sizeof (ControllerType));
    CopyMem (ControllerType, DeviceName, DeviceNameEnd - DeviceName);
    Map = TegraGpioStmmGetMap (ControllerType);
    if (Map == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: no map for %a\n", __FUNCTION__, ControllerType));
      continue;
    }

    DEBUG ((DEBUG_INFO, "%a: found %a map for %a, %u controllers\n", __FUNCTION__, ControllerType, DeviceName, Map->NumControllers));

    mControllerArray = ReallocatePool (
                         mControllerCount * sizeof (NVIDIA_GPIO_CONTROLLER_ENTRY),
                         (mControllerCount + 1) * sizeof (NVIDIA_GPIO_CONTROLLER_ENTRY),
                         mControllerArray
                         );

    if (mControllerArray == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate new array\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    mControllerArray[mControllerCount].BaseAddress       = Regions[Index].DeviceRegionStart;
    mControllerArray[mControllerCount].Handle            = GetDeviceSocketNum (DeviceName);
    mControllerArray[mControllerCount].ControllerCount   = Map->NumControllers;
    mControllerArray[mControllerCount].ControllerDefault = Map->Controllers;

    mControllerCount++;
  }

  InstallGpioProtocols ();

  return EFI_SUCCESS;
}
