/** @file

  BPMP IPC Driver

  SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>

#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <Protocol/BpmpIpc.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DeviceDiscoveryLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include "BpmpIpcDxePrivate.h"

/**
  This is function is caused to allow the system to check if this implementation supports
  the device tree node..

  @param DeviceInfo        - Info regarding device tree base address,node offset,
                             device type and init function.
  @return EFI_SUCCESS      The node is supported by this instance
  @return EFI_UNSUPPORTED  The node is not supported by this instance
**/
EFI_STATUS
BpmpDeviceTreeIsSupported (
  IN OUT  NVIDIA_DT_NODE_INFO  *DeviceInfo
  )
{
  if ((DeviceInfo == NULL) ||
      (DeviceInfo->DeviceTreeBase == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (!EFI_ERROR (DeviceTreeCheckNodeSingleCompatibility ("nvidia,*-bpmp", DeviceInfo->NodeOffset))) {
    DeviceInfo->DeviceType = &gNVIDIANonDiscoverableBpmpDeviceGuid;
    return EFI_SUCCESS;
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
  This is function is caused to allow the system to check if this implementation supports
  the device tree node..

  @param DeviceInfo        - Info regarding device tree base address,node offset,
                             device type and init function.
  @return EFI_SUCCESS      The node is supported by this instance
  @return EFI_UNSUPPORTED  The node is not supported by this instance
**/
EFI_STATUS
HspDeviceTreeIsSupported (
  IN OUT  NVIDIA_DT_NODE_INFO  *DeviceInfo
  )
{
  if ((DeviceInfo == NULL) ||
      (DeviceInfo->DeviceTreeBase == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (!EFI_ERROR (DeviceTreeCheckNodeSingleCompatibility ("nvidia,*-hsp", DeviceInfo->NodeOffset))) {
    // Only support hsp with doorbell
    CONST CHAR8  *InterruptNames;
    INT32        NamesLength;

    InterruptNames = (CONST CHAR8 *)fdt_getprop (DeviceInfo->DeviceTreeBase, DeviceInfo->NodeOffset, "interrupt-names", &NamesLength);
    if ((InterruptNames == NULL) || (NamesLength == 0)) {
      return EFI_UNSUPPORTED;
    }

    while (NamesLength > 0) {
      INT32  Size = AsciiStrSize (InterruptNames);
      if ((Size <= 0) || (Size > NamesLength)) {
        return EFI_UNSUPPORTED;
      }

      if (0 == AsciiStrnCmp (InterruptNames, "doorbell", Size)) {
        DeviceInfo->DeviceType = &gNVIDIANonDiscoverableHspTopDeviceGuid;
        return EFI_SUCCESS;
      }

      NamesLength    -= Size;
      InterruptNames += Size;
    }

    return EFI_UNSUPPORTED;
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
  This function allows for a remote IPC to the BPMP firmware to be executed.
  This is a dummy version that is used if BPMP is not present.

  @param[in]     This                The instance of the NVIDIA_BPMP_IPC_PROTOCOL.
  @param[in,out] Token               Optional pointer to a token structure, if this is NULL
                                     this API will process IPC in a blocking manner.
  @param[in]     MessageRequest      Id of the message to send
  @param[in]     TxData              Pointer to the payload data to send
  @param[in]     TxDataSize          Size of the TxData buffer
  @param[out]    RxData              Pointer to the payload data to receive
  @param[in]     RxDataSize          Size of the RxData buffer
  @param[out]    MessageError        If not NULL, will contain the BPMP error code on return

  @return EFI_SUCCESS               If Token is not NULL IPC has been queued.
  @return EFI_SUCCESS               If Token is NULL IPC has been completed.
  @return EFI_INVALID_PARAMETER     Token is not NULL but Token->Event is NULL
  @return EFI_INVALID_PARAMETER     TxData or RxData are NULL
  @return EFI_DEVICE_ERROR          Failed to send IPC
**/
EFI_STATUS
BpmpIpcDummyCommunicate (
  IN  NVIDIA_BPMP_IPC_PROTOCOL *This,
  IN  OUT NVIDIA_BPMP_IPC_TOKEN *Token, OPTIONAL
  IN  UINT32                     BpmpPhandle,
  IN  UINT32                     MessageRequest,
  IN  VOID                       *TxData,
  IN  UINTN                      TxDataSize,
  OUT VOID                       *RxData,
  IN  UINTN                      RxDataSize,
  IN  INT32                      *MessageError OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

CONST NVIDIA_BPMP_IPC_PROTOCOL  mBpmpDummyProtocol = {
  BpmpIpcDummyCommunicate
};

/**
 This routine finds all supported device tree nodes and installs memory regions
 to hob list.

 @param DeviceTreeBase          A pointer to device tree.
 @param IsNodeSupported         A function pointer which checks if a device tree node is supported.
 @param ImageHandle             Handle of the loaded driver.
 @param[out] DeviceInfo         A pointer to the list of supported device tree nodes.
 @param[out] Device             Handle of the device.
 @param[out] DeviceCount        Number of supported devices.
**/
STATIC
EFI_STATUS
ProcessDTNodes (
  IN VOID                        *DeviceTreeBase,
  IN DEVICE_TREE_NODE_SUPPORTED  IsNodeSupported,
  IN EFI_HANDLE                  ImageHandle,
  OUT NVIDIA_DT_NODE_INFO        **DeviceInfo,
  OUT NON_DISCOVERABLE_DEVICE    **Device,
  OUT UINT32                     *DeviceCount
  )
{
  EFI_STATUS  Status;
  UINT32      Index;
  EFI_HANDLE  DeviceHandle = NULL;

  *DeviceInfo = NULL;

  Status = GetSupportedDeviceTreeNodes (DeviceTreeBase, IsNodeSupported, DeviceCount, *DeviceInfo);
  if ( EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    return Status;
  } else if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }

  *DeviceInfo = AllocateZeroPool (sizeof (NVIDIA_DT_NODE_INFO) * (*DeviceCount));
  if (*DeviceInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Status = GetSupportedDeviceTreeNodes (DeviceTreeBase, IsNodeSupported, DeviceCount, *DeviceInfo);

  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  *Device = (NON_DISCOVERABLE_DEVICE *)AllocateZeroPool (sizeof (NON_DISCOVERABLE_DEVICE) * (*DeviceCount));
  if (NULL == *Device) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate device protocol.\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  for (Index = 0; Index < *DeviceCount; Index++) {
    DeviceHandle = NULL;
    Status       = ProcessDeviceTreeNodeWithHandle (&(*DeviceInfo)[Index], &(*Device)[Index], ImageHandle, &DeviceHandle);

    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != *Device) {
      FreePool (*Device);
    }

    if (NULL != *DeviceInfo) {
      FreePool (*DeviceInfo);
    }
  }

  return Status;
}

/**
  Initialize the Bpmp Ipc Protocol Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
BpmpIpcInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS               Status;
  VOID                     *DeviceTreeBase;
  UINTN                    DeviceTreeSize;
  INT32                    NodeOffset;
  NON_DISCOVERABLE_DEVICE  *BpmpDevice     = NULL;
  NON_DISCOVERABLE_DEVICE  *HspDevice      = NULL;
  NVIDIA_DT_NODE_INFO      *HspDeviceInfo  = NULL;
  NVIDIA_DT_NODE_INFO      *BpmpDeviceInfo = NULL;
  UINT32                   HspDeviceCount  = 0;
  UINT32                   BpmpDeviceCount = 0;

  Status = DtPlatformLoadDtb (&DeviceTreeBase, &DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  // If BPMP is disabled on target return dummy ipc protocol
  BOOLEAN  BpmpPresent = FALSE;

  NodeOffset = fdt_node_offset_by_compatible (DeviceTreeBase, -1, "nvidia,tegra186-bpmp");
  if (NodeOffset >= 0) {
    CONST VOID  *Property    = NULL;
    INT32       PropertySize = 0;
    Property = fdt_getprop (
                 DeviceTreeBase,
                 NodeOffset,
                 "status",
                 &PropertySize
                 );
    if ((Property == NULL) || (AsciiStrCmp (Property, "okay") == 0)) {
      BpmpPresent = TRUE;
    }
  }

  if (!BpmpPresent) {
    goto Done;
  }

  // BPMP
  Status = ProcessDTNodes (DeviceTreeBase, &BpmpDeviceTreeIsSupported, ImageHandle, &BpmpDeviceInfo, &BpmpDevice, &BpmpDeviceCount);

  if (EFI_ERROR (Status)) {
    goto Done;
  }

  // HSP
  Status = ProcessDTNodes (DeviceTreeBase, &HspDeviceTreeIsSupported, ImageHandle, &HspDeviceInfo, &HspDevice, &HspDeviceCount);

  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = BpmpIpcProtocolInit (BpmpDeviceInfo, BpmpDevice, BpmpDeviceCount, HspDeviceInfo, HspDevice, HspDeviceCount);

Done:
  if (EFI_ERROR (Status) || !BpmpPresent) {
    DEBUG ((DEBUG_ERROR, "%a: WARNING: Installing dummy BPMP protocol\n", __FUNCTION__));
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ImageHandle,
                    &gNVIDIABpmpIpcProtocolGuid,
                    &mBpmpDummyProtocol,
                    &gNVIDIADummyBpmpIpcProtocolGuid,
                    NULL,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, Failed to install protocol: %r", __FUNCTION__, Status));
    }
  }

  return Status;
}
