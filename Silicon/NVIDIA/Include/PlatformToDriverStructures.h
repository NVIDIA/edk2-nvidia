/** @file

  Parameter Info structures for all drivers supported by
  Platform To Driver Config Protocol

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

/**

  SDMMC properties extracted from DT

**/

typedef struct {
  BOOLEAN VmmcRegulatorIdPresent;
  BOOLEAN VqmmcRegulatorIdPresent;
  BOOLEAN NonRemovable;
  BOOLEAN Only1v8;
  UINT32  VmmcRegulatorId;
  UINT32  VqmmcRegulatorId;
} SDMMC_PARAMETER_INFO;
