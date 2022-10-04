/** @file
  Logo DXE Driver, install Edkii Platform Logo protocol.

  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2016 - 2017, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "LogoPrivate.h"
#include <Protocol/GraphicsOutput.h>

/**
  Load a platform logo image and return its data and attributes.

  @param This              The pointer to this protocol instance.
  @param Instance          The visible image instance is found.
  @param Image             Points to the image.
  @param Attribute         The display attributes of the image returned.
  @param OffsetX           The X offset of the image regarding the Attribute.
  @param OffsetY           The Y offset of the image regarding the Attribute.

  @retval EFI_SUCCESS      The image was fetched successfully.
  @retval EFI_NOT_FOUND    The specified image could not be found.
**/
EFI_STATUS
EFIAPI
GetImage (
  IN     EDKII_PLATFORM_LOGO_PROTOCOL        *This,
  IN OUT UINT32                              *Instance,
  OUT EFI_IMAGE_INPUT                        *Image,
  OUT EDKII_PLATFORM_LOGO_DISPLAY_ATTRIBUTE  *Attribute,
  OUT INTN                                   *OffsetX,
  OUT INTN                                   *OffsetY
  )
{
  EFI_STATUS                     Status;
  NVIDIA_LOGO_PRIVATE_DATA       *Private;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *GopBlt;
  UINTN                          GopBltSize;
  UINTN                          PixelHeight;
  UINTN                          PixelWidth;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  UINTN                          CurrentLogo;
  UINTN                          SelectedHeight;
  UINTN                          SelectedWidth;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *SelectedGopBlt;

  if ((This == NULL) || (Instance == NULL) || (Image == NULL) ||
      (Attribute == NULL) || (OffsetX == NULL) || (OffsetY == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVIDIA_LOGO_PRIVATE_DATA_FROM_THIS (This);

  if (*Instance != 0) {
    return EFI_NOT_FOUND;
  }

  Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&GraphicsOutput);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Attribute = EdkiiPlatformLogoDisplayAttributeCenter;
  *OffsetX   = 0;
  *OffsetY   = 0;

  SelectedHeight = 0;
  SelectedWidth  = 0;
  SelectedGopBlt = NULL;

  for (CurrentLogo = 0; CurrentLogo < Private->NumLogos; CurrentLogo++) {
    GopBlt      = NULL;
    GopBltSize  = 0;
    PixelHeight = 0;
    PixelWidth  = 0;
    Status      = TranslateBmpToGopBlt (
                    Private->LogoInfo[CurrentLogo].Base,
                    Private->LogoInfo[CurrentLogo].Size,
                    &GopBlt,
                    &GopBltSize,
                    &PixelHeight,
                    &PixelWidth
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    DEBUG ((DEBUG_ERROR, "Found %dx%d\r\n", PixelWidth, PixelHeight));
    // If larger that display or this is smaller then previous image skip
    if ((PixelHeight > GraphicsOutput->Mode->Info->VerticalResolution) ||
        (PixelWidth > GraphicsOutput->Mode->Info->HorizontalResolution) ||
        (PixelHeight < SelectedHeight) ||
        (PixelWidth < SelectedWidth))
    {
      gBS->FreePool (GopBlt);
      GopBlt = NULL;
      continue;
    }

    SelectedHeight = PixelHeight;
    SelectedWidth  = PixelWidth;
    if (SelectedGopBlt != NULL) {
      gBS->FreePool (SelectedGopBlt);
    }

    SelectedGopBlt = GopBlt;
  }

  if ((SelectedGopBlt == NULL) || (SelectedHeight == 0) || (SelectedWidth == 0)) {
    return EFI_NOT_FOUND;
  }

  (*Instance)++;
  Image->Flags  = 0;
  Image->Height = SelectedHeight;
  Image->Width  = SelectedWidth;
  Image->Bitmap = SelectedGopBlt;

  return EFI_SUCCESS;
}

/**
  Entrypoint of this module.

  This function is the entrypoint of this module. It installs the Edkii
  Platform Logo protocol.

  @param  ImageHandle       The firmware allocated handle for the EFI image.
  @param  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.

**/
EFI_STATUS
EFIAPI
InitializeLogo (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  NVIDIA_LOGO_PRIVATE_DATA  *Private;
  UINT32                    Count;
  EFI_HANDLE                Handle;

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (NVIDIA_LOGO_PRIVATE_DATA),
                  (VOID **)&Private
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->SetMem (Private, sizeof (NVIDIA_LOGO_PRIVATE_DATA), 0);

  for (Count = 0; Count < MAX_SUPPORTED_LOGO; Count++) {
    Status = GetSectionFromFv (
               &gNVIDIAPlatformLogoGuid,
               EFI_SECTION_RAW,
               Count,
               &Private->LogoInfo[Count].Base,
               &Private->LogoInfo[Count].Size
               );
    if (EFI_ERROR (Status)) {
      break;
    }
  }

  if (Count == 0) {
    gBS->FreePool (Private);
    return EFI_NOT_FOUND;
  }

  Private->Signature             = NVIDIA_LOGO_SIGNATURE;
  Private->NumLogos              = Count;
  Private->PlatformLogo.GetImage = GetImage;

  Handle = NULL;
  return gBS->InstallMultipleProtocolInterfaces (
                &Handle,
                &gEdkiiPlatformLogoProtocolGuid,
                &Private->PlatformLogo,
                NULL
                );
}
