/** @file
  Provides support for default variable creation.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/VariablePolicyHelperLib.h>
#include <Library/FileHandleLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PcdLib.h>
#include <Protocol/PartitionInfo.h>
#include <libfdt.h>

#define VARIABLE_NODE_PATH     "/firmware/uefi/variables"
#define VARIABLE_GUID_BASED    "guid-based"
#define VARIABLE_GUID_PROP     "guid"
#define VARIABLE_MAX_NAME      64
#define VARIABLE_RUNTIME_PROP  "runtime"
#define VARIABLE_NV_PROP       "non-volatile"
#define VARIABLE_LOCKED_PROP   "locked"
#define VARIABLE_DATA_PROP     "data"
#define ESP_VAR_DIR_PATH       L"EFI\\NVDA\\Variables"
#define ESP_VAR_ATTR_EXP       (EFI_VARIABLE_NON_VOLATILE |\
                               EFI_VARIABLE_BOOTSERVICE_ACCESS |\
                               EFI_VARIABLE_RUNTIME_ACCESS)
#define ESP_VAR_ATTR_SZ        (4)

STATIC VOID     *Registration       = NULL;
STATIC VOID     *RegistrationPolicy = NULL;
STATIC BOOLEAN  VariablesParsed     = FALSE;

/**
  Requests the variable to be locked.

  @param  Guid              Guid of the variable
  @param  VariableName      Name of the variable
  @param  LockType          Variable policy lock type

**/
STATIC
VOID
LockVariable (
  IN EFI_GUID      *Guid,
  IN CONST CHAR16  *VariableName,
  IN UINT8         LockType
  )
{
  EFI_STATUS                      Status;
  EDKII_VARIABLE_POLICY_PROTOCOL  *PolicyProtocol;

  Status = gBS->LocateProtocol (
                  &gEdkiiVariablePolicyProtocolGuid,
                  NULL,
                  (VOID **)&PolicyProtocol
                  );
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
             LockType
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to register lock policy: %r\r\n", Status));
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
  EFI_STATUS   Status;
  CHAR16       VariableName[VARIABLE_MAX_NAME];
  CONST CHAR8  *NodeName;
  BOOLEAN      Locked;
  UINT32       CurrentAttributes;
  UINT32       RequestedAttributes;
  UINTN        DataSize;
  CONST VOID   *Data;
  INT32        Length;
  CHAR16       *UnitAddress;

  Locked              = FALSE;
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
  Status   = gRT->GetVariable (VariableName, Guid, &CurrentAttributes, &DataSize, NULL);

  if (Status == EFI_BUFFER_TOO_SMALL) {
    // Variable already exists
    if (CurrentAttributes == RequestedAttributes) {
      if (Locked) {
        LockVariable (Guid, VariableName, VARIABLE_POLICY_TYPE_LOCK_NOW);
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
    // Other error
    DEBUG ((DEBUG_ERROR, "Error getting info on %s-%r\r\n", VariableName, Status));
    return;
  }

  DataSize = Length;
  Status   = gRT->SetVariable (VariableName, Guid, RequestedAttributes, DataSize, (VOID *)Data);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create variable %s\r\n", VariableName));
    return;
  }

  if (Locked) {
    LockVariable (Guid, VariableName, VARIABLE_POLICY_TYPE_LOCK_NOW);
  }
}

/**
  Util Function to get the Variable Name and Guid read as a file name from the
  EFI partition

  @param FileInfo          File Handle of the Variable File.
  @param VarName           Variable Name output.
  @param EfiVarGuid        Guid of the Variable.

**/
STATIC
EFI_STATUS
GetEspVarNameAndGuid (
  IN  EFI_FILE_INFO  *FileInfo,
  OUT CHAR16         *VarName,
  OUT EFI_GUID       *EfiVarGuid
  )
{
  EFI_STATUS  Status;
  CHAR16      *SplitPos;
  CHAR16      *VarGuid;

  SplitPos = StrStr (FileInfo->FileName, L"-");
  if (SplitPos == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Unexpected File Name %s",
      __FUNCTION__,
      FileInfo->FileName
      ));
    Status = EFI_INVALID_PARAMETER;
    goto exit;
  }

  ZeroMem (VarName, VARIABLE_MAX_NAME);
  Status = StrnCpyS (
             VarName,
             VARIABLE_MAX_NAME,
             FileInfo->FileName,
             ((SplitPos - FileInfo->FileName))
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Strncpy Failed %r\n", __FUNCTION__, Status));
    goto exit;
  }

  VarGuid = SplitPos + 1;
  if ((StrToGuid (VarGuid, EfiVarGuid)) != RETURN_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to convert %s to EFI_GUID\n",
      __FUNCTION__,
      VarGuid
      ));
    Status = EFI_INVALID_PARAMETER;
    goto exit;
  }

exit:
  return Status;
}

/**
  Util Function to get the Variable Data, Size and Attributes from a
  Variable File on the ESP partition.

  @param VarFile          File Handle of the Variable File.
  @param FileSize         Size of the File.
  @param FileData         Output Buffer containing File Data.
  @param VarData          Pointer to the Variable Data.
  @param VarAttr          Variable attributes read from the file.
  @param VarSize          Size of the Variable.

**/
STATIC
EFI_STATUS
GetEspVarDataAndAttr (
  IN  EFI_FILE  *VarFile,
  IN  UINTN     FileSize,
  OUT VOID      **FileData,
  OUT VOID      **VarData,
  OUT UINT32    *VarAttr,
  OUT UINTN     *VarSize
  )
{
  EFI_STATUS     Status;
  EFI_FILE_INFO  *FileInfo;
  CHAR8          *BytePtr;

  FileInfo = FileHandleGetInfo (VarFile);
  if (FileInfo == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid File Handle %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  *FileData = AllocateZeroPool (FileSize);
  if (*FileData == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate buffer for %s\r\n",
      __FUNCTION__,
      FileInfo->FileName
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  FileHandleSetPosition (VarFile, 0);
  Status = FileHandleRead (VarFile, &FileSize, *FileData);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to read File %s %r\n",
      __FUNCTION__,
      FileInfo->FileName,
      Status
      ));
    return Status;
  }

  BytePtr = *FileData;
  CopyMem (VarAttr, BytePtr, ESP_VAR_ATTR_SZ);
  BytePtr += 4;

  *VarData = BytePtr;
  *VarSize = FileSize - 4;
  return Status;
}

/**
  Process an EFI System Partition Variable

  @param Dir          Pointer to the directory containing the variables..
  @param FileInfo     File Info of the Variable File.

**/
STATIC
EFI_STATUS
ProcessEspVariable (
  EFI_FILE       *Dir,
  EFI_FILE_INFO  *FileInfo
  )
{
  EFI_STATUS  Status;
  EFI_FILE    *File;
  CHAR16      VarName[VARIABLE_MAX_NAME];
  UINTN       VarSize;
  EFI_GUID    EfiVarGuid;
  UINT32      VarAttr;
  VOID        *VarData;
  UINT64      FileSize;
  VOID        *FileData = NULL;
  UINTN       ReadSize;

  if ((FileInfo == NULL) || (Dir == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = Dir->Open (
                  Dir,
                  &File,
                  FileInfo->FileName,
                  (EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE),
                  0
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to open File %s %r \n\r",
      __FUNCTION__,
      FileInfo->FileName,
      Status
      ));
    goto exit;
  }

  Status = GetEspVarNameAndGuid (FileInfo, VarName, &EfiVarGuid);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Esp Var Name/Guid %r\n\r",
      __FUNCTION__,
      Status
      ));
    goto exit;
  }

  Status = FileHandleGetSize (File, &FileSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get File Size %lu\n",
      __FUNCTION__,
      FileSize
      ));
    goto exit;
  }

  if ((FileSize > (PcdGet32 (PcdMaxVariableSize) + sizeof (VarAttr)))  ||
      (FileSize < ESP_VAR_ATTR_SZ))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s Invalid File Size %lu (min %u max %u)\r\n",
      __FUNCTION__,
      FileInfo->FileName,
      FileSize,
      ESP_VAR_ATTR_SZ,
      (PcdGet32 (PcdMaxVariableSize) + sizeof (VarAttr))
      ));
    Status = EFI_INVALID_PARAMETER;
    goto exit;
  }

  ReadSize = (UINTN)FileSize;
  Status   = GetEspVarDataAndAttr (
               File,
               ReadSize,
               &FileData,
               &VarData,
               &VarAttr,
               &VarSize
               );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to read File Data %r\n\r",
      __FUNCTION__,
      Status
      ));
    goto exit;
  }

  if ((VarAttr & ESP_VAR_ATTR_EXP) != ESP_VAR_ATTR_EXP) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Unexpected Var Attributes 0x%x (expected%x)\n",
      __FUNCTION__,
      VarAttr,
      ESP_VAR_ATTR_EXP
      ));
    Status = EFI_INVALID_PARAMETER;
    goto exit;
  }

  if (VarSize == 0) {
    VarData = NULL;
  }

  Status = gRT->SetVariable (VarName, &EfiVarGuid, VarAttr, VarSize, VarData);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Set variable %s %r\r\n",
      __FUNCTION__,
      VarName,
      Status
      ));
    goto exit;
  }

exit:
  if (FileHandleDelete (File) != EFI_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to delete File %s\n",
      __FUNCTION__,
      FileInfo->FileName
      ));
  }

  if (FileData != NULL) {
    FreePool (FileData);
  }

  return Status;
}

/**
  Locate the EFI System Partition Variables and Process them.

*/
STATIC
EFI_STATUS
GetAndProcessEspVariables (
  VOID
  )
{
  EFI_STATUS                       Status;
  EFI_FILE_HANDLE                  DirHandle;
  EFI_HANDLE                       EspDeviceHandle;
  BOOLEAN                          NoFile;
  EFI_FILE_INFO                    *FileInfo;
  UINTN                            HandleSize;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *Fs;
  EFI_FILE_HANDLE                  RootDir;

  HandleSize = sizeof (EFI_HANDLE);
  Status     = gBS->LocateHandle (
                      ByProtocol,
                      &gEfiPartTypeSystemPartGuid,
                      NULL,
                      &HandleSize,
                      &EspDeviceHandle
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Locate System Partition Guid %r\n", Status));
    return Status;
  }

  Status = gBS->HandleProtocol (
                  EspDeviceHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&Fs
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to find FS %r\n", Status));
    return Status;
  }

  Status = Fs->OpenVolume (Fs, &RootDir);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to open FS %r\n", Status));
    return Status;
  }

  Status = RootDir->Open (
                      RootDir,
                      &DirHandle,
                      ESP_VAR_DIR_PATH,
                      EFI_FILE_MODE_READ,
                      0
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Can't find ESP Variables Dir %r\n",
      __FUNCTION__,
      Status
      ));
    RootDir->Close (RootDir);
    return Status;
  }

  NoFile   = FALSE;
  FileInfo = NULL;

  for (Status = FileHandleFindFirstFile (DirHandle, &FileInfo);
       !EFI_ERROR (Status) && !NoFile;
       Status = FileHandleFindNextFile (DirHandle, FileInfo, &NoFile))
  {
    // Process each Variable
    if ((StrnCmp (FileInfo->FileName, L".", VARIABLE_MAX_NAME) == 0) ||
        (StrnCmp (FileInfo->FileName, L"..", VARIABLE_MAX_NAME) == 0))
    {
      continue;
    }

    Status = ProcessEspVariable (DirHandle, FileInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to process %s\n",
        __FUNCTION__,
        FileInfo->FileName
        ));
      continue;
    }
  }

  RootDir->Close (RootDir);
  return Status;
}

/**
  Update special variables

**/
STATIC
VOID
UpdateSpecialVariables (
  VOID
  )
{
  LockVariable (
    &gNVIDIAPublicVariableGuid,
    L"TegraPlatformSpec",
    VARIABLE_POLICY_TYPE_LOCK_ON_CREATE
    );
  LockVariable (
    &gNVIDIAPublicVariableGuid,
    L"TegraPlatformCompatSpec",
    VARIABLE_POLICY_TYPE_LOCK_ON_CREATE
    );
}

/**
  Callback when variable protocol is ready.

  @param  Event             Event that was called.
  @param  Context           Context structure.

**/
STATIC
VOID
VariableReady (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS   Status;
  VOID         *Dtb;
  UINTN        DtbSize;
  INT32        NodeOffset;
  INT32        SubNodeOffset = 0;
  INT32        VariableNodeOffset;
  EFI_GUID     *VariableGuid;
  EFI_GUID     DtbGuid;
  CONST CHAR8  *NodeName;
  CONST CHAR8  *GuidStr;
  VOID         *Protocol;
  BOOLEAN      GuidBased;

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

  UpdateSpecialVariables ();

  Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get dtb - %r\r\n", Status));
    goto EspVar;
  }

  NodeOffset = fdt_path_offset (Dtb, VARIABLE_NODE_PATH);
  if (NodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to get Variable Node - %r\r\n", Status));
    goto EspVar;
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
    } else if (AsciiStrCmp (NodeName, "gNVIDIATokenSpaceGuid") == 0) {
      VariableGuid = &gNVIDIATokenSpaceGuid;
    } else if (AsciiStrCmp (NodeName, "gEfiImageSecurityDatabaseGuid") == 0) {
      VariableGuid = &gEfiImageSecurityDatabaseGuid;
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

EspVar:
  Status = GetAndProcessEspVariables ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Process ESP Partition Variables %r\n",
      __FUNCTION__,
      Status
      ));
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
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_EVENT  NotifyEvent;
  EFI_EVENT  NotifyEventPolicy;

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
