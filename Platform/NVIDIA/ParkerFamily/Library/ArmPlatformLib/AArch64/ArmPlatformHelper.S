/** @file
*
*  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2013-2014, ARM Limited. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <AsmMacroIoLibV8.h>
#include <Library/ArmLib.h>

//UINTN
//ArmPlatformGetCorePosition (
//  IN UINTN MpId
//  );
ASM_FUNC(ArmPlatformGetCorePosition)
  mov   x0, #0
  ret

//UINTN
//ArmPlatformGetPrimaryCoreMpId (
//  VOID
//  );
ASM_FUNC(ArmPlatformGetPrimaryCoreMpId)
  bl    ASM_PFX(ArmReadMpidr)
  ret

//UINTN
//ArmPlatformIsPrimaryCore (
//  IN UINTN MpId
//  );
ASM_FUNC(ArmPlatformIsPrimaryCore)
  mov  x0, #1
  ret

ASM_FUNC(ArmPlatformPeiBootAction)
  //b ArmPlatformPeiBootAction
  ret

