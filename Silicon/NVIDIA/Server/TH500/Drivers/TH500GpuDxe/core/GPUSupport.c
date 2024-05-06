/** @file

  NVIDIA GPU Memory information support functions.
    Placeholder until PCD, post devinit scratch, fsp query
    or CXL information available

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

///
/// Libraries
///

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>

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
#include "dev_therm.h"
#include "published/hopper/gh100/dev_therm_addendum.h"

#include "GPUMemoryInfo.h"

#include "GPUSupport.h"

/// Support for Memory Size is broken out based on architecture
#include "hopper/GPUSupportMemSizeLegacy.h"
#include "blackwell/GPUSupportMemSize.h"

///
/// External resources
///

///
/// GPU Mode Macros
///

/*
   While in SHH mode, board shall program the PCI Device ID strap override to the _B range. GH100 (0x2300 � 0x233f is DevID_A, 0x2340 � 0x237f is DevID_B) Device ID allocation is as follows:

  0x2300 = recovery mode and pre-silicon/unfused parts
  0x2301 - 0x233f = GH100 products in endpoint mode
  0x2340 = reserved
  0x2341 - 0x237f = GH100 products in SH mode
  0x2900 = recovery mode and pre-silicon/unfused parts
  0x2901 - 0x293f = GB100 products in endpoint mode
  0x2940 = reserved
  0x2941 - 0x297f = GB100 products in SH mode
*/
#define TH500_GPU_MODE_CHECK_EHH(vid, did) \
    ( ((vid)==0x10de) && ((did)==0x2300) )
#define TH500_GPU_MODE_CHECK_EH(vid, did) \
    ( ((vid)==0x10de) && (((did)>=0x2301) && ((did)<=0x233f)) )
#define TH500_GPU_MODE_CHECK_SHH(vid, did) \
    ( ((vid)==0x10de) && (((did)>=0x2341) && ((did)<=0x237f)) )
#define TH500_GB180_VDK_GPU_MODE_CHECK_SHH(vid, did) \
    ( ((vid)==0x10de) && ((did)==0x293f) )
#define TH500_GB180_GPU_MODE_CHECK_EHH(vid, did) \
    ( ((vid)==0x10de) && ((did)==0x2900) )
#define TH500_GB180_GPU_MODE_CHECK_EH(vid, did) \
    ( ((vid)==0x10de) && (((did)>=0x2901) && ((did)<=0x293f)) )
#define TH500_GB100_GPU_MODE_CHECK_SHH(vid, did) \
    ( ((vid)==0x10de) && (((did)>=0x2941) && ((did)<=0x297f)) )
#define TH500_GB102_GPU_MODE_CHECK_SHH(vid, did) \
    ( ((vid)==0x10de) && (((did)>=0x29C1) && ((did)<=0x29ff)) )
#define TH500_GB180_GPU_MODE_CHECK_SHH(vid, did) \
    ( TH500_GB100_GPU_MODE_CHECK_SHH(vid, did) || \
      TH500_GB102_GPU_MODE_CHECK_SHH(vid, did) )

///
/// GPU Mode check and Firmware complete poll
///

/** Returns the Mode of the GPU

  @param[in]  PciIo   PciIo protocol of controller handle to check status on
  @param[out] GpuMode Mode of controller associated with the PciIo protocol passed as a parameter
  @retval status
            EFI_SUCCESS
            EFI_INVALID_PARAMETER
**/
EFI_STATUS
EFIAPI
CheckGpuMode (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT GPU_MODE            *GpuMode
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  PCI_TYPE00  Pci;

  if (NULL == PciIo) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == GpuMode) {
    return EFI_INVALID_PARAMETER;
  }

  /* Read PciIo config space for Vendor ID and Device ID */
  Status =
    PciIo->Pci.Read (
                 PciIo,
                 EfiPciIoWidthUint8,
                 0,
                 sizeof (Pci),
                 &Pci
                 );

  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of Pci TYPE00 returned '%r'\n", __FUNCTION__, PciIo, Status));
  if (!EFI_ERROR (Status)) {
    BOOLEAN  bVDKPlatform = (TegraGetPlatform () != TEGRA_PLATFORM_VDK);
    /* PCI Device ID and Vendor ID for match to determine support status */
    DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] Controller Handle 2-part Id.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
    if ( TH500_GPU_MODE_CHECK_SHH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId)) {
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] GPU Mode: 'SHH'.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
      *GpuMode = GPU_MODE_SHH;
    } else if ( TH500_GPU_MODE_CHECK_EH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId)) {
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] GPU Mode: 'EH'.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
      *GpuMode = GPU_MODE_EH;
    } else if ( TH500_GPU_MODE_CHECK_EHH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId)) {
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] GPU Mode: 'EHH'.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
      *GpuMode = GPU_MODE_EHH;
    } else if ( bVDKPlatform && (TH500_GB180_VDK_GPU_MODE_CHECK_SHH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId))) {
      /* VDK override to SHH mode from standard Device ID check */
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] [VDK] GPU Mode: 'SHH'.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
      *GpuMode = GPU_MODE_SHH;
    } else if ( TH500_GB180_GPU_MODE_CHECK_SHH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId)) {
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] GPU Mode: 'SHH'.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
      *GpuMode = GPU_MODE_SHH;
    } else if ( TH500_GB180_GPU_MODE_CHECK_EH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId)) {
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] GPU Mode: 'EH'.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
      *GpuMode = GPU_MODE_EH;
    } else if ( TH500_GB180_GPU_MODE_CHECK_EHH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId)) {
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] GPU Mode: 'EHH'.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
      *GpuMode = GPU_MODE_EHH;
    } else {
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] Unsupported GPU ID.\n", __FUNCTION__, Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
      Status = EFI_UNSUPPORTED;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: GPU Mode: '%d'.\n", __FUNCTION__, *GpuMode));

  return Status;
}

/** Returns the State of the firmware initialization for the GPU

  @param[in]  PciIo         PciIo protocol of controller handle to check status on
  @param[out] bInitComplete boolean of firmware initialization completion check
  @retval status
            EFI_SUCCESS
            EFI_INVALID_PARAMETER
**/
EFI_STATUS
EFIAPI
CheckGfwInitComplete (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT BOOLEAN             *bInitComplete
  )
{
  UINT32      RegVal = 0;
  EFI_STATUS  Status;

  if (NULL == PciIo) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == bInitComplete) {
    return EFI_INVALID_PARAMETER;
  }

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_THERM_I2CS_SCRATCH,
                        1,
                        &RegVal
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] PciIo read of '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_THERM_I2CS_SCRATCH", Status));
  }

  if (FLD_TEST_DRF (_THERM, _I2CS_SCRATCH_FSP_BOOT_COMPLETE, _STATUS, _SUCCESS, RegVal)) {
    *bInitComplete = TRUE;
  } else {
    *bInitComplete = FALSE;
  }

  return Status;
}

/** Returns the Memory Size for the GPU

  @param[in] ControllerHandle - Controller Handle to obtain the GPU Memory Information from

  @retval UINT64 containing the GPU MemSize
*/
UINT64
EFIAPI
GetGPUMemSize (
  IN EFI_HANDLE  ControllerHandle
  )
{
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

  /* Read PciIo config space for Vendor ID and Device ID */
  Status =
    PciIo->Pci.Read (
                 PciIo,
                 EfiPciIoWidthUint8,
                 0,
                 sizeof (Pci),
                 &Pci
                 );

  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of Pci TYPE00 returned '%r'\n", __FUNCTION__, PciIo, Status));
  if (EFI_ERROR (Status)) {
    /* PciIo error accessing configuration space */
    DEBUG ((DEBUG_ERROR, "%a: [ImageHandle:%p] 'PciIo' configuration space structure read returned '%r'\n", __FUNCTION__, gImageHandle, Status));
    ASSERT (0);
    return 0ULL;
  }

  // Call appropriate support call for architecture
  if ((TH500_GB180_GPU_MODE_CHECK_SHH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId)) || (TH500_GB180_GPU_MODE_CHECK_EH (Pci.Hdr.VendorId, Pci.Hdr.DeviceId))) {
    fbSize = GetGPUMemSizeSupport (ControllerHandle);
  } else {
    fbSize = GetGPUMemSizeSupportLegacy (ControllerHandle);
  }

  return fbSize;
}
