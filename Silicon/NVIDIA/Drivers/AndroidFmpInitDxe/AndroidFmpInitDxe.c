/** @file
  Android FMP Initialization Dxe

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/NctLib.h>
#include <Library/PrintLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/SavedCapsuleProtocol.h>
#include <Protocol/EFuse.h>

#define FMP_WRITE_LOOP_SIZE    (64 * 1024)
#define ANDROID_ESP_PARTITION  L"staging"
// these can be parsed out from NCT
#define TEGRA_PLATFORM_SPEC_VARIABLE_NAME  L"TegraPlatformSpec"
#define AUTO_UPDATE_BRBCT_VARIABLE_NAME    L"AutoUpdateBrBct"

#define T23X_FUSE_PRODUCTION_MODE_OFFSET  0x100
#define T23X_FUSE_PRODUCTION_MODE_BIT     BIT0

#define NCT_BOARD_INFO_PROC_FAB_LEN  sizeof(((NCT_BOARD_INFO *)0)->ProcFab)
#define TEGRA_PLATFORM_SPEC_MAX_LEN  128

STATIC NVIDIA_SAVED_CAPSULE_PROTOCOL  mProtocol;
STATIC EFI_CAPSULE_HEADER             *mCapsuleHeader = NULL;

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
  EFI_HANDLE          Handle;
  EFI_CAPSULE_HEADER  *Header = NULL;

  Status = AndroidFmpGetCapsule (NULL, &Header);
  if (EFI_ERROR (Status)) {
    // Do not setup Android Capsule update environment and install protocol
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to load capsule\n", __FUNCTION__, Status));
    Status = EFI_SUCCESS;
    goto Exit;
  }

  mProtocol.GetCapsule = AndroidFmpGetCapsule;
  Handle               = NULL;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gNVIDIASavedCapsuleProtocolGuid,
                  &mProtocol,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing protocol: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = AndroidFmpSimulateVars ();
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gNVIDIAAndroidFmpInitCompleteProtocolGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing InitComplete: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  return Status;
}
