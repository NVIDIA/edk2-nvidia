/** @file
  Configuration Manager Data of SMBIOS Type 45 table

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <ConfigurationManagerObject.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciIo.h>
#include <Library/PrintLib.h>
#include <Protocol/FirmwareManagement.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Protocol/Tcg2Protocol.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FmpDeviceLib.h>
#include "ConfigurationSmbiosPrivate.h"

#define BIT_IS_SET(Data, Bit)  ((BOOLEAN)(((Data) & (Bit)) == (Bit)))

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType45 = {
  SMBIOS_TYPE_FIRMWARE_INVENTORY_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType45),
  NULL
};

/**
  Private FMP handle information set for configuration manager SMBIOS type 45

**/
typedef struct {
  EFI_FIRMWARE_IMAGE_DESCRIPTOR    *ImageInfoHead;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR    *ImageInfo;
  UINT32                           DescriptorVersion;
  UINT32                           PackageVersion;
  CHAR16                           *PackageVersionName;
  CHAR16                           *DevicePathString;
} FMP_HANDLE_INFO_SET;

/**
  Private PciIo handle information set for configuration manager SMBIOS type 45

**/
typedef struct {
  UINTN     Segment;
  UINTN     Bus;
  UINTN     Device;
  UINTN     Function;
  CHAR16    *DevicePathString;
} PCIIO_HANDLE_INFO_SET;

/**
  Release allocated resource for private FMP handle information set.

  @param[in ]               NumHandles
  @param[in ]               PrivateInfoSet

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
ReleaseFmpInfoSet (
  IN UINTN                NumHandles,
  IN FMP_HANDLE_INFO_SET  **PrivateInfoSet
  )
{
  UINTN                HandleIndex;
  FMP_HANDLE_INFO_SET  *PrivateInfoSetPtr;

  if (*PrivateInfoSet != NULL) {
    for (HandleIndex = 0; HandleIndex < NumHandles; HandleIndex++) {
      PrivateInfoSetPtr = (*PrivateInfoSet) + HandleIndex;
      if (PrivateInfoSetPtr->ImageInfoHead != NULL) {
        FreePool (PrivateInfoSetPtr->ImageInfoHead);
      }

      if (PrivateInfoSetPtr->PackageVersionName != NULL) {
        FreePool (PrivateInfoSetPtr->PackageVersionName);
      }

      if (PrivateInfoSetPtr->DevicePathString != NULL) {
        FreePool (PrivateInfoSetPtr->DevicePathString);
      }
    }

    FreePool (*PrivateInfoSet);
  }

  return EFI_SUCCESS;
}

/**
  Create and return resource for private FMP handle information set.

  @param[out]               NumHandles
  @param[out]               PrivateInfoSet

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
GetFmpInfoSet (
  UINT32               *NumHandles,
  FMP_HANDLE_INFO_SET  **PrivateInfoSet
  )
{
  EFI_STATUS                        Status;
  EFI_HANDLE                        *HandleBuffer;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL  *Fmp;
  UINTN                             HandleIndex;
  UINTN                             NumFmpHandles;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR     *ImageInfo;
  UINTN                             ImageInfoSize;
  UINTN                             DescriptorSize;
  UINT32                            DescriptorVersion;
  UINT32                            PackageVersion;
  CHAR16                            *PackageVersionName;
  UINT8                             DescriptorCount;
  UINTN                             DescriptorIndex;
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
  FMP_HANDLE_INFO_SET               *PrivateInfoSetPtr;

  NumFmpHandles     = 0;
  PrivateInfoSetPtr = NULL;
  HandleBuffer      = NULL;

  //
  // Locate Firmware Management Protocol and get all its handles
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiFirmwareManagementProtocolGuid,
                  NULL,
                  &NumFmpHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Cannot locate Firmware management Protocol handle buffer. Status = %r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  *PrivateInfoSet = AllocateZeroPool (sizeof (*PrivateInfoSet) * MAX_FIRMWARE_INVENTORY_FMP_DESC_COUNT);
  if (*PrivateInfoSet == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
  } else {
    //
    // For each handle, get the PciIo info and add it to the private FmpInfoSet.
    //
    for (HandleIndex = 0; HandleIndex < NumFmpHandles; HandleIndex++) {
      ImageInfo          = NULL;
      ImageInfoSize      = 0;
      Fmp                = NULL;
      DevicePath         = NULL;
      PackageVersionName = NULL;

      Status = gBS->HandleProtocol (
                      HandleBuffer[HandleIndex],
                      &gEfiFirmwareManagementProtocolGuid,
                      (VOID **)&Fmp
                      );
      if (EFI_ERROR (Status)) {
        continue;
      }

      DevicePath = DevicePathFromHandle (HandleBuffer[HandleIndex]);

      //
      // First call to get buffer size to allocate memory for ImageInfo.
      //
      Status = Fmp->GetImageInfo (
                      Fmp,
                      &ImageInfoSize,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL
                      );
      if (Status != EFI_BUFFER_TOO_SMALL) {
        continue;
      }

      ImageInfo = (EFI_FIRMWARE_IMAGE_DESCRIPTOR *)AllocateZeroPool (ImageInfoSize);
      if (ImageInfo == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }

      //
      // Get current image info in the device.
      //
      Status = Fmp->GetImageInfo (
                      Fmp,
                      &ImageInfoSize,
                      ImageInfo,
                      &DescriptorVersion,
                      &DescriptorCount,
                      &DescriptorSize,
                      &PackageVersion,
                      &PackageVersionName
                      );
      if (EFI_ERROR (Status)) {
        FreePool (ImageInfo);
        continue;
      }

      for (DescriptorIndex = 0; DescriptorIndex < DescriptorCount; DescriptorIndex++) {
        if (*NumHandles < MAX_FIRMWARE_INVENTORY_FMP_DESC_COUNT) {
          PrivateInfoSetPtr = (*PrivateInfoSet) + *NumHandles;
          if (DescriptorIndex == 0) {
            PrivateInfoSetPtr->ImageInfoHead = ImageInfo;
          } else {
            PrivateInfoSetPtr->ImageInfoHead = NULL;
          }

          PrivateInfoSetPtr->ImageInfo          = &ImageInfo[DescriptorIndex];
          PrivateInfoSetPtr->DescriptorVersion  = DescriptorVersion;
          PrivateInfoSetPtr->PackageVersion     = PackageVersion;
          PrivateInfoSetPtr->PackageVersionName = PackageVersionName;

          if (DevicePath != NULL) {
            PrivateInfoSetPtr->DevicePathString = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
          }

          (*NumHandles)++;
        } else {
          Status = EFI_BUFFER_TOO_SMALL;
        }
      }
    }
  }

  FreePool (HandleBuffer);
  return Status;
}

/**
  Release allocated resource for private PciIo handle information set.

  @param[in]               NumHandles
  @param[in]               PrivateInfoSet

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
ReleasePciIoInfoSet (
  IN UINTN                  NumHandles,
  IN PCIIO_HANDLE_INFO_SET  **PrivateInfoSet
  )
{
  UINTN                  HandleIndex;
  PCIIO_HANDLE_INFO_SET  *PrivateInfoSetPtr;

  if (*PrivateInfoSet != NULL) {
    for (HandleIndex = 0; HandleIndex < NumHandles; HandleIndex++) {
      PrivateInfoSetPtr = (*PrivateInfoSet) + HandleIndex;

      if (PrivateInfoSetPtr->DevicePathString != NULL) {
        FreePool (PrivateInfoSetPtr->DevicePathString);
      }
    }

    FreePool (*PrivateInfoSet);
  }

  return EFI_SUCCESS;
}

/**
  Create and return resource for private PciIo handle information set.

  @param[out]               NumHandles
  @param[out]               PrivateInfoSet

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
GetPciIoInfoSet (
  OUT UINT32                 *NumHandles,
  OUT PCIIO_HANDLE_INFO_SET  **PrivateInfoSet
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                *HandleBuffer;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  UINTN                     HandleIndex;
  UINTN                     NumPciIoHandles;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  UINTN                     Segment;
  UINTN                     Bus;
  UINTN                     Device;
  UINTN                     Function;
  PCIIO_HANDLE_INFO_SET     *PrivateInfoSetPtr;

  NumPciIoHandles   = 0;
  PrivateInfoSetPtr = NULL;

  //
  // Locate PciIo Protocol and get all its handles
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &NumPciIoHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Cannot locate PciIo Protocol handle buffer. Status = %r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  *PrivateInfoSet = AllocateZeroPool (sizeof (PCIIO_HANDLE_INFO_SET) * (MAX_FIRMWARE_INVENTORY_PCIIO_COUNT));
  if (*PrivateInfoSet == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
  } else {
    //
    // For each handle, get the PciIo info and add it to the private PciIoInfoSet.
    //
    for (HandleIndex = 0; HandleIndex < NumPciIoHandles; HandleIndex++) {
      PciIo      = NULL;
      DevicePath = NULL;

      Status = gBS->HandleProtocol (
                      HandleBuffer[HandleIndex],
                      &gEfiPciIoProtocolGuid,
                      (VOID **)&PciIo
                      );
      if (EFI_ERROR (Status)) {
        continue;
      }

      DevicePath = DevicePathFromHandle (HandleBuffer[HandleIndex]);
      Status     = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
      if (!EFI_ERROR (Status)) {
        if (*NumHandles < MAX_FIRMWARE_INVENTORY_PCIIO_COUNT) {
          PrivateInfoSetPtr = (*PrivateInfoSet) + *NumHandles;

          PrivateInfoSetPtr->Segment  = Segment;
          PrivateInfoSetPtr->Bus      = Bus;
          PrivateInfoSetPtr->Device   = Device;
          PrivateInfoSetPtr->Function = Function;
          if (DevicePath != NULL) {
            PrivateInfoSetPtr->DevicePathString = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
          }

          (*NumHandles)++;
        } else {
          Status = EFI_BUFFER_TOO_SMALL;
        }
      }
    }
  }

  FreePool (HandleBuffer);
  return Status;
}

/**
  Appending FMP firmware inventory info elements.

  @param[in]                BiosInfo
  @param[in]                SystemSlotInfo
  @param[in]                NumSystemSlots
  @param[in, out]           FirmwareInventoryInfo
  @param[in, out]           NumFirmwareComponents

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
FmpFirmwareInventoryUpdate (
  CM_SMBIOS_BIOS_INFO                *BiosInfo,
  CM_SMBIOS_SYSTEM_SLOTS_INFO        *SystemSlotInfo,
  UINT32                             NumSystemSlots,
  CM_SMBIOS_FIRMWARE_INVENTORY_INFO  **FirmwareInventoryInfo,
  UINT32                             *NumFirmwareComponents
  )
{
  EFI_STATUS                         Status;
  UINT32                             NumFmpHandles;
  UINTN                              HandleIndex;
  UINT32                             DescriptorVersion;
  CM_SMBIOS_FIRMWARE_INVENTORY_INFO  *FirmwareInventoryInfoElement;
  CM_SMBIOS_FIRMWARE_INVENTORY_INFO  *NewFirmwareInventoryInfo;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR      *ImageInfo;
  UINT8                              StrLength;
  CHAR8                              SbiosFirmwareComponentName[] = "System ROM";
  EFI_GUID                           *SbiosDeviceGuid;
  FMP_HANDLE_INFO_SET                *FmpHandleInfoSet;
  CHAR8                              *VendorAsciiStr;
  CHAR8                              *ReleaseDateAsciiStr;
  UINTN                              SystemSlotInfoIndex;
  UINTN                              Segment;
  UINTN                              Bus;
  UINTN                              Device;
  UINTN                              Function;
  CM_OBJECT_TOKEN                    *AssociatedComponentBuffer;
  UINTN                              AssociatedComponentCount;
  CHAR8                              *LowestSupportedVersion;
  UINT32                             NumPciIoHandles;
  INT32                              PciIoHandleIndex;
  PCIIO_HANDLE_INFO_SET              *PciIoHandleInfoSet;

  NumFmpHandles       = 0;
  SbiosDeviceGuid     = NULL;
  FmpHandleInfoSet    = NULL;
  VendorAsciiStr      = NULL;
  ReleaseDateAsciiStr = NULL;
  NumPciIoHandles     = 0;
  PciIoHandleIndex    = 0;
  PciIoHandleInfoSet  = NULL;

  Status = GetFmpInfoSet (&NumFmpHandles, &FmpHandleInfoSet);
  if ((FmpHandleInfoSet == NULL) || EFI_ERROR (Status)) {
    Status =  EFI_DEVICE_ERROR;
    goto Exit;
  }

  Status = GetPciIoInfoSet (&NumPciIoHandles, &PciIoHandleInfoSet);
  if ((PciIoHandleInfoSet == NULL) || EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  Status = FmpDeviceGetImageTypeIdGuidPtr (&SbiosDeviceGuid);
  if ((SbiosDeviceGuid == NULL) || EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  //
  // For each handle, get the FW info and add it to the SMBIOS table.
  //
  for (HandleIndex = 0; HandleIndex < NumFmpHandles; HandleIndex++) {
    ImageInfo                = FmpHandleInfoSet[HandleIndex].ImageInfo;
    DescriptorVersion        = FmpHandleInfoSet[HandleIndex].DescriptorVersion;
    NewFirmwareInventoryInfo = NULL;

    NewFirmwareInventoryInfo = ReallocatePool (
                                 ((*NumFirmwareComponents)) * (sizeof (CM_SMBIOS_FIRMWARE_INVENTORY_INFO)),
                                 ((*NumFirmwareComponents) + 1) * (sizeof (CM_SMBIOS_FIRMWARE_INVENTORY_INFO)),
                                 *FirmwareInventoryInfo
                                 );
    if (NewFirmwareInventoryInfo == NULL) {
      Status =  EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    *FirmwareInventoryInfo = NewFirmwareInventoryInfo;

    FirmwareInventoryInfoElement = (*FirmwareInventoryInfo) + *NumFirmwareComponents;
    ZeroMem (FirmwareInventoryInfoElement, sizeof (*FirmwareInventoryInfoElement));

    if (CompareGuid (&ImageInfo->ImageTypeId, SbiosDeviceGuid)) {
      //
      // SBIOS specific firmware handle for below fields.
      //   a1. Component name
      //   a2. Release date
      //   a3. Manufacturer
      //   a4. Image size
      //   a5. FirmwareVersionFormat/FirmwareVersion
      //
      FirmwareInventoryInfoElement->FirmwareComponentName =  (CHAR8 *)AllocateZeroPool (sizeof (SbiosFirmwareComponentName) + 1);
      if (FirmwareInventoryInfoElement->FirmwareComponentName != NULL) {
        CopyMem (FirmwareInventoryInfoElement->FirmwareComponentName, SbiosFirmwareComponentName, sizeof (SbiosFirmwareComponentName));
      }

      if (BiosInfo != NULL) {
        //
        // Update Firmware release date.
        //
        FirmwareInventoryInfoElement->ReleaseDate = AllocateCopyString (BiosInfo->BiosReleaseDate);

        //
        // Update manufacturer or producer
        //
        FirmwareInventoryInfoElement->Manufacturer = AllocateCopyString (BiosInfo->BiosVendor);

        //
        // Update Firmware image size.
        //
        if (BiosInfo->ExtendedBiosSize.Unit == 0) {
          // Size is in MB
          FirmwareInventoryInfoElement->ImageSize = BiosInfo->ExtendedBiosSize.Size * SIZE_1MB;
        } else {
          // Size is in GB
          FirmwareInventoryInfoElement->ImageSize = BiosInfo->ExtendedBiosSize.Size * SIZE_1GB;
        }

        //
        // Update Firmware image version.
        //
        FirmwareInventoryInfoElement->FirmwareVersionFormat = VersionFormatTypeFreeForm;
        FirmwareInventoryInfoElement->Manufacturer          =  AllocateCopyString (BiosInfo->BiosVersion);
      }
    } else {
      //
      // General firmware handle for below fields.
      //   b1. Component name
      //   b2. Release date
      //   b3. Manufacturer
      //   b4. Image size
      //   b5. FirmwareVersionFormat/LowestSupportedVersion/FirmwareVersion
      //   b6. Associated component information
      //

      //
      // There is no firmware component name in UEFI FMP image descriptor. Using ImageIdName
      // for firmware component name of firmware inventory reporting.
      //
      StrLength = StrLen (ImageInfo->ImageIdName);
      if (StrLength > 0) {
        FirmwareInventoryInfoElement->FirmwareComponentName =  (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (StrLength + 1));
        if (FirmwareInventoryInfoElement->FirmwareComponentName != NULL) {
          UnicodeStrToAsciiStrS (ImageInfo->ImageIdName, FirmwareInventoryInfoElement->FirmwareComponentName, StrLength + 1);
        }
      }

      //
      // Update Firmware release date and manufacturer.
      // There is no such info in UEFI FMP, so leave them as NULL.
      //
      FirmwareInventoryInfoElement->ReleaseDate  = NULL;
      FirmwareInventoryInfoElement->Manufacturer = NULL;

      //
      // Update Firmware image size.
      //
      FirmwareInventoryInfoElement->ImageSize = ImageInfo->Size;

      //
      // Update firmware version, lowest supported image version and version format.
      //
      if (DescriptorVersion >= 2) {
        //
        // If LowestSupportedImageVersion is valid in FMP image descriptor, FirmwareVersion in FirmwareInventoryInfo
        // will use Version field, instead of VersionName. FirmwareVersionFormat turns to VersionFormatType32BitHex.
        //  EXAMPLE: "0x0001002d"
        //
        StrLength              = ((sizeof (UINT32) * 2) + 2 + 1);
        LowestSupportedVersion = (CHAR8 *)AllocateZeroPool (StrLength);
        AsciiSPrint (LowestSupportedVersion, StrLength, "0x%08X", ImageInfo->LowestSupportedImageVersion);
        FirmwareInventoryInfoElement->LowestSupportedVersion = LowestSupportedVersion;
      }

      //
      // FirmwareVersionFormat turns to VersionFormatTypeFreeForm.
      //
      FirmwareInventoryInfoElement->FirmwareVersionFormat = VersionFormatTypeFreeForm;
      StrLength                                           = StrLen (ImageInfo->VersionName);
      if (StrLength > 0) {
        FirmwareInventoryInfoElement->FirmwareVersion =  (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (StrLength + 1));
        if (FirmwareInventoryInfoElement->FirmwareVersion != NULL) {
          UnicodeStrToAsciiStrS (ImageInfo->VersionName, FirmwareInventoryInfoElement->FirmwareVersion, StrLength + 1);
        }
      }

      //
      // Update associated component information.
      //
      Segment  = 0;
      Bus      = 0;
      Device   = 0;
      Function = 0;
      Status   = EFI_NOT_FOUND;
      if (FmpHandleInfoSet[HandleIndex].DevicePathString != NULL) {
        for (PciIoHandleIndex = NumPciIoHandles - 1; PciIoHandleIndex >= 0; PciIoHandleIndex--) {
          if (PciIoHandleInfoSet[PciIoHandleIndex].DevicePathString != NULL) {
            if (StrnCmp (
                  PciIoHandleInfoSet[PciIoHandleIndex].DevicePathString,
                  FmpHandleInfoSet[HandleIndex].DevicePathString,
                  StrLen (PciIoHandleInfoSet[PciIoHandleIndex].DevicePathString)
                  ) == 0)
            {
              Segment  = PciIoHandleInfoSet[PciIoHandleIndex].Segment;
              Bus      = PciIoHandleInfoSet[PciIoHandleIndex].Bus;
              Device   = PciIoHandleInfoSet[PciIoHandleIndex].Device;
              Function = PciIoHandleInfoSet[PciIoHandleIndex].Function;
              Status   = EFI_SUCCESS;
              break;
            }
          }
        }
      }

      AssociatedComponentCount  = 0;
      AssociatedComponentBuffer = NULL;
      if (Status == EFI_SUCCESS) {
        for (SystemSlotInfoIndex = 0; SystemSlotInfoIndex < NumSystemSlots; SystemSlotInfoIndex++) {
          if ((SystemSlotInfo[SystemSlotInfoIndex].SegmentGroupNum == Segment) &&
              (SystemSlotInfo[SystemSlotInfoIndex].BusNum == Bus) &&
              (((SystemSlotInfo[SystemSlotInfoIndex].DevFuncNum >> 3) & 0x1) == Device) &&
              ((SystemSlotInfo[SystemSlotInfoIndex].DevFuncNum & 0x07) == Function)
              )
          {
            AssociatedComponentBuffer = AllocateZeroPool (sizeof (*AssociatedComponentBuffer));

            if (AssociatedComponentBuffer == NULL) {
              DEBUG ((DEBUG_ERROR, "%a: Failed to allocate associated component buffer\n", __FUNCTION__));
              break;
            }

            AssociatedComponentBuffer[0] = SystemSlotInfo[SystemSlotInfoIndex].SystemSlotInfoToken;
            AssociatedComponentCount     = 1;
            break;
          }
        }
      }

      FirmwareInventoryInfoElement->AssociatedComponentCount   = AssociatedComponentCount;
      FirmwareInventoryInfoElement->AssociatedComponentHandles = AssociatedComponentBuffer;
    }

    //
    // Common firmware handle for below fields.
    //   c1. Firmware ID and ID format.
    //   c2. Firmware Characteristics
    //   c3. Firmware State.
    //   c4. associated component information
    //

    //
    // Update Firmware ID and ID format.
    //
    StrLength                                      = StrLen (ImageInfo->ImageIdName);
    FirmwareInventoryInfoElement->FirmwareIdFormat = FirmwareIdFormatTypeFreeForm;
    if (StrLength > 0) {
      FirmwareInventoryInfoElement->FirmwareId =  (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (StrLength + 1));
      if (FirmwareInventoryInfoElement->FirmwareId != NULL) {
        UnicodeStrToAsciiStrS (ImageInfo->ImageIdName, FirmwareInventoryInfoElement->FirmwareId, StrLength + 1);
      }
    }

    //
    // Update Firmware Characteristics.
    //
    if (BIT_IS_SET (ImageInfo->AttributesSupported, IMAGE_ATTRIBUTE_IMAGE_UPDATABLE)) {
      FirmwareInventoryInfoElement->Characteristics.Updatable = 1;
      if (!BIT_IS_SET (ImageInfo->AttributesSetting, IMAGE_ATTRIBUTE_IMAGE_UPDATABLE)) {
        FirmwareInventoryInfoElement->Characteristics.WriteProtected = 1;
      }
    }

    //
    // Update Firmware State.
    //
    if ((ImageInfo->AttributesSetting & IMAGE_ATTRIBUTE_IN_USE) == 0 ) {
      FirmwareInventoryInfoElement->State = FirmwareInventoryStateDisabled;
    } else {
      FirmwareInventoryInfoElement->State = FirmwareInventoryStateEnabled;
    }

    (*NumFirmwareComponents)++;
  }

Exit:
  ReleaseFmpInfoSet (NumFmpHandles, &FmpHandleInfoSet);
  ReleasePciIoInfoSet (NumPciIoHandles, &PciIoHandleInfoSet);

  return Status;
}

/**
  Appending Tpm firmware inventory info elements.

  @param[in]                TpmInfo
  @param[in, out]           FirmwareInventoryInfo
  @param[in, out]           NumFirmwareComponents

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
TpmFirmwareInventoryUpdate (
  CM_SMBIOS_TPM_DEVICE_INFO          *TpmInfo,
  CM_SMBIOS_FIRMWARE_INVENTORY_INFO  **FirmwareInventoryInfo,
  UINT32                             *NumFirmwareComponents
  )
{
  CM_SMBIOS_FIRMWARE_INVENTORY_INFO  *FirmwareInventoryInfoElement;
  CM_SMBIOS_FIRMWARE_INVENTORY_INFO  *NewFirmwareInventoryInfo;
  EFI_TCG2_PROTOCOL                  *Tcg2Protocol;
  EFI_TCG2_BOOT_SERVICE_CAPABILITY   ProtocolCapability;
  CHAR8                              TpmFirmwareComponentName[] = "TPM Firmware";

  if (TpmInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Update associated component information.
  //
  NewFirmwareInventoryInfo = NULL;
  NewFirmwareInventoryInfo = AllocateCopyPool (
                               ((*NumFirmwareComponents) + 1) *
                               (sizeof (CM_SMBIOS_FIRMWARE_INVENTORY_INFO)),
                               *FirmwareInventoryInfo
                               );
  if (NewFirmwareInventoryInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *FirmwareInventoryInfo = NewFirmwareInventoryInfo;

  FirmwareInventoryInfoElement = (*FirmwareInventoryInfo) + *NumFirmwareComponents;
  ZeroMem (FirmwareInventoryInfoElement, sizeof (*FirmwareInventoryInfoElement));

  FirmwareInventoryInfoElement->FirmwareComponentName =  (CHAR8 *)AllocateZeroPool (sizeof (TpmFirmwareComponentName) + 1);
  if (FirmwareInventoryInfoElement->FirmwareComponentName != NULL) {
    CopyMem (FirmwareInventoryInfoElement->FirmwareComponentName, TpmFirmwareComponentName, sizeof (TpmFirmwareComponentName));
  }

  //
  // If LowestSupportedImageVersion is invalid in image descriptor, FirmwareVersion in FirmwareInventoryInfo
  // will use VersionName field. FirmwareVersionFormat turns to VersionFormatTypeFreeForm.
  //
  FirmwareInventoryInfoElement->FirmwareVersionFormat = VersionFormatTypeFreeForm;

  //
  // Update Firmware ID and ID format.
  //
  FirmwareInventoryInfoElement->FirmwareIdFormat = FirmwareIdFormatTypeFreeForm;
  FirmwareInventoryInfoElement->FirmwareId       =  AllocateCopyString (TpmFirmwareComponentName);

  //
  // FirmwareVersionFormat turns to VersionFormatTypeMajorMinor.
  // FirmwareVersion1 : The upper 16 bits of this field SHALL contain the TPM
  // major firmware version (Version Major). The lower 16 bits of this field SHALL
  // contain the TPM minor firmware version (Version Minor).
  // FirmwareVersion2 : This is an OEM extension, and there is not standard format.
  //
  FirmwareInventoryInfoElement->FirmwareVersionFormat = VersionFormatTypeMajorMinor;
  FirmwareInventoryInfoElement->FirmwareVersion       =  (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (MAX_TPM_VERSION_LEN + 1));
  if (FirmwareInventoryInfoElement->FirmwareVersion != NULL) {
    AsciiSPrint (
      FirmwareInventoryInfoElement->FirmwareVersion,
      MAX_TPM_VERSION_LEN,
      "%u.%u",
      TpmInfo->FirmwareVersion1 >> 16,
      TpmInfo->FirmwareVersion1 & 0xFFFF
      );
  }

  //
  // Update Firmware release date and manufacturer.
  // There is no such info in UEFI FMP, so leave them as NULL.
  //
  FirmwareInventoryInfoElement->ReleaseDate = NULL;

  FirmwareInventoryInfoElement->Characteristics.Updatable      = 1;
  FirmwareInventoryInfoElement->Characteristics.WriteProtected = 1;

  //
  // Update Firmware State.
  //
  FirmwareInventoryInfoElement->State = FirmwareInventoryStateDisabled;

  if (!EFI_ERROR (gBS->LocateProtocol (&gEfiTcg2ProtocolGuid, NULL, (VOID **)&Tcg2Protocol))) {
    ProtocolCapability.Size = sizeof (ProtocolCapability);

    if (!EFI_ERROR (Tcg2Protocol->GetCapability (Tcg2Protocol, &ProtocolCapability))) {
      if (ProtocolCapability.TPMPresentFlag) {
        FirmwareInventoryInfoElement->State = FirmwareInventoryStateEnabled;
      }
    }
  }

  FirmwareInventoryInfoElement->Manufacturer =  (CHAR8 *)AllocateZeroPool (sizeof (TpmInfo->VendorID) + 1);
  if (FirmwareInventoryInfoElement->Manufacturer != NULL) {
    CopyMem (FirmwareInventoryInfoElement->Manufacturer, &TpmInfo->VendorID, sizeof (TpmInfo->VendorID));
  }

  FirmwareInventoryInfoElement->AssociatedComponentCount   = 1;
  FirmwareInventoryInfoElement->AssociatedComponentHandles =
    AllocateZeroPool (
      sizeof (CM_OBJECT_TOKEN) * FirmwareInventoryInfoElement->AssociatedComponentCount
      );
  if (FirmwareInventoryInfoElement->AssociatedComponentHandles == NULL) {
    FirmwareInventoryInfoElement->AssociatedComponentCount = 0;
  } else {
    FirmwareInventoryInfoElement->AssociatedComponentHandles[0] = TpmInfo->TpmDeviceInfoToken;
  }

  (*NumFirmwareComponents)++;

  return EFI_SUCCESS;
}

/**
  Install CM object for SMBIOS Type 45

  @param[in, out] Private   Pointer to the private data of SMBIOS creators,

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType45Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO     *Repo;
  CM_SMBIOS_FIRMWARE_INVENTORY_INFO  *FirmwareInventoryInfo;
  UINT32                             NumFirmwareComponents;
  EFI_STATUS                         Status;
  UINT32                             Index;
  CM_SMBIOS_SYSTEM_SLOTS_INFO        *SystemSlotInfo;
  CM_SMBIOS_BIOS_INFO                *BiosInfo;
  CM_SMBIOS_TPM_DEVICE_INFO          *TpmInfo;
  UINT32                             NumSystemSlots;
  EDKII_PLATFORM_REPOSITORY_INFO     *PlatformRepositoryInfo;

  NumFirmwareComponents  = 0;
  NumSystemSlots         = 0;
  NumFirmwareComponents  = 0;
  BiosInfo               = NULL;
  SystemSlotInfo         = NULL;
  TpmInfo                = NULL;
  FirmwareInventoryInfo  = NULL;
  PlatformRepositoryInfo = Private->PlatformRepositoryInfo;

  Repo = Private->Repo;

  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (PlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjBiosInfo)) {
      BiosInfo = (CM_SMBIOS_BIOS_INFO *)PlatformRepositoryInfo[Index].CmObjectPtr;
    } else if (PlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjSystemSlotInfo)) {
      SystemSlotInfo = (CM_SMBIOS_SYSTEM_SLOTS_INFO *)PlatformRepositoryInfo[Index].CmObjectPtr;
      NumSystemSlots = PlatformRepositoryInfo[Index].CmObjectCount;
    } else if (PlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjTpmDeviceInfo)) {
      TpmInfo = (CM_SMBIOS_TPM_DEVICE_INFO *)PlatformRepositoryInfo[Index].CmObjectPtr;
    } else if ((PlatformRepositoryInfo[Index].CmObjectPtr == NULL) ||
               (
                (BiosInfo != NULL) &&
                (SystemSlotInfo != NULL) &&
                (TpmInfo != NULL)
               ))
    {
      break;
    }
  }

  Status = FmpFirmwareInventoryUpdate (BiosInfo, SystemSlotInfo, NumSystemSlots, &FirmwareInventoryInfo, &NumFirmwareComponents);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: installing type 45 for FMP. Status = %r\n", __FUNCTION__, Status));
  }

  Status = TpmFirmwareInventoryUpdate (TpmInfo, &FirmwareInventoryInfo, &NumFirmwareComponents);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: installing type 45 for Tpm. Status = %r\n", __FUNCTION__, Status));
  }

  //
  // Add type 45 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType45,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  //
  // Install CM object for type 45
  //
  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjFirmwareInventoryInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = NumFirmwareComponents * sizeof (CM_SMBIOS_FIRMWARE_INVENTORY_INFO);
  Repo->CmObjectCount = NumFirmwareComponents;
  Repo->CmObjectPtr   = FirmwareInventoryInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
