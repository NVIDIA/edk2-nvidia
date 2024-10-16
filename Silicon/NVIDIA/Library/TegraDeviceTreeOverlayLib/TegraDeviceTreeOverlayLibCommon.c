/** @file
  Tegra Device Tree Overlay Library

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Base.h>
#include <Uefi.h>
#include <libfdt.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>
#include <Protocol/Eeprom.h>
#include "TegraDeviceTreeOverlayLibCommon.h"

STATIC CHAR8  *SWModule;
STATIC VOID   *CpublDtb;

typedef enum {
  MATCH_OR = 0,
  MATCH_AND
} MATCH_OPERATOR;

typedef struct {
  CHAR8             Name[16];
  UINT32            Count;
  MATCH_OPERATOR    MatchOp;
  BOOLEAN (*IsMatch)(
    VOID         *Fdt,
    CONST CHAR8  *Item,
    VOID         *Param
    );
} DT_MATCH_INFO;

typedef UINT32 BOARD_ID_MATCH_TYPE;

#define BOARD_ID_MATCH_EXACT    0
#define BOARD_ID_MATCH_PARTIAL  1
#define BOARD_ID_MATCH_GT       2
#define BOARD_ID_MATCH_GE       3
#define BOARD_ID_MATCH_LT       4
#define BOARD_ID_MATCH_LE       5

STATIC BOOLEAN
MatchId (
  VOID *,
  CONST CHAR8 *,
  VOID *
  );

STATIC BOOLEAN
MatchOdmData (
  VOID *,
  CONST CHAR8 *,
  VOID *
  );

STATIC BOOLEAN
MatchSWModule (
  VOID *,
  CONST CHAR8 *,
  VOID *
  );

STATIC BOOLEAN
MatchFuseInfo (
  VOID *,
  CONST CHAR8 *,
  VOID *
  );

DT_MATCH_INFO  MatchInfoArray[] = {
  {
    .Name    = "ids",
    .MatchOp = MATCH_OR,
    .IsMatch = MatchId,
  },
  {
    .Name    = "odm-data",
    .MatchOp = MATCH_AND,
    .IsMatch = MatchOdmData,
  },
  {
    .Name    = "sw-modules",
    .MatchOp = MATCH_OR,
    .IsMatch = MatchSWModule,
  },
  {
    .Name    = "fuse-info",
    .MatchOp = MATCH_AND,
    .IsMatch = MatchFuseInfo,
  },
  {
    .Name    = "fuse-info-OR",
    .MatchOp = MATCH_OR,
    .IsMatch = MatchFuseInfo,
  },
};

STATIC OVERLAY_BOARD_INFO  *BoardInfo = NULL;

STATIC INTN
GetFabId (
  CONST CHAR8  *BoardId,
  UINTN        *FabIdPrefixLen
  )
{
  INTN  FabId = 0;
  INTN  Index;
  INTN  Id;

  if (AsciiStrLen (BoardId) < 13) {
    return -1;
  }

  if (FabIdPrefixLen != NULL) {
    *FabIdPrefixLen = 0;
  }

  for (Index = 0; Index < 3; Index++) {
    Id = BoardId[Index + 10];
    if ((Id >= '0') && (Id <= '9')) {
      Id = Id - '0';
    } else if ((Id >= 'a') && (Id <= 'z')) {
      if (FabIdPrefixLen != NULL) {
        *FabIdPrefixLen += 1;
      }

      continue;
    } else if ((Id >= 'A') && (Id <= 'Z')) {
      if (FabIdPrefixLen != NULL) {
        *FabIdPrefixLen += 1;
      }

      continue;
    } else {
      return -1;
    }

    FabId = FabId * 10 + Id;
  }

  return FabId;
}

STATIC BOOLEAN
MatchId (
  VOID         *Fdt,
  CONST CHAR8  *Id,
  VOID         *Param
  )
{
  INTN                 IdLen = AsciiStrLen (Id);
  CHAR8                *IdStr = (CHAR8 *)Id;
  BOARD_ID_MATCH_TYPE  MatchType = BOARD_ID_MATCH_EXACT;
  INTN                 FabId, BoardFabId, i;
  INTN                 BoardIdLen;
  UINTN                FabIdPrefixLen;
  CONST CHAR8          *BoardId = NULL;

  BOOLEAN  Matched = FALSE;

  BoardFabId = 0;
  FabId      = 0;

  if ((IdLen > 2) && (IdStr[0] == '>') && (IdStr[1] == '=')) {
    IdStr    += 2;
    IdLen    -= 2;
    MatchType = BOARD_ID_MATCH_GE;
    goto match_type_done;
  }

  if ((IdLen > 1) && (IdStr[0] == '>')) {
    IdStr    += 1;
    IdLen    -= 1;
    MatchType = BOARD_ID_MATCH_GT;
    goto match_type_done;
  }

  if ((IdLen > 2) && (IdStr[0] == '<') && (IdStr[1] == '=')) {
    IdStr    += 2;
    IdLen    -= 2;
    MatchType = BOARD_ID_MATCH_LE;
    goto match_type_done;
  }

  if ((IdLen > 1) && (IdStr[0] == '<')) {
    IdStr    += 1;
    IdLen    -= 1;
    MatchType = BOARD_ID_MATCH_LT;
    goto match_type_done;
  }

  if ((IdLen > 1) && (IdStr[0] == '^')) {
    IdStr    += 1;
    IdLen    -= 1;
    MatchType = BOARD_ID_MATCH_PARTIAL;
    goto match_type_done;
  }

  for (i = 0; i < IdLen; i++) {
    if (IdStr[i] == '*') {
      IdLen     = i;
      MatchType = BOARD_ID_MATCH_PARTIAL;
      break;
    }
  }

match_type_done:
  if ((MatchType == BOARD_ID_MATCH_GE) || (MatchType == BOARD_ID_MATCH_GT) ||
      (MatchType == BOARD_ID_MATCH_LE) || (MatchType == BOARD_ID_MATCH_LT))
  {
    FabId = GetFabId (IdStr, &FabIdPrefixLen);
    if (FabId < 0) {
      goto finish;
    }
  }

  for (i = 0; i < BoardInfo->IdCount; i++) {
    BoardId    = TegraBoardIdFromPartNumber (&BoardInfo->ProductIds[i]);
    BoardIdLen = strlen (BoardId);
    BoardFabId = GetFabId (BoardId, NULL);
    DEBUG ((
      DEBUG_INFO,
      "%a: check if overlay node id %a match with %a\n",
      __FUNCTION__,
      Id,
      BoardId
      ));

    switch (MatchType) {
      case BOARD_ID_MATCH_EXACT:
        if (!CompareMem (IdStr, BoardId, IdLen)) {
          Matched = TRUE;
        }

        break;

      case BOARD_ID_MATCH_PARTIAL:
        if (BoardIdLen < IdLen) {
          break;
        }

        if (!CompareMem (IdStr, BoardId, IdLen)) {
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

        if (CompareMem (IdStr, BoardId, 10 + FabIdPrefixLen)) {
          break;
        }

        if (BoardFabId < 0) {
          break;
        }

        if ((BoardFabId > FabId) &&
            (MatchType == BOARD_ID_MATCH_GT))
        {
          Matched = TRUE;
        } else if ((BoardFabId >= FabId) &&
                   (MatchType == BOARD_ID_MATCH_GE))
        {
          Matched = TRUE;
        } else if ((BoardFabId < FabId) &&
                   (MatchType == BOARD_ID_MATCH_LT))
        {
          Matched = TRUE;
        } else if ((BoardFabId <= FabId) &&
                   (MatchType == BOARD_ID_MATCH_LE))
        {
          Matched = TRUE;
        }

        break;
    }

    if (Matched == TRUE) {
      break;
    }
  }

finish:
  DEBUG ((DEBUG_INFO, "%a: Board Id match result: %u\n", __FUNCTION__, Matched));
  return Matched;
}

STATIC BOOLEAN
MatchOdmData (
  VOID         *Fdt,
  CONST CHAR8  *OdmData,
  VOID         *Param
  )
{
  BOOLEAN  Matched = FALSE;
  INTN     OdmDataNode;

  OdmDataNode = fdt_path_offset (CpublDtb, "/chosen/odm-data");
  if (0 > OdmDataNode) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find node /chosen/odm-data\n", __FUNCTION__));
    goto ret_odm_match;
  }

  if (NULL != fdt_get_property (CpublDtb, OdmDataNode, OdmData, NULL)) {
    Matched = TRUE;
  }

ret_odm_match:
  DEBUG ((DEBUG_INFO, "%a: Matching odm-data %a. Result: %u\n", __FUNCTION__, OdmData, Matched));
  return Matched;
}

STATIC BOOLEAN
MatchSWModule (
  VOID         *Fdt,
  CONST CHAR8  *ModuleStr,
  VOID         *Param
  )
{
  INTN  Ret;

  Ret = AsciiStriCmp (SWModule, ModuleStr);
  DEBUG ((DEBUG_INFO, "%a: Matching sw-module %a. Result: %ld\n", __FUNCTION__, SWModule, Ret));
  return (Ret == 0) ? TRUE : FALSE;
}

STATIC BOOLEAN
MatchFuseInfo (
  VOID         *Fdt,
  CONST CHAR8  *FuseStr,
  VOID         *Param
  )
{
  BOOLEAN                 Matched = FALSE;
  UINT32                  Value;
  UINT32                  Index;
  UINT64                  FuseAddr;
  STATIC TEGRA_FUSE_INFO  *FuseInfo;
  BOOLEAN                 MatchIfNonZero = TRUE;

  if (FuseStr) {
    if (FuseStr[0] == '!') {
      MatchIfNonZero = FALSE;
      FuseStr++;
    }

    for (Index = 0; Index < BoardInfo->FuseCount; Index++) {
      FuseInfo = &BoardInfo->FuseList[Index];
      if (!AsciiStrCmp (FuseStr, FuseInfo->Name)) {
        FuseAddr = BoardInfo->FuseBaseAddr + FuseInfo->Offset;
        Value    = MmioRead32 (FuseAddr);
        DEBUG ((DEBUG_INFO, "%a: %a address 0x%llx is 0x%x, checking bits 0x%x to be 0x%x\n", __FUNCTION__, FuseInfo->Name, FuseAddr, Value, FuseInfo->Value, (MatchIfNonZero) ? FuseInfo->Value : 0));
        if ((MatchIfNonZero && (Value & FuseInfo->Value)) ||
            (!MatchIfNonZero && !(Value & FuseInfo->Value)))
        {
          Matched = TRUE;
          break;
        }
      }
    }
  }

  DEBUG ((DEBUG_INFO, "%a: Matching fuse-info %a. Result: %u\n", __FUNCTION__, FuseStr, Matched));
  return Matched;
}

STATIC EFI_STATUS
PMGetPropertyCount (
  VOID  *Fdt,
  INTN  Node
  )
{
  DT_MATCH_INFO  *MatchIter = MatchInfoArray;
  UINTN          AllCount   = 0;
  UINTN          Index;
  INTN           PropCount;

  for (Index = 0; Index < ARRAY_SIZE (MatchInfoArray); MatchIter++, Index++) {
    PropCount = fdt_stringlist_count (Fdt, Node, MatchIter->Name);
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
FdtDeleteProperty (
  VOID         *FdtBase,
  CONST CHAR8  *TargetPath,
  CONST CHAR8  *PropName
  )
{
  INTN  TargetNode;
  INTN  Err;

  TargetNode = fdt_path_offset (FdtBase, TargetPath);
  if (TargetNode < 0) {
    return EFI_DEVICE_ERROR;
  }

  Err = fdt_nop_property (FdtBase, TargetNode, PropName);
  if ( 0 != Err) {
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DEBUG_INFO, "Deleted property %a from %a\n", PropName, TargetPath));
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FdtDeleteSubNode (
  VOID         *FdtBase,
  CONST CHAR8  *TargetPath,
  CONST CHAR8  *NodeName
  )
{
  INTN  TargetNode;
  INTN  SubNode;

  TargetNode = fdt_path_offset (FdtBase, TargetPath);
  if (TargetNode < 0) {
    return EFI_DEVICE_ERROR;
  }

  SubNode = fdt_subnode_offset (FdtBase, TargetNode, NodeName);
  if (SubNode < 0) {
    return EFI_DEVICE_ERROR;
  }

  SubNode = fdt_del_node (FdtBase, SubNode);
  if (SubNode < 0) {
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DEBUG_INFO, "Deleted subnode %a from %a\n", NodeName, TargetPath));
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FdtCleanFixups (
  VOID         *FdtBase,
  CONST CHAR8  *NodeName
  )
{
  VOID         *FdtBuf;
  UINTN        FdtSize;
  UINTN        BufPageCount;
  INTN         SymbolsNode;
  INTN         FixupsNode;
  INTN         FixupsNodeNew;
  INTN         SubNode;
  INTN         PropOffset = 0;
  INT32        PropLen;
  INT32        PropCount;
  INT32        PStrLen;
  CONST CHAR8  *PropName;
  CONST CHAR8  *PropStr;
  CONST VOID   *Prop;
  CHAR8        *NodePath = NULL;
  UINT32       NodeLen;
  CONST CHAR8  *NewProp;
  UINTN        NewPropLen = 0;
  UINT32       Index;
  BOOLEAN      UpdateProp;
  EFI_STATUS   Status = EFI_SUCCESS;
  INTN         Err    = 0;

  if (NodeName == NULL) {
    return EFI_DEVICE_ERROR;
  }

  FdtBuf = NULL;

  NodeLen  = strlen (NodeName);
  NodePath = (CHAR8 *)AllocateZeroPool (NodeLen+2);
  CopyMem ((VOID *)NodePath+1, (VOID *)NodeName, NodeLen);
  NodePath[0] = '/';

  SymbolsNode = fdt_subnode_offset (FdtBase, 0, "__symbols__");
  if (SymbolsNode >= 0) {
    fdt_for_each_property_offset (PropOffset, FdtBase, SymbolsNode) {
      PropStr = fdt_getprop_by_offset (FdtBase, PropOffset, &PropName, &PropLen);
      if (PropLen >= NodeLen+2) {
        if (0 == CompareMem (NodePath, PropStr, NodeLen+1)) {
          if (PropStr[NodeLen+1] == '/') {
            fdt_nop_property (FdtBase, SymbolsNode, PropName);
          }
        }
      }
    }
  }

  FixupsNode = fdt_subnode_offset (FdtBase, 0, "__local_fixups__");
  if (FixupsNode >= 0) {
    SubNode = fdt_subnode_offset (FdtBase, FixupsNode, NodeName);
    if (SubNode >= 0) {
      if (0 > fdt_del_node (FdtBase, SubNode)) {
        DEBUG ((DEBUG_ERROR, "Error deleting fragment %a from __local_fixups__\n", NodeName));
        Status = EFI_DEVICE_ERROR;
        goto ExitFixups;
      }
    }
  }

  FixupsNode = fdt_subnode_offset (FdtBase, 0, "__fixups__");
  if (FixupsNode < 0) {
    goto ExitFixups;
  }

  FdtSize      = fdt_totalsize (FdtBase);
  BufPageCount = EFI_SIZE_TO_PAGES (FdtSize);
  FdtBuf       = AllocatePages (BufPageCount);

  if (FdtBuf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for overlay dtb. \n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ExitFixups;
  }

  if (fdt_open_into (FdtBase, FdtBuf, FdtSize)) {
    DEBUG ((DEBUG_ERROR, "Failed to copy overlay device tree.\r\n"));
    Status =  EFI_LOAD_ERROR;
    goto ExitFixups;
  }

  DEBUG ((DEBUG_INFO, "Removing fixups for fragment: %a\n", NodePath));

  fdt_for_each_property_offset (PropOffset, FdtBuf, FixupsNode) {
    UpdateProp = FALSE;
    Prop       = fdt_getprop_by_offset (FdtBuf, PropOffset, &PropName, &PropLen);
    NewProp    = (CONST CHAR8 *)AllocateZeroPool (PropLen);

    PropCount = fdt_stringlist_count (FdtBuf, FixupsNode, PropName);
    if (PropCount > 0) {
      for (Index = 0; Index < PropCount; Index++) {
        PropStr = fdt_stringlist_get (FdtBuf, FixupsNode, PropName, Index, &PStrLen);
        if (PStrLen >= NodeLen+2) {
          if (0 == CompareMem (NodePath, PropStr, NodeLen+1)) {
            if ((PropStr[NodeLen+1] == '/') || (PropStr[NodeLen+1] == ':')) {
              UpdateProp = TRUE;
              continue;
            }
          }
        }

        CopyMem ((VOID *)(NewProp + NewPropLen), PropStr, PStrLen+1);
        NewPropLen += PStrLen+1;
      }
    }

    if (UpdateProp == TRUE) {
      FixupsNodeNew = fdt_subnode_offset (FdtBase, 0, "__fixups__");
      if (FixupsNodeNew < 0) {
        Status = EFI_DEVICE_ERROR;
        goto ExitFixups;
      }

      if (NewPropLen == 0) {
        Err = fdt_nop_property (FdtBase, FixupsNodeNew, PropName);
      } else {
        Err = fdt_setprop (FdtBase, FixupsNodeNew, PropName, NewProp, NewPropLen);
      }

      if (0 != Err) {
        DEBUG ((DEBUG_ERROR, "Error(%d) updating __fixups__ property: %a.\n", Err, PropName));
      }
    }

    UpdateProp = FALSE;
    FreePool ((VOID *)NewProp);
    NewPropLen = 0;
  }

ExitFixups:
  if (NodePath) {
    FreePool (NodePath);
  }

  if (FdtBuf) {
    FreePages (FdtBuf, BufPageCount);
  }

  return Status;
}

STATIC
EFI_STATUS
ProcessOverlayDeviceTree (
  VOID  *FdtBase,
  VOID  *FdtOverlay,
  VOID  *FdtBuf
  )
{
  CONST CHAR8    *TargetName;
  INT32          TargetLen;
  INTN           FrNode = 0;
  INTN           BufNode;
  CONST CHAR8    *FrName;
  CONST CHAR8    *NodeName;
  INTN           ConfigNode;
  CONST CHAR8    *PropStr;
  INTN           PropCount;
  INT32          FdtErr;
  EFI_STATUS     Status = EFI_SUCCESS;
  BOOLEAN        Found  = FALSE;
  DT_MATCH_INFO  *MatchIter;
  UINT32         Index;
  UINT32         Count;
  UINT32         NumberSubnodes;
  UINT32         MiscNodes = 0;

  TargetName = fdt_getprop (FdtOverlay, 0, "overlay-name", &TargetLen);
  if ((TargetName != NULL) && (TargetLen != 0)) {
    DEBUG ((DEBUG_ERROR, "Processing \"%a\" DTB overlay\n", TargetName));
  }

  fdt_for_each_subnode (FrNode, FdtOverlay, 0) {
    FrName = fdt_get_name (FdtOverlay, FrNode, NULL);
    if ((AsciiStrCmp (FrName, "__fixups__") == 0) || (AsciiStrCmp (FrName, "__local_fixups__") == 0) || (AsciiStrCmp (FrName, "__symbols__") == 0)) {
      MiscNodes++;
      continue;
    }

    DEBUG ((DEBUG_INFO, "Processing node %a for overlay\n", FrName));

    ConfigNode = fdt_subnode_offset (FdtOverlay, FrNode, "board_config");

    if (ConfigNode < 0) {
      goto process_deletes;
    }

    if (0 > fdt_first_property_offset (FdtOverlay, ConfigNode)) {
      goto process_deletes;
    }

    Status = PMGetPropertyCount (FdtOverlay, ConfigNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Failed to count prop on /%a/board_config.\n", __FUNCTION__, FrName));
      goto delete_fragment;
    }

    MatchIter = MatchInfoArray;
    for (Index = 0; Index < ARRAY_SIZE (MatchInfoArray); Index++, MatchIter++) {
      if ((MatchIter->Count > 0) && MatchIter->IsMatch) {
        UINT32  Data = 0;

        for (Count = 0, Found = FALSE; Count < MatchIter->Count; Count++) {
          PropStr = fdt_stringlist_get (FdtOverlay, ConfigNode, MatchIter->Name, Count, NULL);
          DEBUG ((
            DEBUG_INFO,
            "Check if property %a[%a] on /%a match\n",
            MatchIter->Name,
            PropStr,
            FrName
            ));

          Found = MatchIter->IsMatch (FdtBase, PropStr, &Data);
          if (!Found && (MatchIter->MatchOp == MATCH_AND)) {
            break;
          }

          if (Found && (MatchIter->MatchOp == MATCH_OR)) {
            break;
          }
        }

        if (!Found) {
          goto delete_fragment;
        }

        DEBUG ((
          DEBUG_INFO,
          "Property %a[%a] on /%a match\n",
          MatchIter->Name,
          PropStr,
          FrName
          ));
      }
    }

process_deletes:

    TargetName = fdt_getprop (FdtOverlay, FrNode, "target-path", &TargetLen);
    if ((TargetName == NULL) || (TargetLen <= 0)) {
      DEBUG ((DEBUG_ERROR, "'target-path' not found/empty in fragment %a, skipping deletes\n", FrName));
    } else {
      // Delete Nodes
      PropCount = fdt_stringlist_count (FdtOverlay, FrNode, "delete_node");
      if (PropCount > 0) {
        for (Count = 0; Count < PropCount; Count++) {
          PropStr = fdt_stringlist_get (FdtOverlay, FrNode, "delete_node", Count, NULL);
          if (EFI_ERROR (FdtDeleteSubNode (FdtBase, TargetName, PropStr))) {
            DEBUG ((DEBUG_ERROR, "Error deleting node: %a from %a\n", PropStr, TargetName));
            return EFI_DEVICE_ERROR;
          }

          DEBUG ((DEBUG_INFO, "Node Deleted: %a from %a\n", PropStr, TargetName));
        }
      }

      // Delete Properties
      PropCount = fdt_stringlist_count (FdtOverlay, FrNode, "delete_prop");
      if (PropCount > 0) {
        for (Count = 0; Count < PropCount; Count++) {
          PropStr = fdt_stringlist_get (FdtOverlay, FrNode, "delete_prop", Count, NULL);
          if (EFI_ERROR (FdtDeleteProperty (FdtBase, TargetName, PropStr))) {
            DEBUG ((DEBUG_ERROR, "Error deleting property: %a from %a\n", PropStr, TargetName));
            return EFI_DEVICE_ERROR;
          }

          DEBUG ((DEBUG_INFO, "Property Deleted: %a from %a\n", PropStr, TargetName));
        }
      }
    }

    if (fdt_subnode_offset (FdtOverlay, FrNode, "__overlay__") > 0) {
      continue;
    }

delete_fragment:
    DEBUG ((DEBUG_INFO, "Deleting fragment %a\n", FrName));
    // Delete matching __fixups__
    if (EFI_ERROR (FdtCleanFixups (FdtBuf, FrName))) {
      DEBUG ((DEBUG_ERROR, "Error removing reference to %a in __fixups__.\n", FrName));
      return EFI_DEVICE_ERROR;
    }

    fdt_for_each_subnode (BufNode, FdtBuf, 0) {
      NodeName = fdt_get_name (FdtBuf, BufNode, NULL);
      if (0 == AsciiStrCmp (FrName, NodeName)) {
        FdtErr = fdt_del_node (FdtBuf, BufNode);
        if (FdtErr < 0) {
          DEBUG ((DEBUG_ERROR, "Error deleting fragment %a\n", FrName));
          return EFI_DEVICE_ERROR;
        }

        break;
      }
    }
  }

  NumberSubnodes = 0;
  fdt_for_each_subnode (FrNode, FdtBuf, 0) {
    NumberSubnodes++;
  }

  if (NumberSubnodes <= MiscNodes) {
    DEBUG ((DEBUG_INFO, "No matching fragments in the overlay.\n"));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
ApplyTegraDeviceTreeOverlayCommon (
  VOID                *FdtBase,
  VOID                *FdtOverlay,
  CHAR8               *ModuleStr,
  OVERLAY_BOARD_INFO  *OverlayBoardInfo
  )
{
  EFI_STATUS                    Status;
  INTN                          Err;
  VOID                          *FdtNext;
  VOID                          *FdtBuf;
  UINTN                         BufPageCount;
  UINTN                         FdtSize;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  Err = fdt_check_header (FdtBase);
  if (Err != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device Tree header not valid: Err%d\n", __FUNCTION__, Err));
    return EFI_INVALID_PARAMETER;
  }

  BufPageCount = EFI_SIZE_TO_PAGES (fdt_totalsize (FdtBase));
  FdtBuf       = AllocatePages (BufPageCount);

  if (FdtBuf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for overlay dtb. \n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get PlatformResourceInfo\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  CpublDtb = (VOID *)PlatformResourceInfo->ResourceInfo->DtbLoadAddress;
  ASSERT (CpublDtb != NULL);

  BoardInfo = OverlayBoardInfo;
  Status    = EFI_SUCCESS;
  FdtNext   = FdtOverlay;
  while (fdt_check_header ((VOID *)FdtNext) == 0) {
    /* Process and apply overlay */
    FdtSize = fdt_totalsize (FdtNext);

    if (fdt_open_into (FdtNext, FdtBuf, FdtSize)) {
      DEBUG ((DEBUG_ERROR, "Failed to copy overlay device tree.\r\n"));
      Status =  EFI_LOAD_ERROR;
      goto Exit;
    }

    SWModule = ModuleStr;
    Status   = ProcessOverlayDeviceTree (FdtBase, FdtNext, FdtBuf);
    if (EFI_SUCCESS == Status) {
      Err = fdt_overlay_apply (FdtBase, FdtBuf);
      if (Err != 0) {
        DEBUG ((DEBUG_ERROR, "Failed to apply device tree overlay. Error Code = %d\n", Err));
        Status = EFI_DEVICE_ERROR;
        goto Exit;
      }
    } else {
      DEBUG ((DEBUG_INFO, "Overlay skipped.\n"));
      Status = EFI_SUCCESS;
    }

    FdtNext = (VOID *)((UINT64)FdtNext + FdtSize);
    FdtNext = (VOID *)(ALIGN_VALUE ((UINT64)FdtNext, SIZE_4KB));
  }

Exit:
  FreePages (FdtBuf, BufPageCount);
  return Status;
}
