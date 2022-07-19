/** @file

  Tegra Gpio Driver private structures

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TEGRA_GPIO_PRIVATE_H__
#define __TEGRA_GPIO_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/EmbeddedGpio.h>

#define GPIO_PINS_PER_CONTROLLER  8

#define GPIO_REGISTER_SPACING  0x20

#define GPIO_ENABLE_CONFIG_OFFSET  0x00
#define GPIO_DBC_THRES_REG         0x04
#define GPIO_INPUT_VALUE_OFFSET    0x08
#define GPIO_OUTPUT_CONTROL_OFFET  0x0c
#define GPIO_OUTPUT_VALUE_OFFSET   0x10

#define GPIO_ENABLE_BIT        0
#define GPIO_ENABLE_BIT_VALUE  (1 << GPIO_ENABLE_BIT)
#define GPIO_OUTPUT_BIT        1
#define GPIO_OUTPUT_BIT_VALUE  (1 << GPIO_OUTPUT_BIT)

#define TEGRA_GPIO_ENTRY(Index, ControllerId, ControllerIndex, NumberOfPins) \
  {                                                                    \
    .RegisterBase = ControllerId * SIZE_4KB + ControllerIndex * 0x200, \
    .GpioIndex = GPIO_PINS_PER_CONTROLLER * Index,                     \
    .InternalGpioCount = NumberOfPins,                                 \
  }

typedef struct {
  EFI_PHYSICAL_ADDRESS     BaseAddress;
  UINT32                   Handle;
  UINTN                    ControllerCount;
  CONST GPIO_CONTROLLER    *ControllerDefault;
} NVIDIA_GPIO_CONTROLLER_ENTRY;

#endif
