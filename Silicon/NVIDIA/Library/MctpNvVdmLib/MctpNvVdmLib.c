/** @file

  MCTP NVIDIA Vendor-Defined Message Library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MctpNvVdmLib.h>

VOID
EFIAPI
MctpNvFillVendorId (
  IN  MCTP_VDM_IANA_VENDOR_ID  *Vendor
  )
{
  UINT32  NvId = MCTP_NV_NVIDIA_IANA_ID;

  MctpUint32ToBEBuffer (Vendor->Id, NvId);
}

VOID
EFIAPI
MctpNvReqFillCommon (
  IN  MCTP_NV_VDM_COMMON  *Common,
  IN  UINT8               Command,
  IN  UINT8               Version
  )
{
  Common->Type = MCTP_TYPE_VENDOR_IANA;
  MctpNvFillVendorId (&Common->Vendor);
  Common->InstanceId = MCTP_RQ;
  Common->NvType     = MCTP_NV_TYPE_EROT;
  Common->Command    = Command;
  Common->Version    = Version;
}

VOID
EFIAPI
MctpNvBootCompleteFillReq (
  OUT MCTP_NV_BOOT_COMPLETE_REQUEST  *Request,
  IN UINTN                           BootSlot
  )
{
  MctpNvReqFillCommon (
    &Request->Common,
    MCTP_NV_CMD_BOOT_COMPLETE,
    MCTP_NV_VER_BOOT_COMPLETE
    );

  Request->BootSlot = (UINT8)BootSlot | MCTP_NV_BOOT_COMPLETE_SLOT_VALID;

  ZeroMem (Request->Reserved, sizeof (Request->Reserved));
}
