/** @file

  Tegra GPIO Driver

  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/EmbeddedGpio.h>
#include <libfdt.h>

#include "TegraGpioPrivate.h"

STATIC CONST GPIO_CONTROLLER  Tegra194GpioControllers[] = {
  TEGRA_GPIO_ENTRY (0,  1, 2, 8),
  TEGRA_GPIO_ENTRY (1,  4, 7, 2),
  TEGRA_GPIO_ENTRY (2,  4, 3, 8),
  TEGRA_GPIO_ENTRY (3,  4, 4, 4),
  TEGRA_GPIO_ENTRY (4,  4, 5, 8),
  TEGRA_GPIO_ENTRY (5,  4, 6, 6),
  TEGRA_GPIO_ENTRY (6,  4, 0, 8),
  TEGRA_GPIO_ENTRY (7,  4, 1, 8),
  TEGRA_GPIO_ENTRY (8,  4, 2, 5),
  TEGRA_GPIO_ENTRY (9,  5, 1, 6),
  TEGRA_GPIO_ENTRY (10, 3, 0, 8),
  TEGRA_GPIO_ENTRY (11, 3, 1, 4),
  TEGRA_GPIO_ENTRY (12, 2, 3, 8),
  TEGRA_GPIO_ENTRY (13, 2, 4, 3),
  TEGRA_GPIO_ENTRY (14, 5, 0, 6),
  TEGRA_GPIO_ENTRY (15, 2, 5, 8),
  TEGRA_GPIO_ENTRY (16, 2, 6, 8),
  TEGRA_GPIO_ENTRY (17, 2, 7, 6),
  TEGRA_GPIO_ENTRY (18, 3, 3, 8),
  TEGRA_GPIO_ENTRY (19, 3, 4, 8),
  TEGRA_GPIO_ENTRY (20, 3, 5, 1),
  TEGRA_GPIO_ENTRY (21, 1, 0, 8),
  TEGRA_GPIO_ENTRY (22, 1, 1, 2),
  TEGRA_GPIO_ENTRY (23, 2, 0, 8),
  TEGRA_GPIO_ENTRY (24, 2, 1, 8),
  TEGRA_GPIO_ENTRY (25, 2, 2, 8),
  TEGRA_GPIO_ENTRY (26, 3, 2, 2),
  TEGRA_GPIO_ENTRY (27, 0, 0, 2)
};

STATIC CONST GPIO_CONTROLLER  Tegra194GpioAonControllers[] = {
  TEGRA_GPIO_ENTRY (0, 0, 3, 8),
  TEGRA_GPIO_ENTRY (1, 0, 4, 4),
  TEGRA_GPIO_ENTRY (2, 0, 1, 8),
  TEGRA_GPIO_ENTRY (3, 0, 2, 3),
  TEGRA_GPIO_ENTRY (4, 0, 0, 7)
};

STATIC CONST GPIO_CONTROLLER  Tegra234GpioControllers[] = {
  TEGRA_GPIO_ENTRY (0,  0, 0, 8),
  TEGRA_GPIO_ENTRY (1,  0, 3, 1),
  TEGRA_GPIO_ENTRY (2,  5, 1, 8),
  TEGRA_GPIO_ENTRY (3,  5, 2, 4),
  TEGRA_GPIO_ENTRY (4,  5, 3, 8),
  TEGRA_GPIO_ENTRY (5,  5, 4, 6),
  TEGRA_GPIO_ENTRY (6,  4, 0, 8),
  TEGRA_GPIO_ENTRY (7,  4, 1, 8),
  TEGRA_GPIO_ENTRY (8,  4, 2, 7),
  TEGRA_GPIO_ENTRY (9,  5, 0, 6),
  TEGRA_GPIO_ENTRY (10, 3, 0, 8),
  TEGRA_GPIO_ENTRY (11, 3, 1, 4),
  TEGRA_GPIO_ENTRY (12, 2, 0, 8),
  TEGRA_GPIO_ENTRY (13, 2, 1, 8),
  TEGRA_GPIO_ENTRY (14, 2, 2, 8),
  TEGRA_GPIO_ENTRY (15, 2, 3, 8),
  TEGRA_GPIO_ENTRY (16, 2, 4, 6),
  TEGRA_GPIO_ENTRY (17, 1, 0, 8),
  TEGRA_GPIO_ENTRY (18, 1, 1, 8),
  TEGRA_GPIO_ENTRY (19, 1, 2, 8),
  TEGRA_GPIO_ENTRY (20, 0, 1, 8),
  TEGRA_GPIO_ENTRY (21, 0, 2, 4),
  TEGRA_GPIO_ENTRY (22, 3, 3, 2),
  TEGRA_GPIO_ENTRY (23, 3, 4, 4),
  TEGRA_GPIO_ENTRY (24, 3, 2, 8),
};

STATIC CONST GPIO_CONTROLLER  Tegra234GpioAonControllers[] = {
  TEGRA_GPIO_ENTRY (0, 0, 4, 8),
  TEGRA_GPIO_ENTRY (1, 0, 5, 4),
  TEGRA_GPIO_ENTRY (2, 0, 2, 8),
  TEGRA_GPIO_ENTRY (3, 0, 3, 3),
  TEGRA_GPIO_ENTRY (4, 0, 0, 8),
  TEGRA_GPIO_ENTRY (5, 0, 1, 1),
};

STATIC CONST GPIO_CONTROLLER  Tegra23xGpioControllers[] = {
  TEGRA_GPIO_ENTRY (0,  0, 0, 8),
  TEGRA_GPIO_ENTRY (1,  0, 1, 5),
  TEGRA_GPIO_ENTRY (2,  0, 2, 8),
  TEGRA_GPIO_ENTRY (3,  0, 3, 8),
  TEGRA_GPIO_ENTRY (4,  0, 4, 4),
  TEGRA_GPIO_ENTRY (5,  0, 5, 8),
  TEGRA_GPIO_ENTRY (6,  0, 6, 8),
  TEGRA_GPIO_ENTRY (7,  0, 7, 6),
  TEGRA_GPIO_ENTRY (8,  1, 0, 8),
  TEGRA_GPIO_ENTRY (9,  1, 1, 4),
  TEGRA_GPIO_ENTRY (10, 1, 2, 8),
  TEGRA_GPIO_ENTRY (11, 1, 3, 8),
  TEGRA_GPIO_ENTRY (12, 1, 4, 3),
  TEGRA_GPIO_ENTRY (13, 1, 5, 8),
  TEGRA_GPIO_ENTRY (14, 1, 6, 3),
  TEGRA_GPIO_ENTRY (15, 2, 0, 8),
  TEGRA_GPIO_ENTRY (16, 2, 1, 8),
  TEGRA_GPIO_ENTRY (17, 2, 2, 8),
  TEGRA_GPIO_ENTRY (18, 2, 3, 6),
  TEGRA_GPIO_ENTRY (19, 2, 4, 2),
  TEGRA_GPIO_ENTRY (20, 3, 0, 8),
  TEGRA_GPIO_ENTRY (21, 3, 1, 2)
};

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

STATIC CONST GPIO_CONTROLLER  Th500GpioAonControllers[] = {
  TEGRA_GPIO_ENTRY (0, 0, 0, 8),
  TEGRA_GPIO_ENTRY (1, 0, 0, 4)
};

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra194-gpio",     &gNVIDIANonDiscoverableT194GpioDeviceGuid     },
  { "nvidia,tegra194-gpio-aon", &gNVIDIANonDiscoverableT194GpioAonDeviceGuid  },
  { "nvidia,tegra234-gpio",     &gNVIDIANonDiscoverableT234GpioDeviceGuid     },
  { "nvidia,tegra234-gpio-aon", &gNVIDIANonDiscoverableT234GpioAonDeviceGuid  },
  { "nvidia,tegra23x-gpio",     &gNVIDIANonDiscoverableT23xGpioDeviceGuid     },
  { "nvidia,th500-gpio",        &gNVIDIANonDiscoverableTH500GpioDeviceGuid    },
  { "nvidia,th500-gpio-aon",    &gNVIDIANonDiscoverableTH500GpioAonDeviceGuid },
  { NULL,                       NULL                                          }
};

NVIDIA_GPIO_CONTROLLER_ENTRY  *mControllerArray = NULL;
UINT32                        mControllerCount  = 0;

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA Gpio controller driver",
  .UseDriverBinding                = FALSE,
  .AutoEnableClocks                = FALSE,
  .AutoDeassertReset               = FALSE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .DirectEnumerationSupport        = TRUE
};

STATIC PLATFORM_GPIO_CONTROLLER  *mGpioController  = NULL;
STATIC EMBEDDED_GPIO             *mI2cExpanderGpio = NULL;

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
    return mI2cExpanderGpio->Get (mI2cExpanderGpio, Gpio, Value);
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
    return mI2cExpanderGpio->Set (mI2cExpanderGpio, Gpio, Mode);
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
    return mI2cExpanderGpio->GetMode (mI2cExpanderGpio, Gpio, Mode);
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
 * Installs the Gpio protocols onto the handle
 *
 * @param[in] DriverHandle - driver handle of Gpio controller
 *
 * @return EFI_SUCCESS         - protocols installed
 * @return others              - Failed to install protocols
 */
STATIC
EFI_STATUS
InstallGpioProtocols (
  IN  EFI_HANDLE  DriverHandle
  )
{
  EFI_STATUS                Status;
  PLATFORM_GPIO_CONTROLLER  *GpioController            = NULL;
  PLATFORM_GPIO_CONTROLLER  *I2cExpanderGpioController = NULL;
  UINTN                     CurrentController          = 0;
  UINTN                     ControllerIndex            = 0;
  UINTN                     GpioControllerIndex        = 0;
  UINTN                     TotalControllerCount       = 0;

  for (GpioControllerIndex = 0; GpioControllerIndex < mControllerCount; GpioControllerIndex++) {
    TotalControllerCount += mControllerArray[GpioControllerIndex].ControllerCount;
  }

  Status = gBS->LocateProtocol (&gNVIDIAI2cExpanderGpioProtocolGuid, NULL, (VOID **)&mI2cExpanderGpio);
  if (EFI_ERROR (Status) || (mI2cExpanderGpio == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: No I2C expander protocol found\r\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  Status = gBS->LocateProtocol (&gNVIDIAI2cExpanderPlatformGpioProtocolGuid, NULL, (VOID **)&I2cExpanderGpioController);
  if (EFI_ERROR (Status) || (I2cExpanderGpioController == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: No I2C expander platform protocol found\r\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  GpioController = (PLATFORM_GPIO_CONTROLLER *)AllocatePool (sizeof (PLATFORM_GPIO_CONTROLLER) + (TotalControllerCount +  I2cExpanderGpioController->GpioControllerCount) * sizeof (GPIO_CONTROLLER));
  if (NULL == GpioController) {
    return EFI_OUT_OF_RESOURCES;
  }

  GpioController->GpioControllerCount = TotalControllerCount + I2cExpanderGpioController->GpioControllerCount;
  GpioController->GpioCount           = TotalControllerCount * GPIO_PINS_PER_CONTROLLER + I2cExpanderGpioController->GpioCount;
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

  CopyMem (GpioController->GpioController + CurrentController, I2cExpanderGpioController->GpioController, I2cExpanderGpioController->GpioControllerCount * sizeof (GPIO_CONTROLLER));
  mGpioController = GpioController;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gPlatformGpioProtocolGuid,
                  mGpioController,
                  &gEmbeddedGpioProtocolGuid,
                  &mGpioEmbeddedProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    FreePool (mGpioController);
    mGpioController = NULL;
  }

  return Status;
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
  EFI_STATUS               Status;
  UINTN                    GpioBaseAddress = 0;
  UINTN                    GpioRegionSize  = 0;
  NON_DISCOVERABLE_DEVICE  *Device         = NULL;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      mControllerArray = ReallocatePool (
                           mControllerCount * sizeof (NVIDIA_GPIO_CONTROLLER_ENTRY),
                           (mControllerCount + 1) * sizeof (NVIDIA_GPIO_CONTROLLER_ENTRY),
                           mControllerArray
                           );

      if (mControllerArray == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate new array\r\n", __FUNCTION__));
        return EFI_OUT_OF_RESOURCES;
      }

      Status = DeviceDiscoveryGetMmioRegion (
                 ControllerHandle,
                 1,
                 &GpioBaseAddress,
                 &GpioRegionSize
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      mControllerArray[mControllerCount].BaseAddress = GpioBaseAddress;
      mControllerArray[mControllerCount].Handle      = fdt_get_phandle (
                                                         DeviceTreeNode->DeviceTreeBase,
                                                         DeviceTreeNode->NodeOffset
                                                         );
      if ((mControllerArray[mControllerCount].Handle > MAX_UINT16)) {
        ASSERT (mControllerArray[mControllerCount].Handle <= MAX_UINT16);
        return EFI_UNSUPPORTED;
      }

      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gNVIDIANonDiscoverableDeviceProtocolGuid,
                      (VOID **)&Device
                      );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableT194GpioDeviceGuid)) {
        mControllerArray[mControllerCount].ControllerCount   = ARRAY_SIZE (Tegra194GpioControllers);
        mControllerArray[mControllerCount].ControllerDefault = Tegra194GpioControllers;
      } else if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableT194GpioAonDeviceGuid)) {
        mControllerArray[mControllerCount].ControllerCount   = ARRAY_SIZE (Tegra194GpioAonControllers);
        mControllerArray[mControllerCount].ControllerDefault = Tegra194GpioAonControllers;
      } else if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableT234GpioDeviceGuid)) {
        mControllerArray[mControllerCount].ControllerCount   = ARRAY_SIZE (Tegra234GpioControllers);
        mControllerArray[mControllerCount].ControllerDefault = Tegra234GpioControllers;
      } else if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableT234GpioAonDeviceGuid)) {
        mControllerArray[mControllerCount].ControllerCount   = ARRAY_SIZE (Tegra234GpioAonControllers);
        mControllerArray[mControllerCount].ControllerDefault = Tegra234GpioAonControllers;
      } else if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableT23xGpioDeviceGuid)) {
        mControllerArray[mControllerCount].ControllerCount   = ARRAY_SIZE (Tegra23xGpioControllers);
        mControllerArray[mControllerCount].ControllerDefault = Tegra23xGpioControllers;
      } else if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableTH500GpioDeviceGuid)) {
        mControllerArray[mControllerCount].ControllerCount   = ARRAY_SIZE (Th500GpioControllers);
        mControllerArray[mControllerCount].ControllerDefault = Th500GpioControllers;
      } else if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableTH500GpioAonDeviceGuid)) {
        mControllerArray[mControllerCount].ControllerCount   = ARRAY_SIZE (Th500GpioAonControllers);
        mControllerArray[mControllerCount].ControllerDefault = Th500GpioAonControllers;
      } else {
        return EFI_UNSUPPORTED;
      }

      mControllerCount++;
      return EFI_SUCCESS;

    case DeviceDiscoveryEnumerationCompleted:
      return InstallGpioProtocols (DriverHandle);

    default:
      return EFI_SUCCESS;
  }
}
