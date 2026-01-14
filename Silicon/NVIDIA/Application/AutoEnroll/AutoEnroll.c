#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/GlobalVariable.h>                 // gEfiGlobalVariableGuid (PK, KEK, Boot####, BootOrder)
#include <Guid/ImageAuthentication.h>            // gEfiImageSecurityDatabaseGuid (db, dbx, dbr)
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DevicePath.h>

#define KEYS_DIR           L"\\keys\\"
#define MAX_KEYFILE_BYTES  (4*1024*1024)

// Properties Secure Boot (Time-based Auth Write!)
#define SB_ATTR (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | \
                 EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS)

// Auto boot setup target
#define TARGET_LOADER_PATH  L"\\EFI\\BOOT\\firstLoader.efi"
#define TARGET_DESC         L"FirstLoader"
#define BOOTVAR_ATTR (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)
#define LOAD_OPTION_ACTIVE  0x00000001

typedef struct {
  CHAR16      *VarName;
  EFI_GUID    *VarGuid;
} VAR_DESC;

// Mô tả biến cần enroll
STATIC VAR_DESC VarDb  = { L"db",  (EFI_GUID*)&gEfiImageSecurityDatabaseGuid };
STATIC VAR_DESC VarDbx = { L"dbx", (EFI_GUID*)&gEfiImageSecurityDatabaseGuid };
STATIC VAR_DESC VarKEK = { L"KEK", (EFI_GUID*)&gEfiGlobalVariableGuid };
STATIC VAR_DESC VarPK  = { L"PK",  (EFI_GUID*)&gEfiGlobalVariableGuid };

//
// Utilities
//
STATIC
EFI_STATUS
ReadFileToBuffer(
  IN  EFI_HANDLE                 ImageHandle,
  IN  CHAR16                    *FullPath,
  OUT VOID                     **Buffer,
  OUT UINTN                     *BufferSize
  )
{
  EFI_STATUS                       Status;
  EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfsp = NULL;
  EFI_FILE_PROTOCOL               *Root = NULL;
  EFI_FILE_PROTOCOL               *File = NULL;
  EFI_FILE_INFO                   *Info = NULL;

  *Buffer = NULL;
  *BufferSize = 0;

  Status = gBS->OpenProtocol(ImageHandle,
                             &gEfiLoadedImageProtocolGuid,
                             (VOID**)&LoadedImage,
                             ImageHandle,
                             NULL,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(Status)) return Status;

  Status = gBS->HandleProtocol(LoadedImage->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid,
                               (VOID**)&Sfsp);
  if (EFI_ERROR(Status)) return Status;

  Status = Sfsp->OpenVolume(Sfsp, &Root);
  if (EFI_ERROR(Status)) return Status;

  Status = Root->Open(Root, &File, FullPath, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(Status)) {
    Root->Close(Root);
    return Status;
  }

  // Get file size
  Info = FileHandleGetInfo(File);
  if (Info == NULL) {
    File->Close(File);
    Root->Close(Root);
    return EFI_NOT_FOUND;
  }

  if (Info->FileSize == 0 || Info->FileSize > MAX_KEYFILE_BYTES) {
    FreePool(Info);
    File->Close(File);
    Root->Close(Root);
    return EFI_BAD_BUFFER_SIZE;
  }

  *BufferSize = (UINTN)Info->FileSize;
  *Buffer = AllocateZeroPool(*BufferSize);
  if (*Buffer == NULL) {
    FreePool(Info);
    File->Close(File);
    Root->Close(Root);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = File->Read(File, BufferSize, *Buffer);
  FreePool(Info);
  File->Close(File);
  Root->Close(Root);
  return Status;
}

STATIC
EFI_STATUS
EnrollVariable(
  IN VAR_DESC  *Desc,
  IN VOID      *Data,
  IN UINTN      DataSize
  )
{
  EFI_STATUS Status;
  Status = gRT->SetVariable(
              Desc->VarName,
              Desc->VarGuid,
              SB_ATTR,
              DataSize,
              Data
           );
  return Status;
}

STATIC
VOID
PrintSbState(VOID)
{
  EFI_STATUS Status;
  UINT8      SetupMode = 0xFF;
  UINTN      Size = sizeof(SetupMode);

  Status = gRT->GetVariable(L"SetupMode", &gEfiGlobalVariableGuid, NULL, &Size, &SetupMode);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] SetupMode: <unknown> (Status=%r)\r\n", Status);
  } else {
    Print(L"[AutoEnroll] SetupMode: %u (1=SetupMode, 0=UserMode)\r\n", (UINTN)SetupMode);
  }

  UINT8 SecureBoot = 0xFF;
  Size = sizeof(SecureBoot);
  Status = gRT->GetVariable(L"SecureBoot", &gEfiGlobalVariableGuid, NULL, &Size, &SecureBoot);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] SecureBoot: <unknown> (Status=%r)\r\n", Status);
  } else {
    Print(L"[AutoEnroll] SecureBoot: %u\r\n", (UINTN)SecureBoot);
  }
}

STATIC
EFI_STATUS
TryEnrollFromOneFile(
  IN EFI_HANDLE  ImageHandle,
  IN VAR_DESC   *Desc,
  IN CHAR16     *Path
  )
{
  EFI_STATUS Status;
  VOID      *Buf = NULL;
  UINTN      Sz  = 0;

  Status = ReadFileToBuffer(ImageHandle, Path, &Buf, &Sz);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = EnrollVariable(Desc, Buf, Sz);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] SetVariable %s failed for %s: %r\r\n", Desc->VarName, Path, Status);
  } else {
    Print(L"[AutoEnroll] Enrolled %s from %s (%u bytes)\r\n", Desc->VarName, Path, (UINTN)Sz);
  }
  if (Buf) FreePool(Buf);
  return Status;
}

STATIC
EFI_STATUS
TryEnrollByPattern(
  IN EFI_HANDLE  ImageHandle,
  IN VAR_DESC   *Desc,
  IN CHAR16     *Dir,         // e.g. L"\\keys\\"
  IN CHAR16     *BaseName     // e.g. L"db", "KEK", "PK", "dbx"
  )
{
  EFI_STATUS Status;
  CHAR16     Path[256];

  // Prefer .auth (Time-based Authenticated Variable)
  UnicodeSPrint(Path, sizeof(Path), L"%s%s.auth", Dir, BaseName);
  Status = TryEnrollFromOneFile(ImageHandle, Desc, Path);
  if (!EFI_ERROR(Status)) return Status;

  // Fall back .esl (valid only in SetupMode typically)
  UnicodeSPrint(Path, sizeof(Path), L"%s%s.esl", Dir, BaseName);
  Status = TryEnrollFromOneFile(ImageHandle, Desc, Path);
  if (!EFI_ERROR(Status)) return Status;

  // Try uppercase extensions
  UnicodeSPrint(Path, sizeof(Path), L"%s%s.AUTH", Dir, BaseName);
  Status = TryEnrollFromOneFile(ImageHandle, Desc, Path);
  if (!EFI_ERROR(Status)) return Status;

  UnicodeSPrint(Path, sizeof(Path), L"%s%s.ESL", Dir, BaseName);
  Status = TryEnrollFromOneFile(ImageHandle, Desc, Path);
  return Status;
}

//
// --- Boot Manager automation (replace: bcfg boot add/mv + reset) ---
//

STATIC
EFI_STATUS
FileExistsOnVolume(
  IN EFI_HANDLE  VolumeHandle,
  IN CHAR16     *AbsolutePath
  )
{
  EFI_STATUS                       Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfs;
  EFI_FILE_PROTOCOL               *Root;
  EFI_FILE_PROTOCOL               *File;

  Status = gBS->HandleProtocol(VolumeHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Sfs);
  if (EFI_ERROR(Status)) return Status;

  Status = Sfs->OpenVolume(Sfs, &Root);
  if (EFI_ERROR(Status)) return Status;

  File = NULL;
  Status = Root->Open(Root, &File, AbsolutePath, EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR(Status) && File != NULL) {
    File->Close(File);
    Root->Close(Root);
    return EFI_SUCCESS;
  }

  Root->Close(Root);
  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
FindVolumeContainingFile(
  IN  CHAR16     *AbsolutePath,
  OUT EFI_HANDLE *FoundHandle
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE *Handles;
  UINTN       HandleCount;
  UINTN       i;

  if (FoundHandle == NULL) return EFI_INVALID_PARAMETER;
  *FoundHandle = NULL;

  Handles = NULL;
  HandleCount = 0;

  Status = gBS->LocateHandleBuffer(
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR(Status)) return Status;

  for (i = 0; i < HandleCount; i++) {
    Status = FileExistsOnVolume(Handles[i], AbsolutePath);
    if (!EFI_ERROR(Status)) {
      *FoundHandle = Handles[i];
      FreePool(Handles);
      return EFI_SUCCESS;
    }
  }

  FreePool(Handles);
  return EFI_NOT_FOUND;
}

STATIC
BOOLEAN
IsBootOptionUsed(
  IN UINT16 BootNum
  )
{
  EFI_STATUS Status;
  CHAR16     VarName[12];
  UINTN      Size;

  UnicodeSPrint(VarName, sizeof(VarName), L"Boot%04x", BootNum);

  Size = 0;
  Status = gRT->GetVariable(VarName, &gEfiGlobalVariableGuid, NULL, &Size, NULL);
  return (Status == EFI_BUFFER_TOO_SMALL);
}

STATIC
UINT16
AllocateFreeBootNumber(VOID)
{
  UINT32 Try;
  for (Try = 0; Try <= 0xFFFF; Try++) {
    if (!IsBootOptionUsed((UINT16)Try)) {
      return (UINT16)Try;
    }
  }
  return 0xFFFF;
}

STATIC
EFI_STATUS
GetBootOrder(
  OUT UINT16 **BootOrder,
  OUT UINTN  *BootOrderCount
  )
{
  EFI_STATUS Status;
  UINTN      Size;
  UINT16    *Buf;

  if (BootOrder == NULL || BootOrderCount == NULL) return EFI_INVALID_PARAMETER;

  *BootOrder = NULL;
  *BootOrderCount = 0;

  Size = 0;
  Status = gRT->GetVariable(L"BootOrder", &gEfiGlobalVariableGuid, NULL, &Size, NULL);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS; // empty
  }
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  Buf = AllocateZeroPool(Size);
  if (Buf == NULL) return EFI_OUT_OF_RESOURCES;

  Status = gRT->GetVariable(L"BootOrder", &gEfiGlobalVariableGuid, NULL, &Size, Buf);
  if (EFI_ERROR(Status)) {
    FreePool(Buf);
    return Status;
  }

  *BootOrder = Buf;
  *BootOrderCount = Size / sizeof(UINT16);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetBootOrder(
  IN UINT16 *BootOrder,
  IN UINTN   BootOrderCount
  )
{
  return gRT->SetVariable(
              L"BootOrder",
              &gEfiGlobalVariableGuid,
              BOOTVAR_ATTR,
              BootOrderCount * sizeof(UINT16),
              BootOrder
           );
}

STATIC
EFI_STATUS
PrependBootOrderUnique(
  IN UINT16 BootNum
  )
{
  EFI_STATUS Status;
  UINT16    *OldOrder;
  UINTN      OldCount;

  UINT16    *NewOrder;
  UINTN      out;
  UINTN      i;

  Status = GetBootOrder(&OldOrder, &OldCount);
  if (EFI_ERROR(Status)) return Status;

  NewOrder = AllocateZeroPool((OldCount + 1) * sizeof(UINT16));
  if (NewOrder == NULL) {
    if (OldOrder) FreePool(OldOrder);
    return EFI_OUT_OF_RESOURCES;
  }

  out = 0;
  NewOrder[out++] = BootNum;

  for (i = 0; i < OldCount; i++) {
    if (OldOrder[i] == BootNum) continue;
    NewOrder[out++] = OldOrder[i];
  }

  Status = SetBootOrder(NewOrder, out);

  if (OldOrder) FreePool(OldOrder);
  FreePool(NewOrder);
  return Status;
}

STATIC
EFI_STATUS
SetBootNext(
  IN UINT16 BootNum
  )
{
  return gRT->SetVariable(
              L"BootNext",
              &gEfiGlobalVariableGuid,
              BOOTVAR_ATTR,
              sizeof(UINT16),
              &BootNum
           );
}

STATIC
EFI_STATUS
ReadBootOption(
  IN  UINT16  BootNum,
  OUT VOID   **Buffer,
  OUT UINTN   *BufferSize
  )
{
  EFI_STATUS Status;
  CHAR16     VarName[12];
  UINTN      Size;
  VOID      *Buf;

  if (Buffer == NULL || BufferSize == NULL) return EFI_INVALID_PARAMETER;
  *Buffer = NULL;
  *BufferSize = 0;

  UnicodeSPrint(VarName, sizeof(VarName), L"Boot%04x", BootNum);

  Size = 0;
  Status = gRT->GetVariable(VarName, &gEfiGlobalVariableGuid, NULL, &Size, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) return Status;

  Buf = AllocateZeroPool(Size);
  if (Buf == NULL) return EFI_OUT_OF_RESOURCES;

  Status = gRT->GetVariable(VarName, &gEfiGlobalVariableGuid, NULL, &Size, Buf);
  if (EFI_ERROR(Status)) {
    FreePool(Buf);
    return Status;
  }

  *Buffer = Buf;
  *BufferSize = Size;
  return EFI_SUCCESS;
}

STATIC
CHAR16*
GetLoadOptionDescriptionPtr(
  IN VOID  *LoadOption,
  IN UINTN  LoadOptionSize
  )
{
  UINT8 *Ptr;
  UINTN Min;

  // Must contain Attributes (4) + FilePathListLength (2) at least
  Min = sizeof(UINT32) + sizeof(UINT16);
  if (LoadOption == NULL || LoadOptionSize < Min) return NULL;

  Ptr = (UINT8*)LoadOption + Min;
  // Description is a NUL-terminated CHAR16 string starting here
  return (CHAR16*)Ptr;
}

STATIC
BOOLEAN
DescriptionEquals(
  IN CHAR16 *Desc
  )
{
  if (Desc == NULL) return FALSE;
  return (StrCmp(Desc, TARGET_DESC) == 0);
}

STATIC
EFI_STATUS
FindExistingBootOptionInBootOrderByDescription(
  OUT BOOLEAN *Found,
  OUT UINT16  *BootNumOut
  )
{
  EFI_STATUS Status;
  UINT16    *Order;
  UINTN      Count;
  UINTN      i;

  if (Found == NULL || BootNumOut == NULL) return EFI_INVALID_PARAMETER;
  *Found = FALSE;
  *BootNumOut = 0;

  Status = GetBootOrder(&Order, &Count);
  if (EFI_ERROR(Status)) return Status;

  for (i = 0; i < Count; i++) {
    VOID  *Opt;
    UINTN  OptSize;
    CHAR16 *Desc;

    Opt = NULL;
    OptSize = 0;

    Status = ReadBootOption(Order[i], &Opt, &OptSize);
    if (EFI_ERROR(Status)) {
      continue; // ignore unreadable entry
    }

    Desc = GetLoadOptionDescriptionPtr(Opt, OptSize);
    if (Desc != NULL && DescriptionEquals(Desc)) {
      *Found = TRUE;
      *BootNumOut = Order[i];
      FreePool(Opt);
      break;
    }

    FreePool(Opt);
  }

  if (Order) FreePool(Order);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
BuildLoadOptionBuffer(
  IN  EFI_HANDLE  VolumeHandle,
  IN  CHAR16     *LoaderPath,
  IN  CHAR16     *Description,
  OUT UINT8     **OptionBuffer,
  OUT UINTN      *OptionSize
  )
{
  EFI_DEVICE_PATH_PROTOCOL *FileDp;
  UINTN                    FileDpSize;
  UINTN                    DescSize;
  UINTN                    TotalSize;

  UINT8   *Buf;
  UINT8   *Ptr;

  UINT32  Attributes;
  UINT16  FilePathListLength;

  if (OptionBuffer == NULL || OptionSize == NULL || LoaderPath == NULL || Description == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *OptionBuffer = NULL;
  *OptionSize   = 0;

  FileDp = FileDevicePath(VolumeHandle, LoaderPath);
  if (FileDp == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  FileDpSize = GetDevicePathSize(FileDp);
  DescSize   = StrSize(Description); // includes NUL

  Attributes = LOAD_OPTION_ACTIVE;
  FilePathListLength = (UINT16)FileDpSize;

  // EFI_LOAD_OPTION:
  // UINT32 Attributes
  // UINT16 FilePathListLength
  // CHAR16 Description[] (NUL-terminated)
  // EFI_DEVICE_PATH_PROTOCOL FilePathList[]
  TotalSize = sizeof(UINT32) + sizeof(UINT16) + DescSize + FileDpSize;

  Buf = AllocateZeroPool(TotalSize);
  if (Buf == NULL) {
    FreePool(FileDp);
    return EFI_OUT_OF_RESOURCES;
  }

  Ptr = Buf;
  CopyMem(Ptr, &Attributes, sizeof(UINT32));
  Ptr += sizeof(UINT32);

  CopyMem(Ptr, &FilePathListLength, sizeof(UINT16));
  Ptr += sizeof(UINT16);

  CopyMem(Ptr, Description, DescSize);
  Ptr += DescSize;

  CopyMem(Ptr, FileDp, FileDpSize);

  FreePool(FileDp);

  *OptionBuffer = Buf;
  *OptionSize   = TotalSize;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetBootOptionVariable(
  IN UINT16 BootNum,
  IN UINT8  *OptionBuffer,
  IN UINTN   OptionSize
  )
{
  CHAR16 VarName[12];
  UnicodeSPrint(VarName, sizeof(VarName), L"Boot%04x", BootNum);

  return gRT->SetVariable(
              VarName,
              &gEfiGlobalVariableGuid,
              BOOTVAR_ATTR,
              OptionSize,
              OptionBuffer
           );
}

STATIC
EFI_STATUS
AutoSetupBootForFirstLoader(
  VOID
  )
{
  EFI_STATUS Status;
  EFI_HANDLE VolumeHandle;
  UINT16     BootNum;
  BOOLEAN    FoundExisting;

  UINT8     *OptionBuf;
  UINTN      OptionSize;

  VolumeHandle = NULL;

  // 1) Find volume containing the loader
  Status = FindVolumeContainingFile(TARGET_LOADER_PATH, &VolumeHandle);
  if (EFI_ERROR(Status) || VolumeHandle == NULL) {
    Print(L"[AutoEnroll] Cannot find %s on any filesystem: %r\r\n", TARGET_LOADER_PATH, Status);
    return Status;
  }
  Print(L"[AutoEnroll] Found %s on a filesystem volume.\r\n", TARGET_LOADER_PATH);

  // 2) Try reuse an existing Boot option in BootOrder with same description
  FoundExisting = FALSE;
  BootNum = 0;

  Status = FindExistingBootOptionInBootOrderByDescription(&FoundExisting, &BootNum);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] Warning: cannot scan BootOrder for existing option: %r\r\n", Status);
    FoundExisting = FALSE;
  }

  if (!FoundExisting) {
    // 3) Allocate a free Boot#### and create it
    BootNum = AllocateFreeBootNumber();
    if (BootNum == 0xFFFF) {
      Print(L"[AutoEnroll] No free Boot#### number available.\r\n");
      return EFI_OUT_OF_RESOURCES;
    }

    OptionBuf = NULL;
    OptionSize = 0;

    Status = BuildLoadOptionBuffer(VolumeHandle, TARGET_LOADER_PATH, TARGET_DESC, &OptionBuf, &OptionSize);
    if (EFI_ERROR(Status)) {
      Print(L"[AutoEnroll] BuildLoadOptionBuffer failed: %r\r\n", Status);
      return Status;
    }

    Status = SetBootOptionVariable(BootNum, OptionBuf, OptionSize);
    FreePool(OptionBuf);

    if (EFI_ERROR(Status)) {
      Print(L"[AutoEnroll] Set Boot%04x failed: %r\r\n", BootNum, Status);
      return Status;
    }

    Print(L"[AutoEnroll] Created Boot%04x for %s\r\n", BootNum, TARGET_DESC);
  } else {
    Print(L"[AutoEnroll] Reusing existing Boot%04x (Description=%s)\r\n", BootNum, TARGET_DESC);
  }

  // 4) Prepend to BootOrder and set BootNext
  Status = PrependBootOrderUnique(BootNum);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] Update BootOrder failed: %r\r\n", Status);
    return Status;
  }
  Print(L"[AutoEnroll] BootOrder updated (Boot%04x is first).\r\n", BootNum);

  Status = SetBootNext(BootNum);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] Set BootNext failed: %r\r\n", Status);
    return Status;
  }
  Print(L"[AutoEnroll] BootNext set to Boot%04x\r\n", BootNum);

  return EFI_SUCCESS;
}

//
// UefiMain
//
EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status = EFI_SUCCESS;
  CHAR16    *Dir = KEYS_DIR;

  Print(L"[AutoEnroll] Automatic enroll (db -> dbx -> KEK -> PK), folder=%s\r\n", Dir);
  PrintSbState();

  // 1) db
  Status = TryEnrollByPattern(ImageHandle, &VarDb, Dir, L"db");
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] db: no file or failed (%r). Continuing.\r\n", Status);
  }

  // 2) dbx (optional)
  Status = TryEnrollByPattern(ImageHandle, &VarDbx, Dir, L"dbx");
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] dbx: no file or failed (%r). Continuing.\r\n", Status);
  }

  // 3) KEK
  Status = TryEnrollByPattern(ImageHandle, &VarKEK, Dir, L"KEK");
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] KEK: no file or failed (%r). Continuing.\r\n", Status);
  }

  // 4) PK (should be LAST; enrolling PK often transitions SetupMode->UserMode)
  Status = TryEnrollByPattern(ImageHandle, &VarPK, Dir, L"PK");
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] PK: no file or failed (%r).\r\n", Status);
  }

  PrintSbState();

  // 5) Auto setup Boot entry for firstLoader + BootOrder/BootNext
  Status = AutoSetupBootForFirstLoader();
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] Boot automation failed: %r\r\n", Status);
    Print(L"[AutoEnroll] Done (no reset).\r\n");
    return Status;
  }

  // 6) Reset to apply / boot into firstLoader
  Print(L"[AutoEnroll] Done. Resetting system...\r\n");
  gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

  return EFI_SUCCESS; // Should not return
}
