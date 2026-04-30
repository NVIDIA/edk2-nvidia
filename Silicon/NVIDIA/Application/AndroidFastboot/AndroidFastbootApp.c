/** @file
  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AndroidFastbootApp.h"
#include "FastbootMenu.h"

#include <Uefi.h>

#include <Library/BaseLib.h>

#include <Protocol/AndroidFastbootTransport.h>
#include <Protocol/AndroidFastbootPlatform.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

#include <Guid/FmpCapsule.h>
#include <Guid/NVIDIAPublicVariableGuid.h>

#include <Protocol/BootChainProtocol.h>

#include <Library/PcdLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/PrintLib.h>
#include <Library/AndroidBcbLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/AvbLib.h>

/*
 * UEFI Application using the FASTBOOT_TRANSPORT_PROTOCOL and
 * FASTBOOT_PLATFORM_PROTOCOL to implement the Android Fastboot protocol.
 */

STATIC FASTBOOT_TRANSPORT_PROTOCOL  *mTransport;
STATIC FASTBOOT_PLATFORM_PROTOCOL   *mPlatform;

STATIC EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *mTextOut;
STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL   *mTextIn;

typedef enum {
  ExpectCmdState,
  ExpectDataState,
  FastbootStateMax
} ANDROID_FASTBOOT_STATE;

STATIC ANDROID_FASTBOOT_STATE  mState = ExpectCmdState;

// When in ExpectDataState, the number of bytes of data to expect:
STATIC UINT64  mNumDataBytes;
// .. and the number of bytes so far received this data phase
STATIC UINT64  mBytesReceivedSoFar;
// .. and the buffer to save data into
STATIC UINT8  *mDataBuffer = NULL;

// Event notify functions, from which gBS->Exit shouldn't be called, can signal
// this event when the application should exit
STATIC EFI_EVENT  mFinishedEvent;

STATIC EFI_EVENT  mFatalSendErrorEvent;

// This macro uses sizeof - only use it on array (i.e. string literals)
#define SEND_LITERAL(Str)            mTransport->Send (       \
                                        sizeof (Str) - 1,     \
                                        Str,                  \
                                        &mFatalSendErrorEvent \
                                        )
#define MATCH_CMD_LITERAL(Cmd, Buf)  !AsciiStrnCmp (Cmd, Buf, sizeof (Cmd) - 1)

#define IS_LOWERCASE_ASCII(Char)  (Char >= 'a' && Char <= 'z')

#define FASTBOOT_STRING_MAX_LENGTH   256
#define FASTBOOT_COMMAND_MAX_LENGTH  64

STATIC
VOID
HandleGetVar (
  IN CHAR8  *CmdArg
  )
{
  CHAR8       Response[FASTBOOT_COMMAND_MAX_LENGTH + 1] = "OKAY";
  EFI_STATUS  Status;

  // Respond to getvar:version with 0.4 (version of Fastboot protocol)
  if (!AsciiStrnCmp ("version", CmdArg, sizeof ("version") - 1)) {
    SEND_LITERAL ("OKAY" ANDROID_FASTBOOT_VERSION);
  } else {
    // All other variables are assumed to be platform specific
    Status = mPlatform->GetVar (CmdArg, Response + 4);
    if (EFI_ERROR (Status)) {
      SEND_LITERAL ("FAILSomething went wrong when looking up the variable");
    } else {
      mTransport->Send (AsciiStrLen (Response), Response, &mFatalSendErrorEvent);
    }
  }
}

STATIC
VOID
HandleDownload (
  IN CHAR8  *NumBytesString
  )
{
  CHAR8   Response[13];
  CHAR16  OutputString[FASTBOOT_STRING_MAX_LENGTH];

  // Argument is 8-character ASCII string hex representation of number of bytes
  // that will be sent in the data phase.
  // Response is "DATA" + that same 8-character string.

  // Replace any previously downloaded data
  if (mDataBuffer != NULL) {
    FreePool (mDataBuffer);
    mDataBuffer = NULL;
  }

  // Parse out number of data bytes to expect
  mNumDataBytes = AsciiStrHexToUint64 (NumBytesString);
  if (mNumDataBytes == 0) {
    mTextOut->OutputString (mTextOut, L"ERROR: Fail to get the number of bytes to download.\r\n");
    FastbootMenuSetStatus (L"Fastboot: download failed (invalid size)");
    SEND_LITERAL ("FAILFailed to get the number of bytes to download");
    return;
  }

  UnicodeSPrint (OutputString, sizeof (OutputString), L"Downloading %d bytes\r\n", mNumDataBytes);
  mTextOut->OutputString (mTextOut, OutputString);
  FastbootMenuSetStatus (L"Fastboot: download starting (%Lu bytes)", mNumDataBytes);

  mDataBuffer = AllocatePool (mNumDataBytes);
  if (mDataBuffer == NULL) {
    SEND_LITERAL ("FAILNot enough memory");
  } else {
    ZeroMem (Response, sizeof Response);
    AsciiSPrint (
      Response,
      sizeof Response,
      "DATA%x",
      (UINT32)mNumDataBytes
      );
    mTransport->Send (sizeof Response - 1, Response, &mFatalSendErrorEvent);

    mState              = ExpectDataState;
    mBytesReceivedSoFar = 0;
  }
}

STATIC
VOID
HandleOemBootloaderUpdate (
  VOID
  )
{
  EFI_STATUS                  Status;
  EFI_CAPSULE_HEADER          *CapsuleHeader;
  NVIDIA_BOOT_CHAIN_PROTOCOL  *BootChainProtocol;
  UINT8                       BootChain;

  mTextOut->OutputString (mTextOut, L"OEM bootloader_update: processing capsule via UpdateCapsule\r\n");

  if (mDataBuffer == NULL) {
    SEND_LITERAL ("FAILNo payload downloaded. Use 'fastboot stage' first");
    return;
  }

  if (mNumDataBytes < sizeof (EFI_CAPSULE_HEADER)) {
    SEND_LITERAL ("FAILPayload too small to be a valid capsule");
    return;
  }

  CapsuleHeader = (EFI_CAPSULE_HEADER *)mDataBuffer;
  if (!CompareGuid (&CapsuleHeader->CapsuleGuid, &gEfiFmpCapsuleGuid)) {
    DEBUG ((DEBUG_ERROR, "bootloader_update: invalid capsule GUID, not FMP\n"));
    SEND_LITERAL ("FAILInvalid capsule: not an FMP capsule");
    return;
  }

  if (CapsuleHeader->CapsuleImageSize > mNumDataBytes) {
    SEND_LITERAL ("FAILCapsule image size exceeds downloaded data");
    return;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIABootChainProtocolGuid,
                  NULL,
                  (VOID **)&BootChainProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "bootloader_update: BootChainProtocol not found: %r\n", Status));
    SEND_LITERAL ("FAILCannot determine active boot chain");
    return;
  }

  BootChain = (UINT8)BootChainProtocol->ActiveBootChain;

  DEBUG ((
    DEBUG_ERROR,
    "bootloader_update: capsule size=%u, current slot=%u\n",
    CapsuleHeader->CapsuleImageSize,
    BootChain
    ));

  DEBUG ((
    DEBUG_ERROR,
    "bootloader_update: calling gRT->UpdateCapsule, size=%u\n",
    CapsuleHeader->CapsuleImageSize
    ));

  Status = gRT->UpdateCapsule (&CapsuleHeader, 1, (UINTN)NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "bootloader_update: UpdateCapsule failed: %r\n", Status));
    SEND_LITERAL ("FAILUpdateCapsule failed");
    return;
  }

  DEBUG ((DEBUG_ERROR, "bootloader_update: capsule processed successfully\n"));
  SEND_LITERAL ("OKAY");
}

/**
  Read the AVB locked state. On read failure, fail-safe to TRUE so a
  flaky RPMB / TA cannot be used to bypass lock-state enforcement.
**/
STATIC
BOOLEAN
IsBootloaderLocked (
  VOID
  )
{
  EFI_STATUS  Status;
  BOOLEAN     IsLocked;

  Status = AvbReadDeviceLockedState (&IsLocked);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read locked state: %r (assuming locked)\n", __FUNCTION__, Status));
    return TRUE;
  }

  return IsLocked;
}

STATIC
VOID
HandleFlash (
  IN CHAR8  *PartitionName
  )
{
  EFI_STATUS  Status;
  CHAR16      OutputString[FASTBOOT_STRING_MAX_LENGTH];

  if (IsBootloaderLocked ()) {
    SEND_LITERAL ("FAILBootloader is locked.");
    return;
  }

  // Build output string
  UnicodeSPrint (OutputString, sizeof (OutputString), L"Flashing partition %a\r\n", PartitionName);
  mTextOut->OutputString (mTextOut, OutputString);
  FastbootMenuSetStatus (L"Fastboot: flashing %a", PartitionName);

  if (mDataBuffer == NULL) {
    // Doesn't look like we were sent any data
    SEND_LITERAL ("FAILNo data to flash");
    return;
  }

  if (MATCH_CMD_LITERAL ("staging", PartitionName)) {
    SEND_LITERAL ("INFONote: This is a bootloader update, not a partition write.");
    HandleOemBootloaderUpdate ();
    return;
  }

  Status = mPlatform->FlashPartition (
                        PartitionName,
                        mNumDataBytes,
                        mDataBuffer
                        );
  if (Status == EFI_NOT_FOUND) {
    SEND_LITERAL ("FAILNo such partition.");
    mTextOut->OutputString (mTextOut, L"No such partition.\r\n");
    FastbootMenuSetStatus (L"Fastboot: flash failed (no partition)");
  } else if (EFI_ERROR (Status)) {
    SEND_LITERAL ("FAILError flashing partition.");
    mTextOut->OutputString (mTextOut, L"Error flashing partition.\r\n");
    DEBUG ((DEBUG_ERROR, "Couldn't flash image:  %r\n", Status));
    FastbootMenuSetStatus (L"Fastboot: flash error");
  } else {
    mTextOut->OutputString (mTextOut, L"Done.\r\n");
    FastbootMenuSetStatus (L"Fastboot: flash OK — %a", PartitionName);
    SEND_LITERAL ("OKAY");
  }
}

STATIC
VOID
HandleErase (
  IN CHAR8  *PartitionName
  )
{
  EFI_STATUS  Status;
  CHAR16      OutputString[FASTBOOT_STRING_MAX_LENGTH];

  if (IsBootloaderLocked ()) {
    SEND_LITERAL ("FAILBootloader is locked.");
    return;
  }

  // Build output string
  UnicodeSPrint (OutputString, sizeof (OutputString), L"Erasing partition %a\r\n", PartitionName);
  mTextOut->OutputString (mTextOut, OutputString);
  FastbootMenuSetStatus (L"Fastboot: erasing %a", PartitionName);

  Status = mPlatform->ErasePartition (PartitionName);
  if (EFI_ERROR (Status)) {
    SEND_LITERAL ("FAILCheck device console.");
    DEBUG ((DEBUG_ERROR, "Couldn't erase image:  %r\n", Status));
    FastbootMenuSetStatus (L"Fastboot: erase failed");
  } else {
    SEND_LITERAL ("OKAY");
    FastbootMenuSetStatus (L"Fastboot: erase OK — %a", PartitionName);
  }
}

STATIC
VOID
HandleBoot (
  VOID
  )
{
  EFI_STATUS  Status;

  if (IsBootloaderLocked ()) {
    SEND_LITERAL ("FAILBootloader is locked.");
    return;
  }

  mTextOut->OutputString (mTextOut, L"Booting downloaded image\r\n");
  FastbootMenuSetStatus (L"Fastboot: booting downloaded image");

  if (mDataBuffer == NULL) {
    // Doesn't look like we were sent any data
    SEND_LITERAL ("FAILNo image in memory");
    return;
  }

  // We don't really have any choice but to report success, because once we
  // boot we lose control of the system.
  SEND_LITERAL ("OKAY");

  Status = BootAndroidBootImg (mNumDataBytes, mDataBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to boot downloaded image: %r\n", Status));
  }

  // We shouldn't get here
}

#define FAC_RST_PROTECTION_PARTITION_NAME  L"fac_rst_protection"
#define GPT_PARTITION_NAME_LENGTH          36

//
// Partitions wiped on factory reset / lock / unlock. MSC (BCB) and
// fac_rst_protection are deliberately left out. Wipe mechanism (full
// vs head-only, chunk size, etc.) is decided by the platform driver.
//
STATIC CONST CHAR8  *CONST  mFactoryResetPartitions[] = {
  "userdata",
  "CAC",
  "MDA",
};

/**
  Internal: erase the partitions listed in mFactoryResetPartitions
  (userdata, CAC, MDA). Missing partitions are treated as benign and
  skipped; fail-fast on the first hard error.

  This raw wipe deliberately does NOT consult fac_rst_protection.
  Public entry points (FastbootFactoryReset, FastbootUnlockBootloader)
  are responsible for the FRP gate; FastbootLockBootloader is the
  intentional exception (lock is the safe direction and must not be
  blocked by FRP).
**/
STATIC
EFI_STATUS
EraseFactoryResetPartitions (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       Idx;
  CHAR8       Info[96];
  UINTN       InfoLen;

  for (Idx = 0; Idx < ARRAY_SIZE (mFactoryResetPartitions); Idx++) {
    //
    // Per-partition INFO progress is only meaningful in the fastboot
    // command path; menu / boot-time callers run with mTransport ==
    // NULL and report progress through their own UI.
    //
    if (mTransport != NULL) {
      InfoLen = AsciiSPrint (
                  Info,
                  sizeof (Info),
                  "INFOerasing %a ...",
                  mFactoryResetPartitions[Idx]
                  );
      mTransport->Send (InfoLen, Info, &mFatalSendErrorEvent);
    }

    Status = mPlatform->ErasePartition ((CHAR8 *)mFactoryResetPartitions[Idx]);
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DEBUG_WARN, "%a: %a: not present, skipping\n", __FUNCTION__, mFactoryResetPartitions[Idx]));
      continue;
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: %a: erase failed: %r\n", __FUNCTION__, mFactoryResetPartitions[Idx], Status));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetUnlockAbility (
  OUT BOOLEAN  *UnlockAllowed
  )
{
  EFI_STATUS                   Status;
  UINTN                        NumOfHandles;
  EFI_HANDLE                   *HandleBuffer;
  UINTN                        Index;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;
  EFI_DISK_IO_PROTOCOL         *DiskIo;
  UINT32                       MediaId;
  UINTN                        PartitionSize;
  UINT8                        UnlockAbilityFlag;

  if (UnlockAllowed == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *UnlockAllowed = FALSE;

  NumOfHandles = 0;
  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiPartitionInfoProtocolGuid,
                        NULL,
                        &NumOfHandles,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status) || (NumOfHandles == 0) || (HandleBuffer == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate partition handles: %r\n", __FUNCTION__, Status));
    return EFI_NOT_FOUND;
  }

  for (Index = 0; Index < NumOfHandles; Index++) {
    PartitionInfo = NULL;
    Status        = gBS->HandleProtocol (
                           HandleBuffer[Index],
                           &gEfiPartitionInfoProtocolGuid,
                           (VOID **)&PartitionInfo
                           );
    if (EFI_ERROR (Status) || (PartitionInfo == NULL)) {
      continue;
    }

    if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
      continue;
    }

    if (0 == StrnCmp (
               PartitionInfo->Info.Gpt.PartitionName,
               FAC_RST_PROTECTION_PARTITION_NAME,
               StrnLenS (FAC_RST_PROTECTION_PARTITION_NAME, GPT_PARTITION_NAME_LENGTH)
               ))
    {
      break;
    }
  }

  if (Index >= NumOfHandles) {
    DEBUG ((DEBUG_ERROR, "%a: Partition %s not found\n", __FUNCTION__, FAC_RST_PROTECTION_PARTITION_NAME));
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  BlockIo = NULL;
  Status  = gBS->HandleProtocol (
                   HandleBuffer[Index],
                   &gEfiBlockIoProtocolGuid,
                   (VOID **)&BlockIo
                   );
  if (EFI_ERROR (Status) || (BlockIo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get BlockIo: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  DiskIo = NULL;
  Status = gBS->HandleProtocol (
                  HandleBuffer[Index],
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo
                  );
  if (EFI_ERROR (Status) || (DiskIo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get DiskIo: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  MediaId           = BlockIo->Media->MediaId;
  PartitionSize     = (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  UnlockAbilityFlag = 0;

  if (PartitionSize == 0) {
    DEBUG ((DEBUG_ERROR, "%a: %s partition size is 0\n", __FUNCTION__, FAC_RST_PROTECTION_PARTITION_NAME));
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  Status = DiskIo->ReadDisk (DiskIo, MediaId, PartitionSize - 1, sizeof (UnlockAbilityFlag), &UnlockAbilityFlag);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read %s: %r\n", __FUNCTION__, FAC_RST_PROTECTION_PARTITION_NAME, Status));
    goto Exit;
  }

  *UnlockAllowed = (UnlockAbilityFlag != 0);
  DEBUG ((DEBUG_INFO, "%a: unlock_ability = %d\n", __FUNCTION__, UnlockAbilityFlag));

Exit:
  if (HandleBuffer != NULL) {
    gBS->FreePool (HandleBuffer);
  }

  return Status;
}

EFI_STATUS
FastbootFactoryReset (
  VOID
  )
{
  EFI_STATUS  Status;
  BOOLEAN     UnlockAllowed;

  Status = GetUnlockAbility (&UnlockAllowed);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read unlock ability: %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (!UnlockAllowed) {
    DEBUG ((DEBUG_ERROR, "%a: factory reset blocked by fac_rst_protection\n", __FUNCTION__));
    return EFI_ACCESS_DENIED;
  }

  return EraseFactoryResetPartitions ();
}

EFI_STATUS
FastbootLockBootloader (
  VOID
  )
{
  EFI_STATUS  Status;

  //
  // Lock is the safe direction (re-enabling AVB protection); always
  // wipe user data without consulting fac_rst_protection so an
  // unlocked device can always be re-locked from the menu.
  //
  Status = EraseFactoryResetPartitions ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AvbWriteDeviceLockedState (1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: write locked state: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

EFI_STATUS
FastbootUnlockBootloader (
  VOID
  )
{
  EFI_STATUS  Status;
  BOOLEAN     UnlockAllowed;

  Status = GetUnlockAbility (&UnlockAllowed);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read unlock ability: %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (!UnlockAllowed) {
    DEBUG ((DEBUG_ERROR, "%a: OEM unlock blocked by fac_rst_protection\n", __FUNCTION__));
    return EFI_ACCESS_DENIED;
  }

  Status = EraseFactoryResetPartitions ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AvbWriteDeviceLockedState (0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: write locked state: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

STATIC
VOID
HandleFlashingCommand (
  IN CHAR8  *Command
  )
{
  EFI_STATUS  Status;
  BOOLEAN     IsLocked;
  BOOLEAN     WantLocked;

  if (!AsciiStrCmp (Command, "lock")) {
    WantLocked = TRUE;
  } else if (!AsciiStrCmp (Command, "unlock")) {
    WantLocked = FALSE;
  } else {
    SEND_LITERAL ("FAILUnknown flashing command");
    return;
  }

  Status = AvbReadDeviceLockedState (&IsLocked);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read locked state: %r\n", __FUNCTION__, Status));
    SEND_LITERAL ("FAILBootloader lock state unknown");
    return;
  }

  if (IsLocked == WantLocked) {
    if (WantLocked) {
      SEND_LITERAL ("FAILBootloader is already locked.");
    } else {
      SEND_LITERAL ("FAILBootloader is already unlocked.");
    }

    return;
  }

  //
  // Even when the host issued the lock/unlock command over USB the
  // user must still physically confirm at the device console -- this
  // mirrors stock Android `fastboot oem unlock` behaviour and stops a
  // remote (or unattended-USB) attacker from flipping the AVB lock
  // bit without anyone in front of the screen.
  //
  SEND_LITERAL ("INFONote: please Confirm/Cancel on screen");

  {
    BOOLEAN  Confirmed;

    Confirmed = WantLocked
                  ? FastbootMenuLockConfirmFromHost ()
                  : FastbootMenuUnlockConfirmFromHost ();
    if (!Confirmed) {
      if (WantLocked) {
        SEND_LITERAL ("FAILLock cancelled at device");
        FastbootMenuSetStatus (L"Fastboot: lock cancelled at device");
      } else {
        SEND_LITERAL ("FAILUnlock cancelled at device");
        FastbootMenuSetStatus (L"Fastboot: unlock cancelled at device");
      }

      return;
    }
  }

  Status = WantLocked ? FastbootLockBootloader () : FastbootUnlockBootloader ();
  switch (Status) {
    case EFI_SUCCESS:
      SEND_LITERAL ("OKAY");
      return;
    case EFI_ACCESS_DENIED:
      SEND_LITERAL ("FAILUnlock is not allowed. Enable OEM unlock first");
      return;
    default:
      DEBUG ((DEBUG_ERROR, "%a: %a failed: %r\n", __FUNCTION__, Command, Status));
      if (WantLocked) {
        SEND_LITERAL ("FAILLock device failed");
      } else {
        SEND_LITERAL ("FAILUnlock device failed");
      }

      return;
  }
}

STATIC
VOID
HandleOemCommand (
  IN CHAR8  *Command
  )
{
  EFI_STATUS  Status;

  while (*Command == ' ') {
    Command++;
  }

  if (MATCH_CMD_LITERAL ("bootloader_update", Command)) {
    HandleOemBootloaderUpdate ();
    return;
  }

  FastbootMenuSetStatus (L"Fastboot: OEM command");
  Status = mPlatform->DoOemCommand (Command);
  if (Status == EFI_NOT_FOUND) {
    SEND_LITERAL ("FAILOEM Command not recognised.");
  } else if (Status == EFI_DEVICE_ERROR) {
    SEND_LITERAL ("FAILError while executing command");
  } else if (EFI_ERROR (Status)) {
    SEND_LITERAL ("FAIL");
  } else {
    SEND_LITERAL ("OKAY");
  }
}

STATIC
VOID
AcceptCmd (
  IN        UINTN  Size,
  IN  CONST CHAR8  *Data
  )
{
  CHAR8  Command[FASTBOOT_COMMAND_MAX_LENGTH + 1];

  // Max command size is 64 bytes
  if (Size > FASTBOOT_COMMAND_MAX_LENGTH) {
    SEND_LITERAL ("FAILCommand too large");
    return;
  }

  // Commands aren't null-terminated. Let's get a null-terminated version.
  AsciiStrnCpyS (Command, sizeof Command, Data, Size);

  // Parse command
  if (MATCH_CMD_LITERAL ("getvar", Command)) {
    HandleGetVar (Command + sizeof ("getvar"));
  } else if (MATCH_CMD_LITERAL ("download", Command)) {
    HandleDownload (Command + sizeof ("download"));
  } else if (MATCH_CMD_LITERAL ("verify", Command)) {
    SEND_LITERAL ("FAILNot supported");
  } else if (MATCH_CMD_LITERAL ("flashing", Command)) {
    HandleFlashingCommand (Command + sizeof ("flashing"));
  } else if (MATCH_CMD_LITERAL ("flash", Command)) {
    HandleFlash (Command + sizeof ("flash"));
  } else if (MATCH_CMD_LITERAL ("erase", Command)) {
    HandleErase (Command + sizeof ("erase"));
  } else if (MATCH_CMD_LITERAL ("boot", Command)) {
    HandleBoot ();
  } else if (MATCH_CMD_LITERAL ("continue", Command)) {
    SEND_LITERAL ("OKAY");
    mTextOut->OutputString (mTextOut, L"Received 'continue' command. Exiting Fastboot mode\r\n");
    FastbootMenuSetStatus (L"Fastboot: continue (menu still active)");
  } else if (MATCH_CMD_LITERAL ("reboot", Command)) {
    if (MATCH_CMD_LITERAL ("reboot-booloader", Command)) {
      // fastboot_protocol.txt:
      //    "reboot-bootloader    Reboot back into the bootloader."
      // I guess this means reboot back into fastboot mode to save the user
      // having to do whatever they did to get here again.
      // Here we just reboot normally.
      SEND_LITERAL ("INFOreboot-bootloader not supported, rebooting normally.");
    } else if (MATCH_CMD_LITERAL ("reboot-recovery", Command)) {
      DEBUG ((DEBUG_ERROR, "Fastboot: setting boot-recovery to BCB\n"));
      SetCmdToMiscPartition (NULL, MISC_CMD_TYPE_RECOVERY);
    } else if (MATCH_CMD_LITERAL ("reboot-fastboot", Command)) {
      DEBUG ((DEBUG_ERROR, "Fastboot: setting boot-fastboot to BCB\n"));
      SetCmdToMiscPartition (NULL, MISC_CMD_TYPE_FASTBOOT_USERSPACE);
    }

    SEND_LITERAL ("OKAY");
    gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);

    // Shouldn't get here
    DEBUG ((DEBUG_ERROR, "Fastboot: gRT->ResetSystem didn't work\n"));
  } else if (MATCH_CMD_LITERAL ("powerdown", Command)) {
    SEND_LITERAL ("OKAY");
    gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);

    // Shouldn't get here
    DEBUG ((DEBUG_ERROR, "Fastboot: gRT->ResetSystem didn't work\n"));
  } else if (MATCH_CMD_LITERAL ("oem", Command)) {
    // The "oem" command isn't in the specification, but it was observed in the
    // wild, followed by a space, followed by the actual command.
    HandleOemCommand (Command + sizeof ("oem"));
  } else if (IS_LOWERCASE_ASCII (Command[0])) {
    // Commands starting with lowercase ASCII characters are reserved for the
    // Fastboot protocol. If we don't recognise it, it's probably the future
    // and there are new commands in the protocol.
    // (By the way, the "oem" command mentioned above makes this reservation
    //  redundant, but we handle it here to be spec-compliant)
    SEND_LITERAL ("FAILCommand not recognised. Check Fastboot version.");
  } else {
    HandleOemCommand (Command);
  }
}

STATIC
VOID
AcceptData (
  IN  UINTN  Size,
  IN  VOID   *Data
  )
{
  UINT32        RemainingBytes = mNumDataBytes - mBytesReceivedSoFar;
  CHAR16        OutputString[FASTBOOT_STRING_MAX_LENGTH];
  STATIC UINTN  Count = 0;

  // Protocol doesn't say anything about sending extra data so just ignore it.
  if (Size > RemainingBytes) {
    Size = RemainingBytes;
  }

  CopyMem (&mDataBuffer[mBytesReceivedSoFar], Data, Size);

  mBytesReceivedSoFar += Size;

  // Show download progress. Don't do it for every packet  as outputting text
  // might be time consuming - do it on the last packet and on every 32nd packet
  if (((Count++ % 32) == 0) || (Size == RemainingBytes)) {
    // (Note no newline in format string - it will overwrite the line each time)
    UnicodeSPrint (
      OutputString,
      sizeof (OutputString),
      L"\r%8d / %8d bytes downloaded (%d%%)",
      mBytesReceivedSoFar,
      mNumDataBytes,
      (mBytesReceivedSoFar * 100) / mNumDataBytes // percentage
      );
    mTextOut->OutputString (mTextOut, OutputString);
    FastbootMenuSetStatus (
      L"Fastboot: download %Lu / %Lu (%Lu%%)",
      mBytesReceivedSoFar,
      mNumDataBytes,
      (mNumDataBytes == 0) ? 0 : (mBytesReceivedSoFar * 100) / mNumDataBytes
      );
  }

  if (mBytesReceivedSoFar == mNumDataBytes) {
    // Download finished.

    mTextOut->OutputString (mTextOut, L"\r\n");
    SEND_LITERAL ("OKAY");
    mState = ExpectCmdState;
    FastbootMenuSetStatus (L"Fastboot: download complete");
  }
}

STATIC
BOOLEAN
DrainFastbootPackets (
  VOID
  )
{
  UINTN       Size;
  VOID        *Data;
  EFI_STATUS  Status;
  BOOLEAN     Any;

  Any = FALSE;
  do {
    Status = mTransport->Receive (&Size, &Data);
    if (!EFI_ERROR (Status)) {
      Any = TRUE;
      if (mState == ExpectCmdState) {
        AcceptCmd (Size, (CHAR8 *)Data);
      } else if (mState == ExpectDataState) {
        AcceptData (Size, Data);
      } else {
        ASSERT (FALSE);
      }

      FreePool (Data);
    }
  } while (!EFI_ERROR (Status));

  // Quit if there was a fatal error
  if (Status != EFI_NOT_READY) {
    ASSERT (Status == EFI_DEVICE_ERROR);
    // (Put a newline at the beginning as we are probably in the data phase,
    //  so the download progress line, with no '\n' is probably on the console)
    mTextOut->OutputString (mTextOut, L"\r\nFatal error receiving data. Exiting.\r\n");
    FastbootMenuSetStatus (L"Fastboot: fatal receive error");
    gBS->SignalEvent (mFinishedEvent);
  }

  return Any;
}

/*
  Event notify for a fatal error in transmission.
*/
STATIC
VOID
FatalErrorNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  mTextOut->OutputString (mTextOut, L"Fatal error sending command response. Exiting.\r\n");
  gBS->SignalEvent (mFinishedEvent);
}

/**
  Try every non-Fastboot, non-app, non-hidden, non-shell, active
  boot option in BootOrder until one of them transfers control
  to the OS (which never returns) or the candidate list is
  exhausted.

  Why we do this from inside the Fastboot app instead of just
  returning to BDS:

  Standard EDK2 BDS (MdeModulePkg/Universal/BdsDxe/BdsEntry.c
  BootBootOptions()) already filters CATEGORY_APP and inactive
  entries, so in principle it would naturally land on the kernel
  boot option after our Fastboot returned — except that the
  spec-mandated post-boot logic

      EfiBootManagerBoot (&BootOptions[Index]);
      if ((BootManagerMenu != NULL) &&
          (BootOptions[Index].Status == EFI_SUCCESS)) {
        EfiBootManagerBoot (BootManagerMenu);
        break;
      }

  treats our EFI_SUCCESS return as "the OS booted and exited
  cleanly", and immediately invokes BootManagerMenu. On this
  platform BootManagerMenu is "Enter Setup", so the kernel
  boot option in BootOrder[N] is never reached. By driving the
  boot loop here we keep control inside the app until something
  successfully boots.

  Filtering rules:

    - skip LOAD_OPTION_HIDDEN entries (platform internals like
      "Enter Setup" / "BootManagerMenuApp");
    - skip the Fastboot boot option itself (description
      "Android Fastboot") so we don't recursively re-enter
      ourselves;
    - skip the "UEFI Shell" entry: a clean shell `exit` returns
      EFI_SUCCESS, which (per the BDS rationale above) would
      strand us at the BootManagerMenu instead of the kernel.

  Behaviour on each candidate:

    - if EfiBootManagerBoot does not return, the OS is running.
      Done.
    - if it returns with Status==EFI_NOT_FOUND/etc. (load
      failure — typical of auto-generated eMMC user-data
      entries that have no /EFI/BOOT/BOOTAA64.EFI), we move
      on to the next candidate.
    - if it returns with Status==EFI_SUCCESS (a synchronous
      OS-side exit that we did not expect), we still move on
      rather than dropping back to Setup.

  When no candidate succeeds, we just return; the caller then
  falls back to BDS's normal "return to BDS" path (which on
  this platform terminates at "Enter Setup").
**/
STATIC
VOID
BootNextAndroidKernelOption (
  VOID
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  UINTN                         Attempts;

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
  if (BootOptions == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no boot options found\n", __FUNCTION__));
    return;
  }

  //
  // Iterate every candidate in BootOrder, *not* just the first one. On this
  // platform BootOrder typically looks like:
  //
  //   Boot0005 = Android Fastboot              (us — skip to avoid recursion)
  //   Boot0001 = UEFI Micron ... eMMC User Data (auto-generated; ext4 mount
  //                                             fails when no EFI loader is
  //                                             on the data partition, so
  //                                             EfiBootManagerBoot returns
  //                                             EFI_NOT_FOUND)
  //   Boot0000 = Enter Setup                   (HIDDEN — skip)
  //   Boot0003 = BootManagerMenuApp            (HIDDEN — skip)
  //   Boot0004 = UEFI Shell                    (skip: a clean shell `exit`
  //                                             returns EFI_SUCCESS which
  //                                             would terminate our chain
  //                                             before reaching the kernel)
  //   Boot0002 = UEFI NVIDIA eMMC Kernel Boot  (the actual Android kernel
  //                                             boot via AndroidBootDxe —
  //                                             this is what we want)
  //
  // Stopping at the first candidate (previous behavior) lands us on Boot0001
  // which always fails on Android partitioning. Keep trying subsequent
  // entries until one of them transfers control to the OS (in which case
  // EfiBootManagerBoot never returns) or we exhaust the list.
  //
  Attempts = 0;
  for (Index = 0; Index < BootOptionCount; Index++) {
    if ((BootOptions[Index].Attributes & LOAD_OPTION_ACTIVE) == 0) {
      continue;
    }

    if ((BootOptions[Index].Attributes & LOAD_OPTION_CATEGORY) != LOAD_OPTION_CATEGORY_BOOT) {
      continue;
    }

    if ((BootOptions[Index].Attributes & LOAD_OPTION_HIDDEN) != 0) {
      continue;
    }

    if (BootOptions[Index].Description != NULL) {
      if (StrCmp (BootOptions[Index].Description, L"Android Fastboot") == 0) {
        continue;
      }

      //
      // Skip the UEFI Shell entry: a normal shell session exits with
      // EFI_SUCCESS which BDS interprets as "OS exited cleanly" and which
      // would also abort the rest of our fall-through chain. The Android
      // kernel boot option is what we actually want for both "Continue"
      // and "Boot safe mode".
      //
      if (StrCmp (BootOptions[Index].Description, L"UEFI Shell") == 0) {
        continue;
      }
    }

    Attempts++;
    DEBUG ((
      DEBUG_INFO,
      "%a: trying Boot%04x = %s (attempt %u)\n",
      __FUNCTION__,
      BootOptions[Index].OptionNumber,
      (BootOptions[Index].Description != NULL) ? BootOptions[Index].Description : L"<no name>",
      (UINT32)Attempts
      ));

    //
    // On success EfiBootManagerBoot never returns: the OS takes over. On
    // load/launch failure it returns with BootOptions[Index].Status set
    // (commonly EFI_NOT_FOUND); in that case we simply move on to the
    // next candidate. We do NOT break on a Status==EFI_SUCCESS return
    // either, because the only way to reach this point with SUCCESS is
    // a synchronous OS-side `exit` — for which the most useful behavior
    // is still to keep trying the remaining kernel boot entries instead
    // of dropping straight back to the BDS Setup menu.
    //
    EfiBootManagerBoot (&BootOptions[Index]);

    DEBUG ((
      DEBUG_ERROR,
      "%a: Boot%04x returned, Status=%r — trying next BootOrder entry\n",
      __FUNCTION__,
      BootOptions[Index].OptionNumber,
      BootOptions[Index].Status
      ));
  }

  if (Attempts == 0) {
    DEBUG ((DEBUG_ERROR, "%a: no suitable kernel boot option in BootOrder\n", __FUNCTION__));
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: exhausted %u BootOrder candidate(s) without a successful OS hand-off — falling back to BDS\n",
      __FUNCTION__,
      (UINT32)Attempts
      ));
  }

  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
}

EFI_STATUS
EFIAPI
FastbootAppEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS     Status;
  EFI_EVENT      ReceiveEvent;
  EFI_EVENT      MenuTimerEvent;
  EFI_INPUT_KEY  Key;
  BOOLEAN        Done;
  BOOLEAN        ServicedFastboot;

  //
  // Initialize all event handles to NULL up-front. The cleanup paths below
  // (both error-return and the final teardown) call gBS->CloseEvent on each
  // handle unconditionally; CoreCloseEvent treats NULL as EFI_INVALID_PARAMETER
  // and returns harmlessly, but a stack-resident EFI_EVENT left uninitialised
  // (or with garbage from a CreateEvent that bailed out without writing
  // *Event — see the Type==0 rationale at the CreateEvent calls below) will
  // make CoreCloseEvent dereference a bogus pointer and translation-fault.
  //
  ReceiveEvent         = NULL;
  MenuTimerEvent       = NULL;
  mFinishedEvent       = NULL;
  mFatalSendErrorEvent = NULL;

  mDataBuffer = NULL;
  DEBUG ((DEBUG_ERROR, "Fastboot: Entry\n"));

  Status = gBS->LocateProtocol (
                  &gAndroidFastbootTransportProtocolGuid,
                  NULL,
                  (VOID **)&mTransport
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fastboot: Couldn't open Fastboot Transport Protocol: %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gAndroidFastbootPlatformProtocolGuid, NULL, (VOID **)&mPlatform);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fastboot: Couldn't open Fastboot Platform Protocol: %r\n", Status));
    return Status;
  }

  Status = mPlatform->Init ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fastboot: Couldn't initialise Fastboot Platform Protocol: %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gEfiSimpleTextOutProtocolGuid, NULL, (VOID **)&mTextOut);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Fastboot: Couldn't open Text Output Protocol: %r\n",
      Status
      ));
    return Status;
  }

  Status = gBS->LocateProtocol (&gEfiSimpleTextInProtocolGuid, NULL, (VOID **)&mTextIn);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fastboot: Couldn't open Text Input Protocol: %r\n", Status));
    return Status;
  }

  Status = AvbOpteeInterfaceInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fastboot: Avb OP-TEE initialization failed: %r\n", Status));
    return Status;
  }

  // Disable watchdog
  Status = gBS->SetWatchdogTimer (0, 0x10000, 0, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fastboot: Couldn't disable watchdog timer: %r\n", Status));
  }

  //
  // ReceiveEvent is signalled by the transport when packets arrive; the main
  // loop drains the queued protocol using non-blocking Receive() (EFI_NOT_READY
  // when idle). No notify callback — cooperative polling only.
  //
  // Type MUST be 0 here, NOT EVT_NOTIFY_WAIT/EVT_NOTIFY_SIGNAL: per UEFI spec
  // (and CoreCreateEventEx in MdeModulePkg/Core/Dxe/Event/Event.c), passing
  // either notify-type bit with NotifyFunction == NULL or NotifyTpl <=
  // TPL_APPLICATION causes CreateEvent to bail out with EFI_INVALID_PARAMETER
  // *without writing *Event*, so the local would silently retain stack
  // garbage. CoreCloseEvent later dereferences Event->Signature and faults
  // on a translation error (FAR=garbage). A Type==0 event has no notify
  // semantics but is still a valid handle for both gBS->SignalEvent (used
  // by the transport via mReceiveEvent in FastbootTransportUsb.c) and
  // gBS->CheckEvent.
  //
  Status = gBS->CreateEvent (0, 0, NULL, NULL, &ReceiveEvent);
  ASSERT_EFI_ERROR (Status);

  // Signalled on fatal transport error
  Status = gBS->CreateEvent (0, 0, NULL, NULL, &mFinishedEvent);
  ASSERT_EFI_ERROR (Status);

  // Create event to pass to FASTBOOT_TRANSPORT_PROTOCOL.Send, signalling a
  // fatal error
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  FatalErrorNotify,
                  NULL,
                  &mFatalSendErrorEvent
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEvent (EVT_TIMER, TPL_APPLICATION, NULL, NULL, &MenuTimerEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fastboot: CreateEvent MenuTimer failed: %r\n", Status));
    gBS->CloseEvent (ReceiveEvent);
    gBS->CloseEvent (mFinishedEvent);
    gBS->CloseEvent (mFatalSendErrorEvent);
    mPlatform->UnInit ();
    return Status;
  }

  // Start listening for data (USB gadget in fastboot mode)
  Status = mTransport->Start (
                         ReceiveEvent
                         );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fastboot: Couldn't start transport: %r\n", Status));
    gBS->CloseEvent (MenuTimerEvent);
    gBS->CloseEvent (ReceiveEvent);
    gBS->CloseEvent (mFinishedEvent);
    gBS->CloseEvent (mFatalSendErrorEvent);
    mPlatform->UnInit ();
    return Status;
  }

  FastbootMenuInit (mTextOut, MenuTimerEvent);
  FastbootMenuSetStatus (L"Fastboot: USB ready — use menu or host fastboot");

  Done = FALSE;

  while (!Done) {
    FastbootMenuRedrawIfDirty ();

    //
    // 1) USB fastboot: dequeue all pending packets (Receive is non-blocking;
    //    EFI_NOT_READY when the transport queue is empty).
    //
    ServicedFastboot = DrainFastbootPackets ();

    FastbootMenuRedrawIfDirty ();

    //
    // 2) Fatal receive/send error.
    //
    Status = gBS->CheckEvent (mFinishedEvent);
    if (!EFI_ERROR (Status)) {
      Done = TRUE;
      break;
    }

    //
    // 3) Menu idle timeout: delegated to the menu module.
    //
    Status = gBS->CheckEvent (MenuTimerEvent);
    if (!EFI_ERROR (Status)) {
      Done = FastbootMenuOnIdleTimeout ();
      if (Done) {
        break;
      }
    }

    //
    // 4) Keyboard / menu navigation (non-blocking ReadKeyStroke).
    //    Menu navigation/actions are handled by the menu module; SPACE
    //    remains a non-menu quit path.
    //
    Key.ScanCode    = SCAN_NULL;
    Key.UnicodeChar = CHAR_NULL;
    Status          = mTextIn->ReadKeyStroke (mTextIn, &Key);
    if (!EFI_ERROR (Status)) {
      FASTBOOT_MENU_KEY_RESULT  KeyResult;

      KeyResult = FastbootMenuHandleKey (&Key);
      if (KeyResult == FastbootMenuKeyResultExitApp) {
        Done = TRUE;
        break;
      }

      if (KeyResult == FastbootMenuKeyResultIgnored) {
        if ((Key.ScanCode == SCAN_NULL) && (Key.UnicodeChar == L' ')) {
          //
          // Former standalone fastboot used RETURN or SPACE to exit; ENTER
          // is now the menu activator, so only SPACE retains a non-menu
          // quit path.
          //
          Done = TRUE;
          break;
        }
      }
    }

    if (ServicedFastboot) {
      FastbootMenuArmIdleTimer ();
    }

    //
    // Brief cooperative yield (does not wait on USB or keys).
    //
    gBS->Stall (500);
  }

  mTransport->Stop ();
  gBS->SetTimer (MenuTimerEvent, TimerCancel, 0);
  gBS->CloseEvent (MenuTimerEvent);
  gBS->CloseEvent (ReceiveEvent);
  gBS->CloseEvent (mFinishedEvent);
  gBS->CloseEvent (mFatalSendErrorEvent);

  mPlatform->UnInit ();

  //
  // If the user picked "Continue" or "Boot safe mode" from the
  // bootloader menu, hand control directly to the next bootable
  // BootOrder entry (the Android kernel via AndroidBootDxe). See
  // BootNextAndroidKernelOption() for why we cannot let BDS do
  // this on its own.
  //
  if (FastbootMenuShouldContinueAndroidBoot ()) {
    BootNextAndroidKernelOption ();
  }

  return EFI_SUCCESS;
}
