/** @file
  FW Partition Protocol Image Names support

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/FwImageProtocol.h>

STATIC CONST CHAR16  *SystemFwImageNamesCommon[] = {
  L"adsp-fw",
  L"bpmp-fw",
  L"bpmp-fw-dtb",
  L"cpu-bootloader",
  L"mb1",
  L"MB1_BCT",
  L"mb2",
  L"MEM_BCT",
  L"mts-mce",
  L"rce-fw",
  L"sc7",
  L"secure-os",
  L"spe-fw",
  L"VER",
  NULL
};

STATIC CONST CHAR16  *SystemFwImageNamesT194[] = {
  L"bootloader-dtb",
  L"mts-preboot",
  L"mts-proper",
  NULL
};

STATIC CONST CHAR16  *SystemFwImageNamesT234[] = {
  L"dce-fw",
  L"mb2rf",
  L"nvdec",
  L"psc_bl1",
  L"psc-fw",
  L"pscrf",
  L"pva-fw",
  NULL
};

/**
  Get count of entries in NULL-terminated list.

  @param[in]   List                 Pointer to list of image names

  @retval UINTN                     Number of entries in list

**/
STATIC
UINTN
GetListCount (
  IN  CONST CHAR16  **List
  )
{
  UINTN  Count;

  for (Count = 0; *List != NULL; Count++, List++) {
  }

  return Count;
}

/**
  Combine two NULL-terminated lists of FW image names.

  @param[in]   L1                   Pointer to first list of image names
  @param[in]   L2                   Pointer to second list of image names
  @param[out]  Count                Number of images names in combined list

  @retval CONST CHAR16**            Pointer to NULL-terminated combined
                                    list of image names, caller must free
                                    with FreePool()
  @retval NULL                      Error allocating combined name list

**/
STATIC
CONST CHAR16 **
EFIAPI
CombineLists (
  IN  CONST CHAR16  **L1,
  IN  CONST CHAR16  **L2,
  OUT UINTN         *Count
  )
{
  UINTN         L1Count;
  UINTN         L2Count;
  CONST CHAR16  **CombinedList;

  L1Count = GetListCount (L1);
  L2Count = GetListCount (L2);
  *Count  = L1Count + L2Count;

  CombinedList = (CONST CHAR16 **)AllocateZeroPool (
                                    (*Count + 1) *
                                    sizeof (CHAR16 *)
                                    );
  if (CombinedList == NULL) {
    return NULL;
  }

  CopyMem (CombinedList, L1, L1Count * sizeof (CHAR16 *));
  CopyMem (&CombinedList[L1Count], L2, L2Count * sizeof (CHAR16 *));
  CombinedList[*Count] = NULL;

  return CombinedList;
}

/**
  Get list of required FW image names for the platform.

  @param[in]   ChipId               Chip ID of list to get
  @param[out]  ImageCount           Number of images in list

  @retval CONST CHAR16**            Pointer to array of image names,
                                    caller must free with FreePool()
  @retval NULL                      Error determining image name list

**/
CONST CHAR16 **
EFIAPI
FwImageGetRequiredList (
  IN  UINTN  ChipId,
  OUT UINTN  *ImageCount
  )
{
  CONST CHAR16  **ImageList;

  switch (ChipId) {
    case T194_CHIP_ID:
      ImageList = CombineLists (
                    SystemFwImageNamesT194,
                    SystemFwImageNamesCommon,
                    ImageCount
                    );
      break;
    case T234_CHIP_ID:
      ImageList = CombineLists (
                    SystemFwImageNamesT234,
                    SystemFwImageNamesCommon,
                    ImageCount
                    );
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: Unknown ChipId=%u\n", __FUNCTION__, ChipId));
      ImageList   = NULL;
      *ImageCount = 0;
      break;
  }

  ASSERT (*ImageCount <= FW_IMAGE_MAX_IMAGES);

  return ImageList;
}
