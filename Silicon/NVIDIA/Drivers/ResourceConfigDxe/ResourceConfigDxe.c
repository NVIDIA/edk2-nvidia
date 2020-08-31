/** @file
*  Resource Configuration Dxe
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
#include <NVIDIAConfiguration.h>

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
 * Sets Variable if not currently set
 */
VOID
EFIAPI
SetUnsetVariable (
    IN  CHAR16                       *VariableName,
    IN  EFI_GUID                     *VendorGuid,
    IN  UINT32                       Attributes,
    IN  UINTN                        DataSize,
    IN  VOID                         *Data
    ) {
  EFI_STATUS Status;
  UINTN CurrentSize = 0;
  UINT32 CurrentAttributes;

  Status = gRT->GetVariable (VariableName, VendorGuid, &CurrentAttributes, &CurrentSize, NULL);

  //Delete the variable if the current contents are unexpected.
  if ((Status == EFI_BUFFER_TOO_SMALL) &&
      ((DataSize != CurrentSize) ||
       (Attributes != CurrentAttributes))) {
    gRT->SetVariable (VariableName, VendorGuid, CurrentAttributes, 0, NULL);
    Status = EFI_NOT_FOUND;
  }

  if (Status == EFI_NOT_FOUND) {
    gRT->SetVariable (VariableName, VendorGuid, Attributes, DataSize, Data);
  }
}

/**
  Initializes any variables to current or default settings

**/
VOID
EFIAPI
InitializeSettings (
)
{
  EFI_STATUS               Status;
  VOID                     *AcpiBase;
  NvidiaPcieEnableVariable PcieEnabled = {0};

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    PcdSet8S (PcdPcieEntryInAcpi, PcdGet8 (PcdPcieEntryInAcpi));
  } else {
    PcdSet8S (PcdPcieEntryInAcpi, 0);
  }
  PcdSet8S (PcdQuickBootEnabled, PcdGet8 (PcdQuickBootEnabled));

  SetUnsetVariable (NVIDIA_PCIE_ENABLE_IN_OS_VARIABLE_NAME,
                    &gNVIDIATokenSpaceGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    sizeof (PcieEnabled),
                    (VOID *)&PcieEnabled);
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
