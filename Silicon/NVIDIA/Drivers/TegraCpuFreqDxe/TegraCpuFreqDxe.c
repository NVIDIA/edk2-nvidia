/** @file

  Tegra CPU Frequency Driver.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/BpmpIpc.h>

#pragma pack (1)
typedef struct {
  UINT32    ClusterId;
} BPMP_CPU_NDIV_LIMITS_REQUEST;

typedef struct {
  UINT32    ref_clk_hz;
  UINT16    pdiv;
  UINT16    mdiv;
  UINT16    ndiv_max;
  UINT16    ndiv_min;
} BPMP_CPU_NDIV_LIMITS_RESPONSE;
#pragma pack ()

STATIC
VOID
SetT194CpuNdiv (
  IN UINT64  Ndiv
  )
{
  asm volatile ("msr s3_0_c15_c0_4, %0" : : "r" (Ndiv));
}

/**
  Runtime Configuration Of Tegra Platform.
**/
EFI_STATUS
EFIAPI
TegraCpuFreqInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                     Status;
  UINTN                          ChipID;
  TEGRA_PLATFORM_TYPE            PlatformType;
  NVIDIA_BPMP_IPC_PROTOCOL       *BpmpIpcProtocol;
  BPMP_CPU_NDIV_LIMITS_REQUEST   Request;
  BPMP_CPU_NDIV_LIMITS_RESPONSE  Response;
  INT32                          MessageError;

  Status = gBS->LocateProtocol (
                  &gNVIDIABpmpIpcProtocolGuid,
                  NULL,
                  (VOID **)&BpmpIpcProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ChipID = TegraGetChipID ();

  PlatformType = TegraGetPlatform ();

  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    if (ChipID == T194_CHIP_ID) {
      Request.ClusterId = 0;
      Status            = BpmpIpcProtocol->Communicate (
                                             BpmpIpcProtocol,
                                             NULL,
                                             MRQ_CPU_NDIV_LIMITS,
                                             (VOID *)&Request,
                                             sizeof (BPMP_CPU_NDIV_LIMITS_REQUEST),
                                             (VOID *)&Response,
                                             sizeof (BPMP_CPU_NDIV_LIMITS_RESPONSE),
                                             &MessageError
                                             );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to request NDIV - %r\r\n", __FUNCTION__, Status));
      } else {
        SetT194CpuNdiv (Response.ndiv_max);
      }
    }
  }

  return EFI_SUCCESS;
}
