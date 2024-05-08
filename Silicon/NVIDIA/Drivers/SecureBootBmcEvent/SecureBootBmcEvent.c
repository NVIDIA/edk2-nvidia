/** @file

  This driver sends event to BMC when there is user action to enable secure boot or
  disable secure. An event is also send to BMC when secure boot failure happens.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <UefiSecureBoot.h>
#include <ConfigurationManagerHelper.h>
#include <ConfigurationManagerObject.h>
#include <SmbiosNameSpaceObjects.h>
#include <Guid/ImageAuthentication.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/SecureBootVariableLib.h>

#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/ConfigurationManagerProtocol.h>

#include <NVIDIAStatusCodes.h>
#include <OemStatusCodes.h>

#define SECURE_BOOT_LAST_MODE_VARIABLE_NAME  L"SecureBootLastMode"
#define STATUS_CODE_DATA_MAX_LEN             256
#define SECURE_BOOT_BMC_EVENT_DEBUG          DEBUG_VERBOSE

EDKII_CONFIGURATION_MANAGER_PROTOCOL  *mCfgMgrProtocol = NULL;
CM_SMBIOS_SYSTEM_SLOTS_INFO           *mSystemSlotInfo = NULL;
UINT32                                mNumSystemSlots  = 0;

/** This macro expands to a function that retrieves the System Slot
    information from the Configuration Manager.
*/
GET_OBJECT_LIST (
  EObjNameSpaceSmbios,
  ESmbiosObjSystemSlotInfo,
  CM_SMBIOS_SYSTEM_SLOTS_INFO
  )

/**
  This function write secure boot mode into variable.

  @param[in]  SecureBootMode    The secure boot mode value to save.

  @retval EFI_SUCCESS            This function writes secure boot mode successfully.
  @retval Others                 An unexpected error occurred.

**/
EFI_STATUS
SecureBootSetLastMode (
  IN UINT8  SecureBootMode
  )
{
  EFI_STATUS  Status;

  Status = gRT->SetVariable (
                  SECURE_BOOT_LAST_MODE_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  sizeof (SecureBootMode),
                  &SecureBootMode
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, cannot write %s: %r\n", __func__, SECURE_BOOT_LAST_MODE_VARIABLE_NAME, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  This function read secure boot mode in last boot.

  @param[out]  SecureBootMode    The secure boot mode value in last boot.

  @retval EFI_SUCCESS            This function reads secure boot mode successfully.
  @retval Others                 An unexpected error occurred.

**/
EFI_STATUS
SecureBootGetLastMode (
  OUT UINT8  *SecureBootMode
  )
{
  EFI_STATUS  Status;
  UINT8       *SecureBootLastMode;
  UINTN       DataSize;

  if (SecureBootMode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  SecureBootLastMode = NULL;
  DataSize           = 0;

  Status = GetVariable2 (
             SECURE_BOOT_LAST_MODE_VARIABLE_NAME,
             &gNVIDIATokenSpaceGuid,
             (VOID **)&SecureBootLastMode,
             &DataSize
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((DataSize != sizeof (UINT8)) || (SecureBootLastMode == NULL)) {
    return EFI_VOLUME_CORRUPTED;
  }

  *SecureBootMode = *SecureBootLastMode;

  return EFI_SUCCESS;
}

/**
  This function checks to see if secure boot is enabled or disabled in current boot.

  @retval EFI_SUCCESS            This function is done successfully.
  @retval Others                 An unexpected error occurred.

**/
EFI_STATUS
SecureBootEnableDisableAction (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT8       SecureBootMode;
  UINT8       SecureBootLastMode;

  Status = EFI_SUCCESS;
  //
  // Get current secure boot mode
  //
  if (IsSecureBootEnabled ()) {
    SecureBootMode = SECURE_BOOT_MODE_ENABLE;
  } else {
    SecureBootMode = SECURE_BOOT_MODE_DISABLE;
  }

  //
  // Get last secure boot mode
  //
  Status = SecureBootGetLastMode (&SecureBootLastMode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a, no secure boot mode information in last boot: %r\n", __func__, Status));
    goto ON_FINISH;
  }

  DEBUG ((SECURE_BOOT_BMC_EVENT_DEBUG, "%a, current secure boot mode: %a\n", __func__, (SecureBootMode == SECURE_BOOT_MODE_ENABLE) ? "Enabled" : "Disabled"));
  DEBUG ((SECURE_BOOT_BMC_EVENT_DEBUG, "%a, last secure boot mode: %a\n", __func__, (SecureBootLastMode == SECURE_BOOT_MODE_ENABLE) ? "Enabled" : "Disabled"));

  //
  // Report corresponding event
  //
  if ((SecureBootMode == SECURE_BOOT_MODE_DISABLE) && (SecureBootLastMode == SECURE_BOOT_MODE_ENABLE)) {
    REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
      EFI_PROGRESS_CODE | EFI_OEM_PROGRESS_MAJOR,
      EFI_CLASS_NV_FIRMWARE | EFI_NV_FW_UEFI_PC_SECURE_BOOT_DISABLED,
      OEM_EC_DESC_SECURE_BOOT_DISABLED,
      sizeof (OEM_EC_DESC_SECURE_BOOT_DISABLED)
      );
  } else if ((SecureBootMode == SECURE_BOOT_MODE_ENABLE) && (SecureBootLastMode == SECURE_BOOT_MODE_DISABLE)) {
    REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
      EFI_PROGRESS_CODE | EFI_OEM_PROGRESS_MAJOR,
      EFI_CLASS_NV_FIRMWARE | EFI_NV_FW_UEFI_PC_SECURE_BOOT_ENABLED,
      OEM_EC_DESC_SECURE_BOOT_ENABLED,
      sizeof (OEM_EC_DESC_SECURE_BOOT_ENABLED)
      );
  } else {
    //
    // There is no secure boot state change.
    //
    return EFI_SUCCESS;
  }

ON_FINISH:

  //
  // keep current secure boot mode
  //
  SecureBootSetLastMode (SecureBootMode);

  return Status;
}

/**
  This function reads SMBIOS type 9 records and keep it for later use.

  @retval EFI_SUCCESS            This function is done successfully.
  @retval Others                 An unexpected error occurred.

**/
EFI_STATUS
GetSmbiosType9Records (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mCfgMgrProtocol == NULL) {
    Status = gBS->LocateProtocol (
                    &gEdkiiConfigurationManagerProtocolGuid,
                    NULL,
                    (VOID **)&mCfgMgrProtocol
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (mSystemSlotInfo == NULL) {
    Status = GetESmbiosObjSystemSlotInfo (
               (EDKII_CONFIGURATION_MANAGER_PROTOCOL *CONST)mCfgMgrProtocol,
               CM_NULL_TOKEN,
               &mSystemSlotInfo,
               &mNumSystemSlots
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (mNumSystemSlots == 0) {
      return EFI_NOT_FOUND;
    }
  }

  return EFI_SUCCESS;
}

/**
  This function tries to get PCI slot information.

  @param[in] DevicePath   Pointer to device path protocol.

  @retval CHAR8 *         String pointer to slot information.
  @retval NULL            No slot information found.

**/
CHAR8 *
SecureBootGetSlotInformation (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  EFI_HANDLE           Handle;
  UINTN                SegNumber;
  UINTN                BusNumber;
  UINTN                DevNumber;
  UINTN                FuncNumber;
  UINTN                DevFuncNumber;
  UINTN                Index;
  CHAR8                PciBdfString[] = "PCI Segment 00 Bus 00 Device 00 Func 00";

  PciIo         = NULL;
  Handle        = NULL;
  DevFuncNumber = 0;

  if (DevicePath == NULL) {
    return NULL;
  }

  Status = gBS->LocateDevicePath (&gEfiDevicePathProtocolGuid, &DevicePath, &Handle);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // Check and see if there is PCI IO protocol on this handle.
  //
  Status = gBS->HandleProtocol (Handle, &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // Get device location
  //
  PciIo->GetLocation (PciIo, &SegNumber, &BusNumber, &DevNumber, &FuncNumber);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  DEBUG ((SECURE_BOOT_BMC_EVENT_DEBUG, "%a, Seg: %u Bus: %u Dev: %u Func: %u\n", __func__, SegNumber, BusNumber, DevNumber, FuncNumber));
  DevFuncNumber = ((DevNumber << 3) | (FuncNumber));

  //
  // Search type 9 records to find slot information.
  //
  if (mSystemSlotInfo == NULL) {
    Status = GetSmbiosType9Records ();
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a, there is no type 9 record found: %r\n", __func__, Status));
      return NULL;
    }
  }

  if (mNumSystemSlots == 0) {
    return NULL;
  }

  for (Index = 0; Index < mNumSystemSlots; Index++) {
    if ((mSystemSlotInfo[Index].SegmentGroupNum == SegNumber) &&
        (mSystemSlotInfo[Index].BusNum == BusNumber) &&
        (mSystemSlotInfo[Index].DevFuncNum == DevFuncNumber))
    {
      return (CHAR8 *)AllocateCopyPool (AsciiStrSize (mSystemSlotInfo[Index].SlotDesignation), mSystemSlotInfo[Index].SlotDesignation);
    }
  }

  //
  // No record found in SMBIOS type 9.
  // Use PCI segment, bus, device and function number.
  //
  AsciiSPrint (PciBdfString, sizeof (PciBdfString), "PCI Segment %02u Bus %02u Device %02u Func %02u", (UINT8)SegNumber, (UINT8)BusNumber, (UINT8)DevNumber, (UINT8)FuncNumber);

  return (CHAR8 *)AllocateCopyPool (sizeof (PciBdfString), PciBdfString);
}

/**
  This function tries to get image information.

  @param[in] DevicePath   Pointer to device path protocol.

  @retval CHAR16 *        String pointer to image information.
  @retval NULL            No slot information found.

**/
CHAR16 *
SecureBootGetInfoFromDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Node;
  FILEPATH_DEVICE_PATH      *FileNode;
  CHAR8                     *SlotInfo;
  CHAR16                    *FilePathName;
  CHAR16                    *InfoString;
  UINTN                     InfoStringSize;

  if (DevicePath == NULL) {
    return NULL;
  }

  InfoStringSize = 0;
  FilePathName   = NULL;
  InfoString     = NULL;
  SlotInfo       = NULL;
  Node           = DevicePath;

  //
  // Try to get slot information from device path.
  //
  SlotInfo = SecureBootGetSlotInformation (DevicePath);

  //
  // Check and see if there is image file path name or not.
  //
  while (!IsDevicePathEnd (Node)) {
    if ((DevicePathType (Node) == MEDIA_DEVICE_PATH) &&
        (DevicePathSubType (Node) == MEDIA_FILEPATH_DP))
    {
      FileNode     = (FILEPATH_DEVICE_PATH *)Node;
      FilePathName = FileNode->PathName;
      break;
    }

    Node = NextDevicePathNode (Node);
  }

  if ((SlotInfo == NULL) && (FilePathName == NULL)) {
    return NULL;
  }

  InfoStringSize = 2;
  if (SlotInfo != NULL) {
    InfoStringSize += AsciiStrLen (SlotInfo);
  }

  if (FilePathName != NULL) {
    InfoStringSize += StrLen (FilePathName);
  }

  //
  // Put slot information and file name together.
  //
  InfoStringSize = InfoStringSize * sizeof (CHAR16);
  InfoString     = AllocatePool (InfoStringSize);
  if (InfoString != NULL) {
    UnicodeSPrint (InfoString, InfoStringSize, L"%a %s", (SlotInfo == NULL ? "" : SlotInfo), (FilePathName == NULL ? L"" : FilePathName));
  }

  if (SlotInfo != NULL) {
    FreePool (SlotInfo);
  }

  return InfoString;
}

/**
  This function reports details of input ImageExeInfoItem.

  @retval EFI_SUCCESS            This function is done successfully.
  @retval Others                 An unexpected error occurred.

**/
EFI_STATUS
SecureBootReportExecutionInfo (
  IN EFI_IMAGE_EXECUTION_INFO  *ImageExeInfoItem
  )
{
  CHAR16                    *Name;
  CHAR8                     OemDesc[STATUS_CODE_DATA_MAX_LEN];
  UINTN                     NameSize;
  BOOLEAN                   NameFound;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  CHAR16                    *ImageInfo;

  if (ImageExeInfoItem == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  NameFound = FALSE;
  Name      = NULL;
  ImageInfo = NULL;
  NameSize  = 0;

  if (ImageExeInfoItem->InfoSize == 0) {
    return EFI_NOT_FOUND;
  }

  switch (ImageExeInfoItem->Action) {
    case EFI_IMAGE_EXECUTION_AUTHENTICATION:
    case EFI_IMAGE_EXECUTION_AUTH_SIG_PASSED:
    case EFI_IMAGE_EXECUTION_AUTH_SIG_FOUND:
    case EFI_IMAGE_EXECUTION_INITIALIZED:
      //
      // We don't report success cases.
      //
      break;
    case EFI_IMAGE_EXECUTION_AUTH_UNTESTED:
    case EFI_IMAGE_EXECUTION_AUTH_SIG_FAILED:
    case EFI_IMAGE_EXECUTION_AUTH_SIG_NOT_FOUND:
    case EFI_IMAGE_EXECUTION_POLICY_FAILED:
      Name = (CHAR16 *)((UINT8 *)ImageExeInfoItem + sizeof (EFI_IMAGE_EXECUTION_INFO));
      if ((Name != NULL) && (Name[0] != L'\0')) {
        NameSize = StrSize (Name);
      } else {
        NameSize = sizeof (CHAR16);
      }

      DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)ImageExeInfoItem + sizeof (EFI_IMAGE_EXECUTION_INFO) + NameSize);
      if (IsDevicePathValid (DevicePath, ImageExeInfoItem->InfoSize)) {
        //
        // Try to get image information from device path.
        //
        ImageInfo = SecureBootGetInfoFromDevicePath (DevicePath);
      }

      AsciiSPrint (OemDesc, STATUS_CODE_DATA_MAX_LEN, "%a%s", OEM_EC_DESC_SECURE_BOOT_FAILURE, (ImageInfo == NULL ? Name : ImageInfo));
      OemDesc[STATUS_CODE_DATA_MAX_LEN - 1] = '\0';

      //
      // Report event
      //
      REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
        EFI_ERROR_CODE | EFI_ERROR_MAJOR,
        EFI_CLASS_NV_FIRMWARE | EFI_NV_FW_UEFI_EC_SECURE_BOOT_FAILED,
        OemDesc,
        STATUS_CODE_DATA_MAX_LEN
        );

      DEBUG ((SECURE_BOOT_BMC_EVENT_DEBUG, "%a, Action: 0x%x %a\n", __func__, ImageExeInfoItem->Action, OemDesc));
      break;
    default:
      DEBUG ((SECURE_BOOT_BMC_EVENT_DEBUG, "%a, unknown action: 0x%x\n", __func__, ImageExeInfoItem->Action));
      break;
  }

  if (ImageInfo != NULL) {
    FreePool (ImageInfo);
  }

  return EFI_SUCCESS;
}

/**
  This function reads image execution information table and
  reports any secure boot failure. Per UEFI spec 2.10 section 32.4.2,
  if the image’s signature is not found in the authorized database,
  or is found in the forbidden database, the image will not be started and
  instead, information about it will be placed in the EFI_IMAGE_EXECUTION_INFO_TABLE

  @retval EFI_SUCCESS            This function is done successfully.
  @retval Others                 An unexpected error occurred.

**/
EFI_STATUS
SecureBootFailureReporting (
  VOID
  )
{
  EFI_STATUS                      Status;
  UINTN                           Index;
  EFI_IMAGE_EXECUTION_INFO_TABLE  *ImageExeInfoTable;
  EFI_IMAGE_EXECUTION_INFO        *ImageExeInfoItem;

  ImageExeInfoTable = NULL;

  Status = EfiGetSystemConfigurationTable (&gEfiImageSecurityDatabaseGuid, (VOID **)&ImageExeInfoTable);
  if (EFI_ERROR (Status) || (ImageExeInfoTable == NULL)) {
    DEBUG ((DEBUG_WARN, "%a, read image execution information table failure: %r\n", __func__, Status));
    return Status;
  }

  if (ImageExeInfoTable->NumberOfImages == 0) {
    return EFI_SUCCESS;
  }

  ImageExeInfoItem = (EFI_IMAGE_EXECUTION_INFO *)((UINT8 *)ImageExeInfoTable + sizeof (EFI_IMAGE_EXECUTION_INFO_TABLE));
  for (Index = 0; Index < ImageExeInfoTable->NumberOfImages; Index++) {
    SecureBootReportExecutionInfo (ImageExeInfoItem);
    ImageExeInfoItem = (EFI_IMAGE_EXECUTION_INFO *)((UINT8 *)ImageExeInfoItem + ReadUnaligned32 ((UINT32 *)&ImageExeInfoItem->InfoSize));
  }

  return EFI_SUCCESS;
}

/**
  This function checks secure boot status. If secure boot is enabled
  or disabled in this boot, it sends event to BMC.

  @param[in]  Event    The event of notify protocol.
  @param[in]  Context  Notify event context.
**/
VOID
EFIAPI
SecureBootReadyToBootCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  //
  // Check and see if secure boot is enabled or disabled
  // in this boot.
  //
  SecureBootEnableDisableAction ();

  //
  // Check and see if there is secure boot failure reported.
  //
  SecureBootFailureReporting ();
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
SecureBootBmcEventEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   SecureBootReadyToBootEvent;

  //
  // Register callback function to send the event to BMC
  // when secure boot is enabled or disabled in this boot.
  //
  Status = EfiCreateEventReadyToBootEx (
             TPL_CALLBACK,
             SecureBootReadyToBootCallback,
             NULL,
             &SecureBootReadyToBootEvent
             );

  ASSERT_EFI_ERROR (Status);

  return Status;
}
