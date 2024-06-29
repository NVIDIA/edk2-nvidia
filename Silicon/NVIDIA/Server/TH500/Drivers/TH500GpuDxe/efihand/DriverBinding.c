/** @file

  Provides a driver binding protocol for supported NVIDIA GPUs
  as well as providing the NVIDIA GPU DSD AML Generation Protoocol.

  SPDX-FileCopyrightText: Copyright (c) 2020-2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "DriverBinding.h"
#include <Uefi.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PciIo.h>
#include <Protocol/BpmpIpc.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/DeviceDiscoveryLib/DeviceDiscoveryLibPrivate.h>

#include <TH500/TH500Definitions.h>

#include <IndustryStandard/Pci.h>

#include "DriverBinding.h"
#include "GPUSupport.h"

#include "GpuDsdAmlGeneration.h"
#include "GpuFirmwareBootComplete.h"
/* Only required for 'GetControllerATSRangeInfo' testing being enabled*/
#include "GPUMemoryInfo.h"
#include "UEFIFspRpc.h"

///
/// Local Definitions
///

#define NVIDIA_GPUDEVICELIBDRIVER_VERSION  0x10
#define EGM_SOCKET_ADDRESS_MASK            ((UINT64)(~(BIT45|BIT44)))
#define MaskEgmBaseSocketAddress(addr)  ((addr) & EGM_SOCKET_ADDRESS_MASK)

#define UEFI_GFW_BOOT_COMPLETE_POLL_TIMEOUT_INDEX              600000
#define UEFI_GFW_BOOT_COMPLETE_POLL_TIMEOUT_INDEX_LOG_TRIGGER  10000
#define UEFI_CHECK_GFW_BOOT_COMPLETE_POLL_DELAY_UNITS          5

/** Diagnostic dump of GPU Driver Binding Private Data
    @param[in] This                         Private Data structure.
    @retval Status  EFI_SUCCESS             Private Data successfully dumped.
                    EFI_INVALID_PARAMETER   'This' is NULL.
**/
EFI_STATUS
EFIAPI
NVIDIADriverDumpPrivateData (
  IN NVIDIA_GPU_DRIVER_BINDING_PROTOCOL  *This
  )
{
  NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA  *mPrivateData = NULL;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG_CODE_BEGIN ();
  mPrivateData = NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA_FROM_THIS (This);
  DEBUG ((DEBUG_INFO, "%a: Signature:               '0x%08x'\n", __FUNCTION__, mPrivateData->Signature));
  DEBUG ((DEBUG_INFO, "%a: Handle:                  '0x%0p'\n", __FUNCTION__, mPrivateData->Handle));
  DEBUG ((DEBUG_INFO, "%a: DriverBinding:           '0x%0p'\n", __FUNCTION__, &mPrivateData->DriverBinding));
  DEBUG ((DEBUG_INFO, "%a: SystemTable:             '0x%0p'\n", __FUNCTION__, mPrivateData->SystemTable));
  DEBUG ((DEBUG_INFO, "%a: BootServices:            '0x%0p'\n", __FUNCTION__, mPrivateData->BootServices));
  DEBUG ((DEBUG_INFO, "%a: ManagedControllerHandles '0x%0p'\n", __FUNCTION__, mPrivateData->ManagedControllerHandles));
  DEBUG ((DEBUG_INFO, "%a: nManagedControllers:     '0x%lu'\n", __FUNCTION__, mPrivateData->nManagedControllers));
  DEBUG_CODE_END ();

  return EFI_SUCCESS;
}

/** Controller support check based on PCI Vendor ID and Device ID.
    @param[in] ui16VendorID         PCI Vendor ID of the controller.
    @param[in] ui16DeviceID         PCI Device ID of the controller.
    @retval BOOLEAN                 TRUE    Controller is supported.
                                    FALSE   Controller is not supported.
**/
BOOLEAN
EFIAPI
IsControllerSupported (
  IN UINT16  ui16VendorId,
  IN UINT16  ui16DeviceId
  )
{
  BOOLEAN  bResult = FALSE;

  // Handle matching here for GPU Dxe. GOP driver calls GPUInfo for match.
  CONST UINT16  ui16VendorIDMatch = 0x10DE;

  /// 0x2300 = recovery mode and pre-silicon/unfused parts
  /// 0x2301 - 0x233f = GH100 products in endpoint mode
  /// 0x2340 = throwaway
  /// 0x2341 - 0x237f = GH100 products in SH mode
  ///

  /* EHH */
  if ((ui16VendorId == ui16VendorIDMatch) && (ui16DeviceId == 0x2300)) {
    bResult = TRUE;
  }

  /* EH */
  if ((ui16VendorId == ui16VendorIDMatch) && (ui16DeviceId >= 0x2301) && (ui16DeviceId <= 0x233f)) {
    DEBUG_CODE_BEGIN ();
    DEBUG ((DEBUG_ERROR, "%a: PCI ID [0x%04x, 0x%04x] [EH]\n", __FUNCTION__, ui16VendorId, ui16DeviceId));
    DEBUG_CODE_END ();
    bResult = TRUE;
  }

  /* SHH */
  if ((ui16VendorId == ui16VendorIDMatch) && (ui16DeviceId >= 0x2341) && (ui16DeviceId <= 0x237f)) {
    DEBUG_CODE_BEGIN ();
    DEBUG ((DEBUG_ERROR, "%a: PCI ID [0x%04x, 0x%04x] [SHH]\n", __FUNCTION__, ui16VendorId, ui16DeviceId));
    DEBUG_CODE_END ();
    bResult = TRUE;
  }

  /* GB180 */
  /* EH */
  if ((ui16VendorId == ui16VendorIDMatch) && (ui16DeviceId == 0x2900)) {
    bResult = TRUE;
  }

  /* EH and VDK(SHH) */
  if ((ui16VendorId == ui16VendorIDMatch) && (ui16DeviceId >= 0x2901) && (ui16DeviceId <= 0x293f)) {
    DEBUG_CODE_BEGIN ();
    DEBUG ((DEBUG_ERROR, "%a: PCI ID [0x%04x, 0x%04x] [EHH/VDK(SHH)]\n", __FUNCTION__, ui16VendorId, ui16DeviceId));
    DEBUG_CODE_END ();
    bResult = TRUE;
  }

  /* SH */
  /* TH500+GB100 ranges */
  if ((ui16VendorId == ui16VendorIDMatch) && (ui16DeviceId >= 0x2941) && (ui16DeviceId <= 0x297f)) {
    DEBUG_CODE_BEGIN ();
    DEBUG ((DEBUG_ERROR, "%a: PCI ID [0x%04x, 0x%04x] [SHH]\n", __FUNCTION__, ui16VendorId, ui16DeviceId));
    DEBUG_CODE_END ();
    bResult = TRUE;
  }

  /* TH500+GB102 ranges */
  if ((ui16VendorId == ui16VendorIDMatch) && (ui16DeviceId >= 0x29C1) && (ui16DeviceId <= 0x29ff)) {
    DEBUG_CODE_BEGIN ();
    DEBUG ((DEBUG_ERROR, "%a: PCI ID [0x%04x, 0x%04x] [SHH]\n", __FUNCTION__, ui16VendorId, ui16DeviceId));
    DEBUG_CODE_END ();
    bResult = TRUE;
  }

  DEBUG_CODE_BEGIN ();
  /* TESTING: Add QEMU code for one node to match on DEBUG (QEMU X64) */
  if ((ui16VendorId == 0x8086) && (ui16DeviceId == 0x1237)) {
    bResult = TRUE;
  }

  /* [AARCH64] -device virtio-gpu-pci */
  if ((ui16VendorId == 0x1af4) && (ui16DeviceId == 0x1050)) {
    bResult = TRUE;
  }

  DEBUG_CODE_END ();

  return bResult;
}

/** Driver Binding protocol interface to check whether the controller handle is supported by the driver
        Check the PCI VendorId and DeviceId for supported controllers.
    @param[in] EFI_DRIVER_BINDING_PROTOCOL*      Pointer to the Driver Binding protocol
    @param[in] EFI_HANDLE                        Device Handle of the controller to assess
    @param[in] EFI_DEVICE_PATH_PROTOCOL*         Remaining Device Path (ignored)
    @retval Support status of the driver on the controller handle.
                        EFI_SUCCESS             - The driver is supported on the controller handle.
                        EFI_INVALID_PARAMETER   - ControllerHandle is NULL
                        EFI_UNSUPPORTED         - Driver does not support the controller.
**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS           Status      = EFI_SUCCESS;
  EFI_STATUS           CloseStatus = EFI_SUCCESS;
  EFI_PCI_IO_PROTOCOL  *PciIo      = NULL;
  PCI_TYPE00           Pci;

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: DriverBindingProtocol*: '%p'\n", __FUNCTION__, This));
  DEBUG ((DEBUG_INFO, "%a: ControllerHandle: '%p'\n", __FUNCTION__, ControllerHandle));
  DEBUG ((DEBUG_INFO, "%a: RemainingDevicePath*: '%p'\n", __FUNCTION__, RemainingDevicePath));
  DEBUG_CODE_END ();

  if (ControllerHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  /* Open PciIo protocol on controller to find NVIDIA PCI Controllers */
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  /* coverity[cert_int31_c_violation] violation in EDKII-defined macro */
  if (EFI_ERROR (Status)) {
    return Status;
  }

  {
    PCI_LOCATION_INFO  *PciLocationInfo = NULL;
    GetGPUPciLocation (ControllerHandle, &PciLocationInfo);
    if (PciLocationInfo != NULL) {
      FreePool (PciLocationInfo);
    }
  }

  {
    GPU_MODE  mode;
    Status = CheckGpuMode (PciIo, &mode);
    DEBUG ((DEBUG_INFO, "%a: [%p] GetGpuMode returned '%d'.\n", __FUNCTION__, This, mode));
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
  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of Pci TYPE00 returned '%r'\n", __FUNCTION__, This, Status));
  if (!EFI_ERROR (Status)) {
    UINT16  ui16VendorId = Pci.Hdr.VendorId;
    UINT16  ui16DeviceId = Pci.Hdr.DeviceId;
    DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] Controller Handle 2-part Id.\n", __FUNCTION__, ui16VendorId, ui16DeviceId));
    /* PCI Device ID and Vendor ID for smatch to determine support status */
    if ( !IsControllerSupported (ui16VendorId, ui16DeviceId)) {
      DEBUG ((DEBUG_INFO, "%a: [VID:0x%04x|DID:0x%04x] Controller Handle did not match.\n", __FUNCTION__, ui16VendorId, ui16DeviceId));
      Status = EFI_UNSUPPORTED;
    } else {
      DEBUG ((DEBUG_ERROR, "%a: [VID:0x%04x|DID:0x%04x] Controller Handle matched.\n", __FUNCTION__, ui16VendorId, ui16DeviceId));
    }
  }

  DEBUG ((DEBUG_INFO, "%a: About to close\n", __FUNCTION__));
  CloseStatus = gBS->CloseProtocol (
                       ControllerHandle,
                       &gEfiPciIoProtocolGuid,
                       This->DriverBindingHandle,
                       ControllerHandle
                       );

  /* coverity[cert_int31_c_violation] violation in EDKII-defined macro */
  if (EFI_ERROR (CloseStatus)) {
    DEBUG ((DEBUG_INFO, "%a: CloseProtocol return '%r'\n", __FUNCTION__, Status));
    /* coverity[cert_int31_c_violation] violation in EDKII-defined macro */
    ASSERT_EFI_ERROR (CloseStatus);
    return CloseStatus;
  }

  DEBUG ((DEBUG_INFO, "%a: Return '%r'\n", __FUNCTION__, Status));
  return Status;
}

/** Helper function to get BPMP phandle
    @param[in]  This                  EFI_DRIVER_BINDING_PROTOCOL instance
    @param[in]  Segment               PCI Segment
    @param[out] *Phandle              returned BPMP phandle

    @retval EFI_SUCCESS               BPMP phandle succesfully returned
    @retval EFI_UNSUPPORTED           failed obtaining PCI HostBridge | RootBridgeConfigurationIo Protocol
    @retval EFI_OUT_OF_RESOURCES      failed allocating storage for Root Bridge handles
    @retval other                     BPMP phandle not valid
**/
STATIC
EFI_STATUS
GetBpmpPhandle (
  IN UINTN       Segment,
  IN OUT UINT32  *Phandle
  )
{
  EFI_STATUS                                        Status        = EFI_SUCCESS;
  EFI_HANDLE                                        *HandleBuffer = NULL;
  EFI_HANDLE                                        Handle;
  UINTN                                             NoHandles;
  UINTN                                             Index;
  PCI_ROOT_BRIDGE                                   *RootBridgeBuffer = NULL;
  PCI_ROOT_BRIDGE                                   *RootBridge;
  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *RootBridgeCfgIo;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAPciHostBridgeProtocolGuid,
                  NULL,
                  &NoHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* If the buffer is NULL, assume the Host Bridge protocol isn't available instead of OUT_OF_RESOURCES */
  if (HandleBuffer == NULL) {
    Status = EFI_UNSUPPORTED;
    return Status;
  }

  RootBridgeBuffer = (PCI_ROOT_BRIDGE *)AllocatePool (sizeof (PCI_ROOT_BRIDGE) * NoHandles);
  if (RootBridgeBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  for (Index = 0; Index < NoHandles; Index++) {
    Handle     = HandleBuffer[Index];
    RootBridge = NULL;
    Status     = gBS->HandleProtocol (
                        Handle,
                        &gNVIDIAPciHostBridgeProtocolGuid,
                        (VOID **)&RootBridge
                        );
    if (EFI_ERROR (Status)) {
      FreePool (RootBridgeBuffer);
      Status = EFI_UNSUPPORTED;
      return Status;
    }

    if (RootBridge->Segment != Segment) {
      continue;
    }

    /* segments match, get BPMP phandle */
    RootBridgeCfgIo = NULL;
    Status          = gBS->HandleProtocol (
                             Handle,
                             &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                             (VOID **)&RootBridgeCfgIo
                             );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: error getting RootBridgeCfgIo protocol: %r\n", __FUNCTION__, Status));
      FreePool (RootBridgeBuffer);
      Status = EFI_UNSUPPORTED;
      return Status;
    }

    *Phandle = RootBridgeCfgIo->BpmpPhandle;
    Status   = EFI_SUCCESS;
    break;
  }

  if (RootBridgeBuffer != NULL) {
    FreePool (RootBridgeBuffer);
  }

  return Status;
}

/** Update VDM with PCI Bus Device Function for the given NV GPU

    @param[in]  This                  EFI_DRIVER_BINDING_PROTOCOL instance
    @param[in]  Segment               PCI Segment
    @param[in]  Bus                   PCI Bus
    @param[in]  Device                PCI Device
    @param[in]  Function              PCI Function

    @retval EFI_SUCCESS               PCI VDM updated succesfully.
    @retval EFI_NOT_READY             BPMP IPC protocol unavailable.
    @retval EFI_DEVICE_ERROR          BPMP IPC communication error.
    @retval other                     UPHY phandle not valid
**/
STATIC
EFI_STATUS
UpdateVDM (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN UINTN                        Segment,
  IN UINTN                        Bus,
  IN UINTN                        Device,
  IN UINTN                        Function
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol;
  MRQ_UPHY_COMMAND_PACKET   Request;
  UINT32                    BpmpPhandle;
  EFI_STATUS                Status;
  EFI_STATUS                BpmpStatus;
  INT32                     BpmpMessageError;

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed getting BpmpIPC Protocol: %r\n", __FUNCTION__, Status));
    return EFI_NOT_READY;
  }

  Request.Lane              = (UINT16)0;
  Request.Command           = (UINT16)CmdUphyPcieConfigVdm;
  Request.Controller        = (UINT8)PCIE_ID_TO_INTERFACE (Segment);
  Request.BusDeviceFunction = (UINT16)(((Bus      & 0xff) << 8) |
                                       ((Device   & 0x1f) << 3) |
                                       ((Function & 0x07) << 0));
  Status = GetBpmpPhandle (Segment, &BpmpPhandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed getting UPHY phandle: %r\n", __FUNCTION__, Status));
    Status = EFI_ABORTED;
  } else {
    BpmpStatus = BpmpIpcProtocol->Communicate (
                                    BpmpIpcProtocol,
                                    NULL,           /* Token */
                                    BpmpPhandle,
                                    MRQ_UPHY,
                                    (VOID *)&Request,
                                    sizeof (MRQ_UPHY_COMMAND_PACKET),
                                    NULL,           /* Response     */
                                    0,              /* ResponseSize */
                                    &BpmpMessageError
                                    );
    if (EFI_ERROR (BpmpStatus)) {
      DEBUG ((DEBUG_ERROR, "%a: BpmpMessageError: %d\n", __FUNCTION__, BpmpMessageError));
      Status = EFI_DEVICE_ERROR;
    }
  }

  return Status;
}

/** Driver Binding protocol interface to start the driver on the controller handle supplied
    @param[in] EFI_DRIVER_BINDING_PROTOCOL*      Pointer to the Driver Binding protocol
    @param[in] EFI_HANDLE                        Device Handle of the controller to start the driver on
    @param[in] EFI_DEVICE_PATH_PROTOCOL*         Remaining Device Path (ignored)
    @retval Status of setting the ManagedControllerHandle
                        EFI_SUCCESS             - The Driver was successfully started on the controller handle
                        EFI_INVALID_PARAMETER   - ControllerHandle is NULL
                        EFI_UNSUPPORTED         - Driver does not support the controller.
**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                                  Status        = EFI_SUCCESS;
  EFI_STATUS                                  ErrorStatus   = EFI_UNSUPPORTED;
  NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA      *mPrivateData = NULL;
  EFI_PCI_IO_PROTOCOL                         *PciIo        = NULL;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL      *GpuDsdAmlGeneration;
  NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL  *GpuFirmwareBootCompleteProtocol;
  GPU_MODE                                    GpuMode     = GPU_MODE_EH;
  BOOLEAN                                     bFSPEnabled = TRUE;
  UINTN                                       Segment;
  UINTN                                       Bus;
  UINTN                                       Device;
  UINTN                                       Function;
  BOOLEAN                                     bMaskFspRpcTimeoutError = TRUE;

  DEBUG ((DEBUG_ERROR, "%a: DriverBindingProtocol*: '%p'\n", __FUNCTION__, This));
  DEBUG ((DEBUG_INFO, "%a: ControllerHandle: '%p'\n", __FUNCTION__, ControllerHandle));
  DEBUG ((DEBUG_INFO, "%a: RemainingDevicePath*: '%p'\n", __FUNCTION__, RemainingDevicePath));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ControllerHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  mPrivateData = NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA_FROM_THIS (This);

  // Open protocol instance by driver to force 'managed'
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  DEBUG ((DEBUG_INFO, "%a: OpenProtocol on PciIo returned '%r'\n", __FUNCTION__, Status));
  DEBUG ((DEBUG_INFO, "%a: PciIo ProtocolInstance: '%p' on '%p'\n", __FUNCTION__, PciIo, ControllerHandle));

  /* coverity[cert_int31_c_violation] violation in EDKII-defined macro */
  if (EFI_ERROR (Status)) {
    goto ErrorHandler_CloseProtocol;
    Status = gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiPciIoProtocolGuid,
                    This->DriverBindingHandle,
                    ControllerHandle
                    );
    return EFI_UNSUPPORTED;
  }

  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationGet,
                    0,
                    &mPrivateData->PciAttributes
                    );

  DEBUG ((DEBUG_ERROR, "DEBUG: Get Attributes on Handle [%p]. Status '%r'.\n", ControllerHandle, Status));
  if (EFI_ERROR (Status)) {
    goto ErrorHandler_CloseProtocol;
  }

  DEBUG ((DEBUG_ERROR, "DEBUG: Get Attributes on Handle [%p]. Attributes = %x.\n", ControllerHandle, mPrivateData->PciAttributes));
  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationEnable,
                    EFI_PCI_DEVICE_ENABLE,
                    NULL
                    );

  DEBUG ((DEBUG_ERROR, "DEBUG: Set Attributes [%x] on Handle [%p]. Status '%r'.\n", EFI_PCI_DEVICE_ENABLE, ControllerHandle, Status));
  if (EFI_ERROR (Status)) {
    goto ErrorHandler_RestorePCIAttributes;
  }

  /* update VDM with PCIE BDF for supported downstream controllers */
  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  if (!EFI_ERROR (Status)) {
    Status = UpdateVDM (This, Segment, Bus, Device, Function);
    /* note that a failed VDM update is non-fatal */
    DEBUG ((
      DEBUG_ERROR,
      "%a: VDM%aupdated for Segment,Bus,Device,Function 0x%x,0x%x,0x%x,0x%x\n",
      __FUNCTION__,
      EFI_ERROR (Status) ? " NOT " : " ",
      Segment,
      Bus,
      Device,
      Function
      ));
  } else {
    DEBUG ((DEBUG_ERROR, "%a: ERROR: VDM update failed; PciIo GetLocation Status '%r'.\n", __func__, Status));
  }

  /* Check GPU Mode */
  Status = CheckGpuMode (PciIo, &GpuMode);
  if (EFI_ERROR (Status)) {
    goto ErrorHandler_RestorePCIAttributes;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] GetGpuMode returned '%d'.\n", __FUNCTION__, PciIo, GpuMode));

  if ((GpuMode == GPU_MODE_SHH) || (GpuMode == GPU_MODE_EH)) {
    Status = InstallGpuFirmwareBootCompleteProtocolInstance (ControllerHandle);

    /* Validate protocol installation */
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Protocol Install error on Handle [%p]. Status '%r'.\n", ControllerHandle, Status));
      goto ErrorHandler_RestorePCIAttributes;
    }

    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiNVIDIAGpuFirmwareBootCompleteGuid,
                    (VOID **)&GpuFirmwareBootCompleteProtocol,
                    NULL,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Open 'GpuFirmwareBootCompleteProtocol' Protocol on Handle [%p] Status '%r'.\n", ControllerHandle, Status));
      goto ErrorHandler_RestorePCIAttributes;
    }

    if (GpuFirmwareBootCompleteProtocol != NULL) {
      /* FSP is not active on TEGRA_PATFORM_VDK, skip polling and FSP RCP calls */
      bFSPEnabled = (TegraGetPlatform () != TEGRA_PLATFORM_VDK);
      if (bFSPEnabled) {
        BOOLEAN  bFirmwareComplete = FALSE;
        UINT32   TimeoutIdx        = UEFI_GFW_BOOT_COMPLETE_POLL_TIMEOUT_INDEX;

        while ((!bFirmwareComplete) && (--TimeoutIdx)) {
          /* Note: PciIo protocol required. Obtained from Protocol PrivateData Controller lookup. */
          Status = GpuFirmwareBootCompleteProtocol->GetBootCompleteState (GpuFirmwareBootCompleteProtocol, &bFirmwareComplete);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "ERROR: Open 'GpuFirmwareBootCompleteProtocol' Protocol on Handle [%p] Status '%r'.\n", ControllerHandle, Status));
            ErrorStatus = Status;
            goto BootCompleteErrorCheck;
          }

          /* Timeout Progress */
          DEBUG_CODE_BEGIN ();
          if ((TimeoutIdx%(UEFI_GFW_BOOT_COMPLETE_POLL_TIMEOUT_INDEX_LOG_TRIGGER)) == 0) {
            DEBUG ((DEBUG_INFO, "DEBUG: 'GpuFirmwareBootCompleteProtocol' Poll status [%u/%u]\n", (UINT32)(UEFI_GFW_BOOT_COMPLETE_POLL_TIMEOUT_INDEX-TimeoutIdx), (UINT32)UEFI_GFW_BOOT_COMPLETE_POLL_TIMEOUT_INDEX));
          }

          DEBUG_CODE_END ();

          gBS->Stall (UEFI_CHECK_GFW_BOOT_COMPLETE_POLL_DELAY_UNITS);
        }

        DEBUG ((DEBUG_INFO, "DEBUG: 'GpuFirmwareBootCompleteProtocol' Poll exit state {Idx:%u, Complete:%u}\n", TimeoutIdx, bFirmwareComplete));

        if ((TimeoutIdx == 0) && (!bFirmwareComplete)) {
          DEBUG ((DEBUG_ERROR, "ERROR: [TimeoutIdx:%u] Poll for Firmware Boot Complete timed out.\n", TimeoutIdx));
          ErrorStatus = EFI_TIMEOUT;
          goto ErrorHandler_RestorePCIAttributes;
        }

        DEBUG ((DEBUG_INFO, "INFO: 'GpuFirmwareBootCompleteProtocol->GetBootCompleteState' on Handle [%p]. Status '%r'.\n", ControllerHandle, Status));
        DEBUG ((DEBUG_INFO, "%a: GpuFirmwareBootCompleteProtocol 'GetBootCompleteState' for instance:'%p', '%a'\n", __FUNCTION__, GpuFirmwareBootCompleteProtocol, (bFirmwareComplete ? "TRUE" : "FALSE")));
      } else {
        DEBUG ((DEBUG_INFO, "DEBUG: Tegra Platform VDK Detected. Disabling FSP calls.\n"));
      }
    } else {
      DEBUG ((DEBUG_ERROR, "ERROR: Open 'GpuFirmwareBootCompleteProtocol' Protocol on Handle [%p] Status '%r'.\n", ControllerHandle, Status));
    }

    // Install the GPU DSD AML Generation Protocol instance on the supported ControllerHandle
    Status = InstallGpuDsdAmlGenerationProtocolInstance (ControllerHandle);

    /* Validate protocol installation */
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Protocol Install error on Handle [%p]. Status '%r'.\n", ControllerHandle, Status));
      goto ErrorHandler_RestorePCIAttributes;
    }

    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                    (VOID **)&GpuDsdAmlGeneration,
                    NULL,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Open 'GpuDsdAmlGenerationProtocolInstance' Protocol on Handle [%p] Status '%r'.\n", ControllerHandle, Status));
      goto ErrorHandler_RestorePCIAttributes;
    }

    //  FSP EGM and ATS Range configurtion
    //    Retrieve EGM informatoin from GpuDsdAmlGeneration protocol
    if ((PciIo != NULL) && (GpuDsdAmlGeneration != NULL)) {
      UINT64          EgmBasePa        = 0ULL;
      UINT64          EgmSize          = 0ULL;
      UINT64          HbmBasePa        = 0ULL;
      ATS_RANGE_INFO  ATSRangeInfoData = { 0 };
      ATS_RANGE_INFO  *ATSRangeInfo    = &ATSRangeInfoData;

      /* ATSRangeInfo->RangeStart */
      Status = GetControllerATSRangeInfo (ControllerHandle, ATSRangeInfo);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR: 'GetControllerATSRangeInfo' on Handle [%p] Status '%r'.\n", ControllerHandle, Status));
        goto ErrorHandler_RestorePCIAttributes;
      } else {
        DEBUG ((DEBUG_INFO, "%a: [Controller:%p PciIo:%p] HbmRangeStart: '%p'\n", __FUNCTION__, ControllerHandle, PciIo, ATSRangeInfo->HbmRangeStart));
        DEBUG ((DEBUG_INFO, "%a: [Controller:%p PciIo:%p] HbmRangeSize: '%p'\n", __FUNCTION__, ControllerHandle, PciIo, ATSRangeInfo->HbmRangeSize));
        DEBUG ((DEBUG_INFO, "%a: [Controller:%p PciIo:%p] ProximityDomainStart: '%p'\n", __FUNCTION__, ControllerHandle, PciIo, ATSRangeInfo->ProximityDomainStart));
        DEBUG ((DEBUG_INFO, "%a: [Controller:%p PciIo:%p] NumProximityDomains: '%p'\n", __FUNCTION__, ControllerHandle, PciIo, ATSRangeInfo->NumProximityDomains));
        HbmBasePa = ATSRangeInfo->HbmRangeStart;
      }

      Status = GpuDsdAmlGeneration->GetEgmBasePa (GpuDsdAmlGeneration, &EgmBasePa);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR: 'GpuDsdAmlGeneration->GetEgmBasePa' on Handle [%p] Status '%r'.\n", ControllerHandle, Status));
        goto ErrorHandler_RestorePCIAttributes;
      }

      DEBUG ((DEBUG_INFO, "%a: GpuDsdAmlNodeProtocol 'GetEgmBasePa' for instance:'%p', base PA = 0x%016lx\n", __FUNCTION__, GpuDsdAmlGeneration, EgmBasePa));

      Status = GpuDsdAmlGeneration->GetEgmSize (GpuDsdAmlGeneration, &EgmSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR: 'GpuDsdAmlGeneration->GetEgmSize' on Handle [%p] Status '%r'.\n", ControllerHandle, Status));
        goto ErrorHandler_RestorePCIAttributes;
      }

      DEBUG ((DEBUG_INFO, "%a: GpuDsdAmlNodeProtocol 'GetEgmSize' for instance:'%p', size = 0x%016lx\n", __FUNCTION__, GpuDsdAmlGeneration, EgmSize));

      /* Check FSP enabled and GPU Mode SHH before enabling FSP RPC calls */
      if ((bFSPEnabled) && (GpuMode == GPU_MODE_SHH)) {
        UINT64   EgmBasePaSocketMasked  = MaskEgmBaseSocketAddress (EgmBasePa);
        BOOLEAN  bMaskFspRpcDeviceError = TRUE;
        DEBUG ((DEBUG_ERROR, "%a: [Controller:%p] EGM_SOCKET_ADDRESS_MASK = 0x%016lx\n", __FUNCTION__, ControllerHandle, EGM_SOCKET_ADDRESS_MASK));
        DEBUG ((DEBUG_ERROR, "%a: [Controller:%p] EgmBasePaSocketMasked = 0x%016lx\n", __FUNCTION__, ControllerHandle, EgmBasePaSocketMasked));
        /* Need to adjust for size */
        Status = FspConfigurationEgmBaseAndSize (PciIo, EgmBasePaSocketMasked, EgmSize);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "ERROR: 'FspConfigurationEgmBaseAndSize' failed with status '%r'.\n", Status));
          /* Check for non-FATAL FSP RPC transaction failure */
          if ( bMaskFspRpcDeviceError && (Status == EFI_DEVICE_ERROR)) {
            Status = EFI_SUCCESS;
          }

          /* coverity[cert_int31_c_violation] violation in EDKII-defined macro */
          ASSERT_EFI_ERROR (Status);
        }

        Status = FspConfigurationAtsRange (PciIo, HbmBasePa);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "ERROR: 'FspConfigurationAtsRange' failed with status '%r'.\n", Status));
          /* Check for non-FATAL FSP RPC transaction failure */
          if ( bMaskFspRpcDeviceError && (Status == EFI_DEVICE_ERROR)) {
            Status = EFI_SUCCESS;
          }

          /* coverity[cert_int31_c_violation] violation in EDKII-defined macro */
          ASSERT_EFI_ERROR (Status);
        }
      }
    }

BootCompleteErrorCheck:
    /* Check for FSP RPC transaction timeout, flag as non-FATAL */
    if ( bMaskFspRpcTimeoutError && (ErrorStatus == EFI_TIMEOUT)) {
      ErrorStatus = EFI_SUCCESS;
    }

    DEBUG ((DEBUG_INFO, "%a: Finished, Status '%r'\n", __FUNCTION__, Status));
  }

  return Status;

ErrorHandler_RestorePCIAttributes:
  /* On Error, restore PCI Attributes */
  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationSet,
                    mPrivateData->PciAttributes,
                    NULL
                    );

  /* On Error, close protocol */
ErrorHandler_CloseProtocol:
  Status = gBS->CloseProtocol (
                  ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  This->DriverBindingHandle,
                  ControllerHandle
                  );

  return ErrorStatus;
}

/** Driver Binding protocol interface to stop the driver on the controller handle supplied
    @param  EFI_DRIVER_BINDING_PROTOCOL*        Pointer to the Driver Binding protocol
    @param  EFI_HANDLE                          Device Handle of the controller to assess
    @param  UINTN                               Number of children (ignored - no support for children)
    @param  EFI_HANDLE                          Child Handle Buffer (ignored)
    @retval Status of setting the ManagedControllerHandle
                        EFI_SUCCESS             - The driver was successfully stopped on the controller handle
                        EFI_NOT_READY           - Private Data is not initialized.
                        EFI_INVALID_PARAMETER   - ControllerHandle is NULL
                        EFI_UNSUPPORTED         - Driver does not support the controller.
**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  DEBUG ((DEBUG_INFO, "%a: DriverBindingProtocol*: '%p'\n", __FUNCTION__, This));
  DEBUG ((DEBUG_INFO, "%a: ControllerHandle: '%p'\n", __FUNCTION__, ControllerHandle));
  DEBUG ((DEBUG_INFO, "%a: NumberOfChildren: '%u'\n", __FUNCTION__, NumberOfChildren));
  DEBUG ((DEBUG_INFO, "%a: ChildHandleBuffer*: '%p'\n", __FUNCTION__, ChildHandleBuffer));

  if (ControllerHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = UninstallGpuFirmwareBootCompleteProtocolInstance (ControllerHandle);
  DEBUG ((DEBUG_INFO, "%a: Uninstall GPU Firmware Boot Complete Protocol Instance on '%p': '%r'\n", __FUNCTION__, ControllerHandle, Status));

  Status = UninstallGpuDsdAmlGenerationProtocolInstance (ControllerHandle);
  DEBUG ((DEBUG_INFO, "%a: Uninstall GPU DSD AML Generation Protocol Instance on '%p': '%r'\n", __FUNCTION__, ControllerHandle, Status));

  /* Close protocol instance to clear 'managed' state */
  Status = gBS->CloseProtocol (
                  ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  This->DriverBindingHandle,
                  ControllerHandle
                  );

  DEBUG ((DEBUG_INFO, "%a: Close PciIo Protocol Instance on '%p'\n", __FUNCTION__, ControllerHandle));
  /* coverity[cert_int31_c_violation] violation in EDKII-defined macro */
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

///
/// Driver Binding private data and protocol definition
///

/* Driver Binding private data template */
/* Uncrustify: *INDENT-OFF* */
NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA  mPrivateData = {
  /* .Signature */                NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA_SIGNATURE,
  /* .Handle */                   /* EFI_HANDLE* */        NULL,
  /* .DriverBinding */            { /* EFI_DRIVER_BINDING_PROTOCOL */
                                    NVIDIAGpuDriverSupported,
                                    NVIDIAGpuDriverStart,
                                    NVIDIAGpuDriverStop,
                                    NVIDIA_GPUDEVICELIBDRIVER_VERSION,
                                    NULL, /* ImageHandle */
                                    NULL  /* DriverBindingHandle */
                                  },
  /* .SystemTable */              /* EFI_SYSTEM_TABLE* */  NULL,
  /* .BootServices */             /* EFI_BOOT_SERVICES* */ NULL,
  /* Managed Controller information */
  /* .ManagedControllerHandles */ /* EFI_HANDLE*  */       NULL,
  /* .nManagedControllers */                               0,
  /* .PciAttributes */                                     0
};
/* Uncrustify: *INDENT-ON* */
EFI_DRIVER_BINDING_PROTOCOL  *gNVIDIAGpuDeviceLibDriverBinding = &mPrivateData.DriverBinding;

/** Install the driver binding on the ImageHandle
    @param[in] ImageHandle  ImageHandle to install the DriverBinding on
    @param{in} SystemTable  Pointer to the EFI System Table structure
    @retval EFI_STATUS  EFI_SUCCESS
    @retval EFI_NOT_READY ImageHandle is NULL
    @retval EFI_NOT_READY SystemTable is NULL
      (pass through error from EfiLibInstallDriverBindingComponentName2
**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  // Sanity check state
  if (ImageHandle == NULL) {
    return EFI_NOT_READY;
  }

  if (SystemTable == NULL) {
    return EFI_NOT_READY;
  }

  Status =
    NVIDIADriverDumpPrivateData (&mPrivateData.DriverBinding);

  if (EFI_ERROR (Status)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Install driver model protocol(s) onto ImageHandle
  //
  Status =
    EfiLibInstallDriverBindingComponentName2 (
      ImageHandle,                                              // ImageHandle
      SystemTable,                                              // SystemTable
      &mPrivateData.DriverBinding,                              // DriverBinding
      ImageHandle,                                              // DriverBindingHandle
      NULL,                                                     // ComponentName
      &gNVIDIAGpuDriverComponentName2Protocol                   // ComponentName2
      );
  /* coverity[cert_int31_c_violation] violation in EDKII-defined macro */
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/** Function that unloads the NVIDIA GPU Driver binding.

    @param[in]  ImageHandle  The driver handle managing the Firmware Management Protocol instance to unload.

    @retval EFI_SUCCESS               Driver image was removed successfully.
    @retval EFI_INVALID_PARAMETER     ImageHandle is NULL.
    @retval EFI_INVALID_PARAMETER     ImageHandle does not match driver image handle.
    @retval EFI_UNSUPPORTED           Private Data ImageHandle is NULL
    @retval EFI_UNSUPPORTED           Private Data DriverBindingHandle is NULL
    @retval EFI_UNSUPPORTED           The device is not managed by a driver that follows
                                        the UEFI Driver Model.
    @retval other                     Status from EfiLibUninstallDriverBindingComponentName2

**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverUnloadImage (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status        = EFI_UNSUPPORTED;
  EFI_HANDLE  *HandleBuffer = NULL;
  UINTN       HandleCount;
  UINTN       Index;

  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EFI_UNSUPPORTED;

  /* Driver is Not tracking managed controllers, so disconnect from all handles */
  for (Index = 0; Index < HandleCount; Index++) {
    EFI_STATUS  StatusDisconnect = EFI_SUCCESS;
    if (ImageHandle == HandleBuffer[Index]) {
      continue;
    }

    StatusDisconnect = gBS->DisconnectController (HandleBuffer[Index], ImageHandle, NULL);
    /* On any successful discunnect, change return status */
    if (StatusDisconnect == EFI_SUCCESS) {
      Status = EFI_SUCCESS;
    }

    DEBUG_CODE_BEGIN ();
    /* Logging reduction. Only log successful disconnect. Disable for all controller status. */
    if (EFI_ERROR (Status)) {
      continue;
    }

    DEBUG ((DEBUG_INFO, "%a: DisconnectController ('%p','%p','%p') returned '%r'\n", __FUNCTION__, HandleBuffer[Index], ImageHandle, NULL, Status));
    DEBUG_CODE_END ();
  }

  Status =
    EfiLibUninstallDriverBindingComponentName2 (
      &mPrivateData.DriverBinding,                              // DriverBinding
      NULL,                                                     // ComponentName
      &gNVIDIAGpuDriverComponentName2Protocol                   // ComponentName2
      );

  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  return Status;
}
