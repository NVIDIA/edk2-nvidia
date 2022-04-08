/** @file
  Configuration Manager Library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#include <PiDxe.h>
#include <Uefi/UefiBaseType.h>
#include <ConfigurationManagerObject.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

EFI_STATUS
EFIAPI
RegisterProtocolBasedObjects (
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo,
  EDKII_PLATFORM_REPOSITORY_INFO  **CurrentPlatformRepositoryInfo
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *StartOfList;
  EDKII_PLATFORM_REPOSITORY_INFO  *EndOfList;
  EDKII_PLATFORM_REPOSITORY_INFO  *SearchList;
  EDKII_PLATFORM_REPOSITORY_INFO  *ToAddList;
  UINTN                           NumberOfProtocols;
  UINTN                           ProtocolIndex;
  EDKII_PLATFORM_REPOSITORY_INFO  **ProtocolList;
  VOID                            *NewBuffer;

  StartOfList  = PlatformRepositoryInfo;
  EndOfList    = *CurrentPlatformRepositoryInfo;
  ProtocolList = NULL;

  Status = EfiLocateProtocolBuffer (
             &gNVIDIAConfigurationManagerDataObjectGuid,
             &NumberOfProtocols,
             (VOID ***)&ProtocolList
             );
  if (Status == EFI_NOT_FOUND) {
    Status            =  EFI_SUCCESS;
    NumberOfProtocols = 0;
  } else if (EFI_ERROR (Status)) {
    NumberOfProtocols = 0;
  }

  for (ProtocolIndex = 0; ProtocolIndex < NumberOfProtocols; ProtocolIndex++) {
    ToAddList = ProtocolList[ProtocolIndex];
    if (ToAddList == NULL) {
      continue;
    }

    while (ToAddList->CmObjectPtr != NULL) {
      if (ToAddList->CmObjectToken != CM_NULL_TOKEN) {
        CopyMem (EndOfList, ToAddList, sizeof (EDKII_PLATFORM_REPOSITORY_INFO));
        EndOfList++;
      } else {
        // If there is no token look for matching node
        for (SearchList = StartOfList; SearchList < EndOfList; SearchList++) {
          if (SearchList->CmObjectId == ToAddList->CmObjectId) {
            // Append the node
            NewBuffer = AllocatePool (SearchList->CmObjectSize + ToAddList->CmObjectSize);
            if (NewBuffer == NULL) {
              Status = EFI_OUT_OF_RESOURCES;
              break;
            }

            CopyMem (NewBuffer, SearchList->CmObjectPtr, SearchList->CmObjectSize);
            CopyMem (NewBuffer + SearchList->CmObjectSize, ToAddList->CmObjectPtr, ToAddList->CmObjectSize);

            // We don't free old buffer as it may not have been allocated
            SearchList->CmObjectPtr    = NewBuffer;
            SearchList->CmObjectSize  += ToAddList->CmObjectSize;
            SearchList->CmObjectCount += ToAddList->CmObjectCount;
            break;
          }
        }

        if (SearchList == EndOfList) {
          CopyMem (EndOfList, ToAddList, sizeof (EDKII_PLATFORM_REPOSITORY_INFO));
          EndOfList++;
        }
      }

      ToAddList++;
    }

    if (EFI_ERROR (Status)) {
      break;
    }
  }

  if (!EFI_ERROR (Status)) {
    *CurrentPlatformRepositoryInfo = EndOfList;
  }

  if (ProtocolList != NULL) {
    FreePool (ProtocolList);
    ProtocolList = NULL;
  }

  return Status;
}
