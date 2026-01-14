//
// Uefi1.c - First-stage UEFI loader with encrypted secondLoader
//
// Chain:
//   Firmware -> firstLoader.efi (signed, in ESP)
//            -> secondLoader.enc (encrypted with AES-256-GCM, signed-then-encrypted)
//            -> secondLoader.efi (decrypted in memory) -> kernel Image
//
// firstLoader.efi does:
//   1) Find the filesystem where \EFI\BOOT\secondLoader.enc resides (usually same ESP).
//   2) Load secondLoader.enc into memory.
//   3) Decrypt the encrypted buffer into a plain PE/COFF image (secondLoader.efi) using AES-256-GCM.
//   4) Build a MemMap Device Path (DP) describing that plain buffer.
//   5) LoadImage() + StartImage() secondLoader from that MemMap DP.
//
// NOTE: Uses AES-256-GCM encryption for secure encryption with authentication.
//       Format: IV(12 bytes) + Tag(16 bytes) + Ciphertext
//

#include <Uefi.h>
//#include <Protocol/DevicePath.h>
#include <Guid/FileInfo.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <IndustryStandard/PeImage.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/GlobalVariable.h>
#include <Guid/ImageAuthentication.h>
#include <Guid/FileInfo.h>
#include <Library/FileHandleLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseCryptLib.h> // AeadAesGcmDecrypt, Sha256HashAll
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>

#define UEFI2_ENC_PATH L"\\EFI\\BOOT\\secondLoader.enc"

#pragma pack(push, 1)
typedef struct {
    MEMMAP_DEVICE_PATH       MemMap;
    EFI_DEVICE_PATH_PROTOCOL End;
} MEMMAP_DEVICE_PATH_WITH_END;
#pragma pack(pop)

//
// AES-256-GCM key (32 bytes = 256 bits)
// This key must match the key in aes256gcm_encrypt.py
//
STATIC CONST UINT8 gUefi2Aes256Key[] = {
    0x3A, 0x7F, 0x21, 0x5C, 0x99, 0xDE, 0x42, 0x10,
    0xAB, 0xCD, 0x01, 0x23, 0x45, 0x67, 0x89, 0xFE,
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
};

#define AES256_KEY_SIZE    32
#define AES256_GCM_IV_SIZE 12
#define AES256_GCM_TAG_SIZE 16

//
// Load an entire file into an allocated buffer from the given Root.
//
static EFI_STATUS
LoadFileToBuffer(
    IN  EFI_FILE_PROTOCOL  *Root,
    IN  CONST CHAR16       *Path,
    OUT VOID               **Buffer,
    OUT UINTN              *BufferSize
    )
{
    EFI_STATUS         Status;
    EFI_FILE_PROTOCOL *File;
    EFI_FILE_INFO     *FileInfo = NULL;
    UINTN              InfoSize = 0;

    *Buffer     = NULL;
    *BufferSize = 0;

    Status = Root->Open(
        Root,
        &File,
        (CHAR16 *)Path,
        EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] LoadFileToBuffer: failed to open %s: %r\n", Path, Status);
        return Status;
    }

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"[uefi1] GetInfo(size) failed for %s: %r\n", Path, Status);
        File->Close(File);
        return Status;
    }

    Status = gBS->AllocatePool(EfiLoaderData, InfoSize, (VOID **)&FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] AllocatePool(FileInfo) failed: %r\n", Status);
        File->Close(File);
        return Status;
    }

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] GetInfo(info) failed for %s: %r\n", Path, Status);
        gBS->FreePool(FileInfo);
        File->Close(File);
        return Status;
    }

    *BufferSize = (UINTN)FileInfo->FileSize;
    gBS->FreePool(FileInfo);

    Status = gBS->AllocatePool(EfiLoaderData, *BufferSize, Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] AllocatePool(file buffer) failed: %r\n", Status);
        File->Close(File);
        return Status;
    }

    Status = File->Read(File, BufferSize, *Buffer);
    File->Close(File);

    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] Read() failed for %s: %r\n", Path, Status);
        gBS->FreePool(*Buffer);
        *Buffer     = NULL;
        *BufferSize = 0;
        return Status;
    }

    return EFI_SUCCESS;
}

//
// AES-256-GCM decryption.
// EncryptedBuffer format: IV(12 bytes) + Tag(16 bytes) + Ciphertext(variable)
// DecryptedBuffer will be allocated inside this function.
//

// Assumes:
// - AES256_GCM_IV_SIZE  = 12
// - AES256_GCM_TAG_SIZE = 16
// - AES256_KEY_SIZE     = 32
// - gUefi2Aes256Key is 32 bytes
static EFI_STATUS
DecryptUefi2 (
  IN  CONST UINT8 *EncryptedBuffer,
  IN  UINTN        EncryptedSize,
  OUT UINT8      **DecryptedBuffer
  )
{
  EFI_STATUS   Status;
  CONST UINT8 *Iv;
  CONST UINT8 *Tag;
  CONST UINT8 *Ciphertext;
  UINTN        CiphertextSize;
  UINTN        PlaintextSize;
  UINTN        OutSize;
  BOOLEAN      Ok;

  *DecryptedBuffer = NULL;

  if (EncryptedBuffer == NULL || DecryptedBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (EncryptedSize < (AES256_GCM_IV_SIZE + AES256_GCM_TAG_SIZE)) {
    Print(L"[uefi1] DecryptUefi2: EncryptedSize too small: %u bytes\n", (UINT32)EncryptedSize);
    return EFI_INVALID_PARAMETER;
  }

  Iv           = EncryptedBuffer;
  Tag          = EncryptedBuffer + AES256_GCM_IV_SIZE;
  Ciphertext   = EncryptedBuffer + AES256_GCM_IV_SIZE + AES256_GCM_TAG_SIZE;
  CiphertextSize = EncryptedSize - AES256_GCM_IV_SIZE - AES256_GCM_TAG_SIZE;

  PlaintextSize = CiphertextSize;
  Status = gBS->AllocatePool(EfiLoaderData, PlaintextSize, (VOID **)DecryptedBuffer);
  if (EFI_ERROR(Status)) {
    Print(L"[uefi1] DecryptUefi2: AllocatePool failed: %r\n", Status);
    return Status;
  }

  OutSize = PlaintextSize;

  // AeadAesGcmDecrypt signature (Key, KeySize, IV, IVSize, AAD, AADSize, In, InSize, Tag, TagSize, Out, OutSize)
  Ok = AeadAesGcmDecrypt(
         gUefi2Aes256Key, AES256_KEY_SIZE,
         Iv, AES256_GCM_IV_SIZE,
         NULL, 0,                         // No AAD
         Ciphertext, CiphertextSize,
         Tag, AES256_GCM_TAG_SIZE,
         *DecryptedBuffer, &OutSize
       );
  if (!Ok) {
    Print(L"[uefi1] DecryptUefi2: AeadAesGcmDecrypt failed (TAG mismatch?)\n");
    gBS->FreePool(*DecryptedBuffer);
    *DecryptedBuffer = NULL;
    return EFI_SECURITY_VIOLATION;
  }

  if (OutSize != PlaintextSize) {
    Print(L"[uefi1] DecryptUefi2: Warning plaintext size mismatch: got=%u expected=%u\n",
          (UINT32)OutSize, (UINT32)PlaintextSize);
  }

  Print(L"[uefi1] DecryptUefi2: decrypted %u bytes using BaseCryptLib AES-256-GCM\n", (UINT32)OutSize);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiMain (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
    )
{
    EFI_STATUS                       Status;
    EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfsp;
    EFI_FILE_PROTOCOL               *Root;

    VOID                            *EncBuffer      = NULL;
    UINTN                            EncSize        = 0;
    UINT8                           *PlainBuffer    = NULL;
    UINTN                            PlainSize      = 0;
    MEMMAP_DEVICE_PATH_WITH_END      MemDp;
    EFI_HANDLE                       Uefi2Handle    = NULL;

    Print(L"[uefi1] UefiMain() start. Loading encrypted %s via MemMap DP\n",
          UEFI2_ENC_PATH);

    //
    // Get LOADED_IMAGE for this uefi1.efi
    //
    Status = gBS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImage
    );
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] HandleProtocol(LoadedImage) failed: %r\n", Status);
        return Status;
    }

    //
    // Get filesystem where uefi1.efi resides (typically the ESP, e.g. FS2:)
    //
    Status = gBS->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&Sfsp
    );
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] HandleProtocol(SimpleFileSystem) failed: %r\n", Status);
        return Status;
    }

    Status = Sfsp->OpenVolume(Sfsp, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] OpenVolume() failed: %r\n", Status);
        return Status;
    }

    //
    // Load the encrypted uefi2 image into memory
    //
    Status = LoadFileToBuffer(Root, UEFI2_ENC_PATH, &EncBuffer, &EncSize);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] Failed to load encrypted %s: %r\n", UEFI2_ENC_PATH, Status);
        return Status;
    }

    Print(L"[uefi1] Loaded encrypted %s at 0x%lx, size %u bytes\n",
          UEFI2_ENC_PATH, (UINT64)(UINTN)EncBuffer, (UINT32)EncSize);

    //
    // Decrypt the encrypted buffer into a plain PE/COFF image (uefi2.efi)
    // Format: IV(12) + Tag(16) + Ciphertext
    //
    Status = DecryptUefi2((CONST UINT8 *)EncBuffer, EncSize, &PlainBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] DecryptUefi2() failed: %r\n", Status);
        gBS->FreePool(EncBuffer);
        return Status;
    }

    // PlainSize is the size of the decrypted plaintext
    // For GCM, plaintext size equals ciphertext size (no padding)
    PlainSize = EncSize - AES256_GCM_IV_SIZE - AES256_GCM_TAG_SIZE;

    // We can free the encrypted buffer now if we want to save memory
    gBS->FreePool(EncBuffer);
    EncBuffer = NULL;

    //
    // Build MemMap Device Path describing the in-memory decrypted image
    //
    MemDp.MemMap.Header.Type    = HARDWARE_DEVICE_PATH;
    MemDp.MemMap.Header.SubType = HW_MEMMAP_DP;

    UINT16 MemMapLength = (UINT16)sizeof(MEMMAP_DEVICE_PATH);
    MemDp.MemMap.Header.Length[0] = (UINT8)(MemMapLength & 0xFF);
    MemDp.MemMap.Header.Length[1] = (UINT8)((MemMapLength >> 8) & 0xFF);

    MemDp.MemMap.MemoryType      = EfiLoaderData;
    MemDp.MemMap.StartingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)PlainBuffer;
    MemDp.MemMap.EndingAddress   =
        MemDp.MemMap.StartingAddress + (PlainSize - 1);

    MemDp.End.Type      = END_DEVICE_PATH_TYPE;
    MemDp.End.SubType   = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    MemDp.End.Length[0] = (UINT8)sizeof(EFI_DEVICE_PATH_PROTOCOL);
    MemDp.End.Length[1] = 0;

    //
    // Load uefi2.efi from memory via MemMap DP. Secure Boot will still
    // verify the signature if the decrypted image is a valid signed PE/COFF.
    //
    Status = gBS->LoadImage(
        FALSE,
        ImageHandle,
        (EFI_DEVICE_PATH_PROTOCOL *)&MemDp,
        PlainBuffer,
        PlainSize,
        &Uefi2Handle
    );
    if (EFI_ERROR(Status)) {
        Print(L"[uefi1] LoadImage(uefi2 via MemMap DP) failed: %r\n", Status);
        return Status;
    }

    Print(L"[uefi1] LoadImage(uefi2) OK, starting...\n");

    Status = gBS->StartImage(
        Uefi2Handle,
        NULL,
        NULL
    );
    Print(L"[uefi1] StartImage(uefi2) returned: %r\n", Status);

    // Optionally wipe and free PlainBuffer after StartImage returns
    // (if you want to clear decrypted image from RAM).
    // for (UINTN i = 0; i < PlainSize; i++) {
    //     PlainBuffer[i] = 0;
    // }
    // gBS->FreePool(PlainBuffer);

    return Status;
}
