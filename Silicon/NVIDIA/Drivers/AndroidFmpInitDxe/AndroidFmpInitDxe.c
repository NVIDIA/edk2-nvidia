/** @file
  Android FMP Initialization Dxe

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/DeviceTreeHelperLib.h>

#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/SavedCapsuleProtocol.h>

#define FMP_WRITE_LOOP_SIZE    (64 * 1024)
#define ANDROID_ESP_PARTITION  L"staging"
// these can be parsed out from NCT
#define TEGRA_PLATFORM_SPEC_VARIABLE_NAME  L"TegraPlatformSpec"
#define AUTO_UPDATE_BRBCT_VARIABLE_NAME    L"AutoUpdateBrBct"

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
  CHAR8       *mPlatformSpec       = NULL;
  UINT32      mAutoUpdateBrBctFlag = 1;
  INT32       NodeOffset;

  Status = DeviceTreeGetNodeByPath ("/firmware/uefi", &NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r getting /firmware/uefi\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, "tegra-platform-spec", (CONST VOID **)&mPlatformSpec, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r getting node TegraPlatformSpec\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = gRT->SetVariable (
                  TEGRA_PLATFORM_SPEC_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  AsciiStrLen (mPlatformSpec) + 1,
                  mPlatformSpec
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
