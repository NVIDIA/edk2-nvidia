/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <ArmMpidr.h>
#include <PiDxe.h>
#include <Library/FloorSweepingLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <libfdt.h>

#include "CommonFloorSweepingLib.h"

#define TH500_MAX_SOCKETS            4
#define MAX_CORE_DISABLE_WORDS       3
#define MAX_SCF_CACHE_DISABLE_WORDS  3

EFI_STATUS
EFIAPI
UpdateCpuFloorsweepingConfig (
  IN UINT32  SocketMask,
  IN INT32   CpusOffset,
  IN VOID    *Dtb
  );

STATIC UINT64  *SocketScratchBaseAddr;
STATIC UINT64  TH500SocketScratchBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_SCRATCH_BASE_SOCKET_0,
  TH500_SCRATCH_BASE_SOCKET_1,
  TH500_SCRATCH_BASE_SOCKET_2,
  TH500_SCRATCH_BASE_SOCKET_3,
};

STATIC UINT64  *SocketCbbFabricBaseAddr;
STATIC UINT64  TH500SocketCbbFabricBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_CBB_FABRIC_BASE_SOCKET_0,
  TH500_CBB_FABRIC_BASE_SOCKET_1,
  TH500_CBB_FABRIC_BASE_SOCKET_2,
  TH500_CBB_FABRIC_BASE_SOCKET_3,
};

STATIC UINT64  *SocketMssBaseAddr;
STATIC UINT64  TH500SocketMssBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_MSS_BASE_SOCKET_0,
  TH500_MSS_BASE_SOCKET_1,
  TH500_MSS_BASE_SOCKET_2,
  TH500_MSS_BASE_SOCKET_3,
};

STATIC UINT32  *ScfCacheDisableScratchOffset;
STATIC UINT32  TH500ScfCacheDisableScratchOffset[MAX_SCF_CACHE_DISABLE_WORDS] = {
  TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_0,
  TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_1,
  TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_2,
};

STATIC UINT32  *ScfCacheDisableScratchMask;
STATIC UINT32  TH500ScfCacheDisableScratchMask[MAX_SCF_CACHE_DISABLE_WORDS] = {
  TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_0,
  TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_1,
  TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_2,
};

STATIC CONST CHAR8  *PcieEpCompatibility;

/**
  Initialize global structures

**/
STATIC
EFI_STATUS
EFIAPI
CommonInitializeGlobalStructures (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       ChipId;

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case TH500_CHIP_ID:
      SocketScratchBaseAddr        = TH500SocketScratchBaseAddr;
      ScfCacheDisableScratchOffset = TH500ScfCacheDisableScratchOffset;
      ScfCacheDisableScratchMask   = TH500ScfCacheDisableScratchMask;
      SocketMssBaseAddr            = TH500SocketMssBaseAddr;
      SocketCbbFabricBaseAddr      = TH500SocketCbbFabricBaseAddr;
      PcieEpCompatibility          = NULL;
      Status                       = EFI_SUCCESS;

      break;

    default:
      Status = EFI_UNSUPPORTED;
      break;
  }

  return Status;
}

EFI_STATUS
EFIAPI
TH500UpdatePcieNode (
  IN UINT32  Socket,
  IN UINT32  PcieId,
  IN  VOID   *Dtb,
  IN INT32   NodeOffset
  )
{
  CONST VOID                    *Property;
  INT32                         Length, Ret;
  UINT32                        Tmp32;
  UINT32                        CtrlId;
  UINT64                        Aperture64Base;
  UINT64                        Aperture64Size;
  UINT32                        Aperture32Base;
  UINT32                        Aperture32Size;
  UINT64                        CbbFabricBase;
  UINT64                        CbbCtlOffset;
  UINT64                        EcamBase;
  UINT64                        EcamSize;
  UINT64                        NonPrefBase;
  UINT64                        NonPrefSize;
  UINT64                        PrefBase;
  UINT64                        PrefSize;
  UINT64                        IoBase;
  UINT64                        IoSize;
  UINT64                        MSSBase;
  UINT32                        C2CMode;
  INT32                         RPNodeOffset;
  VOID                          *Hob;
  TEGRABL_EARLY_BOOT_VARIABLES  *Mb1Config = NULL;

  CbbFabricBase = SocketCbbFabricBaseAddr[Socket];
  if (CbbFabricBase == 0) {
    return EFI_SUCCESS;
  }

  CtrlId         = PcieIdToInterface (TH500_CHIP_ID, PcieId);
  CbbCtlOffset   = CbbFabricBase + 0x20 * PcieIdToInterface (TH500_CHIP_ID, PcieId);
  Aperture64Base = (((UINT64)MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_HIGH)) << 32) |
                   MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_LOW);

  Aperture64Size = MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_SIZE);
  Aperture64Size = Aperture64Size << 16;

  Aperture32Base = (((UINT64)MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_32BIT_HIGH)) << 32) |
                   MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_32BIT_LOW);

  Aperture32Size = MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_32BIT_SIZE);
  Aperture32Size = Aperture32Size << 16;

  DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: 64-bit Aperture Base = 0x%llX\n", PcieId, Aperture64Base));
  DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: 64-bit Aperture Size = 0x%llX\n", PcieId, Aperture64Size));

  DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: 32-bit Aperture Base = 0x%llX\n", PcieId, Aperture32Base));
  DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: 32-bit Aperture Size = 0x%llX\n", PcieId, Aperture32Size));

  /*
   * +-----------------------------------------------------+
   * | 64-bit Aperture Usage                               |
   * +----------+------------------------------------------+
   * | 256 MB   | Reserved for VDM                         |
   * +----------+------------------------------------------+
   * | 256 MB   | ECAM                                     |
   * +----------+------------------------------------------+
   * | 512 MB   | RSVD (64K of this is used for I/O)       |
   * +----------+------------------------------------------+
   * | 2 GB     | Non-Prefetchable Region                  |
   * |          | (if 32-bit space is not used)            |
   * +----------+------------------------------------------+
   * | Rest all | Prefetchable Region                      |
   * +----------+------------------------------------------+
  */

  /* Patch ECAM Address in 'reg' property */
  Property = fdt_getprop (Dtb, NodeOffset, "reg", &Length);
  if ((Property == NULL) || (Length != sizeof (Tmp32) * 20)) {
    DEBUG ((DEBUG_ERROR, "Unexpected \"reg\" property. Length = %d\n", Length));
    return EFI_UNSUPPORTED;
  }

  PrefSize  = Aperture64Size;
  PrefSize -= TH500_VDM_SIZE;

  EcamBase  = Aperture64Base + TH500_VDM_SIZE;
  EcamSize  = TH500_ECAM_SIZE;
  PrefSize -= EcamSize;
  DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: ECAM Base = 0x%llX\n", PcieId, EcamBase));
  DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: ECAM Size = 0x%llX\n", PcieId, EcamSize));

  ((UINT32 *)Property)[16] = cpu_to_fdt32 (EcamBase >> 32);
  ((UINT32 *)Property)[17] = cpu_to_fdt32 (EcamBase);
  ((UINT32 *)Property)[18] = cpu_to_fdt32 (EcamSize >> 32);
  ((UINT32 *)Property)[19] = cpu_to_fdt32 (EcamSize);

  /* Patch 'ranges' property */
  Property = fdt_getprop (Dtb, NodeOffset, "ranges", &Length);
  if ((Property == NULL) ||
      ((Length != sizeof (Tmp32) * 21) &&
       (Length != sizeof (Tmp32) * 14)))
  {
    DEBUG ((DEBUG_ERROR, "Unexpected \"ranges\" property. Length = %d\n", Length));
    return EFI_UNSUPPORTED;
  }

  if (Aperture32Base != 0) {
    NonPrefBase = Aperture32Base;
    NonPrefSize = Aperture32Size;
    DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: Non-Prefetchable Base = 0x%llX\n", PcieId, NonPrefBase));
    DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: Non-Prefetchable Size = 0x%llX\n", PcieId, NonPrefSize));

    ((UINT32 *)Property)[0] = cpu_to_fdt32 (0x82000000);
    ((UINT32 *)Property)[1] = cpu_to_fdt32 (NonPrefBase >> 32);
    ((UINT32 *)Property)[2] = cpu_to_fdt32 (NonPrefBase);
    ((UINT32 *)Property)[3] = cpu_to_fdt32 (NonPrefBase >> 32);
    ((UINT32 *)Property)[4] = cpu_to_fdt32 (NonPrefBase);
    ((UINT32 *)Property)[5] = cpu_to_fdt32 (NonPrefSize >> 32);
    ((UINT32 *)Property)[6] = cpu_to_fdt32 (NonPrefSize);

    PrefBase  = EcamBase + EcamSize + 0x20000000;
    PrefSize -= 0x20000000;
  } else {
    NonPrefBase = EcamBase + EcamSize + 0x20000000;
    NonPrefSize = 0x80000000;         /* 2 GB fixed size */
    PrefSize   -= (NonPrefSize + 0x20000000);
    DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: Non-Prefetchable Base = 0x%llX\n", PcieId, NonPrefBase));
    DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: Non-Prefetchable Size = 0x%llX\n", PcieId, NonPrefSize));

    ((UINT32 *)Property)[1] = cpu_to_fdt32 (0x0);
    ((UINT32 *)Property)[2] = cpu_to_fdt32 (0x40000000);
    ((UINT32 *)Property)[3] = cpu_to_fdt32 (NonPrefBase >> 32);
    ((UINT32 *)Property)[4] = cpu_to_fdt32 (NonPrefBase);
    ((UINT32 *)Property)[5] = cpu_to_fdt32 (NonPrefSize >> 32);
    ((UINT32 *)Property)[6] = cpu_to_fdt32 (NonPrefSize);

    PrefBase = NonPrefBase + NonPrefSize;
  }

  DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: Prefetchable Base = 0x%llX\n", PcieId, PrefBase));
  DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: Prefetchable Size = 0x%llX\n", PcieId, PrefSize));

  ((UINT32 *)Property)[8]  = cpu_to_fdt32 (PrefBase >> 32);
  ((UINT32 *)Property)[9]  = cpu_to_fdt32 (PrefBase);
  ((UINT32 *)Property)[10] = cpu_to_fdt32 (PrefBase >> 32);
  ((UINT32 *)Property)[11] = cpu_to_fdt32 (PrefBase);
  ((UINT32 *)Property)[12] = cpu_to_fdt32 (PrefSize >> 32);
  ((UINT32 *)Property)[13] = cpu_to_fdt32 (PrefSize);

  if (Length == (sizeof (Tmp32) * 21)) {
    IoBase = EcamBase + EcamSize;
    IoSize = SIZE_64KB;       /* 64K fixed size I/O aperture */
    DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: IO Base = 0x%llX\n", PcieId, IoBase));
    DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: IO Size = 0x%llX\n", PcieId, IoSize));

    ((UINT32 *)Property)[15] = cpu_to_fdt32 (0x0);
    ((UINT32 *)Property)[16] = cpu_to_fdt32 (0x0);
    ((UINT32 *)Property)[17] = cpu_to_fdt32 (IoBase >> 32);
    ((UINT32 *)Property)[18] = cpu_to_fdt32 (IoBase);
    ((UINT32 *)Property)[19] = cpu_to_fdt32 (IoSize>> 32);
    ((UINT32 *)Property)[20] = cpu_to_fdt32 (IoSize);
  }

  /* Patch 'external-facing' property only for C8 controller */
  if ((SocketMssBaseAddr != NULL) && (PcieIdToInterface (TH500_CHIP_ID, PcieId) == 8)) {
    MSSBase  = SocketMssBaseAddr[Socket];
    C2CMode  = MmioRead32 (MSSBase + TH500_MSS_C2C_MODE);
    C2CMode &= 0x3;
    DEBUG ((DEBUG_INFO, "C2C Mode = %u\n", C2CMode));

    if (C2CMode == TH500_MSS_C2C_MODE_TWO_GPU) {
      RPNodeOffset = fdt_first_subnode (Dtb, NodeOffset);
      if (RPNodeOffset < 0) {
        DEBUG ((DEBUG_ERROR, "RP Sub-Node is not found. Can't patch 'external-facing' property\n"));
      } else {
        INTN  Err;
        Err = fdt_nop_property (Dtb, RPNodeOffset, "external-facing");
        if (0 != Err) {
          DEBUG ((
            DEBUG_ERROR,
            "Failed to delete 'external-facing' property for Ctrl = %d\n",
            PcieIdToInterface (TH500_CHIP_ID, PcieId)
            ));
        } else {
          DEBUG ((
            DEBUG_INFO,
            "Deleted 'external-facing' property for Ctrl = %d\n",
            PcieIdToInterface (TH500_CHIP_ID, PcieId)
            ));
        }
      }
    }
  }

  /* Add 'nvidia,socket-id' property */
  Ret = fdt_setprop_u32 (Dtb, NodeOffset, "nvidia,socket-id", Socket);
  if (Ret) {
    DEBUG ((DEBUG_ERROR, "Failed to add \"nvidia,socket-id\" property: %d\n", Ret));
    return EFI_UNSUPPORTED;
  }

  /* Add 'nvidia,controller-id' property */
  Ret = fdt_setprop_u32 (Dtb, NodeOffset, "nvidia,controller-id", CtrlId);
  if (Ret) {
    DEBUG ((DEBUG_ERROR, "Failed to add \"nvidia,controller-id\" property: %d\n", Ret));
    return EFI_UNSUPPORTED;
  }

  /* Patch 'linux,pci-domain' property from UEFI variables */
  Hob = GetFirstGuidHob (&gNVIDIATH500MB1DataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == (sizeof (TEGRABL_EARLY_BOOT_VARIABLES) * PcdGet32 (PcdTegraMaxSockets))))
  {
    Mb1Config = (TEGRABL_EARLY_BOOT_VARIABLES *)GET_GUID_HOB_DATA (Hob);
  }

  if (Mb1Config != NULL) {
    if ((Mb1Config->Data.Mb1Data.Header.MajorVersion == TEGRABL_MB1_BCT_MAJOR_VERSION) &&
        (Mb1Config->Data.Mb1Data.Header.MinorVersion >= 10))
    {
      Property = fdt_getprop (Dtb, NodeOffset, "linux,pci-domain", &Length);
      if ((Property == NULL) || (Length != sizeof (Tmp32))) {
        DEBUG ((DEBUG_ERROR, "Unexpected pcie property\n"));
        return EFI_UNSUPPORTED;
      }

      DEBUG ((
        DEBUG_INFO,
        "Patching 'linux,pci-domain' with = %x\n",
        Mb1Config->Data.Mb1Data.PcieConfig[Socket][CtrlId].Segment
        ));

      Ret = fdt_setprop_u32 (Dtb, NodeOffset, "linux,pci-domain", Mb1Config->Data.Mb1Data.PcieConfig[Socket][CtrlId].Segment);
      if (Ret) {
        DEBUG ((DEBUG_ERROR, "Failed to add \"linux,pci-domain\" property: %d\n", Ret));
        return EFI_UNSUPPORTED;
      }
    }
  } else {
    DEBUG ((DEBUG_WARN, "Failed to find UEFI early variables to patch \"linux,pci-domain\" property\n"));
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
GetDisableRegArray (
  IN UINT32   SocketMask,
  IN UINT64   SocketOffset,
  IN UINT64   DisableRegAddr,
  IN UINT32   DisableRegMask,
  OUT UINT32  *DisableRegArray
  )
{
  UINTN   Socket;
  UINTN   DisableReg;
  UINT64  SocketBase;

  SocketBase = 0;
  for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++, SocketBase += SocketOffset) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    DisableReg  = MmioRead32 (SocketBase + DisableRegAddr);
    DisableReg &= DisableRegMask;

    DisableRegArray[Socket] = DisableReg;

    DEBUG ((DEBUG_INFO, "%a: Socket %u Addr=0x%llx Reg=0x%x\n", __FUNCTION__, Socket, SocketBase + DisableRegAddr, DisableReg));
  }

  return EFI_SUCCESS;
}

/**
  Floorsweep PCIe

**/
EFI_STATUS
EFIAPI
CommonFloorSweepPcie (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  )
{
  EFI_STATUS           Status;
  INT32                ParentOffset;
  INT32                NodeOffset;
  CHAR8                ParentNameStr[16];
  TEGRA_PLATFORM_TYPE  Platform;
  UINTN                ChipId;
  CHAR8                *ParentNameFormat;
  UINT32               InterfaceSocket;
  INT32                FdtErr;
  UINT32               PcieDisableRegArray[MAX_SUPPORTED_SOCKETS];
  UINTN                Index;
  UINTN                NumParentNodes;

  Status = CommonInitializeGlobalStructures ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ChipId   = TegraGetChipID ();
  Platform = TegraGetPlatform ();

  switch (ChipId) {
    case TH500_CHIP_ID:
      Status = GetDisableRegArray (
                 SocketMask,
                 (1ULL << TH500_SOCKET_SHFT),
                 TH500_SCRATCH_BASE_SOCKET_0 + TH500_PCIE_FLOORSWEEPING_DISABLE_OFFSET,
                 ~TH500_PCIE_FLOORSWEEPING_DISABLE_MASK,
                 PcieDisableRegArray
                 );

      if (Platform == TEGRA_PLATFORM_VDK) {
        PcieDisableRegArray[0] = TH500_PCIE_SIM_FLOORSWEEPING_INFO;
      } else if (Platform == TEGRA_PLATFORM_SYSTEM_FPGA) {
        PcieDisableRegArray[0] = TH500_PCIE_FPGA_FLOORSWEEPING_INFO;
      }

      ParentNameFormat = "/socket@%u";
      NumParentNodes   = PLATFORM_MAX_SOCKETS;
      break;

    default:
      return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < NumParentNodes; Index++) {
    ASSERT (AsciiStrLen (ParentNameFormat) < sizeof (ParentNameStr));
    AsciiSPrint (ParentNameStr, sizeof (ParentNameStr), ParentNameFormat, Index);
    ParentOffset = fdt_path_offset (Dtb, ParentNameStr);
    if (ParentOffset < 0) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find %a\n", __FUNCTION__, ParentNameStr));
      continue;
    }

    fdt_for_each_subnode (NodeOffset, Dtb, ParentOffset) {
      CONST VOID  *Property;
      INT32       Length;
      UINT32      Tmp32;
      UINT32      PcieId;

      Property = fdt_getprop (Dtb, NodeOffset, "device_type", &Length);
      if ((Property == NULL) || (AsciiStrCmp (Property, "pci") != 0)) {
        // not a RP node - check for EP node, if supported
        if (PcieEpCompatibility == NULL) {
          continue;
        } else {
          Property = fdt_getprop (Dtb, NodeOffset, "compatible", &Length);
          if ((Property == NULL) || (AsciiStrCmp (Property, PcieEpCompatibility) != 0)) {
            continue;
          }
        }
      }

      Property = fdt_getprop (Dtb, NodeOffset, "linux,pci-domain", &Length);
      if ((Property == NULL) || (Length != sizeof (Tmp32))) {
        DEBUG ((DEBUG_ERROR, "Invalid pci-domain for %a, skipping\n", fdt_get_name (Dtb, NodeOffset, NULL)));
        continue;
      }

      Tmp32  = *(CONST UINT32 *)Property;
      PcieId = fdt32_to_cpu (Tmp32);
      DEBUG ((DEBUG_INFO, "Found pcie 0x%x (%a)\n", PcieId, fdt_get_name (Dtb, NodeOffset, NULL)));

      InterfaceSocket = PcieIdToSocket (ChipId, PcieId);
      if (!(SocketMask & (1UL << InterfaceSocket)) ||
          ((PcieDisableRegArray[InterfaceSocket] & (1UL << PcieIdToInterface (ChipId, PcieId))) != 0))
      {
        FdtErr = fdt_setprop (Dtb, NodeOffset, "status", "disabled", sizeof ("disabled"));
        if (FdtErr < 0) {
          DEBUG ((DEBUG_ERROR, "Failed to disable PcieId=0x%x node: %a\n", PcieId, fdt_strerror (FdtErr)));
          return EFI_DEVICE_ERROR;
        }

        DEBUG ((DEBUG_INFO, "%a: Disabled PcieId=0x%x reg=0x%x mask=0x%x\n", __FUNCTION__, PcieId, PcieDisableRegArray[InterfaceSocket], SocketMask));
        continue;
      }

      if (ChipId == TH500_CHIP_ID) {
        Status = TH500UpdatePcieNode (InterfaceSocket, PcieId, Dtb, NodeOffset);
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }
    }
  }

  return EFI_SUCCESS;
}

STATIC
UINTN
EFIAPI
BitsSet (
  IN  UINT32  Word
  )
{
  UINTN   Index;
  UINTN   BitsSet;
  UINT32  Bit;

  BitsSet = 0;
  Bit     = 1;
  for (Index = 0; Index < 32; Index++, Bit <<= 1) {
    if ((Word & Bit) != 0) {
      BitsSet++;
    }
  }

  return BitsSet;
}

/**
  Floorsweep ScfCache

**/
EFI_STATUS
EFIAPI
CommonFloorSweepScfCache (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  )
{
  EFI_STATUS  Status;
  UINTN       Socket;
  UINTN       CoresPerSocket;
  UINTN       ScfCacheCount;
  UINT32      ScfCacheSize;
  UINT32      ScfCacheSets;
  INT32       NodeOffset;
  INT32       FdtErr;
  UINT32      Tmp32;
  CHAR8       SocketNodeStr[] = "/socket@xxxxxxxxxxx";

  Status = CommonInitializeGlobalStructures ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (ScfCacheDisableScratchOffset == NULL) {
    // SCF floorsweeping is not supported
    return EFI_SUCCESS;
  }

  CoresPerSocket = ((PLATFORM_MAX_CLUSTERS * PLATFORM_MAX_CORES_PER_CLUSTER) /
                    PLATFORM_MAX_SOCKETS);

  // SCF Cache is distributed as l3-cache over all possible sockets
  for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    // total number of scf cache elements per socket is same as CPU cores
    ScfCacheCount = CoresPerSocket;
    UINT64  ScratchBase = SocketScratchBaseAddr[Socket];
    UINTN   ScfScratchWord;

    if (ScratchBase == 0) {
      continue;
    }

    for (ScfScratchWord = 0;
         ScfScratchWord < MAX_SCF_CACHE_DISABLE_WORDS;
         ScfScratchWord++)
    {
      UINT32  DisableScratchReg;

      DisableScratchReg  = MmioRead32 (ScratchBase + ScfCacheDisableScratchOffset[ScfScratchWord]);
      DisableScratchReg |= ScfCacheDisableScratchMask[ScfScratchWord];
      DisableScratchReg &= ~ScfCacheDisableScratchMask[ScfScratchWord];
      ScfCacheCount     -= BitsSet (DisableScratchReg);
    }

    ScfCacheSize = ScfCacheCount * SCF_CACHE_SLICE_SIZE;
    ScfCacheSets = ScfCacheCount * SCF_CACHE_SLICE_SETS;

    DEBUG ((
      DEBUG_INFO,
      "%a: Socket = %u, ScfCacheCount=%u, ScfCacheSize=%u, ScfCacheSets=%u\n",
      __FUNCTION__,
      Socket,
      ScfCacheCount,
      ScfCacheSize,
      ScfCacheSets
      ));

    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u/l3-cache", Socket);
    NodeOffset = fdt_path_offset (Dtb, SocketNodeStr);
    if (NodeOffset < 0) {
      // Attempt to use the older DTB path if the updated DTB path doesn't work
      AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u/l3cache", Socket);
      NodeOffset = fdt_path_offset (Dtb, SocketNodeStr);
    }

    if (NodeOffset < 0) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find /socket@%u/l3-cache subnode\n", __FUNCTION__, Socket));
      return EFI_DEVICE_ERROR;
    }

    Tmp32  = cpu_to_fdt32 (ScfCacheSize);
    FdtErr = fdt_setprop (Dtb, NodeOffset, "cache-size", &Tmp32, sizeof (Tmp32));
    if (FdtErr < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to set Socket %u l3-cache cache-size: %a\n",
        Socket,
        fdt_strerror (FdtErr)
        ));
      return EFI_DEVICE_ERROR;
    }

    Tmp32  = cpu_to_fdt32 (ScfCacheSets);
    FdtErr = fdt_setprop (Dtb, NodeOffset, "cache-sets", &Tmp32, sizeof (Tmp32));
    if (FdtErr < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to set Socket %u l3-cache cache-sets: %a\n",
        Socket,
        fdt_strerror (FdtErr)
        ));
      return EFI_DEVICE_ERROR;
    }
  }

  return EFI_SUCCESS;
}

/**
  Floorsweep Cpus

**/
EFI_STATUS
EFIAPI
CommonFloorSweepCpus (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  )
{
  CHAR8  CpusStr[] = "/cpus";
  INT32  CpusOffset;

  CpusOffset = fdt_path_offset (Dtb, CpusStr);
  if (CpusOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find %a subnode\n", CpusStr));
    return EFI_DEVICE_ERROR;
  }

  return UpdateCpuFloorsweepingConfig (SocketMask, CpusOffset, Dtb);
}

EFI_STATUS
EFIAPI
TH500FloorSweepCpus (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  )
{
  UINTN       Socket;
  CHAR8       SocketCpusStr[] = "/socket@00/cpus";
  CHAR8       CpusStr[]       = "/cpus";
  EFI_STATUS  Status;

  Status = EFI_UNSUPPORTED;
  for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
    INT32  CpusOffset;

    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    AsciiSPrint (SocketCpusStr, sizeof (SocketCpusStr), "/socket@%u/cpus", Socket);
    CpusOffset = fdt_path_offset (Dtb, SocketCpusStr);
    if (CpusOffset < 0) {
      if (Socket == 0) {
        CpusOffset = fdt_path_offset (Dtb, CpusStr);
        if (CpusOffset < 0) {
          DEBUG ((DEBUG_ERROR, "Failed to find %a subnode\n", CpusStr));
          return EFI_DEVICE_ERROR;
        }
      } else {
        DEBUG ((DEBUG_ERROR, "Failed to find %a subnode\n", SocketCpusStr));
        return EFI_DEVICE_ERROR;
      }
    } else {
      DEBUG ((DEBUG_INFO, "Floorsweeping cpus in %a\n", SocketCpusStr));
    }

    Status = UpdateCpuFloorsweepingConfig ((1UL << Socket), CpusOffset, Dtb);
    if (EFI_ERROR (Status)) {
      break;
    }
  }

  return Status;
}

UINT32
EFIAPI
GetLinearCoreIDFromMpidr (
  IN UINT64  Mpidr
  )
{
  UINTN   Cluster;
  UINTN   Core;
  UINTN   Socket;
  UINT32  LinearCoreId;

  Socket = MPIDR_AFFLVL3_VAL (Mpidr);
  ASSERT (Socket < PLATFORM_MAX_SOCKETS);

  Cluster = MPIDR_AFFLVL2_VAL (Mpidr);
  ASSERT (Cluster < PLATFORM_MAX_CLUSTERS);

  Core = MPIDR_AFFLVL1_VAL (Mpidr);
  ASSERT (Core < PLATFORM_MAX_CORES_PER_CLUSTER);

  LinearCoreId =
    (Socket * PLATFORM_MAX_CORES_PER_SOCKET) +
    (Cluster * PLATFORM_MAX_CORES_PER_CLUSTER) +
    Core;

  DEBUG ((
    DEBUG_INFO,
    "%a: Mpidr=0x%llx Socket=%u Cluster=%u, Core=%u, LinearCoreId=%u\n",
    __FUNCTION__,
    Mpidr,
    Socket,
    Cluster,
    Core,
    LinearCoreId
    ));

  return LinearCoreId;
}

EFI_STATUS
EFIAPI
CommonCheckAndRemapCpu (
  IN UINT32      LogicalCore,
  IN OUT UINT64  *Mpidr
  )
{
  EFI_STATUS  Status;
  UINT32      LinearCoreId;

  LinearCoreId = GetLinearCoreIDFromMpidr (*Mpidr);
  if (IsCoreEnabled (LinearCoreId)) {
    Status = EFI_SUCCESS;
  } else {
    Status = EFI_UNSUPPORTED;
  }

  return Status;
}
