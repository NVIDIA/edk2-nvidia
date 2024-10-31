/** @file

  DTB update library

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NetLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <libfdt.h>

STATIC UINT8    mMacAddress[NET_ETHER_ADDR_LEN] = { 0 };
STATIC UINT8    mNumMacAddresses                = 0;
STATIC UINT64   mMacValue                       = 0;
STATIC BOOLEAN  mMacInfoInitialized             = FALSE;

STATIC CONST CHAR8  *mMacAddressCompatibility[] = {
  "nvidia,eqos",
  "nvidia,nveqos",
  "nvidia,nvmgbe",
  "nvidia,tegra186-eqos",
  "nvidia,tegra194-eqos",
  "nvidia,tegra234-mgbe",
  "nvidia,tegra264-mgbe",
  "nvidia,tegra264-eqos",
  "snps,dwc-qos-ethernet-4.10",
  NULL
};

/**
 * Update DTB BPMP IPC memory regions, if necessary.
 *
 * @param[in] Dtb                   Pointer to DTB

 * @retval None
**/
STATIC
VOID
EFIAPI
DtbUpdateBpmpIpcRegions (
  VOID  *Dtb
  )
{
  CHAR8                         BpmpPathStr[20];
  CHAR8                         *BpmpPathFormat;
  INT32                         NodeOffset;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  TEGRA_RESOURCE_INFO           *ResourceInfo;
  CONST NVDA_MEMORY_REGION      *BpmpIpcRegions;
  UINT32                        Socket;
  UINT32                        MemoryPhandle;
  UINT32                        MaxSockets;
  CONST VOID                    *Property;
  INT32                         PropertySize = 0;
  INT32                         ParentOffset;
  INT32                         AddressCells;
  INT32                         SizeCells;
  UINT32                        CellIndex;
  INT32                         FdtStatus;
  UINT32                        RegData[4];

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob == NULL) ||
      (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get platform resource hob\n", __FUNCTION__));
    return;
  }

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  ResourceInfo         = PlatformResourceInfo->ResourceInfo;
  BpmpIpcRegions       = ResourceInfo->BpmpIpcRegions;
  if (BpmpIpcRegions == NULL) {
    DEBUG ((DEBUG_INFO, "%a: no BPMP IPC regions\n", __FUNCTION__));
    return;
  }

  MaxSockets = PcdGet32 (PcdTegraMaxSockets);
  for (Socket = 0; Socket < MaxSockets; Socket++) {
    if (!(PlatformResourceInfo->SocketMask & (1UL << Socket))) {
      continue;
    }

    if (Socket == 0) {
      BpmpPathFormat = "/bpmp";
    } else {
      BpmpPathFormat = "/bpmp_s%u";
    }

    if (BpmpIpcRegions[Socket].MemoryLength == 0) {
      DEBUG ((DEBUG_ERROR, "%a: BPMP IPC socket%u size 0\n", __FUNCTION__, Socket));
      continue;
    }

    ASSERT (AsciiStrSize (BpmpPathFormat) < sizeof (BpmpPathStr));
    AsciiSPrint (BpmpPathStr, sizeof (BpmpPathStr), BpmpPathFormat, Socket);
    NodeOffset = fdt_path_offset (Dtb, BpmpPathStr);
    if (NodeOffset < 0) {
      DEBUG ((DEBUG_ERROR, "%a: socket%u bpmp node missing\n", __FUNCTION__, Socket));
      continue;
    }

    Property = fdt_getprop (Dtb, NodeOffset, "status", &PropertySize);
    if ((Property != NULL) && (AsciiStrCmp (Property, "okay") != 0)) {
      DEBUG ((DEBUG_ERROR, "%a: socket%u bpmp node disabled\n", __FUNCTION__, Socket));
      continue;
    }

    Property = fdt_getprop (Dtb, NodeOffset, "memory-region", &PropertySize);
    if ((Property == NULL) || (PropertySize != sizeof (UINT32))) {
      DEBUG ((DEBUG_ERROR, "%a: socket%u bad bpmp memory-region %p %d\n", __FUNCTION__, Socket, Property, PropertySize));
      continue;
    }

    MemoryPhandle = fdt32_to_cpu (*(CONST UINT32 *)Property);
    DEBUG ((DEBUG_INFO, "%a: socket%u memory-region phandle = 0x%x\n", __FUNCTION__, Socket, MemoryPhandle));

    NodeOffset = fdt_node_offset_by_phandle (Dtb, MemoryPhandle);
    if (NodeOffset < 0) {
      DEBUG ((DEBUG_INFO, "%a: socket%u err=%d finding phandle=0x%x\n", __FUNCTION__, Socket, NodeOffset, MemoryPhandle));
      continue;
    }

    ParentOffset = fdt_parent_offset (Dtb, NodeOffset);
    if (ParentOffset < 0) {
      DEBUG ((DEBUG_INFO, "%a: socket%u err=%d finding phandle=0x%x parent\n", __FUNCTION__, Socket, ParentOffset, MemoryPhandle));
      continue;
    }

    AddressCells = fdt_address_cells (Dtb, ParentOffset);
    SizeCells    = fdt_size_cells (Dtb, ParentOffset);
    if ((SizeCells <= 0) || (AddressCells <= 0) || (SizeCells > 2) || (AddressCells > 2)) {
      DEBUG ((DEBUG_INFO, "%a: socket%u phandle=0x%x parent error addr=%d, size=%d\n", __FUNCTION__, Socket, MemoryPhandle, AddressCells, SizeCells));
      continue;
    }

    CellIndex = 0;
    if (AddressCells == 2) {
      RegData[CellIndex++] = cpu_to_fdt32 (BpmpIpcRegions[Socket].MemoryBaseAddress >> 32);
    }

    RegData[CellIndex++] = cpu_to_fdt32 (BpmpIpcRegions[Socket].MemoryBaseAddress);

    if (SizeCells == 2) {
      RegData[CellIndex++] = cpu_to_fdt32 (BpmpIpcRegions[Socket].MemoryLength >> 32);
    }

    RegData[CellIndex++] = cpu_to_fdt32 (BpmpIpcRegions[Socket].MemoryLength);

    FdtStatus = fdt_setprop (Dtb, NodeOffset, "reg", RegData, CellIndex * sizeof (UINT32));
    if (FdtStatus != 0) {
      DEBUG ((DEBUG_ERROR, "%a: socket%u phandle=0x%x error=%d setting reg\n", __FUNCTION__, Socket, MemoryPhandle, FdtStatus));
      continue;
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: socket%u updated bpmp-shmem phandle=0x%x cells=%u 0x%llx 0x%llx\n",
      __FUNCTION__,
      Socket,
      MemoryPhandle,
      CellIndex,
      BpmpIpcRegions[Socket].MemoryBaseAddress,
      BpmpIpcRegions[Socket].MemoryLength
      ));
  }
}

/**
  Get MAC address string from value

  @param[in]  MacValue              MAC address value

  @retval CHAR8 *                   MAC address string

**/
STATIC
CHAR8 *
DtbUpdateGetMacString (
  UINT64  MacValue
  )
{
  STATIC CHAR8  MacString[18];
  UINT8         *MacBytes;

  MacBytes = (UINT8 *)&MacValue;

  AsciiSPrint (
    MacString,
    sizeof (MacString),
    "%02x:%02x:%02x:%02x:%02x:%02x",
    MacBytes[5],
    MacBytes[4],
    MacBytes[3],
    MacBytes[2],
    MacBytes[1],
    MacBytes[0]
    );

  return MacString;
}

/**
  Get MAC address info from board info

  @retval none

**/
STATIC
VOID
EFIAPI
DtbUpdateGetMacAddressInfo (
  VOID
  )
{
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *ResourceInfo = NULL;
  TEGRA_BOARD_INFO              *BoardInfo;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    ResourceInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob));
  }

  if (ResourceInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no resource info, hob=0x%p\n", __FUNCTION__, Hob));
    return;
  }

  BoardInfo = ResourceInfo->BoardInfo;

  mMacValue        = 0;
  mNumMacAddresses = BoardInfo->NumMacs;
  CopyMem (mMacAddress, BoardInfo->MacAddr, sizeof (mMacAddress));
  CopyMem (&mMacValue, mMacAddress, NET_ETHER_ADDR_LEN);

  DEBUG ((DEBUG_INFO, "%a: mac=%02x:%02x:%02x:%02x:%02x:%02x, num=%u\n", __FUNCTION__, mMacAddress[5], mMacAddress[4], mMacAddress[3], mMacAddress[2], mMacAddress[1], mMacAddress[0], mNumMacAddresses));

  if ((mMacValue == 0) || (mMacValue == 0xffffffffffff)) {
    DEBUG ((DEBUG_ERROR, "%a: invalid MAC info num=%u addr=0x%llx\n", __FUNCTION__, mNumMacAddresses, mMacValue));
    return;
  }

  mMacInfoInitialized = TRUE;
}

/**
  Get big endian MAC address

  @param[in]  Mac               MAC address in CPU byte order

  @retval UINT64                MAC address in big endian order

**/
STATIC
UINT64
DtbUpdateMacToBEValue (
  UINT64  Mac
  )
{
  return cpu_to_fdt64 (Mac) >> 16;
}

/**
  Update MAC address in ethernet node

  @param[in] NodeOffset            Offset of node to update

  @retval None
**/
STATIC
VOID
EFIAPI
DtbUpdateNodeMacAddress (
  INT32  NodeOffset
  )
{
  UINT64      MacFdt;
  EFI_STATUS  Status;
  UINT32      MacIndex;

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "nvidia,mac-addr-idx", &MacIndex);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: getting mac-addr-idx (%a) failed, using base: %r\n", __FUNCTION__, DeviceTreeGetNodeName (NodeOffset), Status));
    MacIndex = 0;
  }

  MacFdt = DtbUpdateMacToBEValue (mMacValue + MacIndex);
  DEBUG ((DEBUG_INFO, "%a: mac=0x%llx index=%u fdt=0x%llx\n", __FUNCTION__, mMacValue, MacIndex, MacFdt));

  Status = DeviceTreeSetNodeProperty (NodeOffset, "mac-address", &MacFdt, NET_ETHER_ADDR_LEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: error setting mac-address=0x%llx\n", __FUNCTION__, MacFdt));
  }
}

/**
  Update all ethernet node MAC addresses

  @retval None

**/
STATIC
VOID
EFIAPI
DtbUpdateAllNodeMacAddresses (
  VOID
  )
{
  INT32  NodeOffset;

  if (!mMacInfoInitialized) {
    DEBUG ((DEBUG_ERROR, "%a: no MAC info\n", __FUNCTION__));
    return;
  }

  NodeOffset = -1;
  while (EFI_SUCCESS == DeviceTreeGetNextCompatibleNode (mMacAddressCompatibility, &NodeOffset)) {
    DEBUG ((DEBUG_INFO, "%a: updating %a\n", __FUNCTION__, DeviceTreeGetNodeName (NodeOffset)));

    DtbUpdateNodeMacAddress (NodeOffset);
  }
}

/**
  Update chosen node with MAC addresses

  @retval None

**/
STATIC
VOID
EFIAPI
DtbUpdateChosenNodeMacAddresses (
  VOID
  )
{
  CHAR8       Buffer[32];
  CHAR8       *MacString;
  UINTN       ChipID;
  UINT32      Count;
  EFI_STATUS  Status;
  INT32       NodeOffset;
  UINT64      MacValue;

  if (!mMacInfoInitialized) {
    DEBUG ((DEBUG_ERROR, "%a: no MAC info\n", __FUNCTION__));
    return;
  }

  Status = DeviceTreeGetNodeByPath ("/chosen", &NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: No chosen node, unable to add MACs: %r\n", __FUNCTION__, Status));
    return;
  }

  MacString = DtbUpdateGetMacString (mMacValue);
  Status    = DeviceTreeSetNodeProperty (NodeOffset, "nvidia,ether-mac", MacString, AsciiStrSize (MacString));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to set chosen MAC address to %a: %r\n", __FUNCTION__, MacString, Status));
  }

  ChipID = TegraGetChipID ();
  if (mNumMacAddresses == 0) {
    DEBUG ((DEBUG_ERROR, "%a: mNumMacAddresses is 0\n", __FUNCTION__));
  }

  MacValue = mMacValue;
  for (Count = 0; Count < mNumMacAddresses; Count++, MacValue++) {
    AsciiSPrint (Buffer, sizeof (Buffer), "nvidia,ether-mac%u", Count);

    MacString = DtbUpdateGetMacString (MacValue);

    DEBUG ((DEBUG_INFO, "%a: setting %a to %a (%llx)\n", __FUNCTION__, Buffer, MacString, MacValue));

    Status = DeviceTreeSetNodeProperty (NodeOffset, Buffer, MacString, AsciiStrSize (MacString));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: error setting %a to %a (%llx)\n", __FUNCTION__, Buffer, MacString, MacValue));
    }
  }
}

VOID
EFIAPI
DtbUpdateForUefi (
  VOID  *Dtb
  )
{
  SetDeviceTreePointer (Dtb, fdt_totalsize (Dtb));

  DtbUpdateBpmpIpcRegions (Dtb);
  DtbUpdateGetMacAddressInfo ();
  DtbUpdateAllNodeMacAddresses ();
}

VOID
EFIAPI
DtbUpdateForKernel (
  VOID  *Dtb
  )
{
  // perform same updates as UEFI
  DtbUpdateForUefi (Dtb);

  // perform kernel-specific updates
  DtbUpdateChosenNodeMacAddresses ();
}
