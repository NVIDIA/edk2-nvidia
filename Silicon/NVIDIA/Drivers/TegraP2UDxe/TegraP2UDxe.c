/** @file

  Tegra P2U (PIPE to UPHY) Driver

  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <libfdt.h>

#include "TegraP2UDxePrivate.h"

#define BIT(x)  (1 << (x))

#define P2U_CONTROL_CMN                             0x74
#define P2U_CONTROL_CMN_ENABLE_L2_EXIT_RATE_CHANGE  BIT(13)
#define P2U_CONTROL_CMN_SKP_SIZE_PROTECTION_EN      BIT(20)

#define P2U_PERIODIC_EQ_CTRL_GEN3                          0xc0
#define P2U_PERIODIC_EQ_CTRL_GEN3_PERIODIC_EQ_EN           BIT(0)
#define P2U_PERIODIC_EQ_CTRL_GEN3_INIT_PRESET_EQ_TRAIN_EN  BIT(1)
#define P2U_PERIODIC_EQ_CTRL_GEN4                          0xc4
#define P2U_PERIODIC_EQ_CTRL_GEN4_INIT_PRESET_EQ_TRAIN_EN  BIT(1)

#define P2U_RX_DEBOUNCE_TIME                      0xa4
#define P2U_RX_DEBOUNCE_TIME_DEBOUNCE_TIMER_MASK  0xffff
#define P2U_RX_DEBOUNCE_TIME_DEBOUNCE_TIMER_VAL   160

#define P2U_DIR_SEARCH_CTRL                               0xd4
#define P2U_DIR_SEARCH_CTRL_GEN4_FINE_GRAIN_SEARCH_TWICE  BIT(18)

/**
  Function map region into GCD and MMU

  @param[in]  BaseAddress           Base address of region
  @param[in]  Size                  Size of region

  @return EFI_SUCCESS               GCD/MMU Updated.

**/
EFI_STATUS
AddMemoryRegion (
  IN  UINT64  BaseAddress,
  IN  UINT64  Size
  )
{
  EFI_STATUS  Status;
  UINT64      AlignedBaseAddress = BaseAddress & ~(SIZE_4KB-1);
  UINT64      AlignedSize        = Size + (BaseAddress - AlignedBaseAddress);
  UINT64      AlignedEnd;
  UINT64      ScanLocation;

  AlignedSize = ALIGN_VALUE (Size, SIZE_4KB);
  AlignedEnd  = AlignedBaseAddress + AlignedSize;

  ScanLocation = AlignedBaseAddress;
  while (ScanLocation < AlignedEnd) {
    EFI_GCD_MEMORY_SPACE_DESCRIPTOR  MemorySpace;
    UINT64                           OverlapSize;

    Status = gDS->GetMemorySpaceDescriptor (ScanLocation, &MemorySpace);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to GetMemorySpaceDescriptor (0x%llx): %r.\r\n", __FUNCTION__, ScanLocation, Status));
      return Status;
    }

    OverlapSize = MIN (MemorySpace.BaseAddress + MemorySpace.Length, AlignedEnd) - ScanLocation;
    if (MemorySpace.GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
      Status = gDS->AddMemorySpace (
                      EfiGcdMemoryTypeMemoryMappedIo,
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to AddMemorySpace: (0x%llx, 0x%llx) %r.\r\n", __FUNCTION__, ScanLocation, OverlapSize, Status));
        return Status;
      }

      Status = gDS->SetMemorySpaceAttributes (
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to SetMemorySpaceAttributes: (0x%llx, 0x%llx) %r.\r\n", __FUNCTION__, ScanLocation, OverlapSize, Status));
        return Status;
      }
    }

    ScanLocation += OverlapSize;
  }

  return EFI_SUCCESS;
}

/**
 * Finds the P2U entry for the specified name or id.
 *
 * @param TegraP2UList   - List of all P2Us
 * @param P2UId          - Id to match
 *
 * @return Pointer to list entry
 * @return NULL, if not found
 */
STATIC
TEGRAP2U_LIST_ENTRY *
FindP2UEntry (
  IN LIST_ENTRY  *TegraP2UList,
  IN UINT32      P2UId
  )
{
  LIST_ENTRY  *ListNode;

  if (NULL == TegraP2UList) {
    return NULL;
  }

  ListNode = GetFirstNode (TegraP2UList);
  while (ListNode != TegraP2UList) {
    TEGRAP2U_LIST_ENTRY  *Entry = TEGRAP2U_LIST_FROM_LINK (ListNode);
    if (Entry != NULL) {
      if (Entry->P2UId == P2UId) {
        return Entry;
      }
    }

    ListNode = GetNextNode (TegraP2UList, ListNode);
  }

  return NULL;
}

/**
 * This function initializes the specified P2U instance.
 * This is called for each P2U instance associated with a particular PCIe
 * controller by the PCIe host controller driver.
 *
 * @param[in] This                   The instance of the NVIDIA_TEGRAP2U_PROTOCOL.
 * @param[in] P2UId                  Id of the P2U instance to be initialized.
 *
 * @return EFI_SUCCESS               P2U instance initialized.
 * @return EFI_NOT_FOUND             P2U instance ID is not supported.
 * @return EFI_INVALID_PARAMETER     If invalid parameters are passed.
 */
STATIC
EFI_STATUS
TegraP2UInit (
  IN NVIDIA_TEGRAP2U_PROTOCOL  *This,
  IN UINT32                    P2UId
  )
{
  TEGRAP2U_DXE_PRIVATE  *Private;
  TEGRAP2U_LIST_ENTRY   *Entry;
  UINT32                val;
  UINTN                 ChipID;

  ChipID = TegraGetChipID ();

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = TEGRAP2U_PRIVATE_DATA_FROM_THIS (This);

  Entry = FindP2UEntry (&Private->TegraP2UList, P2UId);
  if (Entry == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to find P2U Entry\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  DEBUG ((
    EFI_D_VERBOSE,
    "%a: P2U Base Addr = 0x%08X\r\n",
    __FUNCTION__,
    Entry->BaseAddr
    ));

  if (Entry->SkipSizeProtectionEn) {
    val  = MmioRead32 (Entry->BaseAddr + P2U_CONTROL_CMN);
    val |= P2U_CONTROL_CMN_SKP_SIZE_PROTECTION_EN;
    MmioWrite32 (Entry->BaseAddr + P2U_CONTROL_CMN, val);
  }

  val  = MmioRead32 (Entry->BaseAddr + P2U_PERIODIC_EQ_CTRL_GEN3);
  val &= ~P2U_PERIODIC_EQ_CTRL_GEN3_PERIODIC_EQ_EN;
  val |= P2U_PERIODIC_EQ_CTRL_GEN3_INIT_PRESET_EQ_TRAIN_EN;
  MmioWrite32 (Entry->BaseAddr + P2U_PERIODIC_EQ_CTRL_GEN3, val);

  val  = MmioRead32 (Entry->BaseAddr + P2U_PERIODIC_EQ_CTRL_GEN4);
  val |= P2U_PERIODIC_EQ_CTRL_GEN4_INIT_PRESET_EQ_TRAIN_EN;
  MmioWrite32 (Entry->BaseAddr + P2U_PERIODIC_EQ_CTRL_GEN4, val);

  val  = MmioRead32 (Entry->BaseAddr + P2U_RX_DEBOUNCE_TIME);
  val &= ~P2U_RX_DEBOUNCE_TIME_DEBOUNCE_TIMER_MASK;
  val |= P2U_RX_DEBOUNCE_TIME_DEBOUNCE_TIMER_VAL;
  MmioWrite32 (Entry->BaseAddr + P2U_RX_DEBOUNCE_TIME, val);

  if (ChipID == T234_CHIP_ID ) {
    val  = MmioRead32 (Entry->BaseAddr + P2U_DIR_SEARCH_CTRL);
    val &= ~P2U_DIR_SEARCH_CTRL_GEN4_FINE_GRAIN_SEARCH_TWICE;
    MmioWrite32 (Entry->BaseAddr + P2U_DIR_SEARCH_CTRL, val);
  }

  return EFI_SUCCESS;
}

/**
 * Adds all P2U entries in device tree to list
 *
 * @param[in] Private  - Pointer to module private data
 *
 * @return EFI_SUCCESS - Nodes added
 * @return others      - Failed to add nodes
 */
STATIC
EFI_STATUS
AddP2UEntries (
  IN TEGRAP2U_DXE_PRIVATE  *Private
  )
{
  INT32  NodeOffset = -1;
  UINTN  ChipID;

  if (NULL == Private) {
    return EFI_INVALID_PARAMETER;
  }

  ChipID = TegraGetChipID ();

  do {
    TEGRAP2U_LIST_ENTRY  *ListEntry = NULL;
    INT32                AddressCells;
    INT32                PropertySize;
    CONST VOID           *RegProperty = NULL;
    EFI_STATUS           Status;

    /*
     * Since all the P2U entries have the same compatibility string,
     * we attempt to find all of them and create a list.
     */
    if (ChipID == T194_CHIP_ID) {
      NodeOffset = fdt_node_offset_by_compatible (
                     Private->DeviceTreeBase,
                     NodeOffset,
                     "nvidia,tegra194-p2u"
                     );
      if (NodeOffset <= 0) {
        break;
      }
    } else if (ChipID == T234_CHIP_ID) {
      NodeOffset = fdt_node_offset_by_compatible (
                     Private->DeviceTreeBase,
                     NodeOffset,
                     "nvidia,tegra234-p2u"
                     );
      if (NodeOffset <= 0) {
        break;
      }
    }

    ListEntry = AllocateZeroPool (sizeof (TEGRAP2U_LIST_ENTRY));
    if (NULL == ListEntry) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate list entry\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    InitializeListHead (&ListEntry->NotifyList);
    ListEntry->Signature = TEGRAP2U_LIST_SIGNATURE;
    InsertTailList (&Private->TegraP2UList, &ListEntry->Link);
    Private->TegraP2Us++;
    ListEntry->P2UId = fdt_get_phandle (Private->DeviceTreeBase, NodeOffset);

    AddressCells = fdt_address_cells (Private->DeviceTreeBase, fdt_parent_offset (Private->DeviceTreeBase, NodeOffset));
    if ((AddressCells > 2) || (AddressCells == 0)) {
      DEBUG ((EFI_D_ERROR, "%a: Bad cell value, %d\r\n", __FUNCTION__, AddressCells));
      return EFI_UNSUPPORTED;
    }

    RegProperty = fdt_getprop (
                    Private->DeviceTreeBase,
                    NodeOffset,
                    "reg",
                    &PropertySize
                    );
    if (RegProperty == NULL) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to find \"reg\" entry\r\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    if (PropertySize < sizeof (UINT32) * AddressCells) {
      DEBUG ((EFI_D_ERROR, "%a: Wrongly formatted \"reg\" entry\r\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    if (AddressCells == 2) {
      ListEntry->BaseAddr = fdt64_to_cpu (*((UINT64 *)RegProperty));
    } else {
      ListEntry->BaseAddr = fdt32_to_cpu (*((UINT32 *)RegProperty));
    }

    DEBUG ((
      EFI_D_VERBOSE,
      "%a: P2U Base Addr = 0x%llX\r\n",
      __FUNCTION__,
      ListEntry->BaseAddr
      ));

    Status = AddMemoryRegion (ListEntry->BaseAddr, SIZE_64KB);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        EFI_D_ERROR,
        "%a: Failed to add region 0x%016lx, 0x%016lx: %r.\r\n",
        __FUNCTION__,
        ListEntry->BaseAddr,
        (unsigned long)SIZE_64KB,
        Status
        ));
      return EFI_DEVICE_ERROR;
    }

    if (NULL != fdt_get_property (
                  Private->DeviceTreeBase,
                  NodeOffset,
                  "nvidia,skip-sz-protect-en",
                  NULL
                  ))
    {
      ListEntry->SkipSizeProtectionEn = TRUE;
    } else {
      ListEntry->SkipSizeProtectionEn = FALSE;
    }
  } while (1);

  return EFI_SUCCESS;
}

/**
 * Adds all P2Us in device tree to list
 *
 * @param[in] Private  - Pointer to module private data
 *
 * @return EFI_SUCCESS - Nodes added
 * @return others      - Failed to add nodes
 */
STATIC
EFI_STATUS
BuildP2UNodes (
  IN TEGRAP2U_DXE_PRIVATE  *Private
  )
{
  EFI_STATUS  Status;

  if (NULL == Private) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DtPlatformLoadDtb (&Private->DeviceTreeBase, &Private->DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a failed to get device tree: %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = AddP2UEntries (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a failed to add P2U entries: %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  return Status;
}

/**
  Initialize the TegraP2U Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
TegraP2UDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS            Status;
  TEGRAP2U_DXE_PRIVATE  *Private = NULL;

  Private = AllocatePool (sizeof (TEGRAP2U_DXE_PRIVATE));
  if (NULL == Private) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate private data stucture\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature             = TEGRAP2U_SIGNATURE;
  Private->TegraP2UProtocol.Init = TegraP2UInit;
  InitializeListHead (&Private->TegraP2UList);
  Private->TegraP2Us   = 0;
  Private->ImageHandle = ImageHandle;

  /*
   * Tegra194 has a P2U instance for each UPHY lane that a PCIe controller can
   * use. When PCIe XBAR is configured for a platform, it is also fixed as to
   * which PCIe controller would be enabled with what link width and what are
   * the different UPHY lanes that it is supposed to use (and in turn
   * the P2U instances).
   * Here we attempt to build a list of all P2U nodes available in the DT along
   * with their respective information like base address, phandle etc...
   * When a PCIe controller is discovered by UEFI, PCIe host controller driver
   * would come to know of the P2U instances to be used for that respective
   * controller from the respective PCIe controller's DT node, and invoke P2U
   * driver protocol to call Init() of the each P2U instance.
   */
  Status = BuildP2UNodes (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to parse P2U instances data: %r\r\n", __FUNCTION__, Status));
    goto ErrorBuildP2UNodes;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIATegraP2UProtocolGuid,
                  &Private->TegraP2UProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to install protocols: %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    while (!IsListEmpty (&Private->TegraP2UList)) {
      TEGRAP2U_LIST_ENTRY  *Entry;
      LIST_ENTRY           *Node = GetFirstNode (&Private->TegraP2UList);
      RemoveEntryList (Node);
      Entry = TEGRAP2U_LIST_FROM_LINK (Node);
      if (NULL != Entry) {
        FreePool (Entry);
      }
    }

    Private->TegraP2Us = 0;

ErrorBuildP2UNodes:
    FreePool (Private);
  }

  return Status;
}
