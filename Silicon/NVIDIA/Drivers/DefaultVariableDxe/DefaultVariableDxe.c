/** @file
  Provides support for default variable creation.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/VariablePolicyHelperLib.h>
#include <libfdt.h>

#define VARIABLE_NODE_PATH    "/firmware/uefi/variables"
#define VARIABLE_GUID_BASED   "guid-based"
#define VARIABLE_GUID_PROP    "guid"
#define VARIABLE_MAX_NAME     64
#define VARIABLE_RUNTIME_PROP "runtime"
#define VARIABLE_NV_PROP      "non-volatile"
#define VARIABLE_LOCKED_PROP  "locked"
#define VARIABLE_DATA_PROP    "data"

STATIC VOID     *Registration = NULL;
STATIC VOID     *RegistrationPolicy = NULL;
STATIC BOOLEAN  VariablesParsed = FALSE;

/**
  Requests the variable to be locked.

  @param  Guid              Guid of the variable
  @param  VariableName      Name of the variable

**/
STATIC
VOID
LockVariable (
  IN EFI_GUID     *Guid,
  IN CONST CHAR16 *VariableName
  )
{
  EFI_STATUS                     Status;
  EDKII_VARIABLE_POLICY_PROTOCOL *PolicyProtocol;

  Status = gBS->LocateProtocol (&gEdkiiVariablePolicyProtocolGuid,
                                NULL,
                                (VOID **)&PolicyProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to locate policy protocol\r\n"));
    return;
  }

  Status = RegisterBasicVariablePolicy (
             PolicyProtocol,
             Guid,
             VariableName,
             0,
             0,
             0,
             0,
             VARIABLE_POLICY_TYPE_LOCK_NOW
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to register lock policy\r\n"));
    return;
  }
}

/**
  Process a single variable from the DTB.

  @param  Dtb               Pointer to base of Dtb.
  @param  Offset            Offset of variable node in DTB
  @param  Guid              Guid of the variable

**/
STATIC
VOID
ProcessVariable (
  IN VOID      *Dtb,
  IN INT32     Offset,
  IN EFI_GUID  *Guid
  )
{
  EFI_STATUS  Status;
  CHAR16      VariableName[VARIABLE_MAX_NAME];
  CONST CHAR8 *NodeName;
  BOOLEAN     Locked;
  UINT32      CurrentAttributes;
  UINT32      RequestedAttributes;
  UINTN       DataSize;
  CONST VOID  *Data;
  INT32       Length;
  CHAR16      *UnitAddress;

  Locked = FALSE;
  RequestedAttributes = EFI_VARIABLE_BOOTSERVICE_ACCESS;

  NodeName = fdt_get_name (Dtb, Offset, NULL);
  if (NodeName == NULL) {
    DEBUG ((DEBUG_ERROR, "Node has no name at offset %x\r\n", Offset));
    return;
  }

  Status = AsciiStrToUnicodeStrS (NodeName, VariableName, VARIABLE_MAX_NAME);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to convert variable name to unicode\r\n"));
    return;
  }

  UnitAddress = StrStr (VariableName, L"@");
  if (UnitAddress != NULL) {
    *UnitAddress = L'\0';
  }

  if (fdt_getprop (Dtb, Offset, VARIABLE_RUNTIME_PROP, NULL) != NULL) {
    RequestedAttributes |= EFI_VARIABLE_RUNTIME_ACCESS;
  }
  if (fdt_getprop (Dtb, Offset, VARIABLE_LOCKED_PROP, NULL) != NULL) {
    Locked = TRUE;
  }
  if (fdt_getprop (Dtb, Offset, VARIABLE_NV_PROP, NULL) != NULL) {
    RequestedAttributes |= EFI_VARIABLE_NON_VOLATILE;
  }

  Data = fdt_getprop (Dtb, Offset, VARIABLE_DATA_PROP, &Length);
  if ((Data == NULL) || (Length < 0)) {
    DEBUG ((DEBUG_ERROR, "No data property, %s\r\n", VariableName));
    return;
  }

  DataSize = 0;
  Status = gRT->GetVariable (VariableName, Guid, &CurrentAttributes, &DataSize, NULL);

  if (Status == EFI_BUFFER_TOO_SMALL) {
    //Variable already exists
    if (CurrentAttributes == RequestedAttributes) {
      if (Locked) {
        LockVariable (Guid, VariableName);
      }
      return;
    }

    if (Locked) {
      DEBUG ((DEBUG_ERROR, "Mismatch in locked variable %s attributes, recreating\r\n", VariableName));
      Status = gRT->SetVariable (VariableName, Guid, CurrentAttributes, 0, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to delete variable, skipping\r\n"));
        return;
      }
    } else {
      DEBUG ((DEBUG_ERROR, "Mismatch in non-locked variable %s attributes, ignoring\r\n", VariableName));
      return;
    }
  } else if (Status != EFI_NOT_FOUND) {
    //Other error
    DEBUG ((DEBUG_ERROR, "Error getting info on %s-%r\r\n",VariableName,Status));
    return;
  }

  DataSize = Length;
  Status = gRT->SetVariable (VariableName, Guid, RequestedAttributes, DataSize, (VOID *)Data);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create variable %s\r\n", VariableName));
    return;
  }

  if (Locked) {
    LockVariable (Guid, VariableName);
  }
}


/**
  Callback when variable protocol is ready.

  @param  Event             Event that was called.
  @param  Context           Context structure.

**/
STATIC
VOID
VariableReady (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS  Status;
  VOID        *Dtb;
  UINTN       DtbSize;
  INT32       NodeOffset;
  INT32       SubNodeOffset = 0;
  INT32       VariableNodeOffset;
  EFI_GUID    *VariableGuid;
  EFI_GUID    DtbGuid;
  CONST CHAR8 *NodeName;
  CONST CHAR8 *GuidStr;
  VOID        *Protocol;
  BOOLEAN     GuidBased;

  Status = gBS->LocateProtocol (&gEfiVariableWriteArchProtocolGuid, NULL, &Protocol);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = gBS->LocateProtocol (&gEdkiiVariablePolicyProtocolGuid, NULL, &Protocol);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = gBS->CloseEvent (Event);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to close variable notification event - %r\r\n", Status));
    return;
  }

  if (VariablesParsed) {
    return;
  }
  VariablesParsed = TRUE;

  Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get dtb - %r\r\n", Status));
    return;
  }

  NodeOffset = fdt_path_offset (Dtb, VARIABLE_NODE_PATH);
  if (NodeOffset < 0) {
    return;
  }

  fdt_for_each_subnode (SubNodeOffset, Dtb, NodeOffset) {
    NodeName = fdt_get_name (Dtb, SubNodeOffset, NULL);
    if (NodeName == NULL) {
      DEBUG ((DEBUG_ERROR, "Node has no name at offset %x\r\n", SubNodeOffset));
      continue;
    }

    GuidBased = FALSE;
    if (AsciiStrCmp (NodeName, "gNVIDIAPublicVariableGuid") == 0) {
      VariableGuid = &gNVIDIAPublicVariableGuid;
    } else if (AsciiStrCmp (NodeName, "gEfiGlobalVariableGuid") == 0) {
      VariableGuid = &gEfiGlobalVariableGuid;
    } else if (AsciiStrCmp (NodeName, "gDtPlatformFormSetGuid") == 0) {
      VariableGuid = &gDtPlatformFormSetGuid;
    } else if (AsciiStrCmp (NodeName, VARIABLE_GUID_BASED) == 0) {
      GuidBased = TRUE;
    } else {
      DEBUG ((DEBUG_ERROR, "Unknown expected dtb name:%a\r\n", NodeName));
      continue;
    }

    fdt_for_each_subnode (VariableNodeOffset, Dtb, SubNodeOffset) {
      if (GuidBased) {
        GuidStr = (CONST CHAR8 *)fdt_getprop (Dtb, VariableNodeOffset, VARIABLE_GUID_PROP, NULL);
        if (GuidStr == NULL) {
          DEBUG ((DEBUG_ERROR, "No Guid found\r\n"));
          continue;
        }
        Status = AsciiStrToGuid (GuidStr, &DtbGuid);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to convert %a to GUID - %r\r\n", GuidStr, Status));
          continue;
        }
        VariableGuid = &DtbGuid;
      }

      ProcessVariable (Dtb, VariableNodeOffset, VariableGuid);
    }
  }
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
InitializeDefaultVariable (
  IN EFI_HANDLE               ImageHandle,
  IN EFI_SYSTEM_TABLE         *SystemTable
  )
{
  EFI_EVENT                   NotifyEvent;
  EFI_EVENT                   NotifyEventPolicy;

  NotifyEvent = EfiCreateProtocolNotifyEvent (
                  &gEfiVariableWriteArchProtocolGuid,
                  TPL_CALLBACK,
                  VariableReady,
                  NULL,
                  &Registration
                  );

  if (NotifyEvent == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  NotifyEventPolicy = EfiCreateProtocolNotifyEvent (
                        &gEdkiiVariablePolicyProtocolGuid,
                        TPL_CALLBACK,
                        VariableReady,
                        NULL,
                        &RegistrationPolicy
                        );

  if (NotifyEventPolicy == NULL) {
    gBS->CloseEvent (NotifyEvent);
    return EFI_OUT_OF_RESOURCES;
  }
  return EFI_SUCCESS;
}
