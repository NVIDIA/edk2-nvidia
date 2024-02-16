/** @file

  NOR Flash Driver Private Data

  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
#include <Library/TegraPlatformInfoLib.h>

#include <libfdt.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/BlockIo.h>
#include <Protocol/EraseBlock.h>
#include <Protocol/NorFlash.h>
#include <Protocol/QspiController.h>
#include <Protocol/DeviceTreeNode.h>

#define NOR_FLASH_SIGNATURE  SIGNATURE_32('N','O','R','F')
#define NOR_SFDP_SIGNATURE   SIGNATURE_32('S','F','D','P')
#define QSPI_BASE_ADDRESS    0x3270000

#define MX_TIMEOUT  100

#define NOR_READ_SR1           0x5
#define NOR_SR1_WEL_BMSK       0x2
#define NOR_SR1_WIP_BMSK       0x1
#define NOR_SR1_WEL_RETRY_CNT  2000
#define NOR_SR1_WIP_RETRY_CNT  2000

#define NOR_CMD_SIZE   1
#define NOR_ADDR_SIZE  4

#define NOR_WRITE_DATA_CMD      0x12
#define NOR_FAST_READ_DATA_CMD  0x0C
#define NOR_READ_DATA_CMD       0x13
#define NOR_WREN_DISABLE        0x4
#define NOR_WREN_ENABLE         0x6

#define NOR_READ_SFDP_CMD             0x5A
#define NOR_SFDP_ADDR_SIZE            3
#define NOR_SFDP_WAIT_CYCLES          8
#define NOR_SFDP_PRM_TBL_HDR_MSB      0xFF
#define NOR_SFDP_PRM_TBL_BSC_HDR_LSB  0x0
#define NOR_SFDP_PRM_TBL_SEC_HDR_LSB  0x81
#define NOR_SFDP_PRM_TBL_4BI_HDR_LSB  0x84

#define NOR_SFDP_4KB_ERS_SUPPORTED    0x1
#define NOR_SFDP_4KB_ERS_UNSUPPORTED  0xFF

#define NOR_DUAL_IO_UNSUPPORTED  0xFF

#define NOR_SFDP_ERASE_COUNT  4

#define NOR_SFDP_WRITE_DEF_PAGE                        256
#define NOR_SFDP_PROGRAM_FIRST_BYTE_TIME_DEFAULT       15
#define NOR_SFDP_PROGRAM_ADDITIONAL_BYTE_TIME_DEFAULT  1
#define NOR_SFDP_PROGRAM_PAGE_TIME_DEFAULT             120
#define NOR_SFDP_PROGRAM_MAX_TIME_MULTIPLIER_DEFAULT   24

#define NOR_SFDP_ERASE_REGION_SIZE  256

#define NOR_SFDP_FAST_READ_DEF_WAIT  8

#define NOR_FAST_CMD_THRESH_FREQ  100000000

#define NOR_READ_RDID_CMD              0x9f
#define NOR_READ_RDID_RESP_SIZE        3
#define NOR_RDID_MANU_ID_OFFSET        0
#define NOR_RDID_MEM_INTF_TYPE_OFFSET  1
#define NOR_RDID_MEM_DENSITY_OFFSET    2

#pragma pack(1)
typedef struct {
  UINT32    SFDPSignature;
  UINT8     MinorVersion;
  UINT8     MajorVersion;
  UINT8     NumParamHdrs;
  UINT8     SFDPAccessProtocol;
} NOR_SFDP_HDR;

typedef struct {
  UINT8     ParamIDLSB;
  UINT8     ParamTblMinorVersion;
  UINT8     ParamTblMajorVersion;
  UINT8     ParamTblLen;
  UINT32    ParamTblOffset : 24;
  UINT8     ParamIDMSB;
} NOR_SFDP_PARAM_TBL_HDR;

typedef struct {
  UINT8    Size;
  UINT8    Command;
} NOR_SFDP_PARAM_ERASE_TYPE;

typedef struct {
  // 1st DWORD
  UINT8                        EraseSupport4KB      : 2;
  UINT8                        Reserved             : 6;
  UINT8                        EraseInstruction4KB;
  UINT8                        Supports1s1s2s       : 1;
  UINT8                        AddressBytes         : 2;
  UINT8                        SupportsDTR          : 1;
  UINT8                        Supports1s2s2s       : 1;
  UINT8                        Supports1s4s4s       : 1;
  UINT8                        Supports1s1s4s       : 1;
  UINT8                        Reserved1_1          : 1;
  UINT8                        Reserved1_2;
  // 2nd DWORD
  UINT32                       MemoryDensity;
  // 3rd DWORD
  UINT8                        Read1s4s4sWaitCycles : 4;
  UINT8                        Read1s4s4sModeClocks : 4;
  UINT8                        Read1s4s4sCmd;
  UINT8                        Read1s1s4sWaitCycles : 4;
  UINT8                        Read1s1s4sModeClocks : 4;
  UINT8                        Read1s1s4sCmd;
  // 4th DWORD
  UINT16                       Reserved4;
  UINT8                        DualIODummyCycles    : 5;
  UINT8                        DualIOModeCycles     : 3;
  UINT8                        DualIOInstruction;
  // 5th DWORD
  UINT32                       Supports2s2s2s       : 1;
  UINT32                       Reserved5_1          : 3;
  UINT32                       Supports4s4s4s       : 1;
  UINT32                       Reserved5            : 27;
  // 6th DWORD
  UINT32                       Reserved6;
  // 7th DWORD
  UINT16                       Reserved7;
  UINT8                        Read4s4s4sWaitCycles : 5;
  UINT8                        Read4s4s4sModeClocks : 3;
  UINT8                        Read4s4s4sCmd;
  // 8th & 9th DWORD
  NOR_SFDP_PARAM_ERASE_TYPE    EraseType[NOR_SFDP_ERASE_COUNT];
  // 10th DWORD
  UINT32                       Reserved10;
  // 11th DWORD
  union {
    UINT32    Dword11;
    struct {
      UINT32    ProgramMaxTimeMultiplier              : 4;
      UINT32    PageSize                              : 4;
      UINT32    PageProgramTypicalTime                : 5;
      UINT32    PageProgramTypicalTimeUnits           : 1;
      UINT32    ByteProgramTypicalTimeFirst           : 4;
      UINT32    ByteProgramTypicalTimeFirstUnits      : 1;
      UINT32    ByteProgramTypicalTimeAdditional      : 4;
      UINT32    ByteProgramTypicalTimeAdditionalUnits : 1;
      UINT32    ChipEraseTypicalTime                  : 5;
      UINT32    ChipEraseTypicalTimeUnits             : 2;
      UINT32    Reserved11                            : 1;
    };
  };

  // 12th DWORD
  UINT32    Reserved12;
  // 13th DWORD
  UINT32    Reserved13;
  // 14th DWORD
  UINT32    Reserved14;
  // 15th DWORD
  UINT16    Disable4s4s4sSequence : 4;
  UINT16    Enable4s4s4sSequence  : 5;
  UINT16    SupportsXIP           : 1;
  UINT16    ExitXIP               : 6;
  UINT8     EnterXIP              : 4;
  UINT8     QER                   : 3;
  UINT8     HoldResetDisable      : 1;
  UINT8     Reserved15;
  // 16th DWORD
  UINT32    Reserved16;
} NOR_SFDP_PARAM_BASIC_TBL;

typedef struct {
  BOOLEAN    ReadCmd13          : 1;
  BOOLEAN    ReadCmd0C          : 1;
  UINT8      Reserved2          : 4;
  BOOLEAN    WriteCmd12         : 1;
  UINT8      Reserved3          : 2;
  UINT8      EraseTypeSupported : 4;
  UINT32     Reserved4          : 19;
  UINT8      EraseInstruction[NOR_SFDP_ERASE_COUNT];
} NOR_SFDP_PARAM_4BI_TBL;

typedef struct {
  BOOLEAN    EndDescriptor : 1;
  BOOLEAN    MapDescriptor : 1;
  UINT16     Reserved      : 14;
  UINT8      RegionCount;
  UINT8      Reserved2;
} NOR_SFDP_PARAM_SECTOR_DESCRIPTOR;

typedef struct {
  UINT8     EraseTypeSupported : 4;
  UINT8     Reserved           : 4;
  UINT32    RegionSize         : 24;
} NOR_SFDP_PARAM_SECTOR_REGION;
#pragma pack()

typedef struct {
  NOR_FLASH_ATTRIBUTES    FlashAttributes;
  UINT8                   UniformEraseCmd;
  UINT8                   HybridEraseCmd;
  UINT32                  PageSize;
  UINT8                   ReadWaitCycles;
  UINT64                  HybridMemoryDensity;
  UINT32                  HybridBlockSize;
  BOOLEAN                 FastReadSupport;
} NOR_FLASH_PRIVATE_ATTRIBUTES;

typedef struct {
  UINT32                             Signature;
  UINT32                             FlashInstance;
  EFI_HANDLE                         QspiControllerHandle;
  EFI_HANDLE                         NorFlashHandle;
  BOOLEAN                            ProtocolsInstalled;
  UINT8                              QspiChipSelect;
  NVIDIA_NOR_FLASH_PROTOCOL          NorFlashProtocol;
  EFI_BLOCK_IO_PROTOCOL              BlockIoProtocol;
  EFI_ERASE_BLOCK_PROTOCOL           EraseBlockProtocol;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL    *QspiController;
  EFI_DEVICE_PATH_PROTOCOL           *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL           *NorFlashDevicePath;
  NOR_FLASH_PRIVATE_ATTRIBUTES       PrivateFlashAttributes;
  EFI_EVENT                          VirtualAddrChangeEvent;
  UINT8                              *CommandBuffer;
} NOR_FLASH_PRIVATE_DATA;

typedef struct {
  EFI_STATUS  (*Initialize)(
    UINT64  QspiBaseAddress,
    UINT8   FlashCS
    );
  EFI_STATUS  (*IsInitialized)(
    BOOLEAN  *Initialized
    );
  EFI_STATUS  (*EnableWriteProtect)(
    VOID
    );
  EFI_STATUS  (*IsWriteProtectEnabled)(
    BOOLEAN  *Enabled
    );
  EFI_STATUS  (*Lock)(
    UINT32  Address
    );
  EFI_STATUS  (*IsLocked)(
    UINT32   Address,
    BOOLEAN  *Locked
    );
} NOR_FLASH_LOCK_OPS;

typedef struct {
  CHAR8                 Name[32];
  UINT8                 ManufacturerId;
  UINT8                 MemoryType;
  UINT8                 Density;
  NOR_FLASH_LOCK_OPS    *LockOps;
} NOR_FLASH_DEVICE_INFO;

#define NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL(a)    CR(a, NOR_FLASH_PRIVATE_DATA, NorFlashProtocol, NOR_FLASH_SIGNATURE)
#define NOR_FLASH_PRIVATE_DATA_FROM_BLOCK_IO_PROTOCOL(a)     CR(a, NOR_FLASH_PRIVATE_DATA, BlockIoProtocol, NOR_FLASH_SIGNATURE)
#define NOR_FLASH_PRIVATE_DATA_FROM_ERASE_BLOCK_PROTOCOL(a)  CR(a, NOR_FLASH_PRIVATE_DATA, EraseBlockProtocol, NOR_FLASH_SIGNATURE)

#endif
