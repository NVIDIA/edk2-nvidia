/** @file
FDT Boot code for the Standalone MM Foundation.
Placeholder to consume the DTB and setup the HobList.

SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
Copyright (c) 2017 - 2021, Arm Ltd. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>
#include <PiPei.h>
#include <Base.h>
#include <Library/ArmStandaloneMmCoreEntryPoint.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/StandaloneMmMmuLib.h>
#include <Library/FdtLib.h>
#include <Guid/MmramMemoryReserve.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Library/ArmSvcLib.h>

typedef enum {
  ArmStandaloneMmBootInvalid,
  ArmStandaloneMmBootColdBoot,
  ArmStandaloneMmBootRcm,
  ArmStandaloneMmBootBootwrapperBoot,
  ArmStandaloneMmBootTypeMax,
} ARM_STANDALONE_MM_BOOT_TYPE;

#include "FdtBoot.h"
#include "Base.h"
#include "Uefi/UefiBaseType.h"

#define  DEVICE_REGION_NAME_MAX_LEN  (32)
#define  SCRATCH_SECURE_RAM_L2_70_0  (0x118)
#define  SOCKET_0_BIT_POS            (30)
#define  SOCKET_1_BIT_POS            (31)
#define  MAX_SOCKETS                 (2)

#define  BU_HACKS  (FALSE)

extern EFI_HOB_HANDOFF_INFO_TABLE *
HobConstructor (
  IN VOID   *EfiMemoryBegin,
  IN UINTN  EfiMemoryLength,
  IN VOID   *EfiFreeMemoryBottom,
  IN VOID   *EfiFreeMemoryTop
  );

typedef struct _EFI_MM_DEVICE_REGION {
  EFI_VIRTUAL_ADDRESS    DeviceRegionStart;
  UINT32                 DeviceRegionSize;
  CHAR8                  DeviceRegionName[DEVICE_REGION_NAME_MAX_LEN];
} EFI_MM_DEVICE_REGION;

typedef struct {
  BOOLEAN                        IsFbc;
  ARM_STANDALONE_MM_BOOT_TYPE    BootType;
  UINT32                         SocketMask;
  UINT64                         ErstBase;
  UINT64                         ErstSize;
} STANDALONE_MM_PLATFORM_INFO;

STATIC UINT32  SocketMask = 0;

/**
 * GetDeviceSocketNum
 * Util function to get the socket number from the device region name.
 *
 * @param[in] DeviceRegionName Name of the device region.
 *
 * @retval Socket number.
 */
STATIC
EFIAPI
UINT32
GetDeviceSocketNum (
  CONST CHAR8  *DeviceRegionName
  )
{
  CHAR8   *SockStr;
  UINT32  SockNum;

  SockStr = AsciiStrStr (DeviceRegionName, "-socket");
  if (SockStr != NULL) {
    if (SockStr + AsciiStrLen ("-socket") > DeviceRegionName + AsciiStrLen (DeviceRegionName)) {
      DEBUG ((DEBUG_INFO, "%a: Invalid socket name: %a\r\n", __FUNCTION__, DeviceRegionName));
      SockNum = 0;
      goto ExitGetDeviceSocketNum;
    }

    SockNum = AsciiStrDecimalToUintn ((SockStr + AsciiStrLen ("-socket")));
    if (SockNum >= MAX_SOCKETS) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: SockNum %u is out of range , max(%d)\n",
        __FUNCTION__,
        SockNum,
        MAX_SOCKETS
        ));
    }
  } else {
    SockNum = 0;
  }

ExitGetDeviceSocketNum:
  return SockNum;
}

/*
 * Quick sanity check of the partition manifest.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 */
STATIC
EFI_STATUS
CheckManifest (
  IN VOID  *DtbAddress
  )
{
  INT32  HeaderCheck  = -1;
  INT32  ParentOffset = 0;

  /* Check integrity of DTB */
  HeaderCheck = FdtCheckHeader ((VOID *)DtbAddress);
  if (HeaderCheck != 0) {
    DEBUG ((DEBUG_ERROR, "fdt_check_header failed, err=%d\r\n", HeaderCheck));
    return EFI_DEVICE_ERROR;
  }

  ParentOffset = FdtPathOffset (DtbAddress, "/");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find root node\r\n"));
    return EFI_DEVICE_ERROR;
  }

  ParentOffset = FdtPathOffset (DtbAddress, "/memory-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /memory-regions node\r\n"));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/*
 * Helper function get a 32-bit property from the Manifest and accessing it in a way
 * that won't cause alignment issues if running with MMU disabled.
 */
STATIC
UINT32
FDTGetProperty32 (
  VOID        *DtbAddress,
  INT32       NodeOffset,
  CONST VOID  *PropertyName,
  BOOLEAN     Mandatory
  )
{
  CONST FDT_PROPERTY  *Property;
  INT32               Length;
  UINT32              P32;

  Property = FdtGetProperty (DtbAddress, NodeOffset, PropertyName, &Length);

  if (Property == NULL) {
    if (Mandatory == TRUE) {
      ASSERT (Property != NULL);
      DEBUG ((DEBUG_ERROR, "%a: Failed to get property %a\n", __FUNCTION__, PropertyName));
      return 0;
    } else {
      return 0;
    }
  }

  ASSERT (Length == sizeof (UINT32));

  P32 = Fdt32ToCpu (ReadUnaligned32 ((UINT32 *)Property->Data));

  return P32;
}

/*
 * Helper function get a 64-bit property from the Manifest and accessing it in a way
 * that won't cause alignment issues if running with MMU disabled.
 */
STATIC
UINT64
FDTGetProperty64 (
  VOID        *DtbAddress,
  INT32       NodeOffset,
  CONST VOID  *PropertyName,
  BOOLEAN     Mandatory
  )
{
  CONST FDT_PROPERTY  *Property;
  INT32               Length;
  UINT64              P64;

  Property = FdtGetProperty (DtbAddress, NodeOffset, PropertyName, &Length);

  if (Property == NULL) {
    if (Mandatory == TRUE) {
      ASSERT (Property != NULL);
      DEBUG ((DEBUG_ERROR, "%a: Failed to get property %a\n", __FUNCTION__, PropertyName));
      return 0;
    } else {
      return 0;
    }
  }

  ASSERT (Length == sizeof (UINT64));
  P64 = Fdt64ToCpu (ReadUnaligned64 ((UINT64 *)Property->Data));

  return P64;
}

/*
 * Compute the size of the heap.
 * Compute the heap size by parsing through the memory regions and removing
 * regions that may be set aside for other purposes.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 * @param  [in] HeapBase             Base address of the heap.
 * @param  [in, out] HeapLimit        Limit address of the heap.
 * @retval EFI_SUCCESS              Successfully computed the size of the heap.
 */
STATIC
EFI_STATUS
ComputeHeapSize (
  IN     VOID    *DtbAddress,
  IN     UINT64  HeapBase,
  IN OUT UINT64  *HeapLimit
  )
{
  INT32   ParentOffset  = 0;
  INT32   NodeOffset    = 0;
  UINT64  RegionAddress = 0;
  UINT64  RegionSize    = 0;
  UINT64  LowestRegion  = 0;
  UINT64  HighestRegion = 0;
  UINT64  PagesCount    = 0;

  ParentOffset = FdtPathOffset (DtbAddress, "/memory-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /memory-regions node\r\n"));
    ASSERT (FALSE);
    return EFI_DEVICE_ERROR;
  }

  LowestRegion  =  *HeapLimit;
  HighestRegion =  HeapBase;

  for (NodeOffset = FdtFirstSubnode (DtbAddress, ParentOffset);
       NodeOffset >= 0;
       NodeOffset = FdtNextSubnode (DtbAddress, NodeOffset))
  {
    RegionAddress = FDTGetProperty64 (DtbAddress, NodeOffset, "base-address", TRUE);
    PagesCount    = (UINT64)FDTGetProperty32 (DtbAddress, NodeOffset, "pages-count", TRUE);
    if (PagesCount > (MAX_UINT64/EFI_PAGE_SIZE)) {
      DEBUG ((DEBUG_ERROR, "%a: PagesCount %lu is too large Possible DTB corruption\r\n", __FUNCTION__, PagesCount));
      return EFI_DEVICE_ERROR;
    }

    RegionSize = PagesCount * EFI_PAGE_SIZE;
    if ((RegionAddress >= HeapBase) && (RegionAddress + RegionSize <= *HeapLimit)) {
      DEBUG ((DEBUG_ERROR, "%a: Region %lx-%lx is in the heap\r\n", __FUNCTION__, RegionAddress, RegionAddress + RegionSize));
      LowestRegion = MIN (LowestRegion, RegionAddress);
      DEBUG ((DEBUG_ERROR, "%a: LowestRegion: %lx Limit: %lx\r\n", __FUNCTION__, LowestRegion, *HeapLimit));
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Region %lx-%lx is not in the heap\r\n", __FUNCTION__, RegionAddress, RegionAddress + RegionSize));
      continue;
    }
  }

  *HeapLimit = LowestRegion;

  return EFI_SUCCESS;
}

/*
 * Get the base and size of a region from the manifest.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 * @param  [in] NodeName             Name of the node to get the base and size of.
 * @param  [in, out] RegionBase       Base address of the region.
 * @param  [in, out] RegionSize       Size of the region.
 * @retval EFI_SUCCESS              Successfully got the base and size of the region.
 * @retval EFI_DEVICE_ERROR         Failed to find the node in the manifest.
 */
STATIC
EFI_STATUS
GetRegionInfo (
  IN VOID         *DtbAddress,
  IN CONST CHAR8  *NodeName,
  IN OUT UINTN    *RegionBase,
  IN OUT UINTN    *RegionSize
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  INT32       ParentOffset = 0;
  INT32       NodeOffset   = 0;
  UINT64      PagesCount   = 0;

  ParentOffset = FdtPathOffset (DtbAddress, "/memory-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /memory-regions node\r\n"));
    Status = EFI_DEVICE_ERROR;
    goto ExitGetRegionInfo;
  }

  NodeOffset = FdtSubnodeOffsetNameLen (DtbAddress, ParentOffset, NodeName, AsciiStrLen (NodeName));
  if (NodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find %s node\r\n", NodeName));
    Status = EFI_DEVICE_ERROR;
    goto ExitGetRegionInfo;
  }

  *RegionBase = FDTGetProperty64 (DtbAddress, NodeOffset, "base-address", TRUE);
  PagesCount  = (UINT64)FDTGetProperty32 (DtbAddress, NodeOffset, "pages-count", TRUE);
  if ((PagesCount > (MAX_UINT64/EFI_PAGE_SIZE)) || (PagesCount == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: PagesCount %lu is out of range, max(%lu) or zero\r\n", __FUNCTION__, PagesCount, MAX_UINT64/EFI_PAGE_SIZE));
    Status = EFI_DEVICE_ERROR;
    goto ExitGetRegionInfo;
  }

  *RegionSize = EFI_PAGES_TO_SIZE (PagesCount);

ExitGetRegionInfo:
  return Status;
}

/*
 * From the manifest load-address and entrypoint-offset, find the base address of the SP code.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 */
UINT64
GetSpImageBase (
  IN VOID  *DtbAddress
  )
{
  INT32   ParentOffset = 0;
  UINT64  SpImageBase  = 0;

  ParentOffset = FdtPathOffset (DtbAddress, "/");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find root node\r\n", __FUNCTION__));
    ASSERT (FALSE);
    return 0;
  }

  SpImageBase = FDTGetProperty64 (DtbAddress, ParentOffset, "load-address", TRUE) +
                FDTGetProperty32 (DtbAddress, ParentOffset, "entrypoint-offset", TRUE);

  return SpImageBase;
}

/*
 * Set the permissions of the heap.
 *
 * Mark the heap as Data RW and Code XN. When the SP is booted the entire region
 * is setup as Data RO and Code X.
 * Ideally Hafnium will set the heap permissions once HOB support is added.
 * For now we will set the heap permissions here, don't use the ArmMmuLib to
 * set the heap permissions as it does it one page at a time, so make the FF-A
 * request directly and pass the entire heap size.

 * @param  [in] SpHeapBase           Base address of the heap.
 * @param  [in] SpHeapSize           Size of the heap.

 * @retval EFI_SUCCESS              Successfully set the permissions of the heap.
 * @retval OTHER                    Failed to set the permissions of the heap.
 */
STATIC
EFI_STATUS
SetHeapPermissions (
  IN UINT64  SpHeapBase,
  IN UINT64  SpHeapSize
  )
{
  ARM_SVC_ARGS  EventSvcArgs;

  EventSvcArgs.Arg0 = ARM_FID_FFA_MEM_PERM_SET;
  EventSvcArgs.Arg1 = (UINTN)SpHeapBase;
  EventSvcArgs.Arg2 = (UINTN)EFI_SIZE_TO_PAGES (SpHeapSize);
  EventSvcArgs.Arg3 =   ARM_FFA_SET_MEM_ATTR_MAKE_PERM_REQUEST (
                          ARM_FFA_SET_MEM_ATTR_DATA_PERM_RW,
                          ARM_FFA_SET_MEM_ATTR_CODE_PERM_XN
                          );

  ArmCallSvc (&EventSvcArgs);

  return FfaStatusToEfiStatus (EventSvcArgs.Arg2);
}

/**
 * Check if socket is enabled in the CPU BL Params's socket mask.
 * This API is usually only called from StMM.
 *
 * @param[in] CpuBlAddress          Address of the CPU BL params.
 * @param[in] SocketNum             Socket to check.
 *
 * @retval  TRUE                    Socket is enabled.
 * @retval  FALSE                   Socket is not enabled.
**/
BOOLEAN
EFIAPI
IsSocketEnabled (
  IN UINT32  SocketNum
  )
{
  BOOLEAN  SocketEnabled;

  SocketEnabled = ((SocketMask & (1U << SocketNum)) ? TRUE : FALSE);
  return SocketEnabled;
}

/*
 * SkipDeviceNode
 *   Util function that tells if a device node shouldn't be added to the
 *   Device Region Hob.
 *
 * @param[in] DevRegion  Name of the device region. The expectation is that the
 *                       manifest will name socket specific regions with the
 *                       -socketX suffix (e.g qspi-socket0)
 *
 * @retval    TRUE      Skip adding this region as the socket it belongs to
 *                      is disabled.
 *            FALSE     Add this region to the device region GUID'd HOB.
 */
STATIC
BOOLEAN
SkipDeviceNode (
  IN CONST CHAR8  *DevRegion
  )
{
  BOOLEAN  SkipNode;
  CHAR8    *SockStr;
  UINT32   SockNum;

  SkipNode = FALSE;
  SockStr  = AsciiStrStr (DevRegion, "-socket");
  if (SockStr != NULL) {
    SockNum = GetDeviceSocketNum (DevRegion);
    /* If socket is disabled then don't add this MMIO region */
    if (IsSocketEnabled (SockNum) == FALSE) {
      SkipNode = TRUE;
    }
  }

  return SkipNode;
}

/*
 * Build a HOB list of the device regions (gEfiStandaloneMmDeviceMemoryRegions)
 * by parsing the manifest.
 * This HOB isn't a standard upstream defined HOB, but is used by downstream drivers.
 *
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 * @retval EFI_SUCCESS              Successfully built the HOB list.
 * @retval EFI_NOT_FOUND            Failed to find the /device-regions node.
 * @retval EFI_OUT_OF_RESOURCES     Failed to allocate memory for the HOB list.
 * @retval EFI_DEVICE_ERROR         Failed to get the property from the manifest.
 */
STATIC
EFI_STATUS
BuildDeviceMemRegionsHob (
  IN VOID  *DtbAddress
  )
{
  EFI_STATUS            Status       = EFI_SUCCESS;
  INT32                 ParentOffset = 0;
  INT32                 NodeOffset   = 0;
  UINTN                 NumRegions   = 0;
  EFI_MM_DEVICE_REGION  *DeviceRegions;
  CONST CHAR8           *DeviceRegionName      = NULL;
  INT32                 DeviceRegionNameLength = 0;
  UINTN                 DeviceIndex            = 0;
  UINT64                PagesCount             = 0;

  ParentOffset = FdtPathOffset (DtbAddress, "/device-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /device-regions node\r\n"));
    Status = EFI_NOT_FOUND;
    goto ExitBuildDeviceMemRegionsHob;
  }

  for (NodeOffset = FdtFirstSubnode (DtbAddress, ParentOffset);
       NodeOffset >= 0;
       NodeOffset = FdtNextSubnode (DtbAddress, NodeOffset))
  {
    if (SkipDeviceNode (FdtGetName (DtbAddress, NodeOffset, &DeviceRegionNameLength)) == FALSE) {
      NumRegions++;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: NumRegions: %d\n", __FUNCTION__, NumRegions));
  DeviceRegions = BuildGuidHob (&gArmStandaloneMmDeviceMemoryRegions, sizeof (EFI_MM_DEVICE_REGION) * NumRegions);
  if (DeviceRegions == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to build DeviceRegions HOB\r\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitBuildDeviceMemRegionsHob;
  }

  ZeroMem (DeviceRegions, sizeof (EFI_MM_DEVICE_REGION) * NumRegions);

  for (NodeOffset = FdtFirstSubnode (DtbAddress, ParentOffset);
       NodeOffset >= 0;
       NodeOffset = FdtNextSubnode (DtbAddress, NodeOffset))
  {
    if (SkipDeviceNode (FdtGetName (DtbAddress, NodeOffset, &DeviceRegionNameLength)) == FALSE) {
      DeviceRegions[DeviceIndex].DeviceRegionStart = FDTGetProperty64 (DtbAddress, NodeOffset, "base-address", TRUE);
      PagesCount                                   = (UINT64)FDTGetProperty32 (DtbAddress, NodeOffset, "pages-count", TRUE);
      if ((PagesCount > (MAX_UINT64/EFI_PAGE_SIZE)) || (PagesCount == 0)) {
        DEBUG ((DEBUG_ERROR, "%a: PagesCount %lu is out of range, max(%lu) or zero\r\n", __FUNCTION__, PagesCount, MAX_UINT64/EFI_PAGE_SIZE));
        Status = EFI_DEVICE_ERROR;
        goto ExitBuildDeviceMemRegionsHob;
      }

      DeviceRegions[DeviceIndex].DeviceRegionSize = PagesCount * EFI_PAGE_SIZE;
      DeviceRegionName                            = FdtGetName (DtbAddress, NodeOffset, &DeviceRegionNameLength);
      DEBUG ((DEBUG_INFO, "%a: DeviceRegionName: %a Length %u Start 0x%lx Size %u\n", __FUNCTION__, DeviceRegionName, DeviceRegionNameLength, DeviceRegions[DeviceIndex].DeviceRegionStart, DeviceRegions[DeviceIndex].DeviceRegionSize));
      if (DeviceRegionNameLength >= DEVICE_REGION_NAME_MAX_LEN) {
        DEBUG ((DEBUG_ERROR, "DeviceRegionNameLength is too long\r\n"));
        Status = EFI_DEVICE_ERROR;
        goto ExitBuildDeviceMemRegionsHob;
      }

      CopyMem (DeviceRegions[DeviceIndex].DeviceRegionName, DeviceRegionName, DeviceRegionNameLength);
      DeviceRegions[DeviceIndex].DeviceRegionName[DeviceRegionNameLength] = '\0';
      DEBUG ((DEBUG_INFO, "%a: [%d] DeviceRegionName: %a Start 0x%lx Size %u\n", __FUNCTION__, DeviceIndex, DeviceRegions[DeviceIndex].DeviceRegionName, DeviceRegions[DeviceIndex].DeviceRegionStart, DeviceRegions[DeviceIndex].DeviceRegionSize));
      DeviceIndex++;
    }
  }

ExitBuildDeviceMemRegionsHob:
  return Status;
}

/*
 * Build a HOB list of the Normal Shared Buffer.
 *
 * @param  [in] NsCommBufBase        Base address of the Normal Shared Buffer.
 * @param  [in] NsCommBufSize        Size of the Normal Shared Buffer.
 * @retval EFI_SUCCESS              Successfully built the NsCommBuf HOB.
 * @retval EFI_OUT_OF_RESOURCES     Failed to allocate memory for the NsCommBuf HOB.
 */
STATIC
EFI_STATUS
BuildNsCommBufHob (
  IN UINT64  NsCommBufBase,
  IN UINT64  NsCommBufSize
  )
{
  EFI_STATUS            Status = EFI_SUCCESS;
  EFI_MMRAM_DESCRIPTOR  *NsCommBufMmramRange;

  NsCommBufMmramRange = (EFI_MMRAM_DESCRIPTOR *)BuildGuidHob (
                                                  &gEfiStandaloneMmNonSecureBufferGuid,
                                                  sizeof (EFI_MMRAM_DESCRIPTOR)
                                                  );
  if (NsCommBufMmramRange == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to build NsCommBufMmramRange\r\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitBuildNsCommBufHob;
  }

  NsCommBufMmramRange[0].PhysicalStart = NsCommBufBase;
  NsCommBufMmramRange[0].CpuStart      = NsCommBufBase;
  NsCommBufMmramRange[0].PhysicalSize  = NsCommBufSize;
  NsCommBufMmramRange[0].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;

  DEBUG ((
    DEBUG_INFO,
    "NsCommBufMmramRange[0]: %lx, %lx, %lx, %lx\r\n",
    NsCommBufMmramRange[0].PhysicalStart,
    NsCommBufMmramRange[0].CpuStart,
    NsCommBufMmramRange[0].PhysicalSize,
    NsCommBufMmramRange[0].RegionState
    ));

ExitBuildNsCommBufHob:
  return Status;
}

/*
 * Build a HOB list of the mmram ranges.
 *
 * @param  [in] SpImageBase          Base address of the SP image.
 * @param  [in] SpImageSize          Size of the SP image.
 * @param  [in] SpSharedBufBase      Base address of the Secure Shared Buffer.
 * @param  [in] SpSharedBufSize      Size of the Secure Shared Buffer.
 * @param  [in] NsCommBufBase        Base address of the Normal Shared Buffer.
 * @param  [in] NsCommBufSize        Size of the Normal Shared Buffer.
 * @param  [in] HobStart             Base address of the HobList.
 * @retval EFI_SUCCESS              Successfully built the MMRAM Ranges HOB.
 * @retval EFI_OUT_OF_RESOURCES     Failed to allocate memory for the MMRAM Ranges HOB.
 * @retval EFI_DEVICE_ERROR         Failed to get the property from the manifest.
 */
STATIC
EFI_STATUS
BuildMmramRangesHob (
  IN UINT64                      SpImageBase,
  IN UINT64                      SpImageSize,
  IN UINT64                      SpSharedBufBase,
  IN UINT64                      SpSharedBufSize,
  IN UINT64                      NsCommBufBase,
  IN UINT64                      NsCommBufSize,
  IN EFI_HOB_HANDOFF_INFO_TABLE  *HobStart
  )
{
  EFI_STATUS                      Status = EFI_SUCCESS;
  EFI_MMRAM_HOB_DESCRIPTOR_BLOCK  *MmramRangesHob;
  EFI_MMRAM_DESCRIPTOR            *MmramRanges;
  UINTN                           MmramRangesIndex = 0;

  /* Build up 4 MMRAM ranges:
   * 1. Image
   * 2. Secure Shared Buffer
   * 3. Normal Shared Buffer
   * 4. Heap
   */
  MmramRangesHob = BuildGuidHob (
                     &gEfiMmPeiMmramMemoryReserveGuid,
                     sizeof (EFI_MMRAM_HOB_DESCRIPTOR_BLOCK) +
                     sizeof (EFI_MMRAM_DESCRIPTOR) * (MMRAM_DESC_MIN_COUNT + 1)
                     );
  if (MmramRangesHob == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to build MmramRangesHob\r\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitBuildMmramRangesHob;
  }

  MmramRangesHob->NumberOfMmReservedRegions                      = MMRAM_DESC_MIN_COUNT + 1;
  MmramRanges                                                    = &MmramRangesHob->Descriptor[0];
  MmramRanges[MMRAM_DESC_IDX_IMAGE].PhysicalStart                = SpImageBase;
  MmramRanges[MMRAM_DESC_IDX_IMAGE].CpuStart                     = SpImageBase;
  MmramRanges[MMRAM_DESC_IDX_IMAGE].PhysicalSize                 = SpImageSize;
  MmramRanges[MMRAM_DESC_IDX_IMAGE].RegionState                  = EFI_CACHEABLE | EFI_ALLOCATED;
  MmramRanges[MMRAM_DESC_IDX_SECURE_SHARED_BUFFER].PhysicalStart = SpSharedBufBase;
  MmramRanges[MMRAM_DESC_IDX_SECURE_SHARED_BUFFER].CpuStart      = SpSharedBufBase;
  MmramRanges[MMRAM_DESC_IDX_SECURE_SHARED_BUFFER].PhysicalSize  = SpSharedBufSize;
  MmramRanges[MMRAM_DESC_IDX_SECURE_SHARED_BUFFER].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;
  MmramRanges[MMRAM_DESC_IDX_NORMAL_SHARED_BUFFER].PhysicalStart = NsCommBufBase;
  MmramRanges[MMRAM_DESC_IDX_NORMAL_SHARED_BUFFER].CpuStart      = NsCommBufBase;
  MmramRanges[MMRAM_DESC_IDX_NORMAL_SHARED_BUFFER].PhysicalSize  = NsCommBufSize;
  MmramRanges[MMRAM_DESC_IDX_NORMAL_SHARED_BUFFER].RegionState   = EFI_CACHEABLE | EFI_ALLOCATED;
  MmramRanges[MMRAM_DESC_IDX_HEAP].PhysicalStart                 = (EFI_PHYSICAL_ADDRESS)HobStart;
  MmramRanges[MMRAM_DESC_IDX_HEAP].CpuStart                      = (EFI_PHYSICAL_ADDRESS)HobStart;
  MmramRanges[MMRAM_DESC_IDX_HEAP].PhysicalSize                  = HobStart->EfiFreeMemoryBottom - (EFI_PHYSICAL_ADDRESS)HobStart;
  MmramRanges[MMRAM_DESC_IDX_HEAP].RegionState                   = EFI_CACHEABLE | EFI_ALLOCATED;
  MmramRanges[MMRAM_DESC_MIN_COUNT].PhysicalStart                = HobStart->EfiFreeMemoryBottom;
  MmramRanges[MMRAM_DESC_MIN_COUNT].CpuStart                     = HobStart->EfiFreeMemoryBottom;
  MmramRanges[MMRAM_DESC_MIN_COUNT].PhysicalSize                 = HobStart->EfiFreeMemoryTop - HobStart->EfiFreeMemoryBottom;
  MmramRanges[MMRAM_DESC_MIN_COUNT].RegionState                  = EFI_CACHEABLE;

  for (MmramRangesIndex = 0; MmramRangesIndex < MmramRangesHob->NumberOfMmReservedRegions; MmramRangesIndex++) {
    DEBUG ((
      DEBUG_INFO,
      "MmramRanges[%d]: %lx, %lx, %lx, %lx\r\n",
      MmramRangesIndex,
      MmramRanges[MmramRangesIndex].PhysicalStart,
      MmramRanges[MmramRangesIndex].CpuStart,
      MmramRanges[MmramRangesIndex].PhysicalSize,
      MmramRanges[MmramRangesIndex].RegionState
      ));
  }

ExitBuildMmramRangesHob:
  return Status;
}

/*
 * Parse the boot FDT to get the SP memory base, limit, image base, size, and heap base.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 * @param  [in] TotalSPMemorySize    Size of the SP memory.
 * @param  [in, out] SpMemBase       Base address of the SP memory.
 * @param  [in, out] SpMemLimit      Limit address of the SP memory.
 * @param  [in, out] SpImageBase     Base address of the SP image.
 * @param  [in, out] SpImageLimit    Limit address of the SP image.
 * @param  [in, out] SpHeapBase      Base address of the SP heap.
 * @param  [in, out] SpHeapSize      Size of the SP heap.
 * @retval EFI_SUCCESS              Successfully parsed the boot FDT.
 * @retval EFI_DEVICE_ERROR         Failed to get the property from the manifest.
 */
STATIC
EFI_STATUS
ParseBootFdt (
  IN     VOID    *DtbAddress,
  IN     UINT64  TotalSPMemorySize,
  IN OUT UINT64  *SpMemBase,
  IN OUT UINT64  *SpMemLimit,
  IN OUT UINT64  *SpImageBase,
  IN OUT UINT64  *SpImageSize,
  IN OUT UINT64  *SpHeapBase,
  IN OUT UINT64  *SpHeapSize
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  INT32       ParentOffset = 0;
  UINT64      SpImageLimit = 0;

  ParentOffset = FdtPathOffset (DtbAddress, "/");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find root node %a \n", __FUNCTION__, FdtStrerror (ParentOffset)));
    Status = EFI_NOT_FOUND;
    ASSERT_EFI_ERROR (Status);
    goto ExitParseBootFdt;
  }

  *SpMemBase   = FDTGetProperty64 (DtbAddress, ParentOffset, "load-address", TRUE);
  *SpMemLimit  = *SpMemBase + TotalSPMemorySize;
  *SpImageBase = GetSpImageBase (DtbAddress);
  if ((*SpImageBase == 0) || (*SpImageBase < *SpMemBase) || (*SpImageBase > *SpMemLimit)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid SpImageBase %lx < %lx > %lx\n", __FUNCTION__, *SpImageBase, *SpMemBase, *SpMemLimit));
    Status = EFI_NOT_FOUND;
    ASSERT_EFI_ERROR (Status);
    goto ExitParseBootFdt;
  }

  *SpImageSize = ((EFI_FIRMWARE_VOLUME_HEADER *)*SpImageBase)->FvLength;
  *SpImageSize = ALIGN_VALUE (*SpImageSize, SIZE_4KB);
  SpImageLimit = *SpImageBase + *SpImageSize;
  *SpHeapBase  = SpImageLimit + EFI_PAGES_TO_SIZE (FDTGetProperty32 (DtbAddress, ParentOffset, "reserved-pages-count", TRUE));

  Status = ComputeHeapSize (DtbAddress, *SpHeapBase, SpMemLimit);
  ASSERT_EFI_ERROR (Status);
  *SpHeapSize = *SpMemLimit - *SpHeapBase;

ExitParseBootFdt:
  return Status;
}

/*
 * Build a HOB list of the platform resource info.
 * These are platform specific information that StMM needs to know.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 * @retval EFI_SUCCESS              Successfully built the Platform Resource Info HOB.
 * @retval EFI_OUT_OF_RESOURCES     Failed to allocate memory for the Platform Resource Info HOB.
 */
STATIC
EFI_STATUS
BuildPlatformHob (
  IN VOID    *DtbAddress,
  IN UINTN   NsCommBufBase,
  IN UINTN   SpMemBase,
  IN UINT64  TotalSPMemorySize
  )
{
  EFI_STATUS                   Status = EFI_SUCCESS;
  STANDALONE_MM_PLATFORM_INFO  *StandaloneMmPlatformInfo;
  UINT64                       L2ScratchBase;
  UINT32                       L2ScratchSize;
  INT32                        NodeOffset;
  UINT32                       ScratchValue;

  StandaloneMmPlatformInfo = (STANDALONE_MM_PLATFORM_INFO *)BuildGuidHob (&gArmStandaloneMmPlatformInfoGuid, sizeof (STANDALONE_MM_PLATFORM_INFO));
  if (StandaloneMmPlatformInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to build PlatformResourceInfo\r\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitBuildPlatformHob;
  }

  /* TODO: May need to implement this. Hardcode to coldboot for now.
  */
  StandaloneMmPlatformInfo->BootType = ArmStandaloneMmBootColdBoot;

  NodeOffset = FdtPathOffset (DtbAddress, "/device-regions/tegra-scratch_0_secure_l2");
  if (NodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find sratch_0_secure_l2 mmio region %a \n", __FUNCTION__, FdtStrerror (NodeOffset)));
    Status = EFI_NOT_FOUND;
    ASSERT_EFI_ERROR (Status);
    goto ExitBuildPlatformHob;
  } else {
    L2ScratchBase = FDTGetProperty64 (DtbAddress, NodeOffset, "base-address", TRUE);
    L2ScratchSize = (FDTGetProperty32 (DtbAddress, NodeOffset, "pages-count", TRUE)) * EFI_PAGE_SIZE;
    if ((L2ScratchBase == 0) || (L2ScratchSize == 0)) {
      DEBUG ((DEBUG_ERROR, "%a: Invalid L2ScratchBase %lx or L2ScratchSize%lx \n", __FUNCTION__, L2ScratchBase, L2ScratchSize));
      Status = EFI_NOT_FOUND;
      ASSERT_EFI_ERROR (Status);
      goto ExitBuildPlatformHob;
    }
  }

  SocketMask = MmioBitFieldRead32 (
                 (L2ScratchBase + SCRATCH_SECURE_RAM_L2_70_0),
                 SOCKET_0_BIT_POS,
                 SOCKET_1_BIT_POS
                 );
  ScratchValue = MmioRead32 ((L2ScratchBase + SCRATCH_SECURE_RAM_L2_70_0));
  DEBUG ((DEBUG_ERROR, "Read Back Socket Mask 0x%x RegVal 0x%x Address 0x%lx\n", SocketMask, ScratchValue, (L2ScratchBase + SCRATCH_SECURE_RAM_L2_70_0)));

  /* We didn't read a valid socket mask from the scratch registers */
  ASSERT ((SocketMask != 0x0) && (SocketMask != 0x2));
  StandaloneMmPlatformInfo->SocketMask = SocketMask;

  /*
     Hacky way to checkif we're in FullBootChain mode or not, if the
     NS shared buf is fixed up by the bootloader, the shared buffer should
     be outside of the SP memory region
     */
  if ((NsCommBufBase > SpMemBase) && (NsCommBufBase < (SpMemBase + TotalSPMemorySize))) {
    StandaloneMmPlatformInfo->IsFbc = FALSE;
  } else {
    StandaloneMmPlatformInfo->IsFbc = TRUE;
  }

ExitBuildPlatformHob:
  return Status;
}

/**
  Use the boot information passed by privileged firmware to populate a HOB list
  suitable for consumption by the MM Core and drivers.

  @param  [in, out] TotalSPMemorySize     Size of the SP memory.
  @param  [in]      DtbAddress            Address of the partition manifest.
  @retval Address of the HobList.
  @
**/
VOID *
CreateHobListFromBootInfo (
  IN UINT64  TotalSPMemorySize,
  IN VOID    *DTBAddress
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE  *HobStart;
  UINT64                      SpImageBase     = 0;
  UINT64                      SpMemBase       = 0;
  UINT64                      SpMemLimit      = 0;
  UINT64                      SpHeapBase      = 0;
  UINT64                      SpHeapSize      = 0;
  EFI_STATUS                  Status          = EFI_SUCCESS;
  UINTN                       NsCommBufBase   = 0;
  UINTN                       NsCommBufSize   = 0;
  UINTN                       SpSharedBufBase = 0;
  UINTN                       SpSharedBufSize = 0;
  UINT64                      SpImageSize     = 0;

  DEBUG ((DEBUG_INFO, "Creating HobList from BootInfo. DTBAddress: %lx, TotalSPMemorySize: %lx\r\n", DTBAddress, TotalSPMemorySize));

  /*
   * Check the manifest to ensure the SP is valid.
   */
  Status = CheckManifest (DTBAddress);
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_INFO, "Manifest checked\r\n"));

  /*
   * Parse the boot FDT to get the SP memory base, limit, image base, size, and heap base.
   */
  Status = ParseBootFdt (DTBAddress, TotalSPMemorySize, &SpMemBase, &SpMemLimit, &SpImageBase, &SpImageSize, &SpHeapBase, &SpHeapSize);
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_INFO, "BootFdt parsed\r\n"));

  DEBUG ((DEBUG_INFO, "SpMemBase: %lx, SpHeapBase: %lx, SpHeapSize: %lx\r\n", SpMemBase, SpHeapBase, SpHeapSize));
  DEBUG ((DEBUG_INFO, "SpImageBase: %lx, SpImageSize: %lx\r\n", SpImageBase, SpImageSize));

  /*
   * Get the NsCommBuf and SpSharedBuf base and size.
   */
  Status = GetRegionInfo (DTBAddress, "stmmns-memory", &NsCommBufBase, &NsCommBufSize);
  ASSERT_EFI_ERROR (Status);
  Status = GetRegionInfo (DTBAddress, "stmmsec-memory", &SpSharedBufBase, &SpSharedBufSize);
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_INFO, "NsCommBufBase: %lx, NsCommBufSize: %lx\r\n", NsCommBufBase, NsCommBufSize));
  DEBUG ((DEBUG_INFO, "SpSharedBufBase: %lx, SpSharedBufSize: %lx\r\n", SpSharedBufBase, SpSharedBufSize));

  /*
   * Make the SP heap region RW-XN. Don't use the StandaloneMMULib as it seems to
   * do this on a per page basis which will performance issues.
   */
  Status = SetHeapPermissions (SpHeapBase, SpHeapSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set heap permissions: %lx\r\n", Status));
    ASSERT_EFI_ERROR (Status);
    HobStart = NULL;
    goto ExitFdtBoot;
  }

  DEBUG ((DEBUG_INFO, "Heap Region set to RW-XN\r\n"));

  /*
   * Create a hoblist with a PHIT and EOH
   */
  HobStart = HobConstructor (
               (VOID *)SpMemBase,
               (UINTN)SpMemLimit - SpMemBase,
               (VOID *)SpHeapBase,
               (VOID *)(SpHeapBase + SpHeapSize)
               );

  /*
   * Check that the Hoblist starts at the bottom of the Heap
   */
  ASSERT (HobStart == (VOID *)SpHeapBase);

  DEBUG ((DEBUG_INFO, "HobStart: %lx\r\n", HobStart));

  /*
   * Build a Boot Firmware Volume HOB
   */
  BuildFvHob (SpImageBase, SpImageSize);
  DEBUG ((DEBUG_INFO, "FvHob built\r\n"));

  /*
   * Build a NS Shared Buffer HOB
   */
  Status = BuildNsCommBufHob (NsCommBufBase, NsCommBufSize);
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_INFO, "NsCommBufHob built\r\n"));

  /*
   * Build a MMRAM Ranges HOB for the memory allocator library.
   */
  Status = BuildMmramRangesHob (SpImageBase, SpImageSize, SpSharedBufBase, SpSharedBufSize, NsCommBufBase, NsCommBufSize, HobStart);
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_INFO, "MmramRangesHob built\r\n"));

  /*
   * Build a Platform HOB for the platform specific information.
   */
  Status = BuildPlatformHob (DTBAddress, NsCommBufBase, SpMemBase, TotalSPMemorySize);
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_INFO, "PlatformHob built\r\n"));

  /*
   * Build a Device Mem Regions HOB for the device MMIO regions.
   */
  Status = BuildDeviceMemRegionsHob (DTBAddress);
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_INFO, "DeviceMemRegionsHob built\r\n"));

  DEBUG ((DEBUG_ERROR, "HobList created\r\n"));

ExitFdtBoot:
  return HobStart;
}
