/** @file

  FMP parameter library

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/FmpParamLib.h>
#include <Library/PcdLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>

STATIC UINT32       mDtbLsv                  = 0;
STATIC CONST CHAR8  *mNonUniqueTypeIdGuids[] = {
  "8655e5cf-297b-4213-84d5-b6817203a432",   // th500
};

EFI_STATUS
EFIAPI
FmpParamGetLowestSupportedVersion (
  OUT UINT32  *Lsv
  )
{
  UINT32  PcdLsv;

  if (Lsv == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PcdLsv = PcdGet32 (PcdFmpDeviceBuildTimeLowestSupportedVersion);

  *Lsv = (mDtbLsv > PcdLsv) ? mDtbLsv : PcdLsv;

  return EFI_SUCCESS;
}

VOID
EFIAPI
FmpParamLibInit (
  VOID
  )
{
  EFI_STATUS   Status;
  VOID         *DtbBase;
  UINTN        DtbSize;
  INT32        UefiNode;
  CONST VOID   *Property;
  INT32        Length;
  EFI_GUID     DtbImageTypeIdGuid;
  UINTN        GuidSize;
  BOOLEAN      DtbGuidValid = FALSE;
  UINTN        Index;
  CONST CHAR8  **NonUniqueGuid;
  EFI_GUID     GuidData;

  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: couldn't load DT\n", __FUNCTION__));
    goto Done;
  }

  UefiNode = fdt_path_offset (DtbBase, "/firmware/uefi");
  if (UefiNode >= 0) {
    Property = fdt_getprop (DtbBase, UefiNode, "fmp-lowest-supported-version", &Length);
    if ((Property != NULL) && (Length == sizeof (UINT32))) {
      mDtbLsv = fdt32_to_cpu (*(UINT32 *)Property);

      DEBUG ((DEBUG_INFO, "%a: Got LSV from dtb=0x%x pcd=0x%x\n", __FUNCTION__, mDtbLsv, PcdGet32 (PcdFmpDeviceBuildTimeLowestSupportedVersion)));
    }

    Property = fdt_getprop (DtbBase, UefiNode, "fmp-image-type-id-guid", &Length);
    if ((Property != NULL) && (Length == 37)) {
      Status = AsciiStrToGuid ((CONST CHAR8 *)Property, &DtbImageTypeIdGuid);
      if (!EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "%a: Updating guid from dtb=%g pcd=%g\n", __FUNCTION__, &DtbImageTypeIdGuid, PcdGetPtr (PcdSystemFmpCapsuleImageTypeIdGuid)));

        GuidSize = sizeof (DtbImageTypeIdGuid);
        PcdSetPtrS (PcdSystemFmpCapsuleImageTypeIdGuid, &GuidSize, &DtbImageTypeIdGuid);
        DtbGuidValid = TRUE;
      }
    }
  }

Done:
  NonUniqueGuid = mNonUniqueTypeIdGuids;
  for (Index = 0; Index < ARRAY_SIZE (mNonUniqueTypeIdGuids); Index++, NonUniqueGuid++) {
    AsciiStrToGuid (*NonUniqueGuid, &GuidData);
    if (CompareGuid (&GuidData, PcdGetPtr (PcdSystemFmpCapsuleImageTypeIdGuid))) {
      DEBUG ((DEBUG_WARN, "%a: WARNING: Default FMP image type ID GUID is not unique to this platform! (%g)\n", __FUNCTION__, PcdGetPtr (PcdSystemFmpCapsuleImageTypeIdGuid)));
      break;
    }
  }
}
