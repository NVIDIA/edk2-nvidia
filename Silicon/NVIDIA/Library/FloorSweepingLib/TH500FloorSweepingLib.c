/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <ArmMpidr.h>
#include <Library/FloorSweepingLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

#include "TH500FloorSweepingLib.h"

#define TH500_MAX_SOCKETS                  4
#define TH500_MAX_CORE_DISABLE_WORDS       3
#define TH500_MAX_SCF_CACHE_DISABLE_WORDS  3

EFI_STATUS
EFIAPI
UpdateCpuFloorsweepingConfig (
  IN INT32  CpusOffset,
  IN VOID   *Dtb
  );

UINT64  SocketScratchBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_SCRATCH_BASE_SOCKET_0,
  TH500_SCRATCH_BASE_SOCKET_1,
  TH500_SCRATCH_BASE_SOCKET_2,
  TH500_SCRATCH_BASE_SOCKET_3,
};

UINT64  SocketCbbFabricBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_CBB_FABRIC_BASE_SOCKET_0,
  TH500_CBB_FABRIC_BASE_SOCKET_1,
  TH500_CBB_FABRIC_BASE_SOCKET_2,
  TH500_CBB_FABRIC_BASE_SOCKET_3,
};

UINT64  SocketMssBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_MSS_BASE_SOCKET_0,
  TH500_MSS_BASE_SOCKET_1,
  TH500_MSS_BASE_SOCKET_2,
  TH500_MSS_BASE_SOCKET_3,
};

UINT32  CoreDisableScratchOffset[TH500_MAX_CORE_DISABLE_WORDS] = {
  CPU_FLOORSWEEPING_DISABLE_OFFSET_0,
  CPU_FLOORSWEEPING_DISABLE_OFFSET_1,
  CPU_FLOORSWEEPING_DISABLE_OFFSET_2,
};

UINT32  CoreDisableScratchMask[TH500_MAX_CORE_DISABLE_WORDS] = {
  CPU_FLOORSWEEPING_DISABLE_MASK_0,
  CPU_FLOORSWEEPING_DISABLE_MASK_1,
  CPU_FLOORSWEEPING_DISABLE_MASK_2,
};

UINT32  ScfCacheDisableScratchOffset[TH500_MAX_SCF_CACHE_DISABLE_WORDS] = {
  SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_0,
  SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_1,
  SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_2,
};

UINT32  ScfCacheDisableScratchMask[TH500_MAX_SCF_CACHE_DISABLE_WORDS] = {
  SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_0,
  SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_1,
  SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_2,
};

/**
  Add one socket's enabled cores bit map array to the EnabledCoresBitMap

**/
STATIC
VOID
EFIAPI
AddSocketCoresToEnabledCoresBitMap (
  IN  UINTN   SocketNumber,
  IN  UINT32  *SocketCores,
  IN  UINTN   MaxSupportedCores,
  IN  UINT64  *EnabledCoresBitMap
  )
{
  UINTN  CoresPerSocket;
  UINTN  SocketStartingCore;
  UINTN  EnabledCoresBit;
  UINTN  EnabledCoresIndex;
  UINTN  SocketCoresBit;
  UINTN  SocketCoresIndex;
  UINTN  Core;

  CoresPerSocket     = (PLATFORM_MAX_CLUSTERS * PLATFORM_MAX_CORES_PER_CLUSTER) / PLATFORM_MAX_SOCKETS;
  SocketStartingCore = CoresPerSocket * SocketNumber;

  ASSERT ((SocketStartingCore + CoresPerSocket) <= MaxSupportedCores);
  ASSERT ((ALIGN_VALUE (CoresPerSocket, 32) / 32) <= TH500_MAX_CORE_DISABLE_WORDS);

  for (Core = 0; Core < CoresPerSocket; Core++) {
    SocketCoresIndex = Core / 32;
    SocketCoresBit   = Core % 32;

    EnabledCoresIndex = (Core + SocketStartingCore) / 64;
    EnabledCoresBit   = (Core + SocketStartingCore) % 64;

    EnabledCoresBitMap[EnabledCoresIndex] |=
      (SocketCores[SocketCoresIndex] & (1UL << SocketCoresBit)) ?
      (1ULL << EnabledCoresBit) : 0;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Socket %d cores 0x%x 0x%x 0x%x added as EnabledCores bits %u-%u\n",
    __FUNCTION__,
    SocketNumber,
    SocketCores[2],
    SocketCores[1],
    SocketCores[0],
    SocketStartingCore + CoresPerSocket - 1,
    SocketStartingCore
    ));
}

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
TH500GetEnabledCoresBitMap (
  IN  UINT32  SocketMask,
  IN  UINTN   MaxSupportedCores,
  IN  UINT64  *EnabledCoresBitMap
  )
{
  UINT32  ScratchDisable0Reg;
  UINT32  ScratchDisable1Reg;
  UINT32  ScratchDisable2Reg;
  UINT32  SatMcCore;
  UINT32  CoresPerSocket;
  UINT32  EnaBitMap[TH500_MAX_CORE_DISABLE_WORDS];
  UINTN   Socket;

  // SatMC core is reserved on socket 0.
  CoresPerSocket = (PLATFORM_MAX_CLUSTERS * PLATFORM_MAX_CORES_PER_CLUSTER) / PLATFORM_MAX_SOCKETS;
  SatMcCore      = MmioBitFieldRead32 (
                     SocketScratchBaseAddr[0] + CoreDisableScratchOffset[2],
                     CPU_FLOORSWEEPING_SATMC_CORE_BIT_LO,
                     CPU_FLOORSWEEPING_SATMC_CORE_BIT_HI
                     );
  if (SatMcCore != CPU_FLOORSWEEPING_SATMC_CORE_INVALID) {
    ASSERT (SatMcCore <= CoresPerSocket);
  }

  for (Socket = 0; Socket < TH500_MAX_SOCKETS; Socket++) {
    UINT64  ScratchBase = SocketScratchBaseAddr[Socket];

    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    ScratchDisable0Reg = MmioRead32 (ScratchBase + CoreDisableScratchOffset[0]);
    ScratchDisable1Reg = MmioRead32 (ScratchBase + CoreDisableScratchOffset[1]);
    ScratchDisable2Reg = MmioRead32 (ScratchBase + CoreDisableScratchOffset[2]);

    if ((SatMcCore != CPU_FLOORSWEEPING_SATMC_CORE_INVALID) &&
        (Socket == 0))
    {
      DEBUG ((DEBUG_ERROR, "%a: Mask core %d on socket 0 for SatMC\n", __FUNCTION__, SatMcCore));
      if (SatMcCore < 32) {
        ScratchDisable0Reg |= (1U << SatMcCore);
      } else if (SatMcCore < 64) {
        ScratchDisable1Reg |= (1U << (SatMcCore - 32));
      } else if (SatMcCore < CoresPerSocket) {
        ScratchDisable2Reg |= (1U << (SatMcCore - 64));
      }
    }

    ScratchDisable0Reg |= CoreDisableScratchMask[0];
    ScratchDisable1Reg |= CoreDisableScratchMask[1];
    ScratchDisable2Reg |= CoreDisableScratchMask[2];

    ScratchDisable0Reg &= ~CoreDisableScratchMask[0];
    ScratchDisable1Reg &= ~CoreDisableScratchMask[1];
    ScratchDisable2Reg &= ~CoreDisableScratchMask[2];

    EnaBitMap[0] = ~ScratchDisable0Reg;
    EnaBitMap[1] = ~ScratchDisable1Reg;
    EnaBitMap[2] = ~ScratchDisable2Reg;

    AddSocketCoresToEnabledCoresBitMap (
      Socket,
      EnaBitMap,
      MaxSupportedCores,
      EnabledCoresBitMap
      );
  }

  return EFI_SUCCESS;
}

/**
  Floorsweep PCIe

**/
EFI_STATUS
EFIAPI
TH500FloorSweepPcie (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  )
{
  UINTN                Socket;
  INT32                ParentOffset;
  INT32                NodeOffset;
  INT32                PrevNodeOffset;
  CHAR8                SocketStr[] = "/socket@00";
  TEGRA_PLATFORM_TYPE  Platform;

  for (Socket = 0; Socket < TH500_MAX_SOCKETS; Socket++) {
    UINT32  PcieDisableReg;
    UINT64  ScratchBase;

    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    ScratchBase = SocketScratchBaseAddr[Socket];
    Platform    = TegraGetPlatform ();
    if (Platform == TEGRA_PLATFORM_VDK) {
      PcieDisableReg = PCIE_SIM_FLOORSWEEPING_INFO;
    } else if (Platform == TEGRA_PLATFORM_SYSTEM_FPGA) {
      PcieDisableReg = PCIE_FPGA_FLOORSWEEPING_INFO;
    } else {
      PcieDisableReg = MmioRead32 (ScratchBase + PCIE_FLOORSWEEPING_DISABLE_OFFSET);
    }

    PcieDisableReg |= PCIE_FLOORSWEEPING_DISABLE_MASK;
    PcieDisableReg &= ~PCIE_FLOORSWEEPING_DISABLE_MASK;
    DEBUG ((
      DEBUG_INFO,
      "Socket %u PcieDisableReg=0x%x\n",
      Socket,
      PcieDisableReg
      ));

    AsciiSPrint (SocketStr, sizeof (SocketStr), "/socket@%u", Socket);
    ParentOffset = fdt_path_offset (Dtb, SocketStr);
    if (ParentOffset < 0) {
      DEBUG ((DEBUG_ERROR, "Failed to find %a subnode\n", SocketStr));
      return EFI_DEVICE_ERROR;
    }

    for (NodeOffset = fdt_first_subnode (Dtb, ParentOffset);
         NodeOffset > 0;
         NodeOffset = fdt_next_subnode (Dtb, PrevNodeOffset))
    {
      CONST VOID  *Property;
      INT32       Length;
      UINT32      Tmp32;
      UINT32      PcieId;

      Property = fdt_getprop (Dtb, NodeOffset, "device_type", &Length);
      if ((Property == NULL) || (AsciiStrCmp (Property, "pci") != 0)) {
        PrevNodeOffset = NodeOffset;
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

      if ((PcieDisableReg & (1UL << PCIE_ID_TO_INTERFACE (PcieId))) != 0) {
        INT32  FdtErr;

        FdtErr = fdt_del_node (Dtb, NodeOffset);
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
        UINT64  Aperture64Base;
        UINT64  Aperture64Size;
        UINT64  CbbFabricBase;
        UINT64  CbbCtlOffset;
        UINT64  EcamBase;
        UINT64  EcamSize;
        UINT64  NonPrefBase;
        UINT64  NonPrefSize;
        UINT64  PrefBase;
        UINT64  PrefSize;
        UINT64  IoBase;
        UINT64  IoSize;
        UINT64  MSSBase;
        UINT32  C2CMode;
        INT32   RPNodeOffset;

        CbbFabricBase  = SocketCbbFabricBaseAddr[Socket];
        CbbCtlOffset   = CbbFabricBase + 0x20 * PCIE_ID_TO_INTERFACE (PcieId);
        Aperture64Base = (((UINT64)MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_HIGH)) << 32) |
                         MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_LOW);
        DEBUG ((DEBUG_INFO, "PCIE_SEG[0x%X]: 64-bit Aperture Base = 0x%llX\n", PcieId, Aperture64Base));

        Aperture64Size = MmioRead32 (CbbCtlOffset + TH500_CBB_FABRIC_64BIT_SIZE);
        Aperture64Size = Aperture64Size << 16;
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
        if ((Property == NULL) || (Length != sizeof (Tmp32) * 21)) {
          DEBUG ((DEBUG_ERROR, "Unexpected \"ranges\" property. Length = %d\n", Length));
          return EFI_UNSUPPORTED;
        }

        NonPrefBase = EcamBase + EcamSize + 0x20000000;
        NonPrefSize = 0x80000000;  /* 2 GB fixed size */
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

        /* Patch 'external-facing' property only for C8 controller */
        if (PCIE_ID_TO_INTERFACE (PcieId) == 8) {
          MSSBase  = SocketMssBaseAddr[Socket];
          C2CMode  = MmioRead32 (MSSBase + TH500_MSS_C2C_MODE);
          C2CMode &= 0x3;
          DEBUG ((DEBUG_INFO, "C2C Mode = %d\n", C2CMode));

          if (C2CMode == TH500_MSS_C2C_MODE_TWO_GPU) {
            RPNodeOffset = fdt_first_subnode (Dtb, NodeOffset);
            if (RPNodeOffset < 0) {
              DEBUG ((DEBUG_ERROR, "RP Sub-Node is not found. Can't patch 'external-facing' property\n"));
            } else {
              INTN  Err;
              Err = fdt_delprop (Dtb, RPNodeOffset, "external-facing");
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

        PrevNodeOffset = NodeOffset;
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
TH500FloorSweepScfCache (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  )
{
  UINTN   Socket;
  UINTN   CoresPerSocket;
  UINTN   ScfCacheCount;
  UINT32  ScfCacheSize;
  UINT32  ScfCacheSets;
  INT32   NodeOffset;
  INT32   FdtErr;
  UINT32  Tmp32;
  CHAR8   SocketNodeStr[] = "/socket@xxxxxxxxxx";

  CoresPerSocket = ((PLATFORM_MAX_CLUSTERS * PLATFORM_MAX_CORES_PER_CLUSTER) /
                    PLATFORM_MAX_SOCKETS);

  // SCF Cache is distributed as l3cache over all possible sockets
  for (Socket = 0; Socket < TH500_MAX_SOCKETS; Socket++) {
    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    // total number of scf cache elements per socket is same as CPU cores
    ScfCacheCount = CoresPerSocket;
    UINT64  ScratchBase = SocketScratchBaseAddr[Socket];
    UINTN   ScfScratchWord;

    for (ScfScratchWord = 0;
         ScfScratchWord < TH500_MAX_SCF_CACHE_DISABLE_WORDS;
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

    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u/l3cache", Socket);
    NodeOffset = fdt_path_offset (Dtb, SocketNodeStr);

    if (NodeOffset < 0) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find /socket@%u/l3cache subnode\n", __FUNCTION__, Socket));
      return EFI_DEVICE_ERROR;
    }

    Tmp32  = cpu_to_fdt32 (ScfCacheSize);
    FdtErr = fdt_setprop (Dtb, NodeOffset, "cache-size", &Tmp32, sizeof (Tmp32));
    if (FdtErr < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to set Socket %u l3cache cache-size: %a\n",
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
        "Failed to set Socket %u l3cache cache-sets: %a\n",
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
TH500FloorSweepCpus (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  )
{
  UINTN       Socket;
  CHAR8       SocketCpusStr[] = "/socket@00/cpus";
  EFI_STATUS  Status;

  for (Socket = 0; Socket < TH500_MAX_SOCKETS; Socket++) {
    INT32  CpusOffset;

    if (!(SocketMask & (1UL << Socket))) {
      continue;
    }

    AsciiSPrint (SocketCpusStr, sizeof (SocketCpusStr), "/socket@%u/cpus", Socket);
    CpusOffset = fdt_path_offset (Dtb, SocketCpusStr);
    if (CpusOffset < 0) {
      DEBUG ((DEBUG_ERROR, "Failed to find %a subnode\n", SocketCpusStr));
      return EFI_DEVICE_ERROR;
    }

    DEBUG ((DEBUG_INFO, "Floorsweeping cpus in %a\n", SocketCpusStr));

    Status = UpdateCpuFloorsweepingConfig (CpusOffset, Dtb);
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
TH500CheckAndRemapCpu (
  IN UINT32        LogicalCore,
  IN OUT UINT64    *Mpidr,
  OUT CONST CHAR8  **DtCpuFormat,
  OUT UINTN        *DtCpuId
  )
{
  EFI_STATUS  Status;
  UINT32      LinearCoreId;

  LinearCoreId = GetLinearCoreIDFromMpidr (*Mpidr);
  if (IsCoreEnabled (LinearCoreId)) {
    *DtCpuFormat = "cpu@%u";
    *DtCpuId     = LinearCoreId % PLATFORM_MAX_CORES_PER_SOCKET;
    Status       = EFI_SUCCESS;
  } else {
    Status = EFI_UNSUPPORTED;
  }

  return Status;
}
