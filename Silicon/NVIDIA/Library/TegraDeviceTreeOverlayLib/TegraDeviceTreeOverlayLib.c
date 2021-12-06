/** @file
  Tegra Device Tree Overlay Library

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
**/

#include <Base.h>
#include <Uefi.h>
#include <libfdt.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>

STATIC CHAR8 *SWModule;

typedef enum {
  MATCH_OR=0,
  MATCH_AND
} MATCH_OPERATOR;

typedef struct {
  CHAR8   Name[16];
  UINT32  Count;
  MATCH_OPERATOR MatchOp;
  BOOLEAN (*IsMatch)(VOID *Fdt, CONST CHAR8 *Item, VOID *Param);
} DT_MATCH_INFO;

typedef UINT32 BOARD_ID_MATCH_TYPE;

#define BOARD_ID_MATCH_EXACT   0
#define BOARD_ID_MATCH_PARTIAL 1
#define BOARD_ID_MATCH_GT      2
#define BOARD_ID_MATCH_GE      3
#define BOARD_ID_MATCH_LT      4
#define BOARD_ID_MATCH_LE      5

STATIC BOOLEAN MatchId (VOID *, CONST CHAR8 *, VOID *);
STATIC BOOLEAN MatchOdmData (VOID *, CONST CHAR8 *, VOID *);
STATIC BOOLEAN MatchSWModule (VOID *, CONST CHAR8 *, VOID *);
STATIC BOOLEAN MatchFuseInfo (VOID *, CONST CHAR8 *, VOID *);

DT_MATCH_INFO MatchInfoArray[] = {
  {
    .Name = "ids",
    .MatchOp = MATCH_OR,
    .IsMatch = MatchId,
  },
  {
    .Name = "odm-data",
    .MatchOp = MATCH_AND,
    .IsMatch = MatchOdmData,
  },
  {
    .Name = "sw-modules",
    .MatchOp = MATCH_OR,
    .IsMatch = MatchSWModule,
  },
  {
    .Name = "fuse-info",
    .MatchOp = MATCH_AND,
    .IsMatch = MatchFuseInfo,
  },
};

STATIC TEGRA_BOARD_INFO BoardInfo = {
  .FuseBaseAddr = 0,
  .FuseList = NULL,
  .FuseCount = 0,
  .BoardId = "\0"
};

STATIC INTN GetFabId(CONST CHAR8 *BoardId)
{
  INTN FabId = 0;
  INTN Index;
  INTN Id;

  if (AsciiStrLen(BoardId) < 13) {
    return -1;
  }

  for (Index = 0; Index < 3; Index++) {
    Id = BoardId[Index + 10];
    if (Id >= '0' && Id <= '9') {
      Id = Id - '0';
    } else if (Id >= 'a' && Id <= 'z') {
      Id = Id - 'a' + 10;
    } else if (Id >= 'A' && Id <= 'Z') {
      Id = Id - 'A' + 10;
    } else {
      return -1;
    }
    FabId = FabId * 100 + Id;
  }

  return FabId;
}

STATIC BOOLEAN MatchId(VOID *Fdt, CONST CHAR8 *Id, VOID *Param)
{
  INTN                IdLen = AsciiStrLen(Id);
  CHAR8               *IdStr = (CHAR8 *)Id;
  BOARD_ID_MATCH_TYPE MatchType  = BOARD_ID_MATCH_EXACT;
  INTN                FabId, BoardFabId, i;
  INTN                BoardIdLen;
  CONST CHAR8         *BoardId = NULL;

  BOOLEAN Matched = FALSE;

  BoardFabId = 0;
  FabId = 0;

  if ((IdLen > 2) && (IdStr[0] == '>') && (IdStr[1] == '=')) {
    IdStr += 2;
    IdLen -= 2;
    MatchType = BOARD_ID_MATCH_GE;
    goto match_type_done;
  }

  if ((IdLen > 1) && (IdStr[0] == '>')) {
    IdStr += 1;
    IdLen -= 1;
    MatchType = BOARD_ID_MATCH_GT;
    goto match_type_done;
  }

  if ((IdLen > 2) && (IdStr[0] == '<') && (IdStr[1] == '=')) {
    IdStr += 2;
    IdLen -= 2;
    MatchType = BOARD_ID_MATCH_LE;
    goto match_type_done;
  }

  if ((IdLen > 1) && (IdStr[0] == '<')) {
    IdStr += 1;
    IdLen -= 1;
    MatchType = BOARD_ID_MATCH_LT;
    goto match_type_done;
  }

  if ((IdLen > 1) && (IdStr[0] == '^')) {
    IdStr += 1;
    IdLen -= 1;
    MatchType = BOARD_ID_MATCH_PARTIAL;
    goto match_type_done;
  }

  for (i = 0; i < IdLen; i++) {
    if (IdStr[i] == '*') {
      IdLen = i;
      MatchType = BOARD_ID_MATCH_PARTIAL;
      break;
    }
  }

match_type_done:
  if ((MatchType == BOARD_ID_MATCH_GE) || (MatchType == BOARD_ID_MATCH_GT) ||
      (MatchType == BOARD_ID_MATCH_LE) || (MatchType == BOARD_ID_MATCH_LT)) {
    FabId = GetFabId(IdStr);
    if (FabId < 0) {
      goto finish;
    }
  }

  BoardId = BoardInfo.BoardId;
  BoardIdLen = strlen(BoardId);
  BoardFabId = GetFabId(BoardId);
  DEBUG((DEBUG_INFO,"%a: check if %a(fab:%x) match with plugin %a(fab:%x)\n",
       __FUNCTION__, Id, FabId, BoardId, BoardFabId));

  switch (MatchType) {
    case BOARD_ID_MATCH_EXACT:
      if (BoardIdLen != IdLen) {
        break;
      }
      if (!CompareMem(IdStr, BoardId, IdLen)) {
        Matched = TRUE;
      }
      break;

    case BOARD_ID_MATCH_PARTIAL:
      if (BoardIdLen < IdLen) {
        break;
      }
      if (!CompareMem(IdStr, BoardId, IdLen)) {
        Matched = TRUE;
      }
      break;

    case BOARD_ID_MATCH_GT:
    case BOARD_ID_MATCH_GE:
    case BOARD_ID_MATCH_LT:
    case BOARD_ID_MATCH_LE:
      if (BoardIdLen < 13) {
        break;
      }
      if (CompareMem(IdStr, BoardId, 10)) {
        break;
      }
      if (BoardFabId < 0) {
        break;
      }
      if ((BoardFabId > FabId) &&
        (MatchType == BOARD_ID_MATCH_GT)) {
        Matched = TRUE;
      } else if ((BoardFabId >= FabId) &&
        (MatchType == BOARD_ID_MATCH_GE)) {
        Matched = TRUE;
      } else if ((BoardFabId < FabId) &&
        (MatchType == BOARD_ID_MATCH_LT)) {
        Matched = TRUE;
      } else if ((BoardFabId <= FabId) &&
        (MatchType == BOARD_ID_MATCH_LE)) {
        Matched = TRUE;
      }
      break;
  }

finish:
  DEBUG((DEBUG_INFO,"%a: Board Id match result: %d\n", __FUNCTION__, Matched));
  return Matched;
}

STATIC BOOLEAN MatchOdmData(VOID *Fdt, CONST CHAR8 *OdmData, VOID *Param)
{
  BOOLEAN Matched = FALSE;
  INTN    OdmDataNode;

  OdmDataNode = fdt_path_offset(Fdt, "/chosen/odm-data");
  if (0 > OdmDataNode) {
    DEBUG((DEBUG_ERROR, "%a: Failed to find node /chosen/odm-data\n", __FUNCTION__));
    goto ret_odm_match;
  }

  if (NULL != fdt_get_property(Fdt, OdmDataNode, OdmData, NULL)) {
    Matched = TRUE;
  }

ret_odm_match:
  DEBUG((DEBUG_INFO,"%a: Matching odm-data %a. Result: %d\n", __FUNCTION__, OdmData, Matched));
  return Matched;
}

STATIC BOOLEAN MatchSWModule(VOID *Fdt, CONST CHAR8 *ModuleStr, VOID *Param)
{
  INTN Ret;
  Ret = AsciiStriCmp(SWModule, ModuleStr);
  DEBUG((DEBUG_INFO,"%a: Matching sw-module %a. Result: %ld\n", __FUNCTION__, SWModule, Ret));
  return (Ret == 0) ? TRUE : FALSE;
}

STATIC BOOLEAN MatchFuseInfo(VOID *Fdt, CONST CHAR8 *FuseStr, VOID *Param)
{
  BOOLEAN Matched = FALSE;
  UINT32  Value;
  UINT32  Index;

  if (FuseStr) {
    for (Index = 0; Index < BoardInfo.FuseCount; Index++) {
      if (!AsciiStrnCmp(FuseStr, BoardInfo.FuseList[Index].Name, AsciiStrLen(FuseStr))) {
        Value = MmioRead32(BoardInfo.FuseBaseAddr + BoardInfo.FuseList[Index].Offset);
        if( Value & BoardInfo.FuseList[Index].Value) {
          Matched = TRUE;
          break;
        }
      }
    }
  }
  DEBUG((DEBUG_INFO,"%a: Matching fuse-info %a. Result: %d\n", __FUNCTION__, FuseStr, Matched));
  return Matched;
}

STATIC EFI_STATUS PMGetPropertyCount(VOID *Fdt, INTN Node)
{
  DT_MATCH_INFO *MatchIter = MatchInfoArray;
  UINTN         AllCount = 0;
  UINTN         Index;
  INTN          PropCount;

  for (Index = 0; Index < ARRAY_SIZE(MatchInfoArray); MatchIter++, Index++) {
    PropCount = fdt_stringlist_count(Fdt, Node, MatchIter->Name);
    if (PropCount < 0) {
      DEBUG ((DEBUG_INFO, "%a: Node: %d, Property: %a: Not Found.\n", __FUNCTION__, Node, MatchIter->Name));
      MatchIter->Count = 0;
    } else {
      MatchIter->Count = PropCount;
      DEBUG ((DEBUG_INFO, "%a: Node: %d, Property: %a: Count: %d.\n", __FUNCTION__, Node, MatchIter->Name, PropCount));
    }

    AllCount += MatchIter->Count;
  }

  if (!AllCount) {
    DEBUG ((DEBUG_ERROR, "%a: Found no properties to match in overlay node.\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ReadBoardInfo (
  VOID *Fdt
)
{
  INTN  BoardIdNode;
  INT32 BoardIdLen;
  CONST CHAR8 *BoardId;

  ZeroMem (&BoardInfo, sizeof (BoardInfo));
  GetBoardInfo(&BoardInfo);
  DEBUG((DEBUG_INFO, "Board Id (BCT/EEPROM): %a \n", (CHAR8*)BoardInfo.BoardId));

  if (!AsciiStrnLenS(BoardInfo.BoardId, BOARD_ID_LEN + 1)) {
    DEBUG((DEBUG_WARN, "%a: Failed to get board_id from BCT \n. Reading from device tree.", __FUNCTION__));
    BoardIdNode = fdt_path_offset(Fdt, "/chosen");
    if (0 > BoardIdNode) {
      DEBUG((DEBUG_ERROR, "%a: Failed to find node /chosen\n", __FUNCTION__));
      return EFI_LOAD_ERROR;
    }

    BoardId = fdt_stringlist_get(Fdt, BoardIdNode, "ids", 0, &BoardIdLen);
    if (0 > BoardIdLen) {
      DEBUG((DEBUG_ERROR,"%a: Failed to read prop on /chosen/ids\n", __FUNCTION__));
      return EFI_LOAD_ERROR;
    }
    if (BoardIdLen > BOARD_ID_LEN) {
      DEBUG((DEBUG_ERROR,"%a: BoardId length > %ul(max supported)\n", __FUNCTION__));
      return EFI_LOAD_ERROR;
    }
    AsciiStrCpyS(BoardInfo.BoardId, BOARD_ID_LEN+1, BoardId);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ProcessOverlayDeviceTree (
  VOID *FdtBase,
  VOID *FdtOverlay,
  VOID *FdtBuf
  )
{
  CONST VOID    *Property;
  INT32         PropertyLen;
  INTN          FrNode;
  INTN          BufNode;
  CONST CHAR8   *FrName;
  CONST CHAR8   *NodeName;
  INTN          ConfigNode;
  INT32         FdtErr;
  EFI_STATUS    Status = EFI_SUCCESS;
  BOOLEAN       Found = FALSE;
  DT_MATCH_INFO *MatchIter;
  UINT32        Index;
  UINT32        Count;
  UINT32        NumberSubnodes;

  Property = fdt_getprop (FdtOverlay, 0, "overlay-name", &PropertyLen);
  if (Property != NULL && PropertyLen != 0) {
    DEBUG((DEBUG_ERROR, "Processing \"%a\" DTB overlay\n", Property));
  }

  Status = ReadBoardInfo(FdtBase);
  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_INFO, "Warning: Failed to read board config.\n"));
  }

  fdt_for_each_subnode(FrNode, FdtOverlay, 0) {
    FrName = fdt_get_name(FdtOverlay, FrNode, NULL);
    if (AsciiStrCmp (FrName, "__fixups__") == 0) {
      continue;
    }

    DEBUG((DEBUG_INFO, "Processing node %a for overlay\n", FrName));
    ConfigNode = fdt_subnode_offset(FdtOverlay, FrNode, "board_config");

    if (ConfigNode < 0) {
      continue;
    }

    if(0 > fdt_first_property_offset(FdtOverlay, ConfigNode)) {
      continue;
    }

    Status = PMGetPropertyCount(FdtOverlay, ConfigNode);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_WARN, "%a: Failed to count prop on /%a/board_config.\n", __FUNCTION__, FrName));
      goto delete_fragment;
    }

    MatchIter = MatchInfoArray;
    for (Index = 0; Index < ARRAY_SIZE(MatchInfoArray); Index++, MatchIter++) {
      if (MatchIter->Count > 0 && MatchIter->IsMatch) {
        UINT32 Data = 0;
        CONST CHAR8 *PropStr;

        for (Count = 0, Found = FALSE; Count < MatchIter->Count; Count++) {
          PropStr = fdt_stringlist_get(FdtOverlay, ConfigNode, MatchIter->Name, Count, NULL);
          DEBUG ((DEBUG_INFO, "Check if property %a[%a] on /%a match\n",
               MatchIter->Name, PropStr, FrName));

          Found = MatchIter->IsMatch(FdtBase, PropStr, &Data);
          if (!Found && (MatchIter->MatchOp == MATCH_AND)) {
            break;
          }
          if (Found && (MatchIter->MatchOp == MATCH_OR)) {
             break;
          }
        }
        if(!Found) {
          goto delete_fragment;
        }
        DEBUG ((DEBUG_INFO, "Property %a[%a] on /%a match\n",
               MatchIter->Name, PropStr, FrName));
      }
    }
    continue;

delete_fragment:
    DEBUG((DEBUG_ERROR, "Deleting fragment %a\n", FrName));
    fdt_for_each_subnode(BufNode, FdtBuf, 0) {
      NodeName = fdt_get_name(FdtBuf, BufNode, NULL);
      if (0 == AsciiStrCmp(FrName, NodeName)) {
        FdtErr = fdt_del_node(FdtBuf, BufNode);
        if (FdtErr < 0) {
          DEBUG((DEBUG_ERROR, "Error deleting fragment %a\n", FrName));
          return EFI_DEVICE_ERROR;
        }
        break;
      }
    }
  }

  NumberSubnodes = 0;
  fdt_for_each_subnode(FrNode, FdtBuf, 0) {
    NumberSubnodes++;
  }

  if (NumberSubnodes == 1) {
    fdt_for_each_subnode(FrNode, FdtBuf, 0) {
      FrName = fdt_get_name(FdtBuf, FrNode, NULL);
      if (AsciiStrCmp (FrName, "__fixups__") == 0) {
        FdtErr = fdt_del_node(FdtBuf, FrNode);
        if (FdtErr < 0) {
          DEBUG((DEBUG_ERROR, "Error deleting fragment %a\n", FrName));
          return EFI_DEVICE_ERROR;
        }
      }
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ApplyTegraDeviceTreeOverlay (
  VOID *FdtBase,
  VOID *FdtOverlay,
  CHAR8 *ModuleStr
  )
{
  EFI_STATUS  Status;
  INTN        Err;
  VOID        *FdtNext;
  VOID        *FdtBuf;
  UINTN       BufPageCount;
  UINTN       FdtSize;

  Err = fdt_check_header (FdtBase);
  if (Err != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device Tree header not valid: Err%d\n", __FUNCTION__, Err));
    return EFI_INVALID_PARAMETER;
  }

  BufPageCount = EFI_SIZE_TO_PAGES(fdt_totalsize(FdtBase));
  FdtBuf = AllocatePages(BufPageCount);

  if (FdtBuf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for overlay dtb. \n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Status = EFI_SUCCESS;
  FdtNext = FdtOverlay;
  while (fdt_check_header((VOID *)FdtNext) == 0) {
    /* Process and apply overlay */
    FdtSize = fdt_totalsize (FdtNext);

    if(fdt_open_into (FdtNext, FdtBuf, FdtSize)) {
      DEBUG ((EFI_D_ERROR, "Failed to copy overlay device tree.\r\n"));
      Status =  EFI_LOAD_ERROR;
      goto Exit;
    }

    SWModule = ModuleStr;
    Status = ProcessOverlayDeviceTree(FdtBase, FdtNext, FdtBuf);
    if (EFI_SUCCESS == Status) {
      Err = fdt_overlay_apply(FdtBase, FdtBuf);
      if (Err != 0) {
        DEBUG ((EFI_D_ERROR, "Failed to apply device tree overlay. Error Code = %d\n", Err));
        Status = EFI_DEVICE_ERROR;
        goto Exit;
      }
    }

    FdtNext = (VOID *)((UINT64)FdtNext + FdtSize);
    FdtNext = (VOID *)(ALIGN_VALUE((UINT64)FdtNext, SIZE_4KB));
  }

Exit:
  FreePages(FdtBuf, BufPageCount);
  return Status;
}
