/** @file
  HBM Memory Proximity domain config Parser

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Obtain the total number of proximity domains
  This includes, CPU, GPU and GPU HBM domains
  @param[in] VOID
**/
UINT32
EFIAPI
GetMaxPxmDomains (
  VOID
  );

/**
  Check if GPU is enabled on given Socket ID
  @param[in] UINT32      SocketId
  @return    BOOLEAN     (TRUE if enabled
                          FALSE if disabled)
**/
BOOLEAN
EFIAPI
IsGpuEnabledOnSocket (
  UINTN  SocketId
  );

/**
  Obtain the total number of proximity domains
  This includes, CPU, GPU and GPU HBM domains
  @param[in] VOID
**/
UINT32
EFIAPI
GetMaxPxmDomains (
  VOID
  );

EFI_STATUS
EFIAPI
GenerateHbmMemPxmDmnMap (
  VOID
  );
