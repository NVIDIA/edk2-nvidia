/** @file
  FW Image Protocol Dxe

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/FwImageProtocol.h>
#include <Protocol/FwPartitionProtocol.h>

#define FW_IMAGE_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('F','W','I','M')

// private data structure per image
typedef struct FW_IMAGE_PRIVATE_DATA {
  UINT32                          Signature;

  // image info
  CHAR16                          Name[FW_IMAGE_NAME_LENGTH];
  UINTN                           Bytes;
  UINT32                          BlockSize;
  NVIDIA_FW_PARTITION_PROTOCOL    *FwPartitionA;
  NVIDIA_FW_PARTITION_PROTOCOL    *FwPartitionB;

  // protocol info
  EFI_HANDLE                      Handle;
  NVIDIA_FW_IMAGE_PROTOCOL        Protocol;
} FW_IMAGE_PRIVATE_DATA;

STATIC FW_IMAGE_PRIVATE_DATA  *mPrivate              = NULL;
STATIC UINTN                  mNumFwImages           = 0;
STATIC UINT32                 mBootChain             = MAX_UINT32;
STATIC EFI_EVENT              mAddressChangeEvent    = NULL;
STATIC EFI_EVENT              mNewImageEvent         = NULL;
STATIC VOID                   *mNewImageRegistration = NULL;

/**
  Check if image as an 'A' partition.


  @param[in]  Private               Image private data structure pointer

  @retval BOOLEAN                   Flag TRUE if image has an 'A' partition

**/
STATIC
BOOLEAN
EFIAPI
HasAImage (
  IN  CONST FW_IMAGE_PRIVATE_DATA  *Private
  )
{
  return (Private->FwPartitionA != NULL);
}

/**
  Check if image as an 'B' partition.


  @param[in]  Private               Image private data structure pointer

  @retval BOOLEAN                   Flag TRUE if image has an 'B' partition

**/
STATIC
BOOLEAN
EFIAPI
HasBImage (
  IN  CONST FW_IMAGE_PRIVATE_DATA  *Private
  )
{
  return (Private->FwPartitionB != NULL);
}

/**
  Check if the image's 'B' partition is the active partition.


  @param[in]  Private               Image private data structure pointer

  @retval BOOLEAN                   Flag TRUE if image 'B' partition is active

**/
STATIC
BOOLEAN
EFIAPI
BImageIsActive (
  IN  FW_IMAGE_PRIVATE_DATA  *Private
  )
{
  return (mBootChain == 1);
}

/**
  Get the image's active partition pointer.

  @param[in]  Private                   Image private data structure pointer

  @retval NVIDIA_FW_PARTITION_PROTOCOL  Pointer to the active partition

**/
STATIC
NVIDIA_FW_PARTITION_PROTOCOL *
EFIAPI
ActiveImagePartition (
  IN  FW_IMAGE_PRIVATE_DATA  *Private
  )
{
  return (BImageIsActive (Private)) ? Private->FwPartitionB : Private->FwPartitionA;
}

/**
  Get the image's inactive partition pointer.

  @param[in]  Private                   Image private data structure pointer

  @retval NVIDIA_FW_PARTITION_PROTOCOL  Pointer to the inactive partition

**/
STATIC
NVIDIA_FW_PARTITION_PROTOCOL *
EFIAPI
InactiveImagePartition (
  IN  FW_IMAGE_PRIVATE_DATA  *Private
  )
{
  return (BImageIsActive (Private)) ? Private->FwPartitionA : Private->FwPartitionB;
}

/**
  Check that given Offset and Bytes don't exceed given MaxOffset.

  @param[in]  MaxOffset         Maximum offset allowed
  @param[in]  Offset            Offset of operation
  @param[in]  Bytes             Number of bytes to access at offset

  @retval EFI_SUCCESS           Offset and Bytes are within the MaxOffset limit
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FwImageCheckOffsetAndBytes (
  IN  UINT64  MaxOffset,
  IN  UINT64  Offset,
  IN  UINTN   Bytes
  )
{
  // Check offset and bytes separately to avoid overflow
  if ((Offset > MaxOffset) ||
      (Bytes > MaxOffset) ||
      (Offset + Bytes > MaxOffset))
  {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

// NVIDIA_FW_IMAGE_PROTOCOL.Write()
STATIC
EFI_STATUS
EFIAPI
FwImageWrite (
  IN  NVIDIA_FW_IMAGE_PROTOCOL  *This,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  IN  CONST VOID                *Buffer,
  IN  UINTN                     Flags
  )
{
  FW_IMAGE_PRIVATE_DATA         *Private;
  CONST CHAR16                  *ImageName;
  EFI_STATUS                    Status;
  NVIDIA_FW_PARTITION_PROTOCOL  *Partition;

  if ((Buffer == NULL) || (This == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              FW_IMAGE_PRIVATE_DATA,
              Protocol,
              FW_IMAGE_PRIVATE_DATA_SIGNATURE
              );
  ImageName = Private->Name;

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Entry for name=%s, Bytes=%llu Buffer=0x%p\n",
    __FUNCTION__,
    ImageName,
    Bytes,
    Buffer
    ));

  Status = FwImageCheckOffsetAndBytes (Private->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  // Get partition to use based on active boot chain and override flags
  if (Flags & (FW_IMAGE_RW_FLAG_FORCE_PARTITION_A |
               FW_IMAGE_RW_FLAG_FORCE_PARTITION_B))
  {
    if (Flags & (FW_IMAGE_RW_FLAG_FORCE_PARTITION_A)) {
      Partition = Private->FwPartitionA;
    } else {
      Partition = Private->FwPartitionB;
    }
  } else if (HasAImage (Private) && HasBImage (Private)) {
    Partition = InactiveImagePartition (Private);
  } else {
    Partition = Private->FwPartitionA;
  }

  if (Partition == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "Image %s, flags=0x%x invalid partition, A=%u, B=%u\n",
      ImageName,
      Flags,
      HasAImage (Private),
      HasBImage (Private)
      ));
    return EFI_NOT_FOUND;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "Starting write of %s, Offset=%llu, Bytes=%u\n",
    Partition->PartitionName,
    Offset,
    Bytes
    ));

  Status = Partition->Write (
                        Partition,
                        Offset,
                        Bytes,
                        Buffer
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Error writing %s, Offset=%llu: %r\n",
      Partition->PartitionName,
      Offset,
      Status
      ));
  }

  return Status;
}

// NVIDIA_FW_IMAGE_PROTOCOL.Read()
STATIC
EFI_STATUS
EFIAPI
FwImageRead (
  IN  NVIDIA_FW_IMAGE_PROTOCOL  *This,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  OUT VOID                      *Buffer,
  IN  UINTN                     Flags
  )
{
  FW_IMAGE_PRIVATE_DATA         *Private;
  CONST CHAR16                  *ImageName;
  EFI_STATUS                    Status;
  NVIDIA_FW_PARTITION_PROTOCOL  *Partition;

  if ((Buffer == NULL) || (This == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              FW_IMAGE_PRIVATE_DATA,
              Protocol,
              FW_IMAGE_PRIVATE_DATA_SIGNATURE
              );
  ImageName = Private->Name;

  Status = FwImageCheckOffsetAndBytes (Private->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  // Get partition to use based on active boot chain and override flags
  if (Flags & (FW_IMAGE_RW_FLAG_FORCE_PARTITION_A |
               FW_IMAGE_RW_FLAG_FORCE_PARTITION_B))
  {
    if (Flags & (FW_IMAGE_RW_FLAG_FORCE_PARTITION_A)) {
      Partition = Private->FwPartitionA;
    } else {
      Partition = Private->FwPartitionB;
    }
  } else if (HasAImage (Private) && HasBImage (Private)) {
    if (Flags & FW_IMAGE_RW_FLAG_READ_INACTIVE_IMAGE) {
      Partition = InactiveImagePartition (Private);
    } else {
      Partition = ActiveImagePartition (Private);
    }
  } else {
    Partition = Private->FwPartitionA;
  }

  if (Partition == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "Image %s, flags=0x%x invalid partition, A=%u, B=%u\n",
      ImageName,
      Flags,
      HasAImage (Private),
      HasBImage (Private)
      ));
    return EFI_NOT_FOUND;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "Starting read of %s, Offset=%llu, Bytes=%u\n",
    Partition->PartitionName,
    Offset,
    Bytes
    ));

  Status = Partition->Read (
                        Partition,
                        Offset,
                        Bytes,
                        Buffer
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Error reading %s, Offset=%llu: %r\n",
      Partition->PartitionName,
      Offset,
      Status
      ));
  }

  return Status;
}

// NVIDIA_FW_IMAGE_PROTOCOL.GetAttributes()
STATIC
EFI_STATUS
EFIAPI
FwImageGetAttributes (
  IN  NVIDIA_FW_IMAGE_PROTOCOL  *This,
  IN  FW_IMAGE_ATTRIBUTES       *Attributes
  )
{
  FW_IMAGE_PRIVATE_DATA  *Private;

  if ((Attributes == NULL) || (This == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              FW_IMAGE_PRIVATE_DATA,
              Protocol,
              FW_IMAGE_PRIVATE_DATA_SIGNATURE
              );

  Attributes->Bytes     = Private->Bytes;
  Attributes->BlockSize = Private->BlockSize;

  return EFI_SUCCESS;
}

/**
  Handle address change notification to support runtime execution.

  @param[in]  Event         Event being handled
  @param[in]  Context       Event context

  @retval None

**/
STATIC
VOID
EFIAPI
FwImageDxeAddressChangeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN                  Index;
  FW_IMAGE_PRIVATE_DATA  *Private;

  Private = mPrivate;
  for (Index = 0; Index < mNumFwImages; Index++, Private++) {
    if (Private->FwPartitionA != NULL) {
      EfiConvertPointer (0x0, (VOID **)&Private->FwPartitionA);
    }

    if (Private->FwPartitionB != NULL) {
      EfiConvertPointer (0x0, (VOID **)&Private->FwPartitionB);
    }

    EfiConvertPointer (0x0, (VOID **)&Private->Protocol.ImageName);
    EfiConvertPointer (0x0, (VOID **)&Private->Protocol.Read);
    EfiConvertPointer (0x0, (VOID **)&Private->Protocol.Write);
    EfiConvertPointer (0x0, (VOID **)&Private->Protocol.GetAttributes);
  }

  EfiConvertPointer (0x0, (VOID **)&mPrivate);
}

/**
  Gets the FwPartition attributes for the image and updates private structure.

  @param[in]  Private               Image private data structure pointer

  @retval EFI_SUCCESS               Operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FwImageGetPartitionAttributes (
  FW_IMAGE_PRIVATE_DATA  *Private
  )
{
  FW_PARTITION_ATTRIBUTES  Attributes[2] = { 0 };
  UINTN                    Index;
  EFI_STATUS               Status;

  Index = 0;
  if (Private->FwPartitionA != NULL) {
    Status = Private->FwPartitionA->GetAttributes (Private->FwPartitionA, &Attributes[Index]);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Index++;
  }

  if (Private->FwPartitionB != NULL) {
    Status = Private->FwPartitionB->GetAttributes (Private->FwPartitionB, &Attributes[Index]);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Index++;
  }

  ASSERT (Index >= 1);
  Private->Bytes     = Attributes[0].Bytes;
  Private->BlockSize = Attributes[0].BlockSize;

  if (Index == 2) {
    if (Attributes[0].Bytes != Attributes[1].Bytes) {
      DEBUG ((DEBUG_ERROR, "%a: Image %s A/B have different byte counts\n", __FUNCTION__, Private->Name));
      return EFI_UNSUPPORTED;
    }

    Private->BlockSize = MAX (Attributes[0].BlockSize, Attributes[1].BlockSize);
  }

  return EFI_SUCCESS;
}

/**
  Finds the FwImage private control structure by name.

  @param[in]  ImageName             Pointer to name of FwImage.

  @retval FW_IMAGE_PRIVATE_DATA *   Pointer to private structure for Name.
  @retval NULL                      FwImage Name not found.

**/
FW_IMAGE_PRIVATE_DATA  *
EFIAPI
FwImageFind (
  CONST CHAR16  *ImageName
  )
{
  FW_IMAGE_PRIVATE_DATA  *Private;
  UINTN                  Index;

  Private = mPrivate;
  for (Index = 0; Index < mNumFwImages; Index++, Private++) {
    if (StrCmp (ImageName, Private->Name) == 0) {
      return Private;
    }
  }

  return NULL;
}

/**
  Checks to see if FwImage is not expected to have a backup partition.

  @param[in]  Private               Pointer FwImage control structure.

  @retval BOOLEAN                   TRUE if image has no backup partition.

**/
STATIC
BOOLEAN
EFIAPI
FwImageHasNoBackup (
  FW_IMAGE_PRIVATE_DATA  *Private
  )
{
  return (StrCmp (Private->Name, L"BCT-boot-chain_backup") == 0);
}

/**
  Checks to see if FwImage ready to install.

  @param[in]  Private               Pointer FwImage control structure.

  @retval BOOLEAN                   TRUE if image is ready to install.

**/
STATIC
BOOLEAN
EFIAPI
FwImageIsReadyToInstall (
  FW_IMAGE_PRIVATE_DATA  *Private
  )
{
  if ((Private->FwPartitionA != NULL) &&
      ((PcdGetBool (PcdFwImageEnableBPartitions) && (Private->FwPartitionB != NULL)) ||
       !PcdGetBool (PcdFwImageEnableBPartitions) ||
       FwImageHasNoBackup (Private)))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  Event notification for installation of FwPartition protocol instance.

  @param  Event                 The Event that is being processed.
  @param  Context               Event Context.

**/
VOID
EFIAPI
FwImageProtocolCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  FW_IMAGE_PRIVATE_DATA         *Private;
  EFI_STATUS                    Status;
  UINTN                         HandleSize;
  EFI_HANDLE                    Handle;
  NVIDIA_FW_PARTITION_PROTOCOL  *FwPartitionProtocol;
  CHAR16                        ImageName[MAX_PARTITION_NAME_LEN];
  CONST CHAR16                  *PartitionName;
  UINTN                         BootChain;

  while (TRUE) {
    HandleSize = sizeof (EFI_HANDLE);
    Status     = gBS->LocateHandle (
                        ByRegisterNotify,
                        &gNVIDIAFwPartitionProtocolGuid,
                        mNewImageRegistration,
                        &HandleSize,
                        &Handle
                        );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: No handles: %r\n", __FUNCTION__, Status));
      return;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gNVIDIAFwPartitionProtocolGuid,
                    (VOID **)&FwPartitionProtocol
                    );
    if (EFI_ERROR (Status) || (FwPartitionProtocol == NULL)) {
      DEBUG ((DEBUG_ERROR, "Failed to get FwPartition for handle: %r\n", Status));
      continue;
    }

    PartitionName = FwPartitionProtocol->PartitionName;
    if (StrCmp (PartitionName, L"BCT") == 0) {
      // Don't build an image for BCT, it's handled by BrBctProtocol
      continue;
    }

    Status = GetPartitionBaseNameAndBootChain (PartitionName, ImageName, &BootChain);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get base name for %s: %r\n", PartitionName, Status));
      continue;
    }

    Private = FwImageFind (ImageName);
    if (Private == NULL) {
      DEBUG ((DEBUG_INFO, "%a: Initializing image %s\n", __FUNCTION__, ImageName));

      if (mNumFwImages >= FW_IMAGE_MAX_IMAGES) {
        DEBUG ((DEBUG_ERROR, "%a: too many FW images, can't add %s\n", __FUNCTION__, ImageName));
        continue;
      }

      Private            = &mPrivate[mNumFwImages++];
      Private->Signature = FW_IMAGE_PRIVATE_DATA_SIGNATURE;
      Status             = StrnCpyS (Private->Name, sizeof (Private->Name), ImageName, StrLen (ImageName));
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: failed to add %s: %r\n", __FUNCTION__, ImageName, Status));
        continue;
      }
    }

    DEBUG ((DEBUG_INFO, "%a: Adding partition %s for image %s\n", __FUNCTION__, PartitionName, ImageName));

    if (BootChain == BOOT_CHAIN_A) {
      Private->FwPartitionA = FwPartitionProtocol;
    } else if (BootChain == BOOT_CHAIN_B) {
      if (PcdGetBool (PcdFwImageEnableBPartitions)) {
        Private->FwPartitionB = FwPartitionProtocol;
      }
    } else {
      DEBUG ((DEBUG_ERROR, "%a: bad boot chain=%u for %s\n", __FUNCTION__, BootChain, PartitionName));
      continue;
    }

    Status = FwImageGetPartitionAttributes (Private);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Private->Protocol.ImageName     = Private->Name;
    Private->Protocol.Read          = FwImageRead;
    Private->Protocol.Write         = FwImageWrite;
    Private->Protocol.GetAttributes = FwImageGetAttributes;

    if (FwImageIsReadyToInstall (Private)) {
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Private->Handle,
                      &gNVIDIAFwImageProtocolGuid,
                      &Private->Protocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Couldn't install protocol interface for image=%s: %r\n", __FUNCTION__, Private->Name, Status));
      }
    }
  }
}

/**
  Fw Image Protocol Driver initialization entry point.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwImageDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  VOID        *Hob;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    mBootChain = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ActiveBootChain;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting active boot chain\n",
      __FUNCTION__
      ));
    return EFI_UNSUPPORTED;
  }

  mPrivate = (FW_IMAGE_PRIVATE_DATA *)AllocateRuntimeZeroPool (FW_IMAGE_MAX_IMAGES * sizeof (FW_IMAGE_PRIVATE_DATA));
  if (mPrivate == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: mPrivate allocation failed\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  FwImageDxeAddressChangeNotify,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mAddressChangeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error creating address change event: %r\n",
      __FUNCTION__,
      Status
      ));
    ASSERT_EFI_ERROR (Status);
    goto Done;
  }

  mNewImageEvent = EfiCreateProtocolNotifyEvent (
                     &gNVIDIAFwPartitionProtocolGuid,
                     TPL_CALLBACK,
                     FwImageProtocolCallback,
                     NULL,
                     &mNewImageRegistration
                     );
  if (mNewImageEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: protocol notify failed\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
  }

Done:
  if (EFI_ERROR (Status)) {
    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }
  }

  return Status;
}
