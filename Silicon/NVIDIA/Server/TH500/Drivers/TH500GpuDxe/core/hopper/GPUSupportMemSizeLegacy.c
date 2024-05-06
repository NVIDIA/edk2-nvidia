/** @file

  NVIDIA GPU Memory sizing support function.
    Requires architectural headers for the Hopper.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

///
/// Libraries
///

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

///
/// Protocol(s)
///
#include <Protocol/PciIo.h>

#include <IndustryStandard/Pci.h>

/* Don't define legacy BIT macros, conflict with UEFI macros */
#define NVIDIA_UNDEF_LEGACY_BIT_MACROS
#include <nvmisc.h>

///
/// Chip includes
///
#include "hopper/gh100/dev_fb.h"

#include "GPUSupportMemSizeLegacy.h"

///
/// External resources
///

/** Returns the Memory Size for the GPU

  @param[in] ControllerHandle - Controller Handle to obtain the GPU Memory Information from

  @retval UINT64 containing the GPU MemSize
*/
UINT64
EFIAPI
GetGPUMemSizeSupportLegacy (
  IN EFI_HANDLE  ControllerHandle
  )
{
  UINT32               RegVal32  = 0;
  UINT32               RegAddr32 = NV_PFB_PRI_MMU_LOCAL_MEMORY_RANGE;
  UINT32               lowerRangeMag;
  UINT32               lowerRangeScale;
  UINT64               fbSize = 0ULL;
  EFI_PCI_IO_PROTOCOL  *PciIo = NULL;
  EFI_STATUS           Status;
  PCI_TYPE00           Pci;

  if (NULL == ControllerHandle) {
    ASSERT (FALSE);
    return 0ULL;
  }

  /* Check for installed PciIo Protocol to retrieve PCI Location Information */
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    /* PciIo not present */
    DEBUG ((DEBUG_ERROR, "%a: [ImageHandle:%p] GetProtocol for 'PciIo' returned '%r'\n", __FUNCTION__, gImageHandle, Status));
    ASSERT (0);
    return 0ULL;
  }

  DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] GPU Local Memory offset 0x%08x\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId, RegAddr32));

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        RegAddr32,
                        1,
                        &RegVal32
                        );
  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' returned '%r'\n", __FUNCTION__, ControllerHandle, "NV_PFB_PRI_MMU_LOCAL_MEMORY_RANGE", Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' [0x%08x] = '0x%08x'\n", __FUNCTION__, ControllerHandle, "NV_PFB_PRI_MMU_LOCAL_MEMORY_RANGE", RegAddr32, RegVal32));

  // Adjustment code
  lowerRangeMag   = DRF_VAL (_PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _LOWER_MAG, RegVal32);
  lowerRangeScale = DRF_VAL (_PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _LOWER_SCALE, RegVal32);
  fbSize          = ((UINT64)lowerRangeMag << (lowerRangeScale + 20));

  if (FLD_TEST_DRF (_PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _ECC_MODE, _ENABLED, RegVal32)) {
    fbSize = fbSize / 16 * 15;
  }

  return fbSize;
}
