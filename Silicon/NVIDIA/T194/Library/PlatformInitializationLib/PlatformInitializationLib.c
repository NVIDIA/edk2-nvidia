/** @file
  Implementation for PlatformInitializationLib library class interfaces.

  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/PcdLib.h>
#include <Library/HobLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <T194/T194Definitions.h>
#include <libfdt.h>

BOOLEAN
EFIAPI
T194CheckOSACPIBoot (
  VOID
  )
{
  VOID        *Hob;
  VOID        *Dtb;
  INT32       NodeOffset;

  Hob = NULL;

  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if (Hob == NULL || GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64)) {
    return TRUE;
  }
  Dtb = (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);

  if (fdt_check_header (Dtb) != 0) {
    return TRUE;
  }

  NodeOffset = fdt_path_offset (Dtb, "/chosen");
  if (NodeOffset < 0) {
    return TRUE;
  }

  if (NULL != fdt_get_property (Dtb, NodeOffset, "os-default-to-acpi", NULL)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**

  Constructor for the library.

  @param ImageHandle     The image handle.
  @param SystemTable     The system table.

  @retval  EFI_SUCEESS  Install Boot manager menu success.
  @retval  Other        Return error status.

**/
EFI_STATUS
EFIAPI
T194PlatformInitializationLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  UINTN ChipID;
  VOID  *SystemFmpCapsuleImageTypeIdGuid;
  UINTN GuidSize;

  ChipID = TegraGetChipID();

  if (ChipID == T194_CHIP_ID) {
    // Used in GICv2
    PcdSet64S(PcdGicInterruptInterfaceBase, TegraGetGicInterruptInterfaceBaseAddress(ChipID));

    // Set Default OEM Table ID specific PCDs
    PcdSet64S(PcdAcpiDefaultOemTableId, SIGNATURE_64 ('T','E','G','R','A','1','9','4'));

    // Set Tegra PWM Fan Base
    PcdSet64S(PcdTegraPwmFanBase, FixedPcdGet64 (PcdTegraPwmFanT194Base));

    // Set Boot Image Signing Header Size PCD
    PcdSet32S(PcdBootImgSigningHeaderSize, 0x1000);

    // Set SDHCi Coherent DMA Disable PCD
    PcdSetBoolS(PcdSdhciCoherentDMADisable, TRUE);

    // Set BPMP PCIe Controller Enable PCD
    PcdSetBoolS(PcdBPMPPCIeControllerEnable, TRUE);

    // Set Floor Sweep CPUs PCD
    PcdSetBoolS(PcdFloorsweepCpus, TRUE);

    // Set Default DT Use PCD
    if (T194CheckOSACPIBoot ()) {
      PcdSetBoolS(PcdDefaultDtPref, FALSE);
    }

    // Set CvmEeprom Bus Base
    PcdSet64S (PcdTegraCvmEepromBusBase, FixedPcdGet64 (PcdTegraCvmEepromBusT194Base));

    SystemFmpCapsuleImageTypeIdGuid = PcdGetPtr (PcdSystemFmpCapsuleImageTypeIdGuidT194);
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
T194PlatformInitializationLibDestructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  return EFI_SUCCESS;
}
