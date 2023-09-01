/** @file
  GicC parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "GicParser.h"
#include <Library/ArmGicLib.h>
#include <Library/ArmLib/AArch64/AArch64Lib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <T194/T194Definitions.h>
#include <Protocol/TegraCpuFreq.h>

#define TH500_TRBE_INT  22

EFI_STATUS
EFIAPI
GetPmuBaseInterrupt (
  OUT HARDWARE_INTERRUPT_SOURCE  *PmuBaseInterrupt
  )
{
  EFI_STATUS                         Status;
  UINT32                             PmuHandle;
  UINT32                             NumPmuHandles;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  InterruptData;
  UINT32                             Size;

  NumPmuHandles = 1;
  Status        = GetMatchingEnabledDeviceTreeNodes ("arm,armv8-pmuv3", &PmuHandle, &NumPmuHandles);
  if (EFI_ERROR (Status)) {
    NumPmuHandles = 1;
    Status        = GetMatchingEnabledDeviceTreeNodes ("arm,cortex-a78-pmu", &PmuHandle, &NumPmuHandles);
    if (EFI_ERROR (Status)) {
      NumPmuHandles     = 0;
      *PmuBaseInterrupt = 0;
      return Status;
    }
  }

  // Only one interrupt is expected
  Size   = 1;
  Status = GetDeviceTreeInterrupts (PmuHandle, &InterruptData, &Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (InterruptData.Type == INTERRUPT_PPI_TYPE);
  *PmuBaseInterrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData);
  return Status;
}

EFI_STATUS
EFIAPI
GicCpcParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch,
  OUT CM_OBJECT_TOKEN              **TokenMapPtr OPTIONAL
  )
{
  EFI_STATUS                      Status;
  NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *CpuFreq;
  CM_ARM_CPC_INFO                 *CpcInfo;
  UINT64                          MpIdr;
  UINT32                          NumCores;
  UINT32                          CoreIndex;
  CM_OBJ_DESCRIPTOR               Desc;
  UINT32                          CpcInfoSize;
  UINTN                           ChipID;

  CpcInfo = NULL;

  ChipID = TegraGetChipID ();
  Status = MpCoreInfoGetPlatformInfo (&NumCores, NULL, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get MpCoreInfo\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  CpcInfoSize = NumCores * sizeof (CM_ARM_CPC_INFO);
  CpcInfo     = AllocatePool (CpcInfoSize);
  if (CpcInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate CpcInfo structure array\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = gBS->LocateProtocol (&gNVIDIATegraCpuFrequencyProtocolGuid, NULL, (VOID **)&CpuFreq);
  if (!EFI_ERROR (Status) && (CpuFreq != NULL)) {
    // Populate Cpc structures for all enabled cores and return the list of tokens
    for (CoreIndex = 0; CoreIndex < NumCores; CoreIndex++) {
      Status = MpCoreInfoGetProcessorIdFromIndex (CoreIndex, &MpIdr);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r getting ProcessorId for CoreIndex %u\n", __FUNCTION__, Status, CoreIndex));
        goto CleanupAndReturn;
      }

      Status = CpuFreq->GetCpcInfo (CpuFreq, MpIdr, &CpcInfo[CoreIndex]);
      if (EFI_ERROR (Status)) {
        goto CleanupAndReturn;
      }
    }

    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCpcInfo);
    Desc.Size     = CpcInfoSize;
    Desc.Count    = NumCores;
    Desc.Data     = CpcInfo;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, TokenMapPtr, NULL);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }
  }

CleanupAndReturn:
  if ((TokenMapPtr != NULL) && (EFI_ERROR (Status))) {
    *TokenMapPtr = NULL;
  }

  FREE_NON_NULL (CpcInfo);
  return Status;
}

/** GicC parser function.

  The following structures are populated:
  - EArmObjGicCInfo
  - EArmObjEtInfo (if supported)
  - EArmCpcInfo (via GicCpcParser)

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.
  @param [out] TokenMapPtr     The tokens corresponding to the GicC objects.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GicCParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch,
  OUT CM_OBJECT_TOKEN              **TokenMapPtr OPTIONAL
  )
{
  EFI_STATUS                         Status;
  UINT32                             NumCores;
  CM_ARM_GICC_INFO                   *GicCInfo;
  UINTN                              GicCInfoSize;
  UINT64                             MpIdr;
  CM_OBJ_DESCRIPTOR                  Desc;
  CM_OBJECT_TOKEN                    *GicCInfoTokens;
  TEGRA_GIC_INFO                     *GicInfo;
  UINT32                             CoreIndex;
  UINT64                             PmuBaseInterrupt;
  UINT64                             DbgFeatures;
  UINT16                             TrbeInterrupt;
  CM_ARM_ET_INFO                     *EtInfo;
  CM_OBJECT_TOKEN                    EtToken;
  UINT32                             NumberOfSpeHandles;
  UINT32                             SpeOverflowInterruptHandle;
  UINT32                             NumberOfSpeInterrupts;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  SpeOverflowInterrupt;
  UINT32                             SpeOverflowInterruptNum;
  UINTN                              ChipID;
  CM_OBJECT_TOKEN                    *CpcTokens;
  UINT32                             Socket;

  GicInfo        = NULL;
  GicCInfo       = NULL;
  CpcTokens      = NULL;
  GicCInfoTokens = NULL;

  if (ParserHandle == NULL) {
    ASSERT (ParserHandle != NULL);
    Status = EFI_INVALID_PARAMETER;
    goto CleanupAndReturn;
  }

  ChipID = TegraGetChipID ();

  GicInfo = (TEGRA_GIC_INFO *)AllocatePool (sizeof (TEGRA_GIC_INFO));

  if (!GetGicInfo (GicInfo)) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = MpCoreInfoGetPlatformInfo (&NumCores, NULL, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get MpCoreInfo\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  GicCInfoSize = sizeof (CM_ARM_GICC_INFO) * NumCores;
  GicCInfo     = AllocateZeroPool (GicCInfoSize);
  if (GicCInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // PMU
  if (ChipID != T194_CHIP_ID) {
    Status = GetPmuBaseInterrupt (&PmuBaseInterrupt);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }
  }

  DbgFeatures = ArmReadIdAA64Dfr0 ();

  // The ID_AA64DFR0_EL1.TraceBuffer field identifies support for FEAT_TRBE.
  if (((DbgFeatures >> 44) & 0xF) != 0) {
    TrbeInterrupt = TH500_TRBE_INT;
  } else {
    TrbeInterrupt = 0;
  }

  // The ID_AA64DFR0_EL1.TraceVer field identifies the presence of FEAT_ETE.
  if (((DbgFeatures >> 4) & 0xF) != 0) {
    EtInfo = AllocateZeroPool (sizeof (CM_ARM_ET_INFO));
    NV_ASSERT_RETURN (
      EtInfo != NULL,
      { Status = EFI_OUT_OF_RESOURCES;
        goto CleanupAndReturn;
      },
      "%a: Failed to allocate EtInfo structure\r\n",
      __FUNCTION__
      );
    EtInfo->EtType = ArmEtTypeEte;
    Status         = NvAddSingleCmObj (ParserHandle, CREATE_CM_ARM_OBJECT_ID (EArmObjEtInfo), EtInfo, sizeof (CM_ARM_ET_INFO), &EtToken);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }
  } else {
    EtToken = CM_NULL_TOKEN;
  }

  // Get SpeOverflow interrupt information
  NumberOfSpeHandles      = 1;
  SpeOverflowInterruptNum = 0;
  Status                  = GetMatchingEnabledDeviceTreeNodes ("arm,statistical-profiling-extension-v1", &SpeOverflowInterruptHandle, &NumberOfSpeHandles);
  if (Status == EFI_NOT_FOUND) {
    SpeOverflowInterruptNum = PcdGet32 (PcdSpeOverflowIntrNum);
    DEBUG ((DEBUG_INFO, "%a: SPE not found in DTB. SpeOverflowInterrupt will be 0x%x\n", __FUNCTION__, SpeOverflowInterruptNum));
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error checking for SPE nodes in DTB: %r\n", __FUNCTION__, Status));
  } else {
    NumberOfSpeInterrupts = 1;
    Status                = GetDeviceTreeInterrupts (SpeOverflowInterruptHandle, &SpeOverflowInterrupt, &NumberOfSpeInterrupts);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error getting SPE node interrupt: %r\n", __FUNCTION__, Status));
    } else {
      SpeOverflowInterruptNum = DEVICETREE_TO_ACPI_INTERRUPT_NUM (SpeOverflowInterrupt);
    }
  }

  // CpcInfo
  Status = GicCpcParser (ParserHandle, FdtBranch, &CpcTokens);
  if (EFI_ERROR (Status) && (Status != EFI_UNSUPPORTED)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get CpcTokens\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  // Space for the GicC tokens
  if (TokenMapPtr != NULL) {
    GicCInfoTokens = AllocateZeroPool (sizeof (CM_OBJECT_TOKEN) * NumCores);
    if (GicCInfoTokens == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for %u GicCInfoTokens\n", __FUNCTION__, NumCores));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }
  }

  // Populate GICC structures for all enabled cores
  for (CoreIndex = 0; CoreIndex < NumCores; CoreIndex++) {
    Status = MpCoreInfoGetProcessorIdFromIndex (CoreIndex, &MpIdr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r getting ProcessorId for CoreIndex %u\n", __FUNCTION__, Status, CoreIndex));
      goto CleanupAndReturn;
    }

    Status = MpCoreInfoGetProcessorLocation (MpIdr, &Socket, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r getting ProcessorLocation for MpIdr 0x%lx\n", __FUNCTION__, Status, MpIdr));
      goto CleanupAndReturn;
    }

    GicCInfo[CoreIndex].CPUInterfaceNumber     = CoreIndex;
    GicCInfo[CoreIndex].AcpiProcessorUid       = CoreIndex;
    GicCInfo[CoreIndex].Flags                  = EFI_ACPI_6_4_GIC_ENABLED;
    GicCInfo[CoreIndex].ParkingProtocolVersion = 0;
    if (ChipID == T194_CHIP_ID) {
      GicCInfo[CoreIndex].PerformanceInterruptGsiv = T194_PMU_BASE_INTERRUPT + CoreIndex;
    } else {
      GicCInfo[CoreIndex].PerformanceInterruptGsiv = PmuBaseInterrupt;
    }

    GicCInfo[CoreIndex].ParkedAddress = 0;
    if (GicInfo->Version < 3) {
      GicCInfo[CoreIndex].PhysicalBaseAddress = PcdGet64 (PcdGicInterruptInterfaceBase);
    }

    GicCInfo[CoreIndex].GICV                          = 0;
    GicCInfo[CoreIndex].GICH                          = 0;
    GicCInfo[CoreIndex].VGICMaintenanceInterrupt      = PcdGet32 (PcdArmArchVirtMaintenanceIntrNum);
    GicCInfo[CoreIndex].GICRBaseAddress               = 0;
    GicCInfo[CoreIndex].MPIDR                         = MpIdr;
    GicCInfo[CoreIndex].ProcessorPowerEfficiencyClass = 0;
    GicCInfo[CoreIndex].SpeOverflowInterrupt          = SpeOverflowInterruptNum;
    GicCInfo[CoreIndex].ProximityDomain               = Socket;
    GicCInfo[CoreIndex].ClockDomain                   = 0;
    GicCInfo[CoreIndex].AffinityFlags                 = EFI_ACPI_6_4_GICC_ENABLED;

    GicCInfo[CoreIndex].CpcToken      = CpcTokens ? CpcTokens[CoreIndex] : CM_NULL_TOKEN;
    GicCInfo[CoreIndex].TrbeInterrupt = TrbeInterrupt;
    GicCInfo[CoreIndex].EtToken       = EtToken;
  }

  Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicCInfo);
  Desc.Size     = GicCInfoSize;
  Desc.Count    = NumCores;
  Desc.Data     = GicCInfo;
  Status        = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, &GicCInfoTokens, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (GicInfo);
  FREE_NON_NULL (GicCInfo);
  if ((TokenMapPtr != NULL) && !EFI_ERROR (Status)) {
    *TokenMapPtr = GicCInfoTokens;
  } else {
    FREE_NON_NULL (GicCInfoTokens);
  }

  FREE_NON_NULL (CpcTokens);

  return Status;
}
