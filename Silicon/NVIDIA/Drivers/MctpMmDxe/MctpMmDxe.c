/** @file

  MCTP protocol over MM communication driver

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>
#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/MctpProtocol.h>
#include <Protocol/MmCommunication2.h>
#include <libfdt.h>

#include "MctpMmComm.h"

#define MCTP_MM_SIGNATURE  SIGNATURE_32 ('M','C','T','P')

typedef struct {
  UINT32                  Signature;
  CHAR16                  Name[MCTP_MM_DEVICE_NAME_LENGTH];
  UINT8                   MmIndex;
  UINT8                   DeviceType;
  UINT8                   Socket;

  EFI_HANDLE              Handle;
  NVIDIA_MCTP_PROTOCOL    Protocol;
} MCTP_MM_PRIVATE;

STATIC MCTP_MM_PRIVATE  *mPrivate           = NULL;
STATIC UINTN            mNumDevices         = 0;
STATIC EFI_EVENT        mAddressChangeEvent = NULL;

EFI_MM_COMMUNICATION2_PROTOCOL  *mMctpMmCommProtocol       = NULL;
VOID                            *mMctpMmCommBuffer         = NULL;
VOID                            *mMctpMmCommBufferPhysical = NULL;

// NVIDIA_MCTP_PROTOCOL.Send()
STATIC
EFI_STATUS
EFIAPI
MctpMmSend (
  IN  NVIDIA_MCTP_PROTOCOL  *This,
  IN  BOOLEAN               IsRequest,
  IN  CONST VOID            *Message,
  IN  UINTN                 Length,
  IN OUT UINT8              *MsgTag
  )
{
  MCTP_MM_PRIVATE  *Private;
  EFI_STATUS       Status;

  Private = CR (
              This,
              MCTP_MM_PRIVATE,
              Protocol,
              MCTP_MM_SIGNATURE
              );

  Status = MctpMmSendSend (
             Private->MmIndex,
             IsRequest,
             Message,
             Length,
             MsgTag
             );

  return Status;
}

// NVIDIA_MCTP_PROTOCOL.Recv()
STATIC
EFI_STATUS
EFIAPI
MctpMmRecv (
  IN  NVIDIA_MCTP_PROTOCOL  *This,
  IN  UINTN                 TimeoutMs,
  OUT VOID                  *Message,
  IN OUT UINTN              *Length,
  OUT UINT8                 *MsgTag
  )
{
  MCTP_MM_PRIVATE  *Private;
  EFI_STATUS       Status;

  Private = CR (
              This,
              MCTP_MM_PRIVATE,
              Protocol,
              MCTP_MM_SIGNATURE
              );

  Status = MctpMmSendRecv (
             Private->MmIndex,
             TimeoutMs,
             Message,
             Length,
             MsgTag
             );

  return Status;
}

// NVIDIA_MCTP_PROTOCOL.DoRequest()
STATIC
EFI_STATUS
EFIAPI
MctpMmDoRequest (
  IN  NVIDIA_MCTP_PROTOCOL  *This,
  IN  VOID                  *Request,
  IN  UINTN                 RequestLength,
  IN  VOID                  *ResponseBuffer,
  IN  UINTN                 ResponseBufferLength,
  IN  UINTN                 *ResponseLength
  )
{
  MCTP_MM_PRIVATE  *Private;
  EFI_STATUS       Status;

  Private = CR (
              This,
              MCTP_MM_PRIVATE,
              Protocol,
              MCTP_MM_SIGNATURE
              );

  Status = MctpMmSendDoRequest (
             Private->MmIndex,
             Request,
             RequestLength,
             ResponseBuffer,
             ResponseBufferLength,
             ResponseLength
             );

  return Status;
}

// NVIDIA_MCTP_PROTOCOL.GetDeviceAttributes()
STATIC
EFI_STATUS
EFIAPI
MctpMmGetDeviceAttributes (
  IN  NVIDIA_MCTP_PROTOCOL    *This,
  IN  MCTP_DEVICE_ATTRIBUTES  *Attributes
  )
{
  MCTP_MM_PRIVATE  *Private;

  Private = CR (
              This,
              MCTP_MM_PRIVATE,
              Protocol,
              MCTP_MM_SIGNATURE
              );

  Attributes->DeviceName = Private->Name;
  Attributes->DeviceType = Private->DeviceType;
  Attributes->Socket     = Private->Socket;

  return EFI_SUCCESS;
}

/**
  Get MM MCTP device info and add them to private structure array.

  @param[in]  MaxDevices    Maximum number of devices to support.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
MctpMmAddDevices (
  IN UINTN  MaxDevices
  )
{
  MCTP_MM_PRIVATE      *Private;
  EFI_STATUS           Status;
  UINTN                DeviceCount;
  UINTN                Index;
  MCTP_MM_DEVICE_INFO  *DeviceInfo;

  DeviceInfo = (MCTP_MM_DEVICE_INFO *)AllocateRuntimeZeroPool (MaxDevices * sizeof (*DeviceInfo));
  if (DeviceInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "DeviceInfo alloc failed\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = MctpMmSendGetDevices (
             MaxDevices,
             &DeviceCount,
             DeviceInfo
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (DeviceCount == 0) {
    DEBUG ((DEBUG_INFO, "%a: no devices\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  mPrivate = (MCTP_MM_PRIVATE *)AllocateRuntimeZeroPool (
                                  DeviceCount * sizeof (*mPrivate)
                                  );
  if (mPrivate == NULL) {
    DEBUG ((DEBUG_ERROR, "mPrivate allocation failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Private = mPrivate;
  for (Index = 0; Index < DeviceCount; Index++, Private++) {
    Private->Signature = MCTP_MM_SIGNATURE;
    Private->MmIndex   = DeviceInfo[Index].MmIndex;
    StrCpyS (Private->Name, sizeof (Private->Name), DeviceInfo[Index].Name);
    Private->Socket                       = DeviceInfo[Index].Socket;
    Private->DeviceType                   = DeviceInfo[Index].Type;
    Private->Protocol.Recv                = MctpMmRecv;
    Private->Protocol.Send                = MctpMmSend;
    Private->Protocol.DoRequest           = MctpMmDoRequest;
    Private->Protocol.GetDeviceAttributes = MctpMmGetDeviceAttributes;
    mNumDevices++;
  }

  DEBUG ((DEBUG_INFO, "%a: %u devices added\n", __FUNCTION__, mNumDevices));

  Status = EFI_SUCCESS;

Done:
  if (EFI_ERROR (Status)) {
    FreePool (DeviceInfo);
    mNumDevices = 0;
    if (mPrivate != NULL) {
      FreePool (mPrivate);
      mPrivate = NULL;
    }
  }

  return Status;
}

/**
  Handle address change notification to support runtime execution.

  @param[in]  Event         Event being handled
  @param[in]  Context       Event context

  @retval None

**/
STATIC
VOID
EFIAPI
MctpMmAddressChangeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN                 Index;
  MCTP_MM_PRIVATE       *Private;
  NVIDIA_MCTP_PROTOCOL  *Protocol;

  Private = mPrivate;
  for (Index = 0; Index < mNumDevices; Index++, Private++) {
    Protocol = &Private->Protocol;
    EfiConvertPointer (0x0, (VOID **)&Protocol->Recv);
    EfiConvertPointer (0x0, (VOID **)&Protocol->Send);
    EfiConvertPointer (0x0, (VOID **)&Protocol->DoRequest);
    EfiConvertPointer (0x0, (VOID **)&Protocol->GetDeviceAttributes);
  }

  EfiConvertPointer (0x0, (VOID **)&mPrivate);
  EfiConvertPointer (0x0, (VOID **)&mMctpMmCommProtocol);
  EfiConvertPointer (0x0, (VOID **)&mMctpMmCommBuffer);
}

/**
  Check if qspi DT node has erot subnode.

  @param[in]  DeviceTreeBase  DT base pointer.
  @param[in]  QspiOffset      Node offset of Qspi in DT.
  @param[in]  NumChipSelects  Qspi number of chip selects.
  @param[out] ChipSelect      Pointer to return erot chip select.

  @retval BOOLEAN       TRUE if qspi node has erot subnode.

**/
STATIC
BOOLEAN
EFIAPI
MctpMmQspiNodeHasErot (
  IN CONST VOID  *DeviceTreeBase,
  IN INT32       QspiOffset,
  IN UINT8       NumChipSelects,
  OUT UINT8      *ChipSelect
  )
{
  CONST CHAR8  *NodeName;
  CONST CHAR8  *QspiName;
  CONST VOID   *Property;
  UINT32       NodeChipSelect;
  INT32        SubNode;
  INT32        Length;

  QspiName = fdt_get_name (DeviceTreeBase, QspiOffset, NULL);

  SubNode = 0;
  fdt_for_each_subnode (SubNode, DeviceTreeBase, QspiOffset) {
    NodeName = fdt_get_name (DeviceTreeBase, SubNode, NULL);
    if (AsciiStrnCmp (NodeName, "erot@", AsciiStrLen ("erot@")) == 0) {
      break;
    }
  }
  if (SubNode < 0) {
    DEBUG ((DEBUG_ERROR, "%a: no erot on %a\n", __FUNCTION__, QspiName));
    return FALSE;
  }

  Property = fdt_getprop (DeviceTreeBase, SubNode, "status", NULL);
  if ((Property != NULL) && (AsciiStrCmp (Property, "disabled") == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: %a disabled\n", __FUNCTION__, NodeName));
    return FALSE;
  }

  Property = fdt_getprop (DeviceTreeBase, SubNode, "reg", &Length);
  if ((Property != NULL) && (Length == sizeof (UINT32))) {
    NodeChipSelect = (UINT8)fdt32_to_cpu (*(CONST UINT32 *)Property);
    if (NodeChipSelect < NumChipSelects) {
      *ChipSelect = (UINT8)NodeChipSelect;
      DEBUG ((DEBUG_INFO, "%a: %a has %a CS=%u\n", __FUNCTION__, QspiName, NodeName, *ChipSelect));
      return TRUE;
    }
  }

  DEBUG ((DEBUG_ERROR, "%a: %a bad CS\n", __FUNCTION__, NodeName));

  return FALSE;
}

/**
  Detect erot in DT.

  @retval  BOOLEAN      Flag indicating if erot is present in DT.

**/
STATIC
BOOLEAN
EFIAPI
MctpMmHasErot (
  VOID
  )
{
  CHAR8        SocketNodeStr[] = "/socket@xx";
  VOID         *DeviceTreeBase;
  UINTN        DtbSize;
  CONST VOID   *Property;
  CONST CHAR8  *NodeName;
  UINT32       Socket;
  UINT8        ChipSelect;
  INT32        NodeOffset;
  INT32        SubNode;
  EFI_STATUS   Status;

  Status = DtPlatformLoadDtb (&DeviceTreeBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: couldn't load DT\n", __FUNCTION__));
    return FALSE;
  }

  Socket = 0;
  while (TRUE) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", Socket);
    NodeOffset = fdt_path_offset (DeviceTreeBase, SocketNodeStr);
    if (NodeOffset < 0) {
      break;
    }

    SubNode = 0;
    fdt_for_each_subnode (SubNode, DeviceTreeBase, NodeOffset) {
      NodeName = fdt_get_name (DeviceTreeBase, SubNode, NULL);
      if (AsciiStrnCmp (NodeName, "spi@", AsciiStrLen ("spi@")) == 0) {
        Property = fdt_getprop (DeviceTreeBase, SubNode, "status", NULL);
        if ((Property != NULL) && (AsciiStrCmp (Property, "disabled") == 0)) {
          DEBUG ((DEBUG_INFO, "%a: %a disabled\n", __FUNCTION__, NodeName));
          continue;
        }

        if (MctpMmQspiNodeHasErot (DeviceTreeBase, SubNode, MAX_UINT8, &ChipSelect)) {
          return TRUE;
        }
      }
    }

    Socket++;
    ASSERT (Socket < 100);      // enforce socket@xx string max
  }

  return FALSE;
}

/**
  Entry point of this driver.

  @param  ImageHandle     Image handle this driver.
  @param  SystemTable     Pointer to the System Table.

  @retval EFI_SUCCESS     The entry point is executed successfully.
  @retval other           Some error occurred when executing this entry point.

**/
EFI_STATUS
EFIAPI
MctpMmDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS       Status;
  UINTN            Index;
  MCTP_MM_PRIVATE  *Private;
  EFI_STATUS       UninstallStatus;
  UINTN            MaxDevices;

  if (!MctpMmHasErot ()) {
    return EFI_NOT_FOUND;
  }

  Status = gBS->LocateProtocol (
                  &gEfiMmCommunication2ProtocolGuid,
                  NULL,
                  (VOID **)&mMctpMmCommProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate MmCommunication protocol: %r\n", __FUNCTION__, Status));
    return Status;
  }

  mMctpMmCommBuffer = AllocateRuntimePool (MCTP_COMM_BUFFER_SIZE);
  if (mMctpMmCommBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "CommBuffer allocation failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  mMctpMmCommBufferPhysical = mMctpMmCommBuffer;

  Status = MctpMmSendInitialize (&MaxDevices);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = MctpMmAddDevices (MaxDevices);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Private = mPrivate;
  for (Index = 0; Index < mNumDevices; Index++, Private++) {
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Private->Handle,
                    &gNVIDIAMctpProtocolGuid,
                    &Private->Protocol,
                    NULL
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Couldn't install protocol for Index=%u, device=%s: %r\n",
        __FUNCTION__,
        Index,
        Private->Name,
        Status
        ));
      goto Done;
    }
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  MctpMmAddressChangeNotify,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mAddressChangeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error creating address change event Status = %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

Done:
  if (EFI_ERROR (Status)) {
    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }

    Private = mPrivate;
    for (Index = 0; Index < mNumDevices; Index++, Private++) {
      if (Private->Handle != NULL) {
        UninstallStatus = gBS->UninstallMultipleProtocolInterfaces (
                                 Private->Handle,
                                 &gNVIDIAMctpProtocolGuid,
                                 &Private->Protocol,
                                 NULL
                                 );
        if (EFI_ERROR (UninstallStatus)) {
          DEBUG ((DEBUG_ERROR, "%a: Error uninstalling protocol for device=%s: %r\n", __FUNCTION__, Private->Name, UninstallStatus));
        }

        Private->Handle = NULL;
      }
    }

    if (mPrivate != NULL) {
      FreePool (mPrivate);
      mPrivate = NULL;
    }
  }

  return Status;
}
