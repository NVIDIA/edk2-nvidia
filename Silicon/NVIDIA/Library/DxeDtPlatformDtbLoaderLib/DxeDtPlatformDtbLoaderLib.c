/** @file
*
*  Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <libfdt.h>

/**
  Return a pool allocated copy of the DTB image that is appropriate for
  booting the current platform via DT.

  @param[out]   Dtb                   Pointer to the DTB copy
  @param[out]   DtbSize               Size of the DTB copy

  @retval       EFI_SUCCESS           Operation completed successfully
  @retval       EFI_NOT_FOUND         No suitable DTB image could be located
  @retval       EFI_OUT_OF_RESOURCES  No pool memory available

**/
EFI_STATUS
EFIAPI
DtPlatformLoadDtb (
  OUT   VOID        **Dtb,
  OUT   UINTN       *DtbSize
  )
{
  VOID            *Hob = NULL;

  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if (Hob == NULL || GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64)) {
    return EFI_NOT_FOUND;
  }
  *Dtb = (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);

  if (fdt_check_header (*Dtb) != 0) {
    DEBUG ((EFI_D_ERROR, "%a: No DTB found @ 0x%p\n", __FUNCTION__,
      *Dtb));
    return EFI_NOT_FOUND;
  }

  *DtbSize = fdt_totalsize (*Dtb);

  return EFI_SUCCESS;
}
