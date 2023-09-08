/* @file

  Copyright (c) 2020 Arm, Limited. All rights reserved.
  Copyright (c) 2023 Connect Tech Inc. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#include "DwEqosSnpDxe.h"

EFI_STATUS
EFIAPI
EqosAipGetInformation (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  IN  EFI_GUID                          *InformationType,
  OUT VOID                              **InformationBlock,
  OUT UINTN                             *InformationBlockSize
  )
{
  EFI_ADAPTER_INFO_MEDIA_STATE  *AdapterInfo;
  SIMPLE_NETWORK_DRIVER         *Snp;

  if ((This == NULL) || (InformationBlock == NULL) ||
      (InformationBlockSize == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (!CompareGuid (InformationType, &gEfiAdapterInfoMediaStateGuid)) {
    return EFI_UNSUPPORTED;
  }

  AdapterInfo = AllocateZeroPool (sizeof (EFI_ADAPTER_INFO_MEDIA_STATE));
  if (AdapterInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *InformationBlock     = AdapterInfo;
  *InformationBlockSize = sizeof (EFI_ADAPTER_INFO_MEDIA_STATE);

  Snp = INSTANCE_FROM_AIP_THIS (This);

  if (Snp->SnpMode.MediaPresent) {
    AdapterInfo->MediaState = EFI_SUCCESS;
  } else {
    AdapterInfo->MediaState = EFI_NOT_READY;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EqosAipSetInformation (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  IN  EFI_GUID                          *InformationType,
  IN  VOID                              *InformationBlock,
  IN  UINTN                             InformationBlockSize
  )
{
  if ((This == NULL) || (InformationBlock == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (CompareGuid (InformationType, &gEfiAdapterInfoMediaStateGuid)) {
    return EFI_WRITE_PROTECTED;
  }

  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
EqosAipGetSupportedTypes (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  OUT EFI_GUID                          **InfoTypesBuffer,
  OUT UINTN                             *InfoTypesBufferCount
  )
{
  EFI_GUID  *Guid;

  if ((This == NULL) || (InfoTypesBuffer == NULL) ||
      (InfoTypesBufferCount == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Guid = AllocatePool (sizeof *Guid);
  if (Guid == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (Guid, &gEfiAdapterInfoMediaStateGuid);

  *InfoTypesBuffer      = Guid;
  *InfoTypesBufferCount = 1;

  return EFI_SUCCESS;
}
