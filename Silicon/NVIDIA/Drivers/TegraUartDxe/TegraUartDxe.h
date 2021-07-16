/** @file

  Tegra UART driver's private data structure

  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __TEGRA_UART_DXE_H__
#define __TEGRA_UART_DXE_H__

#include <Uefi.h>

#include <Library/TegraSerialPortLib.h>

#include <Protocol/SerialIo.h>

#define SERIAL_16550_IO_SIGNATURE SIGNATURE_64 ('1','6','5','5','0','U','A','R')
#define SERIAL_SBSA_IO_SIGNATURE  SIGNATURE_64 ('S','B','S','A','U','A','R','T')
#define SERIAL_TCU_IO_SIGNATURE   SIGNATURE_64 ('T','C','U','U','A','R','T','!')

typedef struct {
  EFI_SERIAL_IO_PROTOCOL SerialIoMode;
  UINT64                 Signature;
  TEGRA_UART_OBJ         *TegraUartObj;
  UINTN                  SerialBaseAddress;
} TEGRA_UART_PRIVATE_DATA;

#define SERIAL_16550_IO_PRIVATE_DATA_FROM_PROTOCOL(a) CR (a, TEGRA_UART_PRIVATE_DATA, SerialIoMode, SERIAL_16550_IO_SIGNATURE)
#define SERIAL_SBSA_IO_PRIVATE_DATA_FROM_PROTOCOL(a)  CR (a, TEGRA_UART_PRIVATE_DATA, SerialIoMode, SERIAL_SBSA_IO_SIGNATURE)
#define SERIAL_TCU_IO_PRIVATE_DATA_FROM_PROTOCOL(a)   CR (a, TEGRA_UART_PRIVATE_DATA, SerialIoMode, SERIAL_TCU_IO_SIGNATURE)

#endif // __TEGRA_UART_DXE_H__
