/** @file
  Api's to communicate with RPMB partition on the eMMC device via RPC calls
  from OP-TEE.

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <IndustryStandard/ArmStdSmc.h>

#include <Protocol/SdMmcPassThru.h>
#include <Protocol/DevicePath.h>

#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <IndustryStandard/Emmc.h>
#include <OpteeSmc.h>

STATIC EFI_SD_MMC_PASS_THRU_PROTOCOL  *EmmcPassThru = NULL;
STATIC UINT8                          EmmcSlot;
STATIC EMMC_EXT_CSD                   ExtCsd;

STATIC
VOID
BytesToUint16 (
  UINT8   *ByteArr,
  UINT16  *Res
  )
{
  *Res = ((ByteArr[0] << 8) + ByteArr[1]);
}

STATIC
VOID
U16ToBytes (
  UINT16  U16,
  UINT8   *ByteArr
  )
{
  *ByteArr       = (UINT8)(U16 >> 8);
  *(ByteArr + 1) = (UINT8)U16;
}

STATIC
EFI_STATUS
GetEmmcDevice (
  )
{
  UINTN                          PassThruNumHandles;
  EFI_HANDLE                     *PassThruHandleBuffer;
  EFI_STATUS                     Status;
  UINTN                          Index;
  BOOLEAN                        EmmcFound;
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru;
  EFI_DEVICE_PATH_PROTOCOL       *DevicePath;
  UINT8                          Slot;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSdMmcPassThruProtocolGuid,
                  NULL,
                  &PassThruNumHandles,
                  &PassThruHandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Error locating PassThru handles: %r\n", Status));
    return EFI_NOT_FOUND;
  }

  EmmcFound = FALSE;
  PassThru  = NULL;
  Slot      = 0xFF;
  for (Index = 0; (Index < PassThruNumHandles) && (EmmcFound == FALSE); Index++) {
    Status = gBS->HandleProtocol (
                    PassThruHandleBuffer[Index],
                    &gEfiSdMmcPassThruProtocolGuid,
                    (VOID **)&PassThru
                    );
    if (EFI_ERROR (Status) || (PassThru == NULL)) {
      DEBUG ((
        DEBUG_INFO,
        "Failed to get PassThru for handle index %u: %r\n",
        Index,
        Status
        ));
      continue;
    }

    while (TRUE) {
      Status = PassThru->GetNextSlot (PassThru, &Slot);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "No more legal slots %r", Status));
        break;
      }

      Status = PassThru->BuildDevicePath (
                           PassThru,
                           Slot,
                           &DevicePath
                           );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "Failed in Build DevicePath %r", Status));
        continue;
      }

      if (DevicePath->SubType == MSG_EMMC_DP) {
        EmmcPassThru = PassThru;
        EmmcSlot     = Slot;
        EmmcFound    = TRUE;
        break;
      }
    }
  }

  return Status;
}

STATIC
VOID
PrintCid (
  EMMC_CID  *Cid
  )
{
  DEBUG ((DEBUG_INFO, "==Dump Emmc Cid Register==\n"));
  DEBUG ((
    DEBUG_INFO,
    "Manufac:0x%x ProductName:0x%x:0x%x:0x%x:0x%x:0x%x \n",
    Cid->ManufacturerId,
    Cid->ProductName[0],
    Cid->ProductName[1],
    Cid->ProductName[2],
    Cid->ProductName[3],
    Cid->ProductName[4],
    Cid->ProductName[5]
    ));
  DEBUG ((DEBUG_INFO, "DeviceHead %x OemId %x \n", Cid->DeviceType, Cid->OemId));
}

STATIC
VOID
PrintExtCsd (
  EMMC_EXT_CSD  *ExtCsd
  )
{
  DEBUG ((DEBUG_INFO, "==Dump Emmc ExtCsd Register==\n"));
  DEBUG ((DEBUG_INFO, "  Supported Command Sets                 0x%x\n", ExtCsd->CmdSet));
  DEBUG ((DEBUG_INFO, "  HPI features                           0x%x\n", ExtCsd->HpiFeatures));
  DEBUG ((DEBUG_INFO, "  Background operations support          0x%x\n", ExtCsd->BkOpsSupport));
  DEBUG ((DEBUG_INFO, "  Background operations status           0x%x\n", ExtCsd->BkopsStatus));
  DEBUG ((DEBUG_INFO, "  Number of correctly programmed sectors 0x%x\n", *((UINT32 *)&ExtCsd->CorrectlyPrgSectorsNum[0])));
  DEBUG ((DEBUG_INFO, "  Initialization time after partitioning 0x%x\n", ExtCsd->IniTimeoutAp));
  DEBUG ((DEBUG_INFO, "  TRIM Multiplier                        0x%x\n", ExtCsd->TrimMult));
  DEBUG ((DEBUG_INFO, "  Secure Feature support                 0x%x\n", ExtCsd->SecFeatureSupport));
  DEBUG ((DEBUG_INFO, "  Secure Erase Multiplier                0x%x\n", ExtCsd->SecEraseMult));
  DEBUG ((DEBUG_INFO, "  Secure TRIM Multiplier                 0x%x\n", ExtCsd->SecTrimMult));
  DEBUG ((DEBUG_INFO, "  Boot information                       0x%x\n", ExtCsd->BootInfo));
  DEBUG ((DEBUG_INFO, "  Boot partition size                    0x%x\n", ExtCsd->BootSizeMult));
  DEBUG ((DEBUG_INFO, "  Access size                            0x%x\n", ExtCsd->AccSize));
  DEBUG ((DEBUG_INFO, "  High-capacity erase unit size          0x%x\n", ExtCsd->HcEraseGrpSize));
  DEBUG ((DEBUG_INFO, "  High-capacity erase timeout            0x%x\n", ExtCsd->EraseTimeoutMult));
  DEBUG ((DEBUG_INFO, "  Reliable write sector count            0x%x\n", ExtCsd->RelWrSecC));
  DEBUG ((DEBUG_INFO, "  High-capacity write protect group size 0x%x\n", ExtCsd->HcWpGrpSize));
  DEBUG ((DEBUG_INFO, "  Sleep/awake timeout                    0x%x\n", ExtCsd->SATimeout));
  DEBUG ((DEBUG_INFO, "  Sector Count                           0x%x\n", *((UINT32 *)&ExtCsd->SecCount[0])));
  DEBUG ((DEBUG_INFO, "  Partition switching timing             0x%x\n", ExtCsd->PartitionSwitchTime));
  DEBUG ((DEBUG_INFO, "  Out-of-interrupt busy timing           0x%x\n", ExtCsd->OutOfInterruptTime));
  DEBUG ((DEBUG_INFO, "  I/O Driver Strength                    0x%x\n", ExtCsd->DriverStrength));
  DEBUG ((DEBUG_INFO, "  Device type                            0x%x\n", ExtCsd->DeviceType));
  DEBUG ((DEBUG_INFO, "  CSD STRUCTURE                          0x%x\n", ExtCsd->CsdStructure));
  DEBUG ((DEBUG_INFO, "  Extended CSD revision                  0x%x\n", ExtCsd->ExtCsdRev));
  DEBUG ((DEBUG_INFO, "  Command set                            0x%x\n", ExtCsd->CmdSet));
  DEBUG ((DEBUG_INFO, "  Command set revision                   0x%x\n", ExtCsd->CmdSetRev));
  DEBUG ((DEBUG_INFO, "  Power class                            0x%x\n", ExtCsd->PowerClass));
  DEBUG ((DEBUG_INFO, "  High-speed interface timing            0x%x\n", ExtCsd->HsTiming));
  DEBUG ((DEBUG_INFO, "  Bus width mode                         0x%x\n", ExtCsd->BusWidth));
  DEBUG ((DEBUG_INFO, "  Erased memory content                  0x%x\n", ExtCsd->ErasedMemCont));
  DEBUG ((DEBUG_INFO, "  Partition configuration                0x%x\n", ExtCsd->PartitionConfig));
  DEBUG ((DEBUG_INFO, "  Boot config protection                 0x%x\n", ExtCsd->BootConfigProt));
  DEBUG ((DEBUG_INFO, "  Boot bus Conditions                    0x%x\n", ExtCsd->BootBusConditions));
  DEBUG ((DEBUG_INFO, "  High-density erase group definition    0x%x\n", ExtCsd->EraseGroupDef));
  DEBUG ((DEBUG_INFO, "  Boot write protection status register  0x%x\n", ExtCsd->BootWpStatus));
  DEBUG ((DEBUG_INFO, "  Boot area write protection register    0x%x\n", ExtCsd->BootWp));
  DEBUG ((DEBUG_INFO, "  User area write protection register    0x%x\n", ExtCsd->UserWp));
  DEBUG ((DEBUG_INFO, "  FW configuration                       0x%x\n", ExtCsd->FwConfig));
  DEBUG ((DEBUG_INFO, "  RPMB Size                              0x%x\n", ExtCsd->RpmbSizeMult));
  DEBUG ((DEBUG_INFO, "  H/W reset function                     0x%x\n", ExtCsd->RstFunction));
  DEBUG ((DEBUG_INFO, "  Partitioning Support                   0x%x\n", ExtCsd->PartitioningSupport));
  DEBUG ((
    DEBUG_INFO,
    "  Max Enhanced Area Size                 0x%02x%02x%02x\n", \
    ExtCsd->MaxEnhSizeMult[2],
    ExtCsd->MaxEnhSizeMult[1],
    ExtCsd->MaxEnhSizeMult[0]
    ));
  DEBUG ((DEBUG_INFO, "  Partitions attribute                   0x%x\n", ExtCsd->PartitionsAttribute));
  DEBUG ((DEBUG_INFO, "  Partitioning Setting                   0x%x\n", ExtCsd->PartitionSettingCompleted));
  DEBUG ((
    DEBUG_INFO,
    "  General Purpose Partition 1 Size       0x%02x%02x%02x\n", \
    ExtCsd->GpSizeMult[2],
    ExtCsd->GpSizeMult[1],
    ExtCsd->GpSizeMult[0]
    ));
  DEBUG ((
    DEBUG_INFO,
    "  General Purpose Partition 2 Size       0x%02x%02x%02x\n", \
    ExtCsd->GpSizeMult[5],
    ExtCsd->GpSizeMult[4],
    ExtCsd->GpSizeMult[3]
    ));
  DEBUG ((
    DEBUG_INFO,
    "  General Purpose Partition 3 Size       0x%02x%02x%02x\n", \
    ExtCsd->GpSizeMult[8],
    ExtCsd->GpSizeMult[7],
    ExtCsd->GpSizeMult[6]
    ));
  DEBUG ((
    DEBUG_INFO,
    "  General Purpose Partition 4 Size       0x%02x%02x%02x\n", \
    ExtCsd->GpSizeMult[11],
    ExtCsd->GpSizeMult[10],
    ExtCsd->GpSizeMult[9]
    ));
}

STATIC
EFI_STATUS
RpmbEmmcSelect (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  UINT16                         Rca
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex    = EMMC_SELECT_DESELECT_CARD;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);

  return Status;
}

STATIC
EFI_STATUS
RpmbEmmcGetExtCsd (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  EMMC_EXT_CSD                   *ExtCsd
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  ZeroMem (ExtCsd, sizeof (EMMC_EXT_CSD));
  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex    = EMMC_SEND_EXT_CSD;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAdtc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = 0x00000000;
  Packet.InDataBuffer         = ExtCsd;
  Packet.InTransferLength     = sizeof (EMMC_EXT_CSD);

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a PassThru transaction failed %r\n", __FUNCTION__, Status));
  }

  return Status;
}

STATIC
EFI_STATUS
RpmbEmmcGetCid (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  UINT8 (*Cid)[16]
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;

  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  ZeroMem (Cid, sizeof (EMMC_CID));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex    = EMMC_SEND_CID;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR2;
  SdMmcCmdBlk.CommandArgument = (UINT32)(Slot + 1) << 16;

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Failed CID packet %r\n", Status));
  }

  CopyMem (((UINT8 *)Cid) + 1, &SdMmcStatusBlk.Resp0, sizeof (EMMC_CID) - 1);

  return Status;
}

STATIC
EFI_STATUS
RpmbEmmcGetCsd (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  EMMC_CSD                       *Csd
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  UINT16                               Rca;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  ZeroMem (Csd, sizeof (EMMC_CSD));
  Rca = Slot + 1;

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex    = EMMC_SEND_CSD;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR2;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    //
    // For details, refer to SD Host Controller Simplified Spec 3.0 Table 2-12.
    //
    CopyMem (((UINT8 *)Csd) + 1, &SdMmcStatusBlk.Resp0, sizeof (EMMC_CSD) - 1);
  }

  DEBUG ((DEBUG_INFO, "== Dump Emmc Csd Register==\n"));
  DEBUG ((DEBUG_INFO, "  CSD structure                    0x%x\n", Csd->CsdStructure));
  DEBUG ((DEBUG_INFO, "  System specification version     0x%x\n", Csd->SpecVers));
  DEBUG ((DEBUG_INFO, "  Data read access-time 1          0x%x\n", Csd->Taac));
  DEBUG ((DEBUG_INFO, "  Data read access-time 2          0x%x\n", Csd->Nsac));
  DEBUG ((DEBUG_INFO, "  Max. bus clock frequency         0x%x\n", Csd->TranSpeed));
  DEBUG ((DEBUG_INFO, "  Device command classes           0x%x\n", Csd->Ccc));
  DEBUG ((DEBUG_INFO, "  Max. read data block length      0x%x\n", Csd->ReadBlLen));
  DEBUG ((DEBUG_INFO, "  Partial blocks for read allowed  0x%x\n", Csd->ReadBlPartial));
  DEBUG ((DEBUG_INFO, "  Write block misalignment         0x%x\n", Csd->WriteBlkMisalign));
  DEBUG ((DEBUG_INFO, "  Read block misalignment          0x%x\n", Csd->ReadBlkMisalign));
  DEBUG ((DEBUG_INFO, "  DSR implemented                  0x%x\n", Csd->DsrImp));
  DEBUG ((DEBUG_INFO, "  Device size                      0x%x\n", Csd->CSizeLow | (Csd->CSizeHigh << 2)));
  DEBUG ((DEBUG_INFO, "  Max. read current @ VDD min      0x%x\n", Csd->VddRCurrMin));
  DEBUG ((DEBUG_INFO, "  Max. read current @ VDD max      0x%x\n", Csd->VddRCurrMax));
  DEBUG ((DEBUG_INFO, "  Max. write current @ VDD min     0x%x\n", Csd->VddWCurrMin));
  DEBUG ((DEBUG_INFO, "  Max. write current @ VDD max     0x%x\n", Csd->VddWCurrMax));
  DEBUG ((DEBUG_INFO, "  Device size multiplier           0x%x\n", Csd->CSizeMult));
  DEBUG ((DEBUG_INFO, "  Erase group size                 0x%x\n", Csd->EraseGrpSize));
  DEBUG ((DEBUG_INFO, "  Erase group size multiplier      0x%x\n", Csd->EraseGrpMult));
  DEBUG ((DEBUG_INFO, "  Write protect group size         0x%x\n", Csd->WpGrpSize));
  DEBUG ((DEBUG_INFO, "  Write protect group enable       0x%x\n", Csd->WpGrpEnable));
  DEBUG ((DEBUG_INFO, "  Manufacturer default ECC         0x%x\n", Csd->DefaultEcc));
  DEBUG ((DEBUG_INFO, "  Write speed factor               0x%x\n", Csd->R2WFactor));
  DEBUG ((DEBUG_INFO, "  Max. write data block length     0x%x\n", Csd->WriteBlLen));
  DEBUG ((DEBUG_INFO, "  Partial blocks for write allowed 0x%x\n", Csd->WriteBlPartial));
  DEBUG ((DEBUG_INFO, "  Content protection application   0x%x\n", Csd->ContentProtApp));
  DEBUG ((DEBUG_INFO, "  File format group                0x%x\n", Csd->FileFormatGrp));
  DEBUG ((DEBUG_INFO, "  Copy flag (OTP)                  0x%x\n", Csd->Copy));
  DEBUG ((DEBUG_INFO, "  Permanent write protection       0x%x\n", Csd->PermWriteProtect));
  DEBUG ((DEBUG_INFO, "  Temporary write protection       0x%x\n", Csd->TmpWriteProtect));
  DEBUG ((DEBUG_INFO, "  File format                      0x%x\n", Csd->FileFormat));
  DEBUG ((DEBUG_INFO, "  ECC code                         0x%x\n", Csd->Ecc));

  return Status;
}

STATIC
EFI_STATUS
RpmbEmmcClearPartition (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT16                         Slot
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  UINT8                                Offset;
  UINT8                                Value;
  UINT32                               DevStatus;

  Value  = ExtCsd.PartitionConfig;
  Offset = OFFSET_OF (EMMC_EXT_CSD, PartitionConfig);
  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex    = EMMC_SWITCH;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1b;
  SdMmcCmdBlk.CommandArgument = (UINT32)(Slot + 1) << 16;
  SdMmcCmdBlk.CommandArgument = (Value << 8) | (Offset << 16);

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    CopyMem (&DevStatus, &SdMmcStatusBlk.Resp0, sizeof (UINT32));
  }

  return Status;
}

STATIC
EFI_STATUS
RpmbEmmcSetPartition (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT16                         Slot
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  UINT8                                Offset;
  UINT8                                Value;
  UINT32                               DevStatus;

  Value  = ExtCsd.PartitionConfig;
  Value &= (UINT8) ~0x7;
  Value |= EmmcPartitionRPMB;

  Offset = OFFSET_OF (EMMC_EXT_CSD, PartitionConfig);
  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex    = EMMC_SWITCH;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1b;
  SdMmcCmdBlk.CommandArgument = (UINT32)(Slot + 1) << 16;
  SdMmcCmdBlk.CommandArgument = (Value << 8) | (Offset << 16) | BIT24 | BIT25;

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    CopyMem (&DevStatus, &SdMmcStatusBlk.Resp0, sizeof (UINT32));
  }

  return Status;
}

STATIC
EFI_STATUS
RpmbEmmcSendStatus (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT16                         Slot,
  OUT UINT32                     *DevStatus
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex    = EMMC_SEND_STATUS;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)(Slot + 1) << 16;

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    CopyMem (DevStatus, &SdMmcStatusBlk.Resp0, sizeof (UINT32));
  }

  return Status;
}

STATIC
EFI_STATUS
GetRpmbDevInfo (
  RPMB_DEV_INFO                  *DevInfo,
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot
  )
{
  EFI_STATUS  Status;
  UINT32      DevStatus;
  EMMC_CSD    Csd;
  EMMC_CID    *Cid;

  Status = RpmbEmmcSendStatus (PassThru, Slot, &DevStatus);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Failed to setnd status 0 %r\n", Status));
  }

  // Deselect the device First.
  Status = RpmbEmmcSelect (PassThru, Slot, 0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Error selectint device %r\n", Status));
  }

  Status = RpmbEmmcSendStatus (PassThru, Slot, &DevStatus);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Failed to setnd status 0 %r\n", Status));
  }

  RpmbEmmcGetCsd (PassThru, Slot, &Csd);
  RpmbEmmcGetCid (PassThru, Slot, &DevInfo->Cid);
  Cid = (EMMC_CID *)&DevInfo->Cid;
  PrintCid (Cid);

  Status = RpmbEmmcSelect (PassThru, Slot, (Slot + 1));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Error selectint device %r\n", Status));
  }

  Status = RpmbEmmcGetExtCsd (PassThru, Slot, &ExtCsd);
  PrintExtCsd (&ExtCsd);

  DevInfo->RpmbSizeMult  = ExtCsd.RpmbSizeMult;
  DevInfo->RelWrSecCount = ExtCsd.RelWrSecC;
  DevInfo->RetCode       = RPMB_CMD_GET_DEV_INFO_RET_OK;
  return Status;
}

STATIC
EFI_STATUS
RpmbReadBlocks (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  UINTN                          BlockCount,
  RPMB_FRAME                     *DataFrame
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex = EMMC_READ_MULTIPLE_BLOCK;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAdtc;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1;
  Packet.InDataBuffer      = (VOID *)DataFrame;
  Packet.InTransferLength  = (BlockCount * RPMB_FRAME_SIZE);

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  return Status;
}

STATIC
EFI_STATUS
RpmbWriteBlocks (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  UINTN                          BlockCount,
  RPMB_FRAME                     *DataFrame
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex = EMMC_WRITE_MULTIPLE_BLOCK;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAdtc;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1;
  Packet.OutDataBuffer     = (VOID *)DataFrame;
  Packet.OutTransferLength = (BlockCount * RPMB_FRAME_SIZE);

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  return Status;
}

STATIC
EFI_STATUS
RpmbSetBlockCount (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  UINTN                          BlockCount,
  BOOLEAN                        IsWrite
  )
{
  EFI_STATUS                           Status;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));
  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;
  Packet.Timeout        = EMMC_TRANS_TIMEOUT;

  SdMmcCmdBlk.CommandIndex    = EMMC_SET_BLOCK_COUNT;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = BlockCount;
  // Set the Reliable Write argument always during writes.
  if (IsWrite) {
    SdMmcCmdBlk.CommandArgument |= (1UL << 31);
  }

  Status = PassThru->PassThru (PassThru, Slot, &Packet, NULL);
  return Status;
}

#if 0
STATIC
VOID
DumpRpmbFrame (
  RPMB_FRAME  *RpmbFrame
  )
{
  UINTN  Index;

  for (Index = 0; Index < 2; Index++) {
    DEBUG ((DEBUG_INFO, "Request[%d] : %u \n", Index, RpmbFrame->Request[Index]));
  }

  for (Index = 0; Index < 2; Index++) {
    DEBUG ((DEBUG_INFO, "Result[%d] : %u \n", Index, RpmbFrame->Result[Index]));
  }

  for (Index = 0; Index < 2; Index++) {
    DEBUG ((DEBUG_INFO, "BlockCount[%d] : %u \n", Index, RpmbFrame->BlockCount[Index]));
  }

  for (Index = 0; Index < 2; Index++) {
    DEBUG ((DEBUG_INFO, "Address[%d] : 0x%x \n", Index, RpmbFrame->Address[Index]));
  }

  for (Index = 0; Index < 4; Index++) {
    DEBUG ((DEBUG_INFO, "WrCounter[%u] : 0x%x \n", Index, RpmbFrame->WrCounter[Index]));
  }

  for (Index = 0; Index < RPMB_NONCE_SIZE; Index++) {
    DEBUG ((DEBUG_INFO, "Nonce[%u] : 0x%x \n", Index, RpmbFrame->Nonce[Index]));
  }

  for (Index = 0; Index < RPMB_MAC_SIZE; Index++) {
    DEBUG ((DEBUG_INFO, "MAC[%u] : 0x%x \n", Index, RpmbFrame->Mac[Index]));
  }
}

#endif

STATIC
EFI_STATUS
HandleRpmbWrite (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  RPMB_FRAME                     *ReqFrame,
  UINTN                          NumReqFrames,
  RPMB_FRAME                     *RespFrame,
  UINTN                          NumRespFrames
  )
{
  EFI_STATUS  Status;

  Status = RpmbEmmcSelect (PassThru, Slot, (Slot + 1));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to select RPMB %r\n", __FUNCTION__, Status));
  }

  Status = RpmbEmmcSetPartition (PassThru, Slot);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to select RPMB Partition %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = RpmbSetBlockCount (PassThru, Slot, NumReqFrames, TRUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a [0]:Failed to set BlockCount %d (%r)\n",
      __FUNCTION__,
      NumReqFrames,
      Status
      ));
    return Status;
  }

  Status = RpmbWriteBlocks (PassThru, Slot, NumReqFrames, ReqFrame);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a [1]:Failed to Send Write Res %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = RpmbSetBlockCount (PassThru, Slot, 1, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a [2]:Failed to set BlockCount 1 %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  ZeroMem (RespFrame, RPMB_FRAME_SIZE);
  U16ToBytes (RPMB_MSG_TYPE_REQ_RESULT_READ, RespFrame->Request);
  Status = RpmbWriteBlocks (PassThru, Slot, 1, RespFrame);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[3]:Failed to Read %d %r\n", NumRespFrames, Status));
    return Status;
  }

  Status = RpmbSetBlockCount (PassThru, Slot, 1, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a [4]:Failed to set BlockCount 1 %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = RpmbReadBlocks (PassThru, Slot, 1, RespFrame);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[3]:Failed to Read %d %r\n", NumRespFrames, Status));
    return Status;
  }

  Status = RpmbEmmcClearPartition (PassThru, Slot);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to select RPMB Partition %r\n", __FUNCTION__, Status));
    return Status;
  }

  return Status;
}

STATIC
EFI_STATUS
HandleRpmbRead (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  RPMB_FRAME                     *ReqFrame,
  UINTN                          NumReqFames,
  RPMB_FRAME                     *RespFrame,
  UINTN                          NumRespFrames
  )
{
  EFI_STATUS  Status;

  Status = RpmbEmmcSelect (PassThru, Slot, (Slot + 1));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Failed to select RPMB %r\n", __FUNCTION__, Status));
  }

  Status = RpmbEmmcSetPartition (PassThru, Slot);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to select RPMB Partition %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = RpmbSetBlockCount (PassThru, Slot, 1, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[0]:Failed to set BlockCount 1 (%r)\n", Status));
    return Status;
  }

  Status = RpmbWriteBlocks (PassThru, Slot, 1, ReqFrame);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[1]:Failed to Send Read Res %r\n", Status));
    return Status;
  }

  Status = RpmbSetBlockCount (PassThru, Slot, NumRespFrames, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[2]:Failed to set BlockCount %d %r\n",
      NumRespFrames,
      Status
      ));
    return Status;
  }

  Status = RpmbReadBlocks (PassThru, Slot, NumRespFrames, RespFrame);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[3]:Failed to Read %d %r\n", NumRespFrames, Status));
    return Status;
  }

  Status = RpmbEmmcClearPartition (PassThru, Slot);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to select RPMB Partition %r\n", __FUNCTION__, Status));
    return Status;
  }

  return Status;
}

STATIC
EFI_STATUS
HandleRpmbDataReq (
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  UINT8                          Slot,
  VOID                           *RpmbReq,
  UINT64                         ReqSize,
  VOID                           *RpmbResp,
  UINT64                         RespSize
  )
{
  RPMB_FRAME  *ReqFrame;
  RPMB_FRAME  *RespFrame;
  UINT16      Request;
  UINTN       NumReqFrames;
  UINTN       NumRespFrames;
  EFI_STATUS  Status;

  ReqFrame  = RpmbReq;
  RespFrame = RpmbResp;

  if ((ReqSize % RPMB_FRAME_SIZE) || (ReqSize % RPMB_FRAME_SIZE)) {
    DEBUG ((DEBUG_ERROR, "Invalid Size Req(%u)/Resp(%u) \n", ReqSize, RespSize));
    Status =  EFI_INVALID_PARAMETER;
    goto Error;
  }

  NumReqFrames  = (ReqSize / RPMB_FRAME_SIZE);
  NumRespFrames = (RespSize / RPMB_FRAME_SIZE);
  BytesToUint16 (ReqFrame->Request, &Request);

  switch (Request) {
    case RPMB_MSG_TYPE_REQ_WRITE_COUNTER_VAL_READ:
      if ((NumReqFrames != 1) || (NumRespFrames != 1)) {
        DEBUG ((
          DEBUG_ERROR,
          "Invalid NumFrames (Resp %d Req %d) for Cmd %d\n",
          NumRespFrames,
          NumReqFrames,
          Request
          ));
        Status =  EFI_INVALID_PARAMETER;
        goto Error;
      }

      ZeroMem (RespFrame, RPMB_FRAME_SIZE);
      Status = HandleRpmbRead (PassThru, Slot, RpmbReq, NumReqFrames, RpmbResp, NumRespFrames);
      break;
    case RPMB_MSG_TYPE_REQ_AUTH_DATA_WRITE:
      Status = HandleRpmbWrite (PassThru, Slot, RpmbReq, NumReqFrames, RpmbResp, NumRespFrames);
      break;
    case RPMB_MSG_TYPE_REQ_AUTH_DATA_READ:
      Status = HandleRpmbRead (PassThru, Slot, RpmbReq, NumReqFrames, RpmbResp, NumRespFrames);
      break;
    default:
      Status = EFI_INVALID_PARAMETER;
      goto Error;
      break;
  }

Error:
  return Status;
}

VOID
EFIAPI
HandleCmdRpmb (
  OPTEE_MESSAGE_ARG  *Msg
  )
{
  OPTEE_SHM_COOKIE  *ReqShm;
  OPTEE_SHM_COOKIE  *RespShm;
  VOID              *ReqBuf;
  VOID              *RespBuf;
  UINT64            ReqSize;
  UINT64            RespSize;
  RPMB_REQUEST      *RpmbReq;
  RPMB_DEV_INFO     *DevInfo;
  EFI_STATUS        Status;

  if ((Msg->NumParams != 2) ||
      (Msg->Params[0].Attribute != OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INPUT) ||
      (Msg->Params[1].Attribute != OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_OUTPUT))
  {
    DEBUG ((
      DEBUG_INFO,
      "Invalid RPMB Params Num %d [0].Attr %d [1].Attr %d\n",
      Msg->NumParams,
      Msg->Params[0].Attribute,
      Msg->Params[1].Attribute
      ));
    Msg->Return = OPTEE_ERROR_BAD_PARAMS;
    return;
  }

  ReqShm  = (OPTEE_SHM_COOKIE *)(UINT64)Msg->Params[0].Union.RMemory.SharedMemoryReference;
  ReqBuf  = (UINT8 *)ReqShm->Addr + Msg->Params[0].Union.RMemory.Offset;
  ReqSize = Msg->Params[0].Union.RMemory.Size;

  RespShm  = (OPTEE_SHM_COOKIE *)(UINT64)Msg->Params[1].Union.RMemory.SharedMemoryReference;
  RespBuf  = (UINT8 *)RespShm->Addr + Msg->Params[1].Union.RMemory.Offset;
  RespSize = Msg->Params[1].Union.RMemory.Size;

  DEBUG ((
    DEBUG_INFO,
    "ReqShm Addr %lx Size %ld Offset %ld Buf %lx\n",
    Msg->Params[0].Union.RMemory.SharedMemoryReference,
    ReqSize,
    Msg->Params[0].Union.RMemory.Offset,
    (UINT64)ReqBuf
    ));
  DEBUG ((
    DEBUG_INFO,
    "RespShm Addr %lx Size %ld Offset %ld Buf %lx\n",
    Msg->Params[1].Union.RMemory.SharedMemoryReference,
    RespSize,
    Msg->Params[1].Union.RMemory.Offset,
    (UINT64)RespBuf
    ));

  RpmbReq = ReqBuf;

  if (EmmcPassThru == NULL) {
    Status = GetEmmcDevice ();
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to Locate EMMC PassThrough Protocol:%r\n",
        Status
        ));
      return;
    }
  }

  switch (RpmbReq->Cmd) {
    case RPMB_GET_DEV_INFO:
      DevInfo = RespBuf;
      GetRpmbDevInfo (DevInfo, EmmcPassThru, EmmcSlot);
      Msg->Return = OPTEE_SUCCESS;
      break;
    case RPMB_DATA_REQ:
      Status = HandleRpmbDataReq (
                 EmmcPassThru,
                 EmmcSlot,
                 ((VOID *)((RPMB_REQUEST *)(RpmbReq) + 1)),
                 (ReqSize - sizeof (RPMB_REQUEST)),
                 RespBuf,
                 RespSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "Failed HandleDataReq %r \n", Status));
      } else {
        DEBUG ((DEBUG_INFO, "Handled HandleDataReq %r \n", Status));
        Msg->Return = OPTEE_SUCCESS;
      }

      break;
    default:
      DEBUG ((DEBUG_INFO, "RPMB: UNKNOWN COMMAND %d\n", RpmbReq->Cmd));
      break;
  }
}
