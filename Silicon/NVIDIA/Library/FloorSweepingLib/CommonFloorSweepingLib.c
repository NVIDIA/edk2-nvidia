/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <ArmMpidr.h>
#include <PiDxe.h>
#include <Library/FloorSweepingLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <libfdt.h>

#include "CommonFloorSweepingLib.h"

STATIC UINT64  *SocketCbbFabricBaseAddr                        = NULL;
STATIC UINT64  TH500SocketCbbFabricBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_CBB_FABRIC_BASE_SOCKET_0,
  TH500_CBB_FABRIC_BASE_SOCKET_1,
  TH500_CBB_FABRIC_BASE_SOCKET_2,
  TH500_CBB_FABRIC_BASE_SOCKET_3,
};

STATIC UINT64  *SocketMssBaseAddr                        = NULL;
STATIC UINT64  TH500SocketMssBaseAddr[TH500_MAX_SOCKETS] = {
  TH500_MSS_BASE_SOCKET_0,
  TH500_MSS_BASE_SOCKET_1,
  TH500_MSS_BASE_SOCKET_2,
  TH500_MSS_BASE_SOCKET_3,
};

STATIC TEGRA_PLATFORM_RESOURCE_INFO  *mPlatformResourceInfo = NULL;

/**
  Initialize global structures

**/
EFI_STATUS
EFIAPI
CommonInitializeGlobalStructures (
  IN  VOID                             *Dtb,
  OUT CONST TEGRA_FLOOR_SWEEPING_INFO  **FloorSweepingInfo
  )
{
  EFI_STATUS  Status;
  UINTN       ChipId;
  VOID        *Hob;

  SetDeviceTreePointer (Dtb, fdt_totalsize (Dtb));

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  NV_ASSERT_RETURN (
    (Hob != NULL) &&
    (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)),
    return EFI_DEVICE_ERROR,
    "Failed to get PlatformResourceInfo\r\n"
    );

  mPlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);

  ChipId = TegraGetChipID ();
  switch (ChipId) {
    case TH500_CHIP_ID:
      SocketMssBaseAddr       = TH500SocketMssBaseAddr;
      SocketCbbFabricBaseAddr = TH500SocketCbbFabricBaseAddr;
      Status                  = EFI_SUCCESS;
      break;

    default:
      Status = EFI_SUCCESS;
      break;
  }

  if (mPlatformResourceInfo->FloorSweepingInfo == NULL) {
    Status = EFI_UNSUPPORTED;
  }

  if (!EFI_ERROR (Status)) {
    *FloorSweepingInfo = mPlatformResourceInfo->FloorSweepingInfo;
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

  CtrlId         = PcieIdToInterface (PcieId);
  CbbCtlOffset   = CbbFabricBase + 0x20 * PcieIdToInterface (PcieId);
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
  if ((SocketMssBaseAddr != NULL) && (PcieIdToInterface (PcieId) == 8)) {
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
            PcieIdToInterface (PcieId)
            ));
        } else {
          DEBUG ((
            DEBUG_INFO,
            "Deleted 'external-facing' property for Ctrl = %d\n",
            PcieIdToInterface (PcieId)
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

EFI_STATUS
EFIAPI
FloorSweepDisableNode (
  IN INT32  NodeOffset
  )
{
  EFI_STATUS  Status;

  Status = DeviceTreeSetNodeProperty (NodeOffset, "status", "disabled", sizeof ("disabled"));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: error disabling node %a status=%r\n", __FUNCTION__, DeviceTreeGetNodeName (NodeOffset), Status));
  }

  return Status;
}

/**
  Floorsweep PCIe

**/
EFI_STATUS
EFIAPI
CommonFloorSweepPcie (
  IN  VOID  *Dtb
  )
{
  EFI_STATUS                       Status;
  INT32                            ParentOffset;
  INT32                            NodeOffset;
  CHAR8                            ParentNameStr[16];
  UINTN                            ChipId;
  CONST CHAR8                      *ParentNameFormat;
  UINT32                           InterfaceSocket;
  INT32                            FdtErr;
  UINT32                           *PcieDisableRegArray;
  UINTN                            Index;
  UINTN                            NumParentNodes;
  CONST CHAR8                      *PcieEpCompatibility;
  CONST TEGRA_FLOOR_SWEEPING_INFO  *Info;

  Info = mPlatformResourceInfo->FloorSweepingInfo;

  // check for PCIe floorsweeping supported
  if (Info->PcieDisableRegArray == NULL) {
    DEBUG ((DEBUG_INFO, "%a: no  PcieDisableRegArray\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  PcieDisableRegArray = Info->PcieDisableRegArray;
  PcieEpCompatibility = Info->PcieEpCompatibility;
  ParentNameFormat    = Info->PcieParentNameFormat;
  NumParentNodes      = Info->PcieNumParentNodes;
  ChipId              = TegraGetChipID ();

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
      UINT32      CtrlId;

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

      InterfaceSocket = PcieIdToSocket (PcieId);
      CtrlId          = PcieIdToInterface (PcieId);

      if (!IsSocketEnabled (InterfaceSocket) ||
          ((PcieDisableRegArray[InterfaceSocket] & (1UL << CtrlId)) != 0))
      {
        FdtErr = fdt_setprop (Dtb, NodeOffset, "status", "disabled", sizeof ("disabled"));
        if (FdtErr < 0) {
          DEBUG ((DEBUG_ERROR, "Failed to disable PcieId=0x%x node: %a\n", PcieId, fdt_strerror (FdtErr)));
          return EFI_DEVICE_ERROR;
        }

        DEBUG ((DEBUG_INFO, "%a: Disabled PcieId=0x%x reg=0x%x\n", __FUNCTION__, PcieId, PcieDisableRegArray[InterfaceSocket]));
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
  IN  VOID  *Dtb
  )
{
  UINT32                                Socket;
  UINTN                                 ScfCacheCount;
  UINT32                                ScfCacheSize;
  UINT32                                ScfCacheSets;
  INT32                                 NodeOffset;
  INT32                                 FdtErr;
  UINT32                                Tmp32;
  CHAR8                                 SocketNodeStr[] = "/socket@xxxxxxxxxxx";
  CONST TEGRA_FLOOR_SWEEPING_INFO       *Info;
  CONST TEGRA_FLOOR_SWEEPING_SCF_CACHE  *Scf;

  Info = mPlatformResourceInfo->FloorSweepingInfo;

  Scf = Info->ScfCacheInfo;

  // check for SCF floorsweeping supported
  if (Scf == NULL) {
    DEBUG ((DEBUG_INFO, "%a: no ScfCache info\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  // SCF Cache is distributed as l3-cache over all possible sockets
  MPCORE_FOR_EACH_ENABLED_SOCKET (Socket) {
    ScfCacheCount = Scf->MaxScfCacheCountPerSocket;
    UINT64  ScratchBase = Scf->ScfDisableSocketBase[Socket];
    UINTN   ScfScratchWord;

    if (ScratchBase == 0) {
      continue;
    }

    for (ScfScratchWord = 0;
         ScfScratchWord < Scf->ScfDisableWords;
         ScfScratchWord++)
    {
      UINT32  DisableScratchReg;

      DisableScratchReg   = MmioRead32 (ScratchBase + Scf->ScfDisableOffset[ScfScratchWord]);
      DisableScratchReg >>= Scf->ScfDisableShift[ScfScratchWord];
      DisableScratchReg  &= ~Scf->ScfDisableMask[ScfScratchWord];
      ScfCacheCount      -= BitsSet (DisableScratchReg);
    }

    ScfCacheSize = ScfCacheCount * Scf->ScfSliceSize;
    ScfCacheSets = ScfCacheCount * Scf->ScfSliceSets;

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
  IN  VOID  *Dtb
  )
{
  CHAR8  CpusStr[] = "/cpus";
  INT32  CpusOffset;

  CpusOffset = fdt_path_offset (Dtb, CpusStr);
  if (CpusOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find %a subnode\n", CpusStr));
    return EFI_DEVICE_ERROR;
  }

  return UpdateCpuFloorsweepingConfig (CpusOffset, Dtb);
}

EFI_STATUS
EFIAPI
TH500FloorSweepCpus (
  IN  VOID  *Dtb
  )
{
  UINT32      Socket;
  CHAR8       SocketCpusStr[] = "/socket@00/cpus";
  CHAR8       CpusStr[]       = "/cpus";
  EFI_STATUS  Status;

  Status = EFI_UNSUPPORTED;
  MPCORE_FOR_EACH_ENABLED_SOCKET (Socket) {
    INT32  CpusOffset;

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

    Status = UpdateCpuFloorsweepingConfig (CpusOffset, Dtb);
    if (EFI_ERROR (Status)) {
      break;
    }
  }

  return Status;
}

STATIC
UINT8
EFIAPI
FloorSweepGetDtbNodeSocket (
  INT32   NodeOffset,
  UINT64  SocketAddressMask,
  UINT8   AddressToSocketShift
  )
{
  UINT8   Socket;
  UINT64  UnitAddress;

  UnitAddress = DeviceTreeGetNodeUnitAddress (NodeOffset);
  Socket      = (UnitAddress >> AddressToSocketShift) & SocketAddressMask;

  DEBUG ((DEBUG_INFO, "%a: addr=0x%llx socket=%u\n", __FUNCTION__, UnitAddress, Socket));

  return Socket;
}

STATIC
EFI_STATUS
EFIAPI
FloorSweepIpEntry (
  IN  INT32                                IpsOffset,
  IN  CONST TEGRA_FLOOR_SWEEPING_IP_ENTRY  *IpEntry
  )
{
  EFI_STATUS                       Status;
  UINTN                            Socket;
  INT32                            NodeOffset;
  BOOLEAN                          NodeIsDisabled;
  BOOLEAN                          IpIsDisabled;
  UINTN                            MaxSockets;
  UINT32                           DisableReg;
  UINT32                           Id;
  CONST TEGRA_FLOOR_SWEEPING_INFO  *Info;

  NV_ASSERT_RETURN (((mPlatformResourceInfo != NULL) && (IpEntry->DisableReg != NULL)), return EFI_INVALID_PARAMETER, "%a: Bad param\n", __FUNCTION__);

  Info         = mPlatformResourceInfo->FloorSweepingInfo;
  MaxSockets   = mPlatformResourceInfo->MaxPossibleSockets;
  IpIsDisabled = FALSE;
  for (Socket = 0; Socket < MaxSockets; Socket++) {
    if (!IsSocketEnabled (Socket) || (IpEntry->DisableReg[Socket] != 0)) {
      IpIsDisabled = TRUE;
      break;
    }
  }

  if (!IpIsDisabled) {
    DEBUG ((DEBUG_INFO, "%a: no disables for IP %a\n", __FUNCTION__, IpEntry->IpName));
    return EFI_SUCCESS;
  }

  NodeOffset = 0;
  while (1) {
    Status = DeviceTreeGetNextCompatibleSubnode (IpEntry->CompatibilityList, IpsOffset, &NodeOffset);
    if (EFI_ERROR (Status)) {
      break;
    }

    if (MaxSockets == 1) {
      Socket = 0;
    } else {
      Socket = FloorSweepGetDtbNodeSocket (
                 NodeOffset,
                 Info->SocketAddressMask,
                 Info->AddressToSocketShift
                 );
    }

    if (!IsSocketEnabled (Socket)) {
      NodeIsDisabled = TRUE;
      DisableReg     = MAX_UINT32;
    } else {
      NodeIsDisabled = FALSE;
      DisableReg     = IpEntry->DisableReg[Socket];
      if (DisableReg != 0) {
        if (IpEntry->IdProperty == NULL) {
          NodeIsDisabled = TRUE;
        } else {
          Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, IpEntry->IdProperty, &Id);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a: getting %a failed, ignoring %a node: %r\n", __FUNCTION__, IpEntry->IdProperty, DeviceTreeGetNodeName (NodeOffset), Status));
          } else {
            NodeIsDisabled = ((DisableReg & (1UL << Id)) != 0);
          }
        }
      }
    }

    if (NodeIsDisabled) {
      Status = FloorSweepDisableNode (NodeOffset);
    }

    DEBUG ((DEBUG_INFO, "%a: node %a is %a socket=%u, reg=0x%x, status=%r\n", __FUNCTION__, DeviceTreeGetNodeName (NodeOffset), (NodeIsDisabled) ? "disabled" : "enabled", Socket, DisableReg, Status));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CommonFloorSweepIps (
  VOID
  )
{
  CONST CHAR8                          *IpsRootName = "/bus@0";
  INT32                                IpsOffset;
  EFI_STATUS                           Status;
  CONST TEGRA_FLOOR_SWEEPING_IP_ENTRY  *IpTable;
  CONST TEGRA_FLOOR_SWEEPING_INFO      *Info;

  Info    = mPlatformResourceInfo->FloorSweepingInfo;
  IpTable = Info->IpTable;

  // check for IP floorsweeping supported
  if (IpTable == NULL) {
    DEBUG ((DEBUG_INFO, "%a: no IpTable\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  Status = DeviceTreeGetNodeByPath (IpsRootName, &IpsOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: IP root %a failed, using 0: %r\n", __FUNCTION__, IpsRootName, Status));
    IpsOffset = 0;
  }

  while (IpTable->IpName != NULL) {
    Status = FloorSweepIpEntry (IpsOffset, IpTable);

    DEBUG ((DEBUG_INFO, "%a: floorswept %a nodes: %r\n", __FUNCTION__, IpTable->IpName, Status));

    IpTable++;
  }

  return EFI_SUCCESS;
}
