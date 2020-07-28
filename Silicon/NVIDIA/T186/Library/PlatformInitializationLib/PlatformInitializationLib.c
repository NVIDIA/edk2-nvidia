/** @file
  Implementation for PlatformInitializationLib library class interfaces.

  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>

/**

  Constructor for the library.

  @param ImageHandle     The image handle.
  @param SystemTable     The system table.

  @retval  EFI_SUCEESS  Install Boot manager menu success.
  @retval  Other        Return error status.

**/
EFI_STATUS
EFIAPI
T186PlatformInitializationLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  UINTN ChipID;
  VOID  *SystemFmpCapsuleImageTypeIdGuid;
  UINTN GuidSize;

  ChipID = TegraGetChipID();

  if (ChipID == T186_CHIP_ID) {
    // Used in GICv2
    PcdSet64S(PcdGicInterruptInterfaceBase, TegraGetGicInterruptInterfaceBaseAddress(ChipID));

    // Set Default OEM Table ID specific PCDs
    PcdSet64S(PcdAcpiDefaultOemTableId, SIGNATURE_64 ('T','E','G','R','A','1','8','6'));

    // Set Boot Image Signing Header Size PCD
    PcdSet32S(PcdBootImgSigningHeaderSize, 0x190);

    // Set SDHCi SDR104 Disable PCD
    PcdSetBoolS(PcdSdhciSDR104Disable, TRUE);

    // Set CvmEeprom Bus Base
    PcdSet64S (PcdTegraCvmEepromBusBase, FixedPcdGet64 (PcdTegraCvmEepromBusT186Base));

    SystemFmpCapsuleImageTypeIdGuid = PcdGetPtr (PcdSystemFmpCapsuleImageTypeIdGuidT186);
    GuidSize = sizeof (EFI_GUID);
    PcdSetPtrS (PcdSystemFmpCapsuleImageTypeIdGuid, &GuidSize, SystemFmpCapsuleImageTypeIdGuid);
  }

  return EFI_SUCCESS;
}

/**
  Destructor for the library.

  @param[in]  ImageHandle       Handle that identifies the image to be unloaded.
  @param[in]  SystemTable       System Table

  @retval EFI_SUCCESS           The image has been unloaded.
**/
EFI_STATUS
EFIAPI
T186PlatformInitializationLibDestructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  return EFI_SUCCESS;
}
