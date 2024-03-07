/** @file
  Entry point to the Standalone MM Foundation when initialized during the SEC
  phase on ARM platforms

SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
Copyright (c) 2017 - 2021, Arm Ltd. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <Library/Arm/StandaloneMmCoreEntryPoint.h>

#include <PiPei.h>
#include <Guid/MmramMemoryReserve.h>
#include <Guid/MpInformation.h>

#include <Library/ArmMmuLib.h>
#include <Library/StandaloneMmMmuLib.h>
#include <Library/ArmSvcLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SerialPortLib.h>
#include <Library/PcdLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/PlatformResourceLib.h>
#include <Library/MmServicesTableLib.h>
#include <Protocol/MmCommunication2.h>

#include <IndustryStandard/ArmStdSmc.h>
#include <IndustryStandard/ArmMmSvc.h>
#include <IndustryStandard/ArmFfaSvc.h>

#include <Include/libfdt.h>

#include "../SlabMmuOps/SlabMmuOps.h"

#define SPM_MAJOR_VER_MASK    0xFFFF0000
#define SPM_MINOR_VER_MASK    0x0000FFFF
#define SPM_MAJOR_VER_SHIFT   16
#define FFA_NOT_SUPPORTED     -1
#define FFA_MSG_WAIT_32       0x8400006B
#define FFA_ERROR_32          0x84000060
#define FFA_VMID_SHIFT        16
#define FFA_VMID_MASK         0xFFFF
#define DEFAULT_PAGE_SIZE     SIZE_4KB
#define MAX_MANIFEST_REGIONS  255
#define SP_PKG_HEADER_SIZE    0x18
/* Request the PA of the STMM_FW NS shared buffer */
#define STMM_GET_NS_BUFFER             0xC0270001
#define STMM_GET_ERST_UNCACHED_BUFFER  0xC0270002
#define STMM_GET_ERST_CACHED_BUFFER    0xC0270003
#define STMM_SATMC_EVENT               0xC0270005

#define TH500_ERST_SW_IO_6_GIC_ID_SOCKET0  230

#define ADDRESS_IN_RANGE(addr, min, max)  (((addr) > (min)) && ((addr) < (max)))

STMM_ARM_MEMORY_REGION_DESCRIPTOR  MemoryTable[MAX_MANIFEST_REGIONS+1];
PI_MM_CPU_DRIVER_ENTRYPOINT        CpuDriverEntryPoint = NULL;
EFI_SECURE_PARTITION_BOOT_INFO     PayloadBootInfo;
STATIC STMM_COMM_BUFFERS           StmmCommBuffers;

STATIC CONST UINT32  mSpmMajorVer = SPM_MAJOR_VERSION;
STATIC CONST UINT32  mSpmMinorVer = SPM_MINOR_VERSION;

STATIC CONST UINT32  mSpmMajorVerFfa = SPM_MAJOR_VERSION_FFA;
STATIC CONST UINT32  mSpmMinorVerFfa = SPM_MINOR_VERSION_FFA;

STATIC CHAR8  Version[VERSION_STR_MAX];

/*
 * Helper function get a 32-bit property from the Manifest and accessing it in a way
 * that won't cause alignment issues if running with MMU disabled.
 */
STATIC
UINT64
FDTGetProperty32 (
  VOID        *DtbAddress,
  INT32       NodeOffset,
  CONST VOID  *PropertyName
  )
{
  CONST VOID  *Property;
  INT32       Length;
  UINT32      P32;

  Property = fdt_getprop (DtbAddress, NodeOffset, PropertyName, &Length);

  ASSERT (Property != NULL);
  ASSERT (Length == 4);

  CopyMem ((VOID *)&P32, (UINT32 *)Property, sizeof (UINT32));

  return SwapBytes32 (P32);
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
  CONST VOID  *PropertyName
  )
{
  CONST VOID  *Property;
  INT32       Length;
  UINT64      P64;

  Property = fdt_getprop (DtbAddress, NodeOffset, PropertyName, &Length);

  ASSERT (Property != NULL);
  ASSERT (Length == 8);

  CopyMem ((VOID *)&P64, (UINT64 *)Property, sizeof (UINT64));

  return SwapBytes64 (P64);
}

/*
 * Quick sanity check of the partition manifest.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 */
EFI_STATUS
CheckManifest (
  IN VOID  *DtbAddress
  )
{
  INT32  HeaderCheck  = -1;
  INT32  ParentOffset = 0;

  /* Check integrity of DTB */
  HeaderCheck = fdt_check_header ((VOID *)DtbAddress);
  if (HeaderCheck != 0) {
    DEBUG ((DEBUG_ERROR, "fdt_check_header failed, err=%d\r\n", HeaderCheck));
    return EFI_DEVICE_ERROR;
  }

  ParentOffset = fdt_path_offset (DtbAddress, "/");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find root node\r\n"));
    return EFI_DEVICE_ERROR;
  }

  ParentOffset = fdt_path_offset (DtbAddress, "/memory-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /memory-regions node\r\n"));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
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

  ParentOffset = fdt_path_offset (DtbAddress, "/");

  SpImageBase = FDTGetProperty64 (DtbAddress, ParentOffset, "load-address") +
                FDTGetProperty32 (DtbAddress, ParentOffset, "entrypoint-offset");

  return SpImageBase;
}

/**
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
    if (IsSocketEnabledStMm (StmmCommBuffers.CpuBlParamsAddr, SockNum) == FALSE) {
      SkipNode = TRUE;
    }
  }

  return SkipNode;
}

/*
 * Get the device regions from the manifest and install a guided hob that
 * the other drivers can use.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 * EFI_SUCCESS                       On Success
 * EFI_NOT_FOUND                     Device Regions not found.
 * OTHER                             On failure to install GuidHob
 */
STATIC
EFI_STATUS
GetDeviceMemRegions (
  IN VOID  *DtbAddress
  )
{
  INT32                 ParentOffset   = 0;
  INT32                 NodeOffset     = 0;
  INT32                 PrevNodeOffset = 0;
  CONST CHAR8           *NodeName;
  EFI_MM_DEVICE_REGION  *DeviceRegions;
  UINTN                 NumRegions;
  UINTN                 BufferSize;
  EFI_STATUS            Status;
  UINTN                 Index;

  NumRegions   = 0;
  Index        = 0;
  Status       = EFI_SUCCESS;
  ParentOffset = fdt_path_offset (DtbAddress, "/device-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /device-regions node\r\n"));
    Status = EFI_NOT_FOUND;
    goto GetDeviceMemRegionsExit;
  }

  for (NodeOffset = fdt_first_subnode (DtbAddress, ParentOffset);
       NodeOffset > 0;
       NodeOffset = fdt_next_subnode (DtbAddress, PrevNodeOffset))
  {
    NodeName = fdt_get_name (DtbAddress, NodeOffset, NULL);

    /*
     * Don't account for a device-region whose socket isn't enabled.
     */
    if (SkipDeviceNode (NodeName) == FALSE) {
      NumRegions++;
    }

    PrevNodeOffset = NodeOffset;
  }

  if (NumRegions == 0) {
    goto GetDeviceMemRegionsExit;
  }

  BufferSize    = NumRegions * sizeof (EFI_MM_DEVICE_REGION);
  DeviceRegions = BuildGuidHob (&gEfiStandaloneMmDeviceMemoryRegions, BufferSize);

  for (NodeOffset = fdt_first_subnode (DtbAddress, ParentOffset);
       NodeOffset > 0;
       NodeOffset = fdt_next_subnode (DtbAddress, PrevNodeOffset))
  {
    NodeName = fdt_get_name (DtbAddress, NodeOffset, NULL);

    /*
     * If Socket specific device regions are present, then check if the socket
     * enabled before adding the region.
     */
    if (SkipDeviceNode (NodeName) == TRUE) {
      DEBUG ((
        DEBUG_ERROR,
        "%a Skip Device %a Socket is not enabled\n",
        __FUNCTION__,
        NodeName
        ));
      PrevNodeOffset = NodeOffset;
      continue;
    }

    DeviceRegions[Index].DeviceRegionStart =
      FDTGetProperty64 (DtbAddress, NodeOffset, "base-address");
    DeviceRegions[Index].DeviceRegionSize = FDTGetProperty32 (
                                              DtbAddress,
                                              NodeOffset,
                                              "pages-count"
                                              )
                                            * DEFAULT_PAGE_SIZE;

    AsciiStrnCpyS (
      DeviceRegions[Index].DeviceRegionName,
      DEVICE_REGION_NAME_MAX_LEN,
      NodeName,
      AsciiStrLen (NodeName)
      );
    DEBUG ((
      DEBUG_ERROR,
      "%a: Name %a Start 0x%lx Size %u\n",
      __FUNCTION__,
      DeviceRegions[Index].DeviceRegionName,
      DeviceRegions[Index].DeviceRegionStart,
      DeviceRegions[Index].DeviceRegionSize
      ));
    Index++;
    PrevNodeOffset = NodeOffset;
  }

GetDeviceMemRegionsExit:
  return Status;
}

/*
 * Gather additional information from the Manifest to populate the PayloadBootInfo structure.
 * The PayloadBootInfo.SpImageBase and PayloadBootInfo.SpImageSize fields must already be initialized.
 *
 * @param  [in] DtbAddress           Address of the partition manifest.
 * @param  [in] TotalSPMemorySize    Total memory allocated to the SP.
 */
EFI_STATUS
GetAndPrintManifestinformation (
  IN VOID    *DtbAddress,
  IN UINT64  TotalSPMemorySize
  )
{
  INT32       ParentOffset   = 0;
  INT32       NodeOffset     = 0;
  INT32       PrevNodeOffset = 0;
  CONST VOID  *NodeName;
  UINT64      FfaRxBufferAddr, FfaTxBufferAddr;
  UINT32      FfaRxBufferSize, FfaTxBufferSize;
  UINT32      ReservedPagesSize;
  UINT64      LoadAddress;
  UINT64      SPMemoryLimit;
  UINT64      LowestRegion, HighestRegion;
  UINT64      RegionAddress;
  UINT32      RegionSize;
  UINT32      ErstUncachedSize;
  UINT32      ErstCachedSize;

  ParentOffset = fdt_path_offset (DtbAddress, "/");

  LoadAddress                = FDTGetProperty64 (DtbAddress, ParentOffset, "load-address");
  PayloadBootInfo.SpMemBase  = LoadAddress;
  PayloadBootInfo.SpMemLimit = PayloadBootInfo.SpImageBase + PayloadBootInfo.SpImageSize;
  SPMemoryLimit              = PayloadBootInfo.SpMemBase + TotalSPMemorySize;
  ReservedPagesSize          = FDTGetProperty32 (DtbAddress, ParentOffset, "reserved-pages-count") * DEFAULT_PAGE_SIZE;
  LowestRegion               = SPMemoryLimit;
  HighestRegion              = PayloadBootInfo.SpMemBase;

  ParentOffset = fdt_path_offset (DtbAddress, "/memory-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /memory-regions node\r\n"));
    return EFI_DEVICE_ERROR;
  }

  for (NodeOffset = fdt_first_subnode (DtbAddress, ParentOffset);
       NodeOffset > 0;
       NodeOffset = fdt_next_subnode (DtbAddress, PrevNodeOffset))
  {
    NodeName      = fdt_get_name (DtbAddress, NodeOffset, NULL);
    RegionAddress = FDTGetProperty64 (DtbAddress, NodeOffset, "base-address");
    RegionSize    = FDTGetProperty32 (DtbAddress, NodeOffset, "pages-count") * DEFAULT_PAGE_SIZE;
    if (ADDRESS_IN_RANGE (RegionAddress, LoadAddress, SPMemoryLimit)) {
      LowestRegion  = MIN (LowestRegion, RegionAddress);
      HighestRegion = MAX (HighestRegion, (RegionAddress+RegionSize));
    }

    /* For each known resource type, extract information */
    if (AsciiStrCmp (NodeName, "stmmns-memory") == 0) {
      ErstCachedSize   = FDTGetProperty32 (DtbAddress, NodeOffset, "nv-erst-cached-pages-count") * DEFAULT_PAGE_SIZE;
      ErstUncachedSize = FDTGetProperty32 (DtbAddress, NodeOffset, "nv-erst-uncached-pages-count") * DEFAULT_PAGE_SIZE;

      /* Publish the Ns Buffer Addr Size to what StMM needs.*/
      PayloadBootInfo.SpNsCommBufBase = RegionAddress;
      PayloadBootInfo.SpNsCommBufSize = (RegionSize - ErstUncachedSize - ErstCachedSize);

      /**
       * STMM Buffer Base and Size.
      */
      StmmCommBuffers.NsBufferAddr = PayloadBootInfo.SpNsCommBufBase;
      StmmCommBuffers.NsBufferSize = PayloadBootInfo.SpNsCommBufSize;

      /**
       * ERST Uncached Base and Size.
      */
      StmmCommBuffers.NsErstUncachedBufAddr = (PayloadBootInfo.SpNsCommBufBase + PayloadBootInfo.SpNsCommBufSize);
      StmmCommBuffers.NsErstUncachedBufSize = ErstUncachedSize;

      /**
       * ERST Cached Base and Size.
      */
      StmmCommBuffers.NsErstCachedBufAddr = StmmCommBuffers.NsErstUncachedBufAddr + StmmCommBuffers.NsErstUncachedBufSize;
      StmmCommBuffers.NsErstCachedBufSize = ErstCachedSize;

      DEBUG ((
        DEBUG_INFO,
        "%a: StMM Base 0x%lx Size 0x%x\n",
        __FUNCTION__,
        PayloadBootInfo.SpNsCommBufBase,
        PayloadBootInfo.SpNsCommBufSize
        ));
      DEBUG ((
        DEBUG_INFO,
        "%a: ERST-Uncached Base 0x%lx Size 0x%x\n",
        __FUNCTION__,
        StmmCommBuffers.NsBufferAddr,
        StmmCommBuffers.NsBufferSize
        ));
      DEBUG ((
        DEBUG_INFO,
        "%a: ERST-Cached Base 0x%lx Size 0x%x\n",
        __FUNCTION__,
        StmmCommBuffers.NsErstUncachedBufAddr,
        StmmCommBuffers.NsErstUncachedBufSize
        ));
    } else if (AsciiStrCmp (NodeName, "rx-buffer") == 0) {
      FfaRxBufferAddr = RegionAddress;
      FfaRxBufferSize = RegionSize;
    } else if (AsciiStrCmp (NodeName, "tx-buffer") == 0) {
      FfaTxBufferAddr = RegionAddress;
      FfaTxBufferSize = RegionSize;
    } else if (AsciiStrCmp (NodeName, "stmmsec-memory") == 0) {
      StmmCommBuffers.SecBufferAddr = RegionAddress;
      StmmCommBuffers.SecBufferSize = RegionSize;
    } else if (AsciiStrCmp (NodeName, "cpubl-params") == 0) {
      StmmCommBuffers.CpuBlParamsAddr = RegionAddress;
      StmmCommBuffers.CpuBlParamsSize = RegionSize;
    } else if (AsciiStrCmp (NodeName, "common-shared-buffer-ras-mm") == 0) {
      StmmCommBuffers.RasMmBufferAddr = RegionAddress;
      StmmCommBuffers.RasMmBufferSize = RegionSize;
    } else if (AsciiStrCmp (NodeName, "common-shared-buffer-satmc-mm") == 0) {
      StmmCommBuffers.SatMcMmBufferAddr = RegionAddress;
      StmmCommBuffers.SatMcMmBufferSize = RegionSize;
    }

    PrevNodeOffset = NodeOffset;
  }

  /* Find the free memory in the SP space to use as driver heap */
 #ifdef HEAP_HIGH_REGION
  PayloadBootInfo.SpHeapBase = HighestRegion;
  PayloadBootInfo.SpHeapSize = SPMemoryLimit - PayloadBootInfo.SpHeapBase;
 #else
  PayloadBootInfo.SpHeapBase = PayloadBootInfo.SpMemLimit + ReservedPagesSize;
  PayloadBootInfo.SpHeapSize = LowestRegion - PayloadBootInfo.SpHeapBase;
 #endif
  DEBUG ((DEBUG_ERROR, "SPMEMBASE 0x%x RESERVED 0x%x SIZE 0x%x\n", PayloadBootInfo.SpHeapBase, ReservedPagesSize, PayloadBootInfo.SpHeapSize));

  /* Some StMM regions are not needed or don't apply to an UP migratable partition */
  PayloadBootInfo.SpSharedBufBase = 0;
  PayloadBootInfo.SpSharedBufSize = 0;
  PayloadBootInfo.SpStackBase     = 0;
  PayloadBootInfo.SpPcpuStackSize = 0;
  PayloadBootInfo.NumCpus         = 0;

  PayloadBootInfo.NumSpMemRegions = 6;

  DEBUG ((DEBUG_ERROR, "SP mem base       = 0x%llx \n", PayloadBootInfo.SpMemBase));
  DEBUG ((DEBUG_ERROR, "  SP image base   = 0x%llx \n", PayloadBootInfo.SpImageBase));
  DEBUG ((DEBUG_ERROR, "  SP image size   = 0x%llx \n", PayloadBootInfo.SpImageSize));
  DEBUG ((DEBUG_ERROR, "SP mem limit      = 0x%llx \n", PayloadBootInfo.SpMemLimit));
  DEBUG ((DEBUG_ERROR, "Core-Heap limit   = 0x%llx \n", PayloadBootInfo.SpMemLimit + ReservedPagesSize));
  DEBUG ((DEBUG_ERROR, "FFA rx buf base   = 0x%llx \n", FfaRxBufferAddr));
  DEBUG ((DEBUG_ERROR, "FFA rx buf size   = 0x%llx \n", FfaRxBufferSize));
  DEBUG ((DEBUG_ERROR, "FFA tx buf base   = 0x%llx \n", FfaTxBufferAddr));
  DEBUG ((DEBUG_ERROR, "FFA tx buf size   = 0x%llx \n", FfaTxBufferSize));
  DEBUG ((DEBUG_ERROR, "Driver-Heap base  = 0x%llx \n", PayloadBootInfo.SpHeapBase));
  DEBUG ((DEBUG_ERROR, "Driver-Heap size  = 0x%llx \n", PayloadBootInfo.SpHeapSize));
  DEBUG ((DEBUG_ERROR, "SP real mem limit = 0x%llx \n", SPMemoryLimit));

  DEBUG ((DEBUG_ERROR, "Shared Buffers:\n"));
  DEBUG ((DEBUG_ERROR, "SP NS buf base    = 0x%llx \n", StmmCommBuffers.NsBufferAddr));
  DEBUG ((DEBUG_ERROR, "SP NS buf size    = 0x%llx \n", StmmCommBuffers.NsBufferSize));
  DEBUG ((DEBUG_ERROR, "SP Sec buf base   = 0x%llx \n", StmmCommBuffers.SecBufferAddr));
  DEBUG ((DEBUG_ERROR, "SP Sec buf size   = 0x%llx \n", StmmCommBuffers.SecBufferSize));
  DEBUG ((DEBUG_ERROR, "CPU BL buf base   = 0x%llx \n", StmmCommBuffers.CpuBlParamsAddr));
  DEBUG ((DEBUG_ERROR, "CPU BL buf size   = 0x%llx \n", StmmCommBuffers.CpuBlParamsSize));
  DEBUG ((DEBUG_ERROR, "RAS MM buf base   = 0x%llx \n", StmmCommBuffers.RasMmBufferAddr));
  DEBUG ((DEBUG_ERROR, "RAS MM buf size   = 0x%llx \n", StmmCommBuffers.RasMmBufferSize));
  DEBUG ((DEBUG_ERROR, "SatMc MM buf base = 0x%llx \n", StmmCommBuffers.SatMcMmBufferAddr));
  DEBUG ((DEBUG_ERROR, "SatMc MM buf size = 0x%llx \n", StmmCommBuffers.SatMcMmBufferSize));

  /* Core will take all the memory from SpMemBase to CoreHeapLimit and should not reach the first memory-region */
  ASSERT ((PayloadBootInfo.SpMemLimit + ReservedPagesSize) <= FfaRxBufferAddr);

  if (ADDRESS_IN_RANGE (PayloadBootInfo.SpNsCommBufBase, PayloadBootInfo.SpMemBase, SPMemoryLimit)) {
    DEBUG ((DEBUG_ERROR, "Not FBC\n"));
    StmmCommBuffers.Fbc = FALSE;
    ASSERT ((PayloadBootInfo.SpMemLimit + ReservedPagesSize) <= PayloadBootInfo.SpNsCommBufBase);
  } else {
    StmmCommBuffers.Fbc = TRUE;
  }

  return EFI_SUCCESS;
}

// JDS TODO - clean this up once I figure out how to add the header
typedef
EFI_STATUS
(EFIAPI *ERROR_SERIALIZATION_INTERRUPT_HANDLER)(
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  );

typedef struct {
  ERROR_SERIALIZATION_INTERRUPT_HANDLER    InterruptHandler;
} ERROR_SERIALIZATION_MM_PROTOCOL;

STATIC ERROR_SERIALIZATION_MM_PROTOCOL  *ErrorSerializationProtocol;

EFI_STATUS
GetErrorSerializationProtocol (
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  UINTN       NumHandles;
  UINTN       HandleBufferSize;
  EFI_HANDLE  HandleBuffer[1];

  if (ErrorSerializationProtocol != NULL) {
    return EFI_SUCCESS;
  }

  ErrorSerializationProtocol = NULL;

  HandleBufferSize = sizeof (HandleBuffer);
  Status           = gMmst->MmLocateHandle (
                              ByProtocol,
                              &gNVIDIAErrorSerializationProtocolGuid,
                              NULL,
                              &HandleBufferSize,
                              HandleBuffer
                              );
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Error locating MM-ErrorSerialization handles: %r\n", Status));
    if (Status == EFI_BUFFER_TOO_SMALL) {
      DEBUG ((DEBUG_ERROR, "The Handle buffer size (%lu) is too small\n", HandleBufferSize));
    }

    goto Done;
  }

  NumHandles = HandleBufferSize / sizeof (EFI_HANDLE);

  for (Index = 0; Index < NumHandles; Index++) {
    Status = gMmst->MmHandleProtocol (
                      HandleBuffer[Index],
                      &gNVIDIAErrorSerializationProtocolGuid,
                      (VOID **)&ErrorSerializationProtocol
                      );
    if ((Status != EFI_SUCCESS) || (ErrorSerializationProtocol == NULL)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to get MM-ErrorSerializationProtocol for handle index %u: %r\n",
        Index,
        Status
        ));
      if ((Status == EFI_SUCCESS) && (ErrorSerializationProtocol == NULL)) {
        DEBUG ((DEBUG_ERROR, "Couldn't get MM-ErrorSerialization Protocol\n"));
        Status = EFI_NO_MAPPING;
      }

      goto Done;
    }

    if (TRUE) {
      goto Done;
    }
  }

  DEBUG ((DEBUG_ERROR, "Couldn't locate MM-ErrorSerialization Protocol\n"));
  Status = EFI_NO_MEDIA;

Done:
  return Status;
}

/**
 * Check if payload buffer address is valid for the sender VM's. A valid payload address
 * should be in the correct range for this VM's mailbox (that is in StMM's manifest) and
 * is large enough.
 *
 * @param  [in] CommBufStart    Incoming buffer id comtaining the payload
 * @param  [in] SenderPartId    VM Id of the Source SP.
 *
 * @retval      EFI_SUCCESS            Buffer is valid and big enough.
 *              EFI_INVALID_PARAMETER  The buffer is not in the valid range for this VM OR
 *                                     isn't big enough.
 *              EFI_UNSUPPORTED        Can't lookup the GUID'd Hob to get the Comm
 *                                     Buffers OR communication for the incoming SenderId
 *                                     isn't supported.
 */
STATIC
EFI_STATUS
CheckBufferAddr (
  IN UINTN   CommBufStart,
  IN UINT16  SenderPartId
  )
{
  UINT64             SecBufStart;
  UINT32             SecBufRange;
  UINT64             SecBufEnd;
  UINT64             CommBufEnd;
  EFI_STATUS         Status;
  EFI_HOB_GUID_TYPE  *GuidHob;
  STMM_COMM_BUFFERS  *StmmCommBuffers;

  GuidHob = GetFirstGuidHob (&gNVIDIAStMMBuffersGuid);
  if (GuidHob == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to find Buffers GUID HOB\n"));
    Status = EFI_UNSUPPORTED;
    goto ExitCheckBufferAddr;
  }

  StmmCommBuffers = (STMM_COMM_BUFFERS *)GET_GUID_HOB_DATA (GuidHob);

  if (SenderPartId == RASFW_VMID) {
    SecBufStart = StmmCommBuffers->RasMmBufferAddr;
    SecBufRange = StmmCommBuffers->RasMmBufferSize;
    SecBufEnd   = SecBufStart + SecBufRange;
  } else if (SenderPartId == SATMC_VMID) {
    SecBufStart = StmmCommBuffers->SatMcMmBufferAddr;
    SecBufRange = StmmCommBuffers->SatMcMmBufferSize;
    SecBufEnd   = SecBufStart + SecBufRange;
  } else {
    Status = EFI_UNSUPPORTED;
    goto ExitCheckBufferAddr;
  }

  if ((CommBufStart >= SecBufStart) &&
      (CommBufStart < SecBufEnd))
  {
    CommBufEnd = SecBufEnd;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: CommBuff[0x%lx] not in range [0x%lx - 0x%lx] \n",
      __FUNCTION__,
      CommBufStart,
      SecBufStart,
      SecBufEnd
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitCheckBufferAddr;
  }

  if ((CommBufEnd - CommBufStart) < sizeof (EFI_MM_COMMUNICATE_HEADER)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: CommBuff[0x%lx] not enough %u for header(%u)\n",
      __FUNCTION__,
      CommBufStart,
      (CommBufEnd - CommBufStart),
      sizeof (EFI_MM_COMMUNICATE_HEADER)
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitCheckBufferAddr;
  }

  // perform bounds check.
  if ((CommBufEnd - CommBufStart - sizeof (EFI_MM_COMMUNICATE_HEADER)) <
      ((EFI_MM_COMMUNICATE_HEADER *)CommBufStart)->MessageLength)
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: CommBuff[0x%lx] not enough %u for Payload(%u)\n",
      __FUNCTION__,
      CommBufStart,
      (CommBufEnd - CommBufStart - sizeof (EFI_MM_COMMUNICATE_HEADER)),
      ((EFI_MM_COMMUNICATE_HEADER *)CommBufStart)->MessageLength
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitCheckBufferAddr;
  }

  Status = EFI_SUCCESS;
ExitCheckBufferAddr:
  return Status;
}

/**
 * Handle Communication between SPs. The NS-S communication will not be handled
 * by this function.
 * Check if payload buffer address is in the correct range for the sender VM's
 * mailbox. If Valid, then try to route this request to the correct MMI handler.
 *
 * @param  [in] SenderPartId   Sender VM Id.
 * @param  [in] SecBuf         Pointer to the payload.
 *
 * @retval      EFI_SUCCESS            Successfully parsed the incoming payload
 *                                     and re-directed the call to the appropriate
 *                                     MMI handler.
 *              OTHER                  Invalid Buffer address ORFrom MMI handler when
 *                                     trying to route this request.
 */
STATIC
EFI_STATUS
HandleSpComm (
  IN UINT16  SenderPartId,
  IN UINTN   SecBuf
  )
{
  EFI_STATUS                 Status;
  EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader;

  Status = CheckBufferAddr (SecBuf, SenderPartId);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Address %lx is not valid %r \n",
      __FUNCTION__,
      SecBuf,
      Status
      ));
    goto ExitHandleSpComm;
  }

  CommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)(UINTN)SecBuf;
  Status            = gMmst->MmiManage (
                               &CommunicateHeader->HeaderGuid,
                               NULL,
                               CommunicateHeader->Data,
                               &CommunicateHeader->MessageLength
                               );

ExitHandleSpComm:
  return Status;
}

/**
 * A loop to delegated events.
 *
 * @param  [in] EventCompleteSvcArgs   Pointer to the event completion arguments.
 *
 */
VOID
EFIAPI
DelegatedEventLoop (
  IN ARM_SVC_ARGS  *EventCompleteSvcArgs
  )
{
  EFI_STATUS  Status;
  UINTN       SvcStatus;
  BOOLEAN     FfaEnabled;
  UINT16      SenderPartId;
  UINT16      ReceiverPartId;

  while (TRUE) {
    ArmCallSvc (EventCompleteSvcArgs);

    DEBUG ((DEBUG_INFO, "Received delegated event\n"));
    DEBUG ((DEBUG_INFO, "X0 :  0x%x\n", (UINT32)EventCompleteSvcArgs->Arg0));
    DEBUG ((DEBUG_INFO, "X1 :  0x%x\n", (UINT32)EventCompleteSvcArgs->Arg1));
    DEBUG ((DEBUG_INFO, "X2 :  0x%x\n", (UINT32)EventCompleteSvcArgs->Arg2));
    DEBUG ((DEBUG_INFO, "X3 :  0x%x\n", (UINT32)EventCompleteSvcArgs->Arg3));
    DEBUG ((DEBUG_INFO, "X4 :  0x%x\n", (UINT32)EventCompleteSvcArgs->Arg4));
    DEBUG ((DEBUG_INFO, "X5 :  0x%lx\n", (UINTN)EventCompleteSvcArgs->Arg5));
    DEBUG ((DEBUG_INFO, "X6 :  0x%x\n", (UINT32)EventCompleteSvcArgs->Arg6));
    DEBUG ((DEBUG_INFO, "X7 :  0x%x\n", (UINT32)EventCompleteSvcArgs->Arg7));

    SenderPartId   = EventCompleteSvcArgs->Arg1 >> FFA_VMID_SHIFT;
    ReceiverPartId = EventCompleteSvcArgs->Arg1 & FFA_VMID_MASK;

    FfaEnabled = FeaturePcdGet (PcdFfaEnable);
    if (FfaEnabled) {
      switch (EventCompleteSvcArgs->Arg3) {
        case STMM_GET_NS_BUFFER:
          EventCompleteSvcArgs->Arg5 = StmmCommBuffers.NsBufferAddr;
          EventCompleteSvcArgs->Arg6 = StmmCommBuffers.NsBufferSize;
          Status                     = EFI_SUCCESS;
          break;
        case STMM_GET_ERST_UNCACHED_BUFFER:
          EventCompleteSvcArgs->Arg5 = StmmCommBuffers.NsErstUncachedBufAddr;
          EventCompleteSvcArgs->Arg6 = StmmCommBuffers.NsErstUncachedBufSize;
          Status                     = EFI_SUCCESS;
          break;
        case STMM_GET_ERST_CACHED_BUFFER:
          EventCompleteSvcArgs->Arg5 = StmmCommBuffers.NsErstCachedBufAddr;
          EventCompleteSvcArgs->Arg6 = StmmCommBuffers.NsErstCachedBufSize;
          Status                     = EFI_SUCCESS;
          break;
        case STMM_SATMC_EVENT:
          if (EventCompleteSvcArgs->Arg6 == TH500_ERST_SW_IO_6_GIC_ID_SOCKET0) {
            Status = GetErrorSerializationProtocol ();
            if (ErrorSerializationProtocol != NULL) {
              Status = ErrorSerializationProtocol->InterruptHandler (NULL, NULL, NULL, NULL);
            }
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        case ARM_SMC_ID_MM_COMMUNICATE_AARCH64:
          if (SenderPartId == 0) {
            Status = CpuDriverEntryPoint (
                       EventCompleteSvcArgs->Arg0,
                       EventCompleteSvcArgs->Arg6,
                       EventCompleteSvcArgs->Arg5
                       );
            if (EFI_ERROR (Status)) {
              DEBUG ((
                DEBUG_ERROR,
                "Failed delegated event 0x%x, Status 0x%x\n",
                EventCompleteSvcArgs->Arg3,
                Status
                ));
            }
          } else {
            Status = HandleSpComm (SenderPartId, (UINTN)EventCompleteSvcArgs->Arg5);
            if (EFI_ERROR (Status)) {
              DEBUG ((
                DEBUG_ERROR,
                "Secure SPComm Failed delegated event 0x%x, Status 0x%x\n",
                EventCompleteSvcArgs->Arg3,
                Status
                ));
            }
          }

          break;
        default:
          DEBUG ((DEBUG_ERROR, "Unknown DelegatedEvent request 0x%x\n", EventCompleteSvcArgs->Arg3));
          Status = EFI_UNSUPPORTED;
          break;
      }
    } else {
      Status = CpuDriverEntryPoint (
                 EventCompleteSvcArgs->Arg0,
                 EventCompleteSvcArgs->Arg3,
                 EventCompleteSvcArgs->Arg1
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "Failed delegated event 0x%x, Status 0x%x\n",
          EventCompleteSvcArgs->Arg0,
          Status
          ));
      }
    }

    switch (Status) {
      case EFI_SUCCESS:
        SvcStatus = ARM_SVC_SPM_RET_SUCCESS;
        break;
      case EFI_INVALID_PARAMETER:
        SvcStatus = ARM_SVC_SPM_RET_INVALID_PARAMS;
        break;
      case EFI_ACCESS_DENIED:
        SvcStatus = ARM_SVC_SPM_RET_DENIED;
        break;
      case EFI_OUT_OF_RESOURCES:
        SvcStatus = ARM_SVC_SPM_RET_NO_MEMORY;
        break;
      case EFI_UNSUPPORTED:
        SvcStatus = ARM_SVC_SPM_RET_NOT_SUPPORTED;
        break;
      default:
        SvcStatus = ARM_SVC_SPM_RET_NOT_SUPPORTED;
        break;
    }

    if (FfaEnabled) {
      EventCompleteSvcArgs->Arg0 = ARM_SVC_ID_FFA_MSG_SEND_DIRECT_RESP;
      EventCompleteSvcArgs->Arg1 = ReceiverPartId << FFA_VMID_SHIFT | SenderPartId;
      EventCompleteSvcArgs->Arg2 = 0;
      EventCompleteSvcArgs->Arg3 = ARM_SVC_ID_SP_EVENT_COMPLETE;
      EventCompleteSvcArgs->Arg4 = SvcStatus;
    } else {
      EventCompleteSvcArgs->Arg0 = ARM_SVC_ID_SP_EVENT_COMPLETE;
      EventCompleteSvcArgs->Arg1 = SvcStatus;
    }
  }
}

/**
  Query the SPM version, check compatibility and return success if compatible.

  @retval EFI_SUCCESS       SPM versions compatible.
  @retval EFI_UNSUPPORTED   SPM versions not compatible.
**/
STATIC
EFI_STATUS
GetSpmVersion (
  VOID
  )
{
  EFI_STATUS    Status;
  UINT16        CalleeSpmMajorVer;
  UINT16        CallerSpmMajorVer;
  UINT16        CalleeSpmMinorVer;
  UINT16        CallerSpmMinorVer;
  UINT32        SpmVersion;
  ARM_SVC_ARGS  SpmVersionArgs;

  if (FeaturePcdGet (PcdFfaEnable)) {
    SpmVersionArgs.Arg0  = ARM_SVC_ID_FFA_VERSION_AARCH32;
    SpmVersionArgs.Arg1  = mSpmMajorVerFfa << SPM_MAJOR_VER_SHIFT;
    SpmVersionArgs.Arg1 |= mSpmMinorVerFfa;
    CallerSpmMajorVer    = mSpmMajorVerFfa;
    CallerSpmMinorVer    = mSpmMinorVerFfa;
  } else {
    SpmVersionArgs.Arg0 = ARM_SVC_ID_SPM_VERSION_AARCH32;
    CallerSpmMajorVer   = mSpmMajorVer;
    CallerSpmMinorVer   = mSpmMinorVer;
  }

  ArmCallSvc (&SpmVersionArgs);

  SpmVersion = SpmVersionArgs.Arg0;
  if (SpmVersion == FFA_NOT_SUPPORTED) {
    return EFI_UNSUPPORTED;
  }

  CalleeSpmMajorVer = ((SpmVersion & SPM_MAJOR_VER_MASK) >> SPM_MAJOR_VER_SHIFT);
  CalleeSpmMinorVer = ((SpmVersion & SPM_MINOR_VER_MASK) >> 0);

  // Different major revision values indicate possibly incompatible functions.
  // For two revisions, A and B, for which the major revision values are
  // identical, if the minor revision value of revision B is greater than
  // the minor revision value of revision A, then every function in
  // revision A must work in a compatible way with revision B.
  // However, it is possible for revision B to have a higher
  // function count than revision A.
  if ((CalleeSpmMajorVer == CallerSpmMajorVer) &&
      (CalleeSpmMinorVer >= CallerSpmMinorVer))
  {
    DEBUG ((
      DEBUG_INFO,
      "SPM Version: Major=0x%x, Minor=0x%x\n",
      CalleeSpmMajorVer,
      CalleeSpmMinorVer
      ));
    Status = EFI_SUCCESS;
  } else {
    DEBUG ((
      DEBUG_INFO,
      "Incompatible SPM Versions.\n Callee Version: Major=0x%x, Minor=0x%x.\n Caller: Major=0x%x, Minor>=0x%x.\n",
      CalleeSpmMajorVer,
      CalleeSpmMinorVer,
      CallerSpmMajorVer,
      CallerSpmMinorVer
      ));
    Status = EFI_UNSUPPORTED;
  }

  return Status;
}

/**
 * Initialize parameters to be sent via SVC call.
 *
 * @param[out]     InitMmFoundationSvcArgs  Args structure
 * @param[out]     Ret                      Return Code
 *
 */
STATIC
VOID
InitArmSvcArgs (
  OUT ARM_SVC_ARGS  *InitMmFoundationSvcArgs,
  OUT INT32         *Ret
  )
{
  if (*Ret == 0) {
    InitMmFoundationSvcArgs->Arg0 = FFA_MSG_WAIT_32;
  } else {
    InitMmFoundationSvcArgs->Arg0 = FFA_ERROR_32;
  }

  InitMmFoundationSvcArgs->Arg1 = 0;
  InitMmFoundationSvcArgs->Arg2 = *Ret;
  InitMmFoundationSvcArgs->Arg3 = 0;
  InitMmFoundationSvcArgs->Arg4 = 0;
}

/**
 * Generate a table that contains all the memory regions that need to be mapped as stage-1 translations.
 * For DRAM, simply use the base of the SP (calculated as DTBAddress - sizeof(sp_pkg_header)) and use the total
 * SP Memory size as given by Hafnium.
 * For Devices, parse the Manifest looking for entries in the /device-regions node.
 *
 * Considering that parsing the Manifest in this function is done with caches disabled, it can be quite time consuming,
 * so in special development platforms, a "fast" mode can be used where all of the MMIO space is mapped (limited to
 * socket 0) instead of relying on the Manifest. In this case, access control to MMIO will still be ensured by stage-2
 * translations.
 *
 * @param[in]          TotalSPMemorySize  Total SP Memory size as given by Hafnium
 * @param[in]          DTBAddress         Address of the DTB as found by the entry point loader
 **/
STATIC
VOID
ConfigureStage1Translations (
  IN UINT64  TotalSPMemorySize,
  IN VOID    *DTBAddress
  )
{
  UINT64      Stage1EntriesAddress, Stage1EntriesPages;
  UINT64      RegionAddress, RegionSize;
  UINT64      NsBufferAddress, NsBufferSize;
  EFI_STATUS  Status;
  UINT16      NumRegions       = 0;
  INT32       ParentOffset     = 0;
  INT32       NodeOffset       = 0;
  INT32       PrevNodeOffset   = 0;
  UINT32      ErstCachedSize   = 0;
  UINT32      ErstUnCachedSize = 0;
  UINT32      NsUncachedSize   = 0;

 #ifdef FAST_STAGE1_SETUP
  /*In "Fast" mode, simply allocate the MMIO range of socket 0. That's sufficient for FPGA-based testing. */
  MemoryTable[NumRegions].PhysicalBase = 0;
  MemoryTable[NumRegions].VirtualBase  = 0;
  MemoryTable[NumRegions].Length       = 0x80000000;
  MemoryTable[NumRegions].Attributes   = STMM_ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
  NumRegions++;

 #else
  /* Loop over all the device-regions of the manifest. This is time-consuming with caches disabled. */
  ParentOffset = fdt_path_offset (DTBAddress, "/device-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /device-regions node\r\n"));
    ASSERT (0);
  }

  for (NodeOffset = fdt_first_subnode (DTBAddress, ParentOffset); NodeOffset > 0;
       NodeOffset = fdt_next_subnode (DTBAddress, PrevNodeOffset))
  {
    RegionAddress = PAGE_ALIGN (FDTGetProperty64 (DTBAddress, NodeOffset, "base-address"), DEFAULT_PAGE_SIZE);
    RegionSize    = FDTGetProperty32 (DTBAddress, NodeOffset, "pages-count") * DEFAULT_PAGE_SIZE;

    MemoryTable[NumRegions].PhysicalBase = RegionAddress;
    MemoryTable[NumRegions].VirtualBase  = RegionAddress;
    MemoryTable[NumRegions].Length       = RegionSize;
    MemoryTable[NumRegions].Attributes   = STMM_ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
    NumRegions++;
    ASSERT (NumRegions < MAX_MANIFEST_REGIONS);

    PrevNodeOffset = NodeOffset;
  }

 #endif

  /* Single section for the whole SP memory */
  MemoryTable[NumRegions].PhysicalBase = PAGE_ALIGN ((UINT64)DTBAddress, DEFAULT_PAGE_SIZE);
  MemoryTable[NumRegions].VirtualBase  = MemoryTable[NumRegions].PhysicalBase;
  MemoryTable[NumRegions].Length       = TotalSPMemorySize;
  MemoryTable[NumRegions].Attributes   = STMM_ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;
  NumRegions++;

  /* Loop over all the memory-regions of the manifest. This is time-consuming with caches disabled. */
  ParentOffset = fdt_path_offset (DTBAddress, "/memory-regions");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /memory-regions node\r\n"));
    ASSERT (0);
  }

  NsBufferAddress = 0;
  NsBufferSize    = 0;
  for (NodeOffset = fdt_first_subnode (DTBAddress, ParentOffset); NodeOffset > 0;
       NodeOffset = fdt_next_subnode (DTBAddress, PrevNodeOffset))
  {
    INT32       Length;
    CONST VOID  *NodeName;

    NodeName = fdt_get_name (DTBAddress, NodeOffset, NULL);
    if (NodeName == NULL) {
      PrevNodeOffset = NodeOffset;
      continue;
    }

    if (fdt_getprop (DTBAddress, NodeOffset, "nv-non-secure-memory", &Length) != NULL) {
      NsBufferAddress  = PAGE_ALIGN (FDTGetProperty64 (DTBAddress, NodeOffset, "base-address"), DEFAULT_PAGE_SIZE);
      NsBufferSize     = FDTGetProperty32 (DTBAddress, NodeOffset, "pages-count") * DEFAULT_PAGE_SIZE;
      ErstCachedSize   = FDTGetProperty32 (DTBAddress, NodeOffset, "nv-erst-cached-pages-count") * DEFAULT_PAGE_SIZE;
      ErstUnCachedSize = FDTGetProperty32 (DTBAddress, NodeOffset, "nv-erst-uncached-pages-count") * DEFAULT_PAGE_SIZE;
      NsUncachedSize   = (NsBufferSize - ErstCachedSize);

      /* NS Uncached region (StMM Buffer + Part of ERST) */
      MemoryTable[NumRegions].PhysicalBase = NsBufferAddress;
      MemoryTable[NumRegions].VirtualBase  = NsBufferAddress;
      MemoryTable[NumRegions].Length       = NsUncachedSize;
      MemoryTable[NumRegions].Attributes   = STMM_ARM_MEMORY_REGION_ATTRIBUTE_NONSECURE_UNCACHED_UNBUFFERED;
      DEBUG ((
        DEBUG_ERROR,
        "UnCached NS Address = 0x%llx Size 0x%lx Attr 0x%x \n",
        MemoryTable[NumRegions].PhysicalBase,
        MemoryTable[NumRegions].Length,
        MemoryTable[NumRegions].Attributes
        ));
      NumRegions++;

      /* NS Cached region (erst)*/
      MemoryTable[NumRegions].PhysicalBase = (NsBufferAddress + NsUncachedSize);
      MemoryTable[NumRegions].VirtualBase  = (NsBufferAddress + NsUncachedSize);
      MemoryTable[NumRegions].Length       = ErstCachedSize;
      MemoryTable[NumRegions].Attributes   = STMM_ARM_MEMORY_REGION_ATTRIBUTE_NONSECURE_WRITE_BACK;
      DEBUG ((
        DEBUG_ERROR,
        "Cached NS Address = 0x%llx Size 0x%lx Attr 0x%x \n",
        MemoryTable[NumRegions].PhysicalBase,
        MemoryTable[NumRegions].Length,
        MemoryTable[NumRegions].Attributes
        ));
      NumRegions++;
    }

    if (fdt_getprop (DTBAddress, NodeOffset, "nv-sp-shared-buffer-id", &Length) != NULL) {
      RegionAddress = PAGE_ALIGN (FDTGetProperty64 (DTBAddress, NodeOffset, "base-address"), DEFAULT_PAGE_SIZE);
      RegionSize    = FDTGetProperty32 (DTBAddress, NodeOffset, "pages-count") * DEFAULT_PAGE_SIZE;
      /* Secure Buffer */
      MemoryTable[NumRegions].PhysicalBase = RegionAddress;
      MemoryTable[NumRegions].VirtualBase  = RegionAddress;
      MemoryTable[NumRegions].Length       = RegionSize;
      MemoryTable[NumRegions].Attributes   = STMM_ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;
      NumRegions++;
    }

    if (AsciiStrStr (NodeName, "cpubl-params") != NULL) {
      RegionAddress                        = PAGE_ALIGN (FDTGetProperty64 (DTBAddress, NodeOffset, "base-address"), DEFAULT_PAGE_SIZE);
      RegionSize                           = FDTGetProperty32 (DTBAddress, NodeOffset, "pages-count") * DEFAULT_PAGE_SIZE;
      MemoryTable[NumRegions].PhysicalBase = RegionAddress;
      MemoryTable[NumRegions].VirtualBase  = RegionAddress;
      MemoryTable[NumRegions].Length       = RegionSize;
      MemoryTable[NumRegions].Attributes   = STMM_ARM_MEMORY_REGION_ATTRIBUTE_NONSECURE_UNCACHED_UNBUFFERED;
      DEBUG ((DEBUG_ERROR, "CPUBL Address     = 0x%llx \n", RegionAddress));
      DEBUG ((DEBUG_ERROR, "CPUPL Size      = 0x%llx \n", RegionSize));
      NumRegions++;
    }

    if (AsciiStrStr (NodeName, "stage1-entries") != NULL) {
      Stage1EntriesAddress = PAGE_ALIGN (FDTGetProperty64 (DTBAddress, NodeOffset, "base-address"), DEFAULT_PAGE_SIZE);
      Stage1EntriesPages   = FDTGetProperty32 (DTBAddress, NodeOffset, "pages-count");
      DEBUG ((DEBUG_ERROR, "Stage-1 base      = 0x%llx \n", Stage1EntriesAddress));
      DEBUG ((DEBUG_ERROR, "Stage-1 size      = 0x%llx \n", Stage1EntriesPages*DEFAULT_PAGE_SIZE));
      SlabArmSetEntriesSlab (Stage1EntriesAddress, Stage1EntriesPages);
    }

    PrevNodeOffset = NodeOffset;
  }

  ASSERT (NsBufferAddress != 0);
  ASSERT (NsBufferSize != 0);

  /* Last entry must be all 0 */
  MemoryTable[NumRegions].PhysicalBase = 0;
  MemoryTable[NumRegions].VirtualBase  = 0;
  MemoryTable[NumRegions].Length       = 0;
  MemoryTable[NumRegions].Attributes   = 0;

  Status = SlabArmConfigureMmu (MemoryTable, NULL, NULL);
  ASSERT (Status == EFI_SUCCESS);
}

/**
 * The C entry point of the partition.
 *
 * @param  [in] TotalSPMemorySize    Total memory allocated to the SP.
 * @param  [in] DtbAddress           Address of the partition manifest.
 */
VOID
EFIAPI
_ModuleEntryPointC (
  IN UINT64  TotalSPMemorySize,
  IN VOID    *DTBAddress
  )
{
  PE_COFF_LOADER_IMAGE_CONTEXT  ImageContext;
  ARM_SVC_ARGS                  InitMmFoundationSvcArgs;
  EFI_STATUS                    Status;
  INT32                         Ret;
  UINT32                        SectionHeaderOffset;
  UINT16                        NumberOfSections;
  VOID                          *HobStart;
  VOID                          *TeData;
  UINTN                         TeDataSize;
  EFI_PHYSICAL_ADDRESS          ImageBase;
  STMM_COMM_BUFFERS             *CommBuffersHob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfoHob;

  AsciiSPrint (
    Version,
    sizeof (Version),
    "%s (version %s)\r\n",
    (CHAR16 *)PcdGetPtr (PcdFirmwareFullNameString),
    (CHAR16 *)PcdGetPtr (PcdUefiVersionString)
    );
  DebugPrint (DEBUG_ERROR, Version);

  DEBUG ((DEBUG_ERROR, "EntryPoint: MemorySize=0x%lx DTB@0x%p\n", TotalSPMemorySize, DTBAddress));

  ConfigureStage1Translations (TotalSPMemorySize, DTBAddress);

  ZeroMem ((void *)&PayloadBootInfo, sizeof (EFI_SECURE_PARTITION_BOOT_INFO));
  ZeroMem ((void *)&StmmCommBuffers, sizeof (STMM_COMM_BUFFERS));

  /* Check Manifest */
  Status = CheckManifest (DTBAddress);
  if (EFI_ERROR (Status)) {
    goto finish;
  }

  // Get Secure Partition Manager Version Information
  Status = GetSpmVersion ();
  if (EFI_ERROR (Status)) {
    goto finish;
  }

  /* Locate PE/COFF File information for the Standalone MM core module */
  PayloadBootInfo.SpImageBase = GetSpImageBase (DTBAddress);
  PayloadBootInfo.SpImageSize = ((EFI_FIRMWARE_VOLUME_HEADER *)PayloadBootInfo.SpImageBase)->FvLength;
  PayloadBootInfo.SpImageSize = PAGE_ALIGN (PayloadBootInfo.SpImageSize + DEFAULT_PAGE_SIZE, DEFAULT_PAGE_SIZE);

  Status = LocateStandaloneMmCorePeCoffData (
             (EFI_FIRMWARE_VOLUME_HEADER *)PayloadBootInfo.SpImageBase,
             &TeData,
             &TeDataSize
             );
  if (EFI_ERROR (Status)) {
    goto finish;
  }

  /* Obtain the PE/COFF Section information for the Standalone MM core module */
  Status = GetStandaloneMmCorePeCoffSections (
             TeData,
             &ImageContext,
             &ImageBase,
             &SectionHeaderOffset,
             &NumberOfSections
             );

  if (EFI_ERROR (Status)) {
    goto finish;
  }

  /*
   * ImageBase may deviate from ImageContext.ImageAddress if we are dealing
   * with a TE image, in which case the latter points to the actual offset
   * of the image, whereas ImageBase refers to the address where the image
   * would start if the stripped PE headers were still in place. In either
   * case, we need to fix up ImageBase so it refers to the actual current
   * load address.
   */
  ImageBase += (UINTN)TeData - ImageContext.ImageAddress;

  /*
   * Update the memory access permissions of individual sections in the
   * Standalone MM core module
   */
  Status = UpdateMmFoundationPeCoffPermissions (
             &ImageContext,
             ImageBase,
             SectionHeaderOffset,
             NumberOfSections,
             ArmSetMemoryRegionNoExec,
             ArmSetMemoryRegionReadOnly,
             ArmClearMemoryRegionReadOnly
             );

  if (EFI_ERROR (Status)) {
    goto finish;
  }

  if (ImageContext.ImageAddress != (UINTN)TeData) {
    ImageContext.ImageAddress = (UINTN)TeData;
    ArmSetMemoryRegionNoExec (ImageBase, SIZE_4KB);
    ArmClearMemoryRegionReadOnly (ImageBase, SIZE_4KB);

    Status = PeCoffLoaderRelocateImage (&ImageContext);
    ASSERT_EFI_ERROR (Status);
  }

  /* Create Hoblist based upon boot information passed by Manifest */
  Status = GetAndPrintManifestinformation (DTBAddress, TotalSPMemorySize);
  if (EFI_ERROR (Status)) {
    Status = EFI_UNSUPPORTED;
    goto finish;
  }

  HobStart = CreateHobListFromBootInfo (&CpuDriverEntryPoint, &PayloadBootInfo);
  Status   = GetDeviceMemRegions (DTBAddress);
  if (EFI_ERROR (Status)) {
    // Not ideal, but not fatal, so continue.
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install Device Regions Hob %r\n",
      __FUNCTION__,
      Status
      ));
  }

  StmmCommBuffers.DTBAddress = (PHYSICAL_ADDRESS)DTBAddress;

  /* Guided HOB with the addresses of Manifest buffers */
  CommBuffersHob = (STMM_COMM_BUFFERS *)BuildGuidHob (
                                          &gNVIDIAStMMBuffersGuid,
                                          sizeof (STMM_COMM_BUFFERS)
                                          );
  CopyMem ((VOID *)CommBuffersHob, (VOID *)&StmmCommBuffers, sizeof (STMM_COMM_BUFFERS));

  PlatformResourceInfoHob = (TEGRA_PLATFORM_RESOURCE_INFO *)BuildGuidHob (
                                                              &gNVIDIAPlatformResourceDataGuid,
                                                              sizeof (TEGRA_PLATFORM_RESOURCE_INFO)
                                                              );

  Status = GetPlatformResourceInformationStandaloneMm (
             PlatformResourceInfoHob,
             StmmCommBuffers.CpuBlParamsAddr
             );

  /* Call the MM Core entry point */
  ProcessModuleEntryPointList (HobStart);

  DEBUG ((DEBUG_INFO, "Shared Cpu Driver EP 0x%lx\n", (UINT64)CpuDriverEntryPoint));

finish:
  if (Status == RETURN_UNSUPPORTED) {
    Ret = -1;
  } else if (Status == RETURN_INVALID_PARAMETER) {
    Ret = -2;
  } else if (Status == EFI_NOT_FOUND) {
    Ret = -7;
  } else {
    Ret = 0;
  }

  ZeroMem (&InitMmFoundationSvcArgs, sizeof (InitMmFoundationSvcArgs));
  InitArmSvcArgs (&InitMmFoundationSvcArgs, &Ret);
  DebugPrint (DEBUG_ERROR, "Boot Complete\n");
  DelegatedEventLoop (&InitMmFoundationSvcArgs);
}
