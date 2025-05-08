/** @file

  A driver that sends SMBIOS Type17 data to an RASFW receiver

  SPDX-FileCopyrightText: copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Base.h"
#include "ProcessorBind.h"
#include <IndustryStandard/SmBios.h>
#include <Protocol/Smbios.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/HobLib.h>
#include <Library/ArmSmcLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Protocol/MmCommunication2.h>
#include <Library/SecureFwCommLib.h>
#include <Server/RASNSInterface.h>

typedef struct {
  UINT16    SmbiosHandle;
  UINT64    SerialNumber;
  INT64     PartIndex;
} SmbiosType17DataType;

STATIC
EFI_STATUS
SendSmbiosType17Data (
  IN SmbiosType17DataType  *SmbiosType17Data,
  IN UINTN                 SmbiosType17DataSize
  )
{
  EFI_STATUS                 Status;
  EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader;
  UINT8                      *SendBuffer;
  PHYSICAL_ADDRESS           CommBuffer;
  UINTN                      CommBufferSize;
  UINT16                     PartitionId;
  SmbiosType17DataType       *SmbiosType17DataPtr;

  PartitionId = FfaGetFwPartitionId (
                  RAS_FW_UUID_0,
                  RAS_FW_UUID_1,
                  RAS_FW_UUID_2,
                  RAS_FW_UUID_3
                  );

  Status = FfaGetCommunicationBuffer (
             &CommBuffer,
             &CommBufferSize,
             PartitionId,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get communication buffer\n"));
    goto ExitSendSmbiosType17Data;
  }

  SendBuffer = AllocateZeroPool (OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) + SmbiosType17DataSize);
  if (SendBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate send buffer\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitSendSmbiosType17Data;
  }

  CommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)SendBuffer;
  CopyGuid (&(CommunicateHeader->HeaderGuid), &gNVIDIARasSmbiosMsgGuid);
  CommunicateHeader->MessageLength = SmbiosType17DataSize;
  SmbiosType17DataPtr              = (SmbiosType17DataType *)(CommunicateHeader->Data);
  CopyMem (SmbiosType17DataPtr, SmbiosType17Data, SmbiosType17DataSize);

  Status = FfaGuidedCommunication (CommunicateHeader, CommBuffer, CommBufferSize, PartitionId, RAS_FW_GUID_COMMUNICATION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to send SMBIOS Type17 data\n"));
    goto ExitSendSmbiosType17Data;
  }

ExitSendSmbiosType17Data:
  return Status;
}

/* Util function to get a string from the SMBIOS table

  @param StringId   - SMBIOS string id to get
  @param SmbiosHead - SMBIOS table header

  @return CHAR8 * On Success
  @return NULL On Failure
*/
STATIC
CHAR8 *
GetSmbiosTableString (
  IN     SMBIOS_TABLE_STRING      StringId,
  IN     EFI_SMBIOS_TABLE_HEADER  *SmbiosHead
  )
{
  CHAR8  *TableStrPtr;
  CHAR8  *String;
  CHAR8  *CurStr;
  UINTN  NumChars;
  UINTN  StrNum;

  DEBUG ((DEBUG_ERROR, "Getting string id %u\n", StringId));

  TableStrPtr = (CHAR8 *)SmbiosHead + SmbiosHead->Length;
  String      = NULL;
  StrNum      = 0;
  /* Walk the formatted strings section to find the string id we want */
  while ((*TableStrPtr != 0) && (*(TableStrPtr + 1) != 0)) {
    CurStr = TableStrPtr;

    for (NumChars = 0; NumChars < SMBIOS_STRING_MAX_LENGTH; NumChars++) {
      if (*(CurStr + NumChars) == 0) {
        break;
      }
    }

    /* If the string is too long, exit the table may be corrupt.*/
    if (NumChars == SMBIOS_STRING_MAX_LENGTH) {
      DEBUG ((DEBUG_ERROR, "Invalid SMBIOS string, too long, exiting\n"));
      goto ExitGetSmbiosTableString;
    }

    TableStrPtr += (NumChars + 1);
    StrNum++;
    if (StrNum == StringId) {
      String = CurStr;
      break;
    }
  }

ExitGetSmbiosTableString:
  if (String == NULL) {
    DEBUG ((DEBUG_ERROR, "String not found\n"));
  } else {
    DEBUG ((DEBUG_ERROR, "String found %a\n", String));
  }

  return String;
}

/* Util function to get memory device part index which is the last part of the device locator string.

  @param StringId   - SMBIOS string id to get
  @param SmbiosHead - SMBIOS table header

  @return UINT64 On Success
  @return 0 On Failure
*/
STATIC
INT64
GetMemoryDevicePartIndex (
  IN     SMBIOS_TABLE_STRING      StringId,
  IN     EFI_SMBIOS_TABLE_HEADER  *SmbiosHead
  )
{
  CHAR8  *PartIndexStr;
  INT64  PartIndex;

  PartIndex    = -1;
  PartIndexStr = GetSmbiosTableString (StringId, SmbiosHead);
  if (PartIndexStr == NULL) {
    return 0;
  }

  /* It is possible that the device locator string has been reformatted by an OEM in which case return -1 */
  if (AsciiStrStr (PartIndexStr, "LP5x_") == NULL) {
    DEBUG ((DEBUG_ERROR, "Part index string not found\n"));
    PartIndex = -1;
    goto ExitGetMemoryDevicePartIndex;
  }

  PartIndex = AsciiStrDecimalToUint64 (PartIndexStr + AsciiStrLen ("LP5x_"));
  DEBUG ((DEBUG_ERROR, "Part index %lld\n", PartIndex));
ExitGetMemoryDevicePartIndex:
  return PartIndex;
}

/* Util function to get memory device serial number

  @param StringId   - SMBIOS string id to get
  @param SmbiosHead - SMBIOS table header

  @return UINT64 On Success
  @return 0 On Failure
*/
STATIC
UINT64
GetMemoryDeviceSerialNumber (
  IN     SMBIOS_TABLE_STRING      StringId,
  IN     EFI_SMBIOS_TABLE_HEADER  *SmbiosHead
  )
{
  CHAR8   *SerialNumberStr;
  UINT64  SerialNumber;

  SerialNumberStr = GetSmbiosTableString (StringId, SmbiosHead);
  if (SerialNumberStr == NULL) {
    return 0;
  }

  SerialNumber = AsciiStrDecimalToUint64 (SerialNumberStr);
  DEBUG ((DEBUG_ERROR, "Serial number %llu\n", SerialNumber));
  return SerialNumber;
}

/* Util function to get SMBIOS Type17 data

  @return SmbiosType17DataType * On Success
  @return NULL On Failure
*/
STATIC
SmbiosType17DataType *
GetSmbiosType17Data (
  OUT UINTN  *SmbiosType17DataSize
  )
{
  EFI_STATUS               Status;
  EFI_SMBIOS_PROTOCOL      *SmbiosProtocol;
  EFI_SMBIOS_HANDLE        SmbiosHandle;
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE17      *SmbiosMemoryDevice;
  SMBIOS_TABLE_TYPE16      *SmbiosMemoryArray;
  UINTN                    MemoryDeviceCount;
  UINTN                    Index;
  SmbiosType17DataType     *SmbiosType17Data;

  SmbiosType17Data  = NULL;
  SmbiosHandle      = SMBIOS_HANDLE_PI_RESERVED;
  MemoryDeviceCount = 0;

  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&SmbiosProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Could not locate SMBIOS protocol.  %r\n", Status));
    goto ExitGetSmbiosType17Data;
  }

  Status = SmbiosProtocol->GetNext (SmbiosProtocol, &SmbiosHandle, NULL, &Record, NULL);
  while (!EFI_ERROR (Status) && SmbiosHandle != SMBIOS_HANDLE_PI_RESERVED) {
    if (Record->Type == SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY) {
      SmbiosMemoryArray = (SMBIOS_TABLE_TYPE16 *)Record;
      MemoryDeviceCount = SmbiosMemoryArray->NumberOfMemoryDevices;
      break;
    }

    Status = SmbiosProtocol->GetNext (SmbiosProtocol, &SmbiosHandle, NULL, &Record, NULL);
  }

  if (MemoryDeviceCount == 0) {
    DEBUG ((DEBUG_ERROR, "No memory devices found\n"));
    goto ExitGetSmbiosType17Data;
  }

  SmbiosType17Data = (SmbiosType17DataType *)AllocateZeroPool (MemoryDeviceCount * sizeof (SmbiosType17DataType));
  if (SmbiosType17Data == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate SmbiosType17Data\n"));
    goto ExitGetSmbiosType17Data;
  }

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Index        = 0;
  Status       = SmbiosProtocol->GetNext (SmbiosProtocol, &SmbiosHandle, NULL, &Record, NULL);
  while (!EFI_ERROR (Status) && (SmbiosHandle != SMBIOS_HANDLE_PI_RESERVED) && (Index < MemoryDeviceCount)) {
    if (Record->Type == SMBIOS_TYPE_MEMORY_DEVICE) {
      SmbiosMemoryDevice                   = (SMBIOS_TABLE_TYPE17 *)Record;
      SmbiosType17Data[Index].SmbiosHandle = SmbiosMemoryDevice->Hdr.Handle;
      SmbiosType17Data[Index].PartIndex    = GetMemoryDevicePartIndex (SmbiosMemoryDevice->DeviceLocator, Record);
      SmbiosType17Data[Index].SerialNumber = GetMemoryDeviceSerialNumber (SmbiosMemoryDevice->SerialNumber, Record);
      if (SmbiosType17Data[Index].SerialNumber == 0) {
        DEBUG ((DEBUG_ERROR, "Failed to get memory device serial number\n"));
        goto ErrorExitGetSmbiosType17Data;
      }

      Index++;
    }

    Status = SmbiosProtocol->GetNext (SmbiosProtocol, &SmbiosHandle, NULL, &Record, NULL);
  }

ExitGetSmbiosType17Data:
  *SmbiosType17DataSize = MemoryDeviceCount * sizeof (SmbiosType17DataType);
  return SmbiosType17Data;
ErrorExitGetSmbiosType17Data:
  FreePool (SmbiosType17Data);
  *SmbiosType17DataSize = 0;
  return NULL;
}

/**
  This function will send SMBIOS Type17 Handles to RASFW

  @param  Event    The event of notify protocol.
  @param  Context  Notify event context.
**/
VOID
EFIAPI
SmbiosRasTransferSendTables (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS            Status;
  SmbiosType17DataType  *SmbiosType17Data;
  UINTN                 SmbiosType17DataSize;

  gBS->CloseEvent (Event);

  SmbiosType17Data = GetSmbiosType17Data (&SmbiosType17DataSize);
  if ((SmbiosType17Data == NULL) || (SmbiosType17DataSize == 0)) {
    DEBUG ((DEBUG_ERROR, "Failed to get SMBIOS Type17 data\n"));
    goto ExitSmbiosRasTransferSendTables;
  }

  Status = SendSmbiosType17Data (SmbiosType17Data, SmbiosType17DataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to send SMBIOS Type17 data\n"));
    goto ExitSmbiosRasTransferSendTables;
  }

  FreePool (SmbiosType17Data);

ExitSmbiosRasTransferSendTables:
  return;
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
SmbiosRasTransferEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   ReadyToBootEvent;

  //
  // Register ReadyToBoot event to send the SMBIOS tables once they have all been installed
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  SmbiosRasTransferSendTables,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &ReadyToBootEvent
                  );

  ASSERT_EFI_ERROR (Status);
  return Status;
}
