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
  LEGACY_CM_PROTOCOL_OBJECT  *PlatformRepositoryInfo,
  LEGACY_CM_PROTOCOL_OBJECT  **CurrentPlatformRepositoryInfo
  )
{
  EFI_STATUS                 Status;
  LEGACY_CM_PROTOCOL_OBJECT  *StartOfList;
  LEGACY_CM_PROTOCOL_OBJECT  *EndOfList;
  LEGACY_CM_PROTOCOL_OBJECT  *SearchList;
  LEGACY_CM_PROTOCOL_OBJECT  *ToAddList;
  UINTN                      NumberOfProtocols;
  UINTN                      ProtocolIndex;
  LEGACY_CM_PROTOCOL_OBJECT  **ProtocolList;
  VOID                       *NewBuffer;

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
        CopyMem (EndOfList, ToAddList, sizeof (LEGACY_CM_PROTOCOL_OBJECT));
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
          CopyMem (EndOfList, ToAddList, sizeof (LEGACY_CM_PROTOCOL_OBJECT));
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
