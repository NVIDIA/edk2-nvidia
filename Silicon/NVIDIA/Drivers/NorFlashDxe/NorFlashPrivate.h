/** @file

  NOR Flash Driver Private Data

  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#ifndef __NOR_FLASH_PRIVATE_H__
#define __NOR_FLASH_PRIVATE_H__


#include <PiDxe.h>

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiRuntimeLib.h>

#include <libfdt.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/BlockIo.h>
#include <Protocol/EraseBlock.h>
#include <Protocol/NorFlash.h>
#include <Protocol/QspiController.h>
#include <Protocol/DeviceTreeNode.h>


#define NOR_FLASH_SIGNATURE SIGNATURE_32('N','O','R','F')
#define NOR_SFDP_SIGNATURE  SIGNATURE_32('S','F','D','P')

#define TIMEOUT                       100

#define NOR_READ_SR1                  0x5
#define NOR_SR1_WEL_BMSK              0x2
#define NOR_SR1_WIP_BMSK              0x1
#define NOR_SR1_WEL_RETRY_CNT         2000
#define NOR_SR1_WIP_RETRY_CNT         2000

#define NOR_CMD_SIZE                  1
#define NOR_ADDR_SIZE                 4

#define NOR_WRITE_DATA_CMD            0x12
#define NOR_READ_DATA_CMD             0x13
#define NOR_WREN_DISABLE              0x4
#define NOR_WREN_ENABLE               0x6
#define NOR_ERASE_DATA_CMD            0xDC
#define NOR_DEF_ERASE_DATA_CMD        0xD8

#define NOR_READ_SFDP_CMD             0x5A
#define NOR_SFDP_ADDR_SIZE            3
#define NOR_SFDP_WAIT_CYCLES          8
#define NOR_SFDP_PRM_TBL_LEN_JESD216  36

#define NOR_SFDP_ERASE_COUNT          4

#define NOR_SFDP_WRITE_DEF_PAGE       256


#pragma pack(1)
typedef struct {
  UINT32                           SFDPSignature;
  UINT8                            MinorVersion;
  UINT8                            MajorVersion;
  UINT8                            NumParamHdrs;
  UINT8                            SFDPAccessProtocol;
} NOR_SFDP_HDR;


typedef struct {
  UINT8                            ParamIDLSB;
  UINT8                            ParamTblMinorVersion;
  UINT8                            ParamTblMajorVersion;
  UINT8                            ParamTblLen;
  UINT32                           ParamTblOffset:24;
  UINT8                            ParamIDMSB;
} NOR_SFDP_PARAM_TBL_HDR;


typedef struct {
  UINT8                            Size;
  UINT8                            Command;
} NOR_SFDP_PARAM_ERASE_TYPE;


typedef struct {
  UINT32                           Reserved;
  UINT32                           MemoryDensity;
  UINT32                           Reserved2;
  UINT32                           Reserved3;
  UINT32                           Reserved4;
  UINT32                           Reserved5;
  UINT32                           Reserved6;
  NOR_SFDP_PARAM_ERASE_TYPE        EraseType[NOR_SFDP_ERASE_COUNT];
  UINT32                           Reserved7;
  UINT8                            Reserved8:4;
  UINT8                            PageSize:4;
  UINT32                           Reserved9:24;
} NOR_SFDP_PARAM_TBL;
#pragma pack()


typedef struct {
  UINT32                           Signature;
  UINT32                           FlashInstance;
  EFI_HANDLE                       QspiControllerHandle;
  EFI_HANDLE                       NorFlashHandle;
  BOOLEAN                          ProtocolsInstalled;
  NVIDIA_NOR_FLASH_PROTOCOL        NorFlashProtocol;
  EFI_BLOCK_IO_PROTOCOL            BlockIoProtocol;
  EFI_ERASE_BLOCK_PROTOCOL         EraseBlockProtocol;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiController;
  EFI_DEVICE_PATH_PROTOCOL         *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL         *NorFlashDevicePath;
  NOR_FLASH_ATTRIBUTES             FlashAttributes;
  EFI_EVENT                        VirtualAddrChangeEvent;
  UINT8                            *CommandBuffer;
} NOR_FLASH_PRIVATE_DATA;


#define NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(a)   CR(a, NOR_FLASH_PRIVATE_DATA, NorFlashProtocol, NOR_FLASH_SIGNATURE)
#define NOR_FLASH_PRIVATE_DATA_FROM_BLOCK_IO_PROTOCOL(a)    CR(a, NOR_FLASH_PRIVATE_DATA, BlockIoProtocol, NOR_FLASH_SIGNATURE)
#define NOR_FLASH_PRIVATE_DATA_FROM_ERASE_BLOCK_PROTOCOL(a) CR(a, NOR_FLASH_PRIVATE_DATA, EraseBlockProtocol, NOR_FLASH_SIGNATURE)


#endif
