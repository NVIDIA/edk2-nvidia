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
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

#include "CommonFloorSweepingLib.h"

#define TH500_MAX_SOCKETS            4
#define MAX_CORE_DISABLE_WORDS       3
#define MAX_SCF_CACHE_DISABLE_WORDS  3

EFI_STATUS
EFIAPI
UpdateCpuFloorsweepingConfig (
  IN UINTN  Socket,
  IN INT32  CpusOffset,
  IN VOID   *Dtb
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
      Status                       = EFI_SUCCESS;

      break;

    default:
      Status = EFI_UNSUPPORTED;
      break;
  }

  return Status;
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
  UINTN                Socket;
  INT32                ParentOffset;
  INT32                NodeOffset;
  INT32                TmpOffset;
  CHAR8                SocketStr[] = "/socket@00";
  TEGRA_PLATFORM_TYPE  Platform;
  UINTN                ChipId;

  Status = CommonInitializeGlobalStructures ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ChipId   = TegraGetChipID ();
  Platform = TegraGetPlatform ();

  for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
    UINT32  PcieDisableReg;
    UINT64  ScratchBase;

    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    ScratchBase = SocketScratchBaseAddr[Socket];
    if (ScratchBase == 0) {
      continue;
    }

    switch (ChipId) {
      case TH500_CHIP_ID:
        if (Platform == TEGRA_PLATFORM_VDK) {
          PcieDisableReg = TH500_PCIE_SIM_FLOORSWEEPING_INFO;
        } else if (Platform == TEGRA_PLATFORM_SYSTEM_FPGA) {
          PcieDisableReg = TH500_PCIE_FPGA_FLOORSWEEPING_INFO;
        } else {
          PcieDisableReg = MmioRead32 (ScratchBase + TH500_PCIE_FLOORSWEEPING_DISABLE_OFFSET);
        }

        PcieDisableReg |= TH500_PCIE_FLOORSWEEPING_DISABLE_MASK;
        PcieDisableReg &= ~TH500_PCIE_FLOORSWEEPING_DISABLE_MASK;
        break;

      default:
        return EFI_UNSUPPORTED;
    }

    DEBUG ((
      DEBUG_INFO,
      "Socket %u PcieDisableReg=0x%x\n",
      Socket,
      PcieDisableReg
      ));

    AsciiSPrint (SocketStr, sizeof (SocketStr), "/socket@%u", Socket);
    ParentOffset = fdt_path_offset (Dtb, SocketStr);
    if (ParentOffset < 0) {
      if (Socket == 0) {
        ParentOffset = 0;
      } else {
        DEBUG ((DEBUG_ERROR, "Failed to find %a subnode\n", SocketStr));
        return EFI_DEVICE_ERROR;
      }
    }

    NodeOffset = fdt_first_subnode (Dtb, ParentOffset);
    while (NodeOffset > 0) {
      CONST VOID  *Property;
      INT32       Length, Ret;
      UINT32      Tmp32;
      UINT32      PcieId, CtrlId;

      Property = fdt_getprop (Dtb, NodeOffset, "device_type", &Length);
      if ((Property == NULL) || (AsciiStrCmp (Property, "pci") != 0)) {
        NodeOffset = fdt_next_subnode (Dtb, NodeOffset);
        continue;
      }

      Property = fdt_getprop (Dtb, NodeOffset, "linux,pci-domain", &Length);
      if ((Property == NULL) || (Length != sizeof (Tmp32))) {
        DEBUG ((DEBUG_ERROR, "Unexpected pcie property\n"));
        return EFI_UNSUPPORTED;
      }

      Tmp32  = *(CONST UINT32 *)Property;
      PcieId = fdt32_to_cpu (Tmp32);
      DEBUG ((
        DEBUG_INFO,
        "Found pcie 0x%x (%a)\n",
        PcieId,
        fdt_get_name (Dtb, NodeOffset, NULL)
        ));
      ASSERT (PCIE_ID_TO_SOCKET (PcieId) == Socket);
      CtrlId = PCIE_ID_TO_INTERFACE (PcieId);

      if ((PcieDisableReg & (1UL << PCIE_ID_TO_INTERFACE (PcieId))) != 0) {
        INT32  FdtErr;

        TmpOffset  = NodeOffset;
        NodeOffset = fdt_next_subnode (Dtb, NodeOffset);

        FdtErr = fdt_nop_node (Dtb, TmpOffset);
        if (FdtErr < 0) {
          DEBUG ((
            DEBUG_ERROR,
            "Failed to delete PcieId=0x%x node: %a\n",
            PcieId,
            fdt_strerror (FdtErr)
            ));
          return EFI_DEVICE_ERROR;
        }

        DEBUG ((DEBUG_INFO, "Deleted PcieId=0x%x node\n", PcieId));
      } else {
        /* Patching PCIe DT node */
        UINT64                        Aperture64Base;
        UINT64                        Aperture64Size;
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
          continue;
        }

        switch (ChipId) {
          case TH500_CHIP_ID:
            CbbCtlOffset   = CbbFabricBase + 0x20 * PCIE_ID_TO_INTERFACE (PcieId);
            Aperture64Base = (((UINT64)MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_HIGH)) << 32) |
                             MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_LOW);

            Aperture64Size = MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_SIZE);
            Aperture64Size = Aperture64Size << 16;
            break;

          default:
            return EFI_UNSUPPORTED;
        }

        DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: 64-bit Aperture Base = 0x%llX\n", PcieId, Aperture64Base));
        DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: 64-bit Aperture Size = 0x%llX\n", PcieId, Aperture64Size));

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

        NonPrefBase = EcamBase + EcamSize + 0x20000000;
        NonPrefSize = 0x80000000;   /* 2 GB fixed size */
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
          IoSize = SIZE_64KB; /* 64K fixed size I/O aperture */
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
        if ((SocketMssBaseAddr != NULL) &&
            (PCIE_ID_TO_INTERFACE (PcieId) == 8))
        {
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
                  PCIE_ID_TO_INTERFACE (PcieId)
                  ));
              } else {
                DEBUG ((
                  DEBUG_INFO,
                  "Deleted 'external-facing' property for Ctrl = %d\n",
                  PCIE_ID_TO_INTERFACE (PcieId)
                  ));
              }
            }
          }
        }

        /* Add 'nvidia,socket-id' property */
        Ret = fdt_appendprop_u32 (Dtb, NodeOffset, "nvidia,socket-id", Socket);
        if (Ret) {
          DEBUG ((DEBUG_ERROR, "Failed to add \"nvidia,socket-id\" property: %d\n", Ret));
          return EFI_UNSUPPORTED;
        }

        /* Add 'nvidia,controller-id' property */
        Ret = fdt_appendprop_u32 (Dtb, NodeOffset, "nvidia,controller-id", CtrlId);
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
            *(UINT32 *)Property = cpu_to_fdt32 ((UINT32)(Mb1Config->Data.Mb1Data.PcieConfig[Socket][CtrlId].Segment));
          }
        } else {
          DEBUG ((DEBUG_WARN, "Failed to find UEFI early variables to patch \"linux,pci-domain\" property\n"));
        }

        NodeOffset = fdt_next_subnode (Dtb, NodeOffset);
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

    Status = UpdateCpuFloorsweepingConfig (Socket, CpusOffset, Dtb);
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
