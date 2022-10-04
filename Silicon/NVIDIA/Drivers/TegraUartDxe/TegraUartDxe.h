/** @file

  Tegra UART driver's private data structure

  Copyright (c) 2020-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TEGRA_UART_DXE_H__
#define __TEGRA_UART_DXE_H__

#include <Uefi.h>

#include <Library/TegraSerialPortLib.h>

#include <Protocol/SerialIo.h>

#define SERIAL_16550_IO_SIGNATURE  SIGNATURE_64 ('1','6','5','5','0','U','A','R')
#define SERIAL_SBSA_IO_SIGNATURE   SIGNATURE_64 ('S','B','S','A','U','A','R','T')
#define SERIAL_TCU_IO_SIGNATURE    SIGNATURE_64 ('T','C','U','U','A','R','T','!')

typedef struct {
  EFI_SERIAL_IO_PROTOCOL    SerialIoMode;
  UINT64                    Signature;
  TEGRA_UART_OBJ            *TegraUartObj;
  UINTN                     SerialBaseAddress;
} TEGRA_UART_PRIVATE_DATA;

#define SERIAL_16550_IO_PRIVATE_DATA_FROM_PROTOCOL(a)  CR (a, TEGRA_UART_PRIVATE_DATA, SerialIoMode, SERIAL_16550_IO_SIGNATURE)
#define SERIAL_SBSA_IO_PRIVATE_DATA_FROM_PROTOCOL(a)   CR (a, TEGRA_UART_PRIVATE_DATA, SerialIoMode, SERIAL_SBSA_IO_SIGNATURE)
#define SERIAL_TCU_IO_PRIVATE_DATA_FROM_PROTOCOL(a)    CR (a, TEGRA_UART_PRIVATE_DATA, SerialIoMode, SERIAL_TCU_IO_SIGNATURE)

#endif // __TEGRA_UART_DXE_H__
