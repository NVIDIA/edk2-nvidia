/** @file

  Parameter Info structures for all drivers supported by
  Platform To Driver Config Protocol

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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


/**

  GOP Driver properties extracted from DT

**/

typedef struct {
  UINT8  *DcbImage;
  UINT32 DcbImageLen;
} GOP_PARAMETER_INFO;
