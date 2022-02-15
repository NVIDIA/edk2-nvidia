/** @file
*
*  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TEGRA_SERIAL_PORT_LIB_PRIVATE_H__
#define __TEGRA_SERIAL_PORT_LIB_PRIVATE_H__

#include <Library/TegraSerialPortLib.h>

/**
  Describe Tegra UART info populated based on DT
**/
typedef struct {
  EFI_PHYSICAL_ADDRESS  BaseAddress;
  UINT32                Interrupt;
  UINT32                Type;
} TEGRA_UART_INFO;

/**
  Retrieve the object of tegra serial port library

  @retval TEGRA_UART_OBJ *      Pointer to the Tegra UART library object
**/
typedef
TEGRA_UART_OBJ *
(EFIAPI * SERIAL_PORT_GET_OBJECT) (
  VOID
  );

/**
  Describe compatibility mapping regions terminated with NULL Compatibility string
**/
typedef struct {
  UINT32                  Type;
  SERIAL_PORT_GET_OBJECT  GetObject;
  CONST CHAR8           * Compatibility;
} SERIAL_MAPPING;

#endif //__TEGRA_SERIAL_PORT_LIB_PRIVATE_H__
