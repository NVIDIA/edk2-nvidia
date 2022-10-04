/** @file

  Tegra Pin Control Driver private structures

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef TEGRA_PIN_CONTROL_DXE_PRIVATE_H
#define TEGRA_PIN_CONTROL_DXE_PRIVATE_H

#include <PiDxe.h>
#include <Protocol/PinControl.h>

#define DP_AUX_CONTROL_SIGNATURE  SIGNATURE_32('D','P','A','X')
typedef struct {
  //
  // Standard signature used to identify regulator private data
  //
  UINT32                         Signature;

  NVIDIA_PIN_CONTROL_PROTOCOL    PinControlProtocol;

  UINT32                         PinControlId;

  EFI_PHYSICAL_ADDRESS           BaseAddress;
} DP_AUX_CONTROL_PRIVATE;
#define DP_AUX_CONTROL_PRIVATE_FROM_THIS(a)  CR(a, DP_AUX_CONTROL_PRIVATE, PinControlProtocol, DP_AUX_CONTROL_SIGNATURE)

#define PIN_CONTROL_SIGNATURE  SIGNATURE_32('P','I','N','C')
typedef struct {
  //
  // Standard signature used to identify regulator private data
  //
  UINT32                         Signature;

  NVIDIA_PIN_CONTROL_PROTOCOL    PinControlProtocol;

  UINTN                          NumberOfHandles;

  EFI_HANDLE                     *HandleArray;
} PIN_CONTROL_PRIVATE;
#define PIN_CONTROL_PRIVATE_FROM_THIS(a)  CR(a, PIN_CONTROL_PRIVATE, PinControlProtocol, PIN_CONTROL_SIGNATURE)

#endif
