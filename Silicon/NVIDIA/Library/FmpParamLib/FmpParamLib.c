/** @file

  FMP parameter library

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/FmpParamLib.h>
#include <Library/PcdLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>

STATIC UINT32  mDtbLsv = 0;

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
  EFI_STATUS  Status;
  VOID        *DtbBase;
  UINTN       DtbSize;
  INT32       UefiNode;
  CONST VOID  *Property;
  INT32       Length;
  EFI_GUID    DtbImageTypeIdGuid;
  UINTN       GuidSize;
  BOOLEAN     DtbGuidValid = FALSE;
  UINTN       Index;
  EFI_GUID    *NonUniqueGuid;
  UINTN       NonUniqueGuidCount;
  UINTN       PcdLength;

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

    if (FeaturePcdGet (PcdSupportFmpCertsInDtb)) {
      Property = fdt_getprop (DtbBase, UefiNode, "fmp-pkcs7-cert-buffer-xdr", &Length);
      if ((Property != NULL) && (Length > 0)) {
        PcdLength = Length;
        DEBUG ((DEBUG_INFO, "%a: setting PcdFmpDevicePkcs7CertBufferXdr Length %d\n", __FUNCTION__, PcdLength));
        Status = PcdSetPtrS (PcdFmpDevicePkcs7CertBufferXdr, &PcdLength, Property);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: set PcdFmpDevicePkcs7CertBufferXdr failed Length %d: %r\n", __FUNCTION__, PcdLength, Status));
        }
      }
    }
  }

Done:
  NonUniqueGuid      = PcdGetPtr (PcdNonUniqueSystemFmpCapsuleImageTypeIdGuid);
  NonUniqueGuidCount = PcdGetSize (PcdNonUniqueSystemFmpCapsuleImageTypeIdGuid) / sizeof (EFI_GUID);

  for (Index = 0; Index < NonUniqueGuidCount; Index++, NonUniqueGuid++) {
    if (NonUniqueGuid == NULL) {
      break;
    }

    if (CompareGuid (NonUniqueGuid, PcdGetPtr (PcdSystemFmpCapsuleImageTypeIdGuid))) {
      DEBUG ((DEBUG_WARN, "%a: WARNING: Default FMP image type ID GUID is not unique to this platform! (%g)\n", __FUNCTION__, PcdGetPtr (PcdSystemFmpCapsuleImageTypeIdGuid)));
      break;
    }
  }

  if (PcdGetSize (PcdFmpDevicePkcs7CertBufferXdr) == 1) {
    DEBUG ((DEBUG_WARN, "%a: WARNING: PcdFmpDevicePkcs7CertBufferXdr not set, capsule update not possible.\n", __FUNCTION__));
  }
}
