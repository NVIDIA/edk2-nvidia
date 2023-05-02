/** @file

  Null IPMI driver

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright 1999 - 2021 Intel Corporation. <BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <IndustryStandard/Ipmi.h>
#include <Protocol/IpmiTransportProtocol.h>

/**
  This service enables submitting commands via Ipmi.

  @param[in]         This              This point for IPMI_PROTOCOL structure.
  @param[in]         NetFunction       Net function of the command.
  @param[in]         Command           IPMI Command.
  @param[in]         RequestData       Command Request Data.
  @param[in]         RequestDataSize   Size of Command Request Data.
  @param[out]        ResponseData      Command Response Data. The completion code is the first byte of response data.
  @param[in, out]    ResponseDataSize  Size of Command Response Data.

  @retval EFI_UNSUPPORTED
**/
EFI_STATUS
EFIAPI
IpmiSubmitCommandNull (
  IN IPMI_TRANSPORT  *This,
  IN UINT8           NetFunction,
  IN UINT8           Lun,
  IN UINT8           Command,
  IN UINT8           *CommandData,
  IN UINT32          CommandDataSize,
  OUT UINT8          *ResponseData,
  OUT UINT32         *ResponseDataSize
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Routine Description:
  Updates the BMC status and returns the Com Address

  @param[in] This        - Pointer to IPMI protocol instance
  @param[out] BmcStatus   - BMC status
  @param[out] ComAddress  - Com Address

  @retval EFI_UNSUPPORTED
**/
EFI_STATUS
EFIAPI
GetBmcStatusNull (
  IN IPMI_TRANSPORT   *This,
  OUT BMC_STATUS      *BmcStatus,
  OUT SM_COM_ADDRESS  *ComAddress
  )
{
  return EFI_UNSUPPORTED;
}

IPMI_TRANSPORT  mIpmiTransportNull = {
  0,
  IpmiSubmitCommandNull,
  GetBmcStatusNull,
  0,
  0
};

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
IpmiNullDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT32      Count;

  Count  = 0;
  Status = GetMatchingEnabledDeviceTreeNodes ("ssif-bmc", NULL, &Count);
  if (Status != EFI_NOT_FOUND) {
    return EFI_UNSUPPORTED;
  }

  //
  // If SSIF is not supported, install NULL IPMI protocol to resolve DEPEX
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gIpmiTransportProtocolGuid,
                  &mIpmiTransportNull,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
