/** @file
*  Resource Configuration Dxe
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Guid/MdeModuleHii.h>
#include <Guid/HiiPlatformSetupFormset.h>

#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigRouting.h>

#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/HiiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiLib.h>

#include "ResourceConfigHii.h"

extern EFI_GUID gNVIDIAResourceConfigFormsetGuid;

//
// These are the VFR compiler generated data representing our VFR data.
//
extern UINT8 ResourceConfigHiiBin[];
extern UINT8 ResourceConfigDxeStrings[];

//
// HII specific Vendor Device Path definition.
//
typedef struct {
  VENDOR_DEVICE_PATH                VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL          End;
} HII_VENDOR_DEVICE_PATH;


HII_VENDOR_DEVICE_PATH mResourceConfigHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8) (sizeof (VENDOR_DEVICE_PATH)),
        (UINT8) ((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    RESOURCE_CONFIG_FORMSET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8) (END_DEVICE_PATH_LENGTH),
      (UINT8) ((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

/**
  Initializes any variables to current or default settings

**/
VOID
EFIAPI
InitializeSettings (
)
{
  EFI_STATUS                Status;
  VOID                      *AcpiBase;

  // Initialize PCIe Form Settings
  PcdSet8S (PcdPcieResourceConfigNeeded, PcdGet8 (PcdPcieResourceConfigNeeded));
  PcdSet8S (PcdPcieEntryInAcpiConfigNeeded, PcdGet8 (PcdPcieEntryInAcpiConfigNeeded));
  PcdSet8S (PcdPcieEntryInAcpi, PcdGet8 (PcdPcieEntryInAcpi));
  if (PcdGet8 (PcdPcieResourceConfigNeeded) == 1) {
    Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
    if (EFI_ERROR (Status)) {
      PcdSet8S (PcdPcieResourceConfigNeeded, 0);
      PcdSet8S (PcdPcieEntryInAcpiConfigNeeded, 0);
    }
  }

  // Initialize UART Form Settings
  PcdSet8S (PcdSerialTypeConfig, PcdGet8 (PcdSerialTypeConfig));
  PcdSet8S (PcdSerialPortConfig, PcdGet8 (PcdSerialPortConfig));

  // Initialize Quick Boot Form Settings
  PcdSet8S (PcdQuickBootEnabled, PcdGet8 (PcdQuickBootEnabled));
}

VOID
EFIAPI
OnEndOfDxe (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                      Status;
  EFI_HII_HANDLE                  HiiHandle;
  EFI_HANDLE                      DriverHandle;

  gBS->CloseEvent (Event);

  InitializeSettings ();

  DriverHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (&DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mResourceConfigHiiVendorDevicePath,
                  NULL);
  if (!EFI_ERROR (Status)) {
    HiiHandle = HiiAddPackages (&gNVIDIAResourceConfigFormsetGuid,
                                DriverHandle,
                                ResourceConfigDxeStrings,
                                ResourceConfigHiiBin,
                                NULL
                                );

    if (HiiHandle == NULL) {
      gBS->UninstallMultipleProtocolInterfaces (DriverHandle,
                    &gEfiDevicePathProtocolGuid,
                    &mResourceConfigHiiVendorDevicePath,
                    NULL);
    }
  }
}

/**
  Install Resource Config driver.

  @param  ImageHandle     The image handle.
  @param  SystemTable     The system table.

  @retval EFI_SUCEESS     Install Boot manager menu success.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
ResourceConfigDxeInitialize (
  IN EFI_HANDLE               ImageHandle,
  IN EFI_SYSTEM_TABLE         *SystemTable
)
{
  EFI_STATUS                      Status;
  EFI_EVENT                       EndOfDxeEvent;

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_CALLBACK,
                               OnEndOfDxe,
                               NULL,
                               &gEfiEndOfDxeEventGroupGuid,
                               &EndOfDxeEvent);

  return Status;
}
