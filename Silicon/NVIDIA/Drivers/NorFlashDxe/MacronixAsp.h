/** @file

  Macronix ASP (Advanced Sector Protection) functions

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MACRONIX_ASP_H__
#define __MACRONIX_ASP_H__

#include <NorFlashPrivate.h>

EFI_STATUS
MxAspInitialize (
  UINT64  QspiBaseAddress
  );

EFI_STATUS
MxAspIsInitialized (
  BOOLEAN  *Initialized
  );

EFI_STATUS
MxAspEnable (
  VOID
  );

EFI_STATUS
MxAspIsEnabled (
  BOOLEAN  *Enabled
  );

EFI_STATUS
MxAspLock (
  UINT32  Address
  );

EFI_STATUS
MxAspIsLocked (
  UINT32   Address,
  BOOLEAN  *Locked
  );

#endif
