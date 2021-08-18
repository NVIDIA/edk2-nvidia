/** @file

  NOR Flash Driver

  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/


#include <NorFlashPrivate.h>


EFI_BLOCK_IO_MEDIA Media = {
  0,         // Media ID gets updated during Start
  FALSE,     // Non removable media
  TRUE,      // Media currently present
  0,         // First logical block
  FALSE,     // Not read only
  FALSE,     // Does not cache write data
  SIZE_64KB,  // Block size gets updated during start
  4,         // Alignment required
  0          // Last logical block gets updated during start
};


VENDOR_DEVICE_PATH VendorDevicePath = {
  {
    HARDWARE_DEVICE_PATH,
    HW_VENDOR_DP,
    {
      (UINT8)( sizeof(VENDOR_DEVICE_PATH) ),
      (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8)
    }
  },
  { 0x8332de7f, 0x50c3, 0x47ca, { 0x82, 0x4e, 0x83, 0x3a, 0xac, 0x7c, 0xf1, 0x6d } }
};


BOOLEAN TimeOutMessage = FALSE;


/**
  Read a register in the NOR Flash

  @param[in]  Private               Driver's private data
  @param[in]  Cmd                   Register to be read.
  @param[in]  CmdSize               Length of command.
  @param[out] Resp                  Pointer for register data.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
ReadNorFlashRegister (
  IN  NOR_FLASH_PRIVATE_DATA *Private,
  IN  UINT8                  *Cmd,
  IN  UINT32                 CmdSize,
  OUT UINT8                  *Resp
)
{
  EFI_STATUS              Status;
  QSPI_TRANSACTION_PACKET Packet;

  if ((Private == NULL) ||
      (Resp == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Packet.TxBuf = Cmd;
  Packet.RxBuf = Resp;
  Packet.TxLen = CmdSize;
  Packet.RxLen = sizeof (UINT8);
  Packet.WaitCycles = 0;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash register.\n", __FUNCTION__));
  }

  return Status;
}


/**
  Wait for Write Complete

  @param[in] Private               Driver's private data

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
WaitNorFlashWriteComplete (
  IN NOR_FLASH_PRIVATE_DATA *Private
)
{
  EFI_STATUS Status;
  UINT8      RegCmd;
  UINT8      Resp;
  UINT32     Count;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  RegCmd = NOR_READ_SR1;

  Count = 0;

  do {
    // Error out of retry count exceeds NOR_SR1_WEL_RETRY_CNT
    if (Count == NOR_SR1_WIP_RETRY_CNT) {
      Count = 0;
      if (TimeOutMessage == FALSE) {
        DEBUG ((EFI_D_ERROR, "%a: NOR flash write transactions slower than usual.\n", __FUNCTION__));
        TimeOutMessage = TRUE;
      }
    }

    MicroSecondDelay (TIMEOUT);

    // Read WIP status
    Status = ReadNorFlashRegister (Private, &RegCmd, sizeof (RegCmd), &Resp);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash status 1 register.\n", __FUNCTION__));
      return Status;
    }

    Count++;
  } while ((Resp & NOR_SR1_WIP_BMSK) != 0);

  DEBUG ((EFI_D_INFO, "%a: NOR flash write complete.\n", __FUNCTION__));
  return Status;
}


/**
  Configure write enable latch

  @param[in] Private               Driver's private data
  @param[in] Enable                Enable or disable latching

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
ConfigureNorFlashWriteEnLatch (
  IN NOR_FLASH_PRIVATE_DATA *Private,
  IN BOOLEAN                Enable
)
{
  EFI_STATUS              Status;
  UINT8                   Cmd;
  UINT8                   RegCmd;
  QSPI_TRANSACTION_PACKET Packet;
  UINT8                   Resp;
  UINT8                   Cmp;
  UINT32                  Count;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Cmd = Enable ? NOR_WREN_ENABLE : NOR_WREN_DISABLE;
  Cmp = Enable ? NOR_SR1_WEL_BMSK : 0;

  Packet.TxBuf = &Cmd;
  Packet.RxBuf = NULL;
  Packet.TxLen = sizeof(Cmd);
  Packet.RxLen = 0;
  Packet.WaitCycles = 0;

  RegCmd = NOR_READ_SR1;

  Count = 0;

  do {
    // Error out of retry count exceeds NOR_SR1_WEL_RETRY_CNT
    if (Count == NOR_SR1_WEL_RETRY_CNT) {
      Count = 0;
      if (TimeOutMessage == FALSE) {
        DEBUG ((EFI_D_ERROR, "%a: NOR flash write enable latch slower than usual.\n", __FUNCTION__));
        TimeOutMessage = TRUE;
      }
    }

    // Configure WREN
    Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not program WREN latch.\n", __FUNCTION__));
      return Status;
    }

    MicroSecondDelay (TIMEOUT);

    // Read WREN status
    Status = ReadNorFlashRegister (Private, &RegCmd, sizeof (RegCmd), &Resp);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash status 1 register.\n", __FUNCTION__));
      return Status;
    }
    Count++;
  } while ((Resp & NOR_SR1_WEL_BMSK) != Cmp);

  DEBUG ((EFI_D_INFO, "%a: NOR flash WREN %s.\n", __FUNCTION__, Enable ? L"enabled" : L"disabled"));
  return Status;
}


/**
  Read NOR Flash's SFDP

  @param[in] Private               Driver's private data

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
ReadNorFlashSFDP (
  IN NOR_FLASH_PRIVATE_DATA *Private
)
{
  EFI_STATUS                       Status;
  UINT8                            *Cmd;
  UINT32                           CmdSize;
  INT32                            Count;
  UINT32                           AddressShift;
  UINT32                           Offset;
  UINT32                           SFDPSignature;
  NOR_SFDP_HDR                     SFDPHeader;
  NOR_SFDP_PARAM_TBL_HDR           *SFDPParamTblHeaders;
  NOR_SFDP_PARAM_TBL_HDR           *SFDPParamBasicTblHeader;
  NOR_SFDP_PARAM_TBL_HDR           *SFDPParam4ByteInstructionTblHeader;
  NOR_SFDP_PARAM_TBL_HDR           *SFDPParamSectorTblHeader;
  NOR_SFDP_PARAM_BASIC_TBL         *SFDPParamBasicTbl;
  UINT32                           SFDPParamBasicTblSize;
  NOR_SFDP_PARAM_4BI_TBL           *SFDPParam4ByteInstructionTbl;
  UINT32                           SFDPParam4ByteInstructionTblSize;
  NOR_SFDP_PARAM_SECTOR_DESCRIPTOR *SFDPParamSectorTbl;
  UINT32                           SFDPParamSectorTblSize;
  NOR_SFDP_PARAM_SECTOR_REGION     *SFDPParamSectorTblRegion;
  UINT8                            NumRegions;
  UINT32                           MemoryDensity;
  QSPI_TRANSACTION_PACKET          Packet;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Cmd = NULL;
  SFDPParamTblHeaders = NULL;
  SFDPParamBasicTbl = NULL;
  SFDPParam4ByteInstructionTbl = NULL;
  SFDPParamSectorTbl = NULL;

  // Read SFDP Header
  CmdSize = NOR_CMD_SIZE + NOR_SFDP_ADDR_SIZE;
  Cmd = AllocateZeroPool (CmdSize);
  if (Cmd == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Cmd[0] = NOR_READ_SFDP_CMD;

  ZeroMem (&SFDPHeader, sizeof (SFDPHeader));

  Packet.TxBuf = Cmd;
  Packet.RxBuf = &SFDPHeader;
  Packet.TxLen = CmdSize;
  Packet.RxLen = sizeof (SFDPHeader);
  Packet.WaitCycles  = NOR_SFDP_WAIT_CYCLES;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash's SFDP header.\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Verify the read SFDP signature
  SFDPSignature = NOR_SFDP_SIGNATURE;
  if (0 != CompareMem (&SFDPHeader.SFDPSignature, &SFDPSignature, sizeof (SFDPHeader.SFDPSignature))) {
    DEBUG ((EFI_D_ERROR, "%a: NOR flash's SFDP signature invalid.\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto ErrorExit;
  }

  // Read all parameter table headers
  Offset = sizeof (SFDPHeader);
  AddressShift = 0;
  for (Count = (CmdSize - 1); Count > 0; Count--) {
    Cmd[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
    AddressShift += 8;
  }
  Cmd[0] = NOR_READ_SFDP_CMD;

  SFDPParamTblHeaders = AllocateZeroPool ((SFDPHeader.NumParamHdrs + 1) * sizeof (NOR_SFDP_PARAM_TBL_HDR));
  if (SFDPParamTblHeaders == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Packet.TxBuf = Cmd;
  Packet.RxBuf = SFDPParamTblHeaders;
  Packet.TxLen = CmdSize;
  Packet.RxLen = (SFDPHeader.NumParamHdrs + 1) * sizeof (NOR_SFDP_PARAM_TBL_HDR);
  Packet.WaitCycles  = NOR_SFDP_WAIT_CYCLES;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash's SFDP parameter table headers.\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Find the last basic parameter table header
  for (Count = SFDPHeader.NumParamHdrs; Count >= 0; Count--) {
    if (SFDPParamTblHeaders[Count].ParamIDLSB == NOR_SFDP_PRM_TBL_BSC_HDR_LSB &&
        SFDPParamTblHeaders[Count].ParamIDMSB == NOR_SFDP_PRM_TBL_HDR_MSB) {
      break;
    }
  }

  if (Count < 0) {
    DEBUG ((EFI_D_ERROR, "%a: Could not find compatible NOR flash's SFDP parameter table header.\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  SFDPParamBasicTblHeader = &SFDPParamTblHeaders[Count];

  // Use this basic parameter table header to load the full table
  Offset = SFDPParamBasicTblHeader->ParamTblOffset;
  AddressShift = 0;
  for (Count = (CmdSize - 1); Count > 0; Count--) {
    Cmd[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
    AddressShift += 8;
  }
  Cmd[0] = NOR_READ_SFDP_CMD;

  SFDPParamBasicTblSize = SFDPParamBasicTblHeader->ParamTblLen * sizeof (UINT32);
  SFDPParamBasicTbl = AllocateZeroPool (SFDPParamBasicTblSize);
  if (SFDPParamBasicTbl == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;;
  }

  Packet.TxBuf = Cmd;
  Packet.RxBuf = SFDPParamBasicTbl;
  Packet.TxLen = CmdSize;
  Packet.RxLen = SFDPParamBasicTblSize;
  Packet.WaitCycles  = NOR_SFDP_WAIT_CYCLES;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash's SFDP parameters.\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Find the 4 byte instruction parameter table header
  for (Count = SFDPHeader.NumParamHdrs; Count >= 0; Count--) {
    if (SFDPParamTblHeaders[Count].ParamIDLSB == NOR_SFDP_PRM_TBL_4BI_HDR_LSB &&
        SFDPParamTblHeaders[Count].ParamIDMSB == NOR_SFDP_PRM_TBL_HDR_MSB) {
      break;
    }
  }

  if (Count < 0) {
    DEBUG ((EFI_D_ERROR, "%a: Could not find compatible NOR flash's SFDP 4 byte instruction parameter table header.\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  SFDPParam4ByteInstructionTblHeader = &SFDPParamTblHeaders[Count];

  // Use this 4 byte instruction parameter table header to load the full table
  Offset = SFDPParam4ByteInstructionTblHeader->ParamTblOffset;
  AddressShift = 0;
  for (Count = (CmdSize - 1); Count > 0; Count--) {
    Cmd[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
    AddressShift += 8;
  }
  Cmd[0] = NOR_READ_SFDP_CMD;

  SFDPParam4ByteInstructionTblSize = SFDPParam4ByteInstructionTblHeader->ParamTblLen * sizeof (UINT32);
  SFDPParam4ByteInstructionTbl = AllocateZeroPool (SFDPParam4ByteInstructionTblSize);
  if (SFDPParam4ByteInstructionTbl == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;;
  }

  Packet.TxBuf = Cmd;
  Packet.RxBuf = SFDPParam4ByteInstructionTbl;
  Packet.TxLen = CmdSize;
  Packet.RxLen = SFDPParam4ByteInstructionTblSize;
  Packet.WaitCycles  = NOR_SFDP_WAIT_CYCLES;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash's SFDP 4 byte instruction parameters.\n", __FUNCTION__));
    goto ErrorExit;
  }

  if (SFDPParam4ByteInstructionTbl->ReadCmd13 == FALSE ||
      SFDPParam4ByteInstructionTbl->WriteCmd12 == FALSE) {
    DEBUG ((EFI_D_ERROR, "%a: NOR flash's memory density unsupported.\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  // Calculate memory density in bytes.
  MemoryDensity = SFDPParamBasicTbl->MemoryDensity;

  if (MemoryDensity & BIT31) {
    MemoryDensity &= ~BIT31;
    if (MemoryDensity < 32) {
      DEBUG ((EFI_D_ERROR, "%a: NOR flash's memory density unsupported.\n", __FUNCTION__));
      Status = EFI_UNSUPPORTED;
      goto ErrorExit;
    }
    Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity = (UINT64)1 << (MemoryDensity - 3);
  } else {
    MemoryDensity++;
    MemoryDensity >>= 3;
    Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity = MemoryDensity;
  }

  // If uniform 4K erase is supported, use that mode.
  if (SFDPParamBasicTbl->EraseSupport4KB == NOR_SFDP_4KB_ERS_SUPPORTED &&
      SFDPParamBasicTbl->EraseInstruction4KB != NOR_SFDP_4KB_ERS_UNSUPPORTED) {
    Private->PrivateFlashAttributes.FlashAttributes.BlockSize = SIZE_4KB;
  } else {
    // Find the sector map parameter table header
    for (Count = SFDPHeader.NumParamHdrs; Count >= 0; Count--) {
      if (SFDPParamTblHeaders[Count].ParamIDLSB == NOR_SFDP_PRM_TBL_SEC_HDR_LSB &&
          SFDPParamTblHeaders[Count].ParamIDMSB == NOR_SFDP_PRM_TBL_HDR_MSB) {
        break;
      }
    }

    if (Count < 0) {
      DEBUG ((EFI_D_ERROR, "%a: Could not find compatible NOR flash's SFDP sector parameter table header.\n", __FUNCTION__));
      Status = EFI_UNSUPPORTED;
      goto ErrorExit;
    }

    SFDPParamSectorTblHeader = &SFDPParamTblHeaders[Count];

    // Use this sector map parameter table header to load the full table
    Offset = SFDPParamSectorTblHeader->ParamTblOffset;
    AddressShift = 0;
    for (Count = (CmdSize - 1); Count > 0; Count--) {
      Cmd[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
      AddressShift += 8;
    }
    Cmd[0] = NOR_READ_SFDP_CMD;

    SFDPParamSectorTblSize = SFDPParamSectorTblHeader->ParamTblLen * sizeof (UINT32);
    SFDPParamSectorTbl = AllocateZeroPool (SFDPParamSectorTblSize);
    if (SFDPParamSectorTbl == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;;
    }

    Packet.TxBuf = Cmd;
    Packet.RxBuf = SFDPParamSectorTbl;
    Packet.TxLen = CmdSize;
    Packet.RxLen = SFDPParamSectorTblSize;
    Packet.WaitCycles  = NOR_SFDP_WAIT_CYCLES;

    Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not read NOR flash's SFDP sector parameters.\n", __FUNCTION__));
      goto ErrorExit;
    }

    // From sector map parameter table, locate the map descriptor
    Count = 0;
    while (Count < SFDPParamSectorTblHeader->ParamTblLen) {
      if (!SFDPParamSectorTbl[Count].MapDescriptor) {
        // If not map descriptor, it is command descriptor which if followed by
        // data which is same size as descriptor.
        Count += 2;
        continue;
      } else {
        // If map descriptor, find number of regions in the map.
        NumRegions = SFDPParamSectorTbl[Count].RegionCount;
        Count++;
        break;
      }
    }

    if (Count >=  SFDPParamSectorTblHeader->ParamTblLen) {
      DEBUG ((EFI_D_ERROR, "%a: Could not find compatible NOR flash's SFDP sector parameter mapping table.\n", __FUNCTION__));
      Status = EFI_UNSUPPORTED;
      goto ErrorExit;
    }

    // Out of the regions found in the map, find the region with biggest size.
    SFDPParamSectorTblRegion = (NOR_SFDP_PARAM_SECTOR_REGION *) &SFDPParamSectorTbl[Count++];
    while (NumRegions > 0) {
      if (((NOR_SFDP_PARAM_SECTOR_REGION *) &SFDPParamSectorTbl[Count])->RegionSize >
          SFDPParamSectorTblRegion->RegionSize) {
        SFDPParamSectorTblRegion = (NOR_SFDP_PARAM_SECTOR_REGION *) &SFDPParamSectorTbl[Count];
      }
      Count++;
      NumRegions--;
    }

    for (Count = 0; Count < NOR_SFDP_ERASE_COUNT; Count++) {
      if (SFDPParamSectorTblRegion->EraseTypeSupported & (1 << Count)) {
        break;
      }
    }

    if (Count >=  NOR_SFDP_ERASE_COUNT) {
      DEBUG ((EFI_D_ERROR, "%a: Could not find compatible NOR flash's SFDP sector parameter erase table.\n", __FUNCTION__));
      Status = EFI_UNSUPPORTED;
      goto ErrorExit;
    }

    Private->PrivateFlashAttributes.FlashAttributes.BlockSize = 1 << SFDPParamBasicTbl->EraseType[Count].Size;
  }

  // Look up 4 byte erase command based on the block size.
  for (Count = 0; Count < NOR_SFDP_ERASE_COUNT; Count++) {
    if (Private->PrivateFlashAttributes.FlashAttributes.BlockSize ==
         (1 << SFDPParamBasicTbl->EraseType[Count].Size)) {
      break;
    }
  }

  if (Count >=  NOR_SFDP_ERASE_COUNT) {
    DEBUG ((EFI_D_ERROR, "%a: Could not find compatible NOR flash's block size in SFDP sector parameter erase table.\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  if (!(SFDPParam4ByteInstructionTbl->EraseTypeSupported & (1 << Count))) {
    DEBUG ((EFI_D_ERROR, "%a: Could not find compatible NOR flash's erase table supported in SFDP.\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  Private->PrivateFlashAttributes.EraseCmd = SFDPParam4ByteInstructionTbl->EraseInstruction[Count];

  // If basic parameter table size is more than NOR_SFDP_PRM_TBL_LEN_JESD216,
  // read page size from the table. Otherwise default to NOR_SFDP_WRITE_DEF_PAGE
  if (SFDPParamBasicTblSize > NOR_SFDP_PRM_TBL_LEN_JESD216) {
    Private->PrivateFlashAttributes.PageSize = 1 << SFDPParamBasicTbl->PageSize;
    // Override page size for newer flashes
    if (Private->PrivateFlashAttributes.PageSize > NOR_SFDP_WRITE_DEF_PAGE) {
      // If page size if more then 256, default back to 256
      // to avoid any vendor specific configurations needed
      // to support higher page sizes.
      Private->PrivateFlashAttributes.PageSize = NOR_SFDP_WRITE_DEF_PAGE;
    }
  } else {
    Private->PrivateFlashAttributes.PageSize = NOR_SFDP_WRITE_DEF_PAGE;
  }

  Private->FlashInstance = NOR_SFDP_SIGNATURE;

ErrorExit:
  if (Cmd != NULL) {
    FreePool (Cmd);
  }

  if (SFDPParamTblHeaders != NULL) {
    FreePool (SFDPParamTblHeaders);
  }

  if (SFDPParamBasicTbl != NULL) {
    FreePool (SFDPParamBasicTbl);
  }

  if (SFDPParam4ByteInstructionTbl != NULL) {
    FreePool (SFDPParam4ByteInstructionTbl);
  }
  if (SFDPParamSectorTbl != NULL) {
    FreePool (SFDPParamSectorTbl);
  }

  return Status;
}


/**
  Get NOR Flash Attributes.

  @param[in]  This                  Instance to protocol
  @param[out] Attributes            Pointer to flash attributes

  @retval EFI_SUCCESS               Operation successful.
  @retval others                    Error occurred

**/
EFI_STATUS
NorFlashGetAttributes(
  IN  NVIDIA_NOR_FLASH_PROTOCOL *This,
  OUT NOR_FLASH_ATTRIBUTES      *Attributes
)
{
  NOR_FLASH_PRIVATE_DATA  *Private;

  if (This == NULL ||
      Attributes == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  CopyMem (Attributes, &Private->PrivateFlashAttributes.FlashAttributes, sizeof (NOR_FLASH_ATTRIBUTES));

  return EFI_SUCCESS;
}


/**
  Read data from NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to read from
  @param[in] Size                  Number of bytes to be read
  @param[in] Buffer                Address to read data into

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashRead(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Offset,
  IN UINT32                    Size,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  UINT32                  CmdSize;
  UINT32                  Count;
  UINT32                  AddressShift;
  QSPI_TRANSACTION_PACKET Packet;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  FlashDensity;

  if (This == NULL ||
      Buffer == NULL ||
      Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  // Validate that read start and end offsets are within range.
  FlashDensity = Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity;
  if ((Offset > (FlashDensity - 1)) ||
      ((Offset + Size) > (FlashDensity))) {
    return EFI_INVALID_PARAMETER;
  }

  CmdSize = NOR_CMD_SIZE + NOR_ADDR_SIZE;
  ZeroMem (Private->CommandBuffer, CmdSize);

  AddressShift = 0;
  for (Count = (CmdSize - 1); Count > 0; Count--) {
    Private->CommandBuffer[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
    AddressShift += 8;
  }
  Private->CommandBuffer[0] = NOR_READ_DATA_CMD;

  Packet.TxBuf = Private->CommandBuffer;
  Packet.TxLen = CmdSize;
  Packet.RxBuf = Buffer;
  Packet.RxLen = Size;
  Packet.WaitCycles = 0;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not read data from NOR flash.\n", __FUNCTION__));
    goto ErrorExit;
  }

  DEBUG ((EFI_D_INFO, "%a: Successfully read data from NOR flash.\n", __FUNCTION__));

ErrorExit:

  return Status;
}


/**
  Read data from NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] MediaId               Media ID for the device
  @param[in] Lba                   Logical block to start reading from
  @param[in] BufferSize            Number of bytes to be read
  @param[in] Buffer                Address to read data into

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashReadBlock(
  IN EFI_BLOCK_IO_PROTOCOL     *This,
  IN UINT32                    MediaId,
  IN EFI_LBA                   Lba,
  IN UINTN                     BufferSize,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;

  if (This == NULL ||
      Buffer == NULL ||
      BufferSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_BLOCK_IO_PROTOCOL(This);

  if (MediaId != Private->FlashInstance) {
    return EFI_MEDIA_CHANGED;
  }

  Status = NorFlashRead (&Private->NorFlashProtocol,
                         (Lba * Private->PrivateFlashAttributes.FlashAttributes.BlockSize),
                         BufferSize,
                         Buffer);

  return Status;
}


/**
  Erase data from NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Logical block to start erasing from
  @param[in] NumLba                Number of block to be erased

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashErase(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Lba,
  IN UINT32                    NumLba
)
{
  EFI_STATUS              Status;
  UINT32                  CmdSize;
  UINT32                  Count;
  UINT32                  Block;
  UINT32                  AddressShift;
  QSPI_TRANSACTION_PACKET Packet;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  Offset;
  UINT32                  LastBlock;

  if (This == NULL ||
      NumLba == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  LastBlock = (Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity /
                Private->PrivateFlashAttributes.FlashAttributes.BlockSize) - 1;

  if ((Lba > LastBlock) ||
      ((Lba + NumLba - 1) > LastBlock)) {
    return EFI_INVALID_PARAMETER;
  }

  CmdSize = NOR_CMD_SIZE + NOR_ADDR_SIZE;
  ZeroMem (Private->CommandBuffer, CmdSize);

  for (Block = Lba; Block < (Lba + NumLba); Block++) {
    Status = ConfigureNorFlashWriteEnLatch (Private, TRUE);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not enable NOR flash WREN.\n", __FUNCTION__));
      goto ErrorExit;
    }

    AddressShift = 0;
    Offset = Block * Private->PrivateFlashAttributes.FlashAttributes.BlockSize;
    for (Count = (CmdSize - 1); Count > 0; Count--) {
      Private->CommandBuffer[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
      AddressShift += 8;
    }
    Private->CommandBuffer[0] = Private->PrivateFlashAttributes.EraseCmd;

    Packet.TxBuf = Private->CommandBuffer;
    Packet.TxLen = CmdSize;
    Packet.RxBuf = NULL;
    Packet.RxLen = 0;
    Packet.WaitCycles = 0;

    Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not erase data from NOR flash.\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = WaitNorFlashWriteComplete (Private);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not complete NOR flash write.\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = ConfigureNorFlashWriteEnLatch (Private, FALSE);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not enable NOR flash WREN.\n", __FUNCTION__));
      goto ErrorExit;
    }
  }

  DEBUG ((EFI_D_INFO, "%a: Successfully erased data from NOR flash.\n", __FUNCTION__));

ErrorExit:

  return Status;
}


/**
  Erase data from NOR Flash.

  @param[in]       This            Instance to protocol
  @param[in]       MediaId         Media ID for the device
  @param[in]       LBA             Logical block to start erasing from
  @param[in, out]  Token           A pointer to the token associated with the
                                   transaction.
  @param[in]       Size            Number of bytes to be erased


  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashEraseBlock(
  IN     EFI_ERASE_BLOCK_PROTOCOL *This,
  IN     UINT32                   MediaId,
  IN     EFI_LBA                  LBA,
  IN OUT EFI_ERASE_BLOCK_TOKEN    *Token,
  IN     UINTN                    Size
)
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;

  if (This == NULL ||
      Token == NULL ||
      Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_ERASE_BLOCK_PROTOCOL(This);

  if (MediaId != Private->FlashInstance) {
    return EFI_MEDIA_CHANGED;
  }

  Status = NorFlashErase (&Private->NorFlashProtocol,
                          LBA,
                          Size / Private->PrivateFlashAttributes.FlashAttributes.BlockSize);

  if (Token->Event != NULL) {
    Token->TransactionStatus = Status;
    Status = EFI_SUCCESS;
    gBS->SignalEvent (Token->Event);
  }

  return Status;
}


/**
  Write single page data to NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to write to
  @param[in] Size                  Number of bytes to write
  @param[in] Buffer                Address to write data from

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashWriteSinglePage(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Offset,
  IN UINT32                    Size,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  UINT32                  CmdSize;
  UINT32                  Count;
  UINT32                  AddressShift;
  QSPI_TRANSACTION_PACKET Packet;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  FlashDensity;

  if (This == NULL ||
      Buffer == NULL ||
      Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  FlashDensity = Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity;
  if ((Offset > (FlashDensity - 1)) ||
      ((Offset + Size) > (FlashDensity))) {
    return EFI_INVALID_PARAMETER;
  }

  CmdSize = NOR_CMD_SIZE + NOR_ADDR_SIZE;
  ZeroMem (Private->CommandBuffer, CmdSize + Size);
  Status = ConfigureNorFlashWriteEnLatch (Private, TRUE);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not enable NOR flash WREN.\n", __FUNCTION__));
    goto ErrorExit;
  }

  CopyMem (&Private->CommandBuffer[CmdSize], Buffer, Size);
  AddressShift = 0;
  for (Count = (CmdSize - 1); Count > 0; Count--) {
    Private->CommandBuffer[Count] = (Offset & (0xFF << AddressShift)) >> AddressShift;
    AddressShift += 8;
  }
  Private->CommandBuffer[0] = NOR_WRITE_DATA_CMD;

  Packet.TxBuf = Private->CommandBuffer;
  Packet.TxLen = CmdSize + Size;
  Packet.RxBuf = NULL;
  Packet.RxLen = 0;
  Packet.WaitCycles = 0;

  Status = Private->QspiController->PerformTransaction (Private->QspiController, &Packet);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not write data to NOR flash.\n", __FUNCTION__));
    goto ErrorExit;
  }

  Status = WaitNorFlashWriteComplete (Private);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not complete NOR flash write.\n", __FUNCTION__));
    goto ErrorExit;
  }

  Status = ConfigureNorFlashWriteEnLatch (Private, FALSE);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Could not disable NOR flash WREN.\n", __FUNCTION__));
    goto ErrorExit;
  }

  DEBUG ((EFI_D_INFO, "%a: Successfully wrote data to NOR flash.\n", __FUNCTION__));

ErrorExit:

  return Status;
}


/**
  Write data to NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to write to
  @param[in] Size                  Number of bytes to write
  @param[in] Buffer                Address to write data from

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashWrite(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Offset,
  IN UINT32                    Size,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  FlashDensity;
  UINT32                  PageSize;
  UINT32                  BytesToWrite;

  if (This == NULL ||
      Buffer == NULL ||
      Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);

  FlashDensity = Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity;
  if ((Offset > (FlashDensity - 1)) ||
      ((Offset + Size) > (FlashDensity))) {
    return EFI_INVALID_PARAMETER;
  }

  // Writes need to be confined in a page.
  PageSize = Private->PrivateFlashAttributes.PageSize;
  while (Size > 0) {
    // Calculate offset and size within the page
    BytesToWrite = PageSize - (Offset & (PageSize - 1));
    if (BytesToWrite > Size) {
      BytesToWrite = Size;
    }
    Status = NorFlashWriteSinglePage (This, Offset, BytesToWrite, Buffer);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Could not write data to NOR flash.\n", __FUNCTION__));
      return Status;
    }
    Buffer = (UINT8 *)Buffer + BytesToWrite;
    Offset += BytesToWrite;
    Size -= BytesToWrite;
  }

  DEBUG ((EFI_D_INFO, "%a: Successfully wrote data to NOR flash.\n", __FUNCTION__));

  return Status;
}


/**
  Write data to NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] MediaId               Media ID for the device
  @param[in] Lba                   Logical block to start writing from
  @param[in] BufferSize            Number of bytes to be written
  @param[in] Buffer                Address to write data from

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashWriteBlock(
  IN EFI_BLOCK_IO_PROTOCOL     *This,
  IN UINT32                    MediaId,
  IN EFI_LBA                   Lba,
  IN UINTN                     BufferSize,
  IN VOID                      *Buffer
)
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;
  UINT32                  StartPage;
  UINT32                  NumPages;
  UINT32                  PageSize;
  UINT32                  BlockSize;
  UINT8                   *Data;

  if (This == NULL ||
      Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_BLOCK_IO_PROTOCOL(This);

  if (MediaId != Private->FlashInstance) {
    return EFI_MEDIA_CHANGED;
  }

  Status = NorFlashErase (&Private->NorFlashProtocol,
                          Lba,
                          BufferSize / Private->PrivateFlashAttributes.FlashAttributes.BlockSize);

  BlockSize = Private->PrivateFlashAttributes.FlashAttributes.BlockSize;
  PageSize = Private->PrivateFlashAttributes.PageSize;
  StartPage = (BlockSize / PageSize) * Lba;
  NumPages = BufferSize / PageSize;

  Data = Buffer;
  while (NumPages > 0) {
    Status = NorFlashWriteSinglePage (&Private->NorFlashProtocol,
                                      StartPage *  BufferSize / Private->PrivateFlashAttributes.PageSize,
                                      PageSize,
                                      Data);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    StartPage++;
    NumPages--;
    Data += PageSize;
  }

  return Status;
}


/**
  Check for flash part in device tree.

  Looks through all subnodes of the QSPI node to see if any of them has
  spiflash subnode.

  @param[in]   Controller          The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
CheckNorFlashCompatibility(
  IN EFI_HANDLE                Controller
)
{
  EFI_STATUS                       Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode;

  // Check whether device tree node protocol is available.
  DeviceTreeNode = NULL;
  Status = gBS->HandleProtocol (Controller,
                                &gNVIDIADeviceTreeNodeProtocolGuid,
                                (VOID **)&DeviceTreeNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase,
                          DeviceTreeNode->NodeOffset,
                          "flash@0") >= 0) {
    return EFI_SUCCESS;
  }

  if (fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase,
                          DeviceTreeNode->NodeOffset,
                          "spiflash@0") >= 0) {
    return EFI_SUCCESS;
  }

  return EFI_UNSUPPORTED;
}

/**
  Fixup internal data so that EFI can be call in virtual mode.
  Call the passed in Child Notify event and convert any pointers in
  lib to virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
NorVirtualNotifyEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  NOR_FLASH_PRIVATE_DATA *Private;

  Private = (NOR_FLASH_PRIVATE_DATA *)Context;
  EfiConvertPointer (0x0, (VOID**)&Private->CommandBuffer);
  EfiConvertPointer (0x0, (VOID**)&Private->QspiController->PerformTransaction);
  EfiConvertPointer (0x0, (VOID**)&Private->QspiController);
  return;
}

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Since ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN EFI_HANDLE                    Controller,
  IN EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath
)
{
  EFI_STATUS                       Status;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiInstance;

  // Check whether driver has already been started.
  QspiInstance = NULL;
  Status = gBS->OpenProtocol (Controller,
                              &gNVIDIAQspiControllerProtocolGuid,
                              (VOID**) &QspiInstance,
                              This->DriverBindingHandle,
                              Controller,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CheckNorFlashCompatibility (Controller);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->CloseProtocol (Controller,
                               &gNVIDIAQspiControllerProtocolGuid,
                               This->DriverBindingHandle,
                               Controller);
  return Status;
}


/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN EFI_HANDLE                    Controller,
  IN EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath
)
{
  EFI_STATUS                       Status;
  NOR_FLASH_PRIVATE_DATA           *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiInstance;
  EFI_DEVICE_PATH_PROTOCOL         *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL         *NorFlashDevicePath;
  VOID                             *Interface;

  // Open Qspi Controller Protocol
  QspiInstance = NULL;
  Status = gBS->OpenProtocol (Controller,
                              &gNVIDIAQspiControllerProtocolGuid,
                              (VOID**) &QspiInstance,
                              This->DriverBindingHandle,
                              Controller,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to open QSPI Protocol\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Allocate Private Data
  Private = AllocateRuntimeZeroPool (sizeof (NOR_FLASH_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Private->Signature = NOR_FLASH_SIGNATURE;
  Private->QspiControllerHandle = Controller;
  Private->QspiController = QspiInstance;

  // Read NOR flash's SFDP
  Status = ReadNorFlashSFDP (Private);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Allocate Command Buffer
  Private->CommandBuffer = AllocateRuntimeZeroPool (NOR_CMD_SIZE + NOR_ADDR_SIZE +
                                                    Private->PrivateFlashAttributes.PageSize);
  if (Private->CommandBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  // Get Parent's device path.
  Status = gBS->HandleProtocol (Controller,
                                &gEfiDevicePathProtocolGuid,
                                (VOID **)&ParentDevicePath);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to get parent's device path\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Append Vendor device path to parent device path.
  NorFlashDevicePath = AppendDevicePathNode (ParentDevicePath,
                                             (EFI_DEVICE_PATH_PROTOCOL *)&VendorDevicePath);
  if (NorFlashDevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }
  Private->ParentDevicePath = ParentDevicePath;
  Private->NorFlashDevicePath = NorFlashDevicePath;

  // Install Protocols
  Private->NorFlashProtocol.FvbAttributes = EFI_FVB2_READ_ENABLED_CAP |
                                            EFI_FVB2_READ_STATUS |
                                            EFI_FVB2_STICKY_WRITE |
                                            EFI_FVB2_ERASE_POLARITY |
                                            EFI_FVB2_WRITE_STATUS |
                                            EFI_FVB2_WRITE_ENABLED_CAP;
  Private->NorFlashProtocol.GetAttributes = NorFlashGetAttributes;
  Private->NorFlashProtocol.Read = NorFlashRead;
  Private->NorFlashProtocol.Write = NorFlashWrite;
  Private->NorFlashProtocol.Erase = NorFlashErase;

  Status = gBS->InstallMultipleProtocolInterfaces (&Private->NorFlashHandle,
                                                   &gNVIDIANorFlashProtocolGuid,
                                                   &Private->NorFlashProtocol,
                                                   &gEfiDevicePathProtocolGuid,
                                                   Private->NorFlashDevicePath,
                                                   NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install NOR flash protocols\n", __FUNCTION__));
    goto ErrorExit;
  }

  if (PcdGetBool (PcdTegraNorBlockProtocols)) {
    Media.MediaId = Private->FlashInstance;
    Media.BlockSize = Private->PrivateFlashAttributes.FlashAttributes.BlockSize;
    Media.LastBlock = (Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity /
                        Private->PrivateFlashAttributes.FlashAttributes.BlockSize) - 1;

    Private->BlockIoProtocol.Reset = NULL;
    Private->BlockIoProtocol.ReadBlocks = NorFlashReadBlock;
    Private->BlockIoProtocol.WriteBlocks = NorFlashWriteBlock;
    Private->BlockIoProtocol.FlushBlocks = NULL;
    Private->BlockIoProtocol.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION;
    Private->BlockIoProtocol.Media = &Media;

    Private->EraseBlockProtocol.Revision = EFI_ERASE_BLOCK_PROTOCOL_REVISION;
    Private->EraseBlockProtocol.EraseLengthGranularity = 1;
    Private->EraseBlockProtocol.EraseBlocks = NorFlashEraseBlock;

    Status = gBS->InstallMultipleProtocolInterfaces (&Private->NorFlashHandle,
                                                     &gEfiBlockIoProtocolGuid,
                                                     & Private->BlockIoProtocol,
                                                     &gEfiEraseBlockProtocolGuid,
                                                     &Private->EraseBlockProtocol,
                                                     NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to install NOR flash block protocols\n", __FUNCTION__));
      goto ErrorExit;
    }
  }

  Private->ProtocolsInstalled = TRUE;

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_NOTIFY,
                               NorVirtualNotifyEvent,
                               Private,
                               &gEfiEventVirtualAddressChangeGuid,
                               &Private->VirtualAddrChangeEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to create virtual address callback event\r\n",__FUNCTION__));
    goto ErrorExit;
  }
  // Open caller ID protocol for child
  Status = gBS->InstallMultipleProtocolInterfaces (&Controller,
                                                   &gEfiCallerIdGuid,
                                                   NULL,
                                                   NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to install callerid protocol\n",__FUNCTION__));
    goto ErrorExit;
  }
  Status = gBS->OpenProtocol (Controller,
                              &gEfiCallerIdGuid,
                              (VOID **)&Interface,
                              This->DriverBindingHandle,
                              Private->NorFlashHandle,
                              EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to open caller ID protocol\n", __FUNCTION__));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      gBS->CloseProtocol (Controller,
                          &gEfiCallerIdGuid,
                          This->DriverBindingHandle,
                          Private->NorFlashHandle);
      gBS->UninstallMultipleProtocolInterfaces (Controller,
                                                &gEfiCallerIdGuid,
                                                NULL,
                                                NULL);
      gBS->CloseEvent (Private->VirtualAddrChangeEvent);
      if (Private->ProtocolsInstalled) {
        gBS->UninstallMultipleProtocolInterfaces (Private->NorFlashHandle,
                                                  &gNVIDIANorFlashProtocolGuid,
                                                  &Private->NorFlashProtocol,
                                                  &gEfiDevicePathProtocolGuid,
                                                  Private->NorFlashDevicePath,
                                                  NULL);

        if (PcdGetBool (PcdTegraNorBlockProtocols)) {
          gBS->UninstallMultipleProtocolInterfaces (Private->NorFlashHandle,
                                                    &gEfiBlockIoProtocolGuid,
                                                    &Private->BlockIoProtocol,
                                                    &gEfiEraseBlockProtocolGuid,
                                                    &Private->EraseBlockProtocol,
                                                    NULL);
        }
      }
      if (Private->NorFlashDevicePath != NULL) {
        FreePool (Private->NorFlashDevicePath);
      }
      if (Private->CommandBuffer != NULL) {
        FreePool (Private->CommandBuffer);
      }
      FreePool (Private);
    }
    gBS->CloseProtocol (Controller,
                        &gNVIDIAQspiControllerProtocolGuid,
                        This->DriverBindingHandle,
                        Controller);
  }
  return Status;
}


/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      Controller,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
)
{
  EFI_STATUS             Status;
  NOR_FLASH_PRIVATE_DATA *Private;
  UINT32                 Index;

  for (Index = 0; Index < NumberOfChildren; Index++) {
    Private = NULL;
    Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(This);
    if (Private == NULL) {
      return EFI_DEVICE_ERROR;
    }
    Status = gBS->CloseProtocol (Controller,
                                 &gEfiCallerIdGuid,
                                 This->DriverBindingHandle,
                                 ChildHandleBuffer[Index]);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
    gBS->CloseEvent (Private->VirtualAddrChangeEvent);
    if (Private->ProtocolsInstalled) {
      Status = gBS->UninstallMultipleProtocolInterfaces (ChildHandleBuffer[Index],
                                                         &gNVIDIANorFlashProtocolGuid,
                                                         &Private->NorFlashProtocol,
                                                         &gEfiDevicePathProtocolGuid,
                                                         Private->NorFlashDevicePath,
                                                         NULL);
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      if (PcdGetBool (PcdTegraNorBlockProtocols)) {
        Status = gBS->UninstallMultipleProtocolInterfaces (ChildHandleBuffer[Index],
                                                           &gEfiBlockIoProtocolGuid,
                                                           &Private->BlockIoProtocol,
                                                           &gEfiEraseBlockProtocolGuid,
                                                           &Private->EraseBlockProtocol,
                                                           NULL);
        if (EFI_ERROR (Status)) {
          return EFI_DEVICE_ERROR;
        }
      }
    }
    if (Private->NorFlashDevicePath != NULL) {
      FreePool (Private->NorFlashDevicePath);
    }
    if (Private->CommandBuffer != NULL) {
      FreePool (Private->CommandBuffer);
    }
    FreePool (Private);
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (Controller,
                                                     &gEfiCallerIdGuid,
                                                     NULL,
                                                     NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = gBS->CloseProtocol (Controller,
                               &gNVIDIAQspiControllerProtocolGuid,
                               This->DriverBindingHandle,
                               Controller);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}


EFI_DRIVER_BINDING_PROTOCOL gNorFlashDxeDriverBinding = {
  NorFlashDxeDriverBindingSupported,
  NorFlashDxeDriverBindingStart,
  NorFlashDxeDriverBindingStop,
  0x1,
  NULL,
  NULL
};


/**
  The user Entry Point for module NorFlashDxe. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
InitializeNorFlashDxe (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
)
{
  // TODO: Add component name support.
  return EfiLibInstallDriverBinding (ImageHandle,
                                     SystemTable,
                                     &gNorFlashDxeDriverBinding,
                                     ImageHandle);
}
