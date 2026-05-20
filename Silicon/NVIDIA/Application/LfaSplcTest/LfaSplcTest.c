/** @file
  UEFI Shell application to test the LFA save path by injecting a synthetic
  FFA SPLC_STOP (LIVE_ACT) direct-request message to the StandaloneMM SP.

  Usage: LfaSplcTest [<LfaStoreBase>]

    LfaStoreBase  Optional hex address of the LFA state store
                  (psc-mbox15-socket0 base + 0x1040).  When supplied the
                  store header and entry list are dumped and validated after
                  the save completes.

  Example:
    LfaSplcTest 0x800001040

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/StMmDxeGetBuffersLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Protocol/ShellParameters.h>
#include <StandaloneMm/LfaStore.h>

// Lifecycle constants mirroring ArmStandaloneMmCoreEntryPoint.c
#define FFA_FWK_FLAG(s)  ((1u << 31) | (0xFF & (s)))
#define FFA_SPLC_STOP             0x0Au
#define FFA_SPLC_RESP             0x0Bu
#define FFA_STOP_REASON_LIVE_ACT  0x01u

// FFA partition ID of the NS world (DXE caller)
#define FFA_NS_CALLER_ID  0x0000U

/**
  Send FFA_MSG_SEND_DIRECT_REQ carrying SPLC_STOP/LIVE_ACT to the SP.

  @param[in]  StmmPartId  StandaloneMM partition ID.
  @param[out] RespStatus  SPLC_RESP Arg3 (0 = success).

  @retval EFI_SUCCESS         SPLC_RESP received with no FFA-level error.
  @retval EFI_DEVICE_ERROR    FFA_ERROR returned by firmware.
  @retval EFI_PROTOCOL_ERROR  Unexpected response FID or Arg2.
**/
STATIC
EFI_STATUS
SendSplcStop (
  IN  UINT16  StmmPartId,
  OUT UINTN   *RespStatus
  )
{
  ARM_SMC_ARGS  Args;

  ZeroMem (&Args, sizeof (Args));
  Args.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH64;
  Args.Arg1 = ((UINTN)FFA_NS_CALLER_ID << 16) | StmmPartId;
  Args.Arg2 = FFA_FWK_FLAG (FFA_SPLC_STOP);
  Args.Arg3 = FFA_STOP_REASON_LIVE_ACT;

  ArmCallSmc (&Args);

  if (Args.Arg0 == ARM_FID_FFA_ERROR) {
    Print (L"  FFA_ERROR: code 0x%lx\n", (UINTN)Args.Arg2);
    return EFI_DEVICE_ERROR;
  }

  if (Args.Arg0 != ARM_FID_FFA_MSG_SEND_DIRECT_RESP_AARCH64) {
    Print (L"  Unexpected response FID 0x%lx\n", (UINTN)Args.Arg0);
    return EFI_PROTOCOL_ERROR;
  }

  if (Args.Arg2 != (UINTN)FFA_FWK_FLAG (FFA_SPLC_RESP)) {
    Print (
      L"  Bad Arg2 0x%lx (expected SPLC_RESP 0x%08x)\n",
      (UINTN)Args.Arg2,
      FFA_FWK_FLAG (FFA_SPLC_RESP)
      );
    return EFI_PROTOCOL_ERROR;
  }

  *RespStatus = Args.Arg3;
  return EFI_SUCCESS;
}

/**
  Dump and validate an LFA state store starting at StoreBase.

  Prints the header fields and walks all LFA_STORE_ENTRYs.

  @param[in] StoreBase  Virtual address of the LFA state store.

  @retval EFI_SUCCESS          Header magic/version valid.
  @retval EFI_VOLUME_CORRUPTED Header magic or version mismatch.
**/
STATIC
EFI_STATUS
DumpLfaStore (
  IN UINTN  StoreBase
  )
{
  LFA_STORE_HEADER  *Hdr;
  LFA_STORE_ENTRY   *Entry;
  UINTN             Offset;
  UINT32            I;

  Hdr = (LFA_STORE_HEADER *)StoreBase;

  Print (L"  LFA store @ 0x%lx:\n", StoreBase);
  Print (
    L"    Magic   : 0x%08x  (%a)\n",
    Hdr->Magic,
    (Hdr->Magic == LFA_STORE_MAGIC) ? "OK" : "BAD"
    );
  Print (
    L"    Version : %u  (want %u, %a)\n",
    Hdr->Version,
    LFA_STORE_VERSION,
    (Hdr->Version == LFA_STORE_VERSION) ? "OK" : "BAD"
    );
  Print (L"    Entries : %u\n", Hdr->EntryCount);
  Print (L"    TotalSz : %u bytes\n", Hdr->TotalSize);

  if ((Hdr->Magic != LFA_STORE_MAGIC) || (Hdr->Version != LFA_STORE_VERSION)) {
    Print (L"  Store header invalid — save may have failed or LFA disabled.\n");
    return EFI_VOLUME_CORRUPTED;
  }

  Offset = sizeof (LFA_STORE_HEADER);
  for (I = 0; I < Hdr->EntryCount; I++) {
    Entry = (LFA_STORE_ENTRY *)(StoreBase + Offset);
    Print (
      L"    [%u] GUID %g  DataSize %u bytes\n",
      I,
      &Entry->EntryGuid,
      Entry->DataSize
      );
    Offset += LFA_STORE_ENTRY_TOTAL_SIZE (Entry->DataSize);
    if (Offset > Hdr->TotalSize) {
      Print (L"    [corrupt] entry offset overruns TotalSize!\n");
      return EFI_VOLUME_CORRUPTED;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LfaSplcTestMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                     Status;
  UINT16                         StmmPartId;
  UINTN                          RespStatus;
  EFI_SHELL_PARAMETERS_PROTOCOL  *ShellParams;
  UINTN                          StoreBase;

  StoreBase = 0;

  Print (L"LFA SPLC_STOP injection test\n\n");

  // Optional: parse LFA store address from argv[1]
  if (!EFI_ERROR (
         gBS->HandleProtocol (
                ImageHandle,
                &gEfiShellParametersProtocolGuid,
                (VOID **)&ShellParams
                )
         ) &&
      (ShellParams->Argc >= 2))
  {
    StoreBase = StrHexToUintn (ShellParams->Argv[1]);
    if (StoreBase != 0) {
      Print (L"  LFA store address: 0x%lx\n", StoreBase);
    }
  }

  // Step 1: Discover StandaloneMM partition ID via FFA_PARTITION_INFO_GET_REGS
  Status = GetStmmPartId (&StmmPartId);
  if (EFI_ERROR (Status)) {
    Print (L"  FAIL: GetStmmPartId: %r\n", Status);
    return Status;
  }

  Print (L"  StMM Partition ID: 0x%04x\n", StmmPartId);

  // Step 2: Send SPLC_STOP and verify SPLC_RESP
  Print (L"  Sending SPLC_STOP (LIVE_ACT) ...\n");
  Status = SendSplcStop (StmmPartId, &RespStatus);
  if (EFI_ERROR (Status)) {
    Print (L"\nFAIL: %r\n", Status);
    return Status;
  }

  if (RespStatus != 0) {
    Print (L"  FAIL: SPLC_RESP Arg3 = 0x%lx (want 0)\n", RespStatus);
    return EFI_DEVICE_ERROR;
  }

  Print (L"  SPLC_RESP: OK (status 0)\n\n");

  // Step 3: Validate the LFA store contents if an address was given
  if (StoreBase != 0) {
    Status = DumpLfaStore (StoreBase);
    if (EFI_ERROR (Status)) {
      Print (L"\nFAIL: LFA store validation: %r\n", Status);
      return Status;
    }

    Print (L"\n");
  } else {
    Print (
      L"  Tip: pass psc-mbox15-socket0 base + 0x1040 to verify store contents.\n"
      L"  e.g.: LfaSplcTest 0x<addr>\n\n"
      );
  }

  Print (L"PASS\n");
  return EFI_SUCCESS;
}
