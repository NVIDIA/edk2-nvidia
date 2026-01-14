/*
    Uefi2.c - Second-stage UEFI loader (kernel + initrd via LINUX_EFI_INITRD_MEDIA_GUID)
    Chain:
      Firmware -> uefi1.efi (decrypts uefi2) -> uefi2.efi -> Linux kernel Image
    This loader does the following:
      1) Enumerates all Simple File System handles and finds the one
         that contains \boot\Image.
      2) Loads the Linux kernel Image from \boot\Image into memory.
      3) Loads an initrd from \boot\initrd into memory.
      4) Exposes the initrd via EFI_LOAD_FILE2 + LINUX_EFI_INITRD_MEDIA_GUID.
      5) Constructs a MemMap Device Path (DP) for the in-memory kernel.
      6) Calls LoadImage()/StartImage() with that MemMap DP.
      7) Sets the kernel command line via LOADED_IMAGE.LoadOptions.
*/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadFile2.h>

#include <Guid/FileInfo.h>
#include <Guid/LinuxEfiInitrdMedia.h>

#include <Library/UefiRuntimeServicesTableLib.h>

/* Path for kernel Image and initrd on the root filesystem */
#define KERNEL_PATH L"\\boot\\Image"
#define INITRD_PATH L"\\boot\\initrd"

#define BOOT_OS_VARIABLE_NAME  L"BootChainOsCurrent"
#define BOOT_FW_VARIABLE_NAME  L"BootChainFwCurrent"

#pragma pack(push, 1)
typedef struct {
    MEMMAP_DEVICE_PATH       MemMap;
    EFI_DEVICE_PATH_PROTOCOL End;
} MEMMAP_DEVICE_PATH_WITH_END;
#pragma pack(pop)

/* Device path used to expose initrd via LINUX_EFI_INITRD_MEDIA_GUID */
typedef struct {
    VENDOR_DEVICE_PATH       Vendor;
    EFI_DEVICE_PATH_PROTOCOL End;
} LINUX_INITRD_DEVICE_PATH;

static LINUX_INITRD_DEVICE_PATH mInitrdDevPath = {
    {
        {
            MEDIA_DEVICE_PATH,
            MEDIA_VENDOR_DP,
            { sizeof (VENDOR_DEVICE_PATH), 0 }
        },
        LINUX_EFI_INITRD_MEDIA_GUID
    },
    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 }
    }
};

/* Kernel command line. */
static CONST CHAR16 KernelCmdline[] =
    L"root=/dev/nvme0n1p1 rw rootwait rootdelay=10 rootfstype=ext4 "
    L"mminit_loglevel=4 "
    L"console=ttyTCU0,115200 "
    L"firmware_class.path=/etc/firmware "
    L"fbcon=map:0 net.ifnames=0 nospectre_bhb "
    L"video=efifb:off console=tty0";

/* Context for EFI_LOAD_FILE2 protocol (initrd provider) */
typedef struct {
    EFI_LOAD_FILE2_PROTOCOL Proto;
    VOID                   *InitrdBuffer;
    UINTN                   InitrdSize;
} INITRD_LOADFILE2_CTX;

static INITRD_LOADFILE2_CTX mInitrdLf2;

/* LoadFile2 callback: Linux EFI stub calls this to receive the initrd.*/
static EFI_STATUS EFIAPI
InitrdLoadFile(
    IN EFI_LOAD_FILE2_PROTOCOL  *This,
    IN EFI_DEVICE_PATH_PROTOCOL *FilePath   OPTIONAL,
    IN BOOLEAN                   BootPolicy,
    IN OUT UINTN                *BufferSize,
    IN VOID                     *Buffer      OPTIONAL
    )
{
    INITRD_LOADFILE2_CTX *Ctx = &mInitrdLf2;

    if (BufferSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (Buffer == NULL || *BufferSize < Ctx->InitrdSize) {
        *BufferSize = Ctx->InitrdSize;
        return EFI_BUFFER_TOO_SMALL;
    }

    CopyMem(Buffer, Ctx->InitrdBuffer, Ctx->InitrdSize);
    *BufferSize = Ctx->InitrdSize;

    return EFI_SUCCESS;
}

/* Load an entire file into an allocated buffer from the given Root.*/
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
        Print(L"[uefi2] LoadFileToBuffer: failed to open %s: %r\n", Path, Status);
        return Status;
    }

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"[uefi2] GetInfo(size) failed for %s: %r\n", Path, Status);
        File->Close(File);
        return Status;
    }

    Status = gBS->AllocatePool(EfiLoaderData, InfoSize, (VOID **)&FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] AllocatePool(FileInfo) failed: %r\n", Status);
        File->Close(File);
        return Status;
    }

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] GetInfo(info) failed for %s: %r\n", Path, Status);
        gBS->FreePool(FileInfo);
        File->Close(File);
        return Status;
    }

    *BufferSize = (UINTN)FileInfo->FileSize;
    gBS->FreePool(FileInfo);

    Status = gBS->AllocatePool(EfiLoaderData, *BufferSize, Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] AllocatePool(file buffer) failed: %r\n", Status);
        File->Close(File);
        return Status;
    }

    Status = File->Read(File, BufferSize, *Buffer);
    File->Close(File);

    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] Read() failed for %s: %r\n", Path, Status);
        gBS->FreePool(*Buffer);
        *Buffer     = NULL;
        *BufferSize = 0;
        return Status;
    }

    return EFI_SUCCESS;
}

/*
 Enumerate all Simple File System handles and find one that contains KERNEL_PATH.
 On success, RootOut is an open EFI_FILE_PROTOCOL for the root of that filesystem.
*/

static EFI_STATUS
FindBootFileSystem(
    OUT EFI_FILE_PROTOCOL **RootOut
    )
{
    EFI_STATUS                       Status;
    UINTN                            HandleCount = 0;
    EFI_HANDLE                      *HandleBuffer = NULL;
    UINTN                            Index;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfsp;
    EFI_FILE_PROTOCOL               *Root;
    EFI_FILE_PROTOCOL               *TestFile;

    *RootOut = NULL;

    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiSimpleFileSystemProtocolGuid,
        NULL,
        &HandleCount,
        &HandleBuffer
    );
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] LocateHandleBuffer(SimpleFS) failed: %r\n", Status);
        return Status;
    }

    for (Index = 0; Index < HandleCount; Index++) {
        Status = gBS->HandleProtocol(
            HandleBuffer[Index],
            &gEfiSimpleFileSystemProtocolGuid,
            (VOID**)&Sfsp
        );
        if (EFI_ERROR(Status)) {
            continue;
        }

        Status = Sfsp->OpenVolume(Sfsp, &Root);
        if (EFI_ERROR(Status)) {
            continue;
        }

        // Try to open \boot\Image on this filesystem
        Status = Root->Open(
            Root,
            &TestFile,
            KERNEL_PATH,
            EFI_FILE_MODE_READ,
            0
        );
        if (!EFI_ERROR(Status)) {
            // We found a filesystem that contains the kernel
            TestFile->Close(TestFile);
            *RootOut = Root;

            Print(L"[uefi2] Found %s on filesystem handle #%u\n",
                  KERNEL_PATH, Index);
            gBS->FreePool(HandleBuffer);
            return EFI_SUCCESS;
        }
        // This filesystem does not contain the kernel; continue searching
    }

    gBS->FreePool(HandleBuffer);
    Print(L"[uefi2] Could not find %s on any filesystem\n", KERNEL_PATH);
    return EFI_NOT_FOUND;
}

static EFI_STATUS
LoadAndStartKernelFromAnyFs(
    IN EFI_HANDLE ImageHandle
    )
{
    EFI_STATUS                   Status;
    EFI_FILE_PROTOCOL           *Root          = NULL;
    VOID                        *KernelBuffer  = NULL;
    UINTN                        KernelSize    = 0;
    VOID                        *InitrdBuffer  = NULL;
    UINTN                        InitrdSize    = 0;
    MEMMAP_DEVICE_PATH_WITH_END  KernelDp;
    EFI_HANDLE                   KernelHandle  = NULL;
    EFI_LOADED_IMAGE_PROTOCOL   *KernelLoadedImage;
    EFI_HANDLE                   InitrdHandle  = NULL;

    Print(L"[uefi2] LoadAndStartKernelFromAnyFs() entered\n");

    /* 1) Find the filesystem that contains \boot\Image */
    Print(L"[uefi2] Searching for filesystem containing %s\n", KERNEL_PATH);
    Status = FindBootFileSystem(&Root);
    if (EFI_ERROR(Status) || Root == NULL) {
        Print(L"[uefi2] FindBootFileSystem() failed: %r\n", Status);
        return Status;
    }

    /* 2) Load kernel Image file into memory */
    Status = LoadFileToBuffer(Root, KERNEL_PATH, &KernelBuffer, &KernelSize);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] Failed to load kernel %s: %r\n", KERNEL_PATH, Status);
        return Status;
    }

    Print(L"[uefi2] Loaded kernel %s at 0x%lx, size %u bytes\n",
          KERNEL_PATH, (UINT64)(UINTN)KernelBuffer, (UINT32)KernelSize);

    /* 3) Load initrd from the same filesystem */
    Status = LoadFileToBuffer(Root, INITRD_PATH, &InitrdBuffer, &InitrdSize);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] Failed to load initrd %s: %r\n", INITRD_PATH, Status);
        // You may choose to continue without initrd; here we abort.
        return Status;
    }

    Print(L"[uefi2] Loaded initrd %s at 0x%lx, size %u bytes\n",
          INITRD_PATH, (UINT64)(UINTN)InitrdBuffer, (UINT32)InitrdSize);

    /* 4) Register initrd via EFI_LOAD_FILE2 + LINUX_EFI_INITRD_MEDIA_GUID */
    ZeroMem(&mInitrdLf2, sizeof(mInitrdLf2));
    mInitrdLf2.Proto.LoadFile = InitrdLoadFile;
    mInitrdLf2.InitrdBuffer   = InitrdBuffer;
    mInitrdLf2.InitrdSize     = InitrdSize;

    Status = gBS->InstallMultipleProtocolInterfaces(
        &InitrdHandle,
        &gEfiDevicePathProtocolGuid, (VOID *)&mInitrdDevPath,
        &gEfiLoadFile2ProtocolGuid,  (VOID *)&mInitrdLf2.Proto,
        NULL
    );
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] Failed to install INITRD LoadFile2: %r\n", Status);
        return Status;
    }

    Print(L"[uefi2] Initrd registered via LINUX_EFI_INITRD_MEDIA_GUID (size=%u bytes)\n",
          (UINT32)InitrdSize);

    /* 5) Construct MemMap Device Path for the in-memory kernel image*/
    KernelDp.MemMap.Header.Type    = HARDWARE_DEVICE_PATH;
    KernelDp.MemMap.Header.SubType = HW_MEMMAP_DP;

    {
        UINT16 MemMapLength = (UINT16)sizeof(MEMMAP_DEVICE_PATH);
        KernelDp.MemMap.Header.Length[0] = (UINT8)(MemMapLength & 0xFF);
        KernelDp.MemMap.Header.Length[1] = (UINT8)((MemMapLength >> 8) & 0xFF);
    }

    KernelDp.MemMap.MemoryType      = EfiLoaderData;
    KernelDp.MemMap.StartingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)KernelBuffer;
    KernelDp.MemMap.EndingAddress   =
        KernelDp.MemMap.StartingAddress + (KernelSize - 1);

    KernelDp.End.Type      = END_DEVICE_PATH_TYPE;
    KernelDp.End.SubType   = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    KernelDp.End.Length[0] = (UINT8)sizeof(EFI_DEVICE_PATH_PROTOCOL);
    KernelDp.End.Length[1] = 0;

    /* 6) Load the kernel as an EFI image from memory via MemMap DP */
    Status = gBS->LoadImage(
        FALSE,
        ImageHandle,
        (EFI_DEVICE_PATH_PROTOCOL *)&KernelDp,
        KernelBuffer,
        KernelSize,
        &KernelHandle
    );
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] LoadImage(kernel via MemMap DP) failed: %r\n", Status);
        return Status;
    }

    /* 7) Set kernel command line via LOADED_IMAGE.LoadOptions */
    Status = gBS->HandleProtocol(
        KernelHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&KernelLoadedImage
    );
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] HandleProtocol(LoadedImage for kernel) failed: %r\n", Status);
        return Status;
    }

    KernelLoadedImage->LoadOptions     = (VOID *)KernelCmdline;
    KernelLoadedImage->LoadOptionsSize =
        (UINT32)((StrLen(KernelCmdline) + 1) * sizeof(CHAR16));

    Print(L"[uefi2] Using kernel cmdline: %s\n", KernelCmdline);
    Print(L"[uefi2] Starting kernel Image...\n");

    /* 8) Start the kernel image. Normally we do not return if Linux boots. */
    Status = gBS->StartImage(
        KernelHandle,
        NULL,
        NULL
    );

    Print(L"[uefi2] StartImage(kernel) returned: %r\n", Status);
    return Status;
}

static EFI_STATUS
ProcessBootChain(
    OUT UINT32            *BootChain
    )
{
    EFI_STATUS Status;
    UINT32     BootChainOsCurrent = 0;
    UINTN      Size;
    *BootChain = 0;

    Size = sizeof (BootChain);
    Status   = gRT->GetVariable (BOOT_FW_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &Size, &BootChainOsCurrent);
    if (!EFI_ERROR (Status) && (BootChainOsCurrent <= 1)) {
        *BootChain = BootChainOsCurrent;
    }
    Print(L"[uefi2] Current BOOT_FW_VARIABLE_NAME: %u\n", *BootChain);

    Size = sizeof (BootChain);
    Status   = gRT->GetVariable (BOOT_OS_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &Size, &BootChainOsCurrent);
    if (!EFI_ERROR (Status) && (BootChainOsCurrent <= 1)) {
        *BootChain = BootChainOsCurrent; 
    }
    Print(L"[uefi2] Current BOOT_OS_VARIABLE_NAME: %u\n", *BootChain);

    // TODO: Validate RootfsStatus and update BootChain accordingly

    Status = gRT->SetVariable(BOOT_OS_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS, sizeof (*BootChain), BootChain);
    if (EFI_ERROR (Status)) {
        ErrorPrint (L"Failed to set OS variable: %r\r\n", Status);
    }
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
UefiMain (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
    )
{
    EFI_STATUS  Status;
    UINT32      BootChain = 0;

    Print(L"[uefi2] UefiMain() start\n");
    Status = ProcessBootChain(&BootChain);
    if (EFI_ERROR(Status)) {
        Print(L"[uefi2] ProcessBootChain() failed: %r\n", Status);
        // Keep going
    }
    Status = LoadAndStartKernelFromAnyFs(ImageHandle);
    Print(L"[uefi2] UefiMain() exit: %r\n", Status);
    return Status;
}
