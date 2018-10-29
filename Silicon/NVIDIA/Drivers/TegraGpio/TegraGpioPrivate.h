/** @file

  Tegra Gpio Driver private structures

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __TEGRA_GPIO_PRIVATE_H__
#define __TEGRA_GPIO_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/EmbeddedGpio.h>

#define GPIO_PINS_PER_CONTROLLER  8

#define GPIO_REGISTER_SPACING     0x20

#define GPIO_ENABLE_CONFIG_OFFSET 0x00
#define GPIO_DBC_THRES_REG      0x04
#define GPIO_INPUT_VALUE_OFFSET   0x08
#define GPIO_OUTPUT_CONTROL_OFFET 0x0c
#define GPIO_OUTPUT_VALUE_OFFSET  0x10

#define GPIO_ENABLE_BIT           0
#define GPIO_ENABLE_BIT_VALUE     (1 << GPIO_ENABLE_BIT)
#define GPIO_OUTPUT_BIT           1
#define GPIO_OUTPUT_BIT_VALUE     (1 << GPIO_OUTPUT_BIT)



#define TEGRA_GPIO_ENTRY(Index, ControllerId, ControllerIndex, NumberOfPins) \
  {                                                                    \
    .RegisterBase = ControllerId * SIZE_4KB + ControllerIndex * 0x200, \
    .GpioIndex = GPIO_PINS_PER_CONTROLLER * Index,                     \
    .InternalGpioCount = NumberOfPins,                                 \
  }

#endif
