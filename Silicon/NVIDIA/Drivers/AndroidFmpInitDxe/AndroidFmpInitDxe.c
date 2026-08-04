/** @file
  Android FMP Initialization Dxe

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ImageScaleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/NctLib.h>
#include <Library/PrintLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/AndroidBcbLib.h>

#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiImageEx.h>
#include <Protocol/HiiPackageList.h>
#include <Protocol/FwUpdateSplashProtocol.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/SavedCapsuleProtocol.h>
#include <Protocol/EFuse.h>
#include <Protocol/BootChainProtocol.h>

#include "AndroidFmpInitPrivate.h"

#define FMP_WRITE_LOOP_SIZE                  (64 * 1024)
#define ANDROID_ESP_PARTITION                L"staging"
#define ANDROID_FMP_SPLASH_1080P_SHORT_EDGE  1080U

//
// Lower bound (in percent) for the FMP progress bar painted on top of the
// firmware-update splash. Picked so the user perceives the firmware
// install as the final ~20% of the overall update flow; tweak here if
// the boot-time budget preceding HandleCapsules() changes meaningfully.
//
#define ANDROID_FMP_SPLASH_PROGRESS_BASE  80
// these can be parsed out from NCT
#define TEGRA_PLATFORM_SPEC_VARIABLE_NAME  L"TegraPlatformSpec"
#define AUTO_UPDATE_BRBCT_VARIABLE_NAME    L"AutoUpdateBrBct"

#define T23X_FUSE_PRODUCTION_MODE_OFFSET  0x100
#define T23X_FUSE_PRODUCTION_MODE_BIT     BIT0

#define NCT_BOARD_INFO_PROC_FAB_LEN  sizeof(((NCT_BOARD_INFO *)0)->ProcFab)
#define TEGRA_PLATFORM_SPEC_MAX_LEN  128

STATIC NVIDIA_SAVED_CAPSULE_PROTOCOL  mProtocol;
STATIC EFI_CAPSULE_HEADER             *mCapsuleHeader = NULL;

//
// HII state for the OTA splash. Populated lazily on the first splash request
// so failures (e.g. HII Database protocol not yet installed) don't block the
// capsule init path. ``mHiiHandle == NULL`` means "not registered yet" or
// "registration failed permanently".
//
STATIC EFI_HANDLE                 mDriverImageHandle = NULL;
STATIC EFI_HII_HANDLE             mHiiHandle         = NULL;
STATIC EFI_HII_IMAGE_EX_PROTOCOL  *mHiiImageEx       = NULL;
STATIC BOOLEAN                    mHiiInitTried      = FALSE;

/**
  Get partition BlockIo/DiskIo
  @param[in]   PartName     partition name string
  @param[out]  PartBlockIo  BlockIo of the partition
  @param[out]  PartDiskIo   DiskIo of the partition

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred
**/
STATIC
EFI_STATUS
EFIAPI
AndroidFmpGetPartition (
  IN  CHAR16                 *PartName,
  OUT EFI_BLOCK_IO_PROTOCOL  **PartBlockIo,
  OUT EFI_DISK_IO_PROTOCOL   **PartDiskIo
  )
{
  EFI_STATUS                   Status;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  EFI_HANDLE                   MscHandle = NULL;
  UINTN                        Index;
  UINTN                        NumOfHandles;
  EFI_HANDLE                   *HandleBuffer = NULL;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;
  EFI_DISK_IO_PROTOCOL         *DiskIo;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPartitionInfoProtocolGuid,
                  NULL,
                  &NumOfHandles,
                  &HandleBuffer
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: No %s partition info: %r\n", __FUNCTION__, PartName, Status));
    goto Exit;
  }

  for (Index = 0; Index < NumOfHandles; Index++) {
    // Get partition info protcol from handle and validate
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiPartitionInfoProtocolGuid,
                    (VOID **)&PartitionInfo
                    );

    if (EFI_ERROR (Status) || (PartitionInfo == NULL)) {
      DEBUG ((DEBUG_ERROR, "%a: No partition info from handle: %r\n", __FUNCTION__, Status));
      goto Exit;
    }

    DEBUG ((DEBUG_INFO, "%a: Checking partition name: %s\n", __FUNCTION__, PartitionInfo->Info.Gpt.PartitionName));

    if (0 == StrCmp (PartitionInfo->Info.Gpt.PartitionName, PartName)) {
      MscHandle = HandleBuffer[Index];
      break;
    }
  }

  if (MscHandle == NULL) {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "%a: No %s partition\n", __FUNCTION__, PartName));
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  MscHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: No block io protocol on %s partition\n", __FUNCTION__, PartName));
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  MscHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: No disk io protocol on %s partition\n", __FUNCTION__, PartName));
    goto Exit;
  }

  *PartBlockIo = BlockIo;
  *PartDiskIo  = DiskIo;

Exit:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  return Status;
}

/**
  Get TegraPlatformSpec string from NCT and Hardware.
  Format: ${BOARDID}-${FAB}-${BOARDSKU}-${BOARDREV}-${fuselevel_s}-${hwchiprev}-${ext_target_board}

  @param[out]  Buffer         Provided buffer to store platform spec string
  @param[in]   BufferSize     Size of the buffer in bytes

  @retval EFI_SUCCESS         Operation successful
  @retval others              Error occurred
**/
STATIC
EFI_STATUS
EFIAPI
AndroidFmpGetPlatformSpec (
  OUT CHAR8  *Buffer,
  IN  UINTN  BufferSize
  )
{
  EFI_STATUS             Status;
  NCT_ITEM               NctItem;
  NCT_BOARD_INFO         *BoardInfo;
  NVIDIA_EFUSE_PROTOCOL  *EFuse;
  UINT32                 ChipId;
  UINT32                 FuseData;
  UINT32                 ProductionMode;
  UINT32                 MinorVersion;
  CHAR8                  FabStr[NCT_BOARD_INFO_PROC_FAB_LEN + 1] = "";
  CHAR8                  *BoardRevStr;
  CHAR8                  *ExtTargetBoard = "android";
  UINTN                  Index;

  if ((Buffer == NULL) || (BufferSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  // Read BOARD_INFO from NCT
  Status = NctReadItem (NCT_ID_BOARD_INFO, &NctItem);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read NCT BOARD_INFO: %r\n", __FUNCTION__, Status));
    return Status;
  }

  BoardInfo = &NctItem.BoardInfo;
  // Convert ProcFab to ASCII string
  for (Index = 0; Index < NCT_BOARD_INFO_PROC_FAB_LEN; Index++) {
    FabStr[Index] = (CHAR8)((BoardInfo->ProcFab >> (Index * 8)) & 0xFF);
  }

  // FAB and BOARDREV have the same source
  BoardRevStr = FabStr;

  ChipId = TegraGetChipID ();
  if (ChipId != T234_CHIP_ID) {
    // Only t23x series are supported
    DEBUG ((DEBUG_ERROR, "%a: Unsupported chip 0x%x for reading fuselevel\n", __FUNCTION__, ChipId));
    return EFI_UNSUPPORTED;
  }

  Status = gBS->LocateProtocol (&gNVIDIAEFuseProtocolGuid, NULL, (VOID **)&EFuse);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate EFuse protocol: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = EFuse->ReadReg (EFuse, T23X_FUSE_PRODUCTION_MODE_OFFSET, &FuseData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read production mode: %r\n", __FUNCTION__, Status));
    return Status;
  }

  ProductionMode = (FuseData & T23X_FUSE_PRODUCTION_MODE_BIT) ? 1 : 0;

  // Get hardware chip revision
  MinorVersion = TegraGetMinorVersionNumber ();
  if (MinorVersion == MAX_UINT32) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read minor version\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  /*
   * Construct platform spec string:
   * 1. BOARDID & BOARDSKU are zero-padded to 4 digits
   * 2. BOARDREV is identical to FAB
   * 3. ext_target_board defaults to "android"
   */
  AsciiSPrint (
    Buffer,
    BufferSize,
    "%04u-%a-%04u-%a-%u-%x-%a",
    BoardInfo->ProcBoardId,
    FabStr,
    BoardInfo->ProcSku,
    BoardRevStr,
    ProductionMode,
    MinorVersion,
    ExtTargetBoard
    );

  DEBUG ((DEBUG_INFO, "%a: TegraPlatformSpec %a\n", __FUNCTION__, Buffer));
  return EFI_SUCCESS;
}

/**
  Callback to get capsule from storage or memory
  @param[in]   This           Pointer to This protocol
  @param[out]  CapsuleHeader  Output buffer for capsule

  @retval EFI_SUCCESS         Operation successful
  @retval others              Error occurred
**/
STATIC
EFI_STATUS
EFIAPI
AndroidFmpGetCapsule (
  IN  NVIDIA_SAVED_CAPSULE_PROTOCOL  *This,
  OUT EFI_CAPSULE_HEADER             **CapsuleHeader
  )
{
  VOID                   *Capsule = NULL;
  EFI_STATUS             Status;
  UINTN                  CapsuleSize;
  VOID                   *Data;
  UINTN                  ReadOffset;
  UINTN                  ReadBytes;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;
  EFI_DISK_IO_PROTOCOL   *DiskIo;
  EFI_CAPSULE_HEADER     Header;
  EFI_CAPSULE_HEADER     ZeroHeader = { 0 };
  UINT32                 UefiBootChain;
  UINT32                 BcbBootChain = MAX_UINT32;

  if (mCapsuleHeader != NULL) {
    *CapsuleHeader = mCapsuleHeader;
    return EFI_SUCCESS;
  }

  Status = AndroidFmpGetPartition (ANDROID_ESP_PARTITION, &BlockIo, &DiskIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %r trying to get partition\n", __FUNCTION__, Status));
    return Status;
  }

  Status = DiskIo->ReadDisk (DiskIo, BlockIo->Media->MediaId, 0, sizeof (EFI_CAPSULE_HEADER), (VOID *)&Header);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %r trying to get capsule header\n", __FUNCTION__, Status));
    return Status;
  }

  // Check if capsule in storage is valid
  if ( !CompareGuid (&Header.CapsuleGuid, &gEfiFmpCapsuleGuid)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid capsule header guid\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  //
  // A staged capsule must be paired with a chain switch in BCB; otherwise the
  // OTA flow has not committed and the capsule should not be applied. Discard
  // the capsule when the BCB active chain matches the running UEFI chain (or
  // when BCB is unavailable / unreadable) to avoid replaying a stale capsule.
  //
  UefiBootChain = BcbGetActiveFwBootChain ();
  Status        = AndroidBcbGetActiveBootChain (NULL, &BcbBootChain);
  if (EFI_ERROR (Status) || (BcbBootChain == UefiBootChain)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Discarding staged capsule (UEFI chain=%u, BCB chain=%u, status=%r); cleaning staging partition\n",
      __FUNCTION__,
      UefiBootChain,
      BcbBootChain,
      Status
      ));

    Status = DiskIo->WriteDisk (DiskIo, BlockIo->Media->MediaId, 0, sizeof (EFI_CAPSULE_HEADER), (VOID *)&ZeroHeader);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to clean staging capsule header\n", __FUNCTION__, Status));
    }

    return EFI_NOT_FOUND;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: Chain switch detected (UEFI chain=%u, BCB chain=%u); proceeding with capsule\n",
    __FUNCTION__,
    UefiBootChain,
    BcbBootChain
    ));

  CapsuleSize = Header.CapsuleImageSize;
  Capsule     = AllocatePool (CapsuleSize);
  if (Capsule == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: alloc of %u failed\n", __FUNCTION__, CapsuleSize));
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Load capsule image to buffer
  //
  Data       = (UINT8 *)Capsule;
  ReadOffset = 0x00;
  ReadBytes  = CapsuleSize;

  while (ReadBytes > 0) {
    UINTN  ReadSize;

    ReadSize   = (ReadBytes > FMP_WRITE_LOOP_SIZE) ? FMP_WRITE_LOOP_SIZE : ReadBytes;
    ReadBytes -= ReadSize;

    Status = DiskIo->ReadDisk (DiskIo, BlockIo->Media->MediaId, ReadOffset, ReadSize, (VOID *)Data);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to read capsule\n", __FUNCTION__, Status));
      Status = EFI_ABORTED;
      goto Exit;
    }

    ReadOffset += ReadSize;
    Data       += ReadSize;
  }

  // Erase the capsule header from storage to invalidate capsule as consumed
  Status = DiskIo->WriteDisk (DiskIo, BlockIo->Media->MediaId, 0, sizeof (EFI_CAPSULE_HEADER), (VOID *)&ZeroHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to mark capsule consumed\n", __FUNCTION__, Status));
    Status = EFI_ABORTED;
    goto Exit;
  }

  *CapsuleHeader = (EFI_CAPSULE_HEADER *)Capsule;
  // Cache Capsule memory buffer handler
  mCapsuleHeader = (EFI_CAPSULE_HEADER *)Capsule;

  Status = EFI_SUCCESS;

Exit:
  if (EFI_ERROR (Status) && (Capsule != NULL)) {
    FreePool (Capsule);
  }

  return Status;
}

/**
  Setup Android capsule update necessary efivars

  @retval EFI_SUCCESS         Operation successful
  @retval others              Error occurred
**/
EFI_STATUS
EFIAPI
AndroidFmpSimulateVars (
  VOID
  )
{
  EFI_STATUS  Status;
  CHAR8       PlatformSpec[TEGRA_PLATFORM_SPEC_MAX_LEN];
  UINT32      mAutoUpdateBrBctFlag = 1;

  Status = AndroidFmpGetPlatformSpec (PlatformSpec, sizeof (PlatformSpec));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r getting TegraPlatformSpec\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = gRT->SetVariable (
                  TEGRA_PLATFORM_SPEC_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  AsciiStrLen (PlatformSpec) + 1,
                  PlatformSpec
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error setting variable %s: %r\n", __FUNCTION__, TEGRA_PLATFORM_SPEC_VARIABLE_NAME, Status));
    goto Done;
  }

  Status = gRT->SetVariable (
                  AUTO_UPDATE_BRBCT_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  sizeof (UINT32),
                  &mAutoUpdateBrBctFlag
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error setting variable %s: %r\n", __FUNCTION__, AUTO_UPDATE_BRBCT_VARIABLE_NAME, Status));
    goto Done;
  }

Done:
  return Status;
}

/**
  Setup FMP variables and PCDs for fastboot capsule update path.
  Called when bootonce-bootloader BCB is detected so that the capsule
  update environment is ready before the fastboot application starts.

  @retval EFI_SUCCESS         Operation successful
  @retval others              Error occurred
**/
STATIC
EFI_STATUS
SetupFastbootCapsuleEnv (
  VOID
  )
{
  EFI_STATUS                  Status;
  NVIDIA_BOOT_CHAIN_PROTOCOL  *BootChainProtocol;
  UINT8                       BootChain;
  CHAR8                       PlatformSpec[TEGRA_PLATFORM_SPEC_MAX_LEN];

  Status = AndroidFmpGetPlatformSpec (PlatformSpec, sizeof (PlatformSpec));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r getting TegraPlatformSpec\n", __FUNCTION__, Status));
    return Status;
  }

  Status = gRT->SetVariable (
                  TEGRA_PLATFORM_SPEC_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  AsciiStrLen (PlatformSpec) + 1,
                  PlatformSpec
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error setting variable %s: %r\n", __FUNCTION__, TEGRA_PLATFORM_SPEC_VARIABLE_NAME, Status));
    return Status;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIABootChainProtocolGuid,
                  NULL,
                  (VOID **)&BootChainProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: BootChainProtocol not found: %r\n", __FUNCTION__, Status));
    return Status;
  }

  BootChain = (UINT8)BootChainProtocol->ActiveBootChain;

  Status = gRT->SetVariable (
                  L"FmpCapsuleSinglePartitionChain",
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (UINT8),
                  &BootChain
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: SetVariable FmpCapsuleSinglePartitionChain: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_ERROR, "%a: FmpCapsuleSinglePartitionChain set to %u\n", __FUNCTION__, BootChain));

  Status = PcdSetBoolS (PcdFmpMultiImageMinimalUpdate, TRUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: PcdSet PcdFmpMultiImageMinimalUpdate: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_ERROR, "%a: Fastboot capsule environment ready\n", __FUNCTION__));
  return EFI_SUCCESS;
}

/**
  Lazily register this driver's HII image package list with the HII Database.

  The package list is attached to the PE/COFF image via
  ``UEFI_HII_RESOURCE_SECTION = TRUE`` in the inf and contains the OTA splash
  bitmap. Registration is performed best-effort on first access so that a
  missing HII Database protocol (depex omission is intentional -- we must not
  block capsule init on UI) leaves ``mHiiHandle`` NULL and the splash is
  silently skipped.

  @retval EFI_SUCCESS  HII bundle is registered and ready for image lookup.
  @retval Other        HII protocol missing or registration failed.
**/
STATIC
EFI_STATUS
EnsureHiiBundleRegistered (
  VOID
  )
{
  EFI_STATUS                   Status;
  EFI_HII_DATABASE_PROTOCOL    *HiiDatabase;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;

  if (mHiiHandle != NULL) {
    return EFI_SUCCESS;
  }

  //
  // Latch the first attempt: HII may be unavailable for the lifetime of this
  // driver image (e.g. minimal boot), and retrying every splash call would
  // spam the log without changing the outcome.
  //
  if (mHiiInitTried) {
    return EFI_NOT_FOUND;
  }

  mHiiInitTried = TRUE;

  if (mDriverImageHandle == NULL) {
    DEBUG ((DEBUG_WARN, "%a: driver image handle not captured\n", __FUNCTION__));
    return EFI_NOT_READY;
  }

  Status = gBS->LocateProtocol (
                  &gEfiHiiDatabaseProtocolGuid,
                  NULL,
                  (VOID **)&HiiDatabase
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: HII Database not available: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = gBS->LocateProtocol (
                  &gEfiHiiImageExProtocolGuid,
                  NULL,
                  (VOID **)&mHiiImageEx
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: HII Image Ex not available: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = gBS->OpenProtocol (
                  mDriverImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  mDriverImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: HII package not in PE/COFF resource: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = HiiDatabase->NewPackageList (HiiDatabase, PackageList, NULL, &mHiiHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to publish HII package list: %r\n", __FUNCTION__, Status));
    mHiiHandle = NULL;
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: HII package list registered\n", __FUNCTION__));
  return EFI_SUCCESS;
}

/**
  Render the OTA splash bitmap to the full screen and return immediately.

  The splash is left on the framebuffer for the rest of UEFI execution; the
  next stage (capsule install + reset) will naturally replace it. Best-effort:
  missing HII / GOP / image data yields EFI_SUCCESS without rendering, since
  capsule install must proceed regardless of UI.

  @retval EFI_SUCCESS  Splash shown (or silently skipped).
  @retval Other        Underlying HII / GOP failure (logged).
**/
STATIC
EFI_STATUS
AndroidFmpDisplaySplash (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_IMAGE_ID                   ImageId;
  EFI_IMAGE_INPUT                Image;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *Gop;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *ScaledImage;
  UINTN                          ScreenWidth;
  UINTN                          ScreenHeight;
  UINTN                          PanelShortEdge;

  Status = EnsureHiiBundleRegistered ();
  if (EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  (VOID **)&Gop
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: GOP not available: %r\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  if ((Gop->Mode == NULL) || (Gop->Mode->Info == NULL)) {
    DEBUG ((DEBUG_WARN, "%a: GOP mode info unavailable\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  ScreenWidth    = Gop->Mode->Info->HorizontalResolution;
  ScreenHeight   = Gop->Mode->Info->VerticalResolution;
  PanelShortEdge = (ScreenWidth < ScreenHeight) ? ScreenWidth : ScreenHeight;

  if (PanelShortEdge > ANDROID_FMP_SPLASH_1080P_SHORT_EDGE) {
    ImageId = mOtaImageId4k;
  } else {
    ImageId = mOtaImageId1080;
  }

  Status = mHiiImageEx->GetImageEx (mHiiImageEx, mHiiHandle, ImageId, &Image);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: GetImageEx %d failed: %r\n", __FUNCTION__, ImageId, Status));
    return Status;
  }

  Status = ImageScale (
             Image.Bitmap,
             (UINTN)Image.Width,
             (UINTN)Image.Height,
             ScreenWidth,
             ScreenHeight,
             &ScaledImage
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: ImageScale failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = Gop->Blt (
                  Gop,
                  ScaledImage,
                  EfiBltBufferToVideo,
                  0,
                  0,                                                 // src x/y
                  0,
                  0,                                                 // dst x/y
                  ScreenWidth,
                  ScreenHeight,
                  ScreenWidth * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                  );

  FreePool (ScaledImage);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: GOP Blt failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  OTA splash protocol Show() method. Wraps ``AndroidFmpDisplaySplash``
  so PlatformBootManagerLib can render the splash right before
  HandleCapsules() runs the FMP install. Resolution-aware image
  selection happens inside AndroidFmpDisplaySplash().
**/
STATIC
EFI_STATUS
EFIAPI
AndroidFmpSplashShow (
  IN NVIDIA_FW_UPDATE_SPLASH_PROTOCOL  *This
  )
{
  return AndroidFmpDisplaySplash ();
}

STATIC NVIDIA_FW_UPDATE_SPLASH_PROTOCOL  mSplashProtocol = {
  AndroidFmpSplashShow,
  ANDROID_FMP_SPLASH_PROGRESS_BASE,
  L"Updating SHIELD: "
};

/**
 Android FMP Initialization DXE Driver entry point.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
AndroidFmpInitDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  EFI_HANDLE          CapsuleSplashHandle;
  EFI_HANDLE          CompleteHandle;
  EFI_CAPSULE_HEADER  *Header = NULL;
  MiscCmdType         MiscCmd = MISC_CMD_TYPE_INVALID;

  //
  // Cache the driver's image handle so the splash path can later
  // OpenProtocol(gEfiHiiPackageListProtocolGuid) against it to retrieve the
  // bundled HII resource section.
  //
  mDriverImageHandle  = ImageHandle;
  CapsuleSplashHandle = NULL;
  CompleteHandle      = NULL;

  //
  // When booting into fastboot (bootonce-bootloader), prepare the capsule
  // update environment early so UpdateCapsule can work from the fastboot app.
  //
  Status = GetCmdFromMiscPartition (NULL, &MiscCmd, FALSE);
  if (!EFI_ERROR (Status) && (MiscCmd == MISC_CMD_TYPE_FASTBOOT_BOOTLOADER)) {
    Status = SetupFastbootCapsuleEnv ();
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Got %r setting up fastboot capsule env\n", __FUNCTION__, Status));
    }

    // Skip capsule loading and AutoUpdateBrBct for fastboot path; the
    // capsule will arrive via fastboot stage.  Fall through to install
    // FmpInitComplete so that FmpDxe can start.
  } else {
    Status = AndroidFmpGetCapsule (NULL, &Header);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to load capsule\n", __FUNCTION__, Status));
      Status = EFI_SUCCESS;
      goto Exit;
    }

    mProtocol.GetCapsule = AndroidFmpGetCapsule;

    //
    // Publish the saved-capsule protocol for FmpDxe and the OTA splash
    // protocol so PlatformBootManagerLib can render android_ota_4k.bmp as
    // the background for the FMP install progress that runs inside
    // HandleCapsules(). Drawing the splash here during DXE would be wiped
    // by BDS console reconfiguration, so we defer the actual blit until
    // PlatformBm invokes Show() on the protocol.
    //
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &CapsuleSplashHandle,
                    &gNVIDIASavedCapsuleProtocolGuid,
                    &mProtocol,
                    &gNVIDIAFwUpdateSplashProtocolGuid,
                    &mSplashProtocol,
                    NULL
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error installing protocol: %r\n", __FUNCTION__, Status));
      CapsuleSplashHandle = NULL;
      goto Exit;
    }

    Status = AndroidFmpSimulateVars ();
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &CompleteHandle,
                  &gNVIDIAAndroidFmpInitCompleteProtocolGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing InitComplete: %r\n", __FUNCTION__, Status));
    CompleteHandle = NULL;
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status) && (CapsuleSplashHandle != NULL)) {
    gBS->UninstallMultipleProtocolInterfaces (
           CapsuleSplashHandle,
           &gNVIDIASavedCapsuleProtocolGuid,
           &mProtocol,
           &gNVIDIAFwUpdateSplashProtocolGuid,
           &mSplashProtocol,
           NULL
           );
  }

  return Status;
}
