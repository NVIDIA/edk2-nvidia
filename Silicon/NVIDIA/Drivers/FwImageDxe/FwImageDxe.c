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

STATIC FW_IMAGE_PRIVATE_DATA  *mPrivate           = NULL;
STATIC UINTN                  mNumFwImages        = 0;
STATIC UINT32                 mBootChain          = MAX_UINT32;
STATIC EFI_EVENT              mAddressChangeEvent = NULL;

CONST CHAR16 **
EFIAPI
FwImageGetList (
  OUT UINTN  *ImageCount
  );

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
  Find the FwPartition for an image based on boot chain.

  @param[in]  ImageName             Name of image (partition base name)
  @param[in]  ProtocolBuffer        Pointer to array of FwPartition protocols
  @param[in]  NumProtocols          Number of entries in ProtocolBuffer array
  @param[in]  BootChain             Boot chain (0=a,1=b)

  @retval NVIDIA_FW_PARTITION_PROTOCOL  Pointer to the image's FwPartition

**/
STATIC
NVIDIA_FW_PARTITION_PROTOCOL *
FwImageFindPartition (
  IN  CONST CHAR16                  *ImageName,
  IN  NVIDIA_FW_PARTITION_PROTOCOL  **ProtocolBuffer,
  IN  UINTN                         NumProtocols,
  IN  UINTN                         BootChain
  )
{
  NVIDIA_FW_PARTITION_PROTOCOL  *Protocol;
  NVIDIA_FW_PARTITION_PROTOCOL  *FoundProtocol;
  UINTN                         Index;
  EFI_STATUS                    Status;
  CHAR16                        PartitionName[MAX_PARTITION_NAME_LEN];

  FoundProtocol = NULL;

  Status = GetBootChainPartitionName (ImageName, BootChain, PartitionName);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Error getting partition name for %s, BootChain=%u\n",
      ImageName,
      BootChain
      ));
    return NULL;
  }

  // Look for the bootchain-based name, ensure no duplicates
  for (Index = 0; Index < NumProtocols; Index++) {
    Protocol = ProtocolBuffer[Index];
    if (StrCmp (Protocol->PartitionName, PartitionName) == 0) {
      if (FoundProtocol == NULL) {
        FoundProtocol = Protocol;
      } else {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Duplicate %s partitions\n",
          __FUNCTION__,
          PartitionName
          ));
        return NULL;
      }
    }
  }

  // Look for matching partition name that doesn't have A/B backup, e.g. BCT
  if (FoundProtocol == NULL) {
    for (Index = 0; Index < NumProtocols; Index++) {
      Protocol = ProtocolBuffer[Index];
      if (StrCmp (Protocol->PartitionName, ImageName) == 0) {
        if (FoundProtocol == NULL) {
          FoundProtocol = Protocol;
        } else {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Duplicate %s partitions\n",
            __FUNCTION__,
            ImageName
            ));
          return NULL;
        }
      }
    }
  }

  return FoundProtocol;
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
  FW_PARTITION_ATTRIBUTES  AttributesA, AttributesB;
  EFI_STATUS               Status;

  // get A partition attributes
  Status = Private->FwPartitionA->GetAttributes (Private->FwPartitionA, &AttributesA);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Private->Bytes     = AttributesA.Bytes;
  Private->BlockSize = AttributesA.BlockSize;

  // if B exists, its attributes must match A
  if (Private->FwPartitionB != NULL) {
    Status = Private->FwPartitionB->GetAttributes (Private->FwPartitionB, &AttributesB);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (AttributesA.Bytes != AttributesB.Bytes) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Image %s A/B have different byte counts\n",
        __FUNCTION__,
        Private->Name
        ));
      return EFI_UNSUPPORTED;
    }

    Private->BlockSize = MAX (AttributesA.BlockSize, AttributesB.BlockSize);
  }

  return EFI_SUCCESS;
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
  FW_IMAGE_PRIVATE_DATA         *Private;
  EFI_STATUS                    Status;
  UINTN                         Index;
  UINTN                         ImageCount;
  CONST CHAR16                  **ImageList;
  UINTN                         NumHandles;
  EFI_HANDLE                    *HandleBuffer;
  NVIDIA_FW_PARTITION_PROTOCOL  **ProtocolBuffer;
  VOID                          *Hob;

  ProtocolBuffer = NULL;

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

  ImageList = FwImageGetList (&ImageCount);
  if (ImageList == NULL) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAFwPartitionProtocolGuid,
                  NULL,
                  &NumHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error locating FwPartition handles: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  ProtocolBuffer = (NVIDIA_FW_PARTITION_PROTOCOL **)AllocateZeroPool (
                                                      NumHandles * sizeof (NVIDIA_FW_PARTITION_PROTOCOL *)
                                                      );
  if (ProtocolBuffer == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ProtocolBuffer allocation failed\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  for (Index = 0; Index < NumHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gNVIDIAFwPartitionProtocolGuid,
                    (VOID **)&ProtocolBuffer[Index]
                    );
    if (EFI_ERROR (Status) || (ProtocolBuffer[Index] == NULL)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to get FwPartition for handle index %u: %r\n",
        Index,
        Status
        ));
      goto Done;
    }
  }

  mPrivate = (FW_IMAGE_PRIVATE_DATA *)AllocateRuntimeZeroPool (
                                        ImageCount * sizeof (FW_IMAGE_PRIVATE_DATA)
                                        );
  if (mPrivate == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: mPrivate allocation failed\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  // initialize a private image tracking structure for every FW image
  Private = mPrivate;
  for (Index = 0; Index < ImageCount; Index++) {
    CONST CHAR16  *Name;

    Name = ImageList[Index];
    DEBUG ((DEBUG_VERBOSE, "%a: Initializing image %s\n", __FUNCTION__, Name));

    Private->Signature = FW_IMAGE_PRIVATE_DATA_SIGNATURE;
    Status             = StrnCpyS (Private->Name, sizeof (Private->Name), Name, StrLen (Name));
    if (EFI_ERROR (Status)) {
      goto Done;
    }

    Private->FwPartitionA = FwImageFindPartition (
                              Name,
                              ProtocolBuffer,
                              NumHandles,
                              BOOT_CHAIN_A
                              );
    if (Private->FwPartitionA == NULL) {
      DEBUG ((DEBUG_INFO, "%a: missing A partition for %s\n", __FUNCTION__, Name));
      continue;
    }

    if (PcdGetBool (PcdFwImageEnableBPartitions)) {
      Private->FwPartitionB = FwImageFindPartition (
                                Name,
                                ProtocolBuffer,
                                NumHandles,
                                BOOT_CHAIN_B
                                );
      if (Private->FwPartitionB == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: missing B partition for %s\n", __FUNCTION__, Name));
        Status = EFI_UNSUPPORTED;
        goto Done;
      }
    }

    Status = FwImageGetPartitionAttributes (Private);
    if (EFI_ERROR (Status)) {
      goto Done;
    }

    Private->Protocol.ImageName     = Private->Name;
    Private->Protocol.Read          = FwImageRead;
    Private->Protocol.Write         = FwImageWrite;
    Private->Protocol.GetAttributes = FwImageGetAttributes;

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Private->Handle,
                    &gNVIDIAFwImageProtocolGuid,
                    &Private->Protocol,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Couldn't install protocol interface for Index=%u, image=%s: %r\n",
        __FUNCTION__,
        Index,
        Private->Name,
        Status
        ));
      goto Done;
    }

    mNumFwImages++;
    Private++;
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

Done:
  FreePool (ImageList);

  if (ProtocolBuffer != NULL) {
    FreePool (ProtocolBuffer);
    ProtocolBuffer = NULL;
  }

  if (EFI_ERROR (Status)) {
    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }

    Private = mPrivate;
    for (Index = 0; Index < mNumFwImages; Index++, Private++) {
      if (Private->Handle != NULL) {
        gBS->UninstallMultipleProtocolInterfaces (
               Private->Handle,
               &gNVIDIAFwImageProtocolGuid,
               &Private->Protocol,
               NULL
               );
      }
    }

    if (mPrivate != NULL) {
      FreePool (mPrivate);
      mPrivate = NULL;
    }

    mNumFwImages = 0;
    mBootChain   = MAX_UINT32;
  }

  return Status;
}
