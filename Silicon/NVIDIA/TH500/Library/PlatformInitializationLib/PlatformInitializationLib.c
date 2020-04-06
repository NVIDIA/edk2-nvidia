/** @file
  Implementation for PlatformInitializationLib library class interfaces.

  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <TH500/TH500Definitions.h>

/**

  Constructor for the library.

  @param ImageHandle     The image handle.
  @param SystemTable     The system table.

  @retval  EFI_SUCEESS  Install Boot manager menu success.
  @retval  Other        Return error status.

**/
EFI_STATUS
EFIAPI
TH500PlatformInitializationLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  UINTN ChipID;

  ChipID = TegraGetChipID();

  if (ChipID == TH500_CHIP_ID) {
    // Used in GICv3
    PcdSet64S(PcdGicRedistributorsBase, TegraGetGicRedistributorBaseAddress(ChipID));

    // Set PCI specific PCDs
    PcdSet64S(PcdPciConfigurationSpaceBaseAddress, TH500_PCIE_CFG_BASE_ADDR);
    PcdSet32S(PcdPciBusMin, TH500_PCIE_BUS_MIN);
    PcdSet32S(PcdPciBusMax, TH500_PCIE_BUS_MAX);

    // Set Default OEM Table ID specific PCDs
    PcdSet64S(PcdAcpiDefaultOemTableId, 0x2020203030354854);

    // Set BPMP PCIe Controller Enable PCD
    PcdSetBoolS(PcdBPMPPCIeControllerEnable, TRUE);
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
TH500PlatformInitializationLibDestructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  return EFI_SUCCESS;
}
