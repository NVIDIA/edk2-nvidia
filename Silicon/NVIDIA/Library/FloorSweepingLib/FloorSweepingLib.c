/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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
#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/MceAriLib.h>
#include <Library/NvgLib.h>
#include <Library/TegraPlatformInfoLib.h>

UINT32
GetNumberOfEnabledCpuCores (
  VOID
  )
{
  UINT32    Count;
  UINTN     ChipId;

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
      Count = NvgGetNumberOfEnabledCpuCores ();
      break;
    case T234_CHIP_ID:
      Count = MceAriNumCores ();
      break;
    default:
      ASSERT (FALSE);
      Count = 1;
      break;
  }
  DEBUG ((DEBUG_INFO, "%a: ChipId=0x%x, Count=%u\n", __FUNCTION__, ChipId, Count));

  return Count;
}

EFI_STATUS
EFIAPI
CheckAndRemapCpu (
  IN UINT32         LogicalCore,
  IN OUT UINT64     *Mpidr,
  OUT CONST CHAR8   **DtCpuFormat,
  OUT UINTN         *DtCpuId
  )
{
  UINTN         ChipId;
  EFI_STATUS    Status;

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
      Status = NvgConvertCpuLogicalToMpidr (LogicalCore, Mpidr);
      *Mpidr &= MPIDR_AFFINITY_MASK;
      *DtCpuFormat = "cpu@%x";
      *DtCpuId = *Mpidr;
      break;
    case T234_CHIP_ID:
      Status = MceAriCheckCoreEnabled (Mpidr, DtCpuId);
      *DtCpuFormat = "cpu@%u";
      break;
    default:
      ASSERT (FALSE);
      *Mpidr = 0;
      break;
  }
  DEBUG ((DEBUG_INFO, "%a: ChipId=0x%x, Mpidr=0x%llx Status=%r\n", __FUNCTION__, ChipId, *Mpidr, Status));

  return Status;
}

BOOLEAN
EFIAPI
ClusterIsPresent (
  IN  UINTN ClusterId
  )
{
  UINTN         ChipId;
  BOOLEAN       Present;

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
      Present = NvgClusterIsPresent (ClusterId);
      break;
    case T234_CHIP_ID:
      Present = MceAriClusterIsPresent (ClusterId);
      break;
    default:
      ASSERT (FALSE);
      Present = FALSE;
      break;
  }
  DEBUG ((DEBUG_INFO, "%a: ChipId=0x%x, ClusterId=%u, Present=%d\n",
          __FUNCTION__, ChipId, ClusterId, Present));

  return Present;
}
